#!/usr/bin/env python3
"""Unit tests for AiPB training orchestration."""

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

from ..dataset import TrainingAction, TrainingObservation, TrainingSample
from ..model_contract import MODEL_FEATURE_COUNT, MODEL_FEATURE_SCHEMA_VERSION
from ..training_run import (
    TrainingConfig,
    load_checkpoint,
    run_training,
    split_samples_by_episode,
)

TORCH_AVAILABLE = importlib.util.find_spec("torch") is not None


def assert_optimizer_states_equal(test_case: unittest.TestCase, first: dict, second: dict) -> None:
    test_case.assertEqual(first["param_groups"], second["param_groups"])
    test_case.assertEqual(first["state"].keys(), second["state"].keys())

    import torch

    for parameter_id in first["state"]:
        first_state = first["state"][parameter_id]
        second_state = second["state"][parameter_id]
        test_case.assertEqual(first_state.keys(), second_state.keys())
        for key in first_state:
            first_value = first_state[key]
            second_value = second_state[key]
            if torch.is_tensor(first_value):
                test_case.assertTrue(torch.equal(first_value, second_value))
            else:
                test_case.assertEqual(first_value, second_value)


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
        terminal=index == 2,
    )


def make_samples() -> tuple[TrainingSample, ...]:
    return tuple(
        make_sample(episode_id, index)
        for episode_id in range(1, 5)
        for index in range(3)
    )


class TrainingConfigTests(unittest.TestCase):
    def test_defaults_are_valid(self) -> None:
        TrainingConfig().validate()

    def test_invalid_configuration_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            TrainingConfig(epochs=0).validate()
        with self.assertRaises(ValueError):
            TrainingConfig(validation_split=1.0).validate()
        with self.assertRaises(ValueError):
            TrainingConfig(device="").validate()


class TrainingSplitTests(unittest.TestCase):
    def test_split_is_deterministic_and_keeps_episodes_together(self) -> None:
        samples = make_samples()

        first_train, first_validation = split_samples_by_episode(samples, 0.25, 1234)
        second_train, second_validation = split_samples_by_episode(samples, 0.25, 1234)

        self.assertEqual(first_train, second_train)
        self.assertEqual(first_validation, second_validation)

        train_episodes = {sample.episode_id for sample in first_train}
        validation_episodes = {sample.episode_id for sample in first_validation}

        self.assertTrue(train_episodes.isdisjoint(validation_episodes))
        self.assertEqual(len(first_train) + len(first_validation), len(samples))

    def test_zero_validation_split_keeps_everything_in_training(self) -> None:
        samples = tuple(make_sample(1, index) for index in range(3))

        train, validation = split_samples_by_episode(samples, 0.0, 1234)

        self.assertEqual(train, samples)
        self.assertEqual(validation, ())

    def test_validation_split_requires_multiple_episodes(self) -> None:
        samples = tuple(make_sample(1, index) for index in range(3))

        with self.assertRaises(ValueError):
            split_samples_by_episode(samples, 0.2, 1234)


@unittest.skipUnless(TORCH_AVAILABLE, "PyTorch is not installed in the lightweight test environment")
class TrainingRunTests(unittest.TestCase):
    def test_training_run_creates_last_and_best_checkpoints(self) -> None:
        import tempfile

        samples = make_samples()

        with tempfile.TemporaryDirectory() as directory:
            result = run_training(
                samples,
                TrainingConfig(epochs=2, batch_size=4, validation_split=0.25, seed=7),
                Path(directory),
            )

            self.assertEqual(len(result.history), 2)
            self.assertEqual(result.start_epoch, 1)
            self.assertEqual(result.train_samples + result.validation_samples, 12)
            self.assertGreaterEqual(result.best_epoch, 1)
            self.assertTrue(result.last_checkpoint_path.is_file())
            self.assertIsNotNone(result.best_checkpoint_path)
            self.assertTrue(result.best_checkpoint_path.is_file())

    def test_resume_matches_uninterrupted_training(self) -> None:
        import tempfile
        import torch

        samples = make_samples()
        one_epoch = TrainingConfig(epochs=1, batch_size=4, validation_split=0.25, seed=7)
        two_epochs = TrainingConfig(epochs=2, batch_size=4, validation_split=0.25, seed=7)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            uninterrupted_dir = root / "uninterrupted"
            resumed_dir = root / "resumed"

            run_training(samples, two_epochs, uninterrupted_dir)
            first = run_training(samples, one_epoch, resumed_dir)

            resumed = run_training(
                samples,
                two_epochs,
                resumed_dir,
                resume_from=first.last_checkpoint_path,
            )

            uninterrupted_checkpoint = torch.load(
                uninterrupted_dir / "last.pt",
                weights_only=True,
            )
            resumed_checkpoint = torch.load(
                resumed.last_checkpoint_path,
                weights_only=True,
            )

            self.assertEqual(resumed.start_epoch, 2)
            self.assertEqual(len(resumed.history), 2)

            for name, value in uninterrupted_checkpoint["model_state"].items():
                self.assertTrue(torch.equal(value, resumed_checkpoint["model_state"][name]))

            assert_optimizer_states_equal(
                self,
                uninterrupted_checkpoint["optimizer_state"],
                resumed_checkpoint["optimizer_state"],
            )

    def test_resume_rejects_changed_training_parameters(self) -> None:
        import tempfile

        samples = make_samples()

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = run_training(
                samples,
                TrainingConfig(epochs=1, batch_size=4, validation_split=0.25, seed=7),
                root,
            )

            with self.assertRaises(ValueError):
                run_training(
                    samples,
                    TrainingConfig(epochs=2, batch_size=8, validation_split=0.25, seed=7),
                    root,
                    resume_from=first.last_checkpoint_path,
                )

    def test_load_checkpoint_restores_model_and_optimizer(self) -> None:
        import tempfile
        import torch

        from ..policy_model import build_policy_model
        from ..trainer import create_optimizer

        samples = make_samples()

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = run_training(
                samples,
                TrainingConfig(epochs=1, batch_size=4, validation_split=0.25, seed=7),
                root,
            )

            model = build_policy_model()
            optimizer = create_optimizer(model)
            checkpoint = load_checkpoint(result.last_checkpoint_path, model, optimizer)

            self.assertEqual(checkpoint["epoch"], 1)
            self.assertEqual(checkpoint["format"], "aipb-policy-checkpoint")
            self.assertIn("model_state", checkpoint)
            self.assertIn("optimizer_state", checkpoint)
            self.assertEqual(checkpoint["model"]["feature_count"], MODEL_FEATURE_COUNT)
            self.assertEqual(checkpoint["model"]["feature_schema_version"], MODEL_FEATURE_SCHEMA_VERSION)
            self.assertTrue(all(torch.isfinite(p).all().item() for p in model.parameters()))


if __name__ == "__main__":
    unittest.main()
