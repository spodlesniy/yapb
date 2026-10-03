#!/usr/bin/env python3
"""Stable model input and output contract shared by training and export."""

from __future__ import annotations

from enum import IntEnum


MODEL_INPUT_NAME = "input"
MODEL_OUTPUT_NAME = "output"
MODEL_INPUT_DTYPE = "float32"
MODEL_OUTPUT_DTYPE = "float32"

MODEL_FEATURE_SCHEMA_VERSION = 1
MODEL_ACTION_SCHEMA_VERSION = 1

MODEL_FEATURE_COUNT = 230
MODEL_ACTION_TENSOR_SIZE = 10

MODEL_RUNTIME_INPUT_SHAPE = (1, MODEL_FEATURE_COUNT)
MODEL_RUNTIME_OUTPUT_SHAPE = (1, MODEL_ACTION_TENSOR_SIZE)


class ModelOutputIndex(IntEnum):
    """Stable positions in the policy model output tensor."""

    ACTION_ID = 0
    TARGET_NODE = 1
    TARGET_PLAYER = 2
    TARGET_POSITION_X = 3
    TARGET_POSITION_Y = 4
    TARGET_POSITION_Z = 5
    WEAPON_TYPE = 6
    GRENADE_TYPE = 7
    DURATION = 8
    CONFIDENCE = 9


def validate_feature_count(feature_count: int) -> None:
    """Require the C++/ONNX policy input width."""
    if feature_count != MODEL_FEATURE_COUNT:
        raise ValueError(
            f"model input has {feature_count} features; expected {MODEL_FEATURE_COUNT}"
        )
