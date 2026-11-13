"""D339 blocked-forward memory coverage and correlated TCP regression tests."""

from __future__ import annotations

import contextlib
import io
import json
import socket
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from tools.aipb_training import modelio_blocked_memory as blocked
from tools.aipb_training import modelio_cycle_diagnostics as diagnostics
from tools.aipb_training import modelio_test_server as server
from tools.aipb_training.tests.test_modelio_test_server import fixture_v2


STARTS = ((10.0, 20.0, 30.0), (94.0, 20.0, 30.0),
          (94.0, 104.0, 30.0), (94.0, 20.0, 30.0))
ENDS = ((94.0, 20.0, 30.0), (94.0, 104.0, 30.0),
        (94.0, 20.0, 30.0), (94.0, 104.0, 30.0))
MASKS = (7, 6, 4, 2)
DIRECTIONS = (0, 1, 2, 1)


def example_trace() -> dict:
    rows = []
    for i, (start, end, mask, direction) in enumerate(
            zip(STARTS, ENDS, MASKS, DIRECTIONS)):
        target = (start[0] + 96 if direction == 0 else start[0],
                  start[1] + (96 if direction == 1 else -96 if direction == 2 else 0),
                  start[2])
        rows.append(dict(leg=i + 1, session=7, bot=2, mask=mask,
                         direction=direction, yaw=0.0,
                         origin=list(start), target=list(target),
                         observed=list(end), decision_frame=12 + 2 * i,
                         feedback_frame=13 + 2 * i))
    return dict(schema=1, mode="move-memory", actions=4, records=rows)


class BlockedMemoryTests(unittest.TestCase):
    def test_complete_blocked_forward_coverage(self):
        report = blocked.analyze(example_trace())
        self.assertTrue(report["coverage_complete"])
        self.assertEqual(report["gaps"], [])
        self.assertEqual((report["forward_available"], report["blocked_forward"],
                          report["dual_side"], report["single_side"]),
                         (1, 3, 1, 2))
        self.assertEqual([row["direction"] for row in report["blocked_legs"]],
                         ["left", "right", "left"])
        self.assertTrue(all(row["progressed"] for row in report["blocked_legs"]))

    def test_absent_masks_and_incomplete_trace(self):
        data = example_trace()
        data["records"][1]["mask"] = 7
        report = blocked.analyze(data)
        self.assertIn("missing_two_side_block", report["gaps"])
        data = example_trace()
        data["records"][2]["mask"] = 6
        data["records"][3]["mask"] = 6
        self.assertIn("missing_one_side_block", blocked.analyze(data)["gaps"])
        data = example_trace()
        data["records"] = data["records"][:-1]
        self.assertIn("incomplete_trace", blocked.analyze(data)["gaps"])
        data = example_trace()
        data["records"][0]["mask"] = 2
        data["records"][0]["direction"] = 1
        self.assertIn("missing_forward_available_control",
                      blocked.analyze(data)["gaps"])

    def test_stationary_completed_feedback_is_not_progress(self):
        data = example_trace()
        data["records"][1]["observed"] = data["records"][1]["origin"]
        report = blocked.analyze(data)
        self.assertIn("blocked_leg_without_confirmed_progress", report["gaps"])
        self.assertEqual(report["blocked_legs"][0]["moved"], 0.0)

    def test_cli_validates_and_reports_gap(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "run.json"
            path.write_text(json.dumps(example_trace()), encoding="utf-8")
            with contextlib.redirect_stdout(io.StringIO()) as stream:
                self.assertEqual(blocked.main(["--memory", str(path)]), 0)
            self.assertIn("status=BLOCKED_MEMORY_COVERED", stream.getvalue())
            incomplete = example_trace()
            incomplete["records"].pop()
            path.write_text(json.dumps(incomplete), encoding="utf-8")
            with contextlib.redirect_stdout(io.StringIO()) as stream:
                self.assertEqual(blocked.main(["--memory", str(path)]), 1)
            self.assertIn("status=INSUFFICIENT_BLOCKED_MEMORY_EVIDENCE",
                          stream.getvalue())
            invalid = example_trace()
            invalid["mode"] = "move-avoid"
            path.write_text(json.dumps(invalid), encoding="utf-8")
            with (contextlib.redirect_stdout(io.StringIO()),
                  contextlib.redirect_stderr(io.StringIO())):
                with self.assertRaises(SystemExit) as err:
                    blocked.main(["--memory", str(path)])
            self.assertEqual(err.exception.code, 2)

    def test_memory_uses_only_authorized_blocked_alternatives(self):
        state = server.SequenceState()
        request = server.parse_frame(fixture_v2(mask=6))
        self.assertEqual(server.choose_memory_direction(request, state), 1)
        state.visited = [(10.0, 116.0, 30.0), (10.0, 20.0, 30.0)]
        self.assertEqual(server.choose_memory_direction(request, state), 2)
        self.assertEqual(server.choose_memory_direction(
            server.parse_frame(fixture_v2(mask=2)), state), 1)
        self.assertEqual(server.choose_memory_direction(
            server.parse_frame(fixture_v2(mask=4)), state), 2)

    def test_four_real_tcp_exchange_pairs_with_blocked_masks(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "blocked.json"
            process = subprocess.Popen(
                [sys.executable, "-m", "tools.aipb_training.modelio_test_server",
                 "--port", "0", "--max-connections", "8",
                 "--mode", "move-memory", "--actions", "4",
                 "--trace-file", str(path)],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
            )
            try:
                self.assertIsNotNone(process.stdout)
                ready = process.stdout.readline().strip()
                self.assertTrue(ready.startswith("READY "), ready)
                port = int(ready.split()[1])
                for i in range(8):
                    leg, feedback = divmod(i, 2)
                    origin = ENDS[leg] if feedback else STARTS[leg]
                    frame = bytearray(fixture_v2(
                        frame=12 + i,
                        feedback_decision=11 + i if feedback else None,
                        mask=MASKS[leg]))
                    # Wire-v2 self origin starts after the 12-byte header,
                    # 12-byte model envelope and 40-byte frame stamp.
                    struct.pack_into("<3f", frame, 64, *origin)
                    request = server.parse_frame(bytes(frame))
                    with socket.create_connection((server.HOST, port), timeout=3) as client:
                        client.settimeout(3)
                        client.sendall(frame)
                        header = server.recv_exact(client, server.HEADER.size)
                        size = server.HEADER.unpack(header)[3]
                        response = header + server.recv_exact(client, size)
                    self.assertEqual(response[45], 0 if feedback else 2)
                    if not feedback:
                        expected = server.response_frame(
                            request, move=True, direction=DIRECTIONS[leg])
                        self.assertEqual(response, expected)
                self.assertEqual(process.wait(timeout=4), 0)
                trace = diagnostics.load_trace(path, "move-memory")
                self.assertEqual([item["mask"] for item in trace["records"]],
                                 list(MASKS))
                self.assertEqual([item["direction"] for item in trace["records"]],
                                 list(DIRECTIONS))
                self.assertTrue(blocked.analyze(trace)["coverage_complete"])
                self.assertEqual(len(trace["records"]), 4)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=4)
                if process.stdout:
                    process.stdout.close()
                if process.stderr:
                    process.stderr.close()


if __name__ == "__main__":
    unittest.main()
