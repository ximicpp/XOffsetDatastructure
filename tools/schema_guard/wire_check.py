#!/usr/bin/env python3
'''Exchange deterministic test_cross_compiler_wire-compatible corpora.'''
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--reference-binary", required=True)
    p.add_argument("--target-binary", required=True)
    p.add_argument("--reference-runner", default="[]", help="JSON argv prefix, e.g. [\"wine\"]")
    p.add_argument("--target-runner", default="[]", help="JSON argv prefix")
    p.add_argument("--out", required=True, help="new output directory")
    p.add_argument("--step-timeout", type=int, default=180)
    a = p.parse_args()
    if not 1 <= a.step_timeout <= 600: p.error("invalid step timeout")
    ref = Path(a.reference_binary).resolve()
    target = Path(a.target_binary).resolve()
    if not ref.is_file() or not target.is_file():
        p.error("both binaries must exist")
    try:
        rr, tr = json.loads(a.reference_runner), json.loads(a.target_runner)
        if not all(isinstance(x, list) and all(isinstance(y, str) for y in x) for x in (rr, tr)):
            raise ValueError("runner must be a JSON array of strings")
    except (ValueError, json.JSONDecodeError) as e:
        p.error(str(e))
    out = Path(a.out).resolve()
    out.mkdir(parents=True, exist_ok=False)
    result = {"status":"FAIL", "reference_binary":str(ref), "reference_sha256":sha(ref),
              "target_binary":str(target), "target_sha256":sha(target),
              "reference_runner":rr, "target_runner":tr, "steps":[]}
    def step(name, cmd):
        try:
            q = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=a.step_timeout)
            rc, text = q.returncode, q.stdout + q.stderr
        except (OSError, subprocess.TimeoutExpired) as exc:
            rc, text = None, str(exc)
        (out / (name + ".log")).write_text(text, encoding="utf-8")
        result["steps"].append({"name":name, "command":cmd, "exit_code":rc})
        return rc == 0
    refdir, targetdir = out / "reference", out / "target"
    ok = step("reference_write", [*rr, str(ref), "--write", str(refdir)])
    ok = step("target_write", [*tr, str(target), "--write", str(targetdir)]) and ok
    if ok:
        ok = step("target_reads_reference", [*tr, str(target), "--read", str(refdir)]) and ok
        ok = step("reference_reads_target", [*rr, str(ref), "--read", str(targetdir)]) and ok
    if ok:
        refs = sorted(x.name for x in refdir.glob("*.xwire"))
        targets = sorted(x.name for x in targetdir.glob("*.xwire"))
        ok = bool(refs) and refs == targets and all(
            (refdir / name).read_bytes() == (targetdir / name).read_bytes() for name in refs)
        result["corpus_files"] = refs
        result["byte_identical"] = ok
    result["status"] = ("PASS_SELF_TEST" if ref == target and rr == tr else "PASS") if ok else "FAIL"
    result["scope"] = ("The supplied binary corpus only; using the same binary twice "
                       "is a self-test and is not cross-platform evidence.")
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(result["status"], out / "result.json")
    return 0 if ok else 1
if __name__ == "__main__":
    sys.exit(main())
