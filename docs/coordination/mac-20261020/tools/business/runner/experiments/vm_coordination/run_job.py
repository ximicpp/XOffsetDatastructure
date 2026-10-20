#!/usr/bin/env python3
"""macOS-native stand-in for experiments/vm_coordination/run_job.py (queue supervisor).

The original WSL runner is not part of the restore. campaign.py only depends on
its *observable protocol*, which this file reproduces natively:

  * an exclusive BSD flock on <lock_dir>/queue.lock, held for the whole child
    lifetime, whose JSON body is {"status":"running","supervisor_pid":<this pid>,
    "run_id":..., "owner":...}  (checked by campaign.active_queue);
  * <out>/request.json written before the child starts, with command / owner /
    kind / runner_sha256 / lock_dir / out / cwd / run_id / created_at /
    timeout_seconds / wait_seconds / termination_grace_seconds;
  * the child is this process's *direct* child (os.getppid() == supervisor_pid),
    launched with the exact argv after "--", in this process's cwd;
  * after exit: result.json, events.json, supervisor.log, stdout.log,
    stderr.log and manifest.json ({"files":[{"path","bytes"(int),"sha256"}]}),
    exactly the six members campaign.queue_audit expects.

lock_dir is fixed to parents[2]/build/vm-coordination of this file, so the
unmodified campaign check `lock_dir == RUNNER.parents[2]/"build/vm-coordination"`
holds. This is a single-host serialisation lease, NOT the historical global
WSL lease, and makes no claim about other machines.

Usage:
  python3 -B run_job.py run --owner W3 --kind functional --out <ABS>/queue \
      --timeout-seconds 3600 [--wait-seconds 0] -- <argv...>
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import errno
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
import uuid

HERE = Path(__file__).resolve()
LOCK_DIR = HERE.parents[2] / "build/vm-coordination"
QUEUE_MEMBERS = ("request.json", "result.json", "events.json", "supervisor.log", "stdout.log", "stderr.log")


def now():
    return datetime.now(timezone.utc).isoformat()


def sha(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def dump(path, value):
    with open(path, "x", encoding="utf-8") as target:
        target.write(json.dumps(value, indent=2, sort_keys=True, ensure_ascii=False) + "\n")


def group_alive(pgid):
    try:
        os.killpg(pgid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if "--" not in argv:
        raise SystemExit("usage: run_job.py run ... -- <command>")
    split = argv.index("--")
    head, command = argv[:split], argv[split + 1:]
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=["run"])
    parser.add_argument("--owner", required=True)
    parser.add_argument("--kind", required=True, choices=["functional", "timing", "validation"])
    parser.add_argument("--out", required=True)
    parser.add_argument("--timeout-seconds", type=float, required=True)
    parser.add_argument("--wait-seconds", type=float, default=0)
    args = parser.parse_args(head)
    if not command:
        raise SystemExit("empty command")
    queue = Path(args.out)
    if not queue.is_absolute() or queue.exists():
        raise SystemExit("queue output must be absolute and fresh: " + str(queue))
    if not 0 < args.timeout_seconds <= 3600 or args.wait_seconds < 0:
        raise SystemExit("invalid timeout/wait")
    bundle = queue.parent
    bundle.mkdir(parents=True, exist_ok=True)
    if any(bundle.iterdir()):
        raise SystemExit("bundle directory must be empty before the queue starts: " + str(bundle))
    LOCK_DIR.mkdir(parents=True, exist_ok=True)
    events = []

    def event(name, **extra):
        events.append(dict(t=now(), monotonic_ns=str(time.monotonic_ns()), event=name, **extra))

    lock_fd = os.open(LOCK_DIR / "queue.lock", os.O_RDWR | os.O_CREAT, 0o644)
    deadline = time.monotonic() + args.wait_seconds
    while True:
        try:
            fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            break
        except OSError as error:
            if error.errno not in (errno.EWOULDBLOCK, errno.EAGAIN) or time.monotonic() >= deadline:
                os.close(lock_fd)
                raise SystemExit("queue lease busy: " + str(LOCK_DIR / "queue.lock"))
            time.sleep(0.5)
    run_id = uuid.uuid4().hex
    queue.mkdir()
    cwd = os.getcwd()
    request = dict(schema_version=1, run_id=run_id, owner=args.owner, kind=args.kind, command=command,
                   cwd=cwd, out=str(queue), lock_dir=str(LOCK_DIR), runner_sha256=sha(HERE),
                   runner_path=str(HERE), created_at=now(), timeout_seconds=args.timeout_seconds,
                   wait_seconds=args.wait_seconds, termination_grace_seconds=2,
                   platform="darwin-native-mac-run_job-v1")
    dump(queue / "request.json", request)
    record = dict(status="running", supervisor_pid=os.getpid(), run_id=run_id, owner=args.owner,
                  kind=args.kind, out=str(queue), started_at=now())
    os.ftruncate(lock_fd, 0)
    os.pwrite(lock_fd, json.dumps(record, sort_keys=True).encode(), 0)
    os.fsync(lock_fd)
    event("lease-acquired", run_id=run_id)
    status, returncode, process = "error", None, None
    leftovers = []
    started = time.monotonic()
    with open(queue / "stdout.log", "xb") as stdout, open(queue / "stderr.log", "xb") as stderr:
        try:
            process = subprocess.Popen(command, cwd=cwd, stdin=subprocess.DEVNULL, stdout=stdout,
                                       stderr=stderr, start_new_session=True)
            event("child-started", pid=process.pid)
            try:
                returncode = process.wait(timeout=args.timeout_seconds)
                status = "complete" if returncode == 0 else "failed"
            except subprocess.TimeoutExpired:
                status = "timeout"
                event("timeout")
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    returncode = process.wait(timeout=request["termination_grace_seconds"])
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    returncode = process.wait()
        except OSError as error:
            event("launch-error", error=type(error).__name__ + ": " + str(error))
            status = "error"
    elapsed = time.monotonic() - started
    if process is not None and group_alive(process.pid):
        leftovers.append(process.pid)
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        status = "error"
    event("child-finished", returncode=returncode, status=status)
    finished_at = now()
    result = dict(schema_version=1, run_id=run_id, owner=args.owner, kind=args.kind, status=status,
                  command_returncode=returncode, command_elapsed_seconds=max(elapsed, 1e-6),
                  created_at=request["created_at"], finished_at=finished_at,
                  cleanup=dict(live=[], process_groups=leftovers, zombies=[]))
    dump(queue / "result.json", result)
    dump(queue / "events.json", events)
    with open(queue / "supervisor.log", "x", encoding="utf-8") as log:
        log.write("mac run_job.py run_id=%s status=%s returncode=%s elapsed=%.6f\n" % (run_id, status, returncode, elapsed))
    files = [dict(path=name, bytes=(queue / name).stat().st_size, sha256=sha(queue / name)) for name in QUEUE_MEMBERS]
    dump(queue / "manifest.json", dict(schema_version=1, files=files))
    os.ftruncate(lock_fd, 0)
    os.pwrite(lock_fd, json.dumps(dict(status="idle", last_run_id=run_id, finished_at=finished_at), sort_keys=True).encode(), 0)
    fcntl.flock(lock_fd, fcntl.LOCK_UN)
    os.close(lock_fd)
    print(json.dumps(dict(run_id=run_id, status=status, command_returncode=returncode), sort_keys=True))
    return 0 if status == "complete" else 1


if __name__ == "__main__":
    raise SystemExit(main())
