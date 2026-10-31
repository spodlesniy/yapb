"""Conservative offline assessment of an explicitly declared ground-movement goal.

This does not infer navigation, clearance, or autonomous movement capability.
"""

from __future__ import annotations

if __name__ == "__main__" and not __package__:
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    __package__ = "tools.aipb_training"

import argparse
import csv
import io
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path

from tools.aipb_training.scenario_trace_compare import (
    ANGLE_TOLERANCE,
    POSITION_TOLERANCE,
    VELOCITY_TOLERANCE,
    Bundle,
    InvalidBundle,
    Pose,
    _angle_difference,
    load_bundle,
)

GOAL_VERSION = "aipb-scenario-ground-goal-v1"
GOAL_MAX_BYTES = 512
# The D281 trace records engine float poses, not exact mathematical geometry.
# At the threshold, choose uncertainty rather than deciding by rounding noise.
TRAVEL_TOLERANCE = 1.0  # GoldSrc units
END_POSITION_TOLERANCE = 2.0  # GoldSrc units
NAME = re.compile(r"[A-Za-z0-9_-]{1,63}\Z")


class InvalidGoal(ValueError):
    """A missing or malformed declared target is never inferred."""


@dataclass(frozen=True)
class GroundGoal:
    scenario: str
    map: str
    command_ms: int
    heading: float
    minimum_travel: float


@dataclass(frozen=True)
class GroundOutcome:
    outcome: str
    reason: str
    projected_travel: float
    target: float
    frames: int
    frame: int | None = None


def read_goal(path: Path) -> GroundGoal:
    try:
        with path.open("rb") as handle:
            data = handle.read(GOAL_MAX_BYTES + 1)
    except OSError as exc:
        raise InvalidGoal(f"{path}: {exc}") from exc

    if len(data) > GOAL_MAX_BYTES:
        raise InvalidGoal(f"{path}: goal exceeds {GOAL_MAX_BYTES} bytes")

    try:
        rows = list(csv.reader(io.StringIO(data.decode("utf-8")), strict=True))
    except (UnicodeError, csv.Error) as exc:
        raise InvalidGoal(f"{path}: malformed CSV: {exc}") from exc

    if len(rows) != 1 or len(rows[0]) != 6:
        raise InvalidGoal(f"{path}: exactly one six-field goal record is required")

    version, scenario, map_name, ms_text, heading_text, travel_text = rows[0]
    if version != GOAL_VERSION or not NAME.fullmatch(scenario) or not NAME.fullmatch(map_name):
        raise InvalidGoal(f"{path}: invalid goal version or identity")

    if not re.fullmatch(r"[0-9]+", ms_text):
        raise InvalidGoal(f"{path}: command_ms must be an integer")
    command_ms = int(ms_text)

    try:
        heading = float(heading_text)
        minimum_travel = float(travel_text)
    except ValueError as exc:
        raise InvalidGoal(f"{path}: invalid numeric goal") from exc

    if not (1 <= command_ms <= 60000
            and math.isfinite(heading) and -180 <= heading < 180
            and math.isfinite(minimum_travel) and 0 < minimum_travel <= 4096):
        raise InvalidGoal(f"{path}: goal parameters out of range")

    return GroundGoal(scenario, map_name, command_ms, heading, minimum_travel)


def _pose_continuous(a: Pose, b: Pose) -> bool:
    return (
        all(abs(getattr(a, k) - getattr(b, k)) <= POSITION_TOLERANCE
            for k in ("x", "y", "z"))
        and all(abs(getattr(a, k) - getattr(b, k)) <= VELOCITY_TOLERANCE
                for k in ("vx", "vy", "vz"))
        and all(_angle_difference(getattr(a, k), getattr(b, k)) <= ANGLE_TOLERANCE
                for k in ("yaw", "pitch"))
        and a.ground == b.ground and a.duck == b.duck
    )


def assess_ground_movement(run: Bundle, goal: GroundGoal) -> GroundOutcome:
    def result(outcome: str, reason: str, travel: float = 0.0,
               frame: int | None = None) -> GroundOutcome:
        return GroundOutcome(outcome, reason, travel, goal.minimum_travel, len(run.trace), frame)

    if (run.name, run.map) != (goal.scenario, goal.map):
        return result("Inconclusive", "GoalContextMismatch")

    if run.status != "Completed" or run.source != "live":
        return result("Inconclusive", "IncompleteRun")

    if run.command_ms != goal.command_ms:
        return result("Inconclusive", "CommandDurationMismatch")

    if not run.trace or len(run.trace) != run.frames:
        return result("Inconclusive", "IncompleteTrace")

    if not _pose_continuous(run.start, run.trace[0].before):
        return result("Inconclusive", "StartObservationMismatch", frame=0)

    if math.dist((run.end.x, run.end.y, run.end.z),
                 (run.trace[-1].after.x, run.trace[-1].after.y,
                  run.trace[-1].after.z)) > END_POSITION_TOLERANCE:
        return result("Inconclusive", "EndObservationMismatch", frame=run.trace[-1].index)

    for index, frame in enumerate(run.trace):
        if index and not _pose_continuous(run.trace[index - 1].after, frame.before):
            return result("Inconclusive", "ObservationDiscontinuity", frame=index)

        if (frame.before.ground != 1 or frame.after.ground != 1
                or frame.before.duck != 0 or frame.after.duck != 0
                or frame.buttons & (1 << 1 | 1 << 2)):
            # IN_JUMP and IN_DUCK invalidate a straight grounded experiment.
            return result("Inconclusive", "GroundContactOrStance", frame=index)

        if frame.max_speed <= 0:
            return result("Inconclusive", "UnverifiedMovementSpeed", frame=index)

    start = run.trace[0].before
    end = run.trace[-1].after
    radians = math.radians(goal.heading)
    projected = ((end.x - start.x) * math.cos(radians)
                 + (end.y - start.y) * math.sin(radians))
    if projected >= goal.minimum_travel + TRAVEL_TOLERANCE:
        return result("Success", "TargetReached", projected)

    if projected <= goal.minimum_travel - TRAVEL_TOLERANCE:
        return result("Failure", "BelowTarget", projected)

    return result("Inconclusive", "ThresholdAmbiguity", projected)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path, help="Run directory with result.csv and trace.csv")
    parser.add_argument("goal", type=Path, help="Explicit versioned ground goal CSV")
    args = parser.parse_args(argv)

    try:
        goal = read_goal(args.goal)
        assessment = assess_ground_movement(load_bundle(args.run), goal)
    except (InvalidBundle, InvalidGoal) as exc:
        print(f"InvalidEvidence: {exc}", file=sys.stderr)
        return 2

    print(f"{assessment.outcome}: {assessment.reason}")
    print(f"scenario={goal.scenario} map={goal.map} command_ms={goal.command_ms} "
          f"heading={goal.heading:.6g} projected_travel={assessment.projected_travel:.6g} "
          f"minimum={goal.minimum_travel:.6g} frames={assessment.frames}" +
          (f" first_invalid_frame={assessment.frame}"
           if assessment.frame is not None else ""))
    return {"Success": 0, "Failure": 1, "Inconclusive": 3}[assessment.outcome]


if __name__ == "__main__":
    raise SystemExit(main())
