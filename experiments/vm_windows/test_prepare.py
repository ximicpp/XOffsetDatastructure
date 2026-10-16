#!/usr/bin/env python3
"""Lightweight safety/result checks. Does not configure, compile or run C++."""
from __future__ import annotations

import contextlib
import io
import json
import os
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest import mock

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import bundle_support as support
import prepare
import validate_windows as runner

ROOT = Path(__file__).resolve().parents[2]
FROZEN = ROOT / "docs/validation/2026-10-15-vm-stage2/final-source"


class HandoffTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def archive(self, entries):
        path = self.root / "input.tar.gz"
        with tarfile.open(path, "w:gz") as tar:
            for name, data, kind in entries:
                item = tarfile.TarInfo(name)
                item.type = kind
                item.size = len(data) if kind == tarfile.REGTYPE else 0
                if kind in (tarfile.SYMTYPE, tarfile.LNKTYPE):
                    item.linkname = "../../outside"
                tar.addfile(item, io.BytesIO(data))
        return path

    def manifest(self, name="file.txt", data=b"ok"):
        return {name: {"sha256": support.digest(data), "bytes": len(data)}}

    def test_frozen_prepare_exact_and_small(self):
        out = self.root / "handoff"
        result = prepare.prepare(FROZEN / "source.tar.gz", FROZEN / "source-manifest.json", out)
        self.assertEqual(result["source_file_count"], 121)
        self.assertEqual(result["source_bytes"], 1173446)
        self.assertEqual(support.verify_bundle(out), result)
        self.assertFalse((out / "source/external").exists())
        self.assertFalse((out / "source/build").exists())
        self.assertEqual(support.digest((out / "source/xoffset_virtual_memory.hpp").read_bytes()), support.F_OS_SHA256)

    def test_existing_out_untouched(self):
        out = self.root / "existing"
        out.mkdir()
        marker = out / "marker"
        marker.write_text("keep")
        with self.assertRaisesRegex(ValueError, "new directory"):
            prepare.prepare(FROZEN / "source.tar.gz", FROZEN / "source-manifest.json", out)
        self.assertEqual(marker.read_text(), "keep")

    @unittest.skipIf(os.name == "nt", "Creating symlinks may require Windows developer privileges")
    def test_dangling_output_link_rejected(self):
        out = self.root / "link"
        out.symlink_to(self.root / "missing")
        with self.assertRaisesRegex(ValueError, "new directory"):
            prepare.prepare(FROZEN / "source.tar.gz", FROZEN / "source-manifest.json", out)
        self.assertTrue(out.is_symlink())

    def test_wrong_manifest_rejected_before_write(self):
        manifest = self.root / "manifest.json"
        manifest.write_text("{}")
        out = self.root / "out"
        with self.assertRaisesRegex(ValueError, "frozen Stage II"):
            prepare.prepare(FROZEN / "source.tar.gz", manifest, out)
        self.assertFalse(out.exists())

    def test_wrong_archive_rejected_before_write(self):
        archive = self.archive([("file.txt", b"ok", tarfile.REGTYPE)])
        out = self.root / "out"
        with self.assertRaisesRegex(ValueError, "frozen Stage II"):
            prepare.prepare(archive, FROZEN / "source-manifest.json", out)
        self.assertFalse(out.exists())

    def test_wrong_f_header_rejected_independently(self):
        data = json.dumps(self.manifest("xoffset_virtual_memory.hpp", b"changed-F")).encode()
        with mock.patch.object(support, "MANIFEST_SHA256", support.digest(data)):
            with self.assertRaisesRegex(ValueError, "F OS header"):
                support.frozen_manifest(data)

    def test_paths_are_safe_on_windows(self):
        for name in ("../a", "/a", "a/../b", "a//b", "C:/x", "a\\b", "a/CON.cpp",
                     "a/NUL", "a/COM1.txt", "a/file ", "a/file.", "a:stream", "external/a", "build/a", ".git/config"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                support.safe_name(name)

    def test_links_directories_and_devices_rejected(self):
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE, tarfile.DIRTYPE, tarfile.CHRTYPE, tarfile.FIFOTYPE):
            with self.subTest(kind=kind):
                path = self.archive([("file.txt", b"", kind)])
                with self.assertRaisesRegex(ValueError, "ordinary"):
                    support.archive_files(path, self.manifest())

    def test_duplicate_members_rejected(self):
        path = self.archive([("file.txt", b"ok", tarfile.REGTYPE)] * 2)
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            support.archive_files(path, self.manifest())

    def test_case_insensitive_manifest_collision(self):
        manifest = self.manifest()
        manifest.update(self.manifest("FILE.txt"))
        with self.assertRaisesRegex(ValueError, "collision"):
            support.validate_manifest(manifest)

    def test_hash_size_missing_and_extra_rejected(self):
        for entries in ([('file.txt', b'no', tarfile.REGTYPE)],
                        [('file.txt', b'toolong', tarfile.REGTYPE)],
                        [], [('extra.txt', b'ok', tarfile.REGTYPE)]):
            with self.subTest(entries=entries), self.assertRaises(ValueError):
                support.archive_files(self.archive(entries), self.manifest())

    def test_valid_generic_archive(self):
        path = self.archive([("file.txt", b"ok", tarfile.REGTYPE)])
        self.assertEqual(support.archive_files(path, self.manifest()), {"file.txt": b"ok"})

    def test_duplicate_json_key_rejected(self):
        with self.assertRaisesRegex(ValueError, "Duplicate JSON"):
            support.read_json(b'{"file": 1, "file": 2}')

    def test_size_budget_rejected(self):
        manifest = self.manifest()
        manifest["file.txt"]["bytes"] = support.MAX_BYTES + 1
        with self.assertRaisesRegex(ValueError, "size limit"):
            support.validate_manifest(manifest)

    def test_bundle_changes_rejected(self):
        out = self.root / "handoff"
        prepare.prepare(FROZEN / "source.tar.gz", FROZEN / "source-manifest.json", out)
        (out / "source/xoffset_virtual_memory.hpp").write_bytes(b"new header")
        with self.assertRaisesRegex(ValueError, "Source tree changed"):
            support.verify_bundle(out)

    def test_inventory_empty_duplicate_and_wrong_rejected(self):
        configured = {"mode": "storage-only", "ctest_count": 2, "runtime_test_program_count": 2,
                      "runtime_test_programs": sorted(runner.STORAGE_TESTS)}
        good = {"tests": [{"name": name} for name in sorted(runner.STORAGE_TESTS)]}
        runner.check_inventory(configured, good)
        for discovered in ({"tests": []}, {"tests": good["tests"] * 2}, {"tests": [{"name": "other"}]}):
            with self.subTest(discovered=discovered), self.assertRaises(ValueError):
                runner.check_inventory(configured, discovered)
        with self.assertRaises(ValueError):
            runner.check_inventory({**configured, "mode": "full"}, good)

    def test_junit_requires_executed_exact_cases(self):
        names = sorted(runner.STORAGE_TESTS)
        good = ''.join('<testcase name="' + name + '" status="run" />' for name in names)
        path = self.root / "test.xml"
        path.write_text('<testsuite failures="0">' + good + '</testsuite>')
        self.assertEqual(runner.check_junit(path, runner.STORAGE_TESTS), 2)
        for cases in ('', good * 2, good.replace('status="run"', 'status="notrun"'),
                      good.replace('/>', '><skipped /></testcase>', 1),
                      good.replace('/>', '><failure /></testcase>', 1)):
            with self.subTest(cases=cases):
                path.write_text('<testsuite>' + cases + '</testsuite>')
                with self.assertRaises(ValueError):
                    runner.check_junit(path, runner.STORAGE_TESTS)

    @unittest.skipIf(os.name == "nt", "This is the non-Windows negative control")
    def test_non_windows_run_fails_and_preserves_record(self):
        out = self.root / "results"
        bundle = self.root / "bundle"
        bundle.mkdir()
        with contextlib.redirect_stderr(io.StringIO()), mock.patch.object(runner.subprocess, "run") as run:
            result = runner.main(["--bundle", str(bundle), "--out", str(out)])
        self.assertEqual(result, 1)
        self.assertEqual(json.loads((out / "result.json").read_text())["status"], "failed")
        run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
