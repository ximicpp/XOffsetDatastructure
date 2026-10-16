#!/usr/bin/env python3
"""Lightweight real-process checks; every case uses an isolated queue.

python3 test_run_job.py --evidence-root NEW_DIRECTORY
No C++ builds, stress tests, benchmarks or formal queue lock are used.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time
import unittest


RUNNER = Path(__file__).with_name("run_job.py").resolve()
EVIDENCE = None


class CoordinationTests(unittest.TestCase):
    def setUp(self):
        self.root = EVIDENCE / self._testMethodName
        self.root.mkdir()
        self.lock = self.root / "lock"
        self.clients = []
        self.commands = []

    def tearDown(self):
        # Cancel only clients created by this case; their supervisors reap work.
        for process in self.clients:
            if process.poll() is None:
                process.send_signal(signal.SIGTERM)
                process.wait(timeout=15)
        (self.root / "commands.json").write_text(json.dumps(self.commands, indent=2) + "\n")

    def args(self, out, command, timeout=3, wait=0):
        return [sys.executable, str(RUNNER), "run", "--owner", "W4", "--kind", "validation",
                "--out", str(self.root / out), "--timeout-seconds", str(timeout),
                "--wait-seconds", str(wait), "--termination-grace-seconds", "0.15",
                "--lock-dir", str(self.lock), "--", *command]

    def execute(self, args):
        value = subprocess.run(args, capture_output=True, text=True, timeout=20)
        self.commands.append({"argv": args, "returncode": value.returncode,
                              "stdout": value.stdout, "stderr": value.stderr})
        return value

    def run_python(self, code, out="job", **kwargs):
        return self.execute(self.args(out, [sys.executable, "-c", code], **kwargs))

    def start_python(self, code, out="job", **kwargs):
        args = self.args(out, [sys.executable, "-c", code], **kwargs)
        # Files avoid undrained pipes during cancellation tests.
        stdout = open(self.root / (out + ".client.stdout"), "wb")
        stderr = open(self.root / (out + ".client.stderr"), "wb")
        try:
            value = subprocess.Popen(args, stdout=stdout, stderr=stderr)
        finally:
            stdout.close()
            stderr.close()
        self.clients.append(value)
        self.commands.append({"argv": args, "client_pid": value.pid})
        return value

    def await_file(self, path):
        end = time.monotonic() + 5
        while time.monotonic() < end:
            if path.exists():
                return
            time.sleep(0.02)
        self.fail("file not produced: " + str(path))

    def result(self, out="job"):
        self.await_file(self.root / out / "result.json")
        return json.loads((self.root / out / "result.json").read_text())

    def status(self):
        return json.loads(self.execute([sys.executable, str(RUNNER), "status",
                                        "--lock-dir", str(self.lock)]).stdout)

    def assert_no_live_pid(self, pid):
        state = subprocess.run(["ps", "-p", str(pid), "-o", "stat="], capture_output=True, text=True)
        self.assertTrue(state.returncode != 0 or state.stdout.strip().startswith("Z"), state.stdout)

    def test_success_and_offline_audit(self):
        ran = self.run_python("print('evidence-success')")
        self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
        result = self.result()
        self.assertEqual(result["status"], "complete")
        self.assertEqual((self.root / "job/stdout.log").read_text(), "evidence-success\n")
        audit = self.execute([sys.executable, str(RUNNER), "audit", "--out", str(self.root / "job")])
        self.assertEqual(audit.returncode, 0, audit.stdout)
        self.assertEqual(json.loads(audit.stdout)["job_status"], "complete")

    def test_nonzero_is_failed(self):
        ran = self.run_python("import sys; print('deliberate failure', file=sys.stderr); sys.exit(7)")
        self.assertEqual(ran.returncode, 1)
        self.assertEqual(self.result()["status"], "failed")
        self.assertEqual(self.result()["command_returncode"], 7)

    def test_spawn_error_is_error(self):
        ran = self.execute(self.args("job", [str(self.root / "does-not-exist")]))
        self.assertEqual(ran.returncode, 70)
        self.assertEqual(self.result()["status"], "error")
        self.assertEqual(self.status()["status"], "idle")

    def test_timeout_reaps_term_ignoring_child(self):
        child_pid = self.root / "child.pid"
        child = "import signal,time; signal.signal(signal.SIGTERM,signal.SIG_IGN); time.sleep(60)"
        parent = ("import subprocess,sys,time,pathlib; p=subprocess.Popen([sys.executable,'-c'," +
                  repr(child) + "]); pathlib.Path(" + repr(str(child_pid)) +
                  ").write_text(str(p.pid)); time.sleep(60)")
        ran = self.run_python(parent, timeout=0.5)
        self.assertEqual(ran.returncode, 124, ran.stdout)
        self.assertEqual(self.result()["status"], "timeout")
        self.assert_no_live_pid(int(child_pid.read_text()))
        self.assertFalse(self.result()["cleanup"]["live"])
        self.assertTrue(any(e["event"] == "group_sigkill" for e in json.loads((self.root / "job/events.json").read_text())))

    def test_orphan_child_is_failed_and_reaped(self):
        child_pid = self.root / "child.pid"
        code = ("import subprocess,sys,pathlib; p=subprocess.Popen([sys.executable,'-c','import time; time.sleep(60)']); "
                "pathlib.Path(" + repr(str(child_pid)) + ").write_text(str(p.pid))")
        ran = self.run_python(code)
        self.assertEqual(ran.returncode, 1, ran.stdout)
        self.assertEqual(self.result()["reason"], "leader_left_running_descendants")
        self.assert_no_live_pid(int(child_pid.read_text()))

    def test_child_new_process_group_is_reaped(self):
        child_pid = self.root / "child.pid"
        child = "import os,time; os.setpgrp(); time.sleep(60)"
        code = ("import subprocess,sys,pathlib,time; p=subprocess.Popen([sys.executable,'-c'," + repr(child) +
                "]); pathlib.Path(" + repr(str(child_pid)) + ").write_text(str(p.pid)); time.sleep(0.2)")
        ran = self.run_python(code)
        self.assertEqual(ran.returncode, 1, ran.stdout)
        self.assertEqual(self.result()["reason"], "leader_left_running_descendants")
        self.assert_no_live_pid(int(child_pid.read_text()))

    def test_timeout_child_new_process_group_is_reaped(self):
        child_pid = self.root / "child.pid"
        child = "import os,signal,time; os.setpgrp(); signal.signal(signal.SIGTERM,signal.SIG_IGN); time.sleep(60)"
        code = ("import subprocess,sys,pathlib,time; p=subprocess.Popen([sys.executable,'-c'," + repr(child) +
                "]); pathlib.Path(" + repr(str(child_pid)) + ").write_text(str(p.pid)); time.sleep(60)")
        ran = self.run_python(code, timeout=0.5)
        self.assertEqual(ran.returncode, 124, ran.stdout)
        self.assert_no_live_pid(int(child_pid.read_text()))

    def test_command_stdin_isolated_from_cancellation(self):
        ready = self.root / "ready"
        code = ("import sys,pathlib,time; data=sys.stdin.buffer.read(); assert data==b''; "
                "pathlib.Path(" + repr(str(ready)) + ").touch(); time.sleep(60)")
        client = self.start_python(code)
        self.await_file(ready)
        client.terminate()
        self.assertEqual(client.wait(timeout=10), 130)
        self.assertEqual(self.result()["status"], "cancelled")

    def test_mutex_busy_and_stable_inode(self):
        ready = self.root / "ready"
        client = self.start_python("import pathlib,time; pathlib.Path(" + repr(str(ready)) + ").touch(); time.sleep(0.7)")
        self.await_file(ready)
        inode = (self.lock / "queue.lock").stat().st_ino
        state = self.status()
        self.assertEqual(state["status"], "busy")
        self.assertEqual(state["metadata"]["owner"], "W4")
        ran = self.run_python("raise RuntimeError('must never execute')", out="busy")
        self.assertEqual(ran.returncode, 75, ran.stdout)
        self.assertEqual(self.result("busy")["status"], "busy")
        self.assertFalse((self.root / "busy/stdout.log").exists())
        client.wait(timeout=10)
        self.assertEqual(self.status()["status"], "idle")
        self.assertEqual((self.lock / "queue.lock").stat().st_ino, inode)

    def test_bounded_wait_runs_after_first(self):
        ready = self.root / "ready"
        first = self.start_python("import pathlib,time; pathlib.Path(" + repr(str(ready)) + ").touch(); time.sleep(0.5)")
        self.await_file(ready)
        second = self.run_python("print('second')", out="second", wait=2)
        self.assertEqual(second.returncode, 0, second.stdout)
        self.assertEqual(first.wait(timeout=5), 0)
        self.assertGreaterEqual(self.result("second")["started_at"], self.result()["finished_at"])

    def test_existing_output_unchanged(self):
        out = self.root / "job"
        out.mkdir()
        sentinel = out / "sentinel"
        sentinel.write_bytes(b"do not replace this evidence\x00\xff")
        before = hashlib.sha256(sentinel.read_bytes()).hexdigest()
        ran = self.run_python("raise RuntimeError('must never execute')")
        self.assertEqual(ran.returncode, 70)
        self.assertEqual(sorted(p.name for p in out.iterdir()), ["sentinel"])
        self.assertEqual(hashlib.sha256(sentinel.read_bytes()).hexdigest(), before)

    def test_client_sigint_and_sigterm(self):
        for sig in (signal.SIGINT, signal.SIGTERM):
            name = "signal-" + str(sig)
            ready = self.root / (name + ".pid")
            client = self.start_python("import os,pathlib,time; pathlib.Path(" + repr(str(ready)) +
                                       ").write_text(str(os.getpid())); time.sleep(60)", out=name)
            self.await_file(ready)
            client.send_signal(sig)
            self.assertEqual(client.wait(timeout=10), 130)
            self.assertEqual(self.result(name)["status"], "cancelled")
            self.assert_no_live_pid(int(ready.read_text()))

    def test_killed_client_supervisor_cleans(self):
        ready = self.root / "ready.pid"
        client = self.start_python("import os,pathlib,time; pathlib.Path(" + repr(str(ready)) +
                                   ").write_text(str(os.getpid())); time.sleep(60)")
        self.await_file(ready)
        client.kill()
        client.wait(timeout=5)
        result = self.result()
        self.assertEqual(result["status"], "cancelled")
        self.assertEqual(result["reason"], "client_disconnected")
        self.assert_no_live_pid(int(ready.read_text()))
        # result is written while still locked, so wait for actual unlock too.
        end = time.monotonic() + 5
        while self.status()["status"] != "idle" and time.monotonic() < end:
            time.sleep(0.02)
        self.assertEqual(self.status()["status"], "idle")

    def test_unrelated_process_survives(self):
        unrelated = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"], start_new_session=True)
        try:
            ran = self.run_python("import time; time.sleep(60)", timeout=0.15)
            self.assertEqual(ran.returncode, 124)
            self.assertIsNone(unrelated.poll())
        finally:
            unrelated.terminate()
            unrelated.wait(timeout=5)

    def test_tamper_detected_without_execution(self):
        self.assertEqual(self.run_python("print('once')").returncode, 0)
        copied = self.root / "copied-job"
        shutil.copytree(self.root / "job", copied)
        (copied / "stdout.log").write_text("tampered\n")
        ran = self.execute([sys.executable, str(RUNNER), "audit", "--out", str(copied)])
        self.assertEqual(ran.returncode, 70)
        self.assertIn("manifest mismatch", ran.stdout)

    def test_recursive_artifact_manifest(self):
        nested = self.root / "job/results/nested.json"
        code = ("import pathlib; p=pathlib.Path(" + repr(str(nested)) +
                "); p.parent.mkdir(); p.write_text('nested evidence')")
        self.assertEqual(self.run_python(code).returncode, 0)
        manifest = json.loads((self.root / "job/manifest.json").read_text())
        self.assertIn("results/nested.json", [entry["path"] for entry in manifest["files"]])
        self.assertEqual(self.execute([sys.executable, str(RUNNER), "audit", "--out", str(self.root / "job")]).returncode, 0)
        copied = self.root / "copied-job"
        shutil.copytree(self.root / "job", copied)
        (copied / "results/nested.json").write_text("changed")
        self.assertEqual(self.execute([sys.executable, str(RUNNER), "audit", "--out", str(copied)]).returncode, 70)

    def test_guard_rejects_unguarded_docker_and_nonfinite_deadline(self):
        for command, timeout in [(["docker", "run", "no-image"], "3"),
                                 (["docker", "container", "run", "no-image"], "3"),
                                 ([sys.executable, "-c", "pass"], "nan"),
                                 ([sys.executable, "-c", "pass"], "inf")]:
            ran = self.execute(self.args("job", command, timeout=timeout))
            self.assertEqual(ran.returncode, 2)
            self.assertFalse((self.root / "job").exists())


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence-root", required=True, type=Path)
    args = parser.parse_args()
    EVIDENCE = args.evidence_root.resolve()
    EVIDENCE.mkdir(parents=True, exist_ok=False)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(CoordinationTests)
    names = [test.id() for test in suite]
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    report = {"runner_sha256": hashlib.sha256(RUNNER.read_bytes()).hexdigest(),
              "test_source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "cases": names, "tests_run": result.testsRun,
              "failures": [(test.id(), trace) for test, trace in result.failures],
              "errors": [(test.id(), trace) for test, trace in result.errors],
              "passed": result.wasSuccessful()}
    (EVIDENCE / "test-results.json").write_text(json.dumps(report, indent=2) + "\n")
    sys.exit(0 if result.wasSuccessful() else 1)
