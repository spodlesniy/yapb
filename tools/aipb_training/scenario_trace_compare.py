"""Read-only, conservative comparison of two D281 scenario run bundles."""

from __future__ import annotations

if __name__ == "__main__" and not __package__:
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    __package__ = "tools.aipb_training"

import argparse
import bisect
import csv
import io
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path

TRACE_VERSION = "aipb-scenario-trace-v1"
RESULT_VERSION = "aipb-scenario-result-v1"
MAX_FRAMES = 4096
MAX_TRACE_BYTES = 4 * 1024 * 1024
IDENTIFIER = re.compile(r"[A-Za-z0-9_-]{1,63}\Z")
TRACE_COLUMNS = (
    "frame,action,msec,forward_speed,strafe_speed,max_speed,buttons,old_buttons,"
    "applied_yaw,applied_pitch,"
    "bx,by,bz,bvx,bvy,bvz,byaw,bpitch,bground,bduck,"
    "ax,ay,az,avx,avy,avz,ayaw,apitch,aground,aduck"
).split(",")
POSITION_TOLERANCE = 1.0  # GoldSrc units
VELOCITY_TOLERANCE = 1.0  # units/second
ANGLE_TOLERANCE = 1.0  # degrees
COMMAND_TOLERANCE = 0.01  # units/second, or degrees
MAX_INTERPOLATION_MS = 20  # reject coarse frame intervals when time aligning


class InvalidBundle(ValueError):
    """File is missing, malformed, inconsistent, or uses an unknown schema."""


@dataclass(frozen=True)
class Pose:
    x: float
    y: float
    z: float
    vx: float
    vy: float
    vz: float
    yaw: float
    pitch: float
    ground: int
    duck: int


@dataclass(frozen=True)
class Frame:
    index: int
    action: int
    msec: int
    forward: float
    strafe: float
    max_speed: float
    buttons: int
    old_buttons: int
    yaw: float
    pitch: float
    before: Pose
    after: Pose


@dataclass(frozen=True)
class Bundle:
    name: str
    map: str
    bot: int
    team: int
    round_id: int
    status: str
    reason: str
    frames: int
    command_ms: int
    source: str
    start: Pose
    end: Pose
    trace: tuple[Frame, ...]


@dataclass(frozen=True)
class Comparison:
    outcome: str
    reason: str
    detail: str
    frame: int | None
    end_distance: float
    frames_a: int
    frames_b: int
    command_ms_a: int
    command_ms_b: int
    time_ms: int | None = None
    max_position_delta: float = math.nan
    max_velocity_delta: float = math.nan
    max_angle_delta: float = math.nan


# Parsing uses fixed field counts and bounded input, not assumptions about
# user-controlled CSV paths or a repository-specific working directory.
def _read_csv(path: Path, maximum: int) -> list[list[str]]:
    try:
        with path.open("rb") as handle:
            raw = handle.read(maximum + 1)
    except OSError as exc:
        raise InvalidBundle(f"{path}: {exc}") from exc
    if len(raw) > maximum:
        raise InvalidBundle(f"{path}: file exceeds {maximum} bytes")
    try:
        rows = list(csv.reader(io.StringIO(raw.decode("utf-8")), strict=True))
    except (UnicodeError, csv.Error) as exc:
        raise InvalidBundle(f"{path}: invalid CSV: {exc}") from exc
    if not rows or any(not row for row in rows):
        raise InvalidBundle(f"{path}: missing records or blank lines")
    return rows


def _integer(value: str, name: str, minimum: int, maximum: int) -> int:
    if not re.fullmatch(r"-?[0-9]+", value):
        raise InvalidBundle(f"{name}: invalid integer {value!r}")
    result = int(value)
    if not minimum <= result <= maximum:
        raise InvalidBundle(f"{name}: out of range {result}")
    return result


def _real(value: str, name: str) -> float:
    try:
        number = float(value)
    except ValueError as exc:
        raise InvalidBundle(f"{name}: invalid number {value!r}") from exc
    if not math.isfinite(number):
        raise InvalidBundle(f"{name}: nonfinite number")
    return number


def _pose(row: list[str], offset: int, label: str) -> Pose:
    numbers = [_real(row[offset + i], f"{label}.{i}") for i in range(8)]
    ground = _integer(row[offset + 8], f"{label}.ground", 0, 1)
    duck = _integer(row[offset + 9], f"{label}.duck", 0, 1)
    return Pose(*numbers, ground, duck)


def load_bundle(directory: Path) -> Bundle:
    result_path = directory / "result.csv"
    trace_path = directory / "trace.csv"
    result_rows = _read_csv(result_path, 4096)
    if len(result_rows) != 1 or len(result_rows[0]) != 33:
        raise InvalidBundle(f"{result_path}: expected one 33-field record")
    r = result_rows[0]
    if r[0] != RESULT_VERSION:
        raise InvalidBundle(f"{result_path}: unsupported result version {r[0]!r}")
    if not IDENTIFIER.fullmatch(r[1]) or not IDENTIFIER.fullmatch(r[2]):
        raise InvalidBundle(f"{result_path}: invalid scenario or map identity")
    bot = _integer(r[3], "bot", 1, 32)
    team = _integer(r[4], "team", 1, 2)
    round_id = _integer(r[5], "round_id", 0, 2**32 - 1)
    if r[6] not in {"Completed", "Cancelled", "InvalidScenario", "InvalidClock", "ActionTimeout", "ScenarioTimeout", "FrameLimit"}:
        raise InvalidBundle(f"{result_path}: nonterminal or unknown status {r[6]!r}")
    if not re.fullmatch(r"[A-Za-z0-9 _-]{1,63}", r[7]):
        raise InvalidBundle(f"{result_path}: invalid termination reason")
    frames = _integer(r[8], "frames", 0, MAX_FRAMES)
    command_ms = _integer(r[9], "command_ms", 0, 60000)
    start_game_ms = _integer(r[10], "start_game_ms", 0, 2**63 - 1)
    end_game_ms = _integer(r[11], "end_game_ms", -1, 2**63 - 1)
    if end_game_ms != -1 and end_game_ms < start_game_ms:
        raise InvalidBundle(f"{result_path}: game clock moved backwards")
    if r[12] not in {"live", "last_committed", "missing"}:
        raise InvalidBundle(f"{result_path}: unknown end source")
    start = _pose(r, 13, "result.start")
    end = _pose(r, 23, "result.end")

    trace_rows = _read_csv(trace_path, MAX_TRACE_BYTES)
    if trace_rows[0] != [TRACE_VERSION] or len(trace_rows) < 2 or trace_rows[1] != TRACE_COLUMNS:
        raise InvalidBundle(f"{trace_path}: trace version or columns mismatch")
    if len(trace_rows) - 2 > MAX_FRAMES:
        raise InvalidBundle(f"{trace_path}: trace exceeds frame budget")
    samples: list[Frame] = []
    previous_action = 0
    total_ms = 0
    for index, row in enumerate(trace_rows[2:]):
        if len(row) != len(TRACE_COLUMNS):
            raise InvalidBundle(f"{trace_path}: frame {index} has {len(row)} fields")
        frame_index = _integer(row[0], "frame", 0, MAX_FRAMES - 1)
        action = _integer(row[1], "action", 0, 31)
        msec = _integer(row[2], "msec", 1, 255)
        forward = _real(row[3], "forward_speed")
        strafe = _real(row[4], "strafe_speed")
        max_speed = _real(row[5], "max_speed")
        buttons = _integer(row[6], "buttons", 0, 65535)
        old_buttons = _integer(row[7], "old_buttons", -(2**31), 2**31 - 1)
        yaw = _real(row[8], "applied_yaw")
        pitch = _real(row[9], "applied_pitch")
        before = _pose(row, 10, f"frame[{index}].before")
        after = _pose(row, 20, f"frame[{index}].after")
        if frame_index != index or (index == 0 and action != 0) or (index and action not in {previous_action, previous_action + 1}):
            raise InvalidBundle(f"{trace_path}: noncontiguous frame/action sequence at {index}")
        if max_speed < 0:
            raise InvalidBundle(f"{trace_path}: negative max_speed")
        total_ms += msec
        previous_action = action
        samples.append(Frame(frame_index, action, msec, forward, strafe, max_speed, buttons,
                             old_buttons, yaw, pitch, before, after))
    # The GoldSrc call is recorded before scheduler commit, so a failure can
    # legitimately leave one uncommitted, engine-applied frame in the trace.
    if len(samples) not in {frames, frames + 1} or (len(samples) == frames + 1 and r[6] == "Completed"):
        raise InvalidBundle(f"{trace_path}: frame count inconsistent with result.csv")
    committed_ms = sum(frame.msec for frame in samples[:frames])
    if committed_ms != command_ms or total_ms > command_ms + 255:
        raise InvalidBundle(f"{trace_path}: commanded milliseconds inconsistent with result.csv")
    return Bundle(r[1], r[2], bot, team, round_id, r[6], r[7], frames, command_ms,
                  r[12], start, end, tuple(samples))


def _angle_difference(a: float, b: float) -> float:
    return abs((a - b + 180.0) % 360.0 - 180.0)


def _pose_mismatch(a: Pose, b: Pose, prefix: str) -> str | None:
    for field in ("x", "y", "z"):
        left, right = getattr(a, field), getattr(b, field)
        if abs(left - right) > POSITION_TOLERANCE:
            return f"{prefix}.{field}: {left:.6g} vs {right:.6g} (Δ={abs(left-right):.6g})"
    for field in ("vx", "vy", "vz"):
        left, right = getattr(a, field), getattr(b, field)
        if abs(left - right) > VELOCITY_TOLERANCE:
            return f"{prefix}.{field}: {left:.6g} vs {right:.6g} (Δ={abs(left-right):.6g})"
    for field in ("yaw", "pitch"):
        left, right = getattr(a, field), getattr(b, field)
        if _angle_difference(left, right) > ANGLE_TOLERANCE:
            return f"{prefix}.{field}: {left:.6g} vs {right:.6g} (wrapped Δ={_angle_difference(left,right):.6g})"
    for field in ("ground", "duck"):
        if getattr(a, field) != getattr(b, field):
            return f"{prefix}.{field}: {getattr(a,field)} vs {getattr(b,field)}"
    return None


def _end_distance(a: Pose, b: Pose) -> float:
    return math.dist((a.x, a.y, a.z), (b.x, b.y, b.z))



def _trace_discontinuity(run: Bundle) -> str | None:
    """Check recorded observations before interpolating any physics."""
    if not run.trace:
        return None
    if (reason := _pose_mismatch(run.start, run.trace[0].before, "start")):
        return reason
    for previous, following in zip(run.trace, run.trace[1:]):
        if (reason := _pose_mismatch(previous.after, following.before, "continuity")):
            return f"frames {previous.index}/{following.index}: {reason}"
    return _pose_mismatch(run.trace[-1].after, run.end, "end")


def _time_boundaries(run: Bundle) -> list[int]:
    boundaries = [0]
    for frame in run.trace:
        boundaries.append(boundaries[-1] + frame.msec)
    return boundaries


def _action_timeline(run: Bundle, boundaries: list[int]) -> tuple[tuple[int, int], ...]:
    changes = []
    for i, frame in enumerate(run.trace):
        if i + 1 == len(run.trace) or run.trace[i + 1].action != frame.action:
            changes.append((frame.action, boundaries[i + 1]))
    return tuple(changes)


def _frame_at(run: Bundle, boundaries: list[int], ms: int) -> tuple[int, Frame]:
    index = min(bisect.bisect_right(boundaries, ms) - 1, len(run.trace) - 1)
    return index, run.trace[index]


def _interpolate_angle(start: float, end: float, fraction: float) -> float:
    return start + ((end - start + 180.0) % 360.0 - 180.0) * fraction


def _pose_at_time(run: Bundle, boundaries: list[int], ms: int) -> Pose | None:
    if ms == 0:
        return run.trace[0].before
    index = bisect.bisect_left(boundaries, ms) - 1
    frame = run.trace[index]
    fraction = (ms - boundaries[index]) / frame.msec
    before, after = frame.before, frame.after
    if 0.0 < fraction < 1.0 and (before.ground != after.ground
                                   or before.duck != after.duck):
        return None
    coords = [(getattr(before, field) * (1.0 - fraction)
               + getattr(after, field) * fraction)
              for field in ("x", "y", "z", "vx", "vy", "vz")]
    yaw = _interpolate_angle(before.yaw, after.yaw, fraction)
    pitch = _interpolate_angle(before.pitch, after.pitch, fraction)
    discrete = after if fraction == 1.0 else before
    return Pose(*coords, yaw, pitch, discrete.ground, discrete.duck)


def _command_angle(run: Bundle, boundaries: list[int], ms: int, field: str) -> float:
    index, frame = _frame_at(run, boundaries, ms)
    previous = (getattr(run.trace[index - 1], field) if index
                else getattr(run.trace[0].before, field))
    fraction = min(1.0, (ms - boundaries[index]) / frame.msec)
    return _interpolate_angle(previous, getattr(frame, field), fraction)


def _time_aligned_comparison(
    a: Bundle, b: Bundle, result
) -> Comparison:
    """Use shared commanded-time coordinates, not matching engine frame IDs.

    Linear pose samples are estimates between observations. Do not claim
    reproducibility if the trace is too sparse or a discrete state transition
    occurred between the sampled endpoints.
    """
    ends_a, ends_b = _time_boundaries(a), _time_boundaries(b)
    if (timeline_a := _action_timeline(a, ends_a)) != (timeline_b := _action_timeline(b, ends_b)):
        return result("Diverged", "ActionTimingMismatch",
                      f"action boundaries {timeline_a} vs {timeline_b}")

    if any(frame.msec > MAX_INTERPOLATION_MS for frame in (*a.trace, *b.trace)):
        return result("Inconclusive", "CoarseFrameSampling",
                      f"frame exceeds {MAX_INTERPOLATION_MS} ms interpolation limit")

    max_position = 0.0
    max_velocity = 0.0
    max_angle = 0.0
    for ms in sorted(set(ends_a) | set(ends_b)):
        # Commands are stepwise, with state at a boundary owned by the
        # following action. Unlike a frame-index comparison, old_buttons
        # cannot be matched across different frame partitions.
        if ms < a.command_ms:
            _, cmd_a = _frame_at(a, ends_a, ms)
            _, cmd_b = _frame_at(b, ends_b, ms)
            for field in ("forward", "strafe", "max_speed"):
                left, right = getattr(cmd_a, field), getattr(cmd_b, field)
                if abs(left - right) > COMMAND_TOLERANCE:
                    return result("Diverged", "TimeAlignedCommandMismatch",
                                  f"{field}: {left:.6g} vs {right:.6g}", time_ms=ms)
            if cmd_a.buttons != cmd_b.buttons:
                return result("Diverged", "TimeAlignedCommandMismatch",
                              f"buttons: {cmd_a.buttons} vs {cmd_b.buttons}", time_ms=ms)
        for field in ("yaw", "pitch"):
            left = _command_angle(a, ends_a, ms, field)
            right = _command_angle(b, ends_b, ms, field)
            if _angle_difference(left, right) > ANGLE_TOLERANCE:
                return result("Diverged", "TimeAlignedCommandMismatch",
                              f"{field}: {left:.6g} vs {right:.6g}", time_ms=ms)

        pose_a = _pose_at_time(a, ends_a, ms)
        pose_b = _pose_at_time(b, ends_b, ms)
        if pose_a is None or pose_b is None:
            return result("Inconclusive", "DiscreteTransitionAmbiguity",
                          "ground or crouch transition inside a frame", time_ms=ms)
        if (difference := _pose_mismatch(pose_a, pose_b, "state")):
            return result("Diverged", "TimeAlignedStateMismatch",
                          difference, time_ms=ms)
        max_position = max(max_position, _end_distance(pose_a, pose_b))
        max_velocity = max(max_velocity, math.dist(
            (pose_a.vx, pose_a.vy, pose_a.vz),
            (pose_b.vx, pose_b.vy, pose_b.vz)))
        max_angle = max(max_angle,
                        _angle_difference(pose_a.yaw, pose_b.yaw),
                        _angle_difference(pose_a.pitch, pose_b.pitch))

    return result("Match", "TimeAlignedWithinTolerance",
                  f"aligned {len(set(ends_a) | set(ends_b))} timestamps with "
                  f"linear pose interpolation (max frame {MAX_INTERPOLATION_MS} ms)",
                  max_position_delta=max_position,
                  max_velocity_delta=max_velocity,
                  max_angle_delta=max_angle)


def compare_bundles(a: Bundle, b: Bundle) -> Comparison:
    distance = _end_distance(a.end, b.end) if a.source != "missing" and b.source != "missing" else math.nan

    def result(outcome: str, reason: str, detail: str,
               frame: int | None = None, *, time_ms: int | None = None,
               max_position_delta: float = math.nan,
               max_velocity_delta: float = math.nan,
               max_angle_delta: float = math.nan) -> Comparison:
        return Comparison(outcome, reason, detail, frame, distance,
                          len(a.trace), len(b.trace), a.command_ms, b.command_ms,
                          time_ms, max_position_delta, max_velocity_delta, max_angle_delta)

    if (a.name, a.map, a.bot, a.team) != (b.name, b.map, b.bot, b.team):
        return result("Inconclusive", "ContextMismatch", "scenario/map/bot/team differ")
    if a.status != "Completed" or b.status != "Completed" or a.source != "live" or b.source != "live":
        return result("Inconclusive", "IncompleteRun", f"terminal statuses/sources: {a.status}/{a.source} vs {b.status}/{b.source}")
    if a.frames != len(a.trace) or b.frames != len(b.trace):
        return result("Inconclusive", "UncommittedFrame", "trace includes a frame not committed by the scheduler")
    if not a.trace or not b.trace:
        return result("Inconclusive", "NoFrames", "no applied movement frames available")
    mismatch = _pose_mismatch(a.start, b.start, "start")
    if mismatch:
        return result("Diverged", "StartMismatch", mismatch)
    if a.command_ms != b.command_ms:
        return result("Diverged", "DurationMismatch", f"command_ms {a.command_ms} vs {b.command_ms}")
    for run in (a, b):
        if (discontinuity := _trace_discontinuity(run)):
            return result("Inconclusive", "TraceDiscontinuity", discontinuity)

    # A different GoldSrc frame partition need not mean different physics.
    # Align by accumulated command time, with conservative interpolation limits.
    if len(a.trace) != len(b.trace) or any(
        x.msec != y.msec or x.action != y.action for x, y in zip(a.trace, b.trace)
    ):
        return _time_aligned_comparison(a, b, result)
    for x, y in zip(a.trace, b.trace):
        for field in ("forward", "strafe", "max_speed", "yaw", "pitch"):
            left, right = getattr(x, field), getattr(y, field)
            delta = _angle_difference(left, right) if field in {"yaw", "pitch"} else abs(left - right)
            if delta > COMMAND_TOLERANCE:
                return result("Diverged", "CommandMismatch", f"frame {x.index} {field}: {left:.6g} vs {right:.6g}", x.index)
        for field in ("buttons", "old_buttons"):
            if getattr(x, field) != getattr(y, field):
                return result("Diverged", "CommandMismatch", f"frame {x.index} {field}: {getattr(x, field)} vs {getattr(y, field)}", x.index)
        for phase in ("before", "after"):
            mismatch = _pose_mismatch(getattr(x, phase), getattr(y, phase), phase)
            if mismatch:
                return result("Diverged", "StateMismatch", f"frame {x.index} {mismatch}", x.index)
    mismatch = _pose_mismatch(a.end, b.end, "end")
    if mismatch:
        return result("Diverged", "EndMismatch", mismatch)
    return result("Match", "WithinTolerance", "same command partition and observed states within declared tolerances")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_a", type=Path, help="First attempt directory (result.csv + trace.csv)")
    parser.add_argument("run_b", type=Path, help="Second attempt directory (result.csv + trace.csv)")
    args = parser.parse_args(argv)
    try:
        result = compare_bundles(load_bundle(args.run_a), load_bundle(args.run_b))
    except InvalidBundle as exc:
        print(f"InvalidBundle: {exc}", file=sys.stderr)
        return 2
    print(f"{result.outcome}: {result.reason}: {result.detail}")
    print(f"frames={result.frames_a}/{result.frames_b} command_ms={result.command_ms_a}/{result.command_ms_b} "
          f"end_position_delta={result.end_distance:.6g}" +
          (f" first_mismatch_frame={result.frame}" if result.frame is not None else "") +
          (f" first_mismatch_time_ms={result.time_ms}" if result.time_ms is not None else ""))
    if not math.isnan(result.max_position_delta):
        print(f"time_aligned_max_position_delta={result.max_position_delta:.6g} "
              f"max_velocity_delta={result.max_velocity_delta:.6g} "
              f"max_angle_delta={result.max_angle_delta:.6g}")
    return {"Match": 0, "Diverged": 1, "Inconclusive": 3}[result.outcome]


if __name__ == "__main__":
    raise SystemExit(main())
