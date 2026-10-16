#!/usr/bin/env python3
"""Serialize local VM experiments and retain exclusive, auditable evidence.

POSIX only. Commands must keep descendants in their inherited session.
Docker and other daemon jobs are currently unsupported and must not be hidden
inside scripts. They need a separately validated ownership/cleanup adapter.
"""
from __future__ import annotations

import argparse
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import select
import signal
import stat
import subprocess
import sys
import time
import uuid


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_LOCK_DIR = ROOT / "build/vm-coordination"
EXIT = {"complete": 0, "failed": 1, "error": 70, "busy": 75,
        "timeout": 124, "cancelled": 130}


def utc():
    from datetime import datetime, timezone
    return datetime.now(timezone.utc).isoformat()


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def exclusive_json(path, value):
    with open(path, "x", encoding="utf-8") as target:
        json.dump(value, target, indent=2, sort_keys=True)
        target.write("\n")
        target.flush()
        os.fsync(target.fileno())


def emit(value):
    print(json.dumps(value, sort_keys=True), flush=True)


def lock_open(directory):
    directory.mkdir(parents=True, exist_ok=True)
    # Never unlink/replace this inode, including when recovering stale metadata.
    return os.open(directory / "queue.lock", os.O_RDWR | os.O_CREAT, 0o600)


def lock_record(fd, value):
    raw = (json.dumps(value, sort_keys=True) + "\n").encode()
    os.lseek(fd, 0, os.SEEK_SET)
    os.write(fd, raw)
    os.ftruncate(fd, len(raw))
    os.fsync(fd)


def lock_metadata(fd):
    try:
        os.lseek(fd, 0, os.SEEK_SET)
        return json.loads(os.read(fd, 65536))
    except (ValueError, OSError):
        return None


def status(directory):
    fd = lock_open(directory)
    try:
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return {"status": "busy", "lock": str(directory / "queue.lock"),
                    "metadata": lock_metadata(fd)}
        return {"status": "idle", "lock": str(directory / "queue.lock"),
                "last_record": lock_metadata(fd)}
    finally:
        os.close(fd)


def cancel_reason():
    # The public client owns the write end. EOF also catches a killed client.
    ready, _, _ = select.select([sys.stdin.fileno()], [], [], 0)
    if ready:
        data = os.read(sys.stdin.fileno(), 4096)
        return "client_cancelled" if data else "client_disconnected"
    return None


def group_members(pgid):
    # Only queried after the leader exits or during cleanup, never in the
    # ordinary timing loop. A zombie cannot execute and is reported separately.
    probe = subprocess.run(["ps", "-axo", "pid=,pgid=,stat="],
                           capture_output=True, text=True, timeout=5)
    if probe.returncode:
        raise RuntimeError("cannot inspect owned process group: " + probe.stderr)
    live, zombies, groups = [], [], set()
    for line in probe.stdout.splitlines():
        fields = line.split()
        if len(fields) < 3:
            continue
        pid = int(fields[0])
        try:
            owned = os.getsid(pid) == pgid
            current_group = os.getpgid(pid) if owned else None
        except ProcessLookupError:
            continue
        if owned:
            (zombies if fields[2].startswith("Z") else live).append(pid)
            if not fields[2].startswith("Z"):
                groups.add(current_group)
    return {"live": sorted(live), "zombies": sorted(zombies), "process_groups": sorted(groups)}


def signal_group(session_id, sig):
    # Signal only freshly revalidated owned PIDs. Never send to a PGID from
    # a stale ps snapshot (whose numeric identity could have been reused).
    members = group_members(session_id)
    for pid in members["live"]:
        try:
            if os.getsid(pid) == session_id:
                os.kill(pid, sig)
        except ProcessLookupError:
            pass


def stop_group(process, grace, events):
    pgid = process.pid
    signal_group(pgid, signal.SIGTERM)
    events.append({"event": "group_sigterm", "pgid": pgid, "at": utc()})
    deadline = time.monotonic() + grace
    while time.monotonic() < deadline:
        process.poll()
        members = group_members(pgid)
        if not members["live"]:
            process.wait()
            return members
        signal_group(pgid, signal.SIGTERM)
        time.sleep(0.05)
    signal_group(pgid, signal.SIGKILL)
    events.append({"event": "group_sigkill", "pgid": pgid, "at": utc()})
    # Do not release the queue lock while any owned group member can execute.
    # Uninterruptible kernel tasks may take longer than the requested timeout.
    while True:
        process.poll()
        members = group_members(pgid)
        if not members["live"]:
            process.wait()
            return members
        signal_group(pgid, signal.SIGKILL)
        time.sleep(0.1)


def ensure_stopped(process, grace, events):
    while True:
        try:
            return stop_group(process, grace, events)
        except Exception as error:
            # Inspection failure is not evidence that work ended. Preserve the
            # lock and keep trying, rather than unlocking on a cleanup error.
            events.append({"event": "group_cleanup_pending", "error": str(error), "at": utc()})
            time.sleep(1)


def manifest(out):
    files = []
    for path in sorted(out.rglob("*")):
        if path == out / "manifest.json":
            continue
        kind = path.lstat().st_mode
        if stat.S_ISDIR(kind):
            continue
        if not stat.S_ISREG(kind):
            raise ValueError("evidence must contain regular files/directories, not links or special files")
        files.append({"path": path.relative_to(out).as_posix(),
                      "bytes": path.stat().st_size, "sha256": digest(path)})
    return files


def worker(out):
    request = json.loads((out / "request.json").read_text())
    events = []
    result = {"schema_version": 1, "run_id": request["run_id"], "owner": request["owner"],
              "kind": request["kind"], "status": "error", "command_returncode": None,
              "started_at": None, "finished_at": None, "cleanup": None}
    fd = lock_open(Path(request["lock_dir"]))
    acquired = False
    process = None
    signal_state = []

    def interrupted(signum, frame):
        signal_state.append(signum)

    for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
        signal.signal(sig, interrupted)
    try:
        if digest(__file__) != request["runner_sha256"]:
            raise RuntimeError("runner source changed before supervisor startup")
        wait_start = time.monotonic()
        while True:
            reason = cancel_reason()
            if signal_state or reason:
                result.update(status="cancelled", reason=reason or "supervisor_signal")
                break
            try:
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                acquired = True
                break
            except BlockingIOError:
                if time.monotonic() - wait_start >= request["wait_seconds"]:
                    result.update(status="busy", reason="queue_wait_expired",
                                  lock_holder=lock_metadata(fd))
                    break
                time.sleep(0.05)
        if acquired:
            result["started_at"] = utc()
            lock_record(fd, {"status": "running", "supervisor_pid": os.getpid(),
                             "run_id": request["run_id"], "owner": request["owner"],
                             "kind": request["kind"], "out": str(out),
                             "started_at": result["started_at"]})
            with open(out / "stdout.log", "xb") as stdout, open(out / "stderr.log", "xb") as stderr:
                started = time.monotonic()
                process = subprocess.Popen(request["command"], cwd=request["cwd"],
                                           stdin=subprocess.DEVNULL,
                                           stdout=stdout, stderr=stderr,
                                           start_new_session=True)
                result["command_pid"] = process.pid
                while True:
                    reason = cancel_reason()
                    if signal_state or reason:
                        result.update(status="cancelled", reason=reason or "supervisor_signal")
                        result["cleanup"] = ensure_stopped(process, request["termination_grace_seconds"], events)
                        break
                    if time.monotonic() - started >= request["timeout_seconds"]:
                        result.update(status="timeout", reason="command_deadline_expired")
                        result["cleanup"] = ensure_stopped(process, request["termination_grace_seconds"], events)
                        break
                    rc = process.poll()
                    if rc is not None:
                        members = group_members(process.pid)
                        if members["live"]:
                            # A successful leader that leaves work behind is
                            # a failed job; clean its orphaned group first.
                            result.update(status="failed", reason="leader_left_running_descendants")
                            result["cleanup"] = ensure_stopped(process, request["termination_grace_seconds"], events)
                        else:
                            result.update(status="complete" if rc == 0 else "failed",
                                          reason="command_exit", cleanup=members)
                        break
                    time.sleep(0.05)
                result["command_returncode"] = process.wait()
                result["command_elapsed_seconds"] = time.monotonic() - started
    except BaseException as error:
        result.update(status="error", reason=type(error).__name__ + ": " + str(error))
        if process is not None:
            result["cleanup"] = ensure_stopped(process, request["termination_grace_seconds"], events)
            result["command_returncode"] = process.wait()
    finally:
        result["finished_at"] = utc()
        exclusive_json(out / "events.json", events)
        exclusive_json(out / "result.json", result)
        # This is the supervisor's final log write. Include it in the manifest.
        emit(result)
        exclusive_json(out / "manifest.json", {"schema_version": 1, "files": manifest(out)})
        if acquired:
            lock_record(fd, {"status": result["status"], "run_id": request["run_id"],
                             "owner": request["owner"], "out": str(out),
                             "finished_at": result["finished_at"]})
        os.close(fd)
    return EXIT[result["status"]]


def positive(value):
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise argparse.ArgumentTypeError("must be finite and positive")
    return number


def nonnegative(value):
    number = float(value)
    if not math.isfinite(number) or number < 0:
        raise argparse.ArgumentTypeError("must be finite and nonnegative")
    return number


def audit(out):
    expected = json.loads((out / "manifest.json").read_text())
    if expected != {"schema_version": 1, "files": manifest(out)}:
        raise ValueError("evidence manifest mismatch")
    request = json.loads((out / "request.json").read_text())
    result = json.loads((out / "result.json").read_text())
    if request["run_id"] != result["run_id"] or result["status"] not in EXIT:
        raise ValueError("result identity/status mismatch")
    if result["status"] == "complete":
        if result["command_returncode"] != 0 or not result["started_at"] or result["cleanup"]["live"]:
            raise ValueError("invalid COMPLETE evidence")
    return {"status": "audit_passed", "job_status": result["status"], "run_id": result["run_id"]}


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "_worker":
        return worker(Path(sys.argv[2]))
    parser = argparse.ArgumentParser(description=__doc__)
    subs = parser.add_subparsers(dest="action", required=True)
    check = subs.add_parser("status")
    check.add_argument("--lock-dir", type=Path, default=DEFAULT_LOCK_DIR)
    verify = subs.add_parser("audit", help="offline integrity check; never executes the command")
    verify.add_argument("--out", type=Path, required=True)
    run = subs.add_parser("run")
    run.add_argument("--owner", required=True, choices=("W1", "W2", "W3", "W4"))
    run.add_argument("--kind", required=True, choices=("build", "functional", "diagnostic", "timing", "validation"))
    run.add_argument("--out", type=Path, required=True)
    run.add_argument("--timeout-seconds", required=True, type=positive)
    run.add_argument("--wait-seconds", type=nonnegative, default=15)
    run.add_argument("--termination-grace-seconds", type=positive, default=2)
    run.add_argument("--lock-dir", type=Path, default=DEFAULT_LOCK_DIR,
                     help="isolated test queues only; production jobs use the default")
    run.add_argument("--docker", action="store_true", help="reserved; daemon jobs are not currently supported")
    run.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    try:
        if args.action == "status":
            emit(status(args.lock_dir.resolve()))
            return 0
        if args.action == "audit":
            emit(audit(args.out.resolve()))
            return 0
        command = args.command[1:] if args.command[:1] == ["--"] else args.command
        if not command:
            parser.error("run requires -- COMMAND ARG...")
        if Path(command[0]).name in ("docker", "podman", "nerdctl", "ctr") or args.docker:
            parser.error("daemon jobs are prerequisite_unavailable: validated ownership/stop adapter required")
        out = args.out.resolve()
        out.mkdir(parents=True, exist_ok=False)
        run_id = uuid.uuid4().hex
        request = {"schema_version": 1, "run_id": run_id, "owner": args.owner, "kind": args.kind,
                   "created_at": utc(), "command": command, "cwd": os.getcwd(),
                   "out": str(out), "lock_dir": str(args.lock_dir.resolve()),
                   "timeout_seconds": args.timeout_seconds, "wait_seconds": args.wait_seconds,
                   "termination_grace_seconds": args.termination_grace_seconds,
                   "runner_sha256": digest(__file__),
                   "source_headers": {p.name: digest(p) for p in
                                      (ROOT / "xoffsetdatastructure.hpp", ROOT / "xoffset_virtual_memory.hpp")},
                   "process_contract": "descendants must not detach from inherited session; daemon jobs require explicit adapter"}
        exclusive_json(out / "request.json", request)
        client_signals = []

        def cancel(signum, frame):
            client_signals.append(signum)

        for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            signal.signal(sig, cancel)
        with open(out / "supervisor.log", "xb") as log:
            supervisor = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), "_worker", str(out)],
                                          stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT,
                                          start_new_session=True)
            try:
                cancelled = False
                while supervisor.poll() is None:
                    if client_signals and not cancelled:
                        supervisor.stdin.write(b"cancel\n")
                        supervisor.stdin.flush()
                        cancelled = True
                    time.sleep(0.05)
                rc = supervisor.wait()
            finally:
                supervisor.stdin.close()
        if (out / "result.json").is_file() and (out / "manifest.json").is_file():
            result = json.loads((out / "result.json").read_text())
            if rc == EXIT.get(result["status"]):
                emit(result)
            else:
                emit({"status": "error", "reason": "supervisor exit/result mismatch", "out": str(out)})
                return EXIT["error"]
        else:
            emit({"status": "error", "reason": "supervisor ended without result", "out": str(out)})
            return EXIT["error"]
        return rc
    except (OSError, ValueError, KeyError) as error:
        emit({"status": "error", "reason": type(error).__name__ + ": " + str(error)})
        return EXIT["error"]


if __name__ == "__main__":
    sys.exit(main())
