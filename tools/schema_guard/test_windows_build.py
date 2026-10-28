#!/usr/bin/env python3
"""Failure handling tests for the Windows wire object and linker helpers."""
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import build_wire_windows
import link_wire_windows
import prepare_windows_runtime

class WindowsBuildTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="xsg-windows-build-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def test_runtime_archive_checksum_mismatch_fails_before_extraction(self):
        cache = self.root / "cache"
        cache.mkdir()
        (cache / "mingw-w64-clang-x86_64-crt-1-any.pkg.tar.zst").write_bytes(b"bad archive")
        args = ["prepare_windows_runtime.py", "--zstd", "zstd", "--offline",
                "--archive-dir", str(cache), "--out", str(self.root / "prepared")]
        packages = {"windows-x64": ("clang64", [("crt-1", "0" * 64)])}
        with mock.patch.object(sys, "argv", args), mock.patch.object(prepare_windows_runtime, "PACKAGES", packages), mock.patch.object(prepare_windows_runtime.subprocess, "Popen") as popen:
            with self.assertRaises(ValueError):
                prepare_windows_runtime.main()
            popen.assert_not_called()

    def test_failed_backend_does_not_report_build_pass(self):
        profile = self.root / "profiles.json"
        profile.write_text(json.dumps({
            "windows-x64": {"kind": "host", "compiler": "clang",
                            "flags": ["--target=x86_64-w64-windows-gnu"]},
            "windows-arm64": {"kind": "host", "compiler": "clang",
                              "flags": ["--target=aarch64-w64-windows-gnu"]}}))
        out = self.root / "out"
        def run(cmd, **kwargs):
            if cmd[0] == "xcrun":
                return SimpleNamespace(returncode=1, stdout="", stderr="backend failed")
            Path(cmd[-1]).write_text("ir")
            return SimpleNamespace(returncode=0, stdout="", stderr="")
        args = ["build_wire_windows.py", "--profiles", str(profile), "--out", str(out)]
        with mock.patch.object(sys, "argv", args), mock.patch.object(build_wire_windows.wire_inputs, "fingerprint", return_value=("f"*64, {})), mock.patch.object(build_wire_windows.subprocess, "check_output", return_value="/backend"), mock.patch.object(build_wire_windows.build_evidence, "tool_inputs", return_value=[]), mock.patch.object(build_wire_windows.build_evidence, "prepare", return_value={"build_id":"f"*64}), mock.patch.object(build_wire_windows.subprocess, "run", side_effect=run), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(build_wire_windows.main(), 2)
        data = json.loads((out / "build-result.json").read_text())
        self.assertEqual(data["windows-x64"]["status"], "FAIL_BUILD")
        self.assertEqual(data["windows-arm64"]["status"], "FAIL_BUILD")

    def test_failed_link_does_not_report_success_or_binary_hash(self):
        objects = self.root / "objects"
        objects.mkdir()
        for name in ("windows-x64", "windows-arm64"):
            (objects / (name + ".o")).write_bytes(b"object")
            import build_evidence
            evidence=build_evidence.prepare(name,"fingerprint",[["clang"]],images=["gcc@sha256:"+"a"*64])
            build_evidence.finish(evidence,objects/(name+".o"),objects/(name+".object.manifest.json"))
        roots = [self.root / "x64", self.root / "arm64"]
        for root in roots:
            (root / "lib/clang/23/lib/windows").mkdir(parents=True)
        out = self.root / "out"
        args = ["link_wire_windows.py", "--clang", "clang", "--objects", str(objects),
                "--sysroot-x64", str(roots[0]), "--sysroot-arm64", str(roots[1]), "--out", str(out)]
        failure = SimpleNamespace(returncode=1, stdout="", stderr="link failed")
        with mock.patch.object(sys, "argv", args), mock.patch.object(link_wire_windows.build_evidence, "prepare", return_value={"build_id":"f"*64}), mock.patch.object(link_wire_windows.subprocess, "run", return_value=failure), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(link_wire_windows.main(), 2)
        data = json.loads((out / "link-result.json").read_text())
        self.assertEqual(data["windows-x64"]["status"], "FAIL_LINK")
        self.assertIsNone(data["windows-x64"]["binary_sha256"])

if __name__ == "__main__":
    unittest.main()
