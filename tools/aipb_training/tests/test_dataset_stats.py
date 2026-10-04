#!/usr/bin/env python3
"""Unit tests for AiPB dataset statistics."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from ..dataset_stats import MODEL_ACTION_ID_COUNT, build_parser, summarize_dataset
from ..model_contract import MODEL_ACTION_SCHEMA_VERSION, MODEL_FEATURE_SCHEMA_VERSION


METADATA = {
    "format": "aipb-training-jsonl",
    "version": 1,
    "feature_schema_version": MODEL_FEATURE_SCHEMA_VERSION,
    "action_schema_version": MODEL_ACTION_SCHEMA_VERSION,
    "type": "metadata",
}


def sample(action_id: int, episode_id: int, terminal: bool) -> dict:
    from ..model_contract import MODEL_FEATURE_COUNT

    values = [0.0] * MODEL_FEATURE_COUNT
    return {
        "episode_id": episode_id,
        "observation": {"schema_version": MODEL_FEATURE_SCHEMA_VERSION, "values": values},
        "action": {
            "schema_version": MODEL_ACTION_SCHEMA_VERSION,
            "action_id": action_id,
            "target_node": 42,
            "target_player": -1,
            "target_position": [1.0, 2.0, 3.0],
            "weapon_type": 0,
            "grenade_type": 0,
            "duration": 0.5,
            "confidence": 1.0,
        },
        "reward": 1.0,
        "next_observation": {"schema_version": MODEL_FEATURE_SCHEMA_VERSION, "values": values},
        "result": 1,
        "elapsed_time": 0.25,
        "terminal": terminal,
    }


class DatasetStatsTests(unittest.TestCase):
    def write_dataset(self, values: list[dict]) -> Path:
        handle = tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", suffix=".jsonl", delete=False)
        with handle:
            for value in values:
                handle.write(json.dumps(value) + "\n")
        path = Path(handle.name)
        self.addCleanup(path.unlink, missing_ok=True)
        return path

    def test_summarizes_action_distribution_and_episode_count(self) -> None:
        path = self.write_dataset([
            METADATA,
            sample(1, 10, True),
            sample(1, 10, True),
            sample(8, 20, True),
        ])

        stats = summarize_dataset(path)

        self.assertEqual(stats.samples, 3)
        self.assertEqual(stats.episodes, 2)
        self.assertEqual(stats.terminal_samples, 3)
        self.assertEqual(stats.action_counts[1], 2)
        self.assertEqual(stats.action_counts[8], 1)
        self.assertEqual(stats.action_coverage, 2)

    def test_empty_dataset_has_zero_coverage(self) -> None:
        path = self.write_dataset([METADATA])

        stats = summarize_dataset(path)

        self.assertEqual(stats.samples, 0)
        self.assertEqual(stats.episodes, 0)
        self.assertEqual(stats.action_coverage, 0)
        self.assertEqual(len(stats.action_counts), MODEL_ACTION_ID_COUNT)

    def test_parser_accepts_dataset_path(self) -> None:
        args = build_parser().parse_args(["dataset.jsonl"])

        self.assertEqual(args.dataset, "dataset.jsonl")


if __name__ == "__main__":
    unittest.main()
