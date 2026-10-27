#!/usr/bin/env python3
"""Read-only D210 map-connectivity audit across imported BSP artifacts."""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path


SCHEMA = "0.9.0"
MAP_NAME = re.compile(r"^[A-Za-z0-9_-]+$")


@dataclass(frozen=True)
class MapMetrics:
    name: str
    candidates: int
    edges: int
    spawn_anchors: int
    reachable: int
    unconditional_reachable: int
    unreachable: int
    isolated: int
    strong_components: int
    weak_components: int
    sink_components: int
    conditional_edges: int
    jump_edges: int
    crouch_jump_edges: int
    drop_edges: int
    door_edges: int

    @property
    def conditional_only(self) -> int:
        return self.reachable - self.unconditional_reachable

    @property
    def priority(self) -> tuple[float, float, int, int, str]:
        denominator = max(self.candidates, 1)
        return (
            -self.unreachable / denominator,
            -self.conditional_only / denominator,
            -self.sink_components,
            -self.isolated,
            self.name,
        )


def required_count(data: dict, key: str) -> int:
    value = data.get(key)

    if type(value) not in (int, float) or not math.isfinite(value) or value < 0 or value != int(value):
        raise ValueError(f"{key} must be a nonnegative integer")

    return int(value)


def parse_report(path: Path, expected_name: str) -> MapMetrics:
    with path.open("r", encoding="utf-8") as stream:
        data = json.load(stream)

    if not isinstance(data, dict):
        raise ValueError("report root must be an object")

    if data.get("format") != "aipb-preliminary-connectivity":
        raise ValueError("unexpected connectivity format")

    if data.get("schema_version") != SCHEMA:
        raise ValueError(f"expected connectivity schema {SCHEMA}")

    if data.get("map_name") != expected_name:
        raise ValueError("report belongs to a different map")

    edges = data.get("edges")

    if not isinstance(edges, list):
        raise ValueError("edges must be an array")

    values = {key: required_count(data, key) for key in (
        "candidate_count",
        "edge_count",
        "spawn_anchors",
        "candidates_reachable_from_spawns",
        "candidates_reachable_unconditionally",
        "candidates_unreachable_from_spawns",
        "isolated_candidates",
        "strong_components",
        "unconditional_strong_components",
        "weak_components",
        "largest_strong_component",
        "one_way_sink_components",
        "one_way_sink_candidates",
        "conditional_edges",
        "conditional_door_edges",
        "jump_edges",
        "crouch_jump_edges",
        "drop_edges",
    )}

    def n(key: str) -> int:
        return values[key]

    count = n("candidate_count")
    reachable = n("candidates_reachable_from_spawns")
    fixed = n("candidates_reachable_unconditionally")

    if n("edge_count") != len(edges):
        raise ValueError("edge_count does not match edges length")

    if not (fixed <= reachable <= count and
            n("candidates_unreachable_from_spawns") == count - reachable and
            n("spawn_anchors") <= count and
            n("isolated_candidates") <= count and
            n("weak_components") <= n("strong_components") <=
            n("unconditional_strong_components") <= count and
            n("largest_strong_component") <= count and
            n("one_way_sink_components") <= n("strong_components") and
            n("one_way_sink_candidates") <= count and
            n("conditional_edges") <= n("edge_count") and
            n("conditional_door_edges") <= n("conditional_edges") and
            n("jump_edges") + n("crouch_jump_edges") + n("drop_edges") <= n("edge_count")):
        raise ValueError("inconsistent connectivity counters")

    if count == 0 and n("edge_count") != 0:
        raise ValueError("empty graph contains edges")

    return MapMetrics(
        name=expected_name,
        candidates=count,
        edges=n("edge_count"),
        spawn_anchors=n("spawn_anchors"),
        reachable=reachable,
        unconditional_reachable=fixed,
        unreachable=count - reachable,
        isolated=n("isolated_candidates"),
        strong_components=n("strong_components"),
        weak_components=n("weak_components"),
        sink_components=n("one_way_sink_components"),
        conditional_edges=n("conditional_edges"),
        jump_edges=n("jump_edges"),
        crouch_jump_edges=n("crouch_jump_edges"),
        drop_edges=n("drop_edges"),
        door_edges=n("conditional_door_edges"),
    )


def inspect_results(root: Path, map_name: str | None = None) -> tuple[list[MapMetrics], list[str]]:
    if map_name is not None and not MAP_NAME.fullmatch(map_name):
        raise ValueError("invalid map name")

    if not root.is_dir():
        return [], [f"Results directory does not exist: {root}"]

    reports: list[MapMetrics] = []
    errors: list[str] = []
    imported = 0

    directories = [root / map_name] if map_name is not None else sorted(root.iterdir())

    for directory in directories:
        if not directory.is_dir() or not MAP_NAME.fullmatch(directory.name):
            continue

        manifest = directory / (directory.name + ".aipbmap") / "manifest.json"

        if not manifest.is_file():
            continue

        imported += 1
        report = directory / "preliminary-connectivity.json"

        if not report.is_file():
            errors.append(f"{directory.name}: missing preliminary-connectivity.json")
            continue

        try:
            reports.append(parse_report(report, directory.name))
        except (ValueError, UnicodeError, json.JSONDecodeError, OSError) as error:
            errors.append(f"{directory.name}: {error}")

    if imported == 0:
        errors.append(f"No imported map artifact found: {map_name}" if map_name else
                      "No imported map artifacts found")

    return sorted(reports, key=lambda item: item.priority), errors


def render_audit(reports: list[MapMetrics], errors: list[str]) -> str:
    lines = [
        "# D210 Navigation Readiness Audit",
        "",
        "Offline connectivity hypotheses only; not a GoldSrc movement acceptance test.",
        "",
        f"Imported reports parsed: {len(reports)}",
        f"Missing or invalid reports: {len(errors)}",
        "",
        "## Priority review (highest disconnected fraction first)",
        "",
        "| Map | Nodes | Edges | Spawn anchors | Reachable | Unconditional | Unreachable | Isolated | SCC | One-way sinks | Conditional |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]

    for entry in reports:
        lines.append(
            f"| {entry.name} | {entry.candidates} | {entry.edges} | "
            f"{entry.spawn_anchors} | {entry.reachable} | "
            f"{entry.unconditional_reachable} | {entry.unreachable} | "
            f"{entry.isolated} | {entry.strong_components} | "
            f"{entry.sink_components} | {entry.conditional_edges} |"
        )

    lines += ["", "## Findings", ""]
    findings = []

    for entry in reports:
        reasons = []

        if entry.spawn_anchors == 0:
            reasons.append("no covered spawn anchors")

        if entry.unreachable:
            reasons.append(f"{entry.unreachable} not reachable from spawns")

        if entry.conditional_only:
            reasons.append(f"{entry.conditional_only} reachable only via conditional edges")

        if entry.sink_components:
            reasons.append(f"{entry.sink_components} one-way sink components")

        if entry.isolated:
            reasons.append(f"{entry.isolated} isolated candidates")

        if reasons:
            findings.append(f"- **{entry.name}**: " + "; ".join(reasons))

    lines.extend(findings or ["No flagged graph-coverage issues in the parsed reports."])
    lines += ["", "## Missing or invalid reports", ""]
    lines.extend(f"- {error}" for error in errors)
    if not errors:
        lines.append("None.")

    lines += [
        "",
        "## Interpretation",
        "",
        "Unreachable means a directed graph path from a covered spawn is absent; it is not proof of an unusable map.",
        "Unconditional reachability excludes all links marked conditional, including dynamic doors and conditional support.",
        "One-way sinks are strongly connected components with incoming, but no outgoing, inter-component edges.",
        "No fixed percentage is an automatic pass/fail criterion for provisional navigation.",
        "Review maps in table order, inspect the relevant candidates/links, then validate actual movement in-game.",
        "",
    ]

    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Audit preliminary connectivity for imported maps")
    parser.add_argument("--results", type=Path, required=True, help="Directory containing per-map results")
    parser.add_argument("--output", type=Path, help="Markdown report path (stdout when omitted)")
    parser.add_argument("--map", help="Audit only one named map, e.g. de_dust2")
    arguments = parser.parse_args(argv)

    if arguments.map is not None and not MAP_NAME.fullmatch(arguments.map):
        parser.error("--map must be a simple map name")

    reports, errors = inspect_results(arguments.results, arguments.map)
    text = render_audit(reports, errors)

    if arguments.output:
        try:
            arguments.output.parent.mkdir(parents=True, exist_ok=True)
            arguments.output.write_text(text, encoding="utf-8", newline="\n")
        except OSError as error:
            print(f"Unable to write audit report: {error}", file=sys.stderr)
            return 1

        print(f"Navigation audit: {len(reports)} reports, {len(errors)} errors; {arguments.output}")
    else:
        print(text, end="")

    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
