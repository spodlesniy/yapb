"""Read-only comparison of two turns entered at different speeds in one HLDS run."""

from __future__ import annotations

if __name__ == "__main__" and not __package__:
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    __package__ = "tools.aipb_training"

import argparse
import math
import sys
from dataclasses import dataclass
from pathlib import Path

from tools.aipb_training.scenario_trace_compare import (
    Bundle, InvalidBundle, Pose, _angle_difference, _pose_mismatch, load_bundle
)

DEFAULT_DURATIONS = (50, 200, 300, 200, 500)
YAW_DELTAS = (0.0, 30.0, 0.0, -30.0, 0.0)
COMMAND_TOLERANCE = 0.02
YAW_TOLERANCE = 1.0
MAX_TURN_FRAME_MS = 20


@dataclass(frozen=True)
class TurnSample:
    entry_speed: float
    exit_speed: float
    displacement: float
    entry_yaw: float
    exit_yaw: float
    exit_velocity_yaw_error: float


@dataclass(frozen=True)
class TurnSpeedProfile:
    outcome: str
    reason: str
    slow_turn: TurnSample | None = None
    fast_turn: TurnSample | None = None
    speed_difference: float = math.nan
    final_speed: float = math.nan


def _speed(pose: Pose) -> float:
    return math.hypot(pose.vx, pose.vy)


def _wrap(degrees: float) -> float:
    return (degrees + 180.0) % 360.0 - 180.0


def _turn(start: Pose, end: Pose, entry_yaw: float, exit_yaw: float) -> TurnSample:
    final_velocity_yaw_error = math.nan
    if _speed(end) > 5.0:
        velocity_yaw = math.degrees(math.atan2(end.vy, end.vx))
        final_velocity_yaw_error = _wrap(velocity_yaw - exit_yaw)
    return TurnSample(
        entry_speed=_speed(start),
        exit_speed=_speed(end),
        displacement=math.dist((start.x, start.y), (end.x, end.y)),
        entry_yaw=entry_yaw,
        exit_yaw=exit_yaw,
        exit_velocity_yaw_error=final_velocity_yaw_error,
    )


def measure_turn_speed(
    run: Bundle,
    scenario: str = "ground_turn_speed_01",
    map_name: str = "de_dust2",
    start_yaw: float = -105.0,
    durations: tuple[int, ...] = DEFAULT_DURATIONS,
) -> TurnSpeedProfile:
    """Report measured turn entry speeds, not proof of geometry-independent turning."""
    def inconclusive(reason: str) -> TurnSpeedProfile:
        return TurnSpeedProfile("Inconclusive", reason)

    if len(durations) != 5 or any(not isinstance(ms, int) or ms < 1 for ms in durations):
        return inconclusive("InvalidDuration")
    if not math.isfinite(start_yaw) or not -180 <= start_yaw < 180:
        return inconclusive("InvalidHeading")
    if (run.name, run.map) != (scenario, map_name):
        return inconclusive("ContextMismatch")
    if run.status != "Completed" or run.source != "live":
        return inconclusive("IncompleteRun")
    if run.command_ms != sum(durations):
        return inconclusive("DurationMismatch")
    if not run.trace or len(run.trace) != run.frames:
        return inconclusive("IncompleteTrace")
    if _pose_mismatch(run.start, run.trace[0].before, "start"):
        return inconclusive("StartObservationMismatch")
    if _pose_mismatch(run.end, run.trace[-1].after, "end"):
        return inconclusive("EndObservationMismatch")

    elapsed = [0, 0, 0, 0, 0]
    first = [None] * 5
    last = [None] * 5
    cap = run.trace[0].max_speed
    if not math.isfinite(cap) or cap < 10:
        return inconclusive("InvalidSpeedCap")

    planned_yaw = start_yaw
    previous = None
    for frame in run.trace:
        action = frame.action
        if action not in range(5) or (previous is None and action != 0):
            return inconclusive("UnexpectedAction")
        if previous is not None:
            if action not in (previous.action, previous.action + 1):
                return inconclusive("UnexpectedAction")
            old = previous.after
            new = frame.before
            # The engine assigns the upcoming view angle before capturing "before".
            # Only physical state is continuous across these two observations.
            if (max(abs(getattr(old, field) - getattr(new, field))
                    for field in ("x", "y", "z", "vx", "vy", "vz")) > 0.01
                    or old.ground != new.ground or old.duck != new.duck):
                return inconclusive("ObservationDiscontinuity")

        if first[action] is None:
            first[action] = frame
        last[action] = frame
        elapsed[action] += frame.msec
        if elapsed[action] > durations[action]:
            return inconclusive("PhaseTimingMismatch")
        if not math.isfinite(frame.max_speed) or abs(frame.max_speed - cap) > COMMAND_TOLERANCE:
            return inconclusive("SpeedCapChanged")
        if (frame.before.ground != 1 or frame.after.ground != 1
                or frame.before.duck or frame.after.duck or frame.buttons != 0):
            return inconclusive("GroundContactOrStance")
        forward = cap if action in (1, 2, 3) else 0.0
        if abs(frame.forward - forward) > COMMAND_TOLERANCE or abs(frame.strafe) > COMMAND_TOLERANCE:
            return inconclusive("UnexpectedMovementCommand")
        if action in (1, 3) and frame.msec > MAX_TURN_FRAME_MS:
            return inconclusive("CoarseTurnSampling")
        previous_turns = sum(YAW_DELTAS[:action])
        expected_yaw = _wrap(start_yaw + previous_turns
                             + YAW_DELTAS[action] * elapsed[action] / durations[action])
        if (not math.isfinite(frame.yaw)
                or _angle_difference(frame.yaw, expected_yaw) > YAW_TOLERANCE
                or _angle_difference(frame.before.yaw, expected_yaw) > YAW_TOLERANCE
                or _angle_difference(frame.after.yaw, expected_yaw) > YAW_TOLERANCE):
            return inconclusive("UnexpectedYaw")

        previous = frame

    if elapsed != list(durations) or any(f is None for f in first):
        return inconclusive("PhaseTimingMismatch")
    if _speed(first[0].before) > max(5.0, cap * 0.02):
        return inconclusive("InitialSpeedMismatch")

    low_start = first[1].before
    high_start = first[3].before
    low = _turn(low_start, last[1].after, _wrap(start_yaw), _wrap(start_yaw + 30))
    high = _turn(high_start, last[3].after, _wrap(start_yaw + 30), _wrap(start_yaw))
    if (low.entry_speed > max(5.0, cap * 0.05)
            or high.entry_speed < cap * 0.5
            or high.entry_speed - low.entry_speed < cap * 0.25):
        return inconclusive("SpeedContrastNotObserved")
    if low.displacement < 1.0 or high.displacement < 1.0:
        return inconclusive("InsufficientPhysicalProgress")

    return TurnSpeedProfile(
        outcome="Measured",
        reason="TwoTurnsObserved",
        slow_turn=low,
        fast_turn=high,
        speed_difference=high.entry_speed - low.entry_speed,
        final_speed=_speed(run.end),
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path, help="Saved run directory with result.csv and trace.csv")
    parser.add_argument("--scenario", default="ground_turn_speed_01")
    parser.add_argument("--map", dest="map_name", default="de_dust2")
    parser.add_argument("--heading-deg", type=float, default=-105.0)
    args = parser.parse_args(argv)

    try:
        run = load_bundle(args.run)
    except InvalidBundle as exc:
        print(f"InvalidEvidence: {exc}", file=sys.stderr)
        return 2
    profile = measure_turn_speed(run, args.scenario, args.map_name, args.heading_deg)
    print(f"{profile.outcome}: {profile.reason}")
    if profile.outcome != "Measured":
        return 3

    for label, turn in (("slow_turn", profile.slow_turn), ("fast_turn", profile.fast_turn)):
        print(f"{label}: entry_speed={turn.entry_speed:.6g} "
              f"exit_speed={turn.exit_speed:.6g} displacement={turn.displacement:.6g} "
              f"entry_yaw={turn.entry_yaw:.6g} exit_yaw={turn.exit_yaw:.6g} "
              f"exit_velocity_yaw_error_deg={turn.exit_velocity_yaw_error:.6g}")
    print(f"speed_difference={profile.speed_difference:.6g} "
          f"final_speed={profile.final_speed:.6g}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
