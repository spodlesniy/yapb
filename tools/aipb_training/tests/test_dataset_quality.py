#!/usr/bin/env python3
"""Unit tests for AiPB dataset quality checks."""

from __future__ import annotations

import unittest

from ..dataset_quality import build_parser, check_quality, parse_action_requirement
from ..dataset_stats import DatasetStats


def make_stats(samples: int, episodes: int, counts: tuple[int, ...], action_episodes: tuple[int, ...] | None = None) -> DatasetStats:
    return DatasetStats(samples, episodes, 0, counts, action_episodes or counts)


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

    def test_action_coverage_minimum_is_enforced(self) -> None:
        result = check_quality(make_stats(100, 10, (70, 30, 0)), min_action_coverage=3)
        self.assertEqual(result.failures, ("action coverage 2 < minimum 3",))

    def test_action_coverage_boundary_is_accepted(self) -> None:
        self.assertTrue(check_quality(make_stats(100, 10, (70, 30, 0)), min_action_coverage=2).is_valid)

    def test_action_coverage_must_fit_action_taxonomy(self) -> None:
        with self.assertRaises(ValueError):
            check_quality(make_stats(1, 1, (1,)), min_action_coverage=27)

    def test_action_minimum_is_enforced(self) -> None:
        result = check_quality(make_stats(100, 10, (80, 20)), min_action_samples=((1, 25),))
        self.assertEqual(result.failures, ("action 1.MoveToNode samples 20 < minimum 25",))

    def test_action_minimum_equal_share_is_accepted(self) -> None:
        self.assertTrue(check_quality(make_stats(100, 10, (80, 20)), min_action_samples=((1, 20),)).is_valid)

    def test_action_episode_minimum_is_enforced(self) -> None:
        result = check_quality(make_stats(100, 10, (80, 20), (8, 2)), min_action_episodes=((1, 3),))
        self.assertEqual(result.failures, ("action 1.MoveToNode episodes 2 < minimum 3",))

    def test_action_episode_requirement_is_accepted_at_boundary(self) -> None:
        self.assertTrue(check_quality(make_stats(100, 10, (80, 20), (8, 2)), min_action_episodes=((1, 2),)).is_valid)

    def test_duplicate_action_episode_minimum_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            check_quality(make_stats(1, 1, (1,)), min_action_episodes=((0, 1), (0, 1)))

    def test_duplicate_action_minimum_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            check_quality(make_stats(1, 1, (1,)), min_action_samples=((0, 1), (0, 1)))

    def test_action_requirement_parser(self) -> None:
        self.assertEqual(parse_action_requirement("8:100"), (8, 100))
        with self.assertRaises(ValueError): parse_action_requirement("8")
        with self.assertRaises(ValueError): parse_action_requirement("99:1")
        with self.assertRaises(ValueError): parse_action_requirement("8:-1")

    def test_invalid_thresholds_are_rejected(self) -> None:
        with self.assertRaises(ValueError): check_quality(make_stats(1, 1, (1,)), min_samples=-1)
        with self.assertRaises(ValueError): check_quality(make_stats(1, 1, (1,)), min_episodes=-1)
        with self.assertRaises(ValueError): check_quality(make_stats(1, 1, (1,)), max_dominant_action_share=0.0)
        with self.assertRaises(ValueError): check_quality(make_stats(1, 1, (1,)), max_dominant_action_share=1.1)
        with self.assertRaises(ValueError): check_quality(make_stats(1, 1, (1,)), min_action_coverage=-1)

    def test_parser_accepts_quality_thresholds(self) -> None:
        args = build_parser().parse_args(["dataset.jsonl", "--min-samples", "1000", "--min-episodes", "20", "--min-action-coverage", "8", "--max-dominant-action-share", "0.8", "--min-action-samples", "1:50", "--min-action-samples", "8:25", "--min-action-episodes", "1:10", "--min-action-episodes", "8:5"])
        self.assertEqual((args.min_samples, args.min_episodes, args.min_action_coverage, args.max_dominant_action_share), (1000, 20, 8, 0.8))
        self.assertEqual(args.min_action_samples, ["1:50", "8:25"])
        self.assertEqual(args.min_action_episodes, ["1:10", "8:5"])


if __name__ == "__main__":
    unittest.main()
