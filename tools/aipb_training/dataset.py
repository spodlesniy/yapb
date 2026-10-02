#!/usr/bin/env python3
"""Read and batch an AiPB training dataset."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterator

from validate_dataset import (
    EXPECTED_ACTION_SCHEMA_VERSION,
    EXPECTED_DATASET_VERSION,
    EXPECTED_FEATURE_SCHEMA_VERSION,
    EXPECTED_FORMAT,
    iter_validated_samples,
)


@dataclass(frozen=True)
class TrainingDatasetMetadata:
    format: str
    version: int
    feature_schema_version: int
    action_schema_version: int


@dataclass(frozen=True)
class TrainingObservation:
    values: tuple[float, ...]


@dataclass(frozen=True)
class TrainingAction:
    action_id: int
    target_node: int
    target_player: int
    target_position: tuple[float, float, float]
    weapon_type: int
    grenade_type: int
    duration: float
    confidence: float


@dataclass(frozen=True)
class TrainingSample:
    episode_id: int
    observation: TrainingObservation
    action: TrainingAction
    reward: float
    next_observation: TrainingObservation
    result: int
    elapsed_time: float
    terminal: bool


@dataclass(frozen=True)
class TrainingBatch:
    samples: tuple[TrainingSample, ...]

    @property
    def size(self) -> int:
        return len(self.samples)


def _metadata() -> TrainingDatasetMetadata:
    return TrainingDatasetMetadata(
        format=EXPECTED_FORMAT,
        version=EXPECTED_DATASET_VERSION,
        feature_schema_version=EXPECTED_FEATURE_SCHEMA_VERSION,
        action_schema_version=EXPECTED_ACTION_SCHEMA_VERSION,
    )


def _to_sample(value: dict) -> TrainingSample:
    observation = TrainingObservation(tuple(float(item) for item in value["observation"]["values"]))
    next_observation = TrainingObservation(
        tuple(float(item) for item in value["next_observation"]["values"])
    )

    action_value = value["action"]
    action = TrainingAction(
        action_id=int(action_value["action_id"]),
        target_node=int(action_value["target_node"]),
        target_player=int(action_value["target_player"]),
        target_position=tuple(float(item) for item in action_value["target_position"]),  # type: ignore[arg-type]
        weapon_type=int(action_value["weapon_type"]),
        grenade_type=int(action_value["grenade_type"]),
        duration=float(action_value["duration"]),
        confidence=float(action_value["confidence"]),
    )

    return TrainingSample(
        episode_id=int(value["episode_id"]),
        observation=observation,
        action=action,
        reward=float(value["reward"]),
        next_observation=next_observation,
        result=int(value["result"]),
        elapsed_time=float(value["elapsed_time"]),
        terminal=bool(value["terminal"]),
    )


def iter_training_samples(path: str | Path) -> Iterator[TrainingSample]:
    """Yield validated training samples in the same order as the JSONL file."""
    for value in iter_validated_samples(path):
        yield _to_sample(value)


def load_training_dataset(path: str | Path) -> tuple[TrainingDatasetMetadata, tuple[TrainingSample, ...]]:
    """Load a complete validated dataset into immutable Python structures."""
    return _metadata(), tuple(iter_training_samples(path))


def iter_training_batches(
    samples: Iterator[TrainingSample],
    batch_size: int,
) -> Iterator[TrainingBatch]:
    """Yield deterministic, contiguous batches without changing sample order."""
    if batch_size <= 0:
        raise ValueError("batch_size must be positive")

    batch: list[TrainingSample] = []
    for sample in samples:
        batch.append(sample)
        if len(batch) == batch_size:
            yield TrainingBatch(tuple(batch))
            batch.clear()

    if batch:
        yield TrainingBatch(tuple(batch))
