#!/usr/bin/env python3
"""Mac-side runner for a separately started Windows nodecli wire worker."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import sys
import time
import uuid

MAX_PAYLOAD = 4 * 1024 * 1024

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def pack(directory):
    path = Path(directory)
    if not path.is_dir():
        raise ValueError("wire input directory missing")
    result = {}
    for item in sorted(path.glob("*.xwire")):
        if not item.is_file() or item.is_symlink():
            raise ValueError("invalid corpus file")
        result[item.name] = base64.b64encode(item.read_bytes()).decode("ascii")
    if not result:
        raise ValueError("empty wire corpus")
    return result

def unpack(files, directory):
    if not isinstance(files, dict) or not files:
        raise ValueError("missing output corpus")
    path = Path(directory)
    path.mkdir(parents=True, exist_ok=False)
    for name, data in files.items():
        if not isinstance(name, str) or Path(name).name != name or not name.endswith(".xwire"):
            raise ValueError("invalid output file name")
        if not isinstance(data, str):
            raise ValueError("invalid output data")
        (path / name).write_bytes(base64.b64decode(data, validate=True))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--queue", required=True, help="private Mac directory shared with worker")
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("binary")
    parser.add_argument("operation", choices=["--xsg-identify", "--write", "--read"])
    parser.add_argument("directory", nargs="?")
    a = parser.parse_args()
    if a.operation != "--xsg-identify" and not a.directory:
        parser.error("wire directory required")
    queue = Path(a.queue)
    if not queue.is_dir() or queue.is_symlink() or (queue.stat().st_mode & 0o077):
        parser.error("queue must be an existing private directory (mode 0700)")
    binary = Path(a.binary).resolve()
    if not binary.is_file():
        parser.error("fixture binary missing")
    request_id = uuid.uuid4().hex
    request = queue / (request_id + ".request.json")
    temporary = queue / (request_id + ".pending")
    response = queue / (request_id + ".response.json")
    try:
        payload = {"version": 1, "id": request_id, "binary_sha256": sha(binary),
                   "operation": a.operation,
                   "files": pack(a.directory) if a.operation == "--read" else {}}
        raw = json.dumps(payload).encode("utf-8")
        if len(raw) > MAX_PAYLOAD:
            raise ValueError("request too large")
        with temporary.open("xb") as f:
            f.write(raw)
        os.replace(temporary, request)
        deadline = time.monotonic() + a.timeout
        while not response.is_file():
            if time.monotonic() >= deadline:
                raise TimeoutError("worker did not respond")
            time.sleep(0.2)
        raw = response.read_bytes()
        if len(raw) > MAX_PAYLOAD:
            raise ValueError("response too large")
        data = json.loads(raw)
        if data.get("version") != 1 or data.get("id") != request_id or data.get("binary_sha256") != payload["binary_sha256"]:
            raise ValueError("response identity mismatch")
        if not isinstance(data.get("stdout"), str) or not isinstance(data.get("stderr"), str):
            raise ValueError("invalid worker output")
        rc = data.get("exit_code")
        if not isinstance(rc, int) or isinstance(rc, bool) or rc < 0 or rc > 255:
            raise ValueError("invalid worker exit code")
        if rc == 0 and a.operation == "--write":
            unpack(data.get("files"), a.directory)
        sys.stdout.write(data["stdout"])
        sys.stderr.write(data["stderr"])
        return rc
    except (OSError, ValueError, TimeoutError, json.JSONDecodeError) as exc:
        print("Wire relay:", exc, file=sys.stderr)
        return 1
    finally:
        for p in (temporary, request, response):
            try:
                p.unlink(missing_ok=True)
            except OSError:
                pass

if __name__ == "__main__":
    raise SystemExit(main())
