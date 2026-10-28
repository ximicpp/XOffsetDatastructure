#!/usr/bin/env python3
"""Run a command only after a fresh, complete schema compile matrix passes."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import uuid

HERE = Path(__file__).resolve().parent

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang", default=os.environ.get("XSG_CLANG") or os.environ.get("CXX"))
    parser.add_argument("--ndk", default=os.environ.get("XSG_NDK") or os.environ.get("ANDROID_NDK_HOME"))
    parser.add_argument("--profiles", default=os.environ.get("XSG_PROFILES"))
    parser.add_argument("--wire-profiles", default=os.environ.get("XSG_WIRE_PROFILES"))
    parser.add_argument("--catalog", default=os.environ.get("XSG_CATALOG") or str(HERE / "catalog_slg.hpp"))
    parser.add_argument("--reports", required=True, help="parent for unique, preserved run reports")
    parser.add_argument("--pull", action="store_true", help="allow pulling pinned Docker images")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)
    command = args.command
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        parser.error("provide a command after --")
    reports = Path(args.reports).resolve()
    reports.mkdir(parents=True, exist_ok=True)
    output = reports / ("schema-" + uuid.uuid4().hex)
    gate = [sys.executable, str(HERE / "verify.py"), "--catalog", args.catalog, "--out", str(output)]
    for name, value in (("--clang", args.clang), ("--ndk", args.ndk), ("--profiles", args.profiles)):
        if value:
            gate += [name, value]
    if args.pull:
        gate.append("--pull")
    print("Schema gate report:", output, flush=True)
    result = subprocess.run(gate)
    if result.returncode != 0:
        print("Schema gate did not pass; command was not started.", file=sys.stderr, flush=True)
        return result.returncode if result.returncode > 0 else 1
    if Path(args.catalog).resolve() != (HERE / "catalog_slg.hpp").resolve():
        print("Runtime wire fixture covers only catalog_slg.hpp; custom catalog cannot pass this gate.",
              file=sys.stderr, flush=True)
        return 2
    runtime = [sys.executable, str(HERE / "runtime_matrix.py"), "--out", str(output / "runtime")]
    if args.wire_profiles:
        runtime += ["--profiles", args.wire_profiles]
    result = subprocess.run(runtime)
    if result.returncode != 0:
        print("Runtime/wire matrix did not pass; command was not started.", file=sys.stderr, flush=True)
        return result.returncode if result.returncode > 0 else 1
    print("Complete schema and runtime/wire matrices passed; starting command.", flush=True)
    try:
        result = subprocess.run(command)
    except OSError as exc:
        print("Cannot start command:", exc, file=sys.stderr, flush=True)
        return 127
    return result.returncode if result.returncode >= 0 else 128 - result.returncode

if __name__ == "__main__":
    sys.exit(main())
