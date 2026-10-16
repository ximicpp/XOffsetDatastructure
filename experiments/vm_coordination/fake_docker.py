#!/usr/bin/env python3
"""Synthetic Docker CLI: no sockets, daemon, containers, or network are used.

FAKE_DOCKER_STATE must name a harness-owned directory containing state.json.
The adapter deliberately leaves its JSON container running when its CLI dies.
"""
from __future__ import annotations

from contextlib import contextmanager
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import sys
import time


def now():
    return datetime.now(timezone.utc).isoformat()


def process_exists(pid):
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False


@contextmanager
def transaction():
    root = Path(os.environ["FAKE_DOCKER_STATE"])
    with open(root / "state.lock", "a+b") as lock:
        fcntl.flock(lock.fileno(), fcntl.LOCK_EX)
        state = json.loads((root / "state.json").read_text())
        if state.get("synthetic") is not True:
            raise ValueError("refusing state without the synthetic marker")
        yield root, state
        with open(root / "state.json", "w", encoding="utf-8") as target:
            json.dump(state, target, indent=2, sort_keys=True)
            target.write("\n")


def log(root, event, **fields):
    record = {"synthetic": True, "event": event, "argv": sys.argv[1:],
              "pid": os.getpid(), "at": now(), **fields}
    with open(root / "calls.jsonl", "a", encoding="utf-8") as target:
        target.write(json.dumps(record, sort_keys=True) + "\n")


def find_container(state, identity):
    return next((c for name, c in state["containers"].items()
                 if identity in (name, c["Id"])), None)


def run(args):
    options = {}
    rest = list(args)
    while rest and rest[0].startswith("--"):
        option = rest.pop(0)
        if option == "--rm":
            continue
        if option not in ("--name", "--label", "--cidfile") or not rest:
            raise ValueError("unsupported synthetic run option: " + option)
        if option in options:
            raise ValueError("duplicate synthetic run option: " + option)
        options[option] = rest.pop(0)
    if set(options) != {"--name", "--label", "--cidfile"} or len(rest) != 2:
        raise ValueError("expected injected name, label, cidfile, image, and mode")
    if rest[0] != "synthetic-image" or rest[1] not in ("stopped", "absent", "running"):
        raise ValueError("only the synthetic image and registered modes are supported")
    label_key, label_value = options["--label"].split("=", 1)
    if label_key != "xoffset.vm_job" or not label_value:
        raise ValueError("runner ownership label missing")
    name, mode = options["--name"], rest[1]
    identity = hashlib.sha256(("synthetic:" + name).encode()).hexdigest()
    container = {"Id": identity, "Name": "/" + name,
                 "Config": {"Labels": {label_key: label_value}},
                 "State": {"Running": True, "Status": "running", "ExitCode": 0},
                 "synthetic": True, "client_pid": os.getpid(), "mode": mode}
    with transaction() as (root, state):
        if name in state["containers"]:
            raise ValueError("synthetic name already exists")
        with open(options["--cidfile"], "x", encoding="utf-8") as target:
            target.write(identity + "\n")
        state["containers"][name] = container
        log(root, "run_created", name=name, id=identity, label=label_value, mode=mode)
    if mode == "running":
        while True:
            time.sleep(0.05)
    with transaction() as (root, state):
        if mode == "absent":
            del state["containers"][name]
            log(root, "run_removed", name=name, id=identity)
        else:
            state["containers"][name]["State"].update(Running=False, Status="exited")
            log(root, "run_stopped", name=name, id=identity)
    print("synthetic run completed", flush=True)
    return 0


def main():
    args = sys.argv[1:]
    if args == ["info", "--format", "{{.ID}}"]:
        with transaction() as (root, state):
            log(root, "info", daemon_id=state["daemon_id"])
            print(state["daemon_id"])
        return 0
    if args[:1] == ["run"]:
        return run(args[1:])
    if args[:2] == ["container", "inspect"] and len(args) == 3:
        with transaction() as (root, state):
            container = find_container(state, args[2])
            log(root, "inspect", identity=args[2], found=container is not None,
                id=container["Id"] if container else None,
                running=container["State"]["Running"] if container else None,
                label=container["Config"]["Labels"]["xoffset.vm_job"] if container else None,
                client_alive=process_exists(container["client_pid"]) if container else None)
            if container is None:
                print("Error response from daemon: No such container: " + args[2], file=sys.stderr)
                return 1
            print(json.dumps([container], sort_keys=True))
        return 0
    if args[:2] in (["container", "stop"], ["container", "kill"]):
        operation = args[1]
        if operation == "stop" and len(args) == 5 and args[2:4] == ["--time", "2"]:
            identity = args[4]
        elif operation == "kill" and len(args) == 3:
            identity = args[2]
        else:
            raise ValueError("unexpected synthetic control arguments")
        with transaction() as (root, state):
            container = find_container(state, identity)
            if container is None or identity != container["Id"]:
                raise ValueError("synthetic stop/kill requires the exact owned container ID")
            before = container["State"]["Running"]
            refused = operation == "stop" and state.get("stop_fails", False)
            if not refused:
                container["State"].update(Running=False, Status="exited", ExitCode=137 if operation == "kill" else 143)
            log(root, operation, identity=identity, id=container["Id"],
                label=container["Config"]["Labels"]["xoffset.vm_job"],
                before_running=before, after_running=container["State"]["Running"],
                client_alive=process_exists(container["client_pid"]), refused=refused)
            if refused:
                print("synthetic stop refusal", file=sys.stderr)
                return 1
            print(identity)
        return 0
    raise ValueError("unsupported synthetic Docker command")


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (KeyError, OSError, ValueError) as error:
        print("synthetic Docker error: " + str(error), file=sys.stderr)
        sys.exit(2)
