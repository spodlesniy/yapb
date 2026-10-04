#!/usr/bin/env python3
"""Check whether an AiPB dataset meets caller-defined quality thresholds."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from .dataset_stats import DatasetStats, summarize_dataset


@dataclass(frozen=True)
class DatasetQuality:
    stats: DatasetStats
    failures: tuple[str, ...]

    @property
    def is_valid(self) -> bool:
        return not self.failures


def check_quality(stats: DatasetStats, *, min_samples: int = 0, min_episodes: int = 0,
                  max_dominant_action_share: float | None = None) -> DatasetQuality:
    if min_samples < 0:
        raise ValueError("min_samples must be non-negative")
    if min_episodes < 0:
        raise ValueError("min_episodes must be non-negative")
    if max_dominant_action_share is not None and not 0.0 < max_dominant_action_share <= 1.0:
        raise ValueError("max_dominant_action_share must be in (0, 1]")

    failures: list[str] = []
    if stats.samples < min_samples:
        failures.append(f"samples {stats.samples} < minimum {min_samples}")
    if stats.episodes < min_episodes:
        failures.append(f"episodes {stats.episodes} < minimum {min_episodes}")
    if max_dominant_action_share is not None:
        share = 0.0 if stats.samples == 0 else max(stats.action_counts) / stats.samples
        if share > max_dominant_action_share:
            failures.append(f"dominant action share {share:.4f} > maximum {max_dominant_action_share:.4f}")
    return DatasetQuality(stats=stats, failures=tuple(failures))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Check an AiPB training dataset against explicit quality thresholds.")
    parser.add_argument("dataset", type=Path, help="Path to an aipb-training-jsonl dataset.")
    parser.add_argument("--min-samples", type=int, default=0)
    parser.add_argument("--min-episodes", type=int, default=0)
    parser.add_argument("--max-dominant-action-share", type=float, default=None,
                        help="Fail when one action exceeds this fraction of all samples.")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    quality = check_quality(
        summarize_dataset(args.dataset),
        min_samples=args.min_samples,
        min_episodes=args.min_episodes,
        max_dominant_action_share=args.max_dominant_action_share,
    )
    stats = quality.stats
    print(f"samples={stats.samples}")
    print(f"episodes={stats.episodes}")
    print(f"terminal_samples={stats.terminal_samples}")
    print(f"action_coverage={stats.action_coverage}/{len(stats.action_counts)}")
    if stats.samples:
        action_id = max(range(len(stats.action_counts)), key=stats.action_counts.__getitem__)
        print(f"dominant_action={action_id}")
        print(f"dominant_action_share={stats.action_counts[action_id] / stats.samples:.4f}")
    for failure in quality.failures:
        print(f"quality_failure={failure}")
    print("quality=pass" if quality.is_valid else "quality=fail")
    return 0 if quality.is_valid else 1


if __name__ == "__main__":
    raise SystemExit(main())
