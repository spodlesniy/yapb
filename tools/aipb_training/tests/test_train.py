#!/usr/bin/env python3
"""Unit tests for the AiPB training command entry point."""

from __future__ import annotations

import unittest
from unittest.mock import patch

from ..train import build_parser, main


class TrainCommandTests(unittest.TestCase):
    def test_parser_accepts_quality_thresholds(self) -> None:
        args = build_parser().parse_args([
            "dataset.jsonl",
            "--min-samples", "10000",
            "--min-episodes", "100",
            "--max-dominant-action-share", "0.8",
            "--min-action-samples", "1:50",
            "--min-action-samples", "8:25",
            "--min-action-episodes", "1:10",
            "--min-action-episodes", "8:5",
        ])

        self.assertEqual(args.min_samples, 10000)
        self.assertEqual(args.min_episodes, 100)
        self.assertEqual(args.max_dominant_action_share, 0.8)
        self.assertEqual(args.min_action_samples, ["1:50", "8:25"])
        self.assertEqual(args.min_action_episodes, ["1:10", "8:5"])

    @patch("tools.aipb_training.train.run_training")
    @patch("tools.aipb_training.train.load_training_dataset")
    @patch("tools.aipb_training.train.summarize_dataset")
    @patch("tools.aipb_training.train.check_quality")
    def test_quality_failure_prevents_training(
        self,
        check_quality,
        summarize_dataset,
        load_training_dataset,
        run_training,
    ) -> None:
        quality = type("Quality", (), {"failures": ("samples below minimum",), "is_valid": False})()
        check_quality.return_value = quality
        load_training_dataset.return_value = (None, ())
        summarize_dataset.return_value = None

        self.assertEqual(main(["dataset.jsonl", "--min-samples", "100", "--min-action-samples", "1:50"]), 1)
        run_training.assert_not_called()
        check_quality.assert_called_once()

    @patch("tools.aipb_training.train.run_training")
    @patch("tools.aipb_training.train.load_training_dataset")
    @patch("tools.aipb_training.train.summarize_dataset")
    @patch("tools.aipb_training.train.check_quality")
    def test_quality_pass_allows_training(
        self,
        check_quality,
        summarize_dataset,
        load_training_dataset,
        run_training,
    ) -> None:
        quality = type("Quality", (), {"failures": (), "is_valid": True})()
        check_quality.return_value = quality
        load_training_dataset.return_value = (None, ())
        summarize_dataset.return_value = None
        run_training.side_effect = RuntimeError("training reached")

        with self.assertRaisesRegex(RuntimeError, "training reached"):
            main(["dataset.jsonl", "--min-action-samples", "1:0", "--min-action-episodes", "1:0"])

        run_training.assert_called_once()


if __name__ == "__main__":
    unittest.main()
