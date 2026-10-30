"""Read-only active-ground-braking evidence from a two-phase scenario trace.

A zero-speed crossing is estimated between engine samples, not observed at a
physical instant; a collision may also account for lost speed.
"""

from __future__ import annotations

import argparse
import math
import sys
from dataclasses import dataclass
from pathlib import Path

from tools.aipb_training.scenario_trace_compare import (
    Bundle, InvalidBundle, Pose, _angle_difference, _pose_mismatch, load_bundle
)

COMMAND_TOLERANCE = 0.01
HEADING_TOLERANCE = 1.0
MAX_CROSSING_FRAME_MS = 20


@dataclass(frozen=True)
class ActiveBrakeProfile:
    outcome: str
    reason: str
    accel_distance: float = math.nan
    brake_displacement: float = math.nan
    initial_speed: float = math.nan
    release_speed: float = math.nan
    end_speed: float = math.nan
    stop_time_ms: float | None = None
    stop_distance: float | None = None
    reverse_observed: bool = False


def measure_active_braking(
    run: Bundle,
    scenario: str,
    map_name: str,
    heading_deg: float,
    accel_ms: int,
    brake_ms: int,
    stop_speed: float = 5.0,
) -> ActiveBrakeProfile:
    """Measure projected braking and possible direction reversal, not causation."""
    def uncertain(reason: str) -> ActiveBrakeProfile:
        return ActiveBrakeProfile("Inconclusive", reason)

    if not math.isfinite(heading_deg) or not -180 <= heading_deg < 180:
        return uncertain("InvalidHeading")
    if not math.isfinite(stop_speed) or stop_speed <= 0:
        return uncertain("InvalidStopThreshold")
    if accel_ms <= 0 or brake_ms <= 0 or accel_ms + brake_ms > 60000:
        return uncertain("InvalidDuration")
    if (run.name, run.map) != (scenario, map_name):
        return uncertain("ContextMismatch")
    if run.status != "Completed" or run.source != "live":
        return uncertain("IncompleteRun")
    if run.command_ms != accel_ms + brake_ms:
        return uncertain("DurationMismatch")
    if not run.trace or len(run.trace) != run.frames:
        return uncertain("IncompleteTrace")
    if _pose_mismatch(run.start, run.trace[0].before, "start"):
        return uncertain("StartObservationMismatch")
    if _pose_mismatch(run.end, run.trace[-1].after, "end"):
        return uncertain("EndObservationMismatch")

    radians = math.radians(heading_deg)
    direction = (math.cos(radians), math.sin(radians))

    def speed(pose: Pose) -> float:
        return pose.vx * direction[0] + pose.vy * direction[1]

    def travel(before: Pose, after: Pose) -> float:
        return ((after.x - before.x) * direction[0]
                + (after.y - before.y) * direction[1])

    durations = [0, 0]
    brake_index = None
    previous = None
    initial_cap = run.trace[0].max_speed
    for index, frame in enumerate(run.trace):
        if frame.action not in (0, 1) or (
            index and frame.action < run.trace[index - 1].action
        ):
            return uncertain("UnexpectedAction")
        if frame.action == 1 and brake_index is None:
            brake_index = index
        durations[frame.action] += frame.msec

        if previous is not None and _pose_mismatch(previous.after, frame.before, "frame"):
            return uncertain("ObservationDiscontinuity")
        if (frame.before.ground != 1 or frame.after.ground != 1
                or frame.before.duck or frame.after.duck or frame.buttons != 0):
            return uncertain("GroundContactOrStance")
        if not math.isfinite(frame.max_speed) or frame.max_speed <= 0:
            return uncertain("InvalidSpeedCap")
        if abs(frame.max_speed - initial_cap) > COMMAND_TOLERANCE:
            return uncertain("SpeedCapChanged")
        expected = frame.max_speed if frame.action == 0 else -frame.max_speed
        if (abs(frame.forward - expected) > COMMAND_TOLERANCE
                or abs(frame.strafe) > COMMAND_TOLERANCE
                or _angle_difference(frame.yaw, heading_deg) > HEADING_TOLERANCE
                or _angle_difference(frame.before.yaw, heading_deg) > HEADING_TOLERANCE
                or _angle_difference(frame.after.yaw, heading_deg) > HEADING_TOLERANCE):
            return uncertain("UnexpectedMovementCommand")
        previous = frame

    if brake_index is None or brake_index == 0 or durations != [accel_ms, brake_ms]:
        return uncertain("PhaseTimingMismatch")

    start = run.trace[0].before
    release = run.trace[brake_index - 1].after
    end = run.trace[-1].after
    initial_speed = speed(start)
    release_speed = speed(release)
    if abs(initial_speed) > stop_speed:
        return uncertain("InitialSpeedMismatch")
    if release_speed <= stop_speed:
        return uncertain("InsufficientEntrySpeed")

    braking = run.trace[brake_index:]
    reverse_observed = any(speed(frame.after) < -stop_speed for frame in braking)
    elapsed = 0
    stop_time = None
    stop_distance = None
    stop_reason = "StopNotObserved"
    for index, frame in enumerate(braking):
        before_speed = speed(frame.before)
        after_speed = speed(frame.after)
        if before_speed > 0 and after_speed <= 0:
            # A sign change can skip zero between physical engine frames.
            # Interpolation is an estimate; reject undersampled crossings.
            if frame.msec > MAX_CROSSING_FRAME_MS:
                return uncertain("CoarseStopSampling")
            fraction = before_speed / (before_speed - after_speed)
            stop_time = elapsed + fraction * frame.msec
            stop_distance = travel(release, frame.before) + fraction * travel(
                frame.before, frame.after
            )
            stop_reason = "InterpolatedZeroCrossing"
            break
        if (abs(after_speed) <= stop_speed
                and all(abs(speed(later.after)) <= stop_speed
                        for later in braking[index:])):
            stop_time = float(elapsed + frame.msec)
            stop_distance = travel(release, frame.after)
            stop_reason = "SustainedNearStop"
            break
        elapsed += frame.msec

    return ActiveBrakeProfile(
        outcome="Measured",
        reason=stop_reason,
        accel_distance=travel(start, release),
        brake_displacement=travel(release, end),
        initial_speed=initial_speed,
        release_speed=release_speed,
        end_speed=speed(end),
        stop_time_ms=stop_time,
        stop_distance=stop_distance,
        reverse_observed=reverse_observed,
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path, help="Saved result.csv and trace.csv directory")
    parser.add_argument("--scenario", default="ground_active_brake_01")
    parser.add_argument("--map", dest="map_name", default="de_dust2")
    parser.add_argument("--heading-deg", type=float, required=True)
    parser.add_argument("--accel-ms", type=int, default=500)
    parser.add_argument("--brake-ms", type=int, default=500)
    parser.add_argument("--stop-speed", type=float, default=5.0)
    args = parser.parse_args(argv)

    try:
        run = load_bundle(args.run)
    except InvalidBundle as exc:
        print(f"InvalidEvidence: {exc}", file=sys.stderr)
        return 2

    result = measure_active_braking(
        run, args.scenario, args.map_name, args.heading_deg,
        args.accel_ms, args.brake_ms, args.stop_speed
    )
    print(f"{result.outcome}: {result.reason}")
    if result.outcome != "Measured":
        return 3

    print(f"initial_speed={result.initial_speed:.6g} "
          f"release_speed={result.release_speed:.6g} end_speed={result.end_speed:.6g} "
          f"accel_distance={result.accel_distance:.6g} "
          f"brake_displacement={result.brake_displacement:.6g}")
    print(f"stop_time_ms={f'{result.stop_time_ms:.6g}' if result.stop_time_ms is not None else 'not_observed'} "
          f"stop_distance={f'{result.stop_distance:.6g}' if result.stop_distance is not None else 'not_observed'} "
          f"reverse_observed={int(result.reverse_observed)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
