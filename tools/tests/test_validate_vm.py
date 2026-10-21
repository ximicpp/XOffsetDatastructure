"""Focused guard tests for the maintained validation runner (standard library only)."""
import importlib.util
import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest import mock

RUNNER = Path(__file__).resolve().parents[1] / "validate_vm.py"
spec = importlib.util.spec_from_file_location("validate_vm", RUNNER)
vm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vm)


class ValidationInputs(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def archive(self, entries, name="input.tar.gz"):
        path = self.root / name
        with tarfile.open(path, "w:gz") as bundle:
            for name, content in entries:
                member = tarfile.TarInfo(name)
                member.size = len(content)
                bundle.addfile(member, io.BytesIO(content))
        return path

    def source(self):
        source = self.root / "project"
        source.mkdir()
        (source / "CMakeLists.txt").write_text("cmake_minimum_required(VERSION 3.15)\n")
        (source / "xoffset_virtual_memory.hpp").write_text("// fixture\n")
        (source / "xoffsetdatastructure.hpp").write_text("// fixture\n")
        (source / "tests").mkdir()
        for target in vm.STORAGE:
            (source / "tests" / (target + ".cpp")).write_text("int main() { return 0; }\n")
        return source

    def invoke(self, *arguments, out=None):
        out = out or self.root / "result"
        run = subprocess.run([sys.executable, str(RUNNER), *map(str, arguments), "--out", str(out)],
                             capture_output=True, text=True)
        report = json.loads((out / "result.json").read_text())
        self.assertFalse((out / "SUCCESS").exists())
        return run, report

    def test_flat_and_wrapped_archives(self):
        for index, prefix in enumerate(("", "source/")):
            archive = self.archive([(prefix + "CMakeLists.txt", b"project(fixture)\n"),
                                    (prefix + "xoffset_virtual_memory.hpp", b"header")], f"{index}.tar.gz")
            destination = self.root / f"unpacked{index}"
            vm.extract_archive(archive, destination)
            self.assertEqual((vm.source_root(destination) / "xoffset_virtual_memory.hpp").read_bytes(), b"header")

    def test_traversal_absolute_windows_and_duplicate_members_rejected(self):
        for index, name in enumerate(("../escape", "/absolute", "C:/escape", "dir\\escape")):
            with self.subTest(name=name):
                archive = self.archive([(name, b"bad")], f"bad{index}.tar.gz")
                with self.assertRaises(vm.Unavailable):
                    vm.extract_archive(archive, self.root / f"bad{index}")
        archive = self.archive([("same", b"one"), ("same", b"two")], "duplicate.tar.gz")
        with self.assertRaises(vm.Unavailable):
            vm.extract_archive(archive, self.root / "duplicates")
        self.assertFalse((self.root / "escape").exists())

    def test_symlink_and_hardlink_members_rejected(self):
        for index, kind in enumerate((tarfile.SYMTYPE, tarfile.LNKTYPE)):
            archive = self.root / f"link{index}.tar"
            with tarfile.open(archive, "w") as bundle:
                member = tarfile.TarInfo("link")
                member.type, member.linkname = kind, "../escape"
                bundle.addfile(member)
            with self.assertRaises(vm.Unavailable):
                vm.extract_archive(archive, self.root / f"link{index}")

    def test_missing_dependency_is_unavailable_before_compilation(self):
        run, report = self.invoke("native", "--source-dir", self.source(), "--compiler", "does-not-exist")
        self.assertEqual(run.returncode, 2, run.stderr)
        self.assertEqual(report["status"], "unavailable")
        self.assertIn("--deps", report["reason"])
        self.assertFalse((self.root / "result/compiler.log").exists())

    def test_empty_dependency_tree_is_unavailable(self):
        deps = self.root / "deps"
        for component in vm.BOOST:
            (deps / "boost/libs" / component / "include").mkdir(parents=True)
        (deps / "typelayout/include").mkdir(parents=True)
        with self.assertRaisesRegex(vm.Unavailable, "no headers"):
            vm.dependency_manifest(deps, self.root)

    def test_dependencies_accept_c_header_components(self):
        deps = self.root / "deps"
        for component in vm.BOOST:
            directory = deps / "boost/libs" / component / "include/boost"
            directory.mkdir(parents=True)
            (directory / (component + ".h")).write_text("// C-style header fixture\n")
        layout = deps / "typelayout/include/boost"
        layout.mkdir(parents=True)
        (layout / "typelayout.hpp").write_text("// TypeLayout entry point\n")
        actual, _ = vm.dependency_manifest(deps, self.root)
        self.assertEqual(actual, deps.resolve())
        manifest = json.loads((self.root / "dependencies.json").read_text())
        self.assertIn("boost/libs/predef/include/boost/predef.h", manifest["files"])

    def test_missing_compiler_is_unavailable(self):
        run, report = self.invoke("native", "--suite", "storage", "--source-dir", self.source(),
                                  "--compiler", "no-such-validation-compiler")
        self.assertEqual(run.returncode, 2)
        self.assertEqual(report["status"], "unavailable")
        self.assertIn("--compiler", report["reason"])

    def test_msvc_driver_requires_separate_cmake_validation(self):
        run, report = self.invoke("native", "--suite", "storage", "--source-dir", self.source(),
                                  "--compiler", "cl.exe")
        self.assertEqual(run.returncode, 2)
        self.assertIn("standalone CMake/CI", report["reason"])

    def test_unexpected_inventory_errors_always_record_failed_status(self):
        source = self.source()
        for index, error in enumerate((AttributeError("bad inventory array"), KeyError("name"))):
            out = self.root / f"unexpected{index}"
            with mock.patch.object(vm, "cmake_suite", side_effect=error), contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                code = vm.main(["native", "--suite", "storage", "--source-dir", str(source), "--out", str(out)])
            self.assertEqual(code, 1)
            report = json.loads((out / "result.json").read_text())
            self.assertEqual(report["status"], "failed")
            self.assertIn(type(error).__name__, report["reason"])

    def test_linux_sanitizer_requires_explicit_runtime(self):
        run, report = self.invoke("linux", "--suite", "storage", "--source-dir", self.source(),
                                  "--sanitizer", "address-undefined")
        self.assertEqual(run.returncode, 2, run.stderr)
        self.assertIn("--runtime-dir", report["reason"])

    def runtime(self):
        runtime = self.root / "runtime"
        (runtime / "lib").mkdir(parents=True)
        (runtime / "include").mkdir()
        return runtime

    def test_empty_linux_runtime_is_unavailable_before_docker(self):
        run, report = self.invoke("linux", "--suite", "storage", "--source-dir", self.source(),
                                  "--sanitizer", "address-undefined", "--runtime-dir", self.runtime())
        self.assertEqual(run.returncode, 2, run.stderr)
        self.assertIn("directory is empty", report["reason"])
        self.assertFalse((self.root / "result/image-inspect.log").exists())

    def test_linux_runtime_rejects_external_directory_and_file_links(self):
        runtime = self.runtime()
        outside = self.root / "outside"
        outside.mkdir()
        (outside / "library.a").write_text("library")
        (runtime / "include/stddef.h").write_text("header")
        (runtime / "lib").rmdir()
        (runtime / "lib").symlink_to("../outside", target_is_directory=True)
        with self.assertRaisesRegex(vm.Unavailable, "escapes"):
            vm.linux_runtime_manifest(runtime)
        (runtime / "lib").unlink()
        (runtime / "lib").mkdir()
        (runtime / "lib/library.a").symlink_to("../../outside/library.a")
        with self.assertRaisesRegex(vm.Unavailable, "escapes"):
            vm.linux_runtime_manifest(runtime)

    def test_linux_runtime_hashes_internal_directory_links(self):
        runtime = self.runtime()
        (runtime / "actual-lib").mkdir()
        (runtime / "actual-lib/library.a").write_text("library")
        (runtime / "include/stddef.h").write_text("header")
        (runtime / "lib").rmdir()
        (runtime / "lib").symlink_to("actual-lib", target_is_directory=True)
        manifest = vm.linux_runtime_manifest(runtime)
        self.assertEqual(manifest["lib/library.a"]["sha256"], vm.sha(runtime / "actual-lib/library.a"))
        self.assertIn("include/stddef.h", manifest)
        (runtime / "lib/absolute.a").symlink_to(runtime / "actual-lib/library.a")
        with self.assertRaisesRegex(vm.Unavailable, "absolute runtime link"):
            vm.linux_runtime_manifest(runtime)

    def test_linux_runtime_rejects_directory_cycles(self):
        runtime = self.runtime()
        (runtime / "lib/loop").symlink_to("..", target_is_directory=True)
        with self.assertRaisesRegex(vm.Unavailable, "cyclic"):
            vm.linux_runtime_manifest(runtime)

    def test_nonsanitized_mode_rejects_irrelevant_runtime(self):
        run, report = self.invoke("native", "--suite", "storage", "--source-dir", self.source(),
                                  "--runtime-dir", self.root / "missing")
        self.assertEqual(run.returncode, 2)
        self.assertIn("only to sanitizer", report["reason"])

    def test_cmake_bundle_is_linux_only(self):
        run, report = self.invoke("native", "--suite", "storage", "--source-dir", self.source(),
                                  "--cmake-root", self.root / "missing")
        self.assertEqual(run.returncode, 2)
        self.assertIn("only to Linux", report["reason"])

    def test_cmake_bundle_requires_modules_and_no_links(self):
        bundle = self.root / "cmake"
        (bundle / "bin").mkdir(parents=True)
        for name in ("cmake", "ctest"):
            (bundle / "bin" / name).write_text("executable fixture")
        with self.assertRaisesRegex(vm.Unavailable, "Modules"):
            vm.cmake_bundle_manifest(bundle)
        modules = bundle / "share/cmake-4.2/Modules"
        modules.mkdir(parents=True)
        (modules / "CMake.cmake").write_text("# fixture")
        self.assertIn("bin/ctest", vm.cmake_bundle_manifest(bundle))
        (bundle / "bin/cmake").unlink()
        (bundle / "bin/cmake").symlink_to("ctest")
        with self.assertRaisesRegex(vm.Unavailable, "not symlinks"):
            vm.cmake_bundle_manifest(bundle)

    def test_archive_manifest_mismatch_is_failed(self):
        archive = self.archive([("CMakeLists.txt", b"project(fixture)\n"),
                                ("xoffset_virtual_memory.hpp", b"header")])
        expected = self.root / "expected.json"
        expected.write_text("{}")
        run, report = self.invoke("native", "--suite", "storage", "--source-archive", archive,
                                  "--source-manifest", expected)
        self.assertEqual(run.returncode, 1, run.stderr)
        self.assertEqual(report["status"], "failed")
        self.assertIn("manifest differs", report["reason"])

    def test_existing_output_is_preserved(self):
        out = self.root / "preserved"
        out.mkdir()
        (out / "result.json").write_text('{"status":"old-evidence"}')
        run, report = self.invoke("native", "--source-dir", self.source(), out=out)
        self.assertEqual(run.returncode, 2)
        self.assertEqual(report["status"], "old-evidence")

    def test_snapshot_cannot_recursively_copy_output(self):
        source = self.source()
        (source / "tools").mkdir()
        run, report = self.invoke("native", "--suite", "storage", "--source-dir", source,
                                  out=source / "tools" / "evidence")
        self.assertEqual(run.returncode, 2)
        self.assertIn("outside source directories", report["reason"])

    def test_preserved_source_manifest_matches_archive(self):
        # The follow-up F source archive was never published; Stage 2's preserved pair is.
        evidence = RUNNER.parents[1] / "docs/validation/2026-10-15-vm-stage2/final-source"
        archive = evidence / "source.tar.gz"
        vm.extract_archive(archive, self.root / "frozen")
        expected = json.loads((evidence / "source-manifest.json").read_text())
        self.assertEqual(vm.file_manifest(self.root / "frozen"), expected)

    def test_zero_skipped_failed_and_wrong_test_results_never_pass(self):
        path = self.root / "ctest.xml"
        for content in ("", '<testcase name="expected"><skipped/></testcase>',
                        '<testcase name="expected"><failure/></testcase>',
                        '<testcase name="expected" status="notrun"/>',
                        '<testcase name="different"/>'):
            with self.subTest(content=content):
                path.write_text("<testsuite>" + content + "</testsuite>")
                with self.assertRaises(vm.Failed):
                    vm.verify_ctest_results(path, [{"name": "expected"}])
        path.write_text('<testsuite><testcase name="expected"/></testsuite>')
        self.assertEqual(vm.verify_ctest_results(path, [{"name": "expected"}]), 1)
        path.write_text('<testsuite skipped="1"><testcase name="expected"/></testsuite>')
        with self.assertRaises(vm.Failed):
            vm.verify_ctest_results(path, [{"name": "expected"}])

    def test_extra_cmake_arguments_cannot_change_validation_claims(self):
        for argument in ("-DXOFFSET_STORAGE_ONLY=ON", "-DXOFFSET_STORAGE_ONLY:BOOL=ON",
                         "-DCMAKE_CXX_FLAGS:STRING=", "-DCMAKE_CXX_COMPILER=other",
                         "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_PROJECT_INCLUDE=inject.cmake",
                         "-DCMAKE_TOOLCHAIN_FILE=toolchain.cmake", "-DCTEST_CUSTOM_TESTS_IGNORE=test_vm_pool",
                         "-DVM_SOURCE=other", "-DBUILD_TESTING=OFF", "-UCMAKE_CXX_FLAGS",
                         "-Cinject.cmake", "--preset=other", "-DXOFFSET_CHECK_COMPILES=ON"):
            with self.subTest(argument=argument), self.assertRaises(vm.Unavailable):
                vm.validate_cmake_args([argument])
        vm.validate_cmake_args(["-DEXTERNAL_OPTION:BOOL=ON", "-DOTHER_DATA=a path with spaces"])

    def test_build_inventory_must_match_requested_mode_and_ctest(self):
        build = self.root / "build"
        build.mkdir()
        path = build / "test-inventory.json"
        inventory = {"mode": "storage-only", "ctest_count": 1,
                     "runtime_test_programs": ["test_vm_pool"]}
        path.write_text(json.dumps(inventory))
        with self.assertRaisesRegex(vm.Failed, "mode"):
            vm.verify_build_inventory(build, "full", [{"name": "test_vm_pool"}], self.root)
        with self.assertRaisesRegex(vm.Failed, "registered"):
            vm.verify_build_inventory(build, "storage", [{"name": "different"}], self.root)
        self.assertEqual(vm.verify_build_inventory(build, "storage", [{"name": "test_vm_pool"}], self.root), inventory)


if __name__ == "__main__":
    unittest.main()
