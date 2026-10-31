"""Read-only grounded elevation measurements for the D295 slope approach trial."""

from __future__ import annotations

import argparse
import math
import sys
from dataclasses import dataclass
from pathlib import Path

from tools.aipb_training.scenario_trace_compare import (
    Bundle, InvalidBundle, Pose, _angle_difference, _pose_mismatch, load_bundle
)

DEFAULT_DURATIONS = (500, 250, 1, 2000)
YAW_DELTAS = (0.0, 45.0, 90.0, 0.0)
COMMAND_TOLERANCE = 0.02
YAW_TOLERANCE = 1.0
POSITION_CONTINUITY_TOLERANCE = 0.05


@dataclass(frozen=True)
class SlopeProfile:
    outcome: str
    reason: str
    approach_x: float = math.nan
    approach_y: float = math.nan
    approach_z: float = math.nan
    slope_start_x: float = math.nan
    slope_start_y: float = math.nan
    slope_start_z: float = math.nan
    start_speed: float = math.nan
    end_speed: float = math.nan
    forward_distance: float = math.nan
    horizontal_distance: float = math.nan
    end_height_delta: float = math.nan
    max_height_gain: float = math.nan
    first_height_gain_ms: int | None = None


def measure_slope_forward(
    run: Bundle,
    scenario: str = "ground_slope_forward_01",
    map_name: str = "de_dust2",
    start_yaw: float = -105.0,
    durations: tuple[int, ...] = DEFAULT_DURATIONS,
    *,
    min_rise: float = 4.0,
) -> SlopeProfile:
    """Record grounded height changes without inferring slope geometry from Z alone."""
    if scenario == "ground_slope_forward_02":
        return _measure_direct_slope(run, map_name, start_yaw, min_rise)

    def inconclusive(reason: str) -> SlopeProfile:
        return SlopeProfile("Inconclusive", reason)

    if len(durations) != 4 or any(type(ms) is not int or ms <= 0 for ms in durations):
        return inconclusive("InvalidDuration")
    if not math.isfinite(start_yaw) or not -180 <= start_yaw < 180:
        return inconclusive("InvalidHeading")
    if not math.isfinite(min_rise) or min_rise <= 0:
        return inconclusive("InvalidRiseThreshold")
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

    cap = run.trace[0].max_speed
    if not math.isfinite(cap) or cap < 10.0:
        return inconclusive("InvalidSpeedCap")
    durations_observed = [0, 0, 0, 0]
    first = [None] * 4
    last = [None] * 4
    previous = None

    for frame in run.trace:
        action = frame.action
        if action not in range(4) or (previous is None and action != 0):
            return inconclusive("UnexpectedAction")
        if previous is not None:
            if action not in (previous.action, previous.action + 1):
                return inconclusive("UnexpectedAction")
            old = previous.after
            new = frame.before
            # The view yaw is applied before the pre-move snapshot; physical
            # state must still be continuous across consecutive engine frames.
            if (max(abs(getattr(old, field) - getattr(new, field))
                    for field in ("x", "y", "z", "vx", "vy", "vz"))
                    > POSITION_CONTINUITY_TOLERANCE
                    or old.ground != new.ground or old.duck != new.duck):
                return inconclusive("ObservationDiscontinuity")

        if first[action] is None:
            first[action] = frame
        last[action] = frame
        durations_observed[action] += frame.msec
        if durations_observed[action] > durations[action]:
            return inconclusive("PhaseTimingMismatch")
        if not math.isfinite(frame.max_speed) or abs(frame.max_speed - cap) > COMMAND_TOLERANCE:
            return inconclusive("SpeedCapChanged")
        if (frame.before.ground != 1 or frame.after.ground != 1
                or frame.before.duck or frame.after.duck or frame.buttons != 0):
            return inconclusive("GroundContactOrStance")

        expected_forward = cap if action in (0, 3) else 0.0
        if (abs(frame.forward - expected_forward) > COMMAND_TOLERANCE
                or abs(frame.strafe) > COMMAND_TOLERANCE):
            return inconclusive("UnexpectedMovementCommand")

        previous_angle = sum(YAW_DELTAS[:action])
        elapsed = durations_observed[action]
        expected_yaw = (start_yaw + previous_angle
                        + YAW_DELTAS[action] * elapsed / durations[action])
        for observed in (frame.yaw, frame.before.yaw, frame.after.yaw):
            if not math.isfinite(observed) or _angle_difference(
                observed, expected_yaw
            ) > YAW_TOLERANCE:
                return inconclusive("UnexpectedYaw")
        previous = frame

    if durations_observed != list(durations) or any(f is None for f in first):
        return inconclusive("PhaseTimingMismatch")

    slope_start = first[3].before
    approach = last[1].after
    # GoldSrc may move slightly during the 1-ms idle rotation; do not claim
    # the recorded slope start is bit-exactly the approach endpoint.
    heading = math.radians(start_yaw + sum(YAW_DELTAS))
    direction = (math.cos(heading), math.sin(heading))
    end = last[3].after
    forward_distance = ((end.x - slope_start.x) * direction[0]
                        + (end.y - slope_start.y) * direction[1])
    horizontal_distance = math.hypot(end.x - slope_start.x, end.y - slope_start.y)
    if forward_distance <= 1.0 or horizontal_distance <= 1.0:
        return inconclusive("InsufficientPhysicalProgress")

    max_height_gain = max(
        frame.after.z - slope_start.z for frame in run.trace if frame.action == 3
    )
    elapsed = 0
    first_gain = None
    for frame in run.trace:
        if frame.action == 3:
            elapsed += frame.msec
            if first_gain is None and frame.after.z - slope_start.z >= min_rise:
                first_gain = elapsed

    return SlopeProfile(
        outcome="Measured",
        reason="HeightGainObserved" if max_height_gain >= min_rise else "NoHeightGain",
        approach_x=approach.x,
        approach_y=approach.y,
        approach_z=approach.z,
        slope_start_x=slope_start.x,
        slope_start_y=slope_start.y,
        slope_start_z=slope_start.z,
        start_speed=math.hypot(slope_start.vx, slope_start.vy),
        end_speed=math.hypot(end.vx, end.vy),
        forward_distance=forward_distance,
        horizontal_distance=horizontal_distance,
        end_height_delta=end.z - slope_start.z,
        max_height_gain=max_height_gain,
        first_height_gain_ms=first_gain,
    )



def _measure_direct_slope(
    run: Bundle, map_name: str, heading_deg: float, min_rise: float
) -> SlopeProfile:
    """Evaluate the single 2000-ms ramp trial without its historical approach."""
    def inconclusive(reason: str) -> SlopeProfile:
        return SlopeProfile("Inconclusive", reason)

    if not math.isfinite(heading_deg) or not -180 <= heading_deg < 180:
        return inconclusive("InvalidHeading")
    if not math.isfinite(min_rise) or min_rise <= 0:
        return inconclusive("InvalidRiseThreshold")
    if run.name != "ground_slope_forward_02" or run.map != map_name:
        return inconclusive("ContextMismatch")
    if run.status != "Completed" or run.source != "live":
        return inconclusive("IncompleteRun")
    if run.command_ms != 2000:
        return inconclusive("DurationMismatch")
    if not run.trace or len(run.trace) != run.frames:
        return inconclusive("IncompleteTrace")
    if _pose_mismatch(run.start, run.trace[0].before, "start"):
        return inconclusive("StartObservationMismatch")
    if _pose_mismatch(run.end, run.trace[-1].after, "end"):
        return inconclusive("EndObservationMismatch")

    initial = run.trace[0].before
    if math.hypot(initial.vx, initial.vy) > 5.0 or abs(initial.vz) > 5.0:
        return inconclusive("InitialSpeedMismatch")
    cap = run.trace[0].max_speed
    if not math.isfinite(cap) or cap < 10.0:
        return inconclusive("InvalidSpeedCap")

    elapsed = 0
    first_gain_ms = None
    max_height_gain = -math.inf
    previous = None
    for index, frame in enumerate(run.trace):
        if frame.action != 0 or frame.index != index:
            return inconclusive("UnexpectedAction")
        if frame.msec < 1 or frame.msec > 255:
            return inconclusive("InvalidFrameDuration")
        elapsed += frame.msec
        if elapsed > 2000:
            return inconclusive("PhaseTimingMismatch")
        if previous is not None:
            old, new = previous.after, frame.before
            if (max(abs(getattr(old, name) - getattr(new, name))
                    for name in ("x", "y", "z", "vx", "vy", "vz"))
                    > POSITION_CONTINUITY_TOLERANCE
                    or old.ground != new.ground or old.duck != new.duck):
                return inconclusive("ObservationDiscontinuity")
        if not math.isfinite(frame.max_speed) or abs(frame.max_speed - cap) > COMMAND_TOLERANCE:
            return inconclusive("SpeedCapChanged")
        if (frame.before.ground != 1 or frame.after.ground != 1
                or frame.before.duck or frame.after.duck or frame.buttons != 0):
            return inconclusive("GroundContactOrStance")
        if (abs(frame.forward - cap) > COMMAND_TOLERANCE
                or abs(frame.strafe) > COMMAND_TOLERANCE):
            return inconclusive("UnexpectedMovementCommand")
        for yaw in (frame.yaw, frame.before.yaw, frame.after.yaw):
            if not math.isfinite(yaw) or _angle_difference(yaw, heading_deg) > YAW_TOLERANCE:
                return inconclusive("UnexpectedYaw")

        gain = frame.after.z - initial.z
        max_height_gain = max(max_height_gain, gain)
        if first_gain_ms is None and gain >= min_rise:
            first_gain_ms = elapsed
        previous = frame

    if elapsed != 2000:
        return inconclusive("PhaseTimingMismatch")
    end = run.trace[-1].after
    heading = math.radians(heading_deg)
    forward = ((end.x - initial.x) * math.cos(heading)
               + (end.y - initial.y) * math.sin(heading))
    distance = math.hypot(end.x - initial.x, end.y - initial.y)
    if forward <= 1.0 or distance <= 1.0:
        return inconclusive("InsufficientPhysicalProgress")

    return SlopeProfile(
        outcome="Measured",
        reason="HeightGainObserved" if max_height_gain >= min_rise else "NoHeightGain",
        approach_x=initial.x,
        approach_y=initial.y,
        approach_z=initial.z,
        slope_start_x=initial.x,
        slope_start_y=initial.y,
        slope_start_z=initial.z,
        start_speed=math.hypot(initial.vx, initial.vy),
        end_speed=math.hypot(end.vx, end.vy),
        forward_distance=forward,
        horizontal_distance=distance,
        end_height_delta=end.z - initial.z,
        max_height_gain=max_height_gain,
        first_height_gain_ms=first_gain_ms,
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path, help="Saved result.csv and trace.csv directory")
    parser.add_argument("--scenario", default="ground_slope_forward_01")
    parser.add_argument("--map", dest="map_name", default="de_dust2")
    parser.add_argument("--heading-deg", type=float, default=-105.0)
    parser.add_argument("--min-rise", type=float, default=4.0)
    args = parser.parse_args(argv)
    try:
        run = load_bundle(args.run)
    except InvalidBundle as exc:
        print(f"InvalidEvidence: {exc}", file=sys.stderr)
        return 2

    result = measure_slope_forward(
        run, args.scenario, args.map_name, args.heading_deg, min_rise=args.min_rise
    )
    print(f"{result.outcome}: {result.reason}")
    if result.outcome != "Measured":
        return 3
    print(f"approach=({result.approach_x:.6g},{result.approach_y:.6g},{result.approach_z:.6g}) "
          f"slope_start=({result.slope_start_x:.6g},{result.slope_start_y:.6g},"
          f"{result.slope_start_z:.6g})")
    print(f"start_speed={result.start_speed:.6g} end_speed={result.end_speed:.6g} "
          f"forward_distance={result.forward_distance:.6g} "
          f"horizontal_distance={result.horizontal_distance:.6g}")
    print(f"end_height_delta={result.end_height_delta:.6g} "
          f"max_height_gain={result.max_height_gain:.6g} "
          f"first_height_gain_ms={result.first_height_gain_ms if result.first_height_gain_ms is not None else 'not_observed'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
