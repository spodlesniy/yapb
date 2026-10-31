"""Conservative target-footprint outcomes for supervised Stage C jump trials.

A user-supplied target describes measured geometry; this tool does not inspect BSP.
"""

from __future__ import annotations

import argparse
import csv
import io
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path

from tools.aipb_training.scenario_jump_profile import (
    LANDING_STABILITY_MS, JumpProfile, measure_jump,
)
from tools.aipb_training.scenario_trace_compare import Bundle, InvalidBundle, Pose, load_bundle

TARGET_VERSION = "aipb-scenario-jump-target-v1"
MAX_TARGET_BYTES = 1024
BOUNDARY_TOLERANCE = 1.0  # GoldSrc units: do not decide on sampled near-edge poses.
NAME = re.compile(r"[A-Za-z0-9_-]{1,63}\Z")
JUMP_TARGET_SCENARIOS = frozenset({
    "jump_box_plain_01", "jump_box_crouch_01", "jump_gap_runup_01",
    "jump_runup_short_01", "jump_runup_long_01",
})


class InvalidTarget(ValueError):
    """Malformed or unmeasured target geometry is never guessed."""


@dataclass(frozen=True)
class JumpTarget:
    scenario: str
    map: str
    min_x: float
    max_x: float
    min_y: float
    max_y: float
    surface_z: float
    z_tolerance: float
    heading: float
    inset: float


@dataclass(frozen=True)
class TargetOutcome:
    outcome: str
    reason: str
    profile: JumpProfile
    landing_x: float = math.nan
    landing_y: float = math.nan
    final_x: float = math.nan
    final_y: float = math.nan
    final_z: float = math.nan
    stable_ms: int = 0


def read_target(path: Path) -> JumpTarget:
    try:
        with path.open("rb") as handle:
            raw = handle.read(MAX_TARGET_BYTES + 1)
    except OSError as exc:
        raise InvalidTarget(f"{path}: {exc}") from exc
    if len(raw) > MAX_TARGET_BYTES:
        raise InvalidTarget(f"{path}: target exceeds {MAX_TARGET_BYTES} bytes")
    try:
        rows = list(csv.reader(io.StringIO(raw.decode("utf-8")), strict=True))
    except (UnicodeError, csv.Error) as exc:
        raise InvalidTarget(f"{path}: malformed CSV: {exc}") from exc
    if len(rows) != 1 or len(rows[0]) != 11:
        raise InvalidTarget(f"{path}: exactly one eleven-field record is required")
    version, scenario, map_name, *values = rows[0]
    if version != TARGET_VERSION or not NAME.fullmatch(scenario) or not NAME.fullmatch(map_name):
        raise InvalidTarget(f"{path}: unsupported target version or identity")
    if scenario not in JUMP_TARGET_SCENARIOS:
        raise InvalidTarget(f"{path}: not a supported obstacle jump scenario")
    try:
        x0, x1, y0, y1, z, z_tol, heading, inset = (float(v) for v in values)
    except ValueError as exc:
        raise InvalidTarget(f"{path}: invalid numeric geometry") from exc
    if not all(math.isfinite(v) and abs(v) <= 16384 for v in (x0, x1, y0, y1, z)):
        raise InvalidTarget(f"{path}: invalid world coordinates")
    if not (2 <= z_tol <= 16 and -180 <= heading < 180 and 0 <= inset <= 32):
        raise InvalidTarget(f"{path}: invalid tolerance, heading, or hull inset")
    if x1 - x0 <= 2 * (inset + BOUNDARY_TOLERANCE) or y1 - y0 <= 2 * (inset + BOUNDARY_TOLERANCE):
        raise InvalidTarget(f"{path}: target support rectangle is too narrow")
    return JumpTarget(scenario, map_name, x0, x1, y0, y1, z, z_tol, heading, inset)


def _footprint(pose: Pose, target: JumpTarget) -> str:
    """Return inside/outside/ambiguous for the conservative standing hull center."""
    d = min(pose.x - (target.min_x + target.inset),
            target.max_x - target.inset - pose.x,
            pose.y - (target.min_y + target.inset),
            target.max_y - target.inset - pose.y)
    if d > BOUNDARY_TOLERANCE:
        return "inside"
    if d < -BOUNDARY_TOLERANCE:
        return "outside"
    return "ambiguous"


def _elevation(pose: Pose, target: JumpTarget) -> str:
    remaining = target.z_tolerance - abs(pose.z - target.surface_z)
    if remaining > BOUNDARY_TOLERANCE:
        return "inside"
    if remaining < -BOUNDARY_TOLERANCE:
        return "outside"
    return "ambiguous"


def _projection(x: float, y: float, target: JumpTarget) -> float:
    angle = math.radians(target.heading)
    return x * math.cos(angle) + y * math.sin(angle)


def _near_far(target: JumpTarget) -> tuple[float, float]:
    corners = tuple(_projection(x, y, target)
                    for x in (target.min_x, target.max_x)
                    for y in (target.min_y, target.max_y))
    return min(corners), max(corners)


def classify_target(run: Bundle, target: JumpTarget) -> TargetOutcome:
    profile = measure_jump(run)

    def result(outcome: str, reason: str, landing: Pose | None = None,
               stable_ms: int = 0) -> TargetOutcome:
        return TargetOutcome(outcome, reason, profile,
                             landing.x if landing else math.nan,
                             landing.y if landing else math.nan,
                             run.end.x, run.end.y, run.end.z, stable_ms)

    if (run.name, run.map) != (target.scenario, target.map) or run.name not in JUMP_TARGET_SCENARIOS:
        return result("Inconclusive", "TargetContextMismatch")
    near, far = _near_far(target)
    if _projection(run.start.x, run.start.y, target) >= near - BOUNDARY_TOLERANCE:
        return result("Inconclusive", "UnverifiedApproach")
    # Only well-validated full command programs can prove spatial outcomes.
    if profile.reason == "NoAirborneTransition" and profile.outcome == "NoTakeoff":
        return result("MissedLanding", "NoTakeoff")
    if profile.reason not in {
        "ObservedStableLanding", "AdditionalFlight", "UnstableLanding", "NoLandingObserved",
    }:
        return result("Inconclusive", profile.reason)
    if run.status != "Completed" or run.source != "live" or len(run.trace) != run.frames:
        return result("Inconclusive", "IncompleteRun")
    if not run.end.ground:
        return result("Inconclusive", "NoStableTerminalGround")

    landings = [i for i, f in enumerate(run.trace)
                if f.before.ground == 0 and f.after.ground == 1]
    if not landings:
        return result("Inconclusive", "NoLandingTransition")
    last_landing = landings[-1]
    landing = run.trace[last_landing].after

    # The last landing must remain grounded through a fully observed suffix.
    suffix = run.trace[last_landing + 1:]
    if any(f.before.ground != 1 or f.after.ground != 1 for f in suffix):
        return result("Inconclusive", "UnstableTerminalContact", landing)
    stable_ms = sum(f.msec for f in suffix)
    if stable_ms < LANDING_STABILITY_MS:
        return result("Inconclusive", "UnstableTerminalContact", landing)
    if run.end.duck:
        return result("Inconclusive", "CrouchedTerminalPose", landing, stable_ms)

    if _projection(run.end.x, run.end.y, target) > far + BOUNDARY_TOLERANCE:
        return result("Overshot", "BeyondFarEdge", landing, stable_ms)

    landing_xy = _footprint(landing, target)
    final_xy = _footprint(run.end, target)
    final_z = _elevation(run.end, target)
    if "ambiguous" in (landing_xy, final_xy, final_z):
        return result("Inconclusive", "TargetBoundaryAmbiguity", landing, stable_ms)

    # Confirm grounded standing support on the target after crouch transitions.
    supported_ms = 0
    for frame in reversed(suffix):
        if frame.before.duck or frame.after.duck:
            break
        if _footprint(frame.before, target) != "inside" or _footprint(frame.after, target) != "inside":
            break
        if _elevation(frame.before, target) != "inside" or _elevation(frame.after, target) != "inside":
            break
        supported_ms += frame.msec

    if landing_xy == "inside" and final_xy == "inside" and final_z == "inside":
        if supported_ms >= LANDING_STABILITY_MS:
            return result("TargetReached", "StableOnDeclaredTarget", landing, stable_ms)
        return result("Inconclusive", "UnverifiedTargetSupport", landing, stable_ms)

    return result("MissedLanding", "StableOutsideTarget", landing, stable_ms)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path, help="Directory with result.csv and trace.csv")
    parser.add_argument("target", type=Path, help="Single-record measured target footprint CSV")
    args = parser.parse_args(argv)

    try:
        target = read_target(args.target)
        outcome = classify_target(load_bundle(args.run), target)
    except (InvalidBundle, InvalidTarget) as exc:
        print(f"InvalidEvidence: {exc}", file=sys.stderr)
        return 2

    print(f"{outcome.outcome}: {outcome.reason}")
    print(f"scenario={target.scenario} map={target.map} "
          f"landing=({outcome.landing_x:.6g},{outcome.landing_y:.6g}) "
          f"terminal=({outcome.final_x:.6g},{outcome.final_y:.6g},{outcome.final_z:.6g}) "
          f"grounded_tail_ms={outcome.stable_ms} "
          f"takeoff_speed={outcome.profile.takeoff_speed:.6g} "
          f"flight_ms={outcome.profile.flight_ms}")
    return {"TargetReached": 0, "Overshot": 1, "MissedLanding": 1,
            "Inconclusive": 3}[outcome.outcome]


if __name__ == "__main__":
    raise SystemExit(main())
