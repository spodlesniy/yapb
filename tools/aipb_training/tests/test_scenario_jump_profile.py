"""D306 regressions for Stage C jump physics reports (not geometry success)."""

import csv
import io
import math
import tempfile
import unittest
from contextlib import redirect_stdout
from dataclasses import replace
from pathlib import Path

from tools.aipb_training.scenario_jump_profile import PROGRAMS, main, measure_jump
from tools.aipb_training.scenario_trace_compare import (
    RESULT_VERSION, TRACE_COLUMNS, TRACE_VERSION, Bundle, Frame, Pose,
)


def run_fixture(name="jump_standing_01", mode="landed"):
    program = PROGRAMS[name]
    p = Pose(100, 200, 36, 0, 0, 0, 0, 0, 1, 0)
    start = p
    frames = []
    elapsed = 0
    takeoff_at = None
    for action_index, (duration, forward, jump, duck) in enumerate(program):
        for offset in range(0, duration, 10):
            ms = min(10, duration - offset)
            elapsed += ms
            if jump and takeoff_at is None and mode != "no_takeoff":
                takeoff_at = elapsed
            age = elapsed - takeoff_at if takeoff_at is not None else -1
            flying = takeoff_at is not None and (mode == "never_landed" or age < 200)
            height = (40.0 * math.sin(math.pi * age / 200)
                      if 0 <= age < 200 else 0.0)
            if mode == "never_landed" and age >= 200:
                height = 10.0
            vx = 100.0 if forward else 0.0
            q = replace(p, x=p.x + (vx * ms / 1000),
                        z=start.z + height, vx=vx,
                        vz=100.0 if flying else 0.0,
                        ground=0 if flying else 1,
                        duck=int(duck))
            buttons = (2 if jump else 0) | (4 if duck else 0)
            frames.append(Frame(len(frames), action_index, ms, 250.0 * forward,
                                0.0, 250.0, buttons, buttons, 0.0, 0.0, p, q))
            p = q
    return Bundle(name, "de_dust2", 1, 2, 1, "Completed", "finished",
                  len(frames), elapsed, "live", start, p, tuple(frames))


class JumpProfileTests(unittest.TestCase):
    def test_stable_jump_landings_and_crouch_input(self):
        for name in PROGRAMS:
            with self.subTest(name=name):
                profile = measure_jump(run_fixture(name))
                if name == "jump_control_nojump_01":
                    self.assertEqual((profile.outcome, profile.reason),
                                     ("GroundedControl", "StableWithoutJump"))
                    continue
                self.assertEqual((profile.outcome, profile.reason),
                                 ("Landed", "ObservedStableLanding"))
                self.assertIsNotNone(profile.takeoff_ms)
                self.assertIsNotNone(profile.landing_ms)
                self.assertGreater(profile.landing_ms, profile.takeoff_ms)
                self.assertGreater(profile.apex_rise, 35.0)
                self.assertGreaterEqual(profile.horizontal_travel, 0.0)
                self.assertEqual(profile.crouch_observed,
                                 name in {"jump_crouch_01", "jump_box_crouch_01"})
                self.assertEqual(profile.flight_ms, 200)

    def test_incomplete_airborne_is_not_landing(self):
        finished = run_fixture()
        partial = replace(finished, status="ActionTimeout", frames=6,
                          command_ms=60, trace=finished.trace[:6],
                          end=finished.trace[5].after)
        outcome = measure_jump(partial)
        self.assertEqual((outcome.outcome, outcome.reason),
                         ("Inconclusive", "IncompleteRun"))
        self.assertIsNotNone(outcome.takeoff_ms)
        self.assertIsNone(outcome.landing_ms)
        self.assertGreater(outcome.apex_rise, 0.0)

        cancelled = replace(partial, status="Cancelled")
        self.assertEqual(measure_jump(cancelled).reason, "IncompleteRun")
        lost = replace(finished, source="last_committed")
        self.assertEqual(measure_jump(lost).reason, "IncompleteRun")

    def test_completed_but_no_takeoff_or_no_landing(self):
        no_jump = measure_jump(run_fixture(mode="no_takeoff"))
        self.assertEqual((no_jump.outcome, no_jump.reason),
                         ("NoTakeoff", "NoAirborneTransition"))
        no_landing = measure_jump(run_fixture(mode="never_landed"))
        self.assertEqual((no_landing.outcome, no_landing.reason),
                         ("Inconclusive", "NoLandingObserved"))

    def test_reject_invalid_context_and_start(self):
        r = run_fixture()
        self.assertEqual(measure_jump(replace(r, map="de_inferno")).reason,
                         "ContextMismatch")
        self.assertEqual(measure_jump(replace(r, bot=2)).reason, "ContextMismatch")
        self.assertEqual(measure_jump(replace(r, name="unknown")).reason,
                         "ContextMismatch")
        self.assertEqual(measure_jump(replace(r, start=replace(r.start, z=42))).reason,
                         "StartObservationMismatch")
        self.assertEqual(measure_jump(replace(r, start=replace(r.start, ground=0))).reason,
                         "InvalidStartStance")
        self.assertEqual(measure_jump(replace(r, start=replace(r.start, vx=50))).reason,
                         "InitialSpeedMismatch")

    def test_commands_and_timing_fail_closed(self):
        r = run_fixture("jump_runup_short_01")
        frames = list(r.trace)
        frames[0] = replace(frames[0], buttons=2)
        self.assertEqual(measure_jump(replace(r, trace=tuple(frames))).reason,
                         "CommandMismatch")
        frames = list(r.trace)
        frames[0] = replace(frames[0], forward=0.0)
        self.assertEqual(measure_jump(replace(r, trace=tuple(frames))).reason,
                         "CommandMismatch")
        frames = list(r.trace)
        frames[0] = replace(frames[0], msec=22)
        self.assertEqual(measure_jump(replace(r, trace=tuple(frames))).reason,
                         "CoarseFrameSampling")
        frames = list(r.trace)
        frames[20] = replace(frames[20], action=3)
        self.assertEqual(measure_jump(replace(r, trace=tuple(frames))).reason,
                         "ActionSequenceMismatch")
        frames = list(r.trace)
        frames[10] = replace(frames[10],
                             before=replace(frames[10].before, x=600))
        self.assertEqual(measure_jump(replace(r, trace=tuple(frames))).reason,
                         "ObservationDiscontinuity")
        self.assertEqual(measure_jump(replace(r, command_ms=1400)).reason,
                         "CommandDurationMismatch")

    def test_uncommitted_frame_is_not_evidence(self):
        r = run_fixture()
        partial = replace(r, status="ActionTimeout", frames=5, command_ms=50,
                          trace=r.trace[:6], end=r.trace[4].after)
        measured = measure_jump(partial)
        self.assertEqual(measured.reason, "IncompleteRun")
        self.assertEqual(measured.observed_ms, 50)

    def test_end_observation_and_stable_ground_required(self):
        r = run_fixture()
        altered = replace(r, end=replace(r.end, x=r.end.x + 10))
        self.assertEqual(measure_jump(altered).reason, "EndObservationMismatch")
        # A jump that finishes almost immediately after landing is inconclusive.
        r = run_fixture()
        landing_frame = next(i for i, f in enumerate(r.trace)
                             if f.before.ground == 0 and f.after.ground == 1)
        short = replace(r, trace=r.trace[:landing_frame + 1],
                        frames=landing_frame + 1,
                        command_ms=(landing_frame + 1) * 10,
                        end=r.trace[landing_frame].after)
        self.assertEqual(measure_jump(short).reason, "IncompleteActionProgram")


def pose_values(p):
    return [p.x, p.y, p.z, p.vx, p.vy, p.vz,
            p.yaw, p.pitch, p.ground, p.duck]


class CliTests(unittest.TestCase):
    def test_valid_bundle_cli(self):
        r = run_fixture("jump_standing_01")
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            with (folder / "result.csv").open("w", encoding="utf-8", newline="") as file:
                csv.writer(file).writerow([
                    RESULT_VERSION, r.name, r.map, r.bot, r.team, r.round_id,
                    r.status, r.reason, r.frames, r.command_ms, 1000, 2500,
                    r.source, *pose_values(r.start), *pose_values(r.end),
                ])
            with (folder / "trace.csv").open("w", encoding="utf-8", newline="") as file:
                writer = csv.writer(file)
                writer.writerow([TRACE_VERSION])
                writer.writerow(TRACE_COLUMNS)
                for f in r.trace:
                    writer.writerow([f.index, f.action, f.msec, f.forward, f.strafe,
                                     f.max_speed, f.buttons, f.old_buttons,
                                     f.yaw, f.pitch, *pose_values(f.before),
                                     *pose_values(f.after)])
            with redirect_stdout(io.StringIO()) as output:
                status = main([str(folder)])
            self.assertEqual(status, 0)
            self.assertIn("Landed: ObservedStableLanding", output.getvalue())
            self.assertIn("flight_ms=200", output.getvalue())


if __name__ == "__main__":
    unittest.main()
