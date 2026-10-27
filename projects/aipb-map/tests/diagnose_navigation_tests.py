"""D210.13 location-aware connectivity issue regression fixtures."""

import contextlib
import importlib.util
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPTS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(SCRIPTS))
SPEC = importlib.util.spec_from_file_location("diagnose_navigation", SCRIPTS / "diagnose_navigation.py")
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def record(map_name="de_dust2"):
    edges = [
        (1, 2, False),
        (2, 1, False),
        (2, 3, True),
        (3, 4, False),
        (4, 3, False),
    ]
    report = {
        "format": "aipb-preliminary-connectivity",
        "schema_version": "0.9.0",
        "map_name": map_name,
        "source_sha256": "a" * 64,
        "artifact_sha256": "b" * 64,
        "candidate_count": 5,
        "edge_count": 5,
        "spawn_anchors": 1,
        "candidates_reachable_from_spawns": 4,
        "candidates_reachable_unconditionally": 2,
        "candidates_unreachable_from_spawns": 1,
        "isolated_candidates": 1,
        "strong_components": 3,
        "unconditional_strong_components": 4,
        "weak_components": 2,
        "largest_strong_component": 2,
        "one_way_sink_components": 1,
        "one_way_sink_candidates": 2,
        "conditional_edges": 1,
        "conditional_door_edges": 0,
        "jump_edges": 0,
        "crouch_jump_edges": 0,
        "drop_edges": 0,
        "edges": [
            {"from": a, "to": b, "conditional_support": conditional}
            for a, b, conditional in edges
        ],
    }
    candidates = {
        "format": "aipb-navigation-candidates",
        "schema_version": "0.3.3",
        "map_name": map_name,
        "source_sha256": "a" * 64,
        "artifact_sha256": "b" * 64,
        "candidate_count": 5,
        "candidates": [
            {"id": i, "position": [float(i) * 32, 64.0, 0.0], "support_kind": "world"}
            for i in range(1, 6)
        ],
        "spawns": [
            {"entity_id": "8", "covered_candidate_id": 1.0}
        ],
    }
    return report, candidates


class DiagnoseTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.folder = self.root / "de_dust2"
        self.folder.mkdir()

    def write(self, report=None, candidates=None):
        default_report, default_candidates = record()

        if report is None:
            report = default_report

        if candidates is None:
            candidates = default_candidates

        (self.folder / "preliminary-connectivity.json").write_text(
            json.dumps(report), encoding="utf-8"
        )
        (self.folder / "navigation-candidates.json").write_text(
            json.dumps(candidates), encoding="utf-8"
        )

    def diagnose(self):
        return MODULE.diagnose(
            self.folder / "preliminary-connectivity.json",
            self.folder / "navigation-candidates.json"
        )

    def test_components_locations_and_consistency(self):
        self.write()
        result = self.diagnose()
        self.assertEqual(len(result["issues"]), 2)
        self.assertEqual([issue["component"] for issue in result["issues"]], [5, 3])
        self.assertEqual(result["issues"][0]["unreachable"], 1)
        self.assertEqual(result["issues"][0]["isolated"], 1)
        self.assertEqual(result["issues"][1]["conditional_only"], 2)
        self.assertTrue(result["issues"][1]["sink"])
        self.assertEqual(result["issues"][1]["center"], (112.0, 64.0, 0.0))
        self.assertEqual(result["issues"][0]["reverse_exits"], 0)
        self.assertEqual(result["nodes"][4]["status"], "unreachable+isolated")
        self.assertEqual(result["nodes"][2]["status"], "conditional_only+one_way_sink")
        # Count component rows by their first column, not by arbitrary cell text.
        markdown = MODULE.render_markdown(result, 1)
        component_rows = [line for line in markdown.splitlines() if line.startswith("| 5 | ")]
        self.assertEqual(len(component_rows), 1)
        self.assertNotIn("| 3 | 2 |", markdown)
        self.assertIn("id,x,y,z,status,component", MODULE.render_csv(result))
        self.assertEqual(MODULE.render_csv(result), MODULE.render_csv(self.diagnose()))

    def test_diagnostic_cli_is_read_only(self):
        self.write()
        report_path = self.folder / "preliminary-connectivity.json"
        before = report_path.read_bytes()
        md = self.root / "issues.md"
        csv = self.root / "nodes.csv"
        self.assertEqual(MODULE.main([
            "--results", str(self.root), "--map", "de_dust2",
            "--output", str(md), "--csv", str(csv), "--limit", "1"
        ]), 0)
        self.assertEqual(report_path.read_bytes(), before)
        self.assertIn("Component (min ID)", md.read_text(encoding="utf-8"))
        self.assertEqual(len(csv.read_text(encoding="utf-8").splitlines()), 6)

    def test_rejects_unknown_endpoints_and_inconsistent_counters(self):
        report, candidates = record()
        report["edges"][0]["to"] = 999
        self.write(report, candidates)

        with self.assertRaisesRegex(ValueError, "unknown candidate"):
            self.diagnose()

        report, candidates = record()
        report["candidates_reachable_from_spawns"] = 3
        report["candidates_unreachable_from_spawns"] = 2
        self.write(report, candidates)

        with self.assertRaisesRegex(ValueError, "topology differs"):
            self.diagnose()

    def test_rejects_wrong_identity_and_invalid_source(self):
        report, candidates = record()
        candidates["artifact_sha256"] = "c" * 64
        self.write(report, candidates)

        with self.assertRaisesRegex(ValueError, "artifact_sha256 differs"):
            self.diagnose()

        report, candidates = record()
        candidates["candidates"][2]["position"][1] = float("nan")
        self.write(report, candidates)

        with self.assertRaisesRegex(ValueError, "finite XYZ"):
            self.diagnose()

    def test_missing_report_fails_without_creating_artifacts(self):
        errors = io.StringIO()

        with contextlib.redirect_stderr(errors):
            self.assertEqual(MODULE.main(["--results", str(self.root)]), 1)

        self.assertIn("preliminary-connectivity.json", errors.getvalue())
        self.assertFalse((self.folder / "navigation-candidates.json").exists())


if __name__ == "__main__":
    unittest.main()
