"""D338 bounded matched-start repeated local policy comparison tests."""

from __future__ import annotations

import copy
import json
import tempfile
import unittest
from pathlib import Path

from tools.aipb_training import modelio_paired_trials as paired


def fixture(mode: str, offset: int = 0) -> dict:
    rows = []
    for leg in range(1, 5):
        rows.append(dict(leg=leg, session=1, bot=1, mask=7, direction=0,
                         yaw=0.0, origin=[96.0 * (leg - 1), 0, 0],
                         observed=[96.0 * leg, 0, 0],
                         target=[96.0 * leg, 0, 0],
                         decision_frame=leg * 10,
                         feedback_frame=leg * 10 + 1))
    result = dict(schema=1, mode=mode, actions=4, records=rows)
    for row in result["records"]:
        row["decision_frame"] += offset
        row["feedback_frame"] += offset
    return result


class PairedTrialsTests(unittest.TestCase):
    def test_matched_start_and_circular_yaw(self):
        a, b = fixture("move-avoid"), fixture("move-memory")
        a["records"][0]["yaw"] = 359.98
        b["records"][0]["yaw"] = 0.02
        b["records"][0]["origin"][0] = 0.9
        self.assertEqual(paired.pair_gate(a, b)[:2],
                         (True, "matched_start_only"))
        b["records"][0]["origin"][0] = 1.1
        self.assertEqual(paired.pair_gate(a, b)[1], "start_position_mismatch")
        b["records"][0]["origin"][0] = 0
        b["records"][0]["yaw"] = 0.2
        self.assertEqual(paired.pair_gate(a, b)[1], "start_yaw_mismatch")
        b["records"][0]["yaw"] = 0.02
        b["records"][0]["mask"] = 6
        self.assertEqual(paired.pair_gate(a, b)[1], "first_mask_mismatch")

    def test_incomplete_and_different_action_limit(self):
        a, b = fixture("move-avoid"), fixture("move-memory")
        b["records"].pop()
        self.assertEqual(paired.pair_gate(a, b)[1], "incomplete_trial")
        b["actions"] = 5
        self.assertEqual(paired.pair_gate(a, b)[1], "different_action_limits")
        a["records"].clear()
        self.assertEqual(paired.pair_gate(a, b)[1], "empty_trace")

    def test_repeat_pair_aggregation_and_later_divergence(self):
        pairs = []
        for n in (100, 200, 300):
            a, b = fixture("move-avoid", n), fixture("move-memory", n + 1)
            # Different later trajectories and availability are allowed.
            a["records"][1]["origin"][0] += n
            a["records"][1]["mask"] = 2
            a["records"][1]["direction"] = 1
            pairs.append((a, b))
        result = paired.compare_pairs(pairs)
        self.assertEqual(result["matched"], 3)
        self.assertTrue(result["complete"])
        self.assertEqual(result["mean"]["memory"]["efficiency"], 1)
        self.assertEqual(result["delta_efficiency"], 0)

    def test_cross_pair_start_must_match_baseline(self):
        a, b = fixture("move-avoid"), fixture("move-memory")
        c, d = fixture("move-avoid", 20), fixture("move-memory", 20)
        for data in (c, d):
            data["records"][0]["origin"][0] = 15
        result = paired.compare_pairs([(a, b), (c, d)])
        self.assertEqual(result["matched"], 1)
        self.assertEqual(result["pairs"][1]["reason"],
                         "cross_pair_position_mismatch")
        for data in (c, d):
            data["records"][0]["origin"][0] = 0
            data["records"][0]["mask"] = 6
            data["records"][0]["direction"] = 1
        result = paired.compare_pairs([(a, b), (c, d)])
        self.assertEqual(result["pairs"][1]["reason"],
                         "cross_pair_mask_mismatch")

    def test_detect_reused_runs_without_claiming_independence(self):
        a, b = fixture("move-avoid"), fixture("move-memory")
        result = paired.compare_pairs([(a, b), (copy.deepcopy(a), copy.deepcopy(b))])
        self.assertEqual(result["matched"], 1)
        self.assertEqual(result["pairs"][1]["reason"], "duplicate_memory_trace")
        self.assertFalse(result["complete"])

    def test_cli_file_counts_and_unmatched_exit(self):
        a, b = fixture("move-avoid"), fixture("move-memory")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            avoid = []
            memory = []
            for i in (0, 10):
                x, y = copy.deepcopy(a), copy.deepcopy(b)
                for rec in x["records"]:
                    rec["decision_frame"] += i
                    rec["feedback_frame"] += i
                for rec in y["records"]:
                    rec["decision_frame"] += i
                    rec["feedback_frame"] += i
                pa, pb = path / f"a{i}.json", path / f"m{i}.json"
                pa.write_text(json.dumps(x), encoding="utf-8")
                pb.write_text(json.dumps(y), encoding="utf-8")
                avoid.append(str(pa))
                memory.append(str(pb))
            args = ["--avoid", avoid[0], "--memory", memory[0],
                    "--avoid", avoid[1], "--memory", memory[1]]
            self.assertEqual(paired.main(args), 0)
            b["records"][0]["origin"][0] += 20
            Path(memory[1]).write_text(json.dumps(b), encoding="utf-8")
            self.assertEqual(paired.main(args), 1)
            with self.assertRaises(SystemExit) as error:
                paired.main(["--avoid", avoid[0], "--memory", memory[0]])
            self.assertEqual(error.exception.code, 2)

    def test_pair_count_limit(self):
        with self.assertRaisesRegex(ValueError, "2..10"):
            paired.compare_pairs([])
        with self.assertRaisesRegex(ValueError, "2..10"):
            paired.compare_pairs([(fixture("move-avoid"),
                                   fixture("move-memory"))] * 11)


if __name__ == "__main__":
    unittest.main()
