#!/usr/bin/env python3
"""Compile Windows wire fixtures to COFF objects with the Mac reflection compiler."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import verify
import wire_inputs
import build_evidence

ROOT = Path(__file__).resolve().parents[2]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--profiles", required=True, help="compile profiles from prepare_windows_headers.py")
    p.add_argument("--out", required=True, help="new output directory")
    a = p.parse_args()
    profiles = json.loads(Path(a.profiles).read_text())
    fingerprint, inputs = wire_inputs.fingerprint()
    out = Path(a.out).resolve()
    out.mkdir(parents=True, exist_ok=False)
    result = {}
    for name, triple in (("windows-x64", "x86_64-w64-windows-gnu"),
                         ("windows-arm64", "aarch64-w64-windows-gnu")):
        cfg = profiles[name]
        if cfg.get("kind") != "host" or "--target=" + triple not in cfg.get("flags", []):
            raise ValueError("Windows target must be a matching host compile profile")
        ir, obj = out / (name + ".ll"), out / (name + ".o")
        cmd = [cfg["compiler"], *cfg["flags"], "-O1",
               '-DXSG_WIRE_INPUT_HASH="' + fingerprint + '"']
        for inc in verify.includes(ROOT):
            cmd += ["-I", str(inc)]
        cmd += ["-S", "-emit-llvm", str(ROOT / "tools/schema_guard/wire_runtime.cpp"), "-o", str(ir)]
        stages = [cmd, ["xcrun", "clang++", "--target=" + triple, "-O1", "-c", str(ir), "-o", str(obj)]]
        backend=subprocess.check_output(["xcrun","-f","clang++"],text=True).strip()
        evidence=build_evidence.prepare(name,fingerprint,stages,
            [*build_evidence.tool_inputs(cfg["compiler"]),Path(backend),*build_evidence.flag_inputs(cfg.get("flags",[]))])
        stages[0].insert(1,'-DXSG_BUILD_ID="'+evidence["build_id"]+'"')
        log = []
        for stage in stages:
            try:
                r = subprocess.run(stage, capture_output=True, text=True, errors="replace", timeout=300)
                log.append({"command": stage, "exit_code": r.returncode, "stdout": r.stdout, "stderr": r.stderr})
                if r.returncode:
                    break
            except (OSError, subprocess.TimeoutExpired) as exc:
                log.append({"command": stage, "exit_code": None, "stderr": str(exc)})
                break
        passed = len(log) == len(stages) and log[-1]["exit_code"] == 0 and obj.is_file()
        if passed: build_evidence.finish(evidence,obj,out/(name+".object.manifest.json"))
        result[name] = {"status": "PASS_BUILD" if passed else "FAIL_BUILD", "stages": log}
        print(name, result[name]["status"], flush=True)
    (out / "inputs.json").write_text(json.dumps({"fingerprint": fingerprint, "inputs": inputs}, indent=2) + "\n")
    (out / "build-result.json").write_text(json.dumps(result, indent=2) + "\n")
    return 0 if all(x["status"] == "PASS_BUILD" for x in result.values()) else 2

if __name__ == "__main__":
    sys.exit(main())
