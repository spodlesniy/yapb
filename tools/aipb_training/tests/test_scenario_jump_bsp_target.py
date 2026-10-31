"""D311 BSP-derived jump target fixtures, without a real game installation."""


if __name__ == "__main__" and not __package__:
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
    __package__ = "tools.aipb_training.tests"
import csv
import hashlib
import io
import json
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

from tools.aipb_training.scenario_jump_bsp_target import (
    UnverifiedSurface, _artifact, _convex_hull, _has_standing_support,
    _point_contents, extract_surface_target, generate, main,
)
from tools.aipb_training.scenario_jump_target import read_target, classify_target
from tools.aipb_training.tests.test_scenario_jump_profile import run_fixture


def model(vertices=None, triangles=None):
    if vertices is None:
        vertices = [[105, 180, 0], [185, 180, 0], [185, 220, 0], [105, 220, 0]]
    if triangles is None:
        triangles = [[0, 1, 2], [0, 2, 3]]
    return {
        "meshes": [{"id": "1", "vertices": vertices, "triangles": triangles}],
        "entities": [{"type": "worldspawn", "mesh_id": "1", "origin": [0, 0, 0]}],
        "partition_planes": [{"type": 2, "normal": [0, 0, 1], "distance": 36}],
        "partition_clipnodes": [{"plane": 0, "front": -1, "back": -2}],
        "partition_models": [{"id": "1", "heads": [-1, 0, -1, -1]}],
    }


def landing():
    return (160, 200, 36)


class ExtractorTests(unittest.TestCase):
    def test_static_support_rectangle(self):
        surface = extract_surface_target(model(), landing())
        self.assertEqual(surface.model_id, "1")
        self.assertEqual(surface.origin_z, 36)
        self.assertGreater(surface.min_x, 105)
        self.assertLess(surface.max_x, 185)
        self.assertLess(surface.min_x, landing()[0])
        self.assertGreater(surface.max_x, landing()[0])
        self.assertGreater(surface.max_y - surface.min_y, 4)

    def test_clip_hull_is_required(self):
        source = model()
        source["partition_clipnodes"][0]["back"] = -1
        with self.assertRaises(UnverifiedSurface):
            extract_surface_target(source, landing())

    def test_invalid_models_and_ambiguous_support(self):
        with self.assertRaises(UnverifiedSurface):
            extract_surface_target(model(), (500, 200, 36))
        brush = model()
        brush["entities"][0]["type"] = "func_door"
        with self.assertRaises(UnverifiedSurface):
            extract_surface_target(brush, landing())
        duplicate = model()
        duplicate["meshes"].append({"id": "2", "vertices": duplicate["meshes"][0]["vertices"],
                                     "triangles": duplicate["meshes"][0]["triangles"]})
        duplicate["entities"].append(
            {"type": "func_wall", "mesh_id": "2", "origin": [0, 0, 0]})
        duplicate["partition_models"].append({"id": "2", "heads": [-1, 0, -1, -1]})
        with self.assertRaises(UnverifiedSurface):
            extract_surface_target(duplicate, landing())

    def test_nonconvex_or_overlapping_support_rejected(self):
        overlapping = model(triangles=[[0, 1, 2], [0, 2, 3], [0, 1, 2]])
        with self.assertRaises(UnverifiedSurface):
            extract_surface_target(overlapping, landing())

    def test_convex_hull_is_bounded(self):
        self.assertEqual(len(_convex_hull({(0, 0), (10, 0), (10, 10), (0, 10)})), 4)

    def test_generator_uses_observed_pose_not_manual_label(self):
        r = run_fixture("jump_runup_short_01")
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            bsp = folder / "de_dust2.bsp"
            bsp.write_bytes(b"fixture")
            with patch("tools.aipb_training.scenario_jump_bsp_target.subprocess.run") as run, \
                 patch("tools.aipb_training.scenario_jump_bsp_target._artifact",
                       return_value=model()):
                run.return_value.returncode = 0
                target_text = generate(r, bsp, folder / "aipb-map", "jump_runup_long_01")
            out = folder / "target.csv"
            out.write_text(target_text, encoding="utf-8")
            target = read_target(out)
            self.assertEqual(target.scenario, "jump_runup_long_01")
            self.assertEqual(target.map, "de_dust2")
            self.assertAlmostEqual(target.surface_z, 36)
            self.assertTrue(target.min_x < r.end.x < target.max_x)

    def test_reject_invalid_seed_or_map(self):
        r = run_fixture("jump_runup_short_01")
        with self.assertRaises(UnverifiedSurface):
            generate(replace(r, status="ActionTimeout"),
                     Path("de_dust2.bsp"), Path("aipb-map"), "jump_runup_short_01")
        with self.assertRaises(UnverifiedSurface):
            generate(r, Path("de_inferno.bsp"), Path("aipb-map"), "jump_runup_short_01")

    def test_cli_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory) / "target.csv"
            out.write_text("do not touch", encoding="utf-8")
            with patch("tools.aipb_training.scenario_jump_bsp_target.load_bundle"):
                self.assertEqual(main(["--bsp", "de_dust2.bsp", "--run", directory,
                                       "--map-tool", "aipb-map", "--scenario", "jump_box_plain_01",
                                       "--output", str(out)]), 2)
            self.assertEqual(out.read_text(), "do not touch")


class MapArtifactTests(unittest.TestCase):
    def test_bsp_hash_and_artifact_integrity(self):
        fixture = model()
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            bsp = folder / "de_dust2.bsp"
            bsp.write_bytes(b"synthetic-map")
            imported = folder / "de_dust2.aipbmap"
            imported.mkdir()
            name_map = {
                "meshes": "meshes.jsonl", "entities": "entities.jsonl",
                "partition_planes": "partition_planes.jsonl",
                "partition_clipnodes": "partition_clipnodes.jsonl",
                "partition_models": "partition_models.jsonl",
            }
            hashes = {}
            for key, name in name_map.items():
                data = "".join(json.dumps(row) + "\n" for row in fixture[key]).encode()
                (imported / name).write_bytes(data)
                hashes[name] = hashlib.sha256(data).hexdigest()
            manifest = {
                "format": "aipb-map",
                "schema_version": "0.3.0",
                "source_format": "goldsrc-bsp-v30",
                "source_sha256": hashlib.sha256(bsp.read_bytes()).hexdigest(),
                "has_partition": True,
                "file_sha256": hashes,
            }
            (imported / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            self.assertEqual(_artifact(imported, bsp), fixture)
            (imported / "meshes.jsonl").write_text("tampered", encoding="utf-8")
            with self.assertRaises(UnverifiedSurface):
                _artifact(imported, bsp)


if __name__ == "__main__":
    unittest.main()
