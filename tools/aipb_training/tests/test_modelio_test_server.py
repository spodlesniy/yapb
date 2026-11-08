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


def fixture_v2(*, frame: int = 12, feedback_decision: int | None = None,
               mask: int = 0x07) -> bytes:
    data = bytearray(fixture(frame=frame, feedback_decision=feedback_decision))
    data[4] = server.AFFORDANCE_WIRE_VERSION
    data.append(mask)
    struct.pack_into("<I", data, 8, len(data) - server.HEADER.size)
    return bytes(data)


class ServerProtocolTests(unittest.TestCase):
    def test_exact_wire_fixture_and_default_no_decision(self):
        request = server.parse_frame(fixture())
        self.assertEqual((request.request_id, request.session_id, request.frame_id), (12, 7, 12))
        next_request = server.parse_frame(fixture(frame=13))
        self.assertEqual(next_request.request_id, 13)
        self.assertNotEqual(request.request_id, next_request.request_id)
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

    def test_d334_forward_left_right_targets(self):
        request = server.parse_frame(fixture())
        expected = ((106.0, 20.0, 30.0), (10.0, 116.0, 30.0),
                    (10.0, -76.0, 30.0))
        for direction, target in enumerate(expected):
            response = server.response_frame(request, move=True, direction=direction)
            actual = struct.unpack_from("<3f", response, 54)
            for observed, wanted in zip(actual, target):
                self.assertAlmostEqual(observed, wanted, places=4)
        with self.assertRaises(ValueError):
            server.response_frame(request, move=True, direction=3)

    def test_d335_wire_v2_mask_is_bounded_and_v1_is_unchanged(self):
        original = server.parse_frame(fixture())
        self.assertEqual(original.wire_version, server.WIRE_VERSION)
        self.assertEqual(original.safe_directions_mask, 0)
        for mask in (1, 3, 5, 6, 7):
            request = server.parse_frame(fixture_v2(mask=mask))
            self.assertEqual(request.safe_directions_mask, mask)
            self.assertEqual(request.wire_version, server.AFFORDANCE_WIRE_VERSION)
            reply = server.response_frame(request, move=True, direction=1)
            self.assertEqual(server.HEADER.unpack_from(reply)[1], 2)
        for mask in (0, 8, 255):
            with self.assertRaises(server.ProtocolError):
                server.parse_frame(fixture_v2(mask=mask))
        with self.assertRaises(server.ProtocolError):
            server.parse_frame(fixture_v2()[:-1])
        with self.assertRaises(server.ProtocolError):
            server.parse_frame(fixture_v2() + b"extra")
        with self.assertRaises(server.ProtocolError):
            server.parse_frame(fixture()[:4] + bytes([3]) + fixture()[5:])

    def test_d336_memory_avoids_recent_return_with_full_allowlist(self):
        request = server.parse_frame(fixture_v2(mask=7))
        state = server.SequenceState()
        self.assertEqual(server.choose_memory_direction(request, state), 0)
        state.visited = [(10.0, 20.0, 30.0), (106.0, 20.0, 30.0)]
        self.assertEqual(server.choose_memory_direction(request, state), 1)
        state.visited = [(10.0, 20.0, 30.0), (10.0, 116.0, 30.0)]
        self.assertEqual(server.choose_memory_direction(request, state), 0)
        restricted = server.parse_frame(fixture_v2(mask=4))
        self.assertEqual(server.choose_memory_direction(restricted, state), 2)
        with self.assertRaises(server.ProtocolError):
            server.choose_memory_direction(server.parse_frame(fixture()), state)

    def test_d336_memory_commits_only_verified_feedback_and_evicts_oldest(self):
        state = server.SequenceState()
        for i in range(4):
            start = server.parse_frame(fixture_v2(frame=12 + 3 * i))
            self.assertTrue(server.choose_sequence_action(start, state, 4))
            direction = server.choose_memory_direction(start, state)
            state.pending_origin = (float(i) * 100.0, 20.0, 30.0)
            state.pending_target = server.local_direction_target(start, direction)
            if i == 0:
                self.assertEqual(state.visited, [])
            rejected = server.parse_frame(fixture_v2(frame=13 + 3 * i))
            self.assertFalse(server.choose_sequence_action(rejected, state, 4))
            self.assertEqual(state.completed, i)
            confirmed = server.parse_frame(
                fixture_v2(frame=14 + 3 * i, feedback_decision=12 + 3 * i))
            self.assertFalse(server.choose_sequence_action(confirmed, state, 4))
            self.assertEqual(state.completed, i + 1)
            self.assertLessEqual(len(state.visited), server.MEMORY_HISTORY_CAPACITY)
            self.assertIsNone(state.pending_target)
            self.assertIsNone(state.pending_origin)
        self.assertEqual(len(state.visited), server.MEMORY_HISTORY_CAPACITY)
        self.assertNotIn((0.0, 20.0, 30.0), state.visited)
        self.assertFalse(server.choose_sequence_action(
            server.parse_frame(fixture_v2(frame=24)), state, 4))
        self.assertEqual(server.choose_memory_direction(
            server.parse_frame(fixture_v2(mask=7)),
            server.SequenceState()), 0)

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

    def test_d332_fault_payloads_are_bounded_and_deterministic(self):
        request = server.parse_frame(fixture())
        good = server.response_frame(request, move=True)
        self.assertEqual(server.fault_response(request, good, "none"), good)
        for fault in ("bad-magic", "bad-version", "oversized",
                      "foreign-request", "foreign-session", "foreign-frame",
                      "nonfinite", "unsafe-target", "unsafe-action"):
            corrupt = server.fault_response(request, good, fault)
            self.assertEqual(len(corrupt), len(good), fault)
            self.assertNotEqual(corrupt, good, fault)
        self.assertEqual(server.fault_response(request, good, "no-decision")[40], 1)
        self.assertEqual(server.fault_response(request, good, "duplicate-move")[45], 2)
        with self.assertRaises(ValueError):
            server.fault_response(request, good, "unsupported")

    def test_d332_fault_endpoint_limits(self):
        with self.assertRaises(ValueError):
            server.serve(0, 2, "move-once", fault="bad-magic",
                         fault_on_request=3)
        with self.assertRaises(ValueError):
            server.serve(0, 2, "move-once", fault="bad-magic",
                         fault_on_request=0)
        with self.assertRaises(ValueError):
            server.serve(0, 2, "no-decision", fault="bad-magic")
        with self.assertRaises(ValueError):
            server.serve(0, 2, "move-once", fault="unknown")

    def test_d333_session_action_cap_and_identity(self):
        state = server.SequenceState()
        for i in range(4):
            decision = server.parse_frame(fixture(frame=12 + 2 * i))
            self.assertTrue(server.choose_sequence_action(decision, state, 4))
            feedback = server.parse_frame(
                fixture(frame=13 + 2 * i, feedback_decision=12 + 2 * i))
            self.assertFalse(server.choose_sequence_action(feedback, state, 4))
            self.assertEqual(state.completed, i + 1)
        self.assertFalse(server.choose_sequence_action(
            server.parse_frame(fixture(frame=20)), state, 4))

    def test_d333_session_validation_bounds(self):
        with self.assertRaises(ValueError):
            server.serve(0, 7, "move-session", actions=4)
        with self.assertRaises(ValueError):
            server.serve(0, 16, "move-session", actions=9)
        with self.assertRaises(ValueError):
            server.serve(0, 16, "move-session", actions=3)

    def test_d335_avoid_requires_two_exchanges_per_action(self):
        with self.assertRaises(ValueError):
            server.serve(0, 7, "move-avoid", actions=4)
        with self.assertRaises(ValueError):
            server.serve(0, 16, "move-avoid", actions=9)

    def test_d336_memory_enforces_session_connection_limits(self):
        with self.assertRaises(ValueError):
            server.serve(0, 7, "move-memory", actions=4)
        with self.assertRaises(ValueError):
            server.serve(0, 16, "move-memory", actions=9)

    def test_d334_steer_mode_requires_bounded_exchanges(self):
        with self.assertRaises(ValueError):
            server.serve(0, 7, "move-steer", actions=4)
        with self.assertRaises(ValueError):
            server.serve(0, 16, "move-steer", actions=9)

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

    def test_subprocess_session_four_verified_actions(self):
        process = subprocess.Popen(
            [sys.executable, str(SCRIPT), "--port", "0",
             "--max-connections", "9", "--mode", "move-session",
             "--actions", "4"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        try:
            self.assertIsNotNone(process.stdout)
            ready = process.stdout.readline().strip()
            self.assertTrue(ready.startswith("READY "), ready)
            port = int(ready.split()[1])
            for i in range(9):
                feedback = 12 + i - 1 if i % 2 else None
                with socket.create_connection((server.HOST, port), timeout=2) as client:
                    client.settimeout(2)
                    client.sendall(fixture(frame=12 + i,
                                           feedback_decision=feedback))
                    header = server.recv_exact(client, server.HEADER.size)
                    size = server.HEADER.unpack(header)[3]
                    reply = header + server.recv_exact(client, size)
                expected = 2 if i % 2 == 0 and i < 8 else 0
                self.assertEqual(reply[45], expected)
            self.assertEqual(process.wait(timeout=3), 0)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=3)
            if process.stdout:
                process.stdout.close()
            if process.stderr:
                process.stderr.close()

    def test_subprocess_steer_rotates_only_after_terminal_feedback(self):
        process = subprocess.Popen(
            [sys.executable, str(SCRIPT), "--port", "0",
             "--max-connections", "9", "--mode", "move-steer",
             "--actions", "4"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        try:
            self.assertIsNotNone(process.stdout)
            ready = process.stdout.readline().strip()
            self.assertTrue(ready.startswith("READY "), ready)
            port = int(ready.split()[1])
            targets = ((106.0, 20.0, 30.0), (10.0, 116.0, 30.0),
                       (10.0, -76.0, 30.0), (106.0, 20.0, 30.0))
            for i in range(9):
                feedback = 12 + i - 1 if i % 2 else None
                with socket.create_connection((server.HOST, port), timeout=2) as client:
                    client.settimeout(2)
                    client.sendall(fixture(frame=12 + i,
                                           feedback_decision=feedback))
                    header = server.recv_exact(client, server.HEADER.size)
                    size = server.HEADER.unpack(header)[3]
                    reply = header + server.recv_exact(client, size)
                self.assertEqual(reply[45], 2 if i % 2 == 0 and i < 8 else 0)
                if i % 2 == 0 and i < 8:
                    actual = struct.unpack_from("<3f", reply, 54)
                    for observed, wanted in zip(actual, targets[i // 2]):
                        self.assertAlmostEqual(observed, wanted, places=4)
            self.assertEqual(process.wait(timeout=3), 0)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=3)
            if process.stdout:
                process.stdout.close()
            if process.stderr:
                process.stderr.close()

    def test_subprocess_avoid_selects_only_available_alternatives(self):
        process = subprocess.Popen(
            [sys.executable, str(SCRIPT), "--port", "0",
             "--max-connections", "9", "--mode", "move-avoid",
             "--actions", "4"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        try:
            self.assertIsNotNone(process.stdout)
            ready = process.stdout.readline().strip()
            self.assertTrue(ready.startswith("READY "), ready)
            port = int(ready.split()[1])
            # Preference cycles forward/left/right/forward, skipping blocked
            # choices without producing a forbidden coordinate.
            masks = (0x06, 0x06, 0x03, 0x04)
            chosen = (1, 1, 0, 2)
            for i in range(9):
                feedback = 12 + i - 1 if i % 2 else None
                index = min(i // 2, 3)
                with socket.create_connection((server.HOST, port), timeout=2) as client:
                    client.settimeout(2)
                    client.sendall(fixture_v2(frame=12 + i,
                                            feedback_decision=feedback,
                                            mask=masks[index]))
                    header = server.recv_exact(client, server.HEADER.size)
                    length = server.HEADER.unpack(header)[3]
                    reply = header + server.recv_exact(client, length)
                self.assertEqual(server.HEADER.unpack_from(reply)[1], 2)
                self.assertEqual(reply[45], 2 if i % 2 == 0 and i < 8 else 0)
                if i % 2 == 0 and i < 8:
                    actual = struct.unpack_from("<3f", reply, 54)
                    target = server.response_frame(
                        server.parse_frame(fixture_v2(mask=masks[index])),
                        move=True, direction=chosen[index])
                    self.assertEqual(actual, struct.unpack_from("<3f", target, 54))
            self.assertEqual(process.wait(timeout=3), 0)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=3)
            if process.stdout:
                process.stdout.close()
            if process.stderr:
                process.stderr.close()

    def test_subprocess_memory_responds_after_correlated_feedback(self):
        process = subprocess.Popen(
            [sys.executable, str(SCRIPT), "--port", "0",
             "--max-connections", "9", "--mode", "move-memory",
             "--actions", "4"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        try:
            self.assertIsNotNone(process.stdout)
            ready = process.stdout.readline().strip()
            self.assertTrue(ready.startswith("READY "), ready)
            port = int(ready.split()[1])
            directions = []
            for i in range(9):
                feedback = 12 + i - 1 if i % 2 else None
                with socket.create_connection((server.HOST, port), timeout=2) as client:
                    client.settimeout(2)
                    client.sendall(fixture_v2(frame=12 + i,
                                            feedback_decision=feedback,
                                            mask=7))
                    header = server.recv_exact(client, server.HEADER.size)
                    length = server.HEADER.unpack(header)[3]
                    reply = header + server.recv_exact(client, length)
                self.assertEqual(server.HEADER.unpack_from(reply)[1], 2)
                self.assertEqual(reply[45], 2 if i % 2 == 0 and i < 8 else 0)
                if i % 2 == 0 and i < 8:
                    xy = struct.unpack_from("<2f", reply, 54)
                    targets = ((106.0, 20.0), (10.0, 116.0), (10.0, -76.0))
                    self.assertIn(xy, targets)
                    directions.append(targets.index(xy))
            self.assertEqual(directions[0], 0)
            self.assertEqual(directions[1], 1)
            self.assertEqual(len(directions), 4)
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
