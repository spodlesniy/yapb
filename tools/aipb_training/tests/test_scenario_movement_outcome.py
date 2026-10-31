"""Tests for a declared, bounded GoldSrc ground-movement outcome."""

from __future__ import annotations

if __name__ == "__main__" and not __package__:
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
    __package__ = "tools.aipb_training.tests"

import csv
import io
import tempfile
import unittest
from contextlib import redirect_stdout
from dataclasses import replace
from pathlib import Path

from tools.aipb_training.scenario_movement_outcome import (
    GOAL_VERSION,
    GroundGoal,
    InvalidGoal,
    assess_ground_movement,
    main,
    read_goal,
)
from tools.aipb_training.scenario_trace_compare import (
    RESULT_VERSION,
    TRACE_COLUMNS,
    TRACE_VERSION,
    Bundle,
    Frame,
    Pose,
)


def observation(x: float = 0.0, *, ground: int = 1, duck: int = 0) -> Pose:
    return Pose(x, 0.0, -92.0, 0.0, 0.0, 0.0, 0.0, 0.0, ground, duck)


def trial(travel: float = 20.0) -> Bundle:
    before = observation()
    after = observation(travel)
    frame = Frame(0, 0, 10, 250.0, 0.0, 250.0, 0, 0, 0.0, 0.0,
                  before, after)
    return Bundle("ground_forward_02", "de_dust2", 1, 2, 10,
                  "Completed", "finished", 1, 10, "live",
                  before, after, (frame,))


def goal() -> GroundGoal:
    return GroundGoal("ground_forward_02", "de_dust2", 10, 0.0, 16.0)


def pose_values(pose: Pose) -> list[str]:
    return [str(getattr(pose, field)) for field in
            ("x", "y", "z", "vx", "vy", "vz", "yaw", "pitch", "ground", "duck")]


class GroundOutcomeTests(unittest.TestCase):
    def test_target_reached_and_below_target(self) -> None:
        success = assess_ground_movement(trial(), goal())
        self.assertEqual((success.outcome, success.reason, success.projected_travel),
                         ("Success", "TargetReached", 20.0))
        failure = assess_ground_movement(trial(10.0), goal())
        self.assertEqual((failure.outcome, failure.reason, failure.projected_travel),
                         ("Failure", "BelowTarget", 10.0))

    def test_uncertain_near_threshold(self) -> None:
        for travel in (15.9, 16.0, 16.5, 16.9):
            with self.subTest(travel=travel):
                outcome = assess_ground_movement(trial(travel), goal())
                self.assertEqual((outcome.outcome, outcome.reason),
                                 ("Inconclusive", "ThresholdAmbiguity"))

    def test_heading_projection_not_absolute_distance(self) -> None:
        opposite = replace(goal(), heading=180.0 - 0.01)
        outcome = assess_ground_movement(trial(30), opposite)
        self.assertEqual((outcome.outcome, outcome.reason),
                         ("Failure", "BelowTarget"))

    def test_interrupted_and_untrusted_end(self) -> None:
        for changed in (
            replace(trial(), status="Cancelled"),
            replace(trial(), source="last_committed"),
            replace(trial(), source="missing"),
        ):
            with self.subTest(run=changed):
                self.assertEqual(assess_ground_movement(changed, goal()).reason,
                                 "IncompleteRun")

    def test_wrong_scenario_or_map(self) -> None:
        self.assertEqual(assess_ground_movement(trial(), replace(goal(), map="de_inferno")).reason,
                         "GoalContextMismatch")

    def test_incomplete_command_duration_or_frame_history(self) -> None:
        self.assertEqual(assess_ground_movement(trial(), replace(goal(), command_ms=11)).reason,
                         "CommandDurationMismatch")
        self.assertEqual(assess_ground_movement(replace(trial(), trace=()), goal()).reason,
                         "IncompleteTrace")
        self.assertEqual(assess_ground_movement(replace(trial(), frames=0), goal()).reason,
                         "IncompleteTrace")

    def test_start_pose_mismatch(self) -> None:
        run = replace(trial(), start=observation(8))
        self.assertEqual(assess_ground_movement(run, goal()).reason,
                         "StartObservationMismatch")

    def test_end_pose_mismatch(self) -> None:
        run = replace(trial(), end=observation(24))
        self.assertEqual(assess_ground_movement(run, goal()).reason,
                         "EndObservationMismatch")

    def test_disconnected_frame_history(self) -> None:
        run = trial()
        next_frame = replace(run.trace[0], index=1, before=observation(30),
                             after=observation(40))
        broken = replace(run, command_ms=20, frames=2,
                         end=observation(40), trace=run.trace + (next_frame,))
        self.assertEqual(assess_ground_movement(broken, replace(goal(), command_ms=20)).reason,
                         "ObservationDiscontinuity")

    def test_ground_unduck_and_jump_guards(self) -> None:
        run = trial()
        samples = (
            replace(run.trace[0], after=observation(20, ground=0)),
            replace(run.trace[0], after=observation(20, duck=1)),
            replace(run.trace[0], buttons=2),
            replace(run.trace[0], buttons=4),
        )
        for frame in samples:
            with self.subTest(frame=frame):
                bad = replace(run, trace=(frame,), end=frame.after)
                self.assertEqual(assess_ground_movement(bad, goal()).reason,
                                 "GroundContactOrStance")

    def test_invalid_speed_guard(self) -> None:
        run = trial()
        bad = replace(run, trace=(replace(run.trace[0], max_speed=0.0),))
        self.assertEqual(assess_ground_movement(bad, goal()).reason,
                         "UnverifiedMovementSpeed")

    def test_zero_distance_not_success(self) -> None:
        outcome = assess_ground_movement(trial(0.0), goal())
        self.assertEqual(outcome.outcome, "Failure")


class GoalParserTests(unittest.TestCase):
    def test_goal_round_trip(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "case.goal"
            path.write_text(f"{GOAL_VERSION},ground_forward_02,de_dust2,750,-105,16\n",
                            encoding="utf-8")
            self.assertEqual(
                read_goal(path),
                GroundGoal("ground_forward_02", "de_dust2", 750, -105.0, 16.0),
            )

    def test_bad_goal_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "bad.goal"
            for content in (
                "",
                f"{GOAL_VERSION},ground_forward_02,de_dust2,750,-105\n",
                f"{GOAL_VERSION},ground_forward_02,de_dust2,750,-105,16\nextra\n",
                f"{GOAL_VERSION},../unsafe,de_dust2,750,-105,16\n",
                f"{GOAL_VERSION},ground_forward_02,de_dust2,0,-105,16\n",
                f"{GOAL_VERSION},ground_forward_02,de_dust2,750,180,16\n",
                f"{GOAL_VERSION},ground_forward_02,de_dust2,750,nan,16\n",
                f"{GOAL_VERSION},ground_forward_02,de_dust2,750,-105,-1\n",
                f"{GOAL_VERSION},ground_forward_02,de_dust2,750,-105,inf\n",
                "unknown,ground_forward_02,de_dust2,750,-105,16\n",
            ):
                path.write_text(content, encoding="utf-8")
                with self.subTest(content=content), self.assertRaises(InvalidGoal):
                    read_goal(path)


class EndToEndTests(unittest.TestCase):
    def test_cli_with_actual_bundle(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run_dir = root / "attempt"
            run_dir.mkdir()
            run = trial()
            frame = run.trace[0]
            with (run_dir / "trace.csv").open("w", encoding="utf-8", newline="") as handle:
                writer = csv.writer(handle)
                writer.writerow([TRACE_VERSION])
                writer.writerow(TRACE_COLUMNS)
                writer.writerow([
                    frame.index, frame.action, frame.msec, frame.forward,
                    frame.strafe, frame.max_speed, frame.buttons,
                    frame.old_buttons, frame.yaw, frame.pitch,
                    *pose_values(frame.before), *pose_values(frame.after),
                ])
            with (run_dir / "result.csv").open("w", encoding="utf-8", newline="") as handle:
                csv.writer(handle).writerow([
                    RESULT_VERSION, run.name, run.map, run.bot, run.team,
                    run.round_id, run.status, run.reason, run.frames,
                    run.command_ms, 1000, 1010, run.source,
                    *pose_values(run.start), *pose_values(run.end),
                ])
            goal_file = root / "goal.csv"
            goal_file.write_text(f"{GOAL_VERSION},{run.name},{run.map},10,0,16\n",
                                 encoding="utf-8")
            with redirect_stdout(io.StringIO()) as output:
                code = main([str(run_dir), str(goal_file)])
            self.assertEqual(code, 0)
            self.assertIn("Success: TargetReached", output.getvalue())
            self.assertIn("projected_travel=20", output.getvalue())

            goal_file.write_text(f"{GOAL_VERSION},{run.name},{run.map},10,0,25\n",
                                 encoding="utf-8")
            with redirect_stdout(io.StringIO()) as output:
                code = main([str(run_dir), str(goal_file)])
            self.assertEqual(code, 1)
            self.assertIn("Failure: BelowTarget", output.getvalue())

            goal_file.write_text(f"{GOAL_VERSION},{run.name},{run.map},11,0,16\n",
                                 encoding="utf-8")
            with redirect_stdout(io.StringIO()) as output:
                code = main([str(run_dir), str(goal_file)])
            self.assertEqual(code, 3)
            self.assertIn("Inconclusive: CommandDurationMismatch", output.getvalue())


if __name__ == "__main__":
    unittest.main()
