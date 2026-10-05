#!/usr/bin/env python3
"""Unit tests for the AiPB policy model architecture."""

from __future__ import annotations

import importlib.util
import unittest

from ..model_contract import MODEL_ACTION_ID_COUNT, MODEL_ACTION_TENSOR_SIZE, MODEL_FEATURE_COUNT
from ..policy_model import POLICY_MODEL_ARCHITECTURE, build_policy_model


class PolicyModelArchitectureTests(unittest.TestCase):
    def test_architecture_matches_model_io_contract(self) -> None:
        architecture = POLICY_MODEL_ARCHITECTURE

        self.assertEqual(architecture.input_features, MODEL_FEATURE_COUNT)
        self.assertEqual(architecture.output_features, MODEL_ACTION_TENSOR_SIZE)
        self.assertEqual(architecture.action_class_count, MODEL_ACTION_ID_COUNT)
        self.assertEqual(architecture.continuous_output_features, MODEL_ACTION_TENSOR_SIZE - 1)
        self.assertEqual(architecture.hidden_features, (256, 256, 128))
        self.assertEqual(architecture.normalization, "layernorm")
        self.assertEqual(architecture.activation, "relu")

    def test_model_build_is_available_when_pytorch_is_installed(self) -> None:
        if importlib.util.find_spec("torch") is None:
            self.skipTest("PyTorch is not installed in the lightweight test environment")

        model = build_policy_model()

        self.assertEqual(len(list(model.parameters())), 12)

        import torch

        model.eval()
        inputs = torch.zeros((4, MODEL_FEATURE_COUNT), dtype=torch.float32)
        outputs, logits = model.forward_training(inputs)

        self.assertEqual(tuple(outputs.shape), (4, MODEL_ACTION_TENSOR_SIZE))
        self.assertEqual(tuple(logits.shape), (4, MODEL_ACTION_ID_COUNT))
        self.assertTrue(torch.isfinite(outputs).all().item())

    def test_model_is_deterministic_in_eval_mode(self) -> None:
        if importlib.util.find_spec("torch") is None:
            self.skipTest("PyTorch is not installed in the lightweight test environment")

        import torch

        model = build_policy_model()
        model.eval()

        inputs = torch.zeros((1, MODEL_FEATURE_COUNT), dtype=torch.float32)
        first = model(inputs)
        second = model(inputs)

        self.assertTrue(torch.equal(first, second))


if __name__ == "__main__":
    unittest.main()
