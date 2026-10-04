#!/usr/bin/env python3
"""Check whether an AiPB dataset meets caller-defined quality thresholds."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from .dataset_stats import DatasetStats, summarize_dataset
from .model_contract import MODEL_ACTION_ID_COUNT, MODEL_ACTION_ID_NAMES


@dataclass(frozen=True)
class DatasetQuality:
    stats: DatasetStats
    failures: tuple[str, ...]

    @property
    def is_valid(self) -> bool:
        return not self.failures


def parse_action_requirement(value: str) -> tuple[int, int]:
    try:
        action_id_text, minimum_text = value.split(":", 1)
        action_id = int(action_id_text)
        minimum = int(minimum_text)
    except ValueError as exc:
        raise ValueError("action requirement must use ID:COUNT syntax") from exc

    if not 0 <= action_id < MODEL_ACTION_ID_COUNT:
        raise ValueError(f"action ID must be within [0, {MODEL_ACTION_ID_COUNT})")
    if minimum < 0:
        raise ValueError("action minimum must be non-negative")
    return action_id, minimum


def check_quality(stats: DatasetStats, *, min_samples: int = 0, min_episodes: int = 0,
                  max_dominant_action_share: float | None = None,
                  min_action_samples: tuple[tuple[int, int], ...] = ()) -> DatasetQuality:
    if min_samples < 0:
        raise ValueError("min_samples must be non-negative")
    if min_episodes < 0:
        raise ValueError("min_episodes must be non-negative")
    if max_dominant_action_share is not None and not 0.0 < max_dominant_action_share <= 1.0:
        raise ValueError("max_dominant_action_share must be in (0, 1]")

    seen_actions: set[int] = set()
    for action_id, minimum in min_action_samples:
        if not 0 <= action_id < MODEL_ACTION_ID_COUNT:
            raise ValueError(f"action ID must be within [0, {MODEL_ACTION_ID_COUNT})")
        if minimum < 0:
            raise ValueError("action minimum must be non-negative")
        if action_id in seen_actions:
            raise ValueError(f"duplicate action minimum for action {action_id}")
        seen_actions.add(action_id)

    failures: list[str] = []
    if stats.samples < min_samples:
        failures.append(f"samples {stats.samples} < minimum {min_samples}")
    if stats.episodes < min_episodes:
        failures.append(f"episodes {stats.episodes} < minimum {min_episodes}")
    if max_dominant_action_share is not None:
        share = 0.0 if stats.samples == 0 else max(stats.action_counts) / stats.samples
        if share > max_dominant_action_share:
            failures.append(f"dominant action share {share:.4f} > maximum {max_dominant_action_share:.4f}")

    for action_id, minimum in min_action_samples:
        actual = stats.action_counts[action_id]
        if actual < minimum:
            failures.append(f"action {action_id}.{MODEL_ACTION_ID_NAMES[action_id]} samples {actual} < minimum {minimum}")

    return DatasetQuality(stats=stats, failures=tuple(failures))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Check an AiPB training dataset against explicit quality thresholds.")
    parser.add_argument("dataset", type=Path, help="Path to an aipb-training-jsonl dataset.")
    parser.add_argument("--min-samples", type=int, default=0)
    parser.add_argument("--min-episodes", type=int, default=0)
    parser.add_argument("--max-dominant-action-share", type=float, default=None,
                        help="Fail when one action exceeds this fraction of all samples.")
    parser.add_argument(
        "--min-action-samples",
        action="append",
        default=[],
        metavar="ID:COUNT",
        help="Require at least COUNT samples for action ID. May be repeated.",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        min_action_samples = tuple(parse_action_requirement(value) for value in args.min_action_samples)
    except ValueError as exc:
        parser.error(str(exc))

    quality = check_quality(
        summarize_dataset(args.dataset),
        min_samples=args.min_samples,
        min_episodes=args.min_episodes,
        max_dominant_action_share=args.max_dominant_action_share,
        min_action_samples=min_action_samples,
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
