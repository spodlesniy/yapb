#!/usr/bin/env python3
"""Unit tests for the AiPB model feature schema."""

from __future__ import annotations

import unittest

from ..feature_contract import (
    CORE_FEATURE_NAMES,
    MODEL_FEATURE_INDEX,
    MODEL_FEATURE_NAMES,
    PLAYER_FEATURE_NAMES,
    WAYPOINT_FEATURE_NAMES,
)
from ..model_contract import MODEL_FEATURE_COUNT


class FeatureContractTests(unittest.TestCase):
    def test_feature_schema_has_exact_runtime_width(self) -> None:
        self.assertEqual(len(MODEL_FEATURE_NAMES), MODEL_FEATURE_COUNT)

    def test_feature_names_are_unique(self) -> None:
        self.assertEqual(len(MODEL_FEATURE_INDEX), MODEL_FEATURE_COUNT)

    def test_feature_blocks_have_expected_sizes(self) -> None:
        self.assertEqual(len(CORE_FEATURE_NAMES), 80)
        self.assertEqual(len(PLAYER_FEATURE_NAMES), 11)
        self.assertEqual(len(WAYPOINT_FEATURE_NAMES), 8)

    def test_core_feature_order_matches_runtime_contract(self) -> None:
        self.assertEqual(MODEL_FEATURE_INDEX["round_time_remaining"], 0)
        self.assertEqual(MODEL_FEATURE_INDEX["task_time_remaining"], 1)
        self.assertEqual(MODEL_FEATURE_INDEX["weapon.unknown"], 31)
        self.assertEqual(MODEL_FEATURE_INDEX["reload.none"], 41)
        self.assertEqual(MODEL_FEATURE_INDEX["objective.bomb_planted"], 44)
        self.assertEqual(MODEL_FEATURE_INDEX["navigation.jump"], 51)
        self.assertEqual(MODEL_FEATURE_INDEX["perception.seeing_enemy"], 55)
        self.assertEqual(MODEL_FEATURE_INDEX["task.unknown"], 59)
        self.assertEqual(MODEL_FEATURE_INDEX["task.blind"], 78)
        self.assertEqual(MODEL_FEATURE_INDEX["task.spraypaint"], 79)

    def test_player_and_waypoint_blocks_are_contiguous(self) -> None:
        player_base = len(CORE_FEATURE_NAMES)
        waypoint_base = player_base + 8 * len(PLAYER_FEATURE_NAMES)

        self.assertEqual(MODEL_FEATURE_INDEX["player.0.valid"], player_base)
        self.assertEqual(player_base, 80)
        self.assertEqual(MODEL_FEATURE_INDEX["player.7.armor"], waypoint_base - 1)
        self.assertEqual(MODEL_FEATURE_INDEX["waypoint.0.present"], waypoint_base)
        self.assertEqual(MODEL_FEATURE_INDEX["waypoint.7.distance"], MODEL_FEATURE_COUNT - 1)


if __name__ == "__main__":
    unittest.main()
