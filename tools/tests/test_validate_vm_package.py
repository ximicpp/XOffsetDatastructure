#!/usr/bin/env python3
"""Regression checks for package validation's fail-closed evidence audit."""
import importlib.util
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("package_validator", Path(__file__).resolve().parents[1] / "validate_vm_package.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class PackageAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        self.prefix = self.root / "prefix"
        self.source.mkdir()
        (self.source / "LICENSE").write_text("test license")
        (self.source / "xoffset_virtual_memory.hpp").write_text("test header")
        files = {"include/xoffset_virtual_memory.hpp": "test header",
                 "share/XOffsetDatastructure/licenses/LICENSE": "test license",
                 "lib/cmake/XOffsetDatastructure/XOffsetDatastructureConfig.cmake": "# config",
                 "lib/cmake/XOffsetDatastructure/XOffsetTargets.cmake": 'INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include"'}
        for name, content in files.items():
            path = self.prefix / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)

    def audit(self):
        return MODULE.audit_install(self.prefix, self.source, "storage", [self.source])

    def test_complete_storage_package(self):
        self.assertEqual(self.audit()["file_count"], 4)

    def test_missing_license_rejected(self):
        (self.prefix / "share/XOffsetDatastructure/licenses/LICENSE").unlink()
        with self.assertRaisesRegex(ValueError, "Missing or changed"):
            self.audit()

    def test_changed_header_rejected(self):
        (self.prefix / "include/xoffset_virtual_memory.hpp").write_text("different")
        with self.assertRaisesRegex(ValueError, "Missing or changed"):
            self.audit()

    def test_unexpected_files_rejected(self):
        (self.prefix / "include/accidental-private-header.hpp").touch()
        with self.assertRaisesRegex(ValueError, "Unexpected or missing"):
            self.audit()

    def test_source_and_absolute_interface_leaks_rejected(self):
        target = self.prefix / "lib/cmake/XOffsetDatastructure/XOffsetTargets.cmake"
        for path in (str(self.source), "/opt/toolchain/lib", "C:/toolchain/include"):
            with self.subTest(path=path):
                target.write_text('INTERFACE_INCLUDE_DIRECTORIES "' + path + '"')
                with self.assertRaises(ValueError):
                    self.audit()

    def test_ctest_failure_skip_empty_and_inventory_rejected(self):
        path = self.root / "tests.xml"
        valid = '<testsuite tests="3"><testcase name="consumer_transfer"/><testcase name="consumer_pool_handle_before_owner"/><testcase name="consumer_static_destruction"/></testsuite>'
        path.write_text(valid)
        self.assertEqual(MODULE.check_ctest_xml(path), 3)
        for invalid in ('<testsuite/>', valid.replace('tests="3"', 'tests="3" skipped="1"'),
                        valid.replace('tests="3"', 'tests="3" disabled="1"'),
                        valid.replace('name="consumer_transfer"', 'name="consumer_transfer" status="notrun"'),
                        valid.replace('name="consumer_transfer"', 'name="consumer_transfer" status="disabled"'),
                        valid.replace('consumer_transfer', 'unexpected'),
                        valid.replace('<testcase name="consumer_transfer"/>', '<testcase name="consumer_transfer"><failure/></testcase>')):
            with self.subTest(xml=invalid):
                path.write_text(invalid)
                with self.assertRaises(ValueError):
                    MODULE.check_ctest_xml(path)

    def test_output_inside_copied_tree_rejected(self):
        for directory in ("cmake", "tests", "tools", "examples", "include", "external"):
            with self.subTest(directory=directory):
                with self.assertRaisesRegex(ValueError, "outside copied source"):
                    MODULE.validate_output_location(self.source, self.source / directory / "output")
        MODULE.validate_output_location(self.source, self.source / "build/output")

    def test_missing_library_records_failed_result(self):
        out = self.root / "bad-library-output"
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(MODULE.main(["--source-dir", str(self.source), "--out", str(out),
                                         "--suite", "storage", "--library-dir", str(self.root / "missing")]), 1)
        self.assertEqual(json.loads((out / "result.json").read_text())["status"], "failed")

    def test_changed_package_cannot_be_marked_complete(self):
        expected = MODULE.manifest(self.prefix)
        MODULE.require_unchanged(self.prefix, expected, "Relocated package")
        (self.prefix / "include/xoffset_virtual_memory.hpp").write_text("changed after build")
        with self.assertRaisesRegex(ValueError, "Relocated package changed"):
            MODULE.require_unchanged(self.prefix, expected, "Relocated package")


if __name__ == "__main__":
    unittest.main()
