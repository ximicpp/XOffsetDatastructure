#!/usr/bin/env python3
"""Run only synthetic daemon-job refusal checks; no real Docker CLI or daemon.

Each case owns its fake state, CLI symlink, clients, evidence, and queue lock.
Use --evidence-root NEW_DIRECTORY; existing evidence is never overwritten.
FutureAdapterControls is an unexecuted mixin, excluded from unittest discovery.
Its old run/inspect controls do not prove safe create/start ownership handling.
"""
from contextlib import contextmanager
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
import unittest


RUNNER = Path(__file__).with_name("run_job.py").resolve()
FAKE = Path(__file__).with_name("fake_docker.py").resolve()
SOURCE = Path(__file__).resolve()
EVIDENCE = None
DAEMON = "synthetic-daemon-original"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_new(path, value):
    with open(path, "x", encoding="utf-8") as target:
        json.dump(value, target, indent=2, sort_keys=True)
        target.write("\n")


class FutureAdapterControls:
    """Unexecuted synthetic controls for a future separately reviewed adapter.

    Deliberately not a TestCase: neither this CLI nor unittest discovery loads it.
    These scenarios need updating for a proven create/verify/start handshake.
    """
    def setUp(self):
        self.root = EVIDENCE / self._testMethodName
        self.root.mkdir()
        self.lock = self.root / "lock"
        self.state_root = self.root / "fake-state"
        self.state_root.mkdir()
        write_new(self.state_root / "state.json", {"synthetic": True, "daemon_id": DAEMON,
                                                   "containers": {}, "stop_fails": False})
        binary_dir = self.root / "bin"
        binary_dir.mkdir()
        self.docker = binary_dir / "docker"
        self.docker.symlink_to(FAKE)
        self.env = {**os.environ, "FAKE_DOCKER_STATE": str(self.state_root),
                    "PYTHONDONTWRITEBYTECODE": "1"}
        self.clients = []
        self.commands = []

    @contextmanager
    def state(self):
        with open(self.state_root / "state.lock", "a+b") as lock:
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX)
            value = json.loads((self.state_root / "state.json").read_text())
            yield value
            with open(self.state_root / "state.json", "w", encoding="utf-8") as target:
                json.dump(value, target, indent=2, sort_keys=True)
                target.write("\n")

    def tearDown(self):
        # Restore only owned fake state so pending supervisors can prove cleanup.
        with self.state() as state:
            state["daemon_id"] = DAEMON
            state["stop_fails"] = False
        try:
            for client in self.clients:
                if client.poll() is None:
                    client.send_signal(signal.SIGTERM)
                    client.wait(timeout=15)
                if client.stdout:
                    client.stdout.close()
                if client.stderr:
                    client.stderr.close()
            self.await_condition(lambda: self.queue_status()["status"] == "idle",
                                 "owned queue did not release", timeout=15)
            with self.state() as state:
                self.assertFalse(any(c["State"]["Running"] for c in state["containers"].values()),
                                 "owned fake container still running after cleanup")
        finally:
            write_new(self.root / "commands.json", self.commands)

    def args(self, out="job", mode="stopped", timeout=2, docker=True, command=None):
        args = [sys.executable, str(RUNNER), "run", "--owner", "W4", "--kind", "validation",
                "--out", str(self.root / out), "--timeout-seconds", str(timeout),
                "--wait-seconds", "0", "--termination-grace-seconds", "0.1",
                "--lock-dir", str(self.lock)]
        if docker:
            args.append("--docker")
        return args + ["--", *(command or [str(self.docker), "run", "synthetic-image", mode])]

    def start(self, args, label):
        stdout = open(self.root / (label + ".client.stdout"), "xb")
        stderr = open(self.root / (label + ".client.stderr"), "xb")
        try:
            client = subprocess.Popen(args, stdout=stdout, stderr=stderr, env=self.env)
        finally:
            stdout.close()
            stderr.close()
        self.clients.append(client)
        self.commands.append({"argv": args, "client_pid": client.pid,
                              "stdout": label + ".client.stdout", "stderr": label + ".client.stderr"})
        return client

    def execute(self, args, label):
        client = self.start(args, label)
        rc = client.wait(timeout=20)
        self.commands[-1]["returncode"] = rc
        return rc

    def read_json(self, path):
        return json.loads(path.read_text())

    def await_condition(self, predicate, message, timeout=7):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            if predicate():
                return
            time.sleep(0.02)
        self.fail(message)

    def result(self, out="job"):
        path = self.root / out / "result.json"
        self.await_condition(path.exists, "missing result: " + str(path))
        return self.read_json(path)

    def calls(self):
        path = self.state_root / "calls.jsonl"
        if not path.exists():
            return []
        with self.state():
            return [json.loads(line) for line in path.read_text().splitlines()]

    def queue_status(self):
        args = [sys.executable, str(RUNNER), "status", "--lock-dir", str(self.lock)]
        result = subprocess.run(args, capture_output=True, text=True, env=self.env, timeout=10)
        self.commands.append({"argv": args, "returncode": result.returncode,
                              "stdout": result.stdout, "stderr": result.stderr})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return json.loads(result.stdout)

    def await_created(self):
        self.await_condition(lambda: any(c["event"] == "run_created" for c in self.calls()),
                             "fake run did not create owned state")

    def assert_owned_stop(self, out="job", operation="stop"):
        request = self.read_json(self.root / out / "request.json")
        identity = (self.root / out / "container.cid").read_text().strip()
        calls = self.calls()
        controls = [c for c in calls if c["event"] == operation]
        self.assertEqual(len(controls), 1, calls)
        control = controls[0]
        self.assertEqual(control["identity"], identity)
        self.assertEqual(control["label"], request["run_id"])
        self.assertTrue(control["before_running"])
        self.assertFalse(control["after_running"])
        self.assertFalse(control["client_alive"], "JSON container must outlive the fake CLI")
        prior = calls[:calls.index(control)]
        self.assertTrue(any(c["event"] == "inspect" and c["id"] == identity and
                            c["label"] == request["run_id"] and c["running"] for c in prior))
        self.assertFalse(self.result(out)["cleanup"]["live"])

    def audit(self, out="job"):
        args = [sys.executable, str(RUNNER), "audit", "--out", str(self.root / out)]
        self.assertEqual(self.execute(args, "audit-" + out), 0)

    def test_success_stopped(self):
        self.assertEqual(self.execute(self.args(), "run"), 0)
        result = self.result()
        self.assertEqual(result["status"], "complete")
        self.assertEqual(result["docker_cleanup"]["status"], "stopped")
        self.assertFalse(result["docker_cleanup"]["forced_stop"])
        self.assertFalse(any(c["event"] in ("stop", "kill") for c in self.calls()))
        self.audit()

    def test_success_absent(self):
        self.assertEqual(self.execute(self.args(mode="absent"), "run"), 0)
        result = self.result()
        self.assertEqual(result["status"], "complete")
        self.assertEqual(result["docker_cleanup"]["status"], "absent")
        self.assertEqual(result["docker_cleanup"]["daemon_id"], DAEMON)
        self.assertFalse(any(c["event"] in ("stop", "kill") for c in self.calls()))
        self.audit()

    def test_timeout_stops_remaining_owned_container_by_id(self):
        self.assertEqual(self.execute(self.args(mode="running", timeout=0.4), "run"), 124)
        self.assertEqual(self.result()["status"], "timeout")
        self.assertEqual(self.result()["docker_cleanup"]["status"], "stopped")
        self.assert_owned_stop()
        self.audit()

    def test_stop_failure_uses_owned_id_kill(self):
        with self.state() as state:
            state["stop_fails"] = True
        self.assertEqual(self.execute(self.args(mode="running", timeout=0.4), "run"), 124)
        self.assert_owned_stop(operation="kill")
        stops = [c for c in self.calls() if c["event"] == "stop"]
        self.assertEqual(len(stops), 1)
        self.assertTrue(stops[0]["refused"])
        self.assertTrue(stops[0]["after_running"])
        self.audit()

    def test_killed_public_client_still_stops_owned_container(self):
        client = self.start(self.args(mode="running", timeout=10), "run")
        self.await_created()
        client.kill()
        self.assertEqual(client.wait(timeout=5), -signal.SIGKILL)
        self.assertEqual(self.result()["status"], "cancelled")
        self.assertEqual(self.result()["reason"], "client_disconnected")
        self.assert_owned_stop()
        self.await_condition(lambda: (self.root / "job/manifest.json").exists(), "missing final manifest")
        self.audit()

    def test_non_docker_missing_binary_is_error(self):
        args = self.args(docker=False, command=[str(self.root / "missing-command")])
        self.assertEqual(self.execute(args, "run"), 70)
        self.assertEqual(self.result()["status"], "error")
        self.assertIsNone(self.result()["command_returncode"])
        self.assertNotIn("docker_cleanup", self.result())
        self.assertEqual(self.calls(), [])
        self.audit()

    def test_daemon_mismatch_retains_lock_until_identity_restored(self):
        client = self.start(self.args(mode="running", timeout=0.5), "run")
        self.await_created()
        with self.state() as state:
            state["daemon_id"] = "synthetic-daemon-replaced"
        pending = self.root / "job/cleanup-pending-1.json"
        self.await_condition(pending.exists, "daemon mismatch did not retain cleanup checkpoint")
        self.assertEqual(self.read_json(pending)["status"], "cleanup_pending")
        queue = self.queue_status()
        self.assertEqual(queue["status"], "busy")
        self.assertEqual(queue["metadata"]["status"], "cleanup_pending")
        self.assertFalse((self.root / "job/result.json").exists())
        self.assertIsNone(client.poll())
        with self.state() as state:
            self.assertTrue(next(iter(state["containers"].values()))["State"]["Running"])
        self.assertFalse(any(c["event"] in ("stop", "kill") for c in self.calls()))
        contender = self.args(out="busy", docker=False,
                              command=[sys.executable, "-c", "raise RuntimeError('must not run')"])
        self.assertEqual(self.execute(contender, "contender"), 75)
        self.assertEqual(self.result("busy")["status"], "busy")
        self.assertFalse((self.root / "busy/stdout.log").exists())
        with self.state() as state:
            state["daemon_id"] = DAEMON
        self.assertEqual(client.wait(timeout=10), 70)
        self.assertEqual(self.result()["status"], "error")
        self.assertEqual(self.result()["docker_cleanup"]["status"], "stopped")
        self.assert_owned_stop()
        self.assertEqual(self.queue_status()["status"], "idle")
        self.audit()


class RefusalTests(unittest.TestCase):
    def setUp(self):
        self.root = EVIDENCE / self._testMethodName
        self.root.mkdir()
        self.lock = self.root / "lock"
        self.state_root = self.root / "fake-state"
        self.state_root.mkdir()
        write_new(self.state_root / "state.json", {"synthetic": True, "daemon_id": DAEMON,
                                                   "containers": {}, "stop_fails": False})
        self.bin = self.root / "bin"
        self.bin.mkdir()
        for name in ("docker", "podman", "nerdctl", "ctr"):
            (self.bin / name).symlink_to(FAKE)
        self.commands = []
        self.env = {**os.environ, "FAKE_DOCKER_STATE": str(self.state_root),
                    "PYTHONDONTWRITEBYTECODE": "1"}

    def tearDown(self):
        write_new(self.root / "commands.json", self.commands)

    def assert_refused(self, name, command, docker=False):
        out = self.root / name
        args = [sys.executable, str(RUNNER), "run", "--owner", "W4", "--kind", "validation",
                "--out", str(out), "--timeout-seconds", "1", "--wait-seconds", "0",
                "--lock-dir", str(self.lock)]
        if docker:
            args.append("--docker")
        args.extend(["--", *command])
        result = subprocess.run(args, capture_output=True, text=True, env=self.env, timeout=10)
        self.commands.append({"argv": args, "returncode": result.returncode,
                              "stdout": result.stdout, "stderr": result.stderr})
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("daemon jobs are prerequisite_unavailable: validated ownership/stop adapter required",
                      result.stderr)
        self.assertFalse(out.exists(), "refusal must precede evidence-directory creation")
        self.assertFalse(self.lock.exists(), "refusal must precede queue acquisition")
        self.assertFalse((self.state_root / "calls.jsonl").exists(), "fake CLI must never execute")

    def test_public_docker_flag_refused_before_command(self):
        marker = self.root / "must-not-run"
        self.assert_refused("flag", [sys.executable, "-c",
                            "from pathlib import Path; Path(" + repr(str(marker)) + ").touch()"],
                            docker=True)
        self.assertFalse(marker.exists())

    def test_direct_docker_variants_refused(self):
        variants = (("run", "synthetic-image", "stopped"), ("create", "synthetic-image"),
                    ("start", "unowned"), ("container", "inspect", "unowned"),
                    ("--context", "synthetic", "run", "synthetic-image"))
        for index, args in enumerate(variants):
            with self.subTest(args=args):
                self.assert_refused("direct-" + str(index), [str(self.bin / "docker"), *args])

    def test_docker_flag_with_direct_run_refused(self):
        self.assert_refused("flag-direct", [str(self.bin / "docker"), "run", "synthetic-image", "stopped"],
                            docker=True)

    def test_other_daemon_cli_basenames_refused(self):
        for name in ("podman", "nerdctl", "ctr"):
            with self.subTest(binary=name):
                self.assert_refused(name, [str(self.bin / name), "run", "synthetic-image", "stopped"])


def main():
    global EVIDENCE
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence-root", required=True, type=Path)
    args = parser.parse_args()
    EVIDENCE = args.evidence_root.resolve()
    EVIDENCE.mkdir(parents=True, exist_ok=False)
    initial = {"runner": sha(RUNNER), "fake_docker": sha(FAKE), "test_harness": sha(SOURCE)}
    sources = EVIDENCE / "sources"
    sources.mkdir()
    for path in (RUNNER, FAKE, SOURCE):
        with open(sources / path.name, "xb") as target:
            target.write(path.read_bytes())
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(RefusalTests)
    cases = [test.id() for test in suite]
    with open(EVIDENCE / "tests.log", "x", encoding="utf-8") as log:
        result = unittest.TextTestRunner(stream=log, verbosity=2).run(suite)
    print((EVIDENCE / "tests.log").read_text(), end="")
    final = {"runner": sha(RUNNER), "fake_docker": sha(FAKE), "test_harness": sha(SOURCE)}
    report = {"schema_version": 1, "evidence_kind": "synthetic_daemon_job_refusal_tests",
              "synthetic": True, "real_docker_used": False,
              "fake_docker_executed": False,
              "scope": "public refusal before command execution, evidence output, or queue acquisition",
              "adapter_cleanup_validated": False,
              "unexecuted_future_adapter_cases": ["FutureAdapterControls." + name
                    for name in dir(FutureAdapterControls) if name.startswith("test_")],
              "source_sha256_before": initial, "source_sha256_after": final,
              "source_unchanged": initial == final, "cases": cases, "tests_run": result.testsRun,
              "failures": [(test.id(), trace) for test, trace in result.failures],
              "errors": [(test.id(), trace) for test, trace in result.errors],
              "passed": result.wasSuccessful() and initial == final}
    write_new(EVIDENCE / "test-results.json", report)
    files = []
    for path in sorted(EVIDENCE.rglob("*"), key=lambda p: p.relative_to(EVIDENCE).as_posix()):
        if path.is_symlink():
            files.append({"path": path.relative_to(EVIDENCE).as_posix(), "type": "symlink",
                          "target": str(path.readlink())})
        elif path.is_file():
            files.append({"path": path.relative_to(EVIDENCE).as_posix(), "type": "file",
                          "bytes": path.stat().st_size, "sha256": sha(path)})
    write_new(EVIDENCE / "manifest.json", {"schema_version": 1, "synthetic": True, "files": files})
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
