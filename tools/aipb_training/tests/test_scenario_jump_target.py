"""D309 conservative target-footprint outcomes for Stage C jumps."""

import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

from tools.aipb_training.scenario_jump_target import (
    InvalidTarget, JumpTarget, classify_target, main, read_target,
    TARGET_VERSION,
)
from tools.aipb_training.tests.test_scenario_jump_profile import run_fixture


def target(x0=118, x1=180, y0=180, y1=220, z=36, heading=0, inset=0):
    return JumpTarget("jump_runup_short_01", "de_dust2", x0, x1, y0, y1,
                      z, 3, heading, inset)


class TargetOutcomeTests(unittest.TestCase):
    def setUp(self):
        self.run = run_fixture("jump_runup_short_01")

    def test_target_reached_stable_supported_landing(self):
        result = classify_target(self.run, target())
        self.assertEqual((result.outcome, result.reason),
                         ("TargetReached", "StableOnDeclaredTarget"))
        self.assertGreaterEqual(result.stable_ms, 40)

    def test_overshot_after_landing(self):
        result = classify_target(self.run, target(108, 130))
        self.assertEqual((result.outcome, result.reason),
                         ("Overshot", "BeyondFarEdge"))

    def test_missed_landing_stable_elsewhere(self):
        self.assertEqual(classify_target(self.run, target(170, 210)).outcome,
                         "MissedLanding")
        self.assertEqual(classify_target(self.run, target(z=60)).outcome,
                         "MissedLanding")

    def test_missing_takeoff_is_missed_not_success(self):
        outcome = classify_target(run_fixture("jump_runup_short_01", "no_takeoff"),
                                  target())
        self.assertEqual((outcome.outcome, outcome.reason),
                         ("MissedLanding", "NoTakeoff"))

    def test_inconclusive_context_ambiguity_and_incomplete(self):
        self.assertEqual(classify_target(self.run, replace(target(), map="de_inferno")).reason,
                         "TargetContextMismatch")
        self.assertEqual(classify_target(self.run, target(145, 200)).reason,
                         "TargetBoundaryAmbiguity")
        self.assertEqual(classify_target(self.run, target(100, 180)).reason,
                         "UnverifiedApproach")
        bad = replace(self.run, status="ActionTimeout")
        self.assertEqual(classify_target(bad, target()).outcome, "Inconclusive")
        no_landing = run_fixture("jump_runup_short_01", "never_landed")
        self.assertEqual(classify_target(no_landing, target()).outcome, "Inconclusive")

    def test_crouch_terminal_not_assumed_to_be_standing_support(self):
        altered = replace(self.run, end=replace(self.run.end, duck=1))
        self.assertEqual(classify_target(altered, target()).outcome, "Inconclusive")


class TargetParserTests(unittest.TestCase):
    def test_read_measured_target(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "target.csv"
            path.write_text(
                f"{TARGET_VERSION},jump_runup_short_01,de_dust2,118,180,180,220,36,3,0,0\n",
                encoding="utf-8")
            self.assertEqual(read_target(path), target())
            for row in (
                "",
                f"{TARGET_VERSION},jump_runup_short_01,de_dust2,118,180,180,220,36,3,0\n",
                f"{TARGET_VERSION},../evil,de_dust2,118,180,180,220,36,3,0,0\n",
                f"{TARGET_VERSION},jump_runup_short_01,de_dust2,118,180,180,220,nan,3,0,0\n",
                f"{TARGET_VERSION},jump_runup_short_01,de_dust2,118,180,180,220,36,3,181,0\n",
                f"{TARGET_VERSION},jump_runup_short_01,de_dust2,118,119,180,220,36,3,0,0\n",
                f"{TARGET_VERSION},jump_runup_short_01,de_dust2,118,180,180,220,36,3,0,18\n",
            ):
                path.write_text(row, encoding="utf-8")
                with self.subTest(row=row), self.assertRaises(InvalidTarget):
                    read_target(path)

    def test_cli_outcomes(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "target.csv"
            path.write_text(
                f"{TARGET_VERSION},jump_runup_short_01,de_dust2,118,180,180,220,36,3,0,0\n",
                encoding="utf-8")
            with patch("tools.aipb_training.scenario_jump_target.load_bundle",
                       return_value=run_fixture("jump_runup_short_01")):
                self.assertEqual(main([folder, str(path)]), 0)


if __name__ == "__main__":
    unittest.main()
