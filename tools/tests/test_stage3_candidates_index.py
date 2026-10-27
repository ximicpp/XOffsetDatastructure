"""Generated candidates must refuse stale or ambiguous frozen F anchors."""
import hashlib
import importlib.util
from pathlib import Path
import tarfile
import unittest


SPEC = importlib.util.spec_from_file_location(
    "stage3_candidates_index", Path(__file__).parents[1] / "stage3_candidates_index.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)

# The generators target the frozen F header (docs/VM_STAGE3_RESULTS.md, archived, see docs/ARCHIVED_EVIDENCE.md), not the live one.
FROZEN_F_ARCHIVE = (Path(__file__).resolve().parents[2]
                    / "docs/validation/2026-10-15-vm-followup/wire-inputs-and-versions.tar.gz")
FROZEN_F_SHA256 = "9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2"


def frozen_f_source():
    with tarfile.open(FROZEN_F_ARCHIVE) as bundle:
        data = bundle.extractfile("final/xoffsetdatastructure.hpp").read()
    if hashlib.sha256(data).hexdigest() != FROZEN_F_SHA256:
        raise AssertionError("frozen F header hash mismatch")
    return data.decode("utf-8")



class IndexCandidateGenerationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = frozen_f_source()

    def test_both_variants_are_deterministic_and_isolated(self):
        for variant, enabled in (("C-linear", "false"), ("C-index", "true")):
            with self.subTest(variant=variant):
                result = MODULE.transform(self.source, variant)
                self.assertEqual(result, MODULE.transform(self.source, variant))
                self.assertIn(f"Stage3FreeSlots<{enabled}> free_blocks_;", result)
                self.assertNotIn("std::vector<FreeBlock> free_blocks_;", result)
                self.assertIn("free_blocks_.find_first(bytes, alignment, buffer_.data())", result)
                self.assertEqual(self.source, frozen_f_source())

    def test_unknown_variant_rejected(self):
        with self.assertRaisesRegex(ValueError, "unknown Stage 3"):
            MODULE.transform(self.source, "C-best-fit")

    def test_missing_and_duplicate_anchors_rejected(self):
        for anchor in (MODULE._FREE_BLOCK, MODULE._PRUNE, MODULE._LOOKUP,
                       "        std::vector<FreeBlock> free_blocks_;"):
            for damaged in (self.source.replace(anchor, "", 1), self.source + anchor):
                with self.subTest(anchor=anchor[:50]):
                    with self.assertRaisesRegex(ValueError, "expected one F source anchor"):
                        MODULE.transform(damaged, "C-index")

    def test_generated_input_cannot_be_transformed_again(self):
        transformed = MODULE.transform(self.source, "C-linear")
        with self.assertRaisesRegex(ValueError, "expected one F source anchor"):
            MODULE.transform(transformed, "C-index")


if __name__ == "__main__":
    unittest.main()
