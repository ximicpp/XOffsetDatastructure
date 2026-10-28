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
    def test_android_calls_share_one_budget_and_still_clean_up(self):
        timeouts=[]
        clock=[0.0]
        def invoke(cmd,**kwargs):
            timeouts.append((cmd,kwargs["timeout"]))
            clock[0]+=30
            return SimpleNamespace(returncode=0,stdout="",stderr="")
        argv=["adb_wire_runner.py","--serial","device1","--timeout","50","--",str(self.binary),"--xsg-identify"]
        with mock.patch.object(sys,"argv",argv), \
             mock.patch.object(adb_wire_runner.time,"monotonic",side_effect=lambda:clock[0]), \
             mock.patch.object(adb_wire_runner.subprocess,"run",side_effect=invoke), \
             contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(adb_wire_runner.main(),1)
        self.assertEqual([t for _,t in timeouts[:2]],[50,20])
        self.assertEqual(timeouts[-1][0][-3:-1],["rm","-rf"])
        self.assertEqual(timeouts[-1][1],10)
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
