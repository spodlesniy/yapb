#!/usr/bin/env python3
"""Locate disconnected and one-way navigation components without rebuilding a map."""

from __future__ import annotations

import argparse
import csv
import io
import json
import math
import sys
from collections import deque
from pathlib import Path

from audit_navigation import MAP_NAME, parse_report, required_count


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as source:
        data = json.load(source)

    if not isinstance(data, dict):
        raise ValueError(f"{path.name}: expected JSON object")

    return data


def candidate_id(value: object, field: str) -> int:
    if type(value) not in (int, float) or not math.isfinite(value) or value <= 0 or value != int(value):
        raise ValueError(f"{field}: expected positive candidate ID")

    return int(value)


def walk(start: list[int], graph: list[list[int]]) -> set[int]:
    reached = set(start)
    pending = deque(start)

    while pending:
        current = pending.popleft()

        for neighbor in graph[current]:
            if neighbor not in reached:
                reached.add(neighbor)
                pending.append(neighbor)

    return reached


def components(forward: list[list[int]], reverse: list[list[int]]) -> tuple[list[int], list[list[int]]]:
    count = len(forward)
    visited = [False] * count
    order: list[int] = []

    for root in range(count):
        if visited[root]:
            continue
        visited[root] = True
        frames = [(root, 0)]

        while frames:
            node, offset = frames[-1]

            if offset == len(forward[node]):
                order.append(node)
                frames.pop()
                continue

            frames[-1] = (node, offset + 1)
            target = forward[node][offset]

            if not visited[target]:
                visited[target] = True
                frames.append((target, 0))

    groups = [-1] * count
    members: list[list[int]] = []

    for root in reversed(order):
        if groups[root] != -1:
            continue
        group = len(members)
        pending = [root]
        groups[root] = group
        nodes: list[int] = []

        while pending:
            node = pending.pop()
            nodes.append(node)

            for target in reverse[node]:
                if groups[target] == -1:
                    groups[target] = group
                    pending.append(target)

        members.append(nodes)

    return groups, members


def diagnose(report_path: Path, candidates_path: Path) -> dict:
    connectivity = load_json(report_path)
    source = load_json(candidates_path)
    map_name = connectivity.get("map_name")

    if not isinstance(map_name, str) or not MAP_NAME.fullmatch(map_name):
        raise ValueError("invalid connectivity map_name")

    metrics = parse_report(report_path, map_name)

    if source.get("format") != "aipb-navigation-candidates" or source.get("schema_version") != "0.3.3":
        raise ValueError("unexpected navigation candidates format or schema")

    if source.get("map_name") != map_name:
        raise ValueError("candidates and connectivity map mismatch")

    for field in ("source_sha256", "artifact_sha256"):
        if source.get(field) != connectivity.get(field) or not isinstance(source.get(field), str):
            raise ValueError(f"{field} differs between reports")

    raw_nodes = source.get("candidates")
    raw_spawns = source.get("spawns")

    if not isinstance(raw_nodes, list) or not isinstance(raw_spawns, list) or len(raw_nodes) != metrics.candidates:
        raise ValueError("candidate or spawn arrays disagree with connectivity")

    if required_count(source, "candidate_count") != metrics.candidates:
        raise ValueError("candidate_count differs between reports")

    nodes: list[dict] = []
    indices: dict[int, int] = {}

    for i, candidate in enumerate(raw_nodes):
        if not isinstance(candidate, dict):
            raise ValueError("candidate must be an object")
        node_id = candidate_id(candidate.get("id"), "candidate.id")

        if node_id in indices:
            raise ValueError("duplicate candidate ID")

        xyz = candidate.get("position")

        if not isinstance(xyz, list) or len(xyz) != 3 or any(
            type(value) not in (int, float) or not math.isfinite(value) for value in xyz
        ):
            raise ValueError("candidate position must be a finite XYZ array")

        kind = candidate.get("support_kind")

        if kind not in ("world", "spawn_refinement", "static_brush", "brush_approach"):
            raise ValueError("invalid candidate support kind")

        indices[node_id] = i
        nodes.append({"id": node_id, "position": xyz, "source": kind})

    count = len(nodes)
    forward = [[] for _ in range(count)]
    reverse = [[] for _ in range(count)]
    fixed = [[] for _ in range(count)]

    for edge in connectivity["edges"]:
        if not isinstance(edge, dict):
            raise ValueError("edge must be an object")
        a = candidate_id(edge.get("from"), "edge.from")
        b = candidate_id(edge.get("to"), "edge.to")

        if a not in indices or b not in indices:
            raise ValueError("edge references unknown candidate")

        conditional = edge.get("conditional_support")

        if type(conditional) is not bool:
            raise ValueError("edge conditional_support must be bool")

        ai, bi = indices[a], indices[b]
        forward[ai].append(bi)
        reverse[bi].append(ai)

        if not conditional:
            fixed[ai].append(bi)

    anchors: set[int] = set()

    for spawn in raw_spawns:
        if not isinstance(spawn, dict):
            raise ValueError("spawn must be an object")

        if spawn.get("covered_candidate_id") is None:
            continue

        node_id = candidate_id(spawn["covered_candidate_id"], "spawn.covered_candidate_id")

        if node_id not in indices:
            raise ValueError("spawn anchor references unknown candidate")

        anchors.add(indices[node_id])

    if len(anchors) != metrics.spawn_anchors:
        raise ValueError("spawn anchor count differs from connectivity")

    reachable = walk(sorted(anchors), forward)
    unconditional = walk(sorted(anchors), fixed)
    group_of, groups = components(forward, reverse)
    incoming = [False] * len(groups)
    outgoing = [False] * len(groups)
    exit_to_reachable = [0] * len(groups)

    for index, targets in enumerate(forward):
        for target in targets:
            a, b = group_of[index], group_of[target]

            if a != b:
                incoming[b] = True
                outgoing[a] = True

                if index not in reachable and target in reachable:
                    exit_to_reachable[a] += 1

    sink = {i for i in range(len(groups)) if incoming[i] and not outgoing[i]}
    isolated = {i for i in range(count) if not forward[i] and not reverse[i]}

    if (len(reachable) != metrics.reachable or len(unconditional) != metrics.unconditional_reachable or
        len(groups) != metrics.strong_components or len(sink) != metrics.sink_components or
        len(isolated) != metrics.isolated):
        raise ValueError("detailed graph topology differs from summary counters")

    issues: list[dict] = []
    rows: list[dict] = []

    for group, members in enumerate(groups):
        inaccessible = [i for i in members if i not in reachable]
        conditional_only = [i for i in members if i in reachable and i not in unconditional]
        isolated_members = [i for i in members if i in isolated]

        if not inaccessible and not conditional_only and not isolated_members and group not in sink:
            continue

        ordered = sorted(members, key=lambda index: nodes[index]["id"])
        xs = [nodes[index]["position"][0] for index in members]
        ys = [nodes[index]["position"][1] for index in members]
        zs = [nodes[index]["position"][2] for index in members]
        issues.append({
            "group": group,
            "component": min(nodes[index]["id"] for index in members),
            "size": len(members),
            "unreachable": len(inaccessible),
            "conditional_only": len(conditional_only),
            "isolated": len(isolated_members),
            "sink": group in sink,
            "reverse_exits": exit_to_reachable[group],
            "center": (sum(xs) / len(xs), sum(ys) / len(ys), sum(zs) / len(zs)),
            "bounds": (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)),
            "examples": [nodes[index]["id"] for index in ordered[:8]],
        })

    for index, node in enumerate(nodes):
        group = group_of[index]
        flags = []

        if index not in reachable:
            flags.append("unreachable")
        elif index not in unconditional:
            flags.append("conditional_only")

        if index in isolated:
            flags.append("isolated")

        if group in sink:
            flags.append("one_way_sink")

        rows.append({
            "id": node["id"],
            "x": node["position"][0],
            "y": node["position"][1],
            "z": node["position"][2],
            "status": "+".join(flags) if flags else "reachable",
            "component": min(nodes[n]["id"] for n in groups[group]),
            "incoming_edges": len(reverse[index]),
            "outgoing_edges": len(forward[index]),
            "support_kind": node["source"],
        })

    issues.sort(key=lambda issue: (
        -issue["unreachable"], -int(issue["sink"]), -issue["conditional_only"],
        -issue["isolated"], issue["component"]
    ))
    rows.sort(key=lambda row: row["id"])

    return {"map": map_name, "metrics": metrics, "issues": issues, "nodes": rows}


def render_markdown(result: dict, limit: int) -> str:
    metrics = result["metrics"]
    issues = result["issues"]
    lines = [
        f"# D210.13 Navigation Issues — {result['map']}",
        "",
        "Offline directed-graph findings; not verified player movement.",
        "",
        f"Candidates: {metrics.candidates}; edges: {metrics.edges}; spawn anchors: {metrics.spawn_anchors}.",
        f"Unreachable: {metrics.unreachable}; condition-only: {metrics.conditional_only}; "
        f"isolated: {metrics.isolated}; one-way sink components: {metrics.sink_components}.",
        "",
        f"## Problem components (showing {min(limit, len(issues))} of {len(issues)})",
        "",
        "| Component (min ID) | Nodes | Unreachable | Conditional only | Isolated | Sink | Reverse-only exits | Center XYZ | Example IDs |",
        "| ---: | ---: | ---: | ---: | ---: | ---: | --- | --- | --- |",
    ]

    for issue in issues[:limit]:
        center = ", ".join(f"{coord:.1f}" for coord in issue["center"])
        lines.append(
            f"| {issue['component']} | {issue['size']} | {issue['unreachable']} | "
            f"{issue['conditional_only']} | {issue['isolated']} | "
            f"{'yes' if issue['sink'] else 'no'} | {issue['reverse_exits']} | "
            f"{center} | {', '.join(map(str, issue['examples']))} |"
        )

    lines += [
        "",
        "Component identifiers are the minimum member candidate ID and remain stable when candidate IDs remain stable.",
        "Reverse-only exits count directed edges from unreachable nodes into reachable nodes.",
        "A one-way sink receives edges from other components but has no outgoing edges to them.",
        "See the companion CSV for coordinates and flags for every candidate.",
        "",
    ]

    return "\n".join(lines)


def render_csv(result: dict) -> str:
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=[
        "id", "x", "y", "z", "status", "component",
        "incoming_edges", "outgoing_edges", "support_kind"
    ], lineterminator="\n")
    writer.writeheader()
    writer.writerows(result["nodes"])

    return output.getvalue()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Locate navigation issues from existing JSON reports")
    parser.add_argument("--results", type=Path, required=True, help="Directory with per-map reports")
    parser.add_argument("--map", default="de_dust2", help="Map to inspect (default: de_dust2)")
    parser.add_argument("--output", type=Path, help="Markdown output path")
    parser.add_argument("--csv", type=Path, help="Optional full candidate CSV path")
    parser.add_argument("--limit", type=int, default=30, help="Maximum components in Markdown")
    args = parser.parse_args(argv)

    if not MAP_NAME.fullmatch(args.map) or args.limit < 1:
        parser.error("invalid map name or --limit")

    directory = args.results / args.map

    try:
        result = diagnose(
            directory / "preliminary-connectivity.json",
            directory / "navigation-candidates.json"
        )
        markdown = render_markdown(result, args.limit)
        table = render_csv(result)

        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(markdown, encoding="utf-8", newline="\n")
        else:
            print(markdown, end="")

        if args.csv:
            args.csv.parent.mkdir(parents=True, exist_ok=True)
            args.csv.write_text(table, encoding="utf-8", newline="\n")
    except (OSError, ValueError, UnicodeError, json.JSONDecodeError) as error:
        print(f"Navigation issue diagnostics failed: {error}", file=sys.stderr)
        return 1

    print(f"Navigation issues: {result['map']}, {len(result['issues'])} flagged components", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
