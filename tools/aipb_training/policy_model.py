#!/usr/bin/env python3
"""Initial PyTorch policy model for AiPB behavior cloning."""

from __future__ import annotations

from dataclasses import dataclass

from .model_contract import MODEL_ACTION_TENSOR_SIZE, MODEL_FEATURE_COUNT


@dataclass(frozen=True)
class PolicyModelArchitecture:
    """Framework-independent description of the initial policy network."""

    input_features: int
    hidden_features: tuple[int, ...]
    output_features: int
    normalization: str
    activation: str


POLICY_MODEL_ARCHITECTURE = PolicyModelArchitecture(
    input_features=MODEL_FEATURE_COUNT,
    hidden_features=(256, 256, 128),
    output_features=MODEL_ACTION_TENSOR_SIZE,
    normalization="layernorm",
    activation="relu",
)


def build_policy_model():
    """Build the initial PyTorch policy model.

    PyTorch is imported lazily so validation and dataset tooling remain usable
    without the training dependency installed.
    """
    try:
        import torch.nn as nn
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "PyTorch is required to build the AiPB policy model. "
            "Install tools/aipb_training/requirements.txt."
        ) from exc

    layers = [nn.LayerNorm(POLICY_MODEL_ARCHITECTURE.input_features)]
    input_features = POLICY_MODEL_ARCHITECTURE.input_features

    for hidden_features in POLICY_MODEL_ARCHITECTURE.hidden_features:
        layers.append(nn.Linear(input_features, hidden_features))
        layers.append(nn.ReLU())
        input_features = hidden_features

    layers.append(nn.Linear(input_features, POLICY_MODEL_ARCHITECTURE.output_features))
    return nn.Sequential(*layers)
