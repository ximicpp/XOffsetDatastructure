#!/usr/bin/env python3
"""Self-test of asm_equal.py on tiny programs built with the host C++ compiler (default c++).

A renamed function must compare identical; a function moved to another address must compare
identical in code with a different layout; an added function must be the only difference; and a
changed constant must be reported in the function that changed, with its callers listed as
differing only in what they reference.
"""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import asm_equal as ae  # noqa: E402

PROGRAM = r"""
#include <cstdio>
#define NOINLINE __attribute__((noinline))
ALIGN NOINLINE int HELPER(int x) { return x * SCALE + 1; }
PAD
NOINLINE int caller(int x) { return HELPER(x) + HELPER(x + 1); }
int main(int argc, char**) { std::printf("%d\n", caller(argc)); return 0; }
"""


def build(cxx, out: Path, helper="helper", scale=3, pad="", align=""):
    src = out.with_suffix(".cpp")
    src.write_text(PROGRAM.replace("HELPER", helper).replace("SCALE", str(scale)).replace("PAD", pad)
                   .replace("ALIGN", align))
    out.mkdir()
    subprocess.run([cxx, "-O2", str(src), "-o", str(out / "prog")], check=True)
    return out


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--cxx", default="c++")
    a = p.parse_args()
    with tempfile.TemporaryDirectory() as d:
        d = Path(d)
        base = build(a.cxx, d / "base")
        renamed = build(a.cxx, d / "renamed", helper="helper_renamed")
        aligned = build(a.cxx, d / "aligned", align="__attribute__((aligned(256)))")
        added = build(a.cxx, d / "added", pad="NOINLINE int unused_pad(int x) { return x ^ 0x5a5a; }")
        changed = build(a.cxx, d / "changed", scale=5)
        failures = []

        def check(name, other, same_code, same_layout):
            r = ae.compare_program(base / "prog", other / "prog", "objdump", "readelf")
            got = (r["same_code"], r["same_layout"])
            if got != (same_code, same_layout):
                failures.append(f"{name}: same_code/same_layout {got}, expected {(same_code, same_layout)}")
            return r

        check("renamed", renamed, True, True)
        # Same instructions, helper starts at another address: identical code, layout differs.
        check("aligned", aligned, True, False)
        # One more function: a code difference, while every base function finds its twin.
        r = check("added", added, False, False)
        if [f["name"] for f in r["differ_head"] if f["own_change"]] != ["unused_pad(int)"] or r["differ_base"]:
            failures.append(f"added: expected only unused_pad as head-only, got {r['differ_base']} / {r['differ_head']}")
        r = check("changed", changed, False, False)
        own = sorted(f["name"] for f in r["differ_base"] if f["own_change"])
        via = sorted(f["name"] for f in r["differ_base"] if not f["own_change"])
        if own != ["helper(int)"] or "caller(int)" not in via:
            failures.append(f"changed: own changes {own}, reference-only {via}")
        if not r["details"] or not any("lea" in line or "imul" in line or "$0x" in line for line in r["details"][0]["diff"]):
            failures.append(f"changed: no instruction diff for helper: {r['details']}")
    for f in failures:
        print("FAIL", f)
    print("asm_equal self-test:", "FAILED" if failures else "passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
