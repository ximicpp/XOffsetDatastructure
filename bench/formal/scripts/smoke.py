#!/usr/bin/env python3
"""Functional smoke test of an sc_xoffset build on generated fixtures.

usage: smoke.py --driver build/bin/sc_xoffset --fixtures DIR [--scale S --scale M]
                [--expect-m-wire SHA256 | --no-expect-m-wire]

Per scale: encode the input canonical into the XOffset input wire, validate the
wire against the trace (every stage digest, queries, emit and reload), and run
the timed command once. For M the input wire sha256 must equal the pinned
formal input wire (default: M_INPUT_WIRE below); a library change that alters
the wire format fails here and must be stated.
"""
import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

# The formal M input wire. R3/W9a (Mac FREEZE.json, wire format 2.0):
# 2bf0c0717767f3776a47532e90f39aee6ae9ac0056e94e06731a2f9eaa25c2a4.
# 2026-10-27, wire format 2.1 (40-byte header, no payload header on the wire;
# 16-byte heads): 9,435,800 bytes.
M_INPUT_WIRE = "c0e9d7421932b2d5587dcda5aac0cf6112f34602470940cf7ba0c019841e6bbc"


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def drive(driver, *args):
    r = subprocess.run([str(driver), *args], capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"{' '.join(args[:1])} failed ({r.returncode}): {r.stderr.strip()}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--driver", required=True, type=Path)
    p.add_argument("--fixtures", required=True, type=Path)
    p.add_argument("--scale", action="append", choices=["S", "M", "L"])
    p.add_argument("--expect-m-wire", default=M_INPUT_WIRE)
    p.add_argument("--no-expect-m-wire", action="store_true")
    args = p.parse_args()
    failed = False
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        for scale in args.scale or ["S", "M"]:
            fx = args.fixtures / scale
            wire = tmp / f"{scale}.input.wire"
            drive(args.driver, "encode", "--canonical", str(fx / "input.canonical.bin"), "--wire", str(wire))
            wire_sha = sha256(wire)
            drive(args.driver, "validate", "--wire", str(wire), "--trace", str(fx / "trace.bin"),
                  "--result", str(tmp / f"{scale}.validate.json"))
            out = tmp / f"{scale}.run"
            out.mkdir()
            drive(args.driver, "run", "--wire", str(wire), "--trace", str(fx / "trace.bin"),
                  "--wire-out", str(out / "final.wire"), "--canonical-out", str(out / "final.canonical.bin"),
                  "--queries-out", str(out / "queries.bin"), "--result", str(out / "result.json"))
            manifest = json.loads((fx / "fixture.json").read_text())
            if sha256(out / "final.canonical.bin") != manifest["final_digest"]:
                sys.exit(f"{scale}: run final canonical differs from the fixture")
            line = f"{scale}: encode, validate, run passed; input wire {wire.stat().st_size} bytes sha256 {wire_sha}"
            if scale == "M" and not args.no_expect_m_wire:
                if wire_sha == args.expect_m_wire:
                    line += " (= pinned formal input wire)"
                else:
                    line += f" != pinned formal input wire {args.expect_m_wire}"
                    failed = True
            print(line)
    if failed:
        sys.exit("M input wire changed: the formal input wire is no longer the pinned one")


if __name__ == "__main__":
    main()
