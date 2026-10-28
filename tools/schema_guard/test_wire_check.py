#!/usr/bin/env python3
'''Tests the wire exchange orchestration with deterministic fake binaries.'''
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HERE=Path(__file__).resolve().parent
class WireCheckTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base=Path(tempfile.mkdtemp(prefix="xsg-wire-test-"))
        print("test artifacts:",cls.base,flush=True)
        for name, value in (("ref", "A"), ("same", "A"), ("different", "B")):
            f=cls.base/(name+".py")
            f.write_text("import pathlib,sys\np=pathlib.Path(sys.argv[2])\n"
                         "if sys.argv[1]=='--write':\n p.mkdir(parents=True,exist_ok=True); (p/'case.xwire').write_bytes(b'"+value+"')\n"
                         "elif sys.argv[1]=='--read':\n sys.exit(0 if (p/'case.xwire').read_bytes()==b'"+value+"' else 1)\n")
    def check(self,name,a,b):
        out=self.base/name
        cmd=[sys.executable,str(HERE/"wire_check.py"),
             "--reference-binary",str(self.base/(a+".py")),
             "--target-binary",str(self.base/(b+".py")),
             "--reference-runner",json.dumps([sys.executable]),
             "--target-runner",json.dumps([sys.executable]),"--out",str(out)]
        p=subprocess.run(cmd,capture_output=True,text=True)
        return p,json.loads((out/"result.json").read_text())
    def test_distinct_matching_corpora(self):
        p,r=self.check("matching","ref","same")
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertEqual(r["status"],"PASS")
        self.assertTrue(r["byte_identical"])
        self.assertEqual(len(r["steps"]),4)
    def test_same_binary_is_not_cross_platform_evidence(self):
        p,r=self.check("self","ref","ref")
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertEqual(r["status"],"PASS_SELF_TEST")
    def test_mismatch_fails(self):
        p,r=self.check("bad","ref","different")
        self.assertNotEqual(p.returncode,0)
        self.assertEqual(r["status"],"FAIL")
        self.assertTrue(any(x["exit_code"] for x in r["steps"]))
if __name__=="__main__":
    unittest.main(verbosity=2)
