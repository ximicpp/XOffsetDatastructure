#!/usr/bin/env python3
"""Fingerprint sources and locked dependencies consumed by the wire runtime fixture."""
import hashlib
import json
from pathlib import Path
import verify

ROOT = Path(__file__).resolve().parents[2]
def fingerprint():
    ok, dependencies = verify.verify_dependencies()
    if not ok:
        raise RuntimeError("repository dependency lock mismatch")
    paths = sorted(ROOT.glob("*.hpp")) + [
        ROOT / "tests/test_cross_compiler_wire.cpp",
        ROOT / "bench/slg/include/slg/model_xoffset.hpp",
        ROOT / "tools/schema_guard/catalog_slg.hpp",
        ROOT / "tools/schema_guard/wire_runtime.cpp",
        ROOT / "cmake/XOffsetDependencyLock.cmake",
    ]
    files = {p.relative_to(ROOT).as_posix(): verify.sha(p) for p in paths}
    data = {"files": files, "dependencies": dependencies}
    encoded = json.dumps(data, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(encoded).hexdigest(), data

if __name__ == "__main__":
    print(fingerprint()[0])
