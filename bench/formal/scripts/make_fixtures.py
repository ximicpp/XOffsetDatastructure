#!/usr/bin/env python3
"""Generate the formal-harness fixtures with model/reference.py and check them.

usage: make_fixtures.py --out DIR [--scale S --scale M ...]

Each scale is written to DIR/<scale>/ (input.canonical.bin, trace.bin, ...).
S must equal fixtures/golden-S byte for byte; M must equal the Mac prepared-v1
M fixture (fixtures/prepared-v1-M/fixture.json lists every file's sha256).
Any mismatch exits non-zero. M takes about 30 seconds.
"""
import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
EXPECTED = {"S": HERE / "fixtures" / "golden-S" / "fixture.json",
            "M": HERE / "fixtures" / "prepared-v1-M" / "fixture.json"}


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def check(scale, out):
    manifest = json.loads((out / "fixture.json").read_text())
    for name, ident in manifest["files"].items():
        p = out / name
        if p.stat().st_size != ident["bytes"] or sha256(p) != ident["sha256"]:
            sys.exit(f"{scale}: {name} does not match its own fixture.json")
    expected = EXPECTED.get(scale)
    if expected is None:
        print(f"{scale}: generated (no committed reference)")
        return
    if (out / "fixture.json").read_bytes() != expected.read_bytes():
        sys.exit(f"{scale}: fixture.json differs from {expected.relative_to(HERE)}")
    print(f"{scale}: identical to {expected.relative_to(HERE)} (input {manifest['input_digest'][:16]}, "
          f"trace {manifest['files']['trace.bin']['sha256'][:16]})")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--out", required=True, type=Path)
    p.add_argument("--scale", action="append", choices=["S", "M", "L"])
    args = p.parse_args()
    for scale in args.scale or ["S", "M"]:
        out = args.out / scale
        if out.exists():
            sys.exit(f"{out} exists; give a fresh --out")
        subprocess.run([sys.executable, "-I", "-B", str(HERE / "model" / "reference.py"),
                        "--scale", scale, "--out", str(out)], check=True, stdout=subprocess.DEVNULL)
        check(scale, out)


if __name__ == "__main__":
    main()
