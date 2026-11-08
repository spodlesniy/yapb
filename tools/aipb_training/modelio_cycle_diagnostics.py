"""D337: compare confirmed local Model I/O movement traces.

Only correlated Completed feedback is a movement observation. The engine
remains authoritative for TraceHull safety and physical action completion.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any

SCHEMA = 1
REVISIT_RADIUS = 24.0
MAX_ACTIONS = 8
MAX_RECORDS = 64


class TraceError(ValueError):
    """Malformed or inconsistent bounded trial trace."""


def _position(value: object) -> tuple[float, float, float]:
    if (not isinstance(value, (list, tuple)) or len(value) != 3 or
            any(type(n) not in (int, float) for n in value)):
        raise TraceError("position must contain three numbers")
    pos = tuple(float(n) for n in value)
    if not all(math.isfinite(n) for n in pos):
        raise TraceError("nonfinite position")
    return pos


def _distance(a: tuple[float, float, float],
              b: tuple[float, float, float]) -> float:
    return math.dist(a, b)


def load_trace(path: str | Path, expected_mode: str | None = None) -> dict[str, Any]:
    """Read an explicit local diagnostics file, never an unbounded log."""
    source = Path(path)
    if source.stat().st_size > 65536:
        raise TraceError("trace exceeds size bound")
    with source.open(encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict) or data.get("schema") != SCHEMA:
        raise TraceError("unsupported trace schema")
    mode = data.get("mode")
    if mode not in ("move-avoid", "move-memory") or (
            expected_mode is not None and mode != expected_mode):
        raise TraceError("unexpected policy mode")
    records = data.get("records")
    if not isinstance(records, list) or len(records) > MAX_RECORDS:
        raise TraceError("invalid trace records")
    session: tuple[int, int] | None = None
    for index, item in enumerate(records):
        if not isinstance(item, dict):
            raise TraceError("record is not an object")
        if type(item.get("leg")) is not int or item["leg"] != index + 1:
            raise TraceError("nonsequential completed leg")
        if (type(item.get("session")) is not int or item["session"] <= 0 or
                type(item.get("bot")) is not int or item["bot"] < 0):
            raise TraceError("invalid session/bot identity")
        identity = (item["session"], item["bot"])
        if session is not None and identity != session:
            raise TraceError("trace crosses trial identity")
        session = identity
        if (type(item.get("mask")) is not int or not 1 <= item["mask"] <= 7 or
                type(item.get("direction")) is not int or
                item["direction"] not in (0, 1, 2) or
                not item["mask"] & (1 << item["direction"])):
            raise TraceError("direction not in trace-authorized mask")
        if (type(item.get("yaw")) not in (int, float) or
                not math.isfinite(item["yaw"])):
            raise TraceError("invalid orientation")
        if (type(item.get("decision_frame")) is not int or
                type(item.get("feedback_frame")) is not int or
                not 0 < item["decision_frame"] < item["feedback_frame"]):
            raise TraceError("invalid feedback correlation")
        if index and item["decision_frame"] <= records[index - 1]["feedback_frame"]:
            raise TraceError("nonmonotonic movement frames")
        for key in ("origin", "target", "observed"):
            _position(item.get(key))
    if (type(data.get("actions")) is not int or
            not 4 <= data["actions"] <= MAX_ACTIONS or
            len(records) > data["actions"]):
        raise TraceError("invalid action bound")
    return data


def summarize(trace: dict[str, Any]) -> dict[str, Any]:
    """Count spatial returns in actual confirmed observed positions only."""
    rows = trace["records"]
    if not rows:
        return dict(completed=0, revisits=0, two_step_loops=0,
                    path_length=0.0, displacement=0.0, efficiency=0.0)
    points = [_position(rows[0]["origin"])]
    revisit = loops = 0
    path_length = 0.0
    for item in rows:
        observed = _position(item["observed"])
        path_length += _distance(points[-1], observed)
        # Do not count the just-departed point as an older visit.
        if any(_distance(observed, earlier) <= REVISIT_RADIUS
               for earlier in points[:-1]):
            revisit += 1
        if len(points) >= 2 and _distance(observed, points[-2]) <= REVISIT_RADIUS:
            loops += 1
        points.append(observed)
    displacement = _distance(points[0], points[-1])
    return dict(completed=len(rows), revisits=revisit, two_step_loops=loops,
                path_length=round(path_length, 2),
                displacement=round(displacement, 2),
                efficiency=round(displacement / path_length, 4)
                if path_length > 0 else 0.0)


def same_inputs(avoid: dict[str, Any], memory: dict[str, Any]) -> bool:
    """True only for identical per-leg observed origins, yaw and trace masks."""
    a, b = avoid["records"], memory["records"]
    if len(a) != len(b) or not a:
        return False
    for first, second in zip(a, b):
        if first["mask"] != second["mask"] or (
                abs(first["yaw"] - second["yaw"]) > 0.1 or
                _distance(_position(first["origin"]),
                          _position(second["origin"])) > 1.0):
            return False
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--avoid", required=True, help="D337 avoid JSON trace")
    parser.add_argument("--memory", required=True, help="D337 memory JSON trace")
    args = parser.parse_args(argv)
    try:
        avoid = load_trace(args.avoid, "move-avoid")
        memory = load_trace(args.memory, "move-memory")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        parser.error(str(error))
    controlled = same_inputs(avoid, memory)
    for name, trace in (("avoid", avoid), ("memory", memory)):
        result = summarize(trace)
        print(f"{name}: completed={result['completed']} "
              f"revisits={result['revisits']} "
              f"two_step_loops={result['two_step_loops']} "
              f"path={result['path_length']:.2f} "
              f"displacement={result['displacement']:.2f} "
              f"efficiency={result['efficiency']:.4f}")
    print("identical_inputs=" + ("yes" if controlled else "no"))
    if not controlled:
        print("NOTE: descriptive runs; different positions/masks or action counts "
              "cannot establish causal policy superiority.")
    else:
        print("NOTE: equal logged inputs; actual realized positions may differ.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
