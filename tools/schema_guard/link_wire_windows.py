#!/usr/bin/env python3
"""Link previously compiled Windows wire objects using local MinGW runtime trees."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import build_evidence

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--clang", required=True)
    p.add_argument("--objects", required=True, help="object directory copied from Mac")
    p.add_argument("--sysroot-x64", required=True)
    p.add_argument("--sysroot-arm64", required=True)
    p.add_argument("--out", required=True, help="new output directory")
    a = p.parse_args()
    out = Path(a.out).resolve()
    out.mkdir(parents=True, exist_ok=False)
    objs = Path(a.objects).resolve()
    result = {}
    for name, triple, sysroot in (("windows-x64", "x86_64-w64-windows-gnu", a.sysroot_x64),
                                  ("windows-arm64", "aarch64-w64-windows-gnu", a.sysroot_arm64)):
        root = Path(sysroot).resolve()
        obj = objs / (name + ".o")
        resources = list((root / "lib/clang").glob("*/lib/windows"))
        if len(resources) != 1 or not obj.is_file():
            result[name] = {"status": "BLOCKED_CONFIG"}
            continue
        resource_dir = resources[0].parents[1]
        binary = out / (name + ".exe")
        command = [a.clang, "--target=" + triple, "--sysroot=" + str(root),
                   "-resource-dir=" + str(resource_dir), "-fuse-ld=lld", "-rtlib=compiler-rt",
                   "-nostdlib++", "-static", str(obj), *(str(root / "lib" / lib) for lib in
                   ("libc++.a", "libc++abi.a", "libunwind.a")), "-o", str(binary)]
        receipt=objs/(name+".object.manifest.json")
        if not receipt.is_file():
            result[name]={"status":"BLOCKED_BUILD","reason":"frontend manifest missing"}
            continue
        front=json.loads(receipt.read_text())
        if front["build_id"]!=build_evidence.digest(front["spec"]) or front["binary_sha256"]!=sha(obj):
            result[name]={"status":"BLOCKED_BUILD","reason":"frontend object mismatch"}
            continue
        evidence=build_evidence.prepare(name,front["spec"]["source_fingerprint"],[command],
            [Path(a.clang).resolve().parent,root,obj],frontend=front)
        try:
            run = subprocess.run(command, capture_output=True, text=True, errors="replace", timeout=300)
            rc, log = run.returncode, run.stdout + run.stderr
        except (OSError, subprocess.TimeoutExpired) as exc:
            rc, log = None, str(exc)
        (out / (name + ".log")).write_text(log, encoding="utf-8")
        passed = rc == 0 and binary.is_file()
        if passed: build_evidence.finish(evidence,binary,str(binary)+".manifest.json")
        result[name] = {"status": "PASS_LINK" if passed else "FAIL_LINK", "exit_code": rc,
                        "object_sha256": sha(obj), "binary_sha256": sha(binary) if passed else None,
                        "command": command}
        print(name, result[name]["status"], flush=True)
    (out / "link-result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return 0 if all(x["status"] == "PASS_LINK" for x in result.values()) else 2

if __name__ == "__main__":
    sys.exit(main())
