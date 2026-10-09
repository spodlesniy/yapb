#!/usr/bin/env python3
"""Regression guard for the crlib-first include order required by MSVC x86."""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
INCLUDE = re.compile(r"^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]", re.MULTILINE)


def misplaced_yapb_include(source: str) -> bool:
    """Only files directly importing yapb.h have the ordering constraint."""
    headers = INCLUDE.findall(source)
    return "yapb.h" in headers and headers[0] != "yapb.h"


# The game DLL is not built by the normal AI-only unit test job.
# Keep engine-independent guards explicitly visible in their production users,
# rather than relying on headers included by unrelated source files.
PERCEPTION_HELPERS = ("suppressPreciseBlindAim", "shouldKeepCurrentVisibleEnemy")


def missing_perception_guard_include(source: str) -> bool:
    uses_guard = any(
        re.search(r"\bai::" + re.escape(name) + r"\s*\(", source)
        for name in PERCEPTION_HELPERS
    )
    return uses_guard and "ai/ai_perception_guard.h" not in INCLUDE.findall(source)


class YapbFirstIncludeTests(unittest.TestCase):
    def test_production_translation_units_have_yapb_first(self) -> None:
        files = sorted((ROOT / "src").rglob("*.cpp"))
        self.assertTrue(files, "repository src/*.cpp files must be present")
        errors = [
            str(path.relative_to(ROOT))
            for path in files
            if misplaced_yapb_include(path.read_text(encoding="utf-8"))
        ]
        self.assertFalse(errors, "yapb.h must be first include (MSVC x86): " + ", ".join(errors))

    def test_production_task_priority_references_are_declared(self) -> None:
        # AI unit tests do not compile the full game library on Windows x86.
        header = (ROOT / "inc" / "constant.h").read_text(encoding="utf-8")
        block = re.search(r"namespace\s+TaskPri\s*\{(.*?)\n\};", header, re.DOTALL)
        self.assertIsNotNone(block, "TaskPri namespace was not found")
        declarations = set(re.findall(r"constexpr\s+auto\s+(\w+)\s*\{", block.group(1)))
        usages: set[str] = set()
        for path in (ROOT / "src").rglob("*.cpp"):
            usages.update(re.findall(r"\bTaskPri::(\w+)", path.read_text(encoding="utf-8")))
        self.assertFalse(usages - declarations,
                         "Undefined TaskPri symbols: " + ", ".join(sorted(usages - declarations)))

    def test_perception_guard_users_include_their_defining_header(self) -> None:
        missing = [
            str(path.relative_to(ROOT))
            for path in (ROOT / "src").rglob("*.cpp")
            if missing_perception_guard_include(path.read_text(encoding="utf-8"))
        ]
        self.assertFalse(
            missing, "Direct <ai/ai_perception_guard.h> include missing: " + ", ".join(missing)
        )

    def test_perception_include_regression_detection(self) -> None:
        usage = '#include <yapb.h>\nvoid f() { ai::suppressPreciseBlindAim(1.0f); }\n'
        self.assertTrue(missing_perception_guard_include(usage))
        self.assertFalse(
            missing_perception_guard_include(
                '#include <yapb.h>\n#include <ai/ai_perception_guard.h>\n'
                'void f() { ai::suppressPreciseBlindAim(1.0f); }\n'
            )
        )
        self.assertFalse(missing_perception_guard_include('#include <yapb.h>\n'))

    def test_dataset_writer_crlib_first_for_msvc_x86(self) -> None:
        # Including standard/AI headers first can define placement new before crlib.
        path = ROOT / "src" / "ai" / "ai_training_dataset.cpp"
        source = path.read_text(encoding="utf-8")
        headers = INCLUDE.findall(source)
        self.assertTrue(headers, "dataset writer must include the crlib umbrella header")
        self.assertEqual(headers[0], "yapb.h",
                         "dataset writer must initialize crlib before AI/STL headers (MSVC C2084/C3615)")
        self.assertRegex(
            source,
            r"(?m)^#if defined\(_MSC_VER\)\n#include <yapb\.h>\n#endif$",
            "MSVC requires crlib first, while strict Linux unit tests must skip yapb.h",
        )
        self.assertIn("ai/ai_training_dataset.h", headers)
        self.assertIn("cstdio", headers)

    def test_recent_windows_x86_regression_contracts(self) -> None:
        yapb_header = (ROOT / "inc" / "yapb.h").read_text(encoding="utf-8")
        defuse_header = (ROOT / "inc" / "ai" / "ai_defuse_event.h").read_text(encoding="utf-8")
        combat_source = (ROOT / "src" / "combat.cpp").read_text(encoding="utf-8")
        self.assertIn("#include <ai/ai_defuse_event.h>", yapb_header)
        self.assertIn("class DefuseApproachDiagnosticGate final", defuse_header)
        self.assertRegex(
            combat_source, r"\bauto\s+currentEnemyParts\s*=\s*m_enemyParts\s*;"
        )

    def test_catches_standard_and_local_headers_before_yapb(self) -> None:
        self.assertTrue(misplaced_yapb_include('#include <cstring>\n#include <yapb.h>\n'))
        self.assertTrue(misplaced_yapb_include('#include <ai/ai_perception_guard.h>\n#include <yapb.h>\n'))
        self.assertFalse(misplaced_yapb_include('#include <yapb.h>\n#include <cstring>\n'))
        self.assertFalse(misplaced_yapb_include('#include <ai/ai_goal_navigation_policy.h>\n'))


if __name__ == "__main__":
    unittest.main()
