"""Source-only safety tests for the isolated Stage III Core generator."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest


_SCRIPT = Path(__file__).resolve().parents[1] / "stage3_candidates_core.py"
_SPEC = importlib.util.spec_from_file_location("stage3_candidates_core", _SCRIPT)
assert _SPEC is not None and _SPEC.loader is not None
_MODULE = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(_MODULE)


class CoreGeneratorTests(unittest.TestCase):
    def test_preserves_unrelated_source(self) -> None:
        source = "before\n" + _MODULE._CORE_REUSE_LOOP + "after\n"
        for variant in ("A-core", "B-core"):
            with self.subTest(variant=variant):
                result = _MODULE.transform(source, variant)
                self.assertTrue(result.startswith("before\n"))
                self.assertTrue(result.endswith("after\n"))
                self.assertNotIn("__TAIL_SPLIT__", result)
                self.assertIn("block.user_bytes >= bytes && block.alignment >= alignment", result)
                self.assertIn("if (reinterpret_cast<std::uintptr_t>(ptr) % alignment != 0) continue;", result)
                self.assertEqual(result.count("free_blocks_.erase("), 1)
                self.assertNotIn("push_back", result)

    def test_tail_strategy_is_isolated(self) -> None:
        source = _MODULE._CORE_REUSE_LOOP
        self.assertNotIn("tail_header{", _MODULE.transform(source, "A-core"))
        self.assertIn("tail_header{", _MODULE.transform(source, "B-core"))

    def test_rejects_absent_repeated_and_already_transformed_anchor(self) -> None:
        for source in ("", _MODULE._CORE_REUSE_LOOP * 2,
                       _MODULE.transform(_MODULE._CORE_REUSE_LOOP, "A-core")):
            with self.subTest(source_length=len(source)):
                with self.assertRaisesRegex(ValueError, "anchor count"):
                    _MODULE.transform(source, "B-core")

    def test_rejects_unknown_variant(self) -> None:
        with self.assertRaisesRegex(ValueError, "unsupported"):
            _MODULE.transform(_MODULE._CORE_REUSE_LOOP, "A-wire")


if __name__ == "__main__":
    unittest.main()
