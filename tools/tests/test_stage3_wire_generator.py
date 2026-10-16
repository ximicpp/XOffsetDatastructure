import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import stage3_candidates_wire as generator


class WireGeneratorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (Path(__file__).resolve().parents[2] / "xoffsetdatastructure.hpp").read_text()

    def test_variants_keep_core_and_donor_predicate(self):
        before_core, after_core = self.source.split("    namespace fixed_detail {", 1)
        for variant, flag in (("A-wire", "false"), ("B-wire", "true")):
            with self.subTest(variant=variant):
                output = generator.transform(self.source, variant)
                self.assertEqual(output.split("    namespace fixed_detail {", 1)[0], before_core)
                self.assertIn(f"constexpr bool high_tail = {flag};", output)
                self.assertEqual(output.count("inline void* stage3_try_split_wire("), 1)
                self.assertIn("if (block->block_bytes >= aligned_bytes &&\n"
                              "                                reinterpret_cast<std::uintptr_t>(block) % alignment == 0)", output)
                self.assertIn("padding > advertised - request - hsize", output)
                self.assertIn("residual_advertised > advertised - (residual_address - address)", output)
                self.assertIn(after_core[after_core.index("        inline bool try_expand_dynamic_bytes"):], output)

    def test_rejects_unsupported_and_repeated_transform(self):
        with self.assertRaises(ValueError):
            generator.transform(self.source, "F")
        with self.assertRaises(ValueError):
            generator.transform(generator.transform(self.source, "A-wire"), "B-wire")

    def test_rejects_missing_and_duplicated_anchors(self):
        for anchor in (generator._FUNCTION_ANCHOR, generator._REUSE_ANCHOR):
            for damaged in (self.source.replace(anchor, "", 1), self.source + anchor):
                with self.subTest(anchor=anchor[:60], count=damaged.count(anchor)):
                    with self.assertRaises(ValueError):
                        generator.transform(damaged, "A-wire")


if __name__ == "__main__":
    unittest.main()
