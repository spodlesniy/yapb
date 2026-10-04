#!/usr/bin/env python3
"""Unit tests for AiPB dataset quality checks."""

from __future__ import annotations

import unittest

from ..dataset_quality import build_parser, check_quality
from ..dataset_stats import DatasetStats


def make_stats(samples: int, episodes: int, counts: tuple[int, ...]) -> DatasetStats:
    return DatasetStats(samples, episodes, 0, counts)


class DatasetQualityTests(unittest.TestCase):
    def test_quality_passes_without_thresholds(self) -> None:
        self.assertTrue(check_quality(make_stats(100, 10, (80, 20))).is_valid)

    def test_minimums_are_enforced(self) -> None:
        result = check_quality(make_stats(10, 2, (10, 0)), min_samples=20, min_episodes=5)
        self.assertEqual(result.failures, ("samples 10 < minimum 20", "episodes 2 < minimum 5"))

    def test_dominant_action_share_is_enforced(self) -> None:
        result = check_quality(make_stats(100, 10, (91, 9)), max_dominant_action_share=0.9)
        self.assertEqual(result.failures, ("dominant action share 0.9100 > maximum 0.9000",))

    def test_equal_share_is_accepted(self) -> None:
        self.assertTrue(check_quality(make_stats(100, 10, (90, 10)), max_dominant_action_share=0.9).is_valid)

    def test_invalid_thresholds_are_rejected(self) -> None:
        with self.assertRaises(ValueError): check_quality(make_stats(1, 1, (1,)), min_samples=-1)
        with self.assertRaises(ValueError): check_quality(make_stats(1, 1, (1,)), min_episodes=-1)
        with self.assertRaises(ValueError): check_quality(make_stats(1, 1, (1,)), max_dominant_action_share=0.0)
        with self.assertRaises(ValueError): check_quality(make_stats(1, 1, (1,)), max_dominant_action_share=1.1)

    def test_parser_accepts_quality_thresholds(self) -> None:
        args = build_parser().parse_args(["dataset.jsonl", "--min-samples", "1000", "--min-episodes", "20", "--max-dominant-action-share", "0.8"])
        self.assertEqual((args.min_samples, args.min_episodes, args.max_dominant_action_share), (1000, 20, 0.8))


if __name__ == "__main__":
    unittest.main()
