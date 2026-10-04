#!/usr/bin/env python3
"""Command-line entry point for offline AiPB policy training."""

from __future__ import annotations

import argparse

from .dataset import load_training_dataset
from .dataset_quality import check_quality
from .dataset_stats import summarize_dataset
from .training_run import TrainingConfig, run_training


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Train the AiPB policy model from a JSONL dataset.")
    parser.add_argument("dataset", help="Path to an aipb-training-jsonl dataset.")
    parser.add_argument("--checkpoint-dir", default="tools/aipb_training/checkpoints")
    parser.add_argument("--epochs", type=int, default=10)
    parser.add_argument("--batch-size", type=int, default=64)
    parser.add_argument("--validation-split", type=float, default=0.2)
    parser.add_argument("--seed", type=int, default=1234)
    parser.add_argument("--learning-rate", type=float, default=1e-3)
    parser.add_argument("--weight-decay", type=float, default=1e-4)
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--min-samples", type=int, default=0)
    parser.add_argument("--min-episodes", type=int, default=0)
    parser.add_argument("--max-dominant-action-share", type=float, default=None)
    parser.add_argument(
        "--resume",
        default=None,
        help="Resume from a compatible PyTorch checkpoint. Epochs is the target total.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    _, samples = load_training_dataset(args.dataset)
    quality = check_quality(
        summarize_dataset(args.dataset),
        min_samples=args.min_samples,
        min_episodes=args.min_episodes,
        max_dominant_action_share=args.max_dominant_action_share,
    )
    for failure in quality.failures:
        print(f"quality_failure={failure}")
    if not quality.is_valid:
        return 1

    result = run_training(
        samples,
        TrainingConfig(
            epochs=args.epochs,
            batch_size=args.batch_size,
            validation_split=args.validation_split,
            seed=args.seed,
            learning_rate=args.learning_rate,
            weight_decay=args.weight_decay,
            device=args.device,
        ),
        args.checkpoint_dir,
        resume_from=args.resume,
    )

    for metrics in result.history:
        print(
            f"epoch={metrics.epoch} "
            f"train_loss={metrics.train.loss:.6f} "
            f"train_samples={metrics.train.samples} "
            f"validation_loss={metrics.validation.loss:.6f} "
            f"validation_samples={metrics.validation.samples}"
        )

    print(f"last_checkpoint={result.last_checkpoint_path}")
    if result.best_checkpoint_path is not None:
        print(f"best_checkpoint={result.best_checkpoint_path}")
        print(f"best_epoch={result.best_epoch}")
        print(f"best_validation_loss={result.best_validation_loss:.6f}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
