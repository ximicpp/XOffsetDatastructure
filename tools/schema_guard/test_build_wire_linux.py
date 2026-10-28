#!/usr/bin/env python3
"""Check Docker fixture build orchestration without claiming platform evidence."""
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
import build_wire_linux

class LinuxBuildTests(unittest.TestCase):
    def test_failed_build_never_reuses_old_target_profiles(self):
        with tempfile.TemporaryDirectory(prefix="xsg-linux-test-") as d:
            base=Path(d)
            profiles=base/"base.json"
            profiles.write_text(json.dumps({"reference":"macos-arm64","targets":{
                "linux-x64":{"binary":"stale"},"linux-arm64":{"binary":"stale"},
                "macos-arm64":{"binary":"reference"}}}))
            out=base/"out"
            argv=["build_wire_linux.py","--out",str(out),"--docker","missing-docker","--base-profiles",str(profiles)]
            with mock.patch.object(sys,"argv",argv), mock.patch.object(build_wire_linux.wire_inputs,"fingerprint",return_value=("fingerprint",{})), mock.patch.object(build_wire_linux.subprocess,"run",side_effect=FileNotFoundError("missing")):
                self.assertEqual(build_wire_linux.main(),2)
            result=json.loads((out/"profiles.json").read_text())
            self.assertNotIn("linux-x64",result["targets"])
            self.assertNotIn("linux-arm64",result["targets"])
            self.assertIn("macos-arm64",result["targets"])
            report=json.loads((out/"build-result.json").read_text())
            self.assertEqual(report["linux-x64"]["status"],"FAIL_BUILD")
            self.assertEqual(report["linux-arm64"]["status"],"FAIL_BUILD")
if __name__=="__main__":
    unittest.main(verbosity=2)
