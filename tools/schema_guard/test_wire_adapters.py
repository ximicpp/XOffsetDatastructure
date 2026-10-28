#!/usr/bin/env python3
"""Argument and transport construction tests; no device or container is simulated as evidence."""
import contextlib
import io
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
import adb_wire_runner
import docker_wire_runner

class AdapterTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix="xsg-adapter-test-")
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name).resolve()
        self.binary=self.root/"fixture"
        self.binary.write_text("fixture")
    def test_android_identity_and_unique_cleanup(self):
        calls=[]
        def invoke(cmd,**kwargs):
            calls.append(cmd)
            return SimpleNamespace(returncode=0,stdout="",stderr="")
        argv=["adb_wire_runner.py","--serial","device1","--",str(self.binary),"--xsg-identify"]
        with mock.patch.object(sys,"argv",argv), mock.patch.object(adb_wire_runner.subprocess,"run",side_effect=invoke):
            self.assertEqual(adb_wire_runner.main(),0)
        self.assertTrue(any("push" in c for c in calls))
        self.assertTrue(any("rm" in c and "-rf" in c and c[-1].startswith("/data/local/tmp/xsg-") for c in calls))
        self.assertTrue(all(c[1:3]==["-s","device1"] for c in calls))
    def test_android_transfers_output(self):
        out=self.root/"out"
        calls=[]
        def invoke(cmd,**kwargs):
            calls.append(cmd)
            return SimpleNamespace(returncode=0,stdout="",stderr="")
        argv=["adb_wire_runner.py","--serial","device1","--",str(self.binary),"--write",str(out)]
        with mock.patch.object(sys,"argv",argv), mock.patch.object(adb_wire_runner.subprocess,"run",side_effect=invoke):
            self.assertEqual(adb_wire_runner.main(),0)
        self.assertTrue(out.is_dir())
        self.assertTrue(any("pull" in c for c in calls))
    def test_docker_mounts_fixture_and_corpus_parent(self):
        path=self.root/"corpus"
        argv=["docker_wire_runner.py","--image","image@sha256:"+"a"*64,
              "--platform","linux/arm64","--",str(self.binary),"--write",str(path)]
        with mock.patch.object(sys,"argv",argv), mock.patch.object(docker_wire_runner.subprocess,"run",return_value=SimpleNamespace(returncode=0)) as run:
            self.assertEqual(docker_wire_runner.main(),0)
        cmd=run.call_args.args[0]
        self.assertIn(str(self.binary)+":/xsg/fixture:ro",cmd)
        self.assertIn(str(self.root)+":/xsg/data",cmd)
        self.assertEqual(cmd[-2:],["--write","/xsg/data/corpus"])

if __name__=="__main__":
    unittest.main(verbosity=2)
