#!/usr/bin/env python3
"""Unit tests for the AiPB training core."""

from __future__ import annotations

import importlib.util
import unittest

from ..dataset import TrainingAction, TrainingBatch, TrainingObservation, TrainingSample
from ..model_contract import MODEL_ACTION_TENSOR_SIZE, MODEL_FEATURE_COUNT
from ..policy_model import build_policy_model
from ..trainer import create_optimizer, evaluate, policy_loss, train_epoch
from ..training_contract import encode_policy_batch

TORCH_AVAILABLE = importlib.util.find_spec("torch") is not None

def make_sample(action_id: int, value: float) -> TrainingSample:
    values = (value,) * MODEL_FEATURE_COUNT
    return TrainingSample(
        episode_id=1,
        observation=TrainingObservation(values),
        action=TrainingAction(action_id, 42, -1, (1.0, 2.0, 3.0), 1, 0, 0.5, 0.8),
        reward=1.0, next_observation=TrainingObservation(values), result=2,
        elapsed_time=0.5, terminal=True,
    )

@unittest.skipUnless(TORCH_AVAILABLE, "PyTorch is not installed in the lightweight test environment")
class TrainingCoreTests(unittest.TestCase):
    def setUp(self) -> None:
        import torch
        torch.manual_seed(1234)
        self.model = build_policy_model()
        self.optimizer = create_optimizer(self.model)
        self.batch = encode_policy_batch(TrainingBatch((make_sample(1, 0.1), make_sample(2, 0.2))))

    def test_policy_loss_is_zero_for_identical_tensors(self) -> None:
        import torch
        values = torch.zeros((2, MODEL_ACTION_TENSOR_SIZE), dtype=torch.float32)
        self.assertEqual(float(policy_loss(values, values).item()), 0.0)

    def test_train_epoch_updates_model_and_returns_metrics(self) -> None:
        import torch
        before = [p.detach().clone() for p in self.model.parameters()]
        metrics = train_epoch(self.model, [self.batch], self.optimizer)
        self.assertEqual(metrics.samples, 2)
        self.assertGreater(metrics.loss, 0.0)
        self.assertTrue(any(not torch.equal(a, b.detach()) for a, b in zip(before, self.model.parameters())))

    def test_evaluate_preserves_model_parameters(self) -> None:
        import torch
        train_epoch(self.model, [self.batch], self.optimizer)
        before = [p.detach().clone() for p in self.model.parameters()]
        metrics = evaluate(self.model, [self.batch])
        self.assertEqual(metrics.samples, 2)
        self.assertGreaterEqual(metrics.loss, 0.0)
        self.assertTrue(all(torch.equal(a, b.detach()) for a, b in zip(before, self.model.parameters())))

    def test_empty_batches_are_supported(self) -> None:
        metrics = train_epoch(self.model, [encode_policy_batch(TrainingBatch(()))], self.optimizer)
        self.assertEqual(metrics.samples, 0)
        self.assertEqual(metrics.loss, 0.0)

    def test_model_output_shape_is_checked(self) -> None:
        import torch
        targets = torch.zeros((1, MODEL_ACTION_TENSOR_SIZE), dtype=torch.float32)
        predictions = torch.zeros((1, MODEL_ACTION_TENSOR_SIZE - 1), dtype=torch.float32)
        with self.assertRaises(ValueError):
            policy_loss(predictions, targets)

if __name__ == "__main__":
    unittest.main()
