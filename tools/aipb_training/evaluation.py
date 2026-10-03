#!/usr/bin/env python3
"""Offline evaluation for an AiPB policy checkpoint."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence

from .dataset import TrainingSample
from .model_contract import MODEL_ACTION_ID_COUNT, ModelOutputIndex
from .policy_model import build_policy_model
from .trainer import create_optimizer
from .training_contract import encode_policy_batch
from .dataset import iter_training_batches
from .training_run import load_checkpoint, split_samples_by_episode


OUTPUT_FIELD_NAMES = (
    "action_id",
    "target_node",
    "target_player",
    "target_position.x",
    "target_position.y",
    "target_position.z",
    "weapon_type",
    "grenade_type",
    "duration",
    "confidence",
)


@dataclass(frozen=True)
class EvaluationMetrics:
    loss: float
    samples: int
    mean_absolute_error: float
    action_id_accuracy: float
    field_mean_absolute_error: tuple[float, ...]


def _torch():
    try:
        import torch
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "PyTorch is required for checkpoint evaluation. "
            "Install tools/aipb_training/requirements.txt."
        ) from exc
    return torch


def _batches(
    samples: Sequence[TrainingSample],
    batch_size: int,
):
    return [
        encode_policy_batch(batch)
        for batch in iter_training_batches(iter(samples), batch_size)
    ]


def evaluate_checkpoint(
    samples: Sequence[TrainingSample],
    checkpoint_path: str | Path,
    split: str = "validation",
    batch_size: int = 64,
    device: str = "cpu",
) -> EvaluationMetrics:
    """Evaluate a checkpoint on the deterministic train or validation split."""
    if batch_size <= 0:
        raise ValueError("batch_size must be positive")
    if split not in ("train", "validation"):
        raise ValueError("split must be 'train' or 'validation'")

    torch = _torch()
    model = build_policy_model().to(device)
    optimizer = create_optimizer(model)
    checkpoint = load_checkpoint(checkpoint_path, model, optimizer, device)

    checkpoint_config = checkpoint.get("config", {})
    validation_split = float(checkpoint_config["validation_split"])
    seed = int(checkpoint_config["seed"])
    train_samples, validation_samples = split_samples_by_episode(
        samples,
        validation_split,
        seed,
    )
    selected_samples = train_samples if split == "train" else validation_samples

    if not selected_samples:
        return EvaluationMetrics(0.0, 0, 0.0, 0.0, (0.0,) * len(OUTPUT_FIELD_NAMES))

    model.eval()
    total_loss = 0.0
    total_absolute_error = 0.0
    field_error_totals = [0.0] * len(OUTPUT_FIELD_NAMES)
    action_id_correct = 0
    sample_count = 0

    with torch.no_grad():
        for batch in _batches(selected_samples, batch_size):
            observations = torch.tensor(batch.observations, dtype=torch.float32, device=device)
            targets = torch.tensor(batch.action_targets, dtype=torch.float32, device=device)
            predictions = model(observations)
            loss = torch.nn.functional.smooth_l1_loss(predictions, targets)

            errors = torch.abs(predictions - targets)
            predicted_action_ids = predictions[:, int(ModelOutputIndex.ACTION_ID)]
            valid_action_ids = (predicted_action_ids >= 0.0) & (predicted_action_ids < MODEL_ACTION_ID_COUNT)
            decoded_action_ids = predicted_action_ids.to(torch.int64)
            expected_action_ids = targets[:, int(ModelOutputIndex.ACTION_ID)].to(torch.int64)
            action_id_correct += int(((decoded_action_ids == expected_action_ids) & valid_action_ids).sum().item())

            count = batch.size
            total_loss += float(loss.item()) * count
            total_absolute_error += float(errors.sum().item())
            field_sums = errors.sum(dim=0).detach().cpu().tolist()
            for index, value in enumerate(field_sums):
                field_error_totals[index] += float(value)
            sample_count += count

    return EvaluationMetrics(
        loss=total_loss / sample_count,
        samples=sample_count,
        mean_absolute_error=total_absolute_error / (sample_count * len(OUTPUT_FIELD_NAMES)),
        action_id_accuracy=action_id_correct / sample_count,
        field_mean_absolute_error=tuple(value / sample_count for value in field_error_totals),
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Evaluate a trained AiPB policy checkpoint on a deterministic dataset split."
    )
    parser.add_argument("dataset", help="Path to an aipb-training-jsonl dataset.")
    parser.add_argument("checkpoint", help="Path to a trained PyTorch checkpoint.")
    parser.add_argument(
        "--split",
        choices=("validation", "train"),
        default="validation",
        help="Dataset split to evaluate (default: validation).",
    )
    parser.add_argument("--batch-size", type=int, default=64)
    parser.add_argument("--device", default="cpu")
    return parser


def main() -> int:
    args = build_parser().parse_args()

    from .dataset import load_training_dataset

    _, samples = load_training_dataset(args.dataset)
    metrics = evaluate_checkpoint(
        samples,
        args.checkpoint,
        split=args.split,
        batch_size=args.batch_size,
        device=args.device,
    )

    print(f"split={args.split}")
    print(f"samples={metrics.samples}")
    print(f"loss={metrics.loss:.6f}")
    print(f"mean_absolute_error={metrics.mean_absolute_error:.6f}")
    print(f"action_id_accuracy={metrics.action_id_accuracy:.6f}")

    for name, error in zip(OUTPUT_FIELD_NAMES, metrics.field_mean_absolute_error):
        print(f"mae.{name}={error:.6f}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
