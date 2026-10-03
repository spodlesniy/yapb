#!/usr/bin/env python3
"""Framework-neutral training contract for the AiPB policy model."""

from __future__ import annotations

from dataclasses import dataclass

from .dataset import TrainingBatch, TrainingSample
from .model_contract import MODEL_ACTION_TENSOR_SIZE, MODEL_FEATURE_COUNT, ModelOutputIndex


ACTION_TENSOR_SIZE = MODEL_ACTION_TENSOR_SIZE


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
    target = [0.0] * ACTION_TENSOR_SIZE
    target[ModelOutputIndex.ACTION_ID] = float(action.action_id)
    target[ModelOutputIndex.TARGET_NODE] = float(action.target_node)
    target[ModelOutputIndex.TARGET_PLAYER] = float(action.target_player)
    target[ModelOutputIndex.TARGET_POSITION_X] = float(action.target_position[0])
    target[ModelOutputIndex.TARGET_POSITION_Y] = float(action.target_position[1])
    target[ModelOutputIndex.TARGET_POSITION_Z] = float(action.target_position[2])
    target[ModelOutputIndex.WEAPON_TYPE] = float(action.weapon_type)
    target[ModelOutputIndex.GRENADE_TYPE] = float(action.grenade_type)
    target[ModelOutputIndex.DURATION] = float(action.duration)
    target[ModelOutputIndex.CONFIDENCE] = float(action.confidence)
    return tuple(target)


def encode_policy_batch(batch: TrainingBatch) -> PolicyTrainingBatch:
    """Convert a sample batch into the current model input/target contract."""
    if not batch.samples:
        return PolicyTrainingBatch((), ())

    observations: list[tuple[float, ...]] = []
    action_targets: list[tuple[float, ...]] = []

    for index, sample in enumerate(batch.samples):
        current_feature_count = len(sample.observation.values)
        if current_feature_count != MODEL_FEATURE_COUNT:
            raise ValueError(
                f"sample {index} has {current_feature_count} features; expected {MODEL_FEATURE_COUNT}"
            )

        target = encode_action_target(sample)
        if len(target) != ACTION_TENSOR_SIZE:
            raise ValueError(
                f"action target has {len(target)} values; expected {ACTION_TENSOR_SIZE}"
            )

        observations.append(sample.observation.values)
        action_targets.append(target)

    return PolicyTrainingBatch(tuple(observations), tuple(action_targets))
