#!/usr/bin/env python3
"""Tests for optional iOS x64 simulator fixture build configuration."""
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
import build_wire_apple

class AppleBuildTests(unittest.TestCase):
    def test_optional_simulator_x64_uses_simulator_sdk_and_runner(self):
        with tempfile.TemporaryDirectory(prefix="xsg-apple-build-") as tmp:
            out = Path(tmp) / "out"
            args = ["build_wire_apple.py", "--clang", "clang", "--libcxx-include", "/headers",
                    "--resource-dir", "/resource", "--simulator", "device-id",
                    "--include-simulator-x64", "--out", str(out)]
            def run(cmd, **kwargs):
                Path(cmd[cmd.index("-o")+1]).write_text("fake")
                return SimpleNamespace(returncode=0, stdout="", stderr="")
            with mock.patch.object(sys, "argv", args), mock.patch.object(build_wire_apple.wire_inputs, "fingerprint", return_value=("f"*64, {})), mock.patch.object(build_wire_apple.build_evidence, "tool_inputs", return_value=[]), mock.patch.object(build_wire_apple.build_evidence, "prepare", return_value={"format":1,"build_id":"b"*64,"spec":{}}), mock.patch.object(build_wire_apple.verify, "includes", return_value=[]), mock.patch.object(build_wire_apple.subprocess, "check_output", side_effect=lambda cmd, **kwargs: "/sdk/" + cmd[2]), mock.patch.object(build_wire_apple.subprocess, "run", side_effect=run), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(build_wire_apple.main(), 0)
            profiles = json.loads((out / "profiles.json").read_text())
            target = profiles["targets"]["ios-simulator-x64"]
            self.assertEqual(target["runner"], ["xcrun", "simctl", "spawn", "device-id"])
            log = json.loads((out / "ios-simulator-x64.build.json").read_text())
            self.assertEqual(len(log), 2)
            self.assertIn("x86_64-apple-ios15.0-simulator", log[1]["command"])
            self.assertIn("/sdk/iphonesimulator", log[1]["command"])

    def test_explicit_macos_sdk_without_simulator(self):
        with tempfile.TemporaryDirectory(prefix="xsg-apple-build-") as tmp:
            out = Path(tmp) / "out"
            args = ["build_wire_apple.py", "--clang", "clang", "--libcxx-include", "/headers",
                    "--resource-dir", "/resource", "--macos-sdk", "/chosen/MacOSX26.5.sdk",
                    "--skip-ios-simulator", "--out", str(out)]
            def run(cmd, **kwargs):
                Path(cmd[cmd.index("-o")+1]).write_text("fake")
                return SimpleNamespace(returncode=0, stdout="", stderr="")
            def check_output(cmd, **kwargs):
                if "--sdk" in cmd: self.fail("explicit SDK must not be looked up")
                return "/tools/" + cmd[-1]
            with mock.patch.object(sys, "argv", args), mock.patch.object(build_wire_apple.wire_inputs, "fingerprint", return_value=("f"*64, {})), mock.patch.object(build_wire_apple.build_evidence, "tool_inputs", return_value=[]), mock.patch.object(build_wire_apple.build_evidence, "prepare", return_value={"format":1,"build_id":"b"*64,"spec":{}}) as prepare, mock.patch.object(build_wire_apple.verify, "includes", return_value=[]), mock.patch.object(build_wire_apple.subprocess, "check_output", side_effect=check_output), mock.patch.object(build_wire_apple.subprocess, "run", side_effect=run), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(build_wire_apple.main(), 0)
            profiles = json.loads((out / "profiles.json").read_text())
            self.assertEqual(set(profiles["targets"]), {"macos-arm64", "macos-x64"})
            for call in prepare.call_args_list:
                self.assertIn(Path("/chosen/MacOSX26.5.sdk"), call.args[3])

if __name__ == "__main__":
    unittest.main()
