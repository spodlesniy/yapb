#!/usr/bin/env python3
"""Dataset splitting and end-to-end training orchestration for AiPB."""

from __future__ import annotations

import random
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Sequence

from .dataset import TrainingSample, iter_training_batches
from .model_contract import MODEL_ACTION_TENSOR_SIZE, MODEL_FEATURE_COUNT
from .policy_model import POLICY_MODEL_ARCHITECTURE, build_policy_model
from .trainer import TrainingMetrics, create_optimizer, evaluate, train_epoch
from .training_contract import PolicyTrainingBatch, encode_policy_batch


@dataclass(frozen=True)
class TrainingConfig:
    epochs: int = 10
    batch_size: int = 64
    validation_split: float = 0.2
    seed: int = 1234
    learning_rate: float = 1e-3
    weight_decay: float = 1e-4
    device: str = "cpu"

    def validate(self) -> None:
        if self.epochs <= 0:
            raise ValueError("epochs must be positive")
        if self.batch_size <= 0:
            raise ValueError("batch_size must be positive")
        if not 0.0 <= self.validation_split < 1.0:
            raise ValueError("validation_split must be within [0, 1)")
        if self.learning_rate <= 0.0:
            raise ValueError("learning_rate must be positive")
        if self.weight_decay < 0.0:
            raise ValueError("weight_decay must be non-negative")
        if not self.device:
            raise ValueError("device must not be empty")


@dataclass(frozen=True)
class EpochMetrics:
    epoch: int
    train: TrainingMetrics
    validation: TrainingMetrics


@dataclass(frozen=True)
class TrainingRunResult:
    history: tuple[EpochMetrics, ...]
    best_epoch: int
    best_validation_loss: float
    train_samples: int
    validation_samples: int
    last_checkpoint_path: Path
    best_checkpoint_path: Path | None


def split_samples_by_episode(
    samples: Sequence[TrainingSample],
    validation_split: float,
    seed: int,
) -> tuple[tuple[TrainingSample, ...], tuple[TrainingSample, ...]]:
    """Split samples without allowing an episode to cross the validation boundary."""
    if not 0.0 <= validation_split < 1.0:
        raise ValueError("validation_split must be within [0, 1)")

    if not samples:
        raise ValueError("dataset must contain at least one sample")

    episode_to_samples: dict[int, list[TrainingSample]] = {}
    episode_order: list[int] = []

    for sample in samples:
        if sample.episode_id not in episode_to_samples:
            episode_to_samples[sample.episode_id] = []
            episode_order.append(sample.episode_id)
        episode_to_samples[sample.episode_id].append(sample)

    if validation_split == 0.0:
        return tuple(samples), ()

    if len(episode_order) < 2:
        raise ValueError("validation split requires at least two distinct episodes")

    target_validation_samples = max(1, round(len(samples) * validation_split))
    shuffled_episodes = list(episode_order)
    random.Random(seed).shuffle(shuffled_episodes)

    validation_episodes: set[int] = set()
    validation_count = 0

    for episode_id in shuffled_episodes:
        if validation_count >= target_validation_samples:
            break
        validation_episodes.add(episode_id)
        validation_count += len(episode_to_samples[episode_id])

    if len(validation_episodes) == len(episode_order):
        validation_episodes.remove(shuffled_episodes[-1])

    train = tuple(sample for sample in samples if sample.episode_id not in validation_episodes)
    validation = tuple(sample for sample in samples if sample.episode_id in validation_episodes)

    if not train or not validation:
        raise ValueError("validation split must leave samples in both train and validation sets")

    return train, validation


def _policy_batches(
    samples: Sequence[TrainingSample],
    batch_size: int,
    shuffle: bool,
    seed: int,
) -> list[PolicyTrainingBatch]:
    ordered = list(samples)
    if shuffle:
        random.Random(seed).shuffle(ordered)

    return [
        encode_policy_batch(batch)
        for batch in iter_training_batches(iter(ordered), batch_size)
    ]


def _checkpoint_payload(
    model,
    optimizer,
    epoch: int,
    config: TrainingConfig,
    metrics: EpochMetrics,
    best_epoch: int,
    best_validation_loss: float,
) -> dict:
    return {
        "format": "aipb-policy-checkpoint",
        "version": 1,
        "epoch": epoch,
        "best_epoch": best_epoch,
        "best_validation_loss": best_validation_loss,
        "config": asdict(config),
        "model": {
            "feature_count": MODEL_FEATURE_COUNT,
            "action_tensor_size": MODEL_ACTION_TENSOR_SIZE,
            "architecture": asdict(POLICY_MODEL_ARCHITECTURE),
        },
        "model_state": model.state_dict(),
        "optimizer_state": optimizer.state_dict(),
        "metrics": {
            "train_loss": metrics.train.loss,
            "train_samples": metrics.train.samples,
            "validation_loss": metrics.validation.loss,
            "validation_samples": metrics.validation.samples,
        },
    }


def save_checkpoint(
    path: str | Path,
    model,
    optimizer,
    epoch: int,
    config: TrainingConfig,
    metrics: EpochMetrics,
    best_epoch: int,
    best_validation_loss: float,
) -> Path:
    """Save a self-contained training checkpoint."""
    try:
        import torch
    except ModuleNotFoundError as exc:
        raise RuntimeError("PyTorch is required for checkpointing.") from exc

    checkpoint_path = Path(path)
    checkpoint_path.parent.mkdir(parents=True, exist_ok=True)
    torch.save(
        _checkpoint_payload(
            model,
            optimizer,
            epoch,
            config,
            metrics,
            best_epoch,
            best_validation_loss,
        ),
        checkpoint_path,
    )
    return checkpoint_path


def load_checkpoint(
    path: str | Path,
    model,
    optimizer,
    device: str = "cpu",
) -> dict:
    """Restore model and optimizer state from a compatible checkpoint."""
    try:
        import torch
    except ModuleNotFoundError as exc:
        raise RuntimeError("PyTorch is required for checkpointing.") from exc

    checkpoint = torch.load(Path(path), map_location=device, weights_only=False)

    if checkpoint.get("format") != "aipb-policy-checkpoint" or checkpoint.get("version") != 1:
        raise ValueError("unsupported AiPB policy checkpoint format")

    model_contract = checkpoint.get("model", {})
    if model_contract.get("feature_count") != MODEL_FEATURE_COUNT:
        raise ValueError("checkpoint feature count does not match the current model contract")
    if model_contract.get("action_tensor_size") != MODEL_ACTION_TENSOR_SIZE:
        raise ValueError("checkpoint action tensor size does not match the current model contract")
    if model_contract.get("architecture") != asdict(POLICY_MODEL_ARCHITECTURE):
        raise ValueError("checkpoint architecture does not match the current policy model")

    model.load_state_dict(checkpoint["model_state"])
    optimizer.load_state_dict(checkpoint["optimizer_state"])
    return checkpoint


def run_training(
    samples: Sequence[TrainingSample],
    config: TrainingConfig,
    checkpoint_dir: str | Path = "tools/aipb_training/checkpoints",
) -> TrainingRunResult:
    """Run the configured training/validation loop and persist last/best checkpoints."""
    config.validate()

    train_samples, validation_samples = split_samples_by_episode(
        samples,
        config.validation_split,
        config.seed,
    )

    try:
        import torch
    except ModuleNotFoundError as exc:
        raise RuntimeError("PyTorch is required for AiPB training.") from exc

    torch.manual_seed(config.seed)
    model = build_policy_model().to(config.device)
    optimizer = create_optimizer(
        model,
        learning_rate=config.learning_rate,
        weight_decay=config.weight_decay,
    )

    directory = Path(checkpoint_dir)
    directory.mkdir(parents=True, exist_ok=True)
    last_path = directory / "last.pt"
    best_path = directory / "best.pt"

    history: list[EpochMetrics] = []
    best_epoch = 0
    best_validation_loss = float("inf")
    best_checkpoint_path: Path | None = None

    for epoch in range(1, config.epochs + 1):
        train_batches = _policy_batches(
            train_samples,
            config.batch_size,
            shuffle=True,
            seed=config.seed + epoch,
        )
        train_metrics = train_epoch(model, train_batches, optimizer, config.device)

        if validation_samples:
            validation_batches = _policy_batches(
                validation_samples,
                config.batch_size,
                shuffle=False,
                seed=config.seed,
            )
            validation_metrics = evaluate(model, validation_batches, config.device)
        else:
            validation_metrics = TrainingMetrics(0.0, 0)

        epoch_metrics = EpochMetrics(epoch, train_metrics, validation_metrics)
        history.append(epoch_metrics)

        if validation_samples and validation_metrics.loss < best_validation_loss:
            best_validation_loss = validation_metrics.loss
            best_epoch = epoch
            save_checkpoint(
                best_path,
                model,
                optimizer,
                epoch,
                config,
                epoch_metrics,
                best_epoch,
                best_validation_loss,
            )
            best_checkpoint_path = best_path

        save_checkpoint(
            last_path,
            model,
            optimizer,
            epoch,
            config,
            epoch_metrics,
            best_epoch,
            best_validation_loss,
        )

    return TrainingRunResult(
        history=tuple(history),
        best_epoch=best_epoch,
        best_validation_loss=best_validation_loss,
        train_samples=len(train_samples),
        validation_samples=len(validation_samples),
        last_checkpoint_path=last_path,
        best_checkpoint_path=best_checkpoint_path,
    )
