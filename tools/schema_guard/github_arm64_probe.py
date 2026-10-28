#!/usr/bin/env python3
"""One-shot GitHub Windows ARM64 wire probe. Full gate requires a fresh reverse read on Mac."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def verify_inputs(data):
    inputs = data["inputs"]
    digest = hashlib.sha256(json.dumps(inputs, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    if digest != data["fingerprint"]:
        raise ValueError("input fingerprint mismatch")
    expected = {p.relative_to(ROOT).as_posix() for p in ROOT.glob("*.hpp")} | {
        "tests/test_cross_compiler_wire.cpp", "bench/slg/include/slg/model_xoffset.hpp",
        "tools/schema_guard/catalog_slg.hpp", "tools/schema_guard/wire_runtime.cpp",
        "cmake/XOffsetDependencyLock.cmake"}
    if set(inputs["files"]) != expected:
        raise ValueError("source file set mismatch")
    for path, digest in inputs["files"].items():
        if sha(ROOT / path) != digest:
            raise ValueError("source changed: " + path)
    lock = (ROOT / "cmake/XOffsetDependencyLock.cmake").read_text(encoding="utf-8")
    entries = dict(re.findall(r'"(external/[^"|]+)\|([0-9a-f]{64})"', lock))
    records = inputs["dependencies"]
    if len(records) != 14 or len(entries) != 14:
        raise ValueError("dependency lock count mismatch")
    if {x["path"]: x["expected"] for x in records} != entries or any(x["actual"] != x["expected"] for x in records):
        raise ValueError("dependency evidence mismatch")
    if set(data["gitlinks"]) != {"external/boost", "external/typelayout"}:
        raise ValueError("gitlink set mismatch")
    for name, commit in data["gitlinks"].items():
        actual = subprocess.check_output(["git", "rev-parse", "HEAD:" + name], cwd=ROOT, text=True).strip()
        if actual != commit:
            raise ValueError("gitlink changed: " + name)

def run(args):
    return subprocess.run(args, check=True, capture_output=True, text=True, timeout=120).stdout

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--reference-zip", required=True)
    parser.add_argument("--out", required=True)
    a = parser.parse_args()
    data = json.loads(Path(a.input).read_text(encoding="utf-8"))
    verify_inputs(data)
    if os.environ.get("RUNNER_OS") != "Windows" or os.environ.get("RUNNER_ARCH") != "ARM64":
        raise ValueError("requires GitHub Windows ARM64 runner")
    binary, archive, out = Path(a.binary).resolve(), Path(a.reference_zip).resolve(), Path(a.out).resolve()
    if sha(binary) != data["binary"]["sha256"] or sha(archive) != data["reference"]["sha256"]:
        raise ValueError("input digest mismatch")
    out.mkdir(parents=True, exist_ok=False)
    ref, target = out / "reference", out / "target"
    ref.mkdir()
    with zipfile.ZipFile(archive) as z:
        names = z.namelist()
        if not names or len(names) != len(set(names)) or any(
                not re.fullmatch(r"[a-z0-9_]+\.xwire", name) for name in names):
            raise ValueError("invalid reference archive")
        for name in names:
            (ref / name).write_bytes(z.read(name))
    identity = run([str(binary), "--xsg-identify"]).strip()
    expected = "XSG_RUNTIME_V1 windows-arm64 64 le " + data["fingerprint"]
    if identity != expected:
        raise ValueError("target identity mismatch")
    read_log = run([str(binary), "--read", str(ref)])
    write_log = run([str(binary), "--write", str(target)])
    refs = sorted(p.name for p in ref.glob("*.xwire"))
    targets = sorted(p.name for p in target.glob("*.xwire"))
    if refs != targets or not refs or any(sha(ref / n) != sha(target / n) for n in refs):
        raise ValueError("wire corpus differs")
    result = {"status": "WINDOWS_READ_PASS_MAC_READ_PENDING",
              "runner_os": os.environ["RUNNER_OS"], "runner_arch": os.environ["RUNNER_ARCH"],
              "fingerprint": data["fingerprint"], "identity": identity,
              "reference_sha256": data["reference"]["sha256"],
              "binary_sha256": data["binary"]["sha256"],
              "corpus": {n: sha(target / n) for n in targets},
              "windows_read": read_log, "windows_write": write_log}
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(result["status"])

if __name__ == "__main__":
    sys.exit(main())
