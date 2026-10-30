"""Ground acceleration/coast measurement regression tests."""
import unittest
from dataclasses import replace

from tools.aipb_training.ground_motion_profile import measure_ground_motion
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
