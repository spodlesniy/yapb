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

    def test_catches_standard_and_local_headers_before_yapb(self) -> None:
        self.assertTrue(misplaced_yapb_include('#include <cstring>\n#include <yapb.h>\n'))
        self.assertTrue(misplaced_yapb_include('#include <ai/ai_perception_guard.h>\n#include <yapb.h>\n'))
        self.assertFalse(misplaced_yapb_include('#include <yapb.h>\n#include <cstring>\n'))
        self.assertFalse(misplaced_yapb_include('#include <ai/ai_goal_navigation_policy.h>\n'))


if __name__ == "__main__":
    unittest.main()
