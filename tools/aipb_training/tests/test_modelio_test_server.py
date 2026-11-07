"""Unit and real TCP subprocess tests for the deterministic Model I/O server."""

from __future__ import annotations

import signal
import socket
import struct
import subprocess
import sys
import threading
import time
import unittest
from pathlib import Path

if __name__ == "__main__" and not __package__:
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
    __package__ = "tools.aipb_training.tests"

from tools.aipb_training import modelio_test_server as server


SCRIPT = Path(server.__file__).resolve()


def fixture(*, seen: bool = False, count: int = server.FEATURE_COUNT,
            frame: int = 12, feedback_decision: int | None = None) -> bytes:
    """Build an independent wire-v1 fixture with all 252 model features."""
    payload = bytearray()
    payload.extend(struct.pack("<IQ", server.MODEL_SCHEMA, frame))
    payload.extend(struct.pack("<IQQQffi", 1, 7, frame, 1, 2.0, 0.02, 2))
    payload.extend(struct.pack("<3f3f3f3f", 10, 20, 30, 0, 0, 0,
                               0, 0, 0, 100, 0, 250))
    payload.extend(struct.pack("<BBB", 1, 1, 0))
    payload.extend(struct.pack("<i3fB", 7 if seen else -1,
                               40 if seen else 0, 50 if seen else 0,
                               60 if seen else 0, int(seen)))
    payload.extend(struct.pack("<B", 0))  # zero perception events
    payload.extend(struct.pack("<B", 1 if feedback_decision is not None else 0))
    if feedback_decision is not None:
        payload.extend(struct.pack("<QQQBBf", 7, feedback_decision, frame - 1,
                                   2, 2, 0.5))
    payload.extend(struct.pack("<II", server.FEATURE_SCHEMA, count))
    payload.extend(struct.pack(f"<{count}f", *([0.0] * count)))
    return server.HEADER.pack(server.MAGIC, 1, 1, len(payload)) + payload


class ServerProtocolTests(unittest.TestCase):
    def test_exact_wire_fixture_and_default_no_decision(self):
        request = server.parse_frame(fixture())
        self.assertEqual((request.request_id, request.session_id, request.frame_id), (9, 7, 12))
        self.assertEqual(request.origin, (10.0, 20.0, 30.0))
        raw = server.response_frame(request, move=False)
        self.assertEqual(len(raw), 76)
        self.assertEqual(server.HEADER.unpack_from(raw),
                         (b"AIPB", 1, 2, len(raw) - 12))
        self.assertEqual(raw[40], 1)  # InferenceStatus::NoDecision
        self.assertEqual(raw[45], 0)  # InferenceActionId::None

    def test_explicit_move_from_authorized_self_only(self):
        request = server.parse_frame(fixture(seen=True))
        raw = server.response_frame(request, move=True)
        self.assertEqual(raw[40], 0)  # Success
        self.assertEqual(raw[45], 2)  # MoveToPosition
        self.assertEqual(struct.unpack_from("<3f", raw, 54), (106.0, 20.0, 30.0))

    def test_corrupt_frame_and_schema_rejected(self):
        raw = fixture()
        with self.assertRaises(server.ProtocolError):
            server.parse_frame(raw[:-1])
        with self.assertRaises(server.ProtocolError):
            server.parse_frame(raw + b"x")
        with self.assertRaises(server.ProtocolError):
            server.parse_frame(b"WRNG" + raw[4:])
        with self.assertRaises(server.ProtocolError):
            server.parse_frame(raw[:4] + b"\x02" + raw[5:])
        with self.assertRaises(server.ProtocolError):
            server.parse_frame(fixture(count=251))
        with self.assertRaises(server.ProtocolError):
            server.parse_frame(raw[:8] + b"\xff\xff\xff\xff" + raw[12:])

    def test_hidden_fields_rejected(self):
        raw = bytearray(fixture())
        # 12 header + 12 model envelope + 40 frame stamp + 51 self state.
        # VisibleEnemy has an invalid-by-default (-1) entity and zero position.
        offset = 12 + 12 + 40 + 51 + 4
        struct.pack_into("<f", raw, offset, 999.0)
        with self.assertRaises(server.ProtocolError):
            server.parse_frame(bytes(raw))

    def test_sequence_requires_correlated_terminal_feedback(self):
        state = server.SequenceState()
        first = server.parse_frame(fixture(frame=12))
        self.assertTrue(server.choose_sequence_action(first, state, 2))
        self.assertEqual(state.pending_frame, 12)
        self.assertFalse(server.choose_sequence_action(
            server.parse_frame(fixture(frame=13)), state, 2))
        self.assertEqual(state.completed, 0)
        self.assertFalse(server.choose_sequence_action(
            server.parse_frame(fixture(frame=14, feedback_decision=12)), state, 2))
        self.assertEqual(state.completed, 1)
        self.assertTrue(server.choose_sequence_action(
            server.parse_frame(fixture(frame=15)), state, 2))
        self.assertFalse(server.choose_sequence_action(
            server.parse_frame(fixture(frame=16, feedback_decision=15)), state, 2))
        self.assertEqual(state.completed, 2)
        self.assertFalse(server.choose_sequence_action(
            server.parse_frame(fixture(frame=17)), state, 2))
        with self.assertRaises(server.ProtocolError):
            server.choose_sequence_action(server.parse_frame(fixture(frame=17)), state, 2)

    def test_cli_limits(self):
        with self.assertRaises(ValueError):
            server.serve(70000, 1, "no-decision")
        with self.assertRaises(ValueError):
            server.serve(0, 65, "no-decision")
        with self.assertRaises(ValueError):
            server.serve(0, 3, "move-sequence", legs=2)
        with self.assertRaises(ValueError):
            server.serve(0, 6, "move-sequence", legs=4)


class ServerProcessTests(unittest.TestCase):
    def test_clean_stop_event_while_idle(self):
        stop_event = threading.Event()
        timer = threading.Timer(0.05, stop_event.set)
        timer.start()
        try:
            started = time.monotonic()
            result = server.serve(0, 1, "no-decision", stop_event)
            self.assertEqual(result, 0)
            self.assertLess(time.monotonic() - started, 2.0)
        finally:
            timer.join(timeout=2.0)

    @unittest.skipIf(sys.platform == "win32",
                     "Windows console Ctrl+C requires its own process group")
    def test_subprocess_sigint_exits_cleanly(self):
        process = subprocess.Popen(
            [sys.executable, str(SCRIPT), "--port", "0",
             "--max-connections", "10"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        try:
            self.assertIsNotNone(process.stdout)
            ready = process.stdout.readline().strip()
            self.assertTrue(ready.startswith("READY "), ready)
            process.send_signal(signal.SIGINT)
            self.assertEqual(process.wait(timeout=3), 0)
            self.assertEqual(process.stdout.readline().strip(), "STOPPED")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=3)
            if process.stdout:
                process.stdout.close()
            if process.stderr:
                process.stderr.close()

    def test_subprocess_sequence_two_verified_moves(self):
        process = subprocess.Popen(
            [sys.executable, str(SCRIPT), "--port", "0",
             "--max-connections", "5", "--mode", "move-sequence",
             "--legs", "2"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        try:
            self.assertIsNotNone(process.stdout)
            ready = process.stdout.readline().strip()
            self.assertTrue(ready.startswith("READY "), ready)
            port = int(ready.split()[1])
            expected = ((12, None, 2), (13, 12, 0), (14, None, 2),
                        (15, 14, 0), (16, None, 0))
            for frame, feedback, action in expected:
                with socket.create_connection((server.HOST, port), timeout=2) as client:
                    client.settimeout(2)
                    client.sendall(fixture(frame=frame, feedback_decision=feedback))
                    header = server.recv_exact(client, 12)
                    size = server.HEADER.unpack(header)[3]
                    reply = header + server.recv_exact(client, size)
                self.assertEqual(reply[45], action)
            self.assertEqual(process.wait(timeout=3), 0)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=3)
            if process.stdout:
                process.stdout.close()
            if process.stderr:
                process.stderr.close()

    def test_subprocess_tcp_no_decision(self):
        process = subprocess.Popen(
            [sys.executable, str(SCRIPT), "--port", "0", "--max-connections", "1"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        try:
            self.assertIsNotNone(process.stdout)
            line = process.stdout.readline().strip()
            self.assertTrue(line.startswith("READY "), line)
            port = int(line.split()[1])
            with socket.create_connection((server.HOST, port), timeout=2.0) as client:
                client.settimeout(2.0)
                client.sendall(fixture())
                header = server.recv_exact(client, 12)
                length = server.HEADER.unpack(header)[3]
                reply = header + server.recv_exact(client, length)
            self.assertEqual(reply[40], 1)
            self.assertEqual(reply[45], 0)
            self.assertEqual(process.wait(timeout=3), 0)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=3)
            if process.stdout:
                process.stdout.close()
            if process.stderr:
                process.stderr.close()


if __name__ == "__main__":
    unittest.main()
