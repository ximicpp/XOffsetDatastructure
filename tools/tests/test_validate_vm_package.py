#!/usr/bin/env python3
"""Regression checks for package validation's fail-closed evidence audit."""
import importlib.util
import contextlib
import io
import json
from pathlib import Path
import shutil
import subprocess
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
        (self.source / "cmake").mkdir()
        (self.source / "docs").mkdir()
        (self.source / "XOFFSET_VERSION").write_text("0.1.0-alpha.1\n", encoding="utf-8", newline="\n")
        (self.source / "CHANGELOG.md").write_text("alpha release notes\n", encoding="utf-8", newline="\n")
        (self.source / "docs/ALPHA_COMPATIBILITY.md").write_text("no cross-release compatibility promise\n", encoding="utf-8", newline="\n")
        self.project = Path(__file__).resolve().parents[2]
        for name in ("XOffsetVersion.cmake", "XOffsetDatastructureConfigVersion.cmake.in",
                     "XOffsetDatastructureConfig.cmake.in"):
            shutil.copy2(self.project / "cmake" / name, self.source / "cmake" / name)
        (self.source / "LICENSE").write_text("test license")
        (self.source / "xoffset_virtual_memory.hpp").write_text("test header")
        (self.source / "xoffset_slot_storage.hpp").write_text("test slot header")
        self.version = MODULE.read_version(self.source)
        config = "\n".join("set(" + name + " " + value + ")" for name, value in
                           MODULE.version_config_assertions(self.version).items()) + "\n"
        producer = self.source.parent / "producer-build"
        producer.mkdir()
        (producer / "XOffsetDatastructureConfig.cmake").write_text(config, encoding="utf-8", newline="\n")
        files = {"include/xoffset_virtual_memory.hpp": "test header",
                 "include/xoffset_slot_storage.hpp": "test slot header",
                 "share/XOffsetDatastructure/licenses/LICENSE": "test license",
                 "share/XOffsetDatastructure/VERSION": (self.source / "XOFFSET_VERSION").read_text(encoding="utf-8"),
                 "share/XOffsetDatastructure/CHANGELOG.md": (self.source / "CHANGELOG.md").read_text(encoding="utf-8"),
                 "share/XOffsetDatastructure/ALPHA_COMPATIBILITY.md": (self.source / "docs/ALPHA_COMPATIBILITY.md").read_text(encoding="utf-8"),
                 "lib/cmake/XOffsetDatastructure/XOffsetDatastructureConfig.cmake": config,
                 "lib/cmake/XOffsetDatastructure/XOffsetDatastructureConfigVersion.cmake":
                     (self.source / "cmake/XOffsetDatastructureConfigVersion.cmake.in").read_text(encoding="utf-8").replace("@XOFFSET_VERSION_FULL@", self.version["full"]),
                 "lib/cmake/XOffsetDatastructure/XOffsetTargets.cmake": 'INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include"'}
        for name, content in files.items():
            path = self.prefix / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8", newline="\n")

    def audit(self):
        return MODULE.audit_install(self.prefix, self.source, "storage", [self.source])

    def test_complete_storage_package(self):
        audit = self.audit()
        self.assertEqual(audit["file_count"], 9)
        self.assertEqual(audit["version"], self.version)

    def test_required_release_metadata_is_exact(self):
        for name in ("VERSION", "CHANGELOG.md", "ALPHA_COMPATIBILITY.md"):
            with self.subTest(name=name):
                path = self.prefix / "share/XOffsetDatastructure" / name
                original = path.read_bytes()
                path.write_text("changed", encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "Missing or changed"):
                    self.audit()
                path.unlink()
                with self.assertRaisesRegex(ValueError, "Missing or changed"):
                    self.audit()
                path.write_bytes(original)

    def test_config_and_configversion_tampering_rejected(self):
        directory = self.prefix / "lib/cmake/XOffsetDatastructure"
        for name, before, after in (("XOffsetDatastructureConfig.cmake", "alpha.1", "alpha.2"),
                                    ("XOffsetDatastructureConfigVersion.cmake", "PACKAGE_VERSION_COMPATIBLE FALSE", "PACKAGE_VERSION_COMPATIBLE TRUE")):
            with self.subTest(name=name):
                path = directory / name
                original = path.read_bytes()
                path.write_text(original.decode("utf-8").replace(before, after), encoding="utf-8", newline="\n")
                with self.assertRaisesRegex(ValueError, "Missing or changed"):
                    self.audit()
                path.write_bytes(original)

    def test_version_metadata_mismatch_in_configured_and_installed_copy_rejected(self):
        for path in (self.prefix / "lib/cmake/XOffsetDatastructure/XOffsetDatastructureConfig.cmake",
                     self.source.parent / "producer-build/XOffsetDatastructureConfig.cmake"):
            path.write_text(path.read_text(encoding="utf-8").replace("alpha.1", "alpha.2"), encoding="utf-8", newline="\n")
        with self.assertRaisesRegex(ValueError, "metadata differs from VERSION"):
            self.audit()

    @unittest.skipUnless(shutil.which("cmake"), "CMake required for real package discovery semantics")
    def test_cmake_numeric_exact_and_range_requests_rejected_but_full_identity_queries_pass(self):
        # Generate the actual two templates with CMake. No compiler or native
        # program is run; Threads is a fixture because this tests version lookup.
        package = self.root / "version-package"
        package.mkdir()
        generation = self.root / "generate.cmake"
        generation.write_text('include(CMakePackageConfigHelpers)\n'
            'include("' + (self.source / "cmake/XOffsetVersion.cmake").as_posix() + '")\n'
            'set(XOFFSET_HAS_FULL OFF)\n'
            'configure_package_config_file("' + (self.source / "cmake/XOffsetDatastructureConfig.cmake.in").as_posix() + '" "' + (package / "XOffsetDatastructureConfig.cmake").as_posix() + '" INSTALL_DESTINATION lib/cmake/XOffsetDatastructure)\n'
            'configure_file("' + (self.source / "cmake/XOffsetDatastructureConfigVersion.cmake.in").as_posix() + '" "' + (package / "XOffsetDatastructureConfigVersion.cmake").as_posix() + '" @ONLY NEWLINE_STYLE LF)\n', encoding="utf-8")
        generated = subprocess.run(["cmake", "-P", str(generation)], capture_output=True, text=True)
        self.assertEqual(generated.returncode, 0, generated.stdout + generated.stderr)
        (package / "XOffsetTargets.cmake").write_text("# No targets required for metadata lookup\n", encoding="utf-8")
        consumer = self.root / "version-consumer"
        consumer.mkdir()
        (consumer / "FindThreads.cmake").write_text("set(Threads_FOUND TRUE)\n", encoding="utf-8")
        content = MODULE.version_probe_source(self.version).replace("LANGUAGES CXX", "LANGUAGES NONE")
        content = content.replace("find_package(XOffset", 'list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")\nfind_package(XOffset')
        (consumer / "CMakeLists.txt").write_text(content, encoding="utf-8")
        requests = ("", "0.1.0", "999.0.0", "0.1.0;EXACT", "0.1.0...<999.0.0")
        for index, request in enumerate(requests):
            with self.subTest(request=request):
                run = subprocess.run(["cmake", "-S", str(consumer), "-B", str(self.root / ("request-" + str(index))),
                    "-DXOFFSET_COMPONENT=storage", "-DXOffsetDatastructure_DIR=" + str(package),
                    "-DXOFFSET_VERSION_REQUEST=" + request], capture_output=True, text=True)
                output = run.stdout + run.stderr
                if request:
                    self.assertNotEqual(run.returncode, 0, output)
                    self.assertIn("not accepted", output)
                    self.assertIn(self.version["full"], output)
                else:
                    self.assertEqual(run.returncode, 0, output)
                    self.assertIn("XOFFSET_FULL_VERSION=" + self.version["full"], output)

    @unittest.skipUnless(shutil.which("cmake"), "CMake required for shared VERSION parser checks")
    def test_python_and_cmake_version_parsers_agree_and_reject_malformed_identifiers(self):
        script = self.root / "version.cmake"
        script.write_text('include("' + (self.source / "cmake/XOffsetVersion.cmake").as_posix() + '")\n'
                          'message(STATUS "${XOFFSET_VERSION_FULL}|${XOFFSET_VERSION_NUMERIC}|${XOFFSET_VERSION_PRERELEASE}")\n', encoding="utf-8")
        valid = subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True)
        self.assertEqual(valid.returncode, 0, valid.stdout + valid.stderr)
        self.assertIn("0.1.0-alpha.1|0.1.0|alpha.1", valid.stdout)
        (self.source / "XOFFSET_VERSION").write_text("1.0.0\n", encoding="utf-8")
        self.assertEqual(MODULE.read_version(self.source), {"full": "1.0.0", "numeric": "1.0.0", "prerelease": ""})
        release = subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True)
        self.assertEqual(release.returncode, 0, release.stdout + release.stderr)
        self.assertIn("1.0.0|1.0.0|\n", release.stdout + "\n")
        for version in ("1.0", "1.0.0-", "1.0.0-gamma.1", "0.1.0-alpha", "00.1.0-alpha.1", "0.1.0-alpha.01",
                        "0.1.0-alpha.1\n0.1.0-alpha.2"):
            with self.subTest(version=version):
                (self.source / "XOFFSET_VERSION").write_text(version, encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "at most one prerelease identifier"):
                    MODULE.read_version(self.source)
                invalid = subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True)
                self.assertNotEqual(invalid.returncode, 0)
                self.assertIn("VERSION must be", invalid.stdout + invalid.stderr)

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
