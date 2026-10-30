"""Ground acceleration/coast measurement regression tests."""
import io
import unittest
from contextlib import redirect_stdout
from dataclasses import replace
from unittest.mock import patch

from tools.aipb_training.ground_motion_profile import main, measure_ground_motion
from tools.aipb_training.scenario_trace_compare import Bundle, Frame, Pose


def pose(x, speed, ground=1):
    return Pose(x, 0, -92, speed, 0, 0, 0, 0, ground, 0)


def trial(end=9, *, second_forward=0, second_ground=1):
    first = Frame(0, 0, 10, 100, 0, 250, 0, 0, 0, 0,
                  pose(0, 0), pose(5, 100))
    second = Frame(1, 1, 10, second_forward, 0, 250, 0, 0, 0, 0,
                   first.after, pose(end, 0, second_ground))
    return Bundle("ground_accel_coast_01", "de_dust2", 1, 2, 1,
                  "Completed", "finished", 2, 20, "live",
                  first.before, second.after, (first, second))


def strafe_pose(y, speed, ground=1):
    return Pose(0, y, -92, 0, speed, 0, 0, 0, ground, 0)


def strafe_trial(*, first_forward=0, first_strafe=100, coast_strafe=0):
    # At yaw zero, positive GoldSrc sidemove points toward negative Y.
    first = Frame(0, 0, 10, first_forward, first_strafe, 250, 0, 0, 0, 0,
                  strafe_pose(0, 0), strafe_pose(-5, -100))
    second = Frame(1, 1, 10, 0, coast_strafe, 250, 0, 0, 0, 0,
                   first.after, strafe_pose(-9, 0))
    return Bundle("ground_strafe_coast_01", "de_dust2", 1, 2, 1,
                  "Completed", "finished", 2, 20, "live",
                  first.before, second.after, (first, second))


class GroundMotionTests(unittest.TestCase):
    def measure(self, run):
        return measure_ground_motion(
            run, "ground_accel_coast_01", "de_dust2", 0, 10, 10
        )

    def test_measures_both_phases_and_observed_stop(self):
        result = self.measure(trial())
        self.assertEqual((result.outcome, result.reason), ("Measured", "StopObserved"))
        self.assertEqual((result.accel_distance, result.coast_distance), (5, 4))
        self.assertEqual((result.start_speed, result.release_speed, result.end_speed), (0, 100, 0))
        self.assertEqual((result.stop_time_ms, result.stop_distance), (10, 4))

    def test_stop_not_observed_is_not_a_failure(self):
        run = trial()
        second = replace(run.trace[-1], after=pose(9, 50))
        run = replace(run, end=second.after, trace=(run.trace[0], second))
        self.assertEqual(self.measure(run).reason, "StopNotObserved")

    def test_unexpected_movement_rejected(self):
        self.assertEqual(self.measure(trial(second_forward=100)).reason,
                         "UnexpectedMovementCommand")
        self.assertEqual(self.measure(trial(second_ground=0)).reason,
                         "LeftGroundOrStanceChanged")

    def test_strafe_motion_uses_player_right_vector(self):
        run = strafe_trial()
        outcome = measure_ground_motion(
            run, "ground_strafe_coast_01", "de_dust2", 0, 10, 10,
            axis="strafe"
        )
        self.assertEqual((outcome.outcome, outcome.reason), ("Measured", "StopObserved"))
        self.assertAlmostEqual(outcome.accel_distance, 5)
        self.assertAlmostEqual(outcome.coast_distance, 4)
        self.assertAlmostEqual(outcome.release_speed, 100)
        self.assertAlmostEqual(outcome.end_speed, 0)
        self.assertEqual((outcome.stop_time_ms, outcome.stop_distance), (10, 4))

    def test_strafe_rejects_wrong_or_mixed_commands(self):
        cases = (
            (strafe_trial(first_strafe=0), "UnexpectedMovementCommand"),
            (strafe_trial(first_strafe=-100), "UnexpectedMovementCommand"),
            (strafe_trial(first_forward=50), "UnexpectedMovementCommand"),
            (strafe_trial(coast_strafe=50), "UnexpectedMovementCommand"),
        )
        for run, reason in cases:
            with self.subTest(reason=reason, run=run):
                self.assertEqual(measure_ground_motion(
                    run, "ground_strafe_coast_01", "de_dust2", 0, 10, 10,
                    axis="strafe"
                ).reason, reason)
        self.assertEqual(measure_ground_motion(
            strafe_trial(), "ground_strafe_coast_01", "de_dust2",
            0, 10, 10
        ).reason, "UnexpectedMovementCommand")
        self.assertEqual(measure_ground_motion(
            strafe_trial(), "ground_strafe_coast_01", "de_dust2",
            0, 10, 10, axis="sideways"
        ).reason, "InvalidAxis")

    def test_strafe_cli_and_backward_compatibility(self):
        with patch("tools.aipb_training.ground_motion_profile.load_bundle",
                   return_value=strafe_trial()):
            with redirect_stdout(io.StringIO()) as output:
                code = main([
                    "unused", "--scenario", "ground_strafe_coast_01",
                    "--heading-deg", "0", "--accel-ms", "10",
                    "--coast-ms", "10", "--axis", "strafe",
                ])
        self.assertEqual(code, 0)
        self.assertIn("Measured: StopObserved", output.getvalue())
        self.assertIn("accel_distance=5", output.getvalue())
        self.assertEqual(self.measure(trial()).outcome, "Measured")

    def test_partial_or_inconsistent_evidence(self):
        self.assertEqual(self.measure(replace(trial(), status="Cancelled")).reason,
                         "IncompleteRun")
        self.assertEqual(self.measure(replace(trial(), command_ms=19)).reason,
                         "DurationMismatch")
        self.assertEqual(self.measure(replace(trial(), end=pose(19, 0))).reason,
                         "EndObservationMismatch")
        run = trial()
        second = replace(run.trace[-1], before=pose(7, 100))
        self.assertEqual(self.measure(replace(run, trace=(run.trace[0], second))).reason,
                         "ObservationDiscontinuity")


if __name__ == "__main__":
    unittest.main()
