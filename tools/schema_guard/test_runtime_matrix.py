#!/usr/bin/env python3
"""Orchestration tests using fake binaries; these are not platform evidence."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import runtime_matrix
import wire_inputs

HERE=Path(__file__).resolve().parent

class RuntimeMatrixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base=Path(tempfile.mkdtemp(prefix="xsg-runtime-tests-"))
        cls.fingerprint=wire_inputs.fingerprint()[0]
        print("test artifacts:",cls.base,flush=True)
    def fake(self,name,target,fingerprint,missing=False):
        path=self.base/name
        files=sorted(runtime_matrix.EXPECTED_CORPUS)
        if missing: files.pop()
        source=("import pathlib,sys\n"
                +f"target={target!r}; fingerprint={fingerprint!r}; files={files!r}\n"
                +"if sys.argv[1]=='--xsg-identify': print('XSG_RUNTIME_V1',target,'64','le',fingerprint)\n"
                +"elif sys.argv[1]=='--write':\n p=pathlib.Path(sys.argv[2]); p.mkdir(parents=True,exist_ok=True)\n"
                +" for f in files: (p/f).write_bytes(f.encode())\n"
                +"elif sys.argv[1]=='--read':\n p=pathlib.Path(sys.argv[2]); sys.exit(0 if all((p/f).read_bytes()==f.encode() for f in files) else 1)\n")
        path.write_text("#!"+sys.executable+"\n"+source); path.chmod(0o755)
        return str(path)
    def run_matrix(self,name,targets):
        profile=self.base/(name+".json")
        profile.write_text(json.dumps({"reference":"macos-arm64","targets":targets}))
        out=self.base/name
        q=subprocess.run([sys.executable,str(HERE/"runtime_matrix.py"),"--profiles",str(profile),"--out",str(out)],capture_output=True,text=True)
        return q,json.loads((out/"result.json").read_text())
    def test_missing_targets_block_and_valid_pair_passes(self):
        ref=self.fake("reference","macos-arm64",self.fingerprint)
        other=self.fake("other","macos-x64",self.fingerprint)
        q,r=self.run_matrix("partial",{"macos-arm64":{"binary":ref},"macos-x64":{"binary":other}})
        self.assertEqual(q.returncode,2)
        self.assertEqual(r["targets"]["macos-arm64"]["status"],"PASS_REFERENCE")
        self.assertEqual(r["targets"]["macos-x64"]["status"],"PASS_WIRE")
        self.assertEqual(r["targets"]["windows-x64"]["status"],"BLOCKED_RUNTIME")
    def test_complete_fake_matrix_passes(self):
        targets = {}
        for index, name in enumerate(runtime_matrix.verify.TARGETS):
            targets[name] = {"binary": self.fake("all-"+str(index), name, self.fingerprint)}
            if name.startswith("windows-"): targets[name]["runtime_kind"]="windows"
        q, r = self.run_matrix("all-fake", targets)
        self.assertEqual(q.returncode, 0, q.stderr)
        self.assertEqual(r["status"], "PASS")
        self.assertEqual(r["targets"]["macos-arm64"]["status"], "PASS_REFERENCE")
        self.assertTrue(all(v["status"] == "PASS_WIRE" for k, v in r["targets"].items() if k != "macos-arm64"))

    def test_wine_is_not_a_native_windows_gate_pass(self):
        targets = {}
        for index, name in enumerate(runtime_matrix.verify.TARGETS):
            targets[name] = {"binary": self.fake("compat-"+str(index), name, self.fingerprint)}
            if name.startswith("windows-"): targets[name]["runtime_kind"]="windows"
        targets["windows-arm64"]["runtime_kind"]="wine"
        q,r=self.run_matrix("wine-compat", targets)
        self.assertEqual(q.returncode,2)
        self.assertEqual(r["targets"]["windows-arm64"]["status"],"PASS_WIRE_WINE")
        self.assertEqual(r["status"],"FAIL")

    def test_windows_requires_runtime_kind(self):
        ref=self.fake("ref-kind","macos-arm64",self.fingerprint)
        target=self.fake("missing-kind","windows-x64",self.fingerprint)
        q,r=self.run_matrix("missing-kind-test",{"macos-arm64":{"binary":ref},"windows-x64":{"binary":target}})
        self.assertEqual(q.returncode,2)
        self.assertEqual(r["targets"]["windows-x64"]["status"],"BLOCKED_CONFIG")

    def test_stale_source_fingerprint_is_rejected(self):
        ref=self.fake("ref-stale","macos-arm64","stale")
        q,r=self.run_matrix("stale",{"macos-arm64":{"binary":ref}})
        self.assertEqual(q.returncode,2)
        self.assertEqual(r["targets"]["macos-arm64"]["status"],"FAIL_IDENTITY")
    def test_missing_case_cannot_pass(self):
        ref=self.fake("ref-incomplete","macos-arm64",self.fingerprint,True)
        q,r=self.run_matrix("incomplete",{"macos-arm64":{"binary":ref}})
        self.assertEqual(q.returncode,2)
        self.assertEqual(r["targets"]["macos-arm64"]["status"],"FAIL_WIRE")
if __name__=="__main__":
    unittest.main(verbosity=2)
