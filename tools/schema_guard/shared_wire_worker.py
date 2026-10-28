#!/usr/bin/env python3
"""Execute allowlisted native Windows wire fixtures from a shared relay queue."""
import argparse
import json
import os
from pathlib import Path
import re
import tempfile
import time
import uuid

import nodecli_wire_worker as worker

def process(queue, name, binaries):
    ident = name[:32]
    request_path = queue / name
    if not re.fullmatch(r"[0-9a-f]{32}[.]request[.]json", name) or request_path.is_symlink():
        raise ValueError("invalid request file")
    raw = request_path.read_bytes()
    if len(raw) > worker.MAX_PAYLOAD:
        raise ValueError("request too large")
    request = json.loads(raw)
    if not isinstance(request, dict) or request.get("id") != ident:
        raise ValueError("request filename mismatch")
    with tempfile.TemporaryDirectory(prefix="xsg-shared-worker-") as tmp:
        result = worker.execute(request, binaries, None, "", "", Path(tmp))
    raw = json.dumps(result).encode("utf-8")
    if len(raw) > worker.MAX_PAYLOAD:
        result.update(exit_code=1, stdout="", stderr="response too large", files={})
        raw = json.dumps(result).encode("utf-8")
    pending = queue / (ident + "." + uuid.uuid4().hex + ".pending")
    response = queue / (ident + ".response.json")
    try:
        with pending.open("xb") as stream:
            stream.write(raw)
        os.replace(pending, response)
    finally:
        pending.unlink(missing_ok=True)
    return result["exit_code"]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--queue", required=True, help="guest path to the private shared queue")
    p.add_argument("--native-fixture", action="append", required=True)
    p.add_argument("--interval", type=float, default=0.5)
    p.add_argument("--once", action="store_true", help="process currently pending requests and exit")
    a = p.parse_args()
    queue = Path(a.queue)
    if not queue.is_dir() or queue.is_symlink():
        p.error("queue must be an existing directory")
    binaries = {}
    for name in a.native_fixture:
        path = Path(name).resolve()
        if not path.is_file():
            p.error("fixture missing: " + str(path))
        fingerprint = worker.digest(path)
        if fingerprint in binaries:
            p.error("duplicate fixture")
        binaries[fingerprint] = (path, "native")
    seen = set()
    print("Shared worker ready with", len(binaries), "allowlisted fixtures", flush=True)
    while True:
        failed = False
        for path in sorted(queue.glob("*.request.json")):
            if path.name in seen:
                continue
            seen.add(path.name)
            try:
                status = process(queue, path.name, binaries)
                print("Request", path.name[:32], status, flush=True)
                failed = failed or status != 0
            except (OSError, ValueError) as exc:
                failed = True
                print("Request", path.name[:32], "failed:", exc, flush=True)
        if a.once:
            return 1 if failed else 0
        time.sleep(a.interval)

if __name__ == "__main__":
    raise SystemExit(main())
