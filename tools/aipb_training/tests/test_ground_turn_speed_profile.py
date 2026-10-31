"""D294 combined turn and entry-speed profile regressions."""

import io
import math
import unittest
from contextlib import redirect_stdout, redirect_stderr
from dataclasses import replace
from unittest.mock import patch

from tools.aipb_training.ground_turn_speed_profile import main, measure_turn_speed
from tools.aipb_training.scenario_trace_compare import Bundle, Frame, Pose, InvalidBundle


def pose(x, y, vx, vy, yaw):
    return Pose(x, y, -92, vx, vy, 0, yaw, 0, 1, 0)


def trial():
    # Five command phases, 10ms each. A changed yaw is already visible in before.
    start = pose(0, 0, 0, 0, -105)
    hold = pose(0, 0, 0, 0, -105)
    slow_in = replace(hold, yaw=-75)
    slow_out = pose(3, 1, 60, 10, -75)
    straight_out = pose(12, 4, 210, 90, -75)
    fast_out = pose(15, 6, 190, 45, -105)
    coast_out = pose(16, 6, 0, 0, -105)
    frames = (
        Frame(0, 0, 10, 0, 0, 250, 0, 0, -105, 0, start, hold),
        Frame(1, 1, 10, 250, 0, 250, 0, 0, -75, 0, slow_in, slow_out),
        Frame(2, 2, 10, 250, 0, 250, 0, 0, -75, 0, slow_out, straight_out),
        Frame(3, 3, 10, 250, 0, 250, 0, 0, -105, 0,
              replace(straight_out, yaw=-105), fast_out),
        Frame(4, 4, 10, 0, 0, 250, 0, 0, -105, 0, fast_out, coast_out),
    )
    return Bundle("ground_turn_speed_01", "de_dust2", 1, 2, 1,
                  "Completed", "finished", 5, 50, "live", start, coast_out, frames)


def inspect(run):
    return measure_turn_speed(run, durations=(10, 10, 10, 10, 10))


def full_duration_trial():
    # Expand each compact phase into 10ms samples matching the real scenario.
    base = trial()
    phase_ms = (50, 200, 300, 200, 500)
    yaw_deltas = (0, 30, 0, -30, 0)
    frames = []
    entry_yaw = -105.0
    for template, duration, delta in zip(base.trace, phase_ms, yaw_deltas):
        count = duration // 10

        def interpolate(fraction, yaw):
            before, after = template.before, template.after
            return replace(
                before,
                x=before.x + (after.x - before.x) * fraction,
                y=before.y + (after.y - before.y) * fraction,
                vx=before.vx + (after.vx - before.vx) * fraction,
                vy=before.vy + (after.vy - before.vy) * fraction,
                yaw=yaw,
            )

        for step in range(count):
            # The engine applies the turn angle before the pre-frame pose.
            yaw = entry_yaw + delta * (step + 1) / count
            frames.append(replace(
                template, index=len(frames), msec=10, yaw=yaw,
                before=interpolate(step / count, yaw),
                after=interpolate((step + 1) / count, yaw),
            ))
        entry_yaw += delta

    return replace(base, frames=len(frames), command_ms=sum(phase_ms),
                   trace=tuple(frames))


class TurnSpeedTests(unittest.TestCase):
    def test_two_turns_have_distinct_entry_speeds(self):
        result = inspect(trial())
        self.assertEqual((result.outcome, result.reason), ("Measured", "TwoTurnsObserved"))
        self.assertAlmostEqual(result.slow_turn.entry_speed, 0)
        self.assertAlmostEqual(result.fast_turn.entry_speed, math.hypot(210, 90))
        self.assertAlmostEqual(result.slow_turn.displacement, math.hypot(3, 1))
        self.assertAlmostEqual(result.fast_turn.displacement, math.hypot(3, 2))
        self.assertAlmostEqual(result.slow_turn.exit_yaw, -75)
        self.assertAlmostEqual(result.fast_turn.exit_yaw, -105)
        self.assertAlmostEqual(result.final_speed, 0)

    def test_insufficient_speed_contrast(self):
        run = trial()
        frames = list(run.trace)
        slow = frames[2].after
        stationary = replace(slow, vx=1, vy=0)
        frames[2] = replace(frames[2], after=stationary)
        frames[3] = replace(frames[3], before=replace(stationary, yaw=-105))
        case = replace(run, trace=tuple(frames))
        self.assertEqual(inspect(case).reason, "SpeedContrastNotObserved")

    def test_rejects_untrusted_command_and_physics(self):
        run = trial()
        cases = [
            (replace(run, status="Cancelled"), "IncompleteRun"),
            (replace(run, command_ms=49), "DurationMismatch"),
            (replace(run, frames=4), "IncompleteTrace"),
            (replace(run, start=pose(4, 0, 0, 0, -105)), "StartObservationMismatch"),
            (replace(run, end=pose(50, 0, 0, 0, -105)), "EndObservationMismatch"),
        ]
        def altered(index, **kwargs):
            frames = list(run.trace)
            frames[index] = replace(frames[index], **kwargs)
            return replace(run, trace=tuple(frames))
        cases.extend([
            (altered(1, yaw=-70), "UnexpectedYaw"),
            (altered(3, forward=0), "UnexpectedMovementCommand"),
            (altered(2, max_speed=200), "SpeedCapChanged"),
            (altered(3, buttons=2), "GroundContactOrStance"),
            (altered(3, msec=25), "PhaseTimingMismatch"),
            (altered(4, action=2), "UnexpectedAction"),
        ])
        for case, expected in cases:
            with self.subTest(expected=expected):
                self.assertEqual(inspect(case).reason, expected)

    def test_cli_and_invalid_bundle(self):
        complete = full_duration_trial()
        self.assertEqual(complete.frames, 125)
        self.assertEqual(complete.command_ms, 1250)
        self.assertEqual(
            [sum(frame.msec for frame in complete.trace if frame.action == index)
             for index in range(5)],
            [50, 200, 300, 200, 500],
        )
        self.assertEqual(measure_turn_speed(complete).reason, "TwoTurnsObserved")

        with patch("tools.aipb_training.ground_turn_speed_profile.load_bundle",
                   return_value=complete):
            with redirect_stdout(io.StringIO()) as output:
                self.assertEqual(main(["unused", "--heading-deg", "-105"]), 0)
        self.assertIn("Measured: TwoTurnsObserved", output.getvalue())
        self.assertIn("speed_difference=", output.getvalue())
        with patch("tools.aipb_training.ground_turn_speed_profile.load_bundle",
                   side_effect=InvalidBundle("broken")):
            with redirect_stderr(io.StringIO()) as output:
                self.assertEqual(main(["unused"]), 2)
        self.assertIn("InvalidEvidence: broken", output.getvalue())


if __name__ == "__main__":
    unittest.main()
