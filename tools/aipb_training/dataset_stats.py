#!/usr/bin/env python3
"""Summarize action coverage and basic composition of an AiPB dataset."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from .dataset import iter_training_samples
from .model_contract import MODEL_ACTION_ID_COUNT, MODEL_ACTION_ID_NAMES


@dataclass(frozen=True)
class DatasetStats:
    samples: int
    episodes: int
    terminal_samples: int
    action_counts: tuple[int, ...]
    action_episode_counts: tuple[int, ...]

    @property
    def action_coverage(self) -> int:
        return sum(1 for count in self.action_counts if count > 0)


def summarize_dataset(path: str | Path) -> DatasetStats:
    action_counts = [0] * MODEL_ACTION_ID_COUNT
    action_episode_ids: list[set[int]] = [set() for _ in range(MODEL_ACTION_ID_COUNT)]
    episode_ids: set[int] = set()
    samples = 0
    terminal_samples = 0

    for sample in iter_training_samples(path):
        action_id = sample.action.action_id
        action_counts[action_id] += 1
        action_episode_ids[action_id].add(sample.episode_id)
        episode_ids.add(sample.episode_id)
        samples += 1
        if sample.terminal:
            terminal_samples += 1

    return DatasetStats(
        samples=samples,
        episodes=len(episode_ids),
        terminal_samples=terminal_samples,
        action_counts=tuple(action_counts),
        action_episode_counts=tuple(len(episodes) for episodes in action_episode_ids),
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Show action coverage and composition for an AiPB training dataset."
    )
    parser.add_argument("dataset", help="Path to an aipb-training-jsonl dataset.")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    stats = summarize_dataset(args.dataset)

    print(f"samples={stats.samples}")
    print(f"episodes={stats.episodes}")
    print(f"terminal_samples={stats.terminal_samples}")
    print(f"action_coverage={stats.action_coverage}/{MODEL_ACTION_ID_COUNT}")

    for action_id, (name, count) in enumerate(zip(MODEL_ACTION_ID_NAMES, stats.action_counts)):
        percentage = 0.0 if stats.samples == 0 else 100.0 * count / stats.samples
        print(f"action.{action_id}.{name}={count} ({percentage:.2f}%)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
