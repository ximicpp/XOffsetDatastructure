"""Small, offline integrity checks for the published handoff verifier."""

import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


RUNNER = Path(__file__).resolve().parents[1] / "verify_vm_handoff.py"
SPEC = importlib.util.spec_from_file_location("verify_vm_handoff", RUNNER)
vm = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(vm)


class HandoffVerification(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "project"
        (self.root / "handoff").mkdir(parents=True)
        (self.root / "nested").mkdir()
        self.payload = b"initial payload\n"
        self.file = self.root / "nested/source.hpp"
        self.file.write_bytes(self.payload)
        self.manifest = {
            "schema_version": 1,
            "git_base": "fixture-base",
            "handoff_branch": "codex/fixture",
            "unrelated_metadata": {"preserved": True},
            "files": {"nested/source.hpp": self.record(self.payload)},
        }
        self.save()

    def record(self, data):
        return {"sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data),
                "role": "source"}

    def save(self):
        (self.root / vm.MANIFEST).write_text(json.dumps(self.manifest), encoding="utf-8")

    def invoke(self, *args, optimized=False, runner=RUNNER):
        command = [sys.executable]
        if optimized:
            command.append("-O")
        command.extend([str(runner), *map(str, args)])
        return subprocess.run(command, cwd=self.temp.name, capture_output=True, text=True)

    def rejected(self):
        self.save()
        with self.assertRaises(vm.VerificationError):
            vm.verify(self.root)

    def symlink(self, link, target, directory=False):
        try:
            link.symlink_to(target, target_is_directory=directory)
        except (OSError, NotImplementedError) as error:
            self.skipTest("symlinks unavailable: {}".format(error))

    def test_valid_snapshot(self):
        manifest, size = vm.verify(self.root)
        self.assertEqual(size, len(self.payload))
        self.assertEqual(manifest["git_base"], "fixture-base")
        self.assertTrue(manifest["unrelated_metadata"]["preserved"])

    def test_optional_role_and_uppercase_digest(self):
        record = self.manifest["files"]["nested/source.hpp"]
        record.pop("role")
        record["sha256"] = record["sha256"].upper()
        self.save()
        self.assertEqual(vm.verify(self.root)[1], len(self.payload))

    def test_missing_file(self):
        self.file.unlink()
        self.rejected()

    def test_same_length_tampering(self):
        self.file.write_bytes(b"X" * len(self.payload))
        self.rejected()

    def test_size_mismatch(self):
        self.file.write_bytes(self.payload + b"extra")
        self.rejected()

    def test_invalid_digest_and_size(self):
        for field, value in (("sha256", "f" * 63), ("sha256", "g" * 64),
                             ("sha256", None), ("sha256", 12),
                             ("bytes", -1), ("bytes", True), ("bytes", 1.0),
                             ("bytes", "16"), ("bytes", None), ("role", 12)):
            with self.subTest(field=field, value=value):
                record = self.record(self.payload)
                record[field] = value
                self.manifest["files"] = {"nested/source.hpp": record}
                self.rejected()

    def test_invalid_schema_and_records(self):
        for field, value in (("schema_version", True), ("schema_version", 2),
                             ("schema_version", "1"), ("files", []), ("files", {}),
                             ("files", {"nested/source.hpp": []})):
            with self.subTest(field=field, value=value):
                old = self.manifest[field]
                self.manifest[field] = value
                self.rejected()
                self.manifest[field] = old

    def test_noncanonical_paths(self):
        for relative in ("../escape", "/absolute", "C:/escape", "C:escape",
                         "dir\\escape", "nested/../source.hpp", "./nested/source.hpp",
                         "nested//source.hpp", "nested/source.hpp/", "", ".",
                         "nested/source.hpp:stream", "bad\x00path"):
            with self.subTest(relative=relative):
                self.manifest["files"] = {relative: self.record(self.payload)}
                self.rejected()

    def test_manifest_cannot_list_itself(self):
        self.manifest["files"] = {vm.MANIFEST: self.record(b"anything")}
        self.rejected()

    def test_directory_is_not_a_regular_file(self):
        self.manifest["files"] = {"nested": self.record(b"")}
        self.rejected()

    def test_symlink_file(self):
        self.file.unlink()
        target = Path(self.temp.name) / "outside.hpp"
        target.write_bytes(self.payload)
        self.symlink(self.file, target)
        self.rejected()

    def test_symlink_ancestor(self):
        self.file.unlink()
        self.file.parent.rmdir()
        outside = Path(self.temp.name) / "outside"
        outside.mkdir()
        (outside / "source.hpp").write_bytes(self.payload)
        self.symlink(self.file.parent, outside, directory=True)
        self.rejected()

    def test_symlink_manifest(self):
        target = Path(self.temp.name) / "manifest.json"
        manifest_path = self.root / vm.MANIFEST
        manifest_path.rename(target)
        self.symlink(manifest_path, target)
        with self.assertRaises(vm.VerificationError):
            vm.verify(self.root)

    def test_invalid_json_and_duplicate_keys(self):
        for raw in ("{bad", "[]", '{"schema_version": 1, "schema_version": 1}',
                    '{"schema_version": NaN}',
                    '{"schema_version": 1, "files": {"x": {}, "x": {}}}'):
            with self.subTest(raw=raw):
                (self.root / vm.MANIFEST).write_text(raw, encoding="utf-8")
                result = self.invoke("--root", self.root)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("FAILED", result.stderr)
                self.assertNotIn("Traceback", result.stderr)

    def test_cli_from_unrelated_cwd_reports_metadata(self):
        result = self.invoke("--root", self.root)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("1 files, {} bytes".format(len(self.payload)), result.stdout)
        self.assertIn('git_base="fixture-base"', result.stdout)
        self.assertIn('handoff_branch="codex/fixture"', result.stdout)

    def test_cli_default_root_is_relative_to_script(self):
        tools = self.root / "tools"
        tools.mkdir()
        runner = tools / RUNNER.name
        shutil.copyfile(RUNNER, runner)
        result = self.invoke(runner=runner)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_cli_optimized_python_still_rejects_tampering(self):
        good = self.invoke("--root", self.root, optimized=True)
        self.assertEqual(good.returncode, 0, good.stderr)
        self.file.write_bytes(b"X" * len(self.payload))
        bad = self.invoke("--root", self.root, optimized=True)
        self.assertNotEqual(bad.returncode, 0)
        self.assertIn("SHA-256 mismatch", bad.stderr)
        self.assertIn("do not reset", bad.stderr)

    def test_many_errors_have_bounded_output(self):
        self.manifest["files"] = {
            "missing-{}.hpp".format(index): self.record(b"") for index in range(25)
        }
        self.save()
        result = self.invoke("--root", self.root)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("25 file error(s)", result.stderr)
        self.assertIn("15 more error(s) omitted", result.stderr)


if __name__ == "__main__":
    unittest.main()
