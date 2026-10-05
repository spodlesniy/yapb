#!/usr/bin/env python3
"""Initial PyTorch policy model for AiPB behavior cloning."""

from __future__ import annotations

from dataclasses import dataclass

from .model_contract import MODEL_ACTION_ID_COUNT, MODEL_ACTION_TENSOR_SIZE, MODEL_FEATURE_COUNT


@dataclass(frozen=True)
class PolicyModelArchitecture:
    """Framework-independent description of the initial policy network."""

    input_features: int
    hidden_features: tuple[int, ...]
    output_features: int
    action_class_count: int
    continuous_output_features: int
    normalization: str
    activation: str


POLICY_MODEL_ARCHITECTURE = PolicyModelArchitecture(
    input_features=MODEL_FEATURE_COUNT,
    hidden_features=(256, 256, 128),
    output_features=MODEL_ACTION_TENSOR_SIZE,
    action_class_count=MODEL_ACTION_ID_COUNT,
    continuous_output_features=MODEL_ACTION_TENSOR_SIZE - 1,
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

    trunk = nn.Sequential(*layers)

    class PolicyModel(nn.Module):
        def __init__(self) -> None:
            super().__init__()
            self.trunk = trunk
            self.action_head = nn.Linear(input_features, POLICY_MODEL_ARCHITECTURE.action_class_count)
            self.parameter_head = nn.Linear(input_features, POLICY_MODEL_ARCHITECTURE.continuous_output_features)

        def forward_training(self, observations):
            import torch

            features = self.trunk(observations)
            action_logits = self.action_head(features)
            action_parameters = self.parameter_head(features)
            action_id = action_logits.argmax(dim=1, keepdim=True).to(action_parameters.dtype)
            return torch.cat((action_id, action_parameters), dim=1), action_logits

        def forward(self, observations):
            output, _ = self.forward_training(observations)
            return output

    return PolicyModel()
