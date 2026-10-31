"""Conservative, read-only directional obstacle classification for D299 traces."""

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
    Bundle, InvalidBundle, _angle_difference, _pose_at_time,
    _pose_mismatch, _read_csv, _time_boundaries, load_bundle,
)

# Expected start states are validation guards, never outcome labels.
STARTS = {
    "ground_block_box_01": (-117.0, 2117.3, -92.0, -90.0),
    "ground_block_wall_01": (231.0, 2408.0, -92.0, 90.0),
    "ground_deflect_wall_01": (231.0, 2408.0, -92.0, 45.0),
    "ground_block_corner_01": (418.0, 2434.5, -92.0, 45.0),
}
DURATION_MS = 1000
TAIL_MS = 200
POSE_TOLERANCE = 1.0
CONTINUITY_TOLERANCE = 0.05
COMMAND_TOLERANCE = 0.02
ANGLE_TOLERANCE = 1.0
MAX_FRAME_MS = 20

DIAGNOSTIC_COLUMNS = (
    "kind,frame,action,command_ms,game_time,round_start_time,round_id,"
    "round_over,mp_freezetime,freeze_bots,flags,movetype,solid,deadflag,"
    "maxspeed,friction,basevx,basevy,basevz,forward_speed,strafe_speed,"
    "x,y,z,vx,vy,vz,obstacle_fraction,obstacle_startsolid,obstacle_allsolid"
).split(",")


@dataclass(frozen=True)
class ObstacleProfile:
    outcome: str
    reason: str
    forward_distance: float = math.nan
    lateral_distance: float = math.nan
    tail_forward: float = math.nan
    tail_lateral: float = math.nan
    end_speed: float = math.nan
    stop_ms: int | None = None


@dataclass(frozen=True)
class ObstacleDiagnostics:
    stalls: int
    direct_hits: int


def load_obstacle_diagnostics(directory: Path) -> ObstacleDiagnostics:
    """Validate the bounded v1 diagnostic record, without deriving a label from it."""
    rows = _read_csv(directory / "diagnostics.csv", 128 * 1024)
    if len(rows) < 3 or rows[0] != ["aipb-scenario-diagnostics-v1"] or rows[1] != DIAGNOSTIC_COLUMNS:
        raise InvalidBundle("diagnostics.csv: unsupported version or columns")
    if rows[2][0] != "start" or rows[-1][0] != "end":
        raise InvalidBundle("diagnostics.csv: missing start/end records")
    stalls = 0
    direct_hits = 0
    previous_ms = -1
    for row in rows[2:]:
        if len(row) != len(DIAGNOSTIC_COLUMNS) or row[0] not in {"start", "frame", "stall", "end"}:
            raise InvalidBundle("diagnostics.csv: invalid record")
        try:
            elapsed = int(row[3])
            fraction = float(row[27])
        except ValueError as exc:
            raise InvalidBundle("diagnostics.csv: invalid diagnostic number") from exc
        if not 0 <= elapsed <= DURATION_MS or elapsed < previous_ms or not math.isfinite(fraction):
            raise InvalidBundle("diagnostics.csv: invalid time or obstacle fraction")
        previous_ms = elapsed
        if row[0] == "stall":
            stalls += 1
            if 0.0 <= fraction <= 0.01:
                direct_hits += 1
    return ObstacleDiagnostics(stalls, direct_hits)


def measure_obstacle(run: Bundle) -> ObstacleProfile:
    """Classify physical motion only; command completion alone is not success."""
    def unknown(reason: str) -> ObstacleProfile:
        return ObstacleProfile("Inconclusive", reason)

    expected = STARTS.get(run.name)
    if expected is None or run.map != "de_dust2":
        return unknown("ContextMismatch")
    if run.status != "Completed" or run.source != "live":
        return unknown("IncompleteRun")
    if run.command_ms != DURATION_MS:
        return unknown("DurationMismatch")
    if not run.trace or run.frames != len(run.trace):
        return unknown("IncompleteTrace")
    if _pose_mismatch(run.start, run.trace[0].before, "start"):
        return unknown("StartObservationMismatch")
    if _pose_mismatch(run.end, run.trace[-1].after, "end"):
        return unknown("EndObservationMismatch")
    first = run.trace[0].before
    if (max(abs(first.x - expected[0]), abs(first.y - expected[1]),
            abs(first.z - expected[2])) > POSE_TOLERANCE
            or _angle_difference(first.yaw, expected[3]) > ANGLE_TOLERANCE
            or abs(first.pitch) > ANGLE_TOLERANCE):
        return unknown("UnexpectedStartPose")
    if math.hypot(first.vx, first.vy) > 5.0 or abs(first.vz) > 5.0:
        return unknown("InitialSpeedMismatch")
    cap = run.trace[0].max_speed
    if not math.isfinite(cap) or cap < 10.0:
        return unknown("InvalidSpeedCap")

    previous = None
    elapsed = 0
    # The first frame that belongs to the final continuously stationary suffix.
    stationary_since = DURATION_MS
    for index, frame in enumerate(run.trace):
        if frame.index != index or frame.action != 0:
            return unknown("UnexpectedAction")
        if not 1 <= frame.msec <= MAX_FRAME_MS:
            return unknown("InvalidFrameDuration")
        elapsed += frame.msec
        if elapsed > DURATION_MS:
            return unknown("PhaseTimingMismatch")
        if previous is not None:
            old, new = previous.after, frame.before
            if (max(abs(getattr(old, field) - getattr(new, field))
                    for field in ("x", "y", "z", "vx", "vy", "vz"))
                    > CONTINUITY_TOLERANCE
                    or old.ground != new.ground or old.duck != new.duck):
                return unknown("ObservationDiscontinuity")
        if not math.isfinite(frame.max_speed) or abs(frame.max_speed - cap) > COMMAND_TOLERANCE:
            return unknown("SpeedCapChanged")
        if (abs(frame.forward - cap) > COMMAND_TOLERANCE
                or abs(frame.strafe) > COMMAND_TOLERANCE or frame.buttons != 0):
            return unknown("UnexpectedMovementCommand")
        if frame.before.ground != 1 or frame.after.ground != 1 or frame.before.duck or frame.after.duck:
            return unknown("GroundContactOrStance")
        if (abs(frame.after.z - first.z) > 2.0
                or abs(frame.before.z - first.z) > 2.0):
            return unknown("UnexpectedElevation")
        for observed in (frame.yaw, frame.before.yaw, frame.after.yaw):
            if _angle_difference(observed, expected[3]) > ANGLE_TOLERANCE:
                return unknown("UnexpectedYaw")
        for observed in (frame.pitch, frame.before.pitch, frame.after.pitch):
            if abs(observed) > ANGLE_TOLERANCE:
                return unknown("UnexpectedPitch")
        if math.hypot(frame.after.vx, frame.after.vy) <= 5.0:
            if stationary_since == DURATION_MS:
                stationary_since = elapsed
        else:
            stationary_since = DURATION_MS
        previous = frame

    if elapsed != DURATION_MS:
        return unknown("PhaseTimingMismatch")
    boundaries = _time_boundaries(run)
    tail = _pose_at_time(run, boundaries, DURATION_MS - TAIL_MS)
    if tail is None:
        return unknown("UncertainTailState")
    end = run.trace[-1].after
    direction = math.radians(expected[3])
    cosine, sine = math.cos(direction), math.sin(direction)

    def project(dx: float, dy: float) -> tuple[float, float]:
        return dx * cosine + dy * sine, -dx * sine + dy * cosine

    forward, lateral = project(end.x - first.x, end.y - first.y)
    tail_forward, tail_lateral = project(end.x - tail.x, end.y - tail.y)
    end_speed = math.hypot(end.vx, end.vy)
    kwargs = dict(forward_distance=forward, lateral_distance=lateral,
                  tail_forward=tail_forward, tail_lateral=tail_lateral,
                  end_speed=end_speed, stop_ms=stationary_since if stationary_since < DURATION_MS else None)

    # A command opposed by a wall has a stationary terminal window even if
    # the initial approach was productive. Never infer a global blocked edge.
    if (forward >= 15.0 and abs(tail_forward) <= 1.0
            and abs(tail_lateral) <= 1.0 and end_speed <= 5.0):
        return ObstacleProfile("Blocked", "TerminalStall", **kwargs)
    # Continued longitudinal motion with substantial sideways displacement
    # is a deflection, not a success/failure inference from the filename.
    if (forward >= 20.0 and end_speed >= 20.0
            and abs(lateral) >= 8.0 and abs(tail_lateral) >= 8.0
            and lateral * tail_lateral > 0.0):
        return ObstacleProfile("Deflected", "SustainedLateralMotion", **kwargs)
    if (forward >= 20.0 and tail_forward >= 8.0 and end_speed >= 20.0
            and abs(lateral) < 8.0 and abs(tail_lateral) < 8.0):
        return ObstacleProfile("Progress", "SustainedForwardMotion", **kwargs)
    return ObstacleProfile("Inconclusive", "AmbiguousMovement", **kwargs)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path, help="Directory containing result.csv, trace.csv, diagnostics.csv")
    args = parser.parse_args(argv)
    try:
        run = load_bundle(args.run)
        diagnostics = load_obstacle_diagnostics(args.run)
    except InvalidBundle as exc:
        print(f"InvalidEvidence: {exc}", file=sys.stderr)
        return 2
    result = measure_obstacle(run)
    print(f"{result.outcome}: {result.reason}")
    if math.isfinite(result.forward_distance):
        print(f"forward_distance={result.forward_distance:.6f} lateral_distance={result.lateral_distance:.6f} "
              f"tail_forward={result.tail_forward:.6f} tail_lateral={result.tail_lateral:.6f} "
              f"end_speed={result.end_speed:.6f} "
              f"stop_ms={result.stop_ms if result.stop_ms is not None else 'not_observed'}")
    print(f"diagnostic_stalls={diagnostics.stalls} diagnostic_direct_hits={diagnostics.direct_hits}")
    return 0 if result.outcome != "Inconclusive" else 3


if __name__ == "__main__":
    raise SystemExit(main())
