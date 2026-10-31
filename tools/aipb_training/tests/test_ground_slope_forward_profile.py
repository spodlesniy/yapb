"""D295 ground-slope trajectory regression tests."""

import io
import math
import unittest
from contextlib import redirect_stdout, redirect_stderr
from dataclasses import replace
from unittest.mock import patch

from tools.aipb_training.ground_slope_forward_profile import (
    main, measure_slope_forward
)
from tools.aipb_training.scenario_trace_compare import Bundle, Frame, Pose, InvalidBundle


def trial(*, rise=True):
    position = Pose(0, 0, -92, 0, 0, 0, -105, 0, 1, 0)
    initial = position
    frames = []
    phases = ((0, 50, 500, 0), (1, 25, 250, 45),
              (2, 1, 1, 90), (3, 200, 2000, 0))
    phase_yaw = -105
    for action, count, duration, delta in phases:
        for i in range(count):
            ms = 1 if action == 2 else 10
            yaw = phase_yaw + delta * (i + 1) / count
            before = replace(position, yaw=yaw)
            forward = 250 if action in (0, 3) else 0
            if action == 0:
                after = replace(before, x=before.x + 0.2, vx=100)
            elif action == 3:
                after = replace(before, x=before.x + 0.4,
                                y=before.y + 0.2,
                                z=before.z + (0.1 if rise else 0),
                                vx=200, vy=100)
            else:
                after = replace(before, vx=0, vy=0)
            frames.append(Frame(len(frames), action, ms, forward, 0,
                                250, 0, 0, yaw, 0, before, after))
            position = after
        phase_yaw += delta
    return Bundle("ground_slope_forward_01", "de_dust2", 1, 2, 1,
                  "Completed", "finished", len(frames), 2751, "live",
                  initial, position, tuple(frames))


def direct_trial(*, rise=True):
    # The current direct fixture starts stationary, facing world +X.
    start = Pose(415.624, 2247.172, -92, 0, 0, 0, 0, 0, 1, 0)
    position = start
    frames = []
    for index in range(200):
        after = replace(
            position, x=position.x + 2.0,
            z=position.z + (0.1 if rise else 0.0),
            vx=250,
        )
        frames.append(Frame(index, 0, 10, 250, 0, 250, 0, 0, 0, 0, position, after))
        position = after

    return Bundle(
        "ground_slope_forward_02", "de_dust2", 1, 2, 1,
        "Completed", "finished", len(frames), 2000, "live",
        start, position, tuple(frames),
    )


class SlopeProfileTests(unittest.TestCase):
    def test_rise_and_source_endpoint(self):
        result = measure_slope_forward(trial())
        self.assertEqual((result.outcome, result.reason),
                         ("Measured", "HeightGainObserved"))
        self.assertAlmostEqual(result.approach_x, 10)
        self.assertAlmostEqual(result.slope_start_x, 10)
        self.assertAlmostEqual(result.max_height_gain, 20, places=4)
        self.assertAlmostEqual(result.end_height_delta, 20, places=4)
        self.assertGreater(result.forward_distance, 1)
        self.assertIsNotNone(result.first_height_gain_ms)

    def test_flat_is_measured_but_not_ascent(self):
        result = measure_slope_forward(trial(rise=False))
        self.assertEqual((result.outcome, result.reason),
                         ("Measured", "NoHeightGain"))
        self.assertEqual(result.first_height_gain_ms, None)

    def test_untrusted_states_are_inconclusive(self):
        run = trial()
        samples = list(run.trace)
        cases = [(replace(run, status="Cancelled"), "IncompleteRun"),
                 (replace(run, command_ms=2750), "DurationMismatch"),
                 (replace(run, frames=275), "IncompleteTrace")]
        altered = list(samples)
        altered[55] = replace(altered[55], yaw=55)
        cases.append((replace(run, trace=tuple(altered)), "UnexpectedYaw"))
        altered = list(samples)
        altered[-1] = replace(altered[-1], forward=0)
        cases.append((replace(run, trace=tuple(altered)), "UnexpectedMovementCommand"))
        altered = list(samples)
        altered[-1] = replace(altered[-1], after=replace(altered[-1].after, ground=0))
        cases.append((replace(run, trace=tuple(altered), end=altered[-1].after),
                      "GroundContactOrStance"))
        for item, reason in cases:
            with self.subTest(reason=reason):
                self.assertEqual(measure_slope_forward(item).reason, reason)

    def test_direct_slope_measures_only_uphill_action(self):
        run = direct_trial()
        result = measure_slope_forward(run, scenario="ground_slope_forward_02")
        self.assertEqual((result.outcome, result.reason),
                         ("Measured", "HeightGainObserved"))
        self.assertEqual(result.start_speed, 0)
        self.assertAlmostEqual(result.approach_x, result.slope_start_x)
        self.assertAlmostEqual(result.max_height_gain, 20)
        self.assertAlmostEqual(result.end_height_delta, 20)
        self.assertAlmostEqual(result.forward_distance, 400)
        self.assertAlmostEqual(result.horizontal_distance, 400)
        self.assertAlmostEqual(result.slope_start_y, 2247.172)
        self.assertIsNotNone(result.first_height_gain_ms)

    def test_direct_slope_flat_and_failure_contract(self):
        run = direct_trial(rise=False)
        profile = measure_slope_forward(run, scenario="ground_slope_forward_02")
        self.assertEqual((profile.outcome, profile.reason), ("Measured", "NoHeightGain"))
        self.assertIsNone(profile.first_height_gain_ms)

        def inspect(case):
            return measure_slope_forward(case, scenario="ground_slope_forward_02").reason

        self.assertEqual(inspect(replace(run, command_ms=1999)), "DurationMismatch")
        self.assertEqual(inspect(replace(run, status="Cancelled")), "IncompleteRun")
        self.assertEqual(
            measure_slope_forward(run, scenario="ground_slope_forward_02",
                                  start_yaw=30).reason,
            "UnexpectedYaw",
        )
        changed = list(run.trace)
        changed[1] = replace(changed[1], yaw=30)
        self.assertEqual(inspect(replace(run, trace=tuple(changed))), "UnexpectedYaw")
        changed = list(run.trace)
        changed[-1] = replace(changed[-1], forward=0)
        self.assertEqual(inspect(replace(run, trace=tuple(changed))),
                         "UnexpectedMovementCommand")
        changed = list(run.trace)
        changed[0] = replace(changed[0], before=replace(changed[0].before, vx=20))
        self.assertEqual(inspect(replace(run, trace=tuple(changed),
                                         start=changed[0].before)), "InitialSpeedMismatch")

    def test_direct_cli(self):
        with patch("tools.aipb_training.ground_slope_forward_profile.load_bundle",
                   return_value=direct_trial()):
            with redirect_stdout(io.StringIO()) as output:
                status = main(["unused", "--scenario", "ground_slope_forward_02"])
        self.assertEqual(status, 0)
        self.assertIn("Measured: HeightGainObserved", output.getvalue())
        self.assertIn("start_speed=0", output.getvalue())

        with patch("tools.aipb_training.ground_slope_forward_profile.load_bundle",
                   return_value=direct_trial()):
            with redirect_stdout(io.StringIO()) as rejected:
                status = main(["unused", "--scenario", "ground_slope_forward_02",
                               "--heading-deg", "30"])
        self.assertEqual(status, 3)
        self.assertIn("Inconclusive: UnexpectedYaw", rejected.getvalue())

    def test_cli_and_invalid_bundle(self):
        with patch("tools.aipb_training.ground_slope_forward_profile.load_bundle",
                   return_value=trial()):
            with redirect_stdout(io.StringIO()) as output:
                self.assertEqual(main(["unused"]), 0)
        self.assertIn("Measured: HeightGainObserved", output.getvalue())
        self.assertIn("max_height_gain=", output.getvalue())
        with patch("tools.aipb_training.ground_slope_forward_profile.load_bundle",
                   side_effect=InvalidBundle("corrupt")):
            with redirect_stderr(io.StringIO()) as output:
                self.assertEqual(main(["unused"]), 2)
        self.assertIn("InvalidEvidence: corrupt", output.getvalue())


if __name__ == "__main__":
    unittest.main()
