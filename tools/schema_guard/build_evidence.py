#!/usr/bin/env python3
"""Bind a fixture to its source, commands, tool binaries and SDK/runtime inputs."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def input_record(path):
    p = Path(path).resolve()
    if p.is_file():
        return {"path": str(p), "kind": "file", "sha256": sha(p)}
    if p.is_dir():
        rows = []
        for f in sorted(p.rglob("*")):
            if f.is_file():
                rows.append([f.relative_to(p).as_posix(), sha(f)])
        return {"path": str(p), "kind": "tree", "sha256": digest(rows)}
    raise ValueError("build input missing: " + str(p))

def tool_inputs(program):
    p = Path(shutil.which(program) or program).resolve()
    result = [p]
    # Clang wrappers can delegate to a different executable. Capture that executable
    # and the header/resource paths exposed by Clang's driver trace.
    q = subprocess.run([str(p), "-###", "-x", "c++", "-c", "/dev/null"],
                       capture_output=True, text=True, timeout=30)
    import shlex
    for line in q.stderr.splitlines():
        try:
            args = shlex.split(line)
        except ValueError:
            continue
        if "-cc1" not in args:
            continue
        for i, value in enumerate(args):
            if i == 0 or (i and args[i-1] in ("-isystem", "-internal-isystem", "-internal-externc-isystem", "-resource-dir", "-isysroot")):
                if Path(value).exists():
                    result.append(Path(value))
    return result

def prepare(target, source_fingerprint, commands, inputs=(), images=(), frontend=None):
    if not commands or (not inputs and not images):
        raise ValueError("build evidence requires commands and immutable inputs")
    records = [input_record(p) for p in sorted({str(Path(p).resolve()) for p in inputs})]
    spec = {"target": target, "source_fingerprint": source_fingerprint,
            "commands": json.loads(json.dumps(commands)), "inputs": records, "images": list(images)}
    if frontend is not None: spec["frontend"]=frontend
    record={"format": 1, "build_id": digest(spec), "spec": spec}
    if frontend is not None: record["compile_id"]=frontend.get("compile_id",frontend["build_id"])
    return record

def finish(record, binary, path):
    record = dict(record, binary_sha256=sha(binary))
    Path(path).write_text(json.dumps(record, indent=2) + "\n")
    return {"build_manifest": str(Path(path).resolve()), "expected_build_id": record["build_id"]}

def validate(path, binary, target, fingerprint, expected_build_id, check_inputs=True):
    record = json.loads(Path(path).read_text())
    spec = record["spec"]
    if record.get("format") != 1 or digest(spec) != record.get("build_id") or record["build_id"] != expected_build_id:
        raise ValueError("build id or manifest mismatch")
    if spec.get("target") != target or spec.get("source_fingerprint") != fingerprint:
        raise ValueError("build target/source mismatch")
    if record.get("binary_sha256") != sha(binary):
        raise ValueError("build binary digest mismatch")
    if not spec.get("commands") or (not spec.get("inputs") and not spec.get("images")):
        raise ValueError("build inputs missing")
    if "frontend" in spec:
        front=spec["frontend"]
        if digest(front["spec"])!=front["build_id"] or front["spec"]["source_fingerprint"]!=fingerprint or front["spec"]["target"]!=target or record.get("compile_id")!=front["build_id"]:
            raise ValueError("frontend build mismatch")
        check_records(front["spec"]["inputs"])
    if check_inputs: check_records(spec["inputs"])
    import re
    if any(not re.fullmatch(r".+@sha256:[0-9a-f]{64}", x) for x in spec["images"]):
        raise ValueError("build image is not immutable")
    return record


def check_records(records):
    for item in records:
        if input_record(item["path"]) != item:
            raise ValueError("build input changed: " + item["path"])

def flag_inputs(flags):
    result=[]
    for i,value in enumerate(flags):
        if i and flags[i-1] in ("-isystem","-I","-isysroot","-resource-dir","-L"):
            result.append(Path(value))
        elif value.startswith("--sysroot="):
            result.append(Path(value.split("=",1)[1]))
    return result

def check_link_inputs(path, binary):
    record=json.loads(Path(path).read_text())
    if digest(record["spec"])!=record["build_id"] or sha(binary)!=record["binary_sha256"]:
        raise ValueError("link receipt mismatch")
    check_records(record["spec"]["inputs"])
    return "XSG_BUILD_INPUTS_V1 "+record["build_id"]+" "+record["binary_sha256"]

if __name__=="__main__":
    import argparse
    parser=argparse.ArgumentParser()
    parser.add_argument("--manifest",required=True)
    parser.add_argument("--binary",required=True)
    a=parser.parse_args()
    print(check_link_inputs(a.manifest,a.binary))
