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

    def test_v3_accepts_aim_events_without_counting_samples(self) -> None:
        event = {"type": "aim_event", "event": "target_switched", "reason": "target_changed",
                 "game_time": 35.0, "bot_id": 7, "previous_target_id": 2, "target_id": 5,
                 "view_yaw": 90.0, "view_pitch": 0.0, "yaw_delta": 70.0,
                 "pitch_delta": 1.0, "elapsed": 0.1}
        self.assertEqual(validate_dataset(self.write_dataset([METADATA, make_sample(), event])), 1)

    def test_v3_accepts_flash_avoidance_reason(self) -> None:
        event = {"type": "aim_event", "event": "rapid_aim_turn", "reason": "flash_avoidance",
                 "game_time": 35.0, "bot_id": 7, "previous_target_id": -1, "target_id": -1,
                 "view_yaw": -72.0, "view_pitch": 0.0, "yaw_delta": -72.0,
                 "pitch_delta": 0.0, "elapsed": 0.1}
        self.assertEqual(validate_dataset(self.write_dataset([METADATA, make_sample(), event])), 1)

    def test_v3_rejects_invalid_aim_id(self) -> None:
        event = {"type": "aim_event", "event": "target_switched", "reason": "target_changed",
                 "game_time": 35.0, "bot_id": 7, "previous_target_id": True, "target_id": 5,
                 "view_yaw": 90.0, "view_pitch": 0.0, "yaw_delta": 70.0,
                 "pitch_delta": 1.0, "elapsed": 0.1}
        with self.assertRaises(DatasetValidationError):
            validate_dataset(self.write_dataset([METADATA, event]))

    def test_v2_rejects_aim_diagnostics(self) -> None:
        event = {"type": "aim_event", "event": "rapid_aim_turn", "reason": "aim_enemy",
                 "game_time": 35.0, "bot_id": 7, "previous_target_id": -1, "target_id": 5,
                 "view_yaw": 90.0, "view_pitch": 0.0, "yaw_delta": 70.0,
                 "pitch_delta": 1.0, "elapsed": 0.1}
        with self.assertRaises(DatasetValidationError):
            validate_dataset(self.write_dataset([dict(METADATA, version=2), event]))

    def test_v3_accepts_navigation_events_without_counting_samples(self) -> None:
        event = {"type": "navigation_event", "event": "route_observed",
                 "game_time": 30.0, "bot_id": 7, "path_nodes": [1, 5, 9]}
        path = self.write_dataset([METADATA, make_sample(), event])
        self.assertEqual(validate_dataset(path), 1)


    def test_v3_accepts_dropped_bomb_guard_event(self) -> None:
        event = {"type": "navigation_event", "event": "dropped_bomb_guard",
                 "reason": "primary_assigned", "game_time": 31.0, "bot_id": 7,
                 "guard_exposure": 17, "guard_route_distance": 275.0,
                 "guard_nearest_ally_distance": 240.0}
        self.assertEqual(validate_dataset(self.write_dataset([METADATA, make_sample(), event])), 1)

    def test_v3_accepts_defuse_events_without_counting_them(self) -> None:
        events = [
            {"type": "defuse_event", "event": "defuse_attempt",
             "game_time": 1.0, "bot_id": 3, "evidence_source": "in_use"},
            {"type": "defuse_event", "event": "defuse_start",
             "game_time": 2.0, "bot_id": 3, "evidence_source": "bar_time_positive"},
            {"type": "defuse_event", "event": "defuse_interrupted",
             "game_time": 3.0, "bot_id": 3, "evidence_source": "bar_time_zero"},
            {"type": "defuse_event", "event": "defuse_complete",
             "game_time": 4.0, "bot_id": -1, "evidence_source": "bomb_defused_text_message"},
        ]
        self.assertEqual(validate_dataset(self.write_dataset([METADATA, make_sample(), *events])), 1)

    def test_v3_accepts_approach_diagnostics_without_counting_samples(self) -> None:
        blocked = {"type": "defuse_event", "event": "defuse_approach_blocked",
                   "game_time": 15.0, "bot_id": 4, "attempt_id": 0,
                   "evidence_source": "geometry_reachability"}
        failed = {"type": "defuse_event", "event": "defuse_approach_failed",
                  "game_time": 16.0, "bot_id": 4, "attempt_id": 0,
                  "evidence_source": "graph_route_unavailable"}
        self.assertEqual(validate_dataset(self.write_dataset([METADATA, make_sample(), blocked, failed])), 1)

    def test_v3_rejects_incorrect_approach_evidence(self) -> None:
        event = {"type": "defuse_event", "event": "defuse_approach_failed",
                 "game_time": 17.0, "bot_id": 4, "evidence_source": "bar_time_positive"}
        with self.assertRaises(DatasetValidationError):
            validate_dataset(self.write_dataset([METADATA, event]))

    def test_v3_rejects_unattributed_approach_failure(self) -> None:
        event = {"type": "defuse_event", "event": "defuse_approach_failed",
                 "game_time": 17.0, "bot_id": -1, "evidence_source": "graph_route_unavailable"}
        with self.assertRaises(DatasetValidationError):
            validate_dataset(self.write_dataset([METADATA, event]))

    def test_defuse_interruption_accepts_observed_state_without_callback_claim(self) -> None:
        for source in ("observed_dead", "game_state"):
            event = {"type": "defuse_event", "event": "defuse_interrupted",
                     "game_time": 3.0, "bot_id": 3, "attempt_id": 2,
                     "evidence_source": source}
            self.assertEqual(validate_dataset(self.write_dataset([METADATA, event])), 0)

    def test_defuse_start_rejects_non_authoritative_evidence(self) -> None:
        event = {"type": "defuse_event", "event": "defuse_start",
                 "game_time": 3.0, "bot_id": 3, "evidence_source": "in_use"}
        with self.assertRaises(DatasetValidationError):
            validate_dataset(self.write_dataset([METADATA, event]))

    def test_defuse_attempt_rejects_bar_time_as_input_evidence(self) -> None:
        event = {"type": "defuse_event", "event": "defuse_attempt",
                 "game_time": 3.0, "bot_id": 3, "evidence_source": "bar_time_positive"}
        with self.assertRaises(DatasetValidationError):
            validate_dataset(self.write_dataset([METADATA, event]))

    def test_defuse_rejects_invalid_attempt_id(self) -> None:
        event = {"type": "defuse_event", "event": "defuse_interrupted",
                 "game_time": 3.0, "bot_id": 3, "attempt_id": True,
                 "evidence_source": "game_state"}
        with self.assertRaises(DatasetValidationError):
            validate_dataset(self.write_dataset([METADATA, event]))

    def test_v2_rejects_defuse_events(self) -> None:
        event = {"type": "defuse_event", "event": "defuse_start",
                 "game_time": 3.0, "bot_id": 3, "evidence_source": "bar_time_positive"}
        with self.assertRaises(DatasetValidationError):
            validate_dataset(self.write_dataset([dict(METADATA, version=2), event]))

    def test_defuse_complete_needs_authoritative_evidence_and_unknown_actor(self) -> None:
        event = {"type": "defuse_event", "event": "defuse_complete",
                 "game_time": 3.0, "bot_id": 3, "evidence_source": "round_message"}
        with self.assertRaises(DatasetValidationError):
            validate_dataset(self.write_dataset([METADATA, event]))

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
