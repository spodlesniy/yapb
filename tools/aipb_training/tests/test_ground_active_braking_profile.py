"""D292 active-braking command and measurement regressions."""

from __future__ import annotations

if __name__ == "__main__" and not __package__:
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
    __package__ = "tools.aipb_training.tests"

import io
import unittest
from contextlib import redirect_stdout, redirect_stderr
from dataclasses import replace
from unittest.mock import patch

from tools.aipb_training.ground_active_braking_profile import (
    main, measure_active_braking
)
from tools.aipb_training.scenario_trace_compare import Bundle, Frame, Pose, InvalidBundle


def pose(x: float, vx: float, *, ground: int = 1, duck: int = 0) -> Pose:
    return Pose(x, 0, -92, vx, 0, 0, 0, 0, ground, duck)


def trial() -> Bundle:
    start = pose(0, 0)
    release = pose(5, 100)
    first = Frame(0, 0, 10, 250, 0, 250, 0, 0, 0, 0, start, release)
    mid = pose(5.6, 30)
    first_brake = Frame(1, 1, 10, -250, 0, 250, 0, 0, 0, 0, release, mid)
    crossed = pose(5.7, -20)
    second_brake = Frame(2, 1, 10, -250, 0, 250, 0, 0, 0, 0, mid, crossed)
    end = pose(5.2, -70)
    last_brake = Frame(3, 1, 10, -250, 0, 250, 0, 0, 0, 0, crossed, end)
    return Bundle("ground_active_brake_01", "de_dust2", 1, 2, 1,
                  "Completed", "finished", 4, 40, "live",
                  start, end, (first, first_brake, second_brake, last_brake))


def analyze(run: Bundle, *, accel_ms: int = 10, brake_ms: int = 30,
            stop_speed: float = 5) -> object:
    return measure_active_braking(
        run, "ground_active_brake_01", "de_dust2",
        0, accel_ms, brake_ms, stop_speed
    )


class ActiveBrakingTests(unittest.TestCase):
    def test_interpolates_zero_crossing_without_claiming_exact_stop(self) -> None:
        result = analyze(trial())
        self.assertEqual((result.outcome, result.reason),
                         ("Measured", "InterpolatedZeroCrossing"))
        self.assertAlmostEqual(result.accel_distance, 5)
        self.assertAlmostEqual(result.brake_displacement, 0.2)
        self.assertAlmostEqual(result.stop_time_ms, 16)
        self.assertAlmostEqual(result.stop_distance, 0.66)
        self.assertEqual((result.initial_speed, result.release_speed, result.end_speed),
                         (0, 100, -70))
        self.assertTrue(result.reverse_observed)

    def test_near_stop_without_reverse(self) -> None:
        run = trial()
        second = replace(run.trace[2], after=pose(5.7, 3))
        last = replace(run.trace[3], before=second.after,
                       after=pose(5.73, 3))
        case = replace(run, end=last.after,
                       trace=(run.trace[0], run.trace[1], second, last))
        result = analyze(case)
        self.assertEqual((result.outcome, result.reason),
                         ("Measured", "SustainedNearStop"))
        self.assertEqual(result.stop_time_ms, 20)
        self.assertAlmostEqual(result.stop_distance, 0.7)
        self.assertFalse(result.reverse_observed)

    def test_stop_not_observed_is_not_success_or_failure(self) -> None:
        run = trial()
        mid = replace(run.trace[1], after=pose(5.6, 70))
        next_frame = replace(run.trace[2], before=mid.after,
                             after=pose(6.1, 45))
        last = replace(run.trace[3], before=next_frame.after,
                       after=pose(6.4, 20))
        run = replace(run, end=last.after,
                      trace=(run.trace[0], mid, next_frame, last))
        result = analyze(run)
        self.assertEqual((result.outcome, result.reason),
                         ("Measured", "StopNotObserved"))
        self.assertIsNone(result.stop_time_ms)
        self.assertIsNone(result.stop_distance)
        self.assertFalse(result.reverse_observed)

    def test_undersampled_zero_crossing_is_inconclusive(self) -> None:
        run = trial()
        long = replace(run.trace[2], msec=30)
        case = replace(run, command_ms=60,
                       trace=(run.trace[0], run.trace[1], long, run.trace[3]))
        self.assertEqual(analyze(case, brake_ms=50).reason, "CoarseStopSampling")

    def test_invalid_commands_and_observations(self) -> None:
        run = trial()
        first, second, third, fourth = run.trace
        cases = (
            (replace(run, trace=(replace(first, forward=200), second, third, fourth)),
             "UnexpectedMovementCommand"),
            (replace(run, trace=(first, replace(second, forward=250), third, fourth)),
             "UnexpectedMovementCommand"),
            (replace(run, trace=(first, replace(second, strafe=100), third, fourth)),
             "UnexpectedMovementCommand"),
            (replace(run, trace=(first, replace(second, yaw=5), third, fourth)),
             "UnexpectedMovementCommand"),
            (replace(run, trace=(first, replace(second, max_speed=200), third, fourth)),
             "SpeedCapChanged"),
            (replace(run, trace=(first, replace(second, buttons=2), third, fourth)),
             "GroundContactOrStance"),
            (replace(run, trace=(first, replace(second, after=pose(5.6, 30, ground=0)),
                                 third, fourth)), "GroundContactOrStance"),
            (replace(run, trace=(first, replace(second, before=pose(20, 100)),
                                 third, fourth)), "ObservationDiscontinuity"),
            (replace(run, trace=(first, replace(second, action=0), third, fourth)),
             "UnexpectedMovementCommand"),
            (replace(run, trace=(first, second, replace(third, action=0), fourth)),
             "UnexpectedAction"),
        )
        for case, reason in cases:
            with self.subTest(reason=reason, case=case):
                self.assertEqual(analyze(case).reason, reason)

    def test_rejects_untrusted_or_unpowered_evidence(self) -> None:
        run = trial()
        first = run.trace[0]
        no_speed = replace(first, after=pose(5, 0))
        bad_cases = (
            (replace(run, status="Cancelled"), "IncompleteRun"),
            (replace(run, source="last_committed"), "IncompleteRun"),
            (replace(run, command_ms=39), "DurationMismatch"),
            (replace(run, frames=3), "IncompleteTrace"),
            (replace(run, start=pose(7, 0)), "StartObservationMismatch"),
            (replace(run, end=pose(20, -70)), "EndObservationMismatch"),
            (replace(run, trace=(replace(first, before=pose(0, 50)), *run.trace[1:]),
                     start=pose(0, 50)), "InitialSpeedMismatch"),
            (replace(run, trace=(no_speed,
                                 replace(run.trace[1], before=no_speed.after),
                                 *run.trace[2:])), "InsufficientEntrySpeed"),
        )
        for case, reason in bad_cases:
            with self.subTest(reason=reason):
                self.assertEqual(analyze(case).reason, reason)
        self.assertEqual(analyze(run, stop_speed=0).reason, "InvalidStopThreshold")
        self.assertEqual(analyze(run, accel_ms=0).reason, "InvalidDuration")
        self.assertEqual(measure_active_braking(
            run, "wrong", "de_dust2", 0, 10, 30
        ).reason, "ContextMismatch")

    def test_cli_and_invalid_bundle(self) -> None:
        with patch("tools.aipb_training.ground_active_braking_profile.load_bundle",
                   return_value=trial()):
            with redirect_stdout(io.StringIO()) as output:
                code = main(["unused", "--heading-deg", "0",
                             "--accel-ms", "10", "--brake-ms", "30"])
        self.assertEqual(code, 0)
        self.assertIn("Measured: InterpolatedZeroCrossing", output.getvalue())
        self.assertIn("stop_time_ms=16", output.getvalue())
        with patch("tools.aipb_training.ground_active_braking_profile.load_bundle",
                   side_effect=InvalidBundle("bad")):
            with redirect_stderr(io.StringIO()) as error:
                code = main(["unused", "--heading-deg", "0"])
        self.assertEqual(code, 2)
        self.assertIn("InvalidEvidence: bad", error.getvalue())


if __name__ == "__main__":
    unittest.main()
