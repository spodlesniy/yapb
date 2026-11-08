"""D338: bounded matched-start evaluation of repeated local policy trials.

A matched initial pose and mask are observable, not proof of equal later
conditions or causal policy superiority. All physical movement remains HLDS-led.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from typing import Any, Sequence

from tools.aipb_training import modelio_cycle_diagnostics as diag

MIN_PAIRS = 2
MAX_PAIRS = 10
POSITION_TOLERANCE = 1.0
YAW_TOLERANCE = 0.1


def _angle_difference(a: float, b: float) -> float:
    """Smallest circular yaw difference in degrees."""
    return abs((a - b + 180.0) % 360.0 - 180.0)


def pair_gate(avoid: dict[str, Any], memory: dict[str, Any]) -> tuple[bool, str, float, float]:
    """Require equal initial observable conditions and complete bounded trials."""
    a, b = avoid["records"], memory["records"]
    if not a or not b:
        return False, "empty_trace", 0.0, 0.0
    distance = math.dist(a[0]["origin"], b[0]["origin"])
    yaw_delta = _angle_difference(a[0]["yaw"], b[0]["yaw"])
    if avoid["actions"] != memory["actions"]:
        return False, "different_action_limits", distance, yaw_delta
    if len(a) != avoid["actions"] or len(b) != memory["actions"]:
        return False, "incomplete_trial", distance, yaw_delta
    if distance > POSITION_TOLERANCE:
        return False, "start_position_mismatch", distance, yaw_delta
    if yaw_delta > YAW_TOLERANCE:
        return False, "start_yaw_mismatch", distance, yaw_delta
    if a[0]["mask"] != b[0]["mask"]:
        return False, "first_mask_mismatch", distance, yaw_delta
    return True, "matched_start_only", distance, yaw_delta


def _fingerprint(data: dict[str, Any]) -> str:
    """Detect reused trace content; distinct content is not proof of independence."""
    encoded = json.dumps(data, sort_keys=True, separators=(",", ":"),
                         allow_nan=False).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def compare_pairs(pairs: Sequence[tuple[dict[str, Any], dict[str, Any]]]) -> dict[str, Any]:
    """Aggregate only fully recorded matched-start pairs, without causal claims."""
    if not MIN_PAIRS <= len(pairs) <= MAX_PAIRS:
        raise ValueError("pair count must be 2..10")
    seen: dict[str, set[str]] = {"avoid": set(), "memory": set()}
    reports: list[dict[str, Any]] = []
    accepted: list[dict[str, Any]] = []
    baseline = pairs[0][0]["records"][0] if pairs[0][0]["records"] else None
    for index, (avoid, memory) in enumerate(pairs, 1):
        eligible, reason, distance, yaw_delta = pair_gate(avoid, memory)
        if eligible and baseline is not None:
            start = avoid["records"][0]
            if math.dist(start["origin"], baseline["origin"]) > POSITION_TOLERANCE:
                eligible, reason = False, "cross_pair_position_mismatch"
            elif _angle_difference(start["yaw"], baseline["yaw"]) > YAW_TOLERANCE:
                eligible, reason = False, "cross_pair_yaw_mismatch"
            elif start["mask"] != baseline["mask"]:
                eligible, reason = False, "cross_pair_mask_mismatch"
        for name, data in (("avoid", avoid), ("memory", memory)):
            digest = _fingerprint(data)
            if digest in seen[name]:
                eligible, reason = False, "duplicate_" + name + "_trace"
            seen[name].add(digest)
        a, b = diag.summarize(avoid), diag.summarize(memory)
        row = dict(pair=index, eligible=eligible, reason=reason,
                   start_distance=round(distance, 3),
                   yaw_difference=round(yaw_delta, 3),
                   first_mask=(avoid["records"][0]["mask"]
                               if avoid["records"] else None),
                   avoid=a, memory=b)
        reports.append(row)
        if eligible:
            accepted.append(row)

    def mean(metric: str, mode: str) -> float:
        return sum(row[mode][metric] for row in accepted) / len(accepted)

    result: dict[str, Any] = dict(pairs=reports, matched=len(accepted),
                                  total=len(pairs),
                                  complete=len(accepted) == len(pairs))
    if accepted:
        result["mean"] = {
            key: {metric: round(mean(metric, key), 4) for metric in
                  ("efficiency", "revisits", "two_step_loops", "path_length", "displacement")}
            for key in ("avoid", "memory")
        }
        result["delta_efficiency"] = round(
            result["mean"]["memory"]["efficiency"] -
            result["mean"]["avoid"]["efficiency"], 4)
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--avoid", action="append", required=True,
                        help="one D337 avoid JSON trace per pair, in trial order")
    parser.add_argument("--memory", action="append", required=True,
                        help="one D337 memory JSON trace per pair, in trial order")
    args = parser.parse_args(argv)
    if (len(args.avoid) != len(args.memory) or
            not MIN_PAIRS <= len(args.avoid) <= MAX_PAIRS):
        parser.error("provide matching --avoid/--memory file counts (2..10 pairs)")
    try:
        pairs = [(diag.load_trace(a, "move-avoid"),
                  diag.load_trace(m, "move-memory"))
                 for a, m in zip(args.avoid, args.memory)]
        result = compare_pairs(pairs)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        parser.error(str(exc))
    for row in result["pairs"]:
        print(f"pair={row['pair']} eligible={'yes' if row['eligible'] else 'no'} "
              f"reason={row['reason']} start_distance={row['start_distance']:.3f} "
              f"yaw_diff={row['yaw_difference']:.3f} first_mask={row['first_mask']} "
              f"avoid_efficiency={row['avoid']['efficiency']:.4f} "
              f"memory_efficiency={row['memory']['efficiency']:.4f}")
    print(f"matched_pairs={result['matched']}/{result['total']}")
    if "mean" in result:
        for mode in ("avoid", "memory"):
            values = result["mean"][mode]
            print(f"{mode}_mean: efficiency={values['efficiency']:.4f} "
                  f"revisits={values['revisits']:.4f} "
                  f"two_step_loops={values['two_step_loops']:.4f} "
                  f"path={values['path_length']:.2f} "
                  f"displacement={values['displacement']:.2f}")
        print(f"memory_minus_avoid_efficiency={result['delta_efficiency']:+.4f}")
    print("NOTE: matched initial traces do not establish equal later geometry, "
          "mask availability, or causal policy superiority; operator must verify "
          "map and starting conditions for each independent run.")
    if not result["complete"]:
        print("STATUS: INCOMPLETE_MATCHED_START_EVIDENCE")
        return 1
    print("STATUS: MATCHED_START_DESCRIPTIVE_EVIDENCE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
