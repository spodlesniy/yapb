"""Read-only ground acceleration and coast measurements from a completed trace.

Measurements describe one observed trial, not traversability or engine guarantees.
"""

from __future__ import annotations

import argparse
import math
import sys
from dataclasses import dataclass
from pathlib import Path

from tools.aipb_training.scenario_trace_compare import (
    InvalidBundle, Bundle, Pose, _angle_difference, _pose_mismatch, load_bundle
)

COMMAND_TOLERANCE = 0.01
HEADING_TOLERANCE = 1.0


@dataclass(frozen=True)
class MotionProfile:
    outcome: str
    reason: str
    accel_distance: float = math.nan
    coast_distance: float = math.nan
    start_speed: float = math.nan
    release_speed: float = math.nan
    end_speed: float = math.nan
    average_acceleration: float = math.nan
    average_coast_deceleration: float = math.nan
    stop_time_ms: int | None = None
    stop_distance: float | None = None


def measure_ground_motion(
    run: Bundle,
    scenario: str,
    map_name: str,
    heading_deg: float,
    accel_ms: int,
    coast_ms: int,
    stop_speed: float = 5.0,
    *,
    axis: str = "forward",
) -> MotionProfile:
    """Measure one fully observed, two-action, grounded acceleration/coast trial."""
    def uncertain(reason: str) -> MotionProfile:
        return MotionProfile("Inconclusive", reason)

    if axis not in ("forward", "strafe"):
        return uncertain("InvalidAxis")
    if not math.isfinite(heading_deg) or not -180 <= heading_deg < 180:
        return uncertain("InvalidHeading")
    if not math.isfinite(stop_speed) or stop_speed <= 0:
        return uncertain("InvalidStopThreshold")
    if accel_ms <= 0 or coast_ms <= 0:
        return uncertain("InvalidDuration")
    if (run.name, run.map) != (scenario, map_name):
        return uncertain("ContextMismatch")
    if run.status != "Completed" or run.source != "live":
        return uncertain("IncompleteRun")
    if run.command_ms != accel_ms + coast_ms:
        return uncertain("DurationMismatch")
    if not run.trace or len(run.trace) != run.frames:
        return uncertain("IncompleteTrace")

    if _pose_mismatch(run.start, run.trace[0].before, "start"):
        return uncertain("StartObservationMismatch")
    if _pose_mismatch(run.end, run.trace[-1].after, "end"):
        return uncertain("EndObservationMismatch")

    duration = [0, 0]
    release_index = None
    previous = None
    for index, frame in enumerate(run.trace):
        if frame.action not in (0, 1) or (index and frame.action < run.trace[index - 1].action):
            return uncertain("UnexpectedAction")
        if frame.action == 0 and release_index is not None:
            return uncertain("UnexpectedAction")
        if frame.action == 1 and release_index is None:
            release_index = index
        duration[frame.action] += frame.msec
        if previous is not None and _pose_mismatch(previous.after, frame.before, "frame"):
            return uncertain("ObservationDiscontinuity")
        if (frame.before.ground != 1 or frame.after.ground != 1
                or frame.before.duck or frame.after.duck
                or frame.buttons & 6):
            return uncertain("LeftGroundOrStanceChanged")
        if frame.max_speed <= 0 or not math.isfinite(frame.max_speed):
            return uncertain("InvalidSpeedCap")
        driven = frame.forward if axis == "forward" else frame.strafe
        cross = frame.strafe if axis == "forward" else frame.forward
        if (abs(cross) > COMMAND_TOLERANCE
                or (frame.action == 0 and driven <= COMMAND_TOLERANCE)
                or (frame.action == 1 and abs(driven) > COMMAND_TOLERANCE)
                or _angle_difference(frame.yaw, heading_deg) > HEADING_TOLERANCE
                or _angle_difference(frame.before.yaw, heading_deg) > HEADING_TOLERANCE
                or _angle_difference(frame.after.yaw, heading_deg) > HEADING_TOLERANCE):
            return uncertain("UnexpectedMovementCommand")
        previous = frame

    if release_index is None or duration != [accel_ms, coast_ms]:
        return uncertain("PhaseTimingMismatch")

    # GoldSrc's positive sidemove follows the right vector (yaw - 90 degrees).
    movement_heading = heading_deg - 90.0 if axis == "strafe" else heading_deg
    radians = math.radians(movement_heading)
    direction = (math.cos(radians), math.sin(radians))

    def speed(pose: Pose) -> float:
        return pose.vx * direction[0] + pose.vy * direction[1]

    def travel(start: Pose, end: Pose) -> float:
        return ((end.x - start.x) * direction[0]
                + (end.y - start.y) * direction[1])

    start = run.trace[0].before
    release = run.trace[release_index - 1].after
    end = run.trace[-1].after
    initial_speed = speed(start)
    released_speed = speed(release)
    terminal_speed = speed(end)
    elapsed_coast = 0
    stop_time = None
    stop_distance = None
    # Require the observed speed to remain below threshold for the remaining
    # coast frames, rather than treating a transient low reading as stopped.
    for frame in run.trace[release_index:]:
        elapsed_coast += frame.msec
        if stop_time is None and all(
            abs(speed(later.after)) <= stop_speed
            for later in run.trace[frame.index:]):
            stop_time = elapsed_coast
            stop_distance = travel(release, frame.after)

    return MotionProfile(
        outcome="Measured",
        reason="StopObserved" if stop_time is not None else "StopNotObserved",
        accel_distance=travel(start, release),
        coast_distance=travel(release, end),
        start_speed=initial_speed,
        release_speed=released_speed,
        end_speed=terminal_speed,
        average_acceleration=(released_speed - initial_speed) * 1000.0 / accel_ms,
        average_coast_deceleration=(released_speed - terminal_speed) * 1000.0 / coast_ms,
        stop_time_ms=stop_time,
        stop_distance=stop_distance,
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path, help="Saved result.csv and trace.csv directory")
    parser.add_argument("--scenario", default="ground_accel_coast_01")
    parser.add_argument("--map", dest="map_name", default="de_dust2")
    parser.add_argument("--heading-deg", type=float, required=True,
                        help="Player view yaw; the projected strafe axis is yaw - 90 degrees")
    parser.add_argument("--axis", choices=("forward", "strafe"), default="forward")
    parser.add_argument("--accel-ms", type=int, required=True)
    parser.add_argument("--coast-ms", type=int, required=True)
    parser.add_argument("--stop-speed", type=float, default=5.0)
    args = parser.parse_args(argv)
    try:
        run = load_bundle(args.run)
    except InvalidBundle as exc:
        print(f"InvalidEvidence: {exc}", file=sys.stderr)
        return 2
    profile = measure_ground_motion(
        run, args.scenario, args.map_name, args.heading_deg,
        args.accel_ms, args.coast_ms, args.stop_speed, axis=args.axis
    )
    print(f"{profile.outcome}: {profile.reason}")
    if profile.outcome != "Measured":
        return 3
    print(f"start_speed={profile.start_speed:.6g} release_speed={profile.release_speed:.6g} "
          f"end_speed={profile.end_speed:.6g} "
          f"accel_distance={profile.accel_distance:.6g} "
          f"coast_distance={profile.coast_distance:.6g}")
    print(f"average_acceleration={profile.average_acceleration:.6g} "
          f"average_coast_deceleration={profile.average_coast_deceleration:.6g} "
          f"stop_time_ms={profile.stop_time_ms if profile.stop_time_ms is not None else 'not_observed'} "
          f"stop_distance={f'{profile.stop_distance:.6g}' if profile.stop_distance is not None else 'not_observed'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
