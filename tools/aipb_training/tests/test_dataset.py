#!/usr/bin/env python3
"""Unit tests for the AiPB training dataset loader and batching."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from tools.aipb_training.dataset import (
    TrainingAction,
    TrainingBatch,
    TrainingObservation,
    TrainingSample,
    iter_training_batches,
    iter_training_samples,
    load_training_dataset,
)
from tools.aipb_training.model_contract import (
    MODEL_ACTION_SCHEMA_VERSION,
    MODEL_FEATURE_COUNT,
    MODEL_FEATURE_SCHEMA_VERSION,
)


METADATA = {
    "format": "aipb-training-jsonl",
    "version": 3,
    "feature_schema_version": MODEL_FEATURE_SCHEMA_VERSION,
    "action_schema_version": MODEL_ACTION_SCHEMA_VERSION,
    "type": "metadata",
}


def make_sample(episode_id: int) -> dict:
    return {
        "episode_id": episode_id,
        "observation": {"schema_version": MODEL_FEATURE_SCHEMA_VERSION, "values": [0.1] * MODEL_FEATURE_COUNT},
        "action": {
            "schema_version": MODEL_ACTION_SCHEMA_VERSION,
            "action_id": 1,
            "target_node": 42,
            "target_player": -1,
            "target_position": [0.0, 0.0, 0.0],
            "weapon_type": 0,
            "grenade_type": 0,
            "duration": 0.0,
            "confidence": 0.8,
        },
        "reward": 1.0,
        "next_observation": {"schema_version": MODEL_FEATURE_SCHEMA_VERSION, "values": [0.2] * MODEL_FEATURE_COUNT},
        "result": 1,
        "elapsed_time": 0.5,
        "terminal": True,
    }


def make_training_sample(episode_id: int) -> TrainingSample:
    values = (0.0,) * MODEL_FEATURE_COUNT
    return TrainingSample(
        episode_id=episode_id,
        observation=TrainingObservation(values),
        action=TrainingAction(
            action_id=1,
            target_node=-1,
            target_player=-1,
            target_position=(0.0, 0.0, 0.0),
            weapon_type=0,
            grenade_type=0,
            duration=0.0,
            confidence=1.0,
        ),
        reward=0.0,
        next_observation=TrainingObservation(values),
        result=1,
        elapsed_time=0.0,
        terminal=True,
    )


class TrainingDatasetLoaderTests(unittest.TestCase):
    def write_dataset(self, lines: list[dict]) -> Path:
        handle = tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            suffix=".jsonl",
            delete=False,
        )
        with handle:
            for line in lines:
                handle.write(json.dumps(line) + "\n")
        self.addCleanup(Path(handle.name).unlink, missing_ok=True)
        return Path(handle.name)

    def test_iter_training_samples_preserves_file_order_and_types(self) -> None:
        path = self.write_dataset([METADATA, make_sample(7), make_sample(8)])

        samples = list(iter_training_samples(path))

        self.assertEqual([sample.episode_id for sample in samples], [7, 8])
        self.assertEqual(len(samples[0].observation.values), MODEL_FEATURE_COUNT)
        self.assertEqual(samples[0].observation.values[:3], (0.1, 0.1, 0.1))
        self.assertIsInstance(samples[0].observation, TrainingObservation)
        self.assertIsInstance(samples[0].action, TrainingAction)
        self.assertEqual(samples[0].action.target_position, (0.0, 0.0, 0.0))
        self.assertEqual(samples[0].next_observation.values[:3], (0.2, 0.2, 0.2))
        self.assertIsInstance(samples[0].terminal, bool)

    def test_load_training_dataset_returns_metadata_and_immutable_samples(self) -> None:
        path = self.write_dataset([METADATA, make_sample(3)])

        metadata, samples = load_training_dataset(path)

        self.assertEqual(metadata.format, "aipb-training-jsonl")
        self.assertEqual(metadata.version, 2)
        self.assertEqual(metadata.feature_schema_version, MODEL_FEATURE_SCHEMA_VERSION)
        self.assertEqual(metadata.action_schema_version, MODEL_ACTION_SCHEMA_VERSION)
        self.assertEqual(len(samples), 1)
        self.assertIsInstance(samples, tuple)

    def test_invalid_dataset_is_rejected_before_sample_conversion(self) -> None:
        sample = make_sample(1)
        sample["action"]["confidence"] = 2.0
        path = self.write_dataset([METADATA, sample])

        with self.assertRaises(ValueError):
            list(iter_training_samples(path))

    def test_batches_preserve_order_and_have_bounded_size(self) -> None:
        samples = iter([make_training_sample(i) for i in range(1, 6)])

        batches = list(iter_training_batches(samples, 2))

        self.assertEqual([batch.size for batch in batches], [2, 2, 1])
        self.assertTrue(all(isinstance(batch, TrainingBatch) for batch in batches))
        self.assertEqual(
            [sample.episode_id for batch in batches for sample in batch.samples],
            [1, 2, 3, 4, 5],
        )

    def test_batches_require_positive_batch_size(self) -> None:
        with self.assertRaises(ValueError):
            list(iter_training_batches(iter([]), 0))


if __name__ == "__main__":
    unittest.main()
