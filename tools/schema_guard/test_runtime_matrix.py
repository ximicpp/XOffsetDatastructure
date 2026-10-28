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
import build_evidence

HERE=Path(__file__).resolve().parent

class RuntimeMatrixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base=Path(tempfile.mkdtemp(prefix="xsg-runtime-tests-"))
        cls.fingerprint=wire_inputs.fingerprint()[0]
        print("test artifacts:",cls.base,flush=True)
    def fake(self,name,target,fingerprint,missing=False,environment=None):
        path=self.base/name
        files=sorted(runtime_matrix.EXPECTED_CORPUS)
        if missing: files.pop()
        tool=self.base/(name+".tool"); tool.write_text("toolchain")
        evidence=build_evidence.prepare(target,fingerprint,[["test-compiler","-O1"]],[tool])
        env=environment or ("windows" if target.startswith("windows-") else ("ios-simulator" if target.startswith("ios-simulator-") else target.rsplit("-",1)[0]))
        source=("import pathlib,sys\n"
                +f"target={target!r}; fingerprint={fingerprint!r}; files={files!r}; environment={env!r}; build_id={evidence['build_id']!r}\n"
                +"if sys.argv[1]=='--xsg-identify': print('XSG_RUNTIME_V2',target,'64','le',fingerprint,environment,build_id)\n"
                +"elif sys.argv[1]=='--write':\n p=pathlib.Path(sys.argv[2]); p.mkdir(parents=True,exist_ok=True)\n"
                +" for f in files: (p/f).write_bytes(f.encode())\n"
                +"elif sys.argv[1]=='--read':\n p=pathlib.Path(sys.argv[2]); sys.exit(0 if all((p/f).read_bytes()==f.encode() for f in files) else 1)\n")
        path.write_text("#!"+sys.executable+"\n"+source); path.chmod(0o755)
        build_evidence.finish(evidence,path,str(path)+".manifest.json")
        return str(path)
    def run_matrix(self,name,targets):
        for cfg in targets.values():
            manifest=Path(cfg["binary"]+".manifest.json")
            if manifest.is_file():
                cfg.setdefault("build_manifest",str(manifest))
                cfg.setdefault("expected_build_id",json.loads(manifest.read_text())["build_id"])
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
            targets[name] = {"binary": self.fake("compat-"+str(index), name, self.fingerprint,environment="wine" if name=="windows-arm64" else None)}
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

    def test_mislabeled_wine_is_rejected(self):
        ref=self.fake("ref-wine","macos-arm64",self.fingerprint)
        target=self.fake("mislabel","windows-arm64",self.fingerprint,environment="wine")
        q,r=self.run_matrix("mislabel-test",{"macos-arm64":{"binary":ref},"windows-arm64":{"binary":target,"runtime_kind":"windows"}})
        self.assertEqual(r["targets"]["windows-arm64"]["status"],"FAIL_IDENTITY")

    def test_changed_toolchain_is_rejected(self):
        ref=self.fake("ref-tool","macos-arm64",self.fingerprint)
        (self.base/"ref-tool.tool").write_text("updated compiler")
        q,r=self.run_matrix("changed-tool",{"macos-arm64":{"binary":ref}})
        self.assertEqual(r["targets"]["macos-arm64"]["status"],"BLOCKED_BUILD")

    def test_each_exchange_has_budget_for_four_operations(self):
        from unittest import mock
        import types
        # Exercise the real main orchestration, replacing only external processes.
        ref=self.fake("ref-budget","macos-arm64",self.fingerprint)
        target=self.fake("target-budget","linux-arm64",self.fingerprint)
        cfg={}
        for name,binary in (("macos-arm64",ref),("linux-arm64",target)):
            path=Path(binary+".manifest.json"); r=json.loads(path.read_text())
            cfg[name]={"binary":binary,"build_manifest":str(path),"expected_build_id":r["build_id"]}
        profile=self.base/"budget.json"; profile.write_text(json.dumps({"targets":cfg}))
        out=self.base/"budget-out"
        observed=[]
        def invoke(cmd,timeout=180):
            observed.append((cmd,timeout))
            if "wire_check.py" in " ".join(cmd):
                dest=Path(cmd[cmd.index("--out")+1]); dest.mkdir()
                status="PASS_SELF_TEST" if dest.name=="macos-arm64" else "PASS"
                (dest/"result.json").write_text(json.dumps({"status":status,"corpus_files":sorted(runtime_matrix.EXPECTED_CORPUS)}))
                return 0,"",""
            return real_invoke(cmd,timeout)
        real_invoke=runtime_matrix.invoke
        with mock.patch.object(sys,"argv",["runtime_matrix.py","--profiles",str(profile),"--out",str(out),"--step-timeout","7"]), mock.patch.object(runtime_matrix,"invoke",side_effect=invoke):
            self.assertEqual(runtime_matrix.main(),2)
        exchanges=[(c,t) for c,t in observed if "wire_check.py" in " ".join(c)]
        self.assertTrue(exchanges)
        self.assertTrue(all(t>=4*7 and c[c.index("--step-timeout")+1]=="7" for c,t in exchanges))

    def test_stale_source_fingerprint_is_rejected(self):
        ref=self.fake("ref-stale","macos-arm64","stale")
        q,r=self.run_matrix("stale",{"macos-arm64":{"binary":ref}})
        self.assertEqual(q.returncode,2)
        self.assertEqual(r["targets"]["macos-arm64"]["status"],"BLOCKED_BUILD")
    def test_missing_case_cannot_pass(self):
        ref=self.fake("ref-incomplete","macos-arm64",self.fingerprint,True)
        q,r=self.run_matrix("incomplete",{"macos-arm64":{"binary":ref}})
        self.assertEqual(q.returncode,2)
        self.assertEqual(r["targets"]["macos-arm64"]["status"],"FAIL_WIRE")
if __name__=="__main__":
    unittest.main(verbosity=2)
