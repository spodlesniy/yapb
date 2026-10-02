#!/usr/bin/env python3
"""Framework-neutral training contract for the AiPB policy model."""

from __future__ import annotations

from dataclasses import dataclass

from dataset import TrainingBatch, TrainingSample


ACTION_TENSOR_SIZE = 10


@dataclass(frozen=True)
class PolicyTrainingBatch:
    """Model-ready policy inputs and action targets."""

    observations: tuple[tuple[float, ...], ...]
    action_targets: tuple[tuple[float, ...], ...]

    @property
    def size(self) -> int:
        return len(self.observations)

    @property
    def feature_count(self) -> int:
        if not self.observations:
            return 0
        return len(self.observations[0])


def encode_action_target(sample: TrainingSample) -> tuple[float, ...]:
    """Encode an action using the exact order of the inference output tensor."""
    action = sample.action
    return (
        float(action.action_id),
        float(action.target_node),
        float(action.target_player),
        float(action.target_position[0]),
        float(action.target_position[1]),
        float(action.target_position[2]),
        float(action.weapon_type),
        float(action.grenade_type),
        float(action.duration),
        float(action.confidence),
    )


def encode_policy_batch(batch: TrainingBatch) -> PolicyTrainingBatch:
    """Convert a sample batch into the current model input/target contract."""
    if not batch.samples:
        return PolicyTrainingBatch((), ())

    feature_count = len(batch.samples[0].observation.values)
    observations: list[tuple[float, ...]] = []
    action_targets: list[tuple[float, ...]] = []

    for index, sample in enumerate(batch.samples):
        current_feature_count = len(sample.observation.values)
        if current_feature_count != feature_count:
            raise ValueError(
                f"sample {index} has {current_feature_count} features; expected {feature_count}"
            )

        target = encode_action_target(sample)
        if len(target) != ACTION_TENSOR_SIZE:
            raise ValueError(
                f"action target has {len(target)} values; expected {ACTION_TENSOR_SIZE}"
            )

        observations.append(sample.observation.values)
        action_targets.append(target)

    return PolicyTrainingBatch(tuple(observations), tuple(action_targets))
