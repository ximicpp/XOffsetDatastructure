import hashlib
import sys
import tarfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import stage3_candidates_wire as generator

# The generators target the frozen F header (docs/VM_STAGE3_RESULTS.md), not the live one.
FROZEN_F_ARCHIVE = (Path(__file__).resolve().parents[2]
                    / "docs/validation/2026-10-15-vm-followup/wire-inputs-and-versions.tar.gz")
FROZEN_F_SHA256 = "9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2"


def frozen_f_source():
    with tarfile.open(FROZEN_F_ARCHIVE) as bundle:
        data = bundle.extractfile("final/xoffsetdatastructure.hpp").read()
    if hashlib.sha256(data).hexdigest() != FROZEN_F_SHA256:
        raise AssertionError("frozen F header hash mismatch")
    return data.decode("utf-8")



class WireGeneratorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = frozen_f_source()

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
