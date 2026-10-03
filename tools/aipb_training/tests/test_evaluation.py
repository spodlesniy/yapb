#!/usr/bin/env python3
"""Unit tests for AiPB checkpoint evaluation."""

from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

from ..dataset import TrainingAction, TrainingObservation, TrainingSample
from ..evaluation import (
    OUTPUT_FIELD_NAMES,
    build_parser,
    evaluate_checkpoint,
)
from ..model_contract import MODEL_FEATURE_COUNT
from ..training_run import TrainingConfig, run_training


TORCH_AVAILABLE = importlib.util.find_spec("torch") is not None


def make_sample(episode_id: int, index: int) -> TrainingSample:
    values = (float(index),) * MODEL_FEATURE_COUNT
    return TrainingSample(
        episode_id=episode_id,
        observation=TrainingObservation(values),
        action=TrainingAction(1, 42, -1, (1.0, 2.0, 3.0), 1, 0, 0.8, 0.8),
        reward=1.0,
        next_observation=TrainingObservation(values),
        result=2,
        elapsed_time=0.5,
        terminal=index == 1,
    )


def make_samples() -> tuple[TrainingSample, ...]:
    return tuple(
        make_sample(episode_id, index)
        for episode_id in range(1, 5)
        for index in range(2)
    )


class EvaluationParserTests(unittest.TestCase):
    def test_defaults_to_validation_split(self) -> None:
        args = build_parser().parse_args(["dataset.jsonl", "checkpoint.pt"])

        self.assertEqual(args.split, "validation")
        self.assertEqual(args.batch_size, 64)
        self.assertEqual(args.device, "cpu")


@unittest.skipUnless(TORCH_AVAILABLE, "PyTorch is not installed in the lightweight test environment")
class EvaluationTests(unittest.TestCase):
    def test_validation_evaluation_uses_checkpoint_split(self) -> None:
        samples = make_samples()

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = run_training(
                samples,
                TrainingConfig(epochs=1, batch_size=2, validation_split=0.25, seed=7),
                root,
            )

            metrics = evaluate_checkpoint(
                samples,
                result.last_checkpoint_path,
                split="validation",
                batch_size=2,
            )

            self.assertEqual(metrics.samples, result.validation_samples)
            self.assertEqual(len(metrics.field_mean_absolute_error), len(OUTPUT_FIELD_NAMES))
            self.assertGreaterEqual(metrics.loss, 0.0)
            self.assertGreaterEqual(metrics.mean_absolute_error, 0.0)
        self.assertGreaterEqual(metrics.action_id_accuracy, 0.0)
        self.assertLessEqual(metrics.action_id_accuracy, 1.0)

    def test_invalid_split_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            evaluate_checkpoint(
                make_samples(),
                "missing.pt",
                split="invalid",
            )


if __name__ == "__main__":
    unittest.main()

    def test_action_id_metric_matches_runtime_truncation(self) -> None:
        samples = make_samples()

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = run_training(
                samples,
                TrainingConfig(epochs=1, batch_size=2, validation_split=0.25, seed=7),
                root,
            )

            metrics = evaluate_checkpoint(
                samples,
                result.last_checkpoint_path,
                split="validation",
                batch_size=2,
            )

            self.assertGreaterEqual(metrics.action_id_accuracy, 0.0)
            self.assertLessEqual(metrics.action_id_accuracy, 1.0)
