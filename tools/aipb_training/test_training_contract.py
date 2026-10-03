#!/usr/bin/env python3
"""Unit tests for the AiPB framework-neutral training contract."""

from __future__ import annotations

import unittest

from .dataset import TrainingAction, TrainingBatch, TrainingObservation, TrainingSample
from .model_contract import MODEL_FEATURE_COUNT
from .training_contract import ACTION_TENSOR_SIZE, encode_action_target, encode_policy_batch


def make_sample(
    episode_id: int,
    feature_values: tuple[float, ...] | None = None,
    action_id: int = 3,
) -> TrainingSample:
    values = feature_values if feature_values is not None else (0.1,) * MODEL_FEATURE_COUNT
    return TrainingSample(
        episode_id=episode_id,
        observation=TrainingObservation(values),
        action=TrainingAction(
            action_id=action_id,
            target_node=42,
            target_player=-7,
            target_position=(1.0, 2.0, 3.0),
            weapon_type=4,
            grenade_type=5,
            duration=0.75,
            confidence=0.8,
        ),
        reward=1.0,
        next_observation=TrainingObservation(values),
        result=1,
        elapsed_time=0.25,
        terminal=False,
    )


class TrainingContractTests(unittest.TestCase):
    def test_action_target_matches_inference_tensor_order(self) -> None:
        target = encode_action_target(make_sample(1, action_id=6))

        self.assertEqual(len(target), ACTION_TENSOR_SIZE)
        self.assertEqual(
            target,
            (6.0, 42.0, -7.0, 1.0, 2.0, 3.0, 4.0, 5.0, 0.75, 0.8),
        )

    def test_policy_batch_preserves_observations_and_targets(self) -> None:
        batch = TrainingBatch((make_sample(1), make_sample(2, action_id=9)))

        encoded = encode_policy_batch(batch)

        self.assertEqual(encoded.size, 2)
        self.assertEqual(encoded.feature_count, MODEL_FEATURE_COUNT)
        self.assertEqual(len(encoded.observations[0]), MODEL_FEATURE_COUNT)
        self.assertEqual(encoded.action_targets[0][0], 3.0)
        self.assertEqual(encoded.action_targets[1][0], 9.0)

    def test_policy_batch_rejects_mismatched_feature_counts(self) -> None:
        batch = TrainingBatch(
            (
                make_sample(1),
                make_sample(2, feature_values=(0.1,) * (MODEL_FEATURE_COUNT - 1)),
            )
        )

        with self.assertRaises(ValueError):
            encode_policy_batch(batch)

    def test_empty_policy_batch_is_supported(self) -> None:
        encoded = encode_policy_batch(TrainingBatch(()))

        self.assertEqual(encoded.size, 0)
        self.assertEqual(encoded.feature_count, 0)
        self.assertEqual(encoded.observations, ())
        self.assertEqual(encoded.action_targets, ())


if __name__ == "__main__":
    unittest.main()
