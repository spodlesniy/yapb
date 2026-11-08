"""Local, deterministic Model I/O wire-v1/v2 server for standalone integration tests.

This is not a trained model, a public network service, or an HLDS plugin.
"""

from __future__ import annotations

import argparse
import math
import signal
import socket
import struct
import threading
import time
from dataclasses import dataclass, field
from typing import Sequence

HOST = "127.0.0.1"
HEADER = struct.Struct("<4sHHI")
MAGIC = b"AIPB"
WIRE_VERSION = 1
AFFORDANCE_WIRE_VERSION = 2
REQUEST_KIND = 1
RESPONSE_KIND = 2
MODEL_SCHEMA = 1
STAMP_SCHEMA = 1
FEATURE_SCHEMA = 7
ACTION_SCHEMA = 2
FEATURE_COUNT = 252
MAX_MESSAGE = 2048
MAX_EVENTS = 4
MAX_CONNECTIONS = 64
SESSION_MIN_ACTIONS = 4
SESSION_MAX_ACTIONS = 8
MEMORY_HISTORY_CAPACITY = 4
MEMORY_STEP_DISTANCE = 96.0
READ_TIMEOUT = 0.5
ACCEPT_POLL_INTERVAL = 0.2
LISTENER_IDLE_TIMEOUT = 30.0
FAULT_SLEEP_SECONDS = 0.1  # Exceeds D330's 20-ms external deadline.
FAULT_CHOICES = ("none", "disconnect", "timeout", "truncated",
                 "bad-magic", "bad-version", "oversized",
                 "foreign-request", "foreign-session", "foreign-frame",
                 "nonfinite", "unsafe-target", "unsafe-action",
                 "no-decision", "duplicate-move")


class ProtocolError(ValueError):
    """Reject a malformed request without returning a model decision."""


@dataclass(frozen=True)
class Request:
    request_id: int
    session_id: int
    frame_id: int
    bot_index: int
    origin: tuple[float, float, float]
    yaw: float
    alive: bool
    on_ground: bool
    feedback_decision: int | None = None
    feedback_observed: int = 0
    feedback_completed: bool = False
    wire_version: int = WIRE_VERSION
    safe_directions_mask: int = 0


@dataclass
class SequenceState:
    completed: int = 0
    pending_frame: int = 0
    last_frame: int = 0
    # Stored only after correlated Completed feedback, never on proposal.
    visited: list[tuple[float, float, float]] = field(default_factory=list)
    pending_origin: tuple[float, float, float] | None = None
    pending_target: tuple[float, float, float] | None = None


class Reader:
    def __init__(self, data: bytes):
        self.data = data
        self.offset = 0

    def read(self, format_string: str):
        codec = struct.Struct("<" + format_string)
        if self.offset + codec.size > len(self.data):
            raise ProtocolError("truncated payload")
        values = codec.unpack_from(self.data, self.offset)
        self.offset += codec.size
        return values[0] if len(values) == 1 else values

    def f32(self) -> float:
        value = self.read("f")
        if not math.isfinite(value):
            raise ProtocolError("nonfinite number")
        return value

    def vec3(self) -> tuple[float, float, float]:
        return self.f32(), self.f32(), self.f32()

    def boolean(self) -> bool:
        value = self.read("B")
        if value not in (0, 1):
            raise ProtocolError("invalid boolean")
        return bool(value)

    def finish(self):
        if self.offset != len(self.data):
            raise ProtocolError("extra payload bytes")


def parse_frame(frame: bytes) -> Request:
    """Decode the same field order and allowlist as ai_model_io_wire.h."""
    if len(frame) < HEADER.size or len(frame) > MAX_MESSAGE:
        raise ProtocolError("message size outside limits")
    magic, version, kind, length = HEADER.unpack_from(frame)
    if (magic != MAGIC or version not in (WIRE_VERSION, AFFORDANCE_WIRE_VERSION)
            or kind != REQUEST_KIND or length != len(frame) - HEADER.size):
        raise ProtocolError("invalid wire header")

    reader = Reader(frame[HEADER.size:])
    model_schema = reader.read("I")
    request_id = reader.read("Q")
    stamp_schema = reader.read("I")
    session_id = reader.read("Q")
    frame_id = reader.read("Q")
    reader.read("Q")  # round ID
    game_time = reader.f32()
    frame_seconds = reader.f32()
    bot_index = reader.read("i")
    if (model_schema != MODEL_SCHEMA or stamp_schema != STAMP_SCHEMA
            or request_id == 0 or session_id == 0 or frame_id == 0
            or bot_index <= 0 or game_time < 0 or frame_seconds < 0):
        raise ProtocolError("invalid session/frame identity")

    origin = reader.vec3()
    reader.vec3()  # velocity
    view = reader.vec3()
    reader.f32()  # health
    armor = reader.f32()
    max_speed = reader.f32()
    alive = reader.boolean()
    on_ground = reader.boolean()
    reader.boolean()  # crouching
    if armor < 0 or max_speed < 0:
        raise ProtocolError("invalid self state")

    seen_index = reader.read("i")
    seen_position = reader.vec3()
    seen = reader.boolean()
    if seen:
        if seen_index <= 0 or seen_index == bot_index:
            raise ProtocolError("invalid visible entity")
    elif seen_index != -1 or seen_position != (0.0, 0.0, 0.0):
        raise ProtocolError("hidden entity fields")

    event_count = reader.read("B")
    if event_count > MAX_EVENTS:
        raise ProtocolError("too many events")
    for _ in range(event_count):
        kind = reader.read("B")
        evidence = reader.read("B")
        subject = reader.read("i")
        position = reader.vec3()
        has_position = reader.boolean()
        if not has_position and position != (0.0, 0.0, 0.0):
            raise ProtocolError("latent event position")
        if kind in (5, 6):  # RoundStarted/RoundEnded, PublicEvent
            valid = evidence == 2 and subject == -1 and not has_position
        elif kind == 4:  # EntityDisappeared, Remembered
            valid = evidence == 5 and subject > 0 and subject != bot_index and not has_position
        elif kind == 3:  # EntityAppeared, Seen
            valid = (evidence == 3 and seen and subject == seen_index
                     and has_position and position == seen_position)
        elif kind == 1:  # Sound, Heard
            valid = evidence == 4 and subject == -1 and has_position
        else:
            valid = False
        if not valid:
            raise ProtocolError("unauthorized event")

    feedback_decision: int | None = None
    feedback_observed = 0
    feedback_completed = False
    if reader.boolean():  # ActionFeedback
        feedback_session = reader.read("Q")
        decision_frame = reader.read("Q")
        observed_frame = reader.read("Q")
        action = reader.read("B")
        result_type = reader.read("B")
        elapsed = reader.f32()
        if (feedback_session != session_id or decision_frame == 0
                or decision_frame > observed_frame or observed_frame > frame_id
                or action == 0 or result_type not in (1, 2, 3, 4, 5, 6)
                or elapsed < 0):
            raise ProtocolError("invalid feedback")
        feedback_decision = decision_frame
        feedback_observed = observed_frame
        feedback_completed = action == 2 and result_type == 2

    feature_schema = reader.read("I")
    count = reader.read("I")
    if feature_schema != FEATURE_SCHEMA or count != FEATURE_COUNT:
        raise ProtocolError("unsupported feature vector")
    for _ in range(count):
        reader.f32()
    safe_directions_mask = 0
    if version == AFFORDANCE_WIRE_VERSION:
        safe_directions_mask = reader.read("B")
        if not 1 <= safe_directions_mask <= 7:
            raise ProtocolError("invalid safe-direction mask")
    reader.finish()
    return Request(request_id, session_id, frame_id, bot_index,
                   origin, view[1], alive, on_ground,
                   feedback_decision, feedback_observed, feedback_completed,
                   version, safe_directions_mask)


def local_direction_target(request: Request, direction: int) -> tuple[float, float, float]:
    if direction not in (0, 1, 2):
        raise ValueError("invalid local direction")
    angle = math.radians(request.yaw + (90.0 if direction == 1
                                       else -90.0 if direction == 2 else 0.0))
    return (request.origin[0] + MEMORY_STEP_DISTANCE * math.cos(angle),
            request.origin[1] + MEMORY_STEP_DISTANCE * math.sin(angle),
            request.origin[2])


def choose_memory_direction(request: Request, state: SequenceState) -> int:
    """Pick a traced direction using only four verified recent locations.

    Revisit avoidance dominates heading continuity; neither may override
    the engine-supplied allowlist. Ties are deterministic, favoring forward.
    """
    if (request.wire_version != AFFORDANCE_WIRE_VERSION or
            not 1 <= request.safe_directions_mask <= 7):
        raise ProtocolError("memory policy requires wire-v2 affordances")
    if any(not math.isfinite(value) for point in state.visited for value in point):
        raise ProtocolError("nonfinite memory")
    best_direction: int | None = None
    best_score = -float("inf")
    for direction in range(3):
        if not request.safe_directions_mask & (1 << direction):
            continue
        target = local_direction_target(request, direction)
        if not all(math.isfinite(v) and abs(v) <= 3.4e38 for v in target):
            continue
        # A return to a recent position is penalized even if the bot turns.
        distance = min((
            (target[0] - point[0]) ** 2 + (target[1] - point[1]) ** 2
            for point in state.visited
        ), default=MEMORY_STEP_DISTANCE ** 2)
        novelty = min(distance / (MEMORY_STEP_DISTANCE ** 2), 1.0)
        alignment = 0.0
        if len(state.visited) >= 2:
            prev = state.visited[-2]
            last = state.visited[-1]
            dx, dy = last[0] - prev[0], last[1] - prev[1]
            norm = math.hypot(dx, dy)
            if norm > 0.001:
                vx, vy = target[0] - request.origin[0], target[1] - request.origin[1]
                alignment = (dx * vx + dy * vy) / (norm * MEMORY_STEP_DISTANCE)
        score = novelty + 0.30 * alignment + (0.03 if direction == 0 else 0.0)
        if score > best_score:
            best_score, best_direction = score, direction
    if best_direction is None:
        raise ProtocolError("no finite traced memory candidate")
    return best_direction


def response_frame(request: Request, *, move: bool, direction: int = 0) -> bytes:
    action_id = 0
    confidence = 0.0
    target = (0.0, 0.0, 0.0)
    if direction not in (0, 1, 2):
        raise ValueError("invalid local direction")
    if move and request.alive and request.on_ground:
        # The engine's traced allowlist remains the final authority.
        target = local_direction_target(request, direction)
        if all(math.isfinite(v) and abs(v) <= 3.4e38 for v in target):
            action_id = 2  # InferenceActionId::MoveToPosition
            confidence = 1.0
        else:
            target = (0.0, 0.0, 0.0)

    # Model schema, echoed identity, inference status, action schema, raw action.
    payload = struct.pack("<IQQQBI Bii3fBBff",
                          MODEL_SCHEMA, request.request_id, request.session_id,
                          request.frame_id, 0 if action_id else 1,
                          ACTION_SCHEMA, action_id, -1, -1,
                          *target, 0, 0, 0.0, confidence)
    frame = HEADER.pack(MAGIC, request.wire_version, RESPONSE_KIND, len(payload)) + payload
    if len(frame) > MAX_MESSAGE:
        raise ProtocolError("response too large")
    return frame


def recv_exact(client: socket.socket, length: int) -> bytes:
    parts = bytearray()
    while len(parts) < length:
        part = client.recv(length - len(parts))
        if not part:
            raise ProtocolError("early EOF")
        parts.extend(part)
    return bytes(parts)


def choose_sequence_action(request: Request, state: SequenceState, legs: int) -> bool:
    """Require a correlated terminal result between successive model actions."""
    if request.frame_id <= state.last_frame:
        raise ProtocolError("stale sequence frame")
    state.last_frame = request.frame_id
    if state.pending_frame:
        if (request.feedback_completed
                and request.feedback_decision == state.pending_frame
                and request.feedback_observed >= state.pending_frame
                and request.feedback_observed < request.frame_id):
            state.pending_frame = 0
            state.completed += 1
            if state.pending_target is not None and state.pending_origin is not None:
                if not state.visited:
                    state.visited.append(state.pending_origin)
                state.visited.append(state.pending_target)
                del state.visited[:-MEMORY_HISTORY_CAPACITY]
            state.pending_origin = None
            state.pending_target = None
        return False
    if state.completed >= legs:
        return False
    if not request.alive or not request.on_ground:
        return False
    state.pending_frame = request.frame_id
    return True


def fault_response(request: Request, frame: bytes, fault: str) -> bytes:
    """Apply an explicitly selected wire fault to one outbound test response."""
    if fault not in FAULT_CHOICES:
        raise ValueError("unknown fault")
    if fault == "no-decision":
        return response_frame(request, move=False)
    if fault == "duplicate-move":
        return response_frame(request, move=True)
    damaged = bytearray(frame)
    if fault == "bad-magic":
        damaged[0] = ord("X")
    elif fault == "bad-version":
        struct.pack_into("<H", damaged, 4, request.wire_version + 1)
    elif fault == "oversized":
        struct.pack_into("<I", damaged, 8, MAX_MESSAGE)
    elif fault == "foreign-request":
        struct.pack_into("<Q", damaged, 16, request.request_id + 1)
    elif fault == "foreign-session":
        struct.pack_into("<Q", damaged, 24, request.session_id + 1)
    elif fault == "foreign-frame":
        struct.pack_into("<Q", damaged, 32, request.frame_id + 1)
    elif fault == "nonfinite":
        struct.pack_into("<f", damaged, 72, float("nan"))
    elif fault == "unsafe-target":
        struct.pack_into("<f", damaged, 54, request.origin[0] + 512.0)
    elif fault == "unsafe-action":
        damaged[45] = 11  # InferenceActionId::Fire; never authorized by D330.
    return bytes(damaged)


def send_fault_response(client: socket.socket, frame: bytes, fault: str) -> None:
    if fault == "disconnect":
        return  # Scoped client is closed immediately after dispatch.
    if fault == "timeout":
        time.sleep(FAULT_SLEEP_SECONDS)
        return
    if fault == "truncated":
        client.sendall(frame[:HEADER.size + 4])
        return
    client.sendall(frame)


def handle_client(client: socket.socket, mode: str, emitted: set[tuple[int, int]],
                  sequences: dict[tuple[int, int], SequenceState] | None = None,
                  legs: int = 2, fault: str = "none", actions: int = 4):
    client.settimeout(READ_TIMEOUT)
    try:
        header = recv_exact(client, HEADER.size)
        magic, version, kind, payload_length = HEADER.unpack(header)
        if (magic != MAGIC or version not in (WIRE_VERSION, AFFORDANCE_WIRE_VERSION)
                or kind != REQUEST_KIND
                or payload_length == 0 or payload_length > MAX_MESSAGE - HEADER.size):
            raise ProtocolError("invalid length or frame kind")
        request = parse_frame(header + recv_exact(client, payload_length))
        key = request.session_id, request.bot_index
        direction = 0
        if mode in ("move-sequence", "move-session", "move-steer", "move-avoid",
                    "move-memory"):
            if sequences is None:
                raise ProtocolError("sequence store missing")
            state = sequences.setdefault(key, SequenceState())
            move = choose_sequence_action(
                request, state, actions if mode != "move-sequence" else legs)
            if mode == "move-steer":
                direction = state.completed % 3
            elif mode == "move-avoid":
                if request.wire_version != AFFORDANCE_WIRE_VERSION:
                    raise ProtocolError("move-avoid requires wire-v2 affordances")
                preference = state.completed % 3
                allowed = request.safe_directions_mask
                direction = next(
                    candidate for candidate in
                    ((preference + offset) % 3 for offset in range(3))
                    if allowed & (1 << candidate)
                )
            elif mode == "move-memory":
                if request.wire_version != AFFORDANCE_WIRE_VERSION:
                    raise ProtocolError("move-memory requires wire-v2 affordances")
                if move:
                    direction = choose_memory_direction(request, state)
                    state.pending_origin = request.origin
                    state.pending_target = local_direction_target(request, direction)
        else:
            move = mode == "move-once" and key not in emitted
        reply = response_frame(request, move=move, direction=direction)
        if fault != "none":
            reply = fault_response(request, reply, fault)
        send_fault_response(client, reply, fault)
        if fault == "none" and move and reply[45] == 2 and mode == "move-once":
            emitted.add(key)
    except (ProtocolError, OSError, struct.error, OverflowError):
        # Invalid clients are disconnected without a success response.
        return


def serve(port: int, max_connections: int, mode: str,
          stop_event: threading.Event | None = None, legs: int = 2,
          fault: str = "none", fault_on_request: int = 1,
          actions: int = 4) -> int:
    if not 0 <= port <= 65535 or not 1 <= max_connections <= MAX_CONNECTIONS:
        raise ValueError("port or max-connections outside limits")
    if legs not in (2, 3):
        raise ValueError("sequence legs must be 2 or 3")
    if not SESSION_MIN_ACTIONS <= actions <= SESSION_MAX_ACTIONS:
        raise ValueError("session actions must be 4..8")
    if fault not in FAULT_CHOICES or not 1 <= fault_on_request <= max_connections:
        raise ValueError("unknown fault or fault-on-request outside connection limit")
    if fault != "none" and mode == "no-decision":
        raise ValueError("fault injection requires move-once or move-sequence mode")
    if mode == "move-sequence" and max_connections < 2 * legs:
        raise ValueError("move-sequence needs at least two connections per leg")
    if mode in ("move-session", "move-steer", "move-avoid", "move-memory") and max_connections < 2 * actions:
        raise ValueError("extended movement needs two connections per action")
    if stop_event is None:
        stop_event = threading.Event()
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind((HOST, port))
        listener.listen(4)
        # Polling is required for Ctrl+C responsiveness during accept on Windows.
        listener.settimeout(ACCEPT_POLL_INTERVAL)
        idle_deadline = time.monotonic() + LISTENER_IDLE_TIMEOUT
        print(f"READY {listener.getsockname()[1]}", flush=True)
        emitted: set[tuple[int, int]] = set()
        sequences: dict[tuple[int, int], SequenceState] = {}
        accepted = 0
        while accepted < max_connections:
            if stop_event.is_set():
                print("STOPPED", flush=True)
                return 0
            if time.monotonic() >= idle_deadline:
                return 1
            try:
                client, _ = listener.accept()
            except socket.timeout:
                continue
            with client:
                current_fault = fault if accepted + 1 == fault_on_request else "none"
                handle_client(client, mode, emitted, sequences, legs, current_fault, actions)
            accepted += 1
            idle_deadline = time.monotonic() + LISTENER_IDLE_TIMEOUT
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=0,
                        help="IPv4 loopback port; 0 selects an ephemeral test port")
    parser.add_argument("--max-connections", type=int, default=1,
                        help=f"bounded accepted connections (1..{MAX_CONNECTIONS})")
    parser.add_argument("--mode", choices=("no-decision", "move-once", "move-sequence",
                                           "move-session", "move-steer", "move-avoid",
                                           "move-memory"),
                        default="no-decision")
    parser.add_argument("--legs", type=int, choices=(2, 3), default=2,
                        help="number of verified movement legs in move-sequence mode")
    parser.add_argument("--actions", type=int, choices=range(
        SESSION_MIN_ACTIONS, SESSION_MAX_ACTIONS + 1), default=4,
                        metavar="{4,5,6,7,8}",
                        help="maximum verified actions for move-session mode")
    parser.add_argument("--fault", choices=FAULT_CHOICES, default="none",
                        help="inject one bounded failure into an explicit move trial")
    parser.add_argument("--fault-on-request", type=int, default=1,
                        help="1-based accepted TCP connection to inject the fault into")
    args = parser.parse_args(argv)
    stop_event = threading.Event()

    def request_stop(_signum: int, _frame: object) -> None:
        stop_event.set()

    previous_handler = signal.signal(signal.SIGINT, request_stop)
    try:
        return serve(args.port, args.max_connections, args.mode, stop_event,
                     args.legs, args.fault, args.fault_on_request, args.actions)
    except KeyboardInterrupt:
        # Preserve a clean exit if an interrupt arrives outside socket polling.
        print("STOPPED", flush=True)
        return 0
    except (ValueError, OSError) as exc:
        parser.error(str(exc))
    finally:
        signal.signal(signal.SIGINT, previous_handler)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
