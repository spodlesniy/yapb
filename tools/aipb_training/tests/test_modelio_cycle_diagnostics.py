"""D337 bounded cycle diagnostics and paired policy comparisons."""

from __future__ import annotations

import json
import math
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

from tools.aipb_training import modelio_cycle_diagnostics as diag
from tools.aipb_training import modelio_test_server as server
from tools.aipb_training.tests.test_modelio_test_server import fixture_v2


def record(leg: int, origin: tuple[float, float, float],
           observed: tuple[float, float, float],
           mask: int = 7, direction: int = 0,
           yaw: float = 0.0) -> dict:
    return dict(leg=leg, session=7, bot=2, mask=mask, direction=direction,
                yaw=yaw, origin=list(origin), target=list(observed),
                observed=list(observed), decision_frame=leg * 10,
                feedback_frame=leg * 10 + 1)


def trace(mode: str, observations: list[tuple[float, float, float]]) -> dict:
    rows = [record(i + 1, observations[i], observations[i + 1])
            for i in range(len(observations) - 1)]
    return dict(schema=1, mode=mode, actions=4, records=rows)


class CycleDiagnosticsTests(unittest.TestCase):
    def test_two_step_loop_and_revisit_count_only_real_observations(self):
        data = trace("move-memory", [
            (0, 0, 0), (96, 0, 0), (0, 0, 0), (96, 0, 0), (192, 0, 0)])
        summary = diag.summarize(data)
        self.assertEqual(summary["completed"], 4)
        self.assertEqual(summary["two_step_loops"], 2)
        self.assertEqual(summary["revisits"], 2)
        self.assertEqual(summary["path_length"], 384)
        self.assertEqual(summary["displacement"], 192)
        self.assertEqual(summary["efficiency"], 0.5)

    def test_revisit_radius_and_straight_path(self):
        data = trace("move-avoid", [
            (0, 0, 0), (96, 0, 0), (192, 0, 0), (288, 0, 0)])
        self.assertEqual(diag.summarize(data)["revisits"], 0)
        self.assertEqual(diag.summarize(data)["two_step_loops"], 0)
        small_return = trace("move-memory", [
            (0, 0, 0), (96, 0, 0), (23, 0, 0)])
        self.assertEqual(diag.summarize(small_return)["revisits"], 1)
        self.assertEqual(diag.summarize(small_return)["two_step_loops"], 1)
        outside = trace("move-memory", [
            (0, 0, 0), (96, 0, 0), (25, 0, 0)])
        self.assertEqual(diag.summarize(outside)["revisits"], 0)

    def test_identical_inputs_detects_diverging_positions_and_masks(self):
        a = trace("move-avoid", [(0, 0, 0), (96, 0, 0), (0, 0, 0)])
        b = trace("move-memory", [(0, 0, 0), (96, 0, 0), (192, 0, 0)])
        self.assertTrue(diag.same_inputs(a, b))
        b["records"][1]["origin"][0] += 2
        self.assertFalse(diag.same_inputs(a, b))
        b["records"][1]["origin"][0] -= 2
        b["records"][0]["mask"] = 6
        self.assertFalse(diag.same_inputs(a, b))
        b["records"][0]["mask"] = 7
        b["records"][1]["yaw"] = 2
        self.assertFalse(diag.same_inputs(a, b))

    def test_load_trace_rejects_untrusted_or_unconfirmed_records(self):
        data = trace("move-avoid", [(0, 0, 0), (96, 0, 0)])
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "trace.json"

            def check_invalid(mutated: dict):
                output.write_text(json.dumps(mutated), encoding="utf-8")
                with self.assertRaises(diag.TraceError):
                    diag.load_trace(output, "move-avoid")

            output.write_text(json.dumps(data), encoding="utf-8")
            self.assertEqual(diag.load_trace(output, "move-avoid"), data)
            check_invalid(dict(data, mode="move-memory"))
            check_invalid(dict(data, actions=9))
            check_invalid(dict(data, records=[dict(data["records"][0],
                                                    direction=2, mask=1)]))
            check_invalid(dict(data, records=[dict(data["records"][0],
                                                    observed=[math.inf, 0, 0])]))
            check_invalid(dict(data, records=[dict(data["records"][0],
                                                    feedback_frame=10)]))
            check_invalid(dict(data, records=[dict(data["records"][0],
                                                    decision_frame=0)]))
            check_invalid(dict(data, records=[dict(data["records"][0],
                                                    leg=2)]))
            check_invalid(dict(data, records=[dict(data["records"][0])] * 65))

    def test_trace_never_commits_before_correlated_terminal_feedback(self):
        state = server.SequenceState()
        first = server.parse_frame(fixture_v2(frame=12))
        self.assertTrue(server.choose_sequence_action(first, state, 4))
        state.pending_trace = record(1, (10, 20, 30), (106, 20, 30))
        stale = server.parse_frame(fixture_v2(frame=13))
        self.assertFalse(server.choose_sequence_action(stale, state, 4))
        self.assertEqual(state.trace_records, [])
        confirmed = server.parse_frame(fixture_v2(
            frame=14, feedback_decision=12))
        self.assertFalse(server.choose_sequence_action(confirmed, state, 4))
        self.assertEqual(len(state.trace_records), 1)
        self.assertEqual(state.trace_records[0]["observed"], [10, 20, 30])
        self.assertEqual(state.trace_records[0]["feedback_frame"], 14)
        self.assertIsNone(state.pending_trace)

    def test_tcp_trace_file_contains_only_confirmed_actions(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "run.json"
            process = subprocess.Popen(
                [sys.executable, "-m", "tools.aipb_training.modelio_test_server",
                 "--port", "0", "--max-connections", "8",
                 "--mode", "move-avoid", "--actions", "4",
                 "--trace-file", str(path)],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
            )
            try:
                self.assertIsNotNone(process.stdout)
                line = process.stdout.readline().strip()
                self.assertTrue(line.startswith("READY "), line)
                port = int(line.split()[1])
                for i in range(8):
                    with socket.create_connection((server.HOST, port), timeout=3) as client:
                        client.settimeout(3)
                        client.sendall(fixture_v2(
                            frame=12 + i,
                            feedback_decision=12 + i - 1 if i % 2 else None,
                            mask=7))
                        header = server.recv_exact(client, server.HEADER.size)
                        size = server.HEADER.unpack(header)[3]
                        reply = header + server.recv_exact(client, size)
                    self.assertEqual(reply[45], 2 if i % 2 == 0 else 0)
                    deadline = time.monotonic() + 2.0
                    while True:
                        try:
                            report = diag.load_trace(path, "move-avoid")
                            if len(report["records"]) == (i + 1) // 2:
                                break
                        except (FileNotFoundError, json.JSONDecodeError):
                            pass
                        if time.monotonic() >= deadline:
                            self.fail("server trace did not commit the expected feedback")
                        time.sleep(0.01)
                self.assertEqual(process.wait(timeout=4), 0)
                records = diag.load_trace(path)["records"]
                self.assertEqual(len(records), 4)
                self.assertEqual(records[0]["decision_frame"], 12)
                self.assertEqual(records[0]["feedback_frame"], 13)
                self.assertEqual(records[0]["direction"], 0)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=4)
                if process.stdout:
                    process.stdout.close()
                if process.stderr:
                    process.stderr.close()

    def test_cli_comparison_output(self):
        a = trace("move-avoid", [
            (0, 0, 0), (96, 0, 0), (0, 0, 0)])
        b = trace("move-memory", [
            (0, 0, 0), (96, 0, 0), (192, 0, 0)])
        with tempfile.TemporaryDirectory() as directory:
            pa = Path(directory) / "avoid.json"
            pb = Path(directory) / "memory.json"
            pa.write_text(json.dumps(a), encoding="utf-8")
            pb.write_text(json.dumps(b), encoding="utf-8")
            self.assertEqual(diag.load_trace(pa, "move-avoid"), a)
            self.assertEqual(diag.load_trace(pb, "move-memory"), b)
            self.assertTrue(diag.same_inputs(a, b))


if __name__ == "__main__":
    unittest.main()
