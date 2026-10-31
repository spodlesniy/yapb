"""Guard direct-file Python execution without relying on the current directory."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
PACKAGE = ROOT / "tools" / "aipb_training"

# Each is an ordinary runnable CLI that must work from outside the checkout.
CLI = (
    "dataset_quality.py", "dataset_stats.py", "deploy.py", "evaluation.py",
    "export.py", "ground_active_braking_profile.py", "ground_motion_profile.py",
    "ground_obstacle_profile.py", "ground_slope_forward_profile.py",
    "ground_turn_speed_profile.py", "scenario_jump_bsp_target.py",
    "scenario_jump_label_audit.py", "scenario_jump_profile.py",
    "scenario_jump_target.py", "scenario_movement_outcome.py",
    "scenario_trace_compare.py", "train.py", "validate_dataset.py",
)


class PythonEntrypointTests(unittest.TestCase):
    def test_all_package_files_compile(self):
        import ast
        for file in sorted(PACKAGE.rglob("*.py")):
            with self.subTest(file=file.relative_to(ROOT)):
                ast.parse(file.read_text(encoding="utf-8"), filename=str(file))

    def test_direct_cli_help_from_foreign_directory(self):
        with tempfile.TemporaryDirectory() as workdir:
            env = os.environ.copy()
            env.pop("PYTHONPATH", None)
            for name in CLI:
                with self.subTest(name=name):
                    result = subprocess.run(
                        [sys.executable, str(PACKAGE / name), "--help"],
                        cwd=workdir, env=env, capture_output=True, text=True,
                        timeout=25, check=False,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn("usage:", result.stdout.lower())

    def test_direct_test_module_from_foreign_directory(self):
        with tempfile.TemporaryDirectory() as workdir:
            env = os.environ.copy()
            env.pop("PYTHONPATH", None)
            for name in ("test_scenario_jump_target.py", "test_dataset_quality.py"):
                with self.subTest(name=name):
                    result = subprocess.run(
                        [sys.executable, str(PACKAGE / "tests" / name), "--help"],
                        cwd=workdir, env=env, capture_output=True, text=True,
                        timeout=25, check=False,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn("usage:", result.stdout.lower())


if __name__ == "__main__" and not __package__:
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
    __package__ = "tools.aipb_training.tests"


if __name__ == "__main__":
    unittest.main()
