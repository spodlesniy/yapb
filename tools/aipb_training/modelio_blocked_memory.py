"""D339: read-only coverage of locally traced blocked-forward memory decisions.

The engine provides the safe-direction mask and confirms physical completion.
This tool does not prove that the memory policy improves navigation.
"""

from __future__ import annotations

import argparse
import json
import math
from typing import Any

from tools.aipb_training import modelio_cycle_diagnostics as diagnostics

MIN_CONFIRMED_DISTANCE = 24.0


def analyze(trace: dict[str, Any]) -> dict[str, Any]:
    """Report whether a bounded completed trace covers both blocked cases."""
    rows = trace["records"]
    blocked = []
    dual_side = 0
    single_side = 0
    forward_available = 0
    for row in rows:
        mask = row["mask"]
        if mask & 1:
            forward_available += 1
            continue
        distance = math.dist(row["origin"], row["observed"])
        blocked.append(dict(leg=row["leg"], mask=mask,
                            direction=("left" if row["direction"] == 1
                                       else "right"),
                            moved=round(distance, 2),
                            progressed=distance >= MIN_CONFIRMED_DISTANCE))
        if mask == 6:
            dual_side += 1
        elif mask in (2, 4):
            single_side += 1

    gaps = []
    if len(rows) != trace["actions"]:
        gaps.append("incomplete_trace")
    if not forward_available:
        gaps.append("missing_forward_available_control")
    if not dual_side:
        gaps.append("missing_two_side_block")
    if not single_side:
        gaps.append("missing_one_side_block")
    if any(not leg["progressed"] for leg in blocked):
        gaps.append("blocked_leg_without_confirmed_progress")
    return dict(completed=len(rows), requested=trace["actions"],
                forward_available=forward_available,
                blocked_forward=len(blocked), dual_side=dual_side,
                single_side=single_side, blocked_legs=blocked,
                gaps=gaps, coverage_complete=not gaps)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--memory", required=True,
                        help="D337 move-memory JSON trace")
    args = parser.parse_args(argv)
    try:
        report = analyze(diagnostics.load_trace(args.memory, "move-memory"))
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        parser.error(str(exc))
    print(f"completed={report['completed']}/{report['requested']} "
          f"forward_available={report['forward_available']} "
          f"blocked_forward={report['blocked_forward']} "
          f"two_side={report['dual_side']} one_side={report['single_side']}")
    for row in report["blocked_legs"]:
        print(f"leg={row['leg']} mask={row['mask']} "
              f"direction={row['direction']} moved={row['moved']:.2f} "
              f"progressed={int(row['progressed'])}")
    print("missing=" + (",".join(report["gaps"]) if report["gaps"] else "none"))
    print("NOTE: this is blocked-forward movement coverage, not evidence of "
          "policy superiority or general route planning.")
    print("status=" + ("BLOCKED_MEMORY_COVERED" if report["coverage_complete"]
                       else "INSUFFICIENT_BLOCKED_MEMORY_EVIDENCE"))
    return 0 if report["coverage_complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
