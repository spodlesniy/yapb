#!/usr/bin/env python3
"""Unit tests for the AiPB training dataset validator."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from .validate_dataset import DatasetValidationError, validate_dataset


METADATA = {
    "format": "aipb-training-jsonl",
    "version": 1,
    "feature_schema_version": 1,
    "action_schema_version": 1,
    "type": "metadata",
}


def make_sample(feature_count: int = 2) -> dict:
    return {
        "episode_id": 1,
        "observation": {"schema_version": 1, "values": [0.1] * feature_count},
        "action": {
            "schema_version": 1,
            "action_id": 1,
            "target_node": 42,
            "target_player": -1,
            "target_position": [1.0, 2.0, 3.0],
            "weapon_type": 0,
            "grenade_type": 0,
            "duration": 0.5,
            "confidence": 0.8,
        },
        "reward": 1.0,
        "next_observation": {"schema_version": 1, "values": [0.2] * feature_count},
        "result": 1,
        "elapsed_time": 0.5,
        "terminal": True,
    }


class TrainingDatasetValidatorTests(unittest.TestCase):
    def write_dataset(self, lines: list[dict]) -> Path:
        handle = tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", suffix=".jsonl", delete=False)
        with handle:
            for line in lines:
                handle.write(json.dumps(line) + "\n")
        self.addCleanup(Path(handle.name).unlink, missing_ok=True)
        return Path(handle.name)

    def test_valid_metadata_only_dataset(self) -> None:
        path = self.write_dataset([METADATA])
        self.assertEqual(validate_dataset(path), 0)

    def test_valid_dataset_counts_samples(self) -> None:
        path = self.write_dataset([METADATA, make_sample()])
        self.assertEqual(validate_dataset(path), 1)

    def test_rejects_invalid_metadata(self) -> None:
        metadata = dict(METADATA)
        metadata["version"] = 2
        path = self.write_dataset([metadata])

        with self.assertRaises(DatasetValidationError):
            validate_dataset(path)

    def test_rejects_mismatched_feature_count(self) -> None:
        path = self.write_dataset([METADATA, make_sample(2), make_sample(3)])

        with self.assertRaises(DatasetValidationError):
            validate_dataset(path)

    def test_rejects_invalid_confidence(self) -> None:
        sample = make_sample()
        sample["action"]["confidence"] = 1.1
        path = self.write_dataset([METADATA, sample])

        with self.assertRaises(DatasetValidationError):
            validate_dataset(path)

    def test_rejects_non_terminal_negative_elapsed_time(self) -> None:
        sample = make_sample()
        sample["elapsed_time"] = -0.1
        path = self.write_dataset([METADATA, sample])

        with self.assertRaises(DatasetValidationError):
            validate_dataset(path)


if __name__ == "__main__":
    unittest.main()
