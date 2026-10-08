#!/usr/bin/env python3
"""Unit tests for the AiPB training dataset validator."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from tools.aipb_training.model_contract import (
    MODEL_ACTION_ID_COUNT,
    MODEL_FEATURE_COUNT,
    MODEL_ACTION_SCHEMA_VERSION,
    MODEL_FEATURE_SCHEMA_VERSION,
)
from tools.aipb_training.validate_dataset import DatasetValidationError, validate_dataset


METADATA = {
    "format": "aipb-training-jsonl",
    "version": 3,
    "feature_schema_version": MODEL_FEATURE_SCHEMA_VERSION,
    "action_schema_version": MODEL_ACTION_SCHEMA_VERSION,
    "type": "metadata",
}


def make_sample(feature_count: int = MODEL_FEATURE_COUNT) -> dict:
    return {
        "episode_id": 1,
        "observation": {"schema_version": MODEL_FEATURE_SCHEMA_VERSION, "values": [0.1] * feature_count},
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
        "next_observation": {"schema_version": MODEL_FEATURE_SCHEMA_VERSION, "values": [0.2] * feature_count},
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

    def test_legacy_v2_dataset_still_valid(self) -> None:
        path = self.write_dataset([dict(METADATA, version=2), make_sample()])
        self.assertEqual(validate_dataset(path), 1)

    def test_v3_ignores_diagnostic_events(self) -> None:
        event = {"type": "combat_event", "event": "weapon_fire",
                 "game_time": 12.5, "bot_id": 7, "evidence_source": "clip_decrease"}
        path = self.write_dataset([METADATA, make_sample(), event])
        self.assertEqual(validate_dataset(path), 1)

    def test_v3_accepts_navigation_events_without_counting_samples(self) -> None:
        event = {"type": "navigation_event", "event": "route_observed",
                 "game_time": 30.0, "bot_id": 7, "path_nodes": [1, 5, 9]}
        path = self.write_dataset([METADATA, make_sample(), event])
        self.assertEqual(validate_dataset(path), 1)

    def test_v2_rejects_navigation_events(self) -> None:
        event = {"type": "navigation_event", "event": "task_change",
                 "game_time": 30.0, "bot_id": 7}
        path = self.write_dataset([dict(METADATA, version=2), make_sample(), event])
        with self.assertRaises(DatasetValidationError):
            validate_dataset(path)

    def test_rejects_wrong_runtime_feature_count(self) -> None:
        path = self.write_dataset([METADATA, make_sample(230)])

        with self.assertRaises(DatasetValidationError):
            validate_dataset(path)

    def test_valid_dataset_counts_samples(self) -> None:
        path = self.write_dataset([METADATA, make_sample()])
        self.assertEqual(validate_dataset(path), 1)

    def test_rejects_invalid_metadata(self) -> None:
        metadata = dict(METADATA)
        metadata["version"] = 1
        path = self.write_dataset([metadata])

        with self.assertRaises(DatasetValidationError):
            validate_dataset(path)

    def test_rejects_mismatched_feature_count(self) -> None:
        path = self.write_dataset([METADATA, make_sample(2), make_sample(3)])

        with self.assertRaises(DatasetValidationError):
            validate_dataset(path)

    def test_rejects_action_id_outside_model_contract(self) -> None:
        sample = make_sample()
        sample["action"]["action_id"] = MODEL_ACTION_ID_COUNT
        path = self.write_dataset([METADATA, sample])

        with self.assertRaises(DatasetValidationError):
            validate_dataset(path)

    def test_rejects_action_parameter_mismatch(self) -> None:
        sample = make_sample()
        sample["action"]["action_id"] = 8
        path = self.write_dataset([METADATA, sample])
        with self.assertRaises(DatasetValidationError):
            validate_dataset(path)

    def test_accepts_valid_grenade_semantics(self) -> None:
        sample = make_sample()
        sample["action"].update(
            action_id=24,
            target_node=-1,
            target_player=-1,
            target_position=[100.0, 200.0, 300.0],
            weapon_type=0,
            grenade_type=3,
            duration=0.0,
        )
        path = self.write_dataset([METADATA, sample])
        self.assertEqual(validate_dataset(path), 1)

    def test_rejects_wrong_grenade_for_action(self) -> None:
        sample = make_sample()
        sample["action"].update(
            action_id=23,
            target_node=-1,
            target_player=-1,
            target_position=[100.0, 200.0, 300.0],
            grenade_type=3,
            duration=0.0,
        )
        path = self.write_dataset([METADATA, sample])
        with self.assertRaises(DatasetValidationError):
            validate_dataset(path)

    def test_rejects_non_default_parameter_for_targetless_action(self) -> None:
        sample = make_sample()
        sample["action"]["action_id"] = 18
        sample["action"]["target_node"] = 42
        path = self.write_dataset([METADATA, sample])
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
