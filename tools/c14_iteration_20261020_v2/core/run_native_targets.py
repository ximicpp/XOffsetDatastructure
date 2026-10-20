"""Execute an exact finite request only while the original shared lease is held.

No scheduler, second lock, timing campaign or retry policy is introduced here.
Validation owns invocation and results. This tool accepts only the prebound
source/compiler argv in one reviewed request, and checks their input identities.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


def fid(p):
    p = Path(p)
    return {"bytes": p.stat().st_size, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}


def matches(p, identity):
    actual = fid(p)
    return actual["sha256"] == identity["sha256"] and ("bytes" not in identity or actual["bytes"] == identity["bytes"])


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--request", type=Path, required=True)
    a = p.parse_args()
    q = json.loads(a.request.read_text(encoding="utf-8"))
    if q["status"] != "ready_for_validation_review" or q["timing_admitted"]:
        raise ValueError("request not a finite correctness registration")
    lock = Path(q["global_lease_file"])
    with lock.open("r+") as f:
        try:
            fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            pass
        else:
            fcntl.flock(f, fcntl.LOCK_UN)
            raise ValueError("original global lease is not held; refusing native execution")
        f.seek(0)
        holder = json.load(f)
    if holder["status"] != "running":
        raise ValueError("lease not running")
    out = Path(q["output"])
    allowed = Path("/root/xoffset-c14-iteration/20261020/v2/validation")
    if allowed not in out.parents:
        raise ValueError("output outside new validation tree")
    for path, identity in q["input_files"].items():
        if not matches(path, identity):
            raise ValueError("changed input: " + path)
    for name, value in q["source_members"].items():
        path = Path(q["source"]) / name
        if fid(path) != {"bytes": value["bytes"], "sha256": value["candidate_sha256"]}:
            raise ValueError("changed candidate member: " + name)
    out.mkdir(parents=True, exist_ok=False)
    receipt = {"request": {"path": str(a.request), **fid(a.request)},
               "candidate": q["candidate"], "status": "running", "lease": holder,
               "commands": [], "timing_samples": 0}
    env = dict(PATH="/usr/bin:/bin", LANG="C", LC_ALL="C", TZ="UTC",
               ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:allocator_may_return_null=1",
               UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    try:
        for job in q["commands"]:
            start = time.monotonic()
            record = {"label": job["label"], "argv": job["argv"], "kind": job["kind"]}
            try:
                with (out / (job["label"] + ".log")).open("xb") as log:
                    result = subprocess.run(job["argv"], stdout=log, stderr=subprocess.STDOUT,
                        env=env, timeout=job["timeout_seconds"], check=False)
                record["returncode"] = result.returncode
            finally:
                record["elapsed_seconds"] = time.monotonic() - start
                receipt["commands"].append(record)
            if result.returncode:
                raise RuntimeError(job["label"] + " failed; diagnose before any new revision")
        for path, identity in q["input_files"].items():
            if not matches(path, identity):
                raise ValueError("input changed after execution: " + path)
        receipt["status"] = "passed"
    except BaseException as error:
        receipt["status"] = "failed"
        receipt["error"] = str(error)
        raise
    finally:
        (out / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()
