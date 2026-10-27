"""Regression checks for the D210 offline connectivity audit."""

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "audit_navigation.py"
SPEC = importlib.util.spec_from_file_location("audit_navigation", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
import sys
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def sample(name="de_test", candidates=5, reachable=4, unconditional=2):
    return {
        "format": "aipb-preliminary-connectivity",
        "schema_version": "0.9.0",
        "map_name": name,
        "candidate_count": candidates,
        "edge_count": 2,
        "edges": [{"id": 1}, {"id": 2}],
        "spawn_anchors": 1,
        "candidates_reachable_from_spawns": reachable,
        "candidates_reachable_unconditionally": unconditional,
        "candidates_unreachable_from_spawns": candidates - reachable,
        "isolated_candidates": 1,
        "strong_components": 3,
        "unconditional_strong_components": 4,
        "weak_components": 2,
        "largest_strong_component": 2,
        "one_way_sink_components": 1,
        "one_way_sink_candidates": 2,
        "conditional_edges": 1,
        "conditional_door_edges": 1,
        "jump_edges": 0,
        "crouch_jump_edges": 0,
        "drop_edges": 0,
    }


class AuditTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def put(self, name, data=None, report=True):
        directory = self.root / name
        artifact = directory / (name + ".aipbmap")
        artifact.mkdir(parents=True, exist_ok=True)
        (artifact / "manifest.json").write_text("{}", encoding="utf-8")

        if report:
            (directory / "preliminary-connectivity.json").write_text(
                json.dumps(data if data is not None else sample(name)), encoding="utf-8"
            )

    def test_deterministic_ranking_and_conditions(self):
        self.put("de_first", sample("de_first", 5, 2, 1))
        self.put("de_second", sample("de_second", 5, 4, 2))
        reports, errors = MODULE.inspect_results(self.root)
        self.assertEqual(errors, [])
        self.assertEqual([x.name for x in reports], ["de_first", "de_second"])
        self.assertEqual(reports[1].conditional_only, 2)
        text = MODULE.render_audit(reports, errors)
        self.assertIn("reachable only via conditional edges", text)
        self.assertEqual(text, MODULE.render_audit(reports, errors))
        out = self.root / "audit.md"
        self.assertEqual(MODULE.main(["--results", str(self.root), "--output", str(out)]), 0)
        self.assertIn("de_first", out.read_text(encoding="utf-8"))

    def test_missing_and_invalid_are_errors(self):
        self.put("cs_missing", report=False)
        corrupt = sample("de_bad")
        corrupt["candidates_reachable_unconditionally"] = 6
        self.put("de_bad", corrupt)
        reports, errors = MODULE.inspect_results(self.root)
        self.assertEqual(reports, [])
        self.assertEqual(len(errors), 2)
        self.assertEqual(MODULE.main(["--results", str(self.root)]), 1)

    def test_invalid_schema_and_wrong_map(self):
        wrong = sample("de_wrong")
        wrong["schema_version"] = "0.8.0"
        self.put("de_wrong", wrong)
        with self.assertRaisesRegex(ValueError, "schema"):
            MODULE.parse_report(self.root / "de_wrong" / "preliminary-connectivity.json", "de_wrong")
        wrong["schema_version"] = "0.9.0"
        wrong["map_name"] = "different"
        self.put("de_wrong", wrong)
        with self.assertRaisesRegex(ValueError, "different map"):
            MODULE.parse_report(self.root / "de_wrong" / "preliminary-connectivity.json", "de_wrong")

    def test_single_map_filter(self):
        self.put("de_dust2", sample("de_dust2"))
        self.put("cs_other", report=False)
        reports, errors = MODULE.inspect_results(self.root, "de_dust2")
        self.assertEqual(errors, [])
        self.assertEqual([item.name for item in reports], ["de_dust2"])
        self.assertEqual(MODULE.main(["--results", str(self.root), "--map", "de_dust2"]), 0)
        reports, errors = MODULE.inspect_results(self.root, "cs_other")
        self.assertEqual(reports, [])
        self.assertEqual(errors, ["cs_other: missing preliminary-connectivity.json"])
        reports, errors = MODULE.inspect_results(self.root, "de_absent")
        self.assertEqual(reports, [])
        self.assertEqual(errors, ["No imported map artifact found: de_absent"])
        with self.assertRaisesRegex(ValueError, "invalid map name"):
            MODULE.inspect_results(self.root, "../de_dust2")

    def test_empty_directory(self):
        reports, errors = MODULE.inspect_results(self.root)
        self.assertEqual(reports, [])
        self.assertEqual(errors, ["No imported map artifacts found"])

    def test_no_anchors_does_not_fabricate_reachability(self):
        report = sample("cs_no_spawn", 5, 0, 0)
        report["spawn_anchors"] = 0
        self.put("cs_no_spawn", report)
        parsed, errors = MODULE.inspect_results(self.root)
        self.assertFalse(errors)
        self.assertIn("no covered spawn anchors", MODULE.render_audit(parsed, errors))


if __name__ == "__main__":
    unittest.main()
