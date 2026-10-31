"""Read-only Stage C jump trajectory measurements from bounded Scenario Runner traces.

A landing is an observed physics event, not proof of obstacle clearance.
"""

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
    Bundle, InvalidBundle, Pose, _pose_mismatch, load_bundle,
)

# (duration_ms, forward fraction, jump button, duck button) per D304 fixture.
PROGRAMS = {
    "jump_control_nojump_01": ((1500, 0, False, False),),
    "jump_standing_01": ((60, 0, True, False), (1440, 0, False, False)),
    "jump_forward_01": ((60, 1, True, False), (340, 1, False, False),
                        (1100, 0, False, False)),
    "jump_runup_short_01": ((200, 1, False, False), (60, 1, True, False),
                            (340, 1, False, False), (900, 0, False, False)),
    "jump_runup_long_01": ((500, 1, False, False), (60, 1, True, False),
                           (340, 1, False, False), (900, 0, False, False)),
    "jump_crouch_01": ((60, 0, True, False), (250, 0, False, True),
                       (1190, 0, False, False)),
    "jump_box_plain_01": ((200, 1, False, False), (60, 1, True, False),
                          (340, 1, False, False), (900, 0, False, False)),
    "jump_box_crouch_01": ((200, 1, False, False), (60, 1, True, False),
                           (250, 1, False, True), (90, 1, False, False),
                           (900, 0, False, False)),
    "jump_gap_runup_01": ((500, 1, False, False), (60, 1, True, False),
                          (340, 1, False, False), (900, 0, False, False)),
}

JUMP_BUTTON = 2
DUCK_BUTTON = 4
FRAME_LIMIT_MS = 20
SPEED_TOLERANCE = 1.0
START_VELOCITY_LIMIT = 5.0
LANDING_STABILITY_MS = 40
CONTROL_POSITION_TOLERANCE = 1.0


@dataclass(frozen=True)
class JumpProfile:
    outcome: str
    reason: str
    frames: int
    observed_ms: int
    takeoff_ms: int | None = None
    landing_ms: int | None = None
    flight_ms: int | None = None
    takeoff_speed: float = math.nan
    apex_rise: float = math.nan
    horizontal_travel: float = math.nan
    landing_rise: float = math.nan
    crouch_observed: bool = False


def measure_jump(run: Bundle) -> JumpProfile:
    """Conservatively measure observed events; never infer a safe geometry route."""
    def report(outcome: str, reason: str, **values: object) -> JumpProfile:
        return JumpProfile(outcome, reason, run.frames, run.command_ms, **values)

    program = PROGRAMS.get(run.name)
    if program is None or run.map != "de_dust2" or run.bot != 1:
        return report("Inconclusive", "ContextMismatch")
    if not run.trace or run.frames > len(run.trace):
        return report("Inconclusive", "IncompleteTrace")
    if run.start.ground != 1 or run.start.duck != 0:
        return report("Inconclusive", "InvalidStartStance")
    if math.dist((run.start.vx, run.start.vy, run.start.vz), (0, 0, 0)) > START_VELOCITY_LIMIT:
        return report("Inconclusive", "InitialSpeedMismatch")
    if _pose_mismatch(run.start, run.trace[0].before, "start"):
        return report("Inconclusive", "StartObservationMismatch")

    # The parser permits one uncommitted frame on a failed run.
    # Only frames committed by the scheduler may contribute physical evidence.
    frames = run.trace[:run.frames]
    elapsed = 0
    phase_ms = 0
    phase = 0
    takeoff_ms = None
    landing_ms = None
    takeoff_speed = math.nan
    landing_rise = math.nan
    apex_rise = 0.0
    crouch_observed = False
    first_airborne_was_jump = False
    extra_flight = False
    stable_ms = 0
    previous = None
    start = run.trace[0].before
    last = start

    for frame in frames:
        if frame.msec > FRAME_LIMIT_MS:
            return report("Inconclusive", "CoarseFrameSampling")
        if frame.action != phase:
            if (frame.action != phase + 1 or phase_ms != program[phase][0]
                    or phase + 1 >= len(program)):
                return report("Inconclusive", "ActionSequenceMismatch")
            phase += 1
            phase_ms = 0
        duration, forward, jump, duck = program[phase]
        if phase_ms + frame.msec > duration:
            return report("Inconclusive", "ActionTimingMismatch")
        if (frame.buttons != (JUMP_BUTTON if jump else 0) | (DUCK_BUTTON if duck else 0)
                or abs(frame.strafe) > SPEED_TOLERANCE
                or frame.max_speed < 10.0
                or abs(frame.forward - forward * frame.max_speed) > SPEED_TOLERANCE):
            return report("Inconclusive", "CommandMismatch")
        if previous is not None and _pose_mismatch(previous, frame.before, "continuity"):
            return report("Inconclusive", "ObservationDiscontinuity")
        phase_ms += frame.msec
        elapsed += frame.msec
        apex_rise = max(apex_rise, frame.before.z - start.z, frame.after.z - start.z)
        crouch_observed |= bool(frame.before.duck or frame.after.duck)
        if takeoff_ms is None:
            if frame.before.ground == 1 and frame.after.ground == 0:
                takeoff_ms = elapsed
                takeoff_speed = math.hypot(frame.after.vx, frame.after.vy)
                first_airborne_was_jump = bool(frame.buttons & JUMP_BUTTON)
            elif frame.before.ground == 0:
                return report("Inconclusive", "MissingTakeoffTransition")
        elif landing_ms is None:
            if frame.before.ground == 0 and frame.after.ground == 1:
                landing_ms = elapsed
                landing_rise = frame.after.z - start.z
            elif frame.before.ground == 1:
                return report("Inconclusive", "MissingLandingTransition")
        elif frame.before.ground == 0 or frame.after.ground == 0:
            extra_flight = True
        else:
            stable_ms += frame.msec
        last = frame.after
        previous = frame.after

    metrics = {
        "takeoff_ms": takeoff_ms,
        "landing_ms": landing_ms,
        "flight_ms": (landing_ms - takeoff_ms if landing_ms is not None
                      and takeoff_ms is not None else None),
        "takeoff_speed": takeoff_speed,
        "apex_rise": apex_rise,
        "horizontal_travel": math.hypot(last.x - start.x, last.y - start.y),
        "landing_rise": landing_rise,
        "crouch_observed": crouch_observed,
    }
    if elapsed != run.command_ms:
        return report("Inconclusive", "CommandDurationMismatch")
    if run.status != "Completed" or run.source != "live" or len(run.trace) != run.frames:
        return report("Inconclusive", "IncompleteRun", **metrics)
    if (phase != len(program) - 1 or phase_ms != program[-1][0]
            or run.command_ms != sum(action[0] for action in program)):
        return report("Inconclusive", "IncompleteActionProgram", **metrics)
    if _pose_mismatch(last, run.end, "end"):
        return report("Inconclusive", "EndObservationMismatch", **metrics)
    if not any(action[2] for action in program):
        if (takeoff_ms is None and last.ground == 1
                and math.dist((start.x, start.y, start.z),
                              (last.x, last.y, last.z)) <= CONTROL_POSITION_TOLERANCE):
            return report("GroundedControl", "StableWithoutJump", **metrics)
        return report("Inconclusive", "ControlDidNotRemainGrounded", **metrics)
    if takeoff_ms is None:
        return report("NoTakeoff", "NoAirborneTransition", **metrics)
    if not first_airborne_was_jump:
        return report("Inconclusive", "FallBeforeJump", **metrics)
    if landing_ms is None or last.ground != 1:
        return report("Inconclusive", "NoLandingObserved", **metrics)
    if extra_flight:
        return report("Inconclusive", "AdditionalFlight", **metrics)
    if stable_ms < LANDING_STABILITY_MS:
        return report("Inconclusive", "UnstableLanding", **metrics)
    return report("Landed", "ObservedStableLanding", **metrics)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path, help="Directory with result.csv and trace.csv")
    args = parser.parse_args(argv)
    try:
        profile = measure_jump(load_bundle(args.run))
    except InvalidBundle as exc:
        print(f"InvalidEvidence: {exc}", file=sys.stderr)
        return 2

    print(f"{profile.outcome}: {profile.reason}")
    def metric(value: float) -> str:
        return f"{value:.6g}" if math.isfinite(value) else "unknown"

    print(f"frames={profile.frames} command_ms={profile.observed_ms} "
          f"takeoff_ms={profile.takeoff_ms} landing_ms={profile.landing_ms} "
          f"flight_ms={profile.flight_ms} "
          f"takeoff_speed={metric(profile.takeoff_speed)} "
          f"apex_rise={metric(profile.apex_rise)} "
          f"horizontal_travel={metric(profile.horizontal_travel)} "
          f"landing_rise={metric(profile.landing_rise)} "
          f"crouch_observed={int(profile.crouch_observed)}")
    return {"GroundedControl": 0, "Landed": 0, "NoTakeoff": 1,
            "Inconclusive": 3}[profile.outcome]


if __name__ == "__main__":
    raise SystemExit(main())
