#!/usr/bin/env python3
"""Serve wire fixture requests from a private Mac queue on a Windows node."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
import build_evidence

MAX_PAYLOAD = 4 * 1024 * 1024

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def nodecli(argv):
    p = subprocess.run(argv, capture_output=True, text=True, errors="replace", timeout=90)
    if p.returncode:
        raise RuntimeError("nodecli failed: " + (p.stderr or p.stdout)[-2000:])
    try:
        data = json.loads(p.stdout)
    except ValueError as exc:
        raise RuntimeError("invalid nodecli response") from exc
    if data.get("status") not in (None, "completed") or data.get("exit_code") not in (None, 0):
        raise RuntimeError("nodecli operation failed")
    return data

def names(nodecli_path, mac_node, mac_cwd, queue):
    code = "import pathlib,sys; print('\\n'.join(x.name for x in pathlib.Path(sys.argv[1]).glob('*.request.json') if x.is_file()))"
    data = nodecli([nodecli_path, "--json", "--compact", "--node", mac_node,
                    "--cwd", mac_cwd, "--", "python3", "-c", code, queue])
    return [n for n in data.get("stdout", "").splitlines()
            if re.fullmatch(r"[0-9a-f]{32}[.]request[.]json", n)]

def transfer(nodecli_path, source_node, source, destination_node, destination):
    result = nodecli([nodecli_path, "--json", "copy", "--source-node", source_node,
                      "--source", str(source), "--destination-node", destination_node,
                      "--destination", str(destination)])
    if not result.get("verified"):
        raise RuntimeError("nodecli copy was not verified")

def unpack(files, out):
    if not isinstance(files, dict) or not files:
        raise ValueError("missing input corpus")
    out.mkdir()
    for name, value in files.items():
        if not isinstance(name, str) or Path(name).name != name or not name.endswith(".xwire"):
            raise ValueError("invalid corpus name")
        if not isinstance(value, str):
            raise ValueError("invalid corpus data")
        (out / name).write_bytes(base64.b64decode(value, validate=True))

def pack(directory):
    result = {}
    for item in sorted(directory.glob("*.xwire")):
        if item.is_symlink() or not item.is_file():
            raise ValueError("invalid output file")
        result[item.name] = base64.b64encode(item.read_bytes()).decode("ascii")
    if not result:
        raise ValueError("empty output corpus")
    return result

def execute(request, binaries, adapter, adb, serial, work):
    request_id = request.get("id")
    binary_hash = request.get("binary_sha256")
    response = {"version": 1, "id": request_id, "binary_sha256": binary_hash,
                "exit_code": 1, "stdout": "", "stderr": "", "files": {}}
    try:
        if request.get("version") != 1 or not isinstance(request_id, str) or not re.fullmatch(r"[0-9a-f]{32}", request_id):
            raise ValueError("invalid request identity")
        if binary_hash not in binaries:
            raise ValueError("fixture hash is not in the worker allowlist")
        binary, kind = binaries[binary_hash]
        if digest(binary) != binary_hash:
            raise ValueError("worker fixture changed after startup")
        operation = request.get("operation")
        if operation not in ("--xsg-identify", "--xsg-check-build", "--read", "--write"):
            raise ValueError("invalid operation")
        if operation=="--xsg-check-build":
            if kind!="native": raise ValueError("build check requires native fixture")
            response.update(exit_code=0,stdout=build_evidence.check_link_inputs(str(binary)+".manifest.json",binary)+"\n")
            return response
        corpus = work / "corpus"
        if operation == "--read":
            unpack(request.get("files"), corpus)
        command = ([sys.executable, str(adapter), "--adb", adb, "--serial", serial, "--", str(binary)]
                   if kind == "android" else [str(binary)])
        command.append(operation)
        if operation != "--xsg-identify":
            command.append(str(corpus))
        run = subprocess.run(command, capture_output=True, text=True, errors="replace", timeout=90)
        response.update(exit_code=run.returncode if 0 <= run.returncode <= 255 else 1,
                        stdout=run.stdout, stderr=run.stderr)
        if run.returncode == 0 and operation == "--write":
            response["files"] = pack(corpus)
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        response["stderr"] = "Worker: " + str(exc)
    return response

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nodecli", default="nodecli")
    parser.add_argument("--mac-node", default="mac-mini")
    parser.add_argument("--mac-cwd", required=True)
    parser.add_argument("--queue", required=True, help="private absolute directory on Mac")
    parser.add_argument("--adapter", required=True, help="local copy of adb_wire_runner.py")
    parser.add_argument("--adb", default="adb")
    parser.add_argument("--serial", required=True)
    parser.add_argument("--android-fixture", action="append", default=[])
    parser.add_argument("--native-fixture", action="append", default=[])
    parser.add_argument("--interval", type=float, default=0.5)
    a = parser.parse_args()
    if not re.fullmatch(r"/[A-Za-z0-9_./-]+", a.queue) or ".." in Path(a.queue).parts:
        parser.error("queue must be an absolute simple path on Mac")
    adapter = Path(a.adapter).resolve()
    if not adapter.is_file():
        parser.error("ADB adapter missing")
    binaries = {}
    for paths, kind in ((a.android_fixture, "android"), (a.native_fixture, "native")):
        for name in paths:
            path = Path(name).resolve()
            if not path.is_file():
                parser.error("fixture missing: " + str(path))
            fingerprint = digest(path)
            if fingerprint in binaries:
                parser.error("duplicate fixture")
            binaries[fingerprint] = (path, kind)
    if not binaries:
        parser.error("at least one fixture is required")
    print("Wire worker ready with", len(binaries), "allowlisted fixtures", flush=True)
    seen = set()
    with tempfile.TemporaryDirectory(prefix="xsg-nodecli-worker-") as tmp:
        root = Path(tmp)
        while True:
            try:
                pending = names(a.nodecli, a.mac_node, a.mac_cwd, a.queue)
                for name in sorted(pending):
                    if name in seen:
                        continue
                    ident = name[:32]
                    with tempfile.TemporaryDirectory(prefix=ident + "-", dir=root) as directory:
                        work = Path(directory)
                        local_request = work / name
                        try:
                            transfer(a.nodecli, a.mac_node, a.queue + "/" + name, "local", local_request)
                            raw = local_request.read_bytes()
                            if len(raw) > MAX_PAYLOAD:
                                raise ValueError("request too large")
                            request = json.loads(raw)
                            if request.get("id") != ident:
                                raise ValueError("request filename mismatch")
                            response = execute(request, binaries, adapter, a.adb, a.serial, work)
                        except (OSError, ValueError, RuntimeError) as exc:
                            print("Request", ident, "failed:", exc, file=sys.stderr, flush=True)
                            continue
                        raw = json.dumps(response).encode("utf-8")
                        if len(raw) > MAX_PAYLOAD:
                            response.update(exit_code=1, stdout="", stderr="response too large", files={})
                            raw = json.dumps(response).encode("utf-8")
                        result = work / (ident + ".response.json")
                        result.write_bytes(raw)
                        transfer(a.nodecli, "local", result, a.mac_node, a.queue + "/" + result.name)
                        seen.add(name)
                        print("Request", ident, request["operation"], response["exit_code"], flush=True)
            except (RuntimeError, OSError) as exc:
                print("Worker polling error:", exc, file=sys.stderr, flush=True)
            time.sleep(a.interval)

if __name__ == "__main__":
    raise SystemExit(main())
