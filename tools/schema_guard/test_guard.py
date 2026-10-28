#!/usr/bin/env python3
'''Integration checks for the compile-only gate; set XSG_TEST_CLANG.'''
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
CLANG = os.environ.get("XSG_TEST_CLANG")
@unittest.skipUnless(CLANG, "set XSG_TEST_CLANG to the P2996 wrapper")
class CompileGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base = Path(tempfile.mkdtemp(prefix="xsg-gate-test-"))
        cls.catalog = (HERE / "catalog.hpp").read_text()
        print("test artifacts:", cls.base, flush=True)
    def gate(self, name, catalog, extra=()):
        c = self.base / (name + ".hpp")
        c.write_text(catalog)
        out = self.base / name
        cmd = [sys.executable, str(HERE / "verify.py"), "--clang", CLANG,
               "--catalog", str(c), "--out", str(out), "--target", "macos-arm64", *extra]
        p = subprocess.run(cmd, text=True, capture_output=True)
        return p, json.loads((out / "result.json").read_text())
    def test_correct_fixture(self):
        p, r = self.gate("valid", self.catalog)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(r["targets"]["macos-arm64"]["status"], "PASS_COMPILE")
        self.assertEqual(r["targets"]["linux-x64"]["status"], "NOT_RUN")
    def test_wire_pin_mismatch(self):
        changed = self.catalog.replace("@72:i16}", "@72:i32}", 1)
        self.assertNotEqual(changed, self.catalog)
        p, r = self.gate("wire_bad", changed)
        self.assertNotEqual(p.returncode, 0)
        self.assertEqual(r["targets"]["macos-arm64"]["status"], "FAIL_LAYOUT")
    def test_typelayout_mismatch(self):
        changed = self.catalog.replace("[64-le]record[s:80,a:8]", "[64-le]record[s:81,a:8]", 1)
        self.assertNotEqual(changed, self.catalog)
        p, r = self.gate("type_bad", changed)
        self.assertNotEqual(p.returncode, 0)
        self.assertEqual(r["targets"]["macos-arm64"]["status"], "FAIL_LAYOUT")
    def test_semantic_mismatch(self):
        changed = self.catalog.replace("epoch|title|items|keys", "title|epoch|items|keys", 1)
        self.assertNotEqual(changed, self.catalog)
        p, r = self.gate("semantic_bad", changed)
        self.assertNotEqual(p.returncode, 0)
        self.assertEqual(r["targets"]["macos-arm64"]["status"], "FAIL_SEMANTIC")
    def test_root_field_list_required(self):
        changed = self.catalog + "\n#undef XSG_RECORD_FIELDS\n#define XSG_RECORD_FIELDS(X) X(pin::Inner, \"id|active|name\")\n"
        p, r = self.gate("root_fields_bad", changed)
        self.assertNotEqual(p.returncode, 0)
        self.assertEqual(r["targets"]["macos-arm64"]["status"], "FAIL_SEMANTIC")
    def test_empty_catalog_rejected(self):
        changed = self.catalog + "\n#undef XSG_SCHEMA_ROOTS\n#define XSG_SCHEMA_ROOTS(X)\n"
        p, r = self.gate("empty_bad", changed)
        self.assertNotEqual(p.returncode, 0)
        self.assertEqual(r["targets"]["macos-arm64"]["status"], "FAIL_COMPILE")
    def test_wrong_target_rejected(self):
        profiles = self.base / "profiles.json"
        profiles.write_text(json.dumps({"macos-arm64":{"kind":"host","compiler":CLANG,
            "flags":["-std=c++26","-freflection","-fexpansion-statements","-stdlib=libc++",
                     "--target=x86_64-apple-macos11"]}}))
        p, r = self.gate("target_bad", self.catalog, ("--profiles", str(profiles)))
        self.assertNotEqual(p.returncode, 0)
        self.assertEqual(r["targets"]["macos-arm64"]["status"], "FAIL_COMPILE")
        self.assertIn("target mismatch", (self.base / "target_bad" / "macos-arm64.log").read_text())
if __name__ == "__main__":
    unittest.main(verbosity=2)
