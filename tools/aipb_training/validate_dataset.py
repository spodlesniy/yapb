#!/usr/bin/env python3
"""Validate an AiPB training dataset produced by the in-game exporter."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any


EXPECTED_FORMAT = "aipb-training-jsonl"
EXPECTED_DATASET_VERSION = 1
EXPECTED_FEATURE_SCHEMA_VERSION = 1
EXPECTED_ACTION_SCHEMA_VERSION = 1
REQUIRED_OBSERVATION_KEYS = {"schema_version", "values"}
REQUIRED_ACTION_KEYS = {
    "schema_version",
    "action_id",
    "target_node",
    "target_player",
    "target_position",
    "weapon_type",
    "grenade_type",
    "duration",
    "confidence",
}
REQUIRED_SAMPLE_KEYS = {
    "episode_id",
    "observation",
    "action",
    "reward",
    "next_observation",
    "result",
    "elapsed_time",
    "terminal",
}


class DatasetValidationError(ValueError):
    """Raised when a dataset violates the AiPB JSONL contract."""


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise DatasetValidationError(message)


def _is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(float(value))


def _validate_vec3(value: Any, field_name: str) -> None:
    _require(isinstance(value, list) and len(value) == 3, f"{field_name} must contain exactly three values")
    _require(all(_is_number(item) for item in value), f"{field_name} must contain finite numbers")


def _validate_observation(value: Any, field_name: str) -> int:
    _require(isinstance(value, dict), f"{field_name} must be an object")
    _require(set(value) == REQUIRED_OBSERVATION_KEYS, f"{field_name} keys do not match the schema")
    _require(value["schema_version"] == EXPECTED_FEATURE_SCHEMA_VERSION, f"{field_name} has an unsupported schema version")
    values = value["values"]
    _require(isinstance(values, list), f"{field_name}.values must be an array")
    _require(all(_is_number(item) for item in values), f"{field_name}.values must contain finite numbers")
    return len(values)


def _validate_action(value: Any) -> None:
    _require(isinstance(value, dict), "action must be an object")
    _require(set(value) == REQUIRED_ACTION_KEYS, "action keys do not match the schema")
    _require(value["schema_version"] == EXPECTED_ACTION_SCHEMA_VERSION, "action has an unsupported schema version")

    for field_name in ("action_id", "target_node", "target_player", "weapon_type", "grenade_type"):
        value_field = value[field_name]
        _require(isinstance(value_field, int) and not isinstance(value_field, bool),
                 f"action.{field_name} must be an integer")

    _require(0 <= value["action_id"] <= 255, "action.action_id must fit in uint8")
    _validate_vec3(value["target_position"], "action.target_position")

    for field_name in ("duration", "confidence"):
        _require(_is_number(value[field_name]), f"action.{field_name} must be a finite number")

    _require(0.0 <= float(value["confidence"]) <= 1.0, "action.confidence must be within [0, 1]")


def _validate_sample(value: Any, line_number: int) -> int:
    _require(isinstance(value, dict), f"line {line_number}: sample must be an object")
    _require(set(value) == REQUIRED_SAMPLE_KEYS, f"line {line_number}: sample keys do not match the schema")

    _require(
        isinstance(value["episode_id"], int)
        and not isinstance(value["episode_id"], bool)
        and value["episode_id"] > 0,
        f"line {line_number}: episode_id must be a positive integer",
    )

    observation_size = _validate_observation(value["observation"], f"line {line_number}: observation")
    next_observation_size = _validate_observation(value["next_observation"], f"line {line_number}: next_observation")
    _require(
        observation_size == next_observation_size,
        f"line {line_number}: observation and next_observation must have the same feature count",
    )

    _validate_action(value["action"])

    for field_name in ("reward", "elapsed_time"):
        _require(_is_number(value[field_name]), f"line {line_number}: {field_name} must be a finite number")

    _require(isinstance(value["result"], int) and not isinstance(value["result"], bool),
             f"line {line_number}: result must be an integer")
    _require(0 <= value["result"] <= 255, f"line {line_number}: result must fit in uint8")
    _require(isinstance(value["terminal"], bool), f"line {line_number}: terminal must be boolean")
    _require(value["elapsed_time"] >= 0.0, f"line {line_number}: elapsed_time must be non-negative")

    return observation_size


def iter_validated_samples(path: str | Path):
    """Yield validated sample dictionaries in file order."""
    dataset_path = Path(path)
    _require(dataset_path.is_file(), f"dataset file does not exist: {dataset_path}")

    feature_count: int | None = None

    with dataset_path.open("r", encoding="utf-8") as stream:
        first_line = stream.readline()
        _require(bool(first_line), "dataset is empty")

        try:
            metadata = json.loads(first_line)
        except json.JSONDecodeError as exc:
            raise DatasetValidationError(f"line 1: invalid JSON: {exc.msg}") from exc

        _require(isinstance(metadata, dict), "line 1: metadata must be an object")
        expected_metadata = {
            "format": EXPECTED_FORMAT,
            "version": EXPECTED_DATASET_VERSION,
            "feature_schema_version": EXPECTED_FEATURE_SCHEMA_VERSION,
            "action_schema_version": EXPECTED_ACTION_SCHEMA_VERSION,
            "type": "metadata",
        }
        _require(metadata == expected_metadata, "line 1: metadata does not match the supported dataset contract")

        for line_number, line in enumerate(stream, start=2):
            if not line.strip():
                continue

            try:
                value = json.loads(line)
            except json.JSONDecodeError as exc:
                raise DatasetValidationError(f"line {line_number}: invalid JSON: {exc.msg}") from exc

            current_feature_count = _validate_sample(value, line_number)
            if feature_count is None:
                feature_count = current_feature_count
            else:
                _require(
                    current_feature_count == feature_count,
                    f"line {line_number}: feature count differs from earlier samples",
                )
            yield value


def validate_dataset(path: str | Path) -> int:
    return sum(1 for _ in iter_validated_samples(path))


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate an AiPB training dataset JSONL file.")
    parser.add_argument("dataset", type=Path, help="Path to the exported AiPB training dataset.")
    args = parser.parse_args()

    try:
        sample_count = validate_dataset(args.dataset)
    except DatasetValidationError as exc:
        parser.error(str(exc))

    print(f"valid: {args.dataset} ({sample_count} samples)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
