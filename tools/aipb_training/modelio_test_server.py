"""Local, deterministic Model I/O wire-v1 server for standalone integration tests.

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
from dataclasses import dataclass
from typing import Sequence

HOST = "127.0.0.1"
HEADER = struct.Struct("<4sHHI")
MAGIC = b"AIPB"
WIRE_VERSION = 1
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
READ_TIMEOUT = 0.5
ACCEPT_POLL_INTERVAL = 0.2
LISTENER_IDLE_TIMEOUT = 30.0


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
    if (magic != MAGIC or version != WIRE_VERSION or kind != REQUEST_KIND
            or length != len(frame) - HEADER.size):
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

    feature_schema = reader.read("I")
    count = reader.read("I")
    if feature_schema != FEATURE_SCHEMA or count != FEATURE_COUNT:
        raise ProtocolError("unsupported feature vector")
    for _ in range(count):
        reader.f32()
    reader.finish()
    return Request(request_id, session_id, frame_id, bot_index,
                   origin, view[1], alive, on_ground)


def response_frame(request: Request, *, move: bool) -> bytes:
    action_id = 0
    confidence = 0.0
    target = (0.0, 0.0, 0.0)
    if move and request.alive and request.on_ground:
        angle = math.radians(request.yaw)
        target = (request.origin[0] + 96.0 * math.cos(angle),
                  request.origin[1] + 96.0 * math.sin(angle),
                  request.origin[2])
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
    frame = HEADER.pack(MAGIC, WIRE_VERSION, RESPONSE_KIND, len(payload)) + payload
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


def handle_client(client: socket.socket, mode: str, emitted: set[tuple[int, int]]):
    client.settimeout(READ_TIMEOUT)
    try:
        header = recv_exact(client, HEADER.size)
        magic, version, kind, payload_length = HEADER.unpack(header)
        if (magic != MAGIC or version != WIRE_VERSION or kind != REQUEST_KIND
                or payload_length == 0 or payload_length > MAX_MESSAGE - HEADER.size):
            raise ProtocolError("invalid length or frame kind")
        request = parse_frame(header + recv_exact(client, payload_length))
        key = request.session_id, request.bot_index
        move = mode == "move-once" and key not in emitted
        reply = response_frame(request, move=move)
        client.sendall(reply)
        if move and reply[45] == 2:
            emitted.add(key)
    except (ProtocolError, OSError, struct.error, OverflowError):
        # Invalid clients are disconnected without a success response.
        return


def serve(port: int, max_connections: int, mode: str,
          stop_event: threading.Event | None = None) -> int:
    if not 0 <= port <= 65535 or not 1 <= max_connections <= MAX_CONNECTIONS:
        raise ValueError("port or max-connections outside limits")
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
                handle_client(client, mode, emitted)
            accepted += 1
            idle_deadline = time.monotonic() + LISTENER_IDLE_TIMEOUT
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=0,
                        help="IPv4 loopback port; 0 selects an ephemeral test port")
    parser.add_argument("--max-connections", type=int, default=1,
                        help=f"bounded accepted connections (1..{MAX_CONNECTIONS})")
    parser.add_argument("--mode", choices=("no-decision", "move-once"),
                        default="no-decision")
    args = parser.parse_args(argv)
    stop_event = threading.Event()

    def request_stop(_signum: int, _frame: object) -> None:
        stop_event.set()

    previous_handler = signal.signal(signal.SIGINT, request_stop)
    try:
        return serve(args.port, args.max_connections, args.mode, stop_event)
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
