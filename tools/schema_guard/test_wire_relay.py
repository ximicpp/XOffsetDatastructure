#!/usr/bin/env python3
"""Regression tests for the cross-node wire relay; no mock is platform evidence."""
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import threading
import time
from types import SimpleNamespace
import unittest
from unittest import mock

import nodecli_wire_worker as worker
import relay_wire_runner as relay

class RelayTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="xsg-relay-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.queue = self.root / "queue"
        self.queue.mkdir(mode=0o700)
        self.binary = self.root / "fixture"
        self.binary.write_bytes(b"test fixture")
        self.adapter = self.root / "adapter.py"
        self.adapter.write_text("unused")
        self.fingerprint = relay.sha(self.binary)

    def execute(self, request, runner):
        work = self.root / "worker"
        work.mkdir(exist_ok=True)
        with mock.patch.object(worker.subprocess, "run", side_effect=runner):
            return worker.execute(request, {self.fingerprint: (self.binary, "android")},
                                  self.adapter, "adb", "serial", work)

    def test_worker_rejects_unknown_binary_and_traversal(self):
        request = {"version": 1, "id": "a" * 32, "binary_sha256": "0" * 64,
                   "operation": "--xsg-identify"}
        response = self.execute(request, lambda *a, **k: self.fail("must not run"))
        self.assertEqual(response["exit_code"], 1)
        request.update(binary_sha256=self.fingerprint, operation="--read",
                       files={"../escape.xwire": "YQ=="})
        response = self.execute(request, lambda *a, **k: self.fail("must not run"))
        self.assertEqual(response["exit_code"], 1)
        self.assertFalse((self.root / "escape.xwire").exists())

    def test_live_request_response_and_output(self):
        def process():
            until = time.monotonic() + 5
            while time.monotonic() < until:
                pending = list(self.queue.glob("*.request.json"))
                if pending:
                    request = json.loads(pending[0].read_text())
                    def fake_command(cmd, **kwargs):
                        Path(cmd[-1]).mkdir()
                        (Path(cmd[-1]) / "case.xwire").write_bytes(b"wire")
                        return SimpleNamespace(returncode=0, stdout="ok\\n", stderr="")
                    response = self.execute(request, fake_command)
                    (self.queue / (request["id"] + ".response.json")).write_text(json.dumps(response))
                    return
                time.sleep(0.01)
        thread = threading.Thread(target=process)
        thread.start()
        destination = self.root / "output"
        args = ["relay_wire_runner.py", "--queue", str(self.queue), "--timeout", "5",
                "--", str(self.binary), "--write", str(destination)]
        with mock.patch.object(sys, "argv", args), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(relay.main(), 0)
        thread.join()
        self.assertEqual((destination / "case.xwire").read_bytes(), b"wire")
        self.assertEqual(list(self.queue.iterdir()), [])

    def test_partially_transferred_response_is_read_again(self):
        def process():
            until = time.monotonic() + 5
            while time.monotonic() < until:
                pending = list(self.queue.glob("*.request.json"))
                if pending:
                    request = json.loads(pending[0].read_text())
                    response = self.execute(request, lambda *a, **k: SimpleNamespace(
                        returncode=0, stdout="identity\n", stderr=""))
                    raw = json.dumps(response)
                    target = self.queue / (request["id"] + ".response.json")
                    target.write_text(raw[:len(raw) // 2])
                    time.sleep(0.5)
                    target.write_text(raw)
                    return
                time.sleep(0.01)
        thread = threading.Thread(target=process)
        thread.start()
        args = ["relay_wire_runner.py", "--queue", str(self.queue), "--timeout", "5",
                "--", str(self.binary), "--xsg-identify"]
        output = io.StringIO()
        with mock.patch.object(sys, "argv", args), contextlib.redirect_stdout(output):
            self.assertEqual(relay.main(), 0)
        thread.join()
        self.assertEqual(output.getvalue(), "identity\n")
        self.assertEqual(list(self.queue.iterdir()), [])

    def test_absent_worker_fails_closed(self):
        args = ["relay_wire_runner.py", "--queue", str(self.queue), "--timeout", "0",
                "--", str(self.binary), "--xsg-identify"]
        with mock.patch.object(sys, "argv", args), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(relay.main(), 1)
        self.assertEqual(list(self.queue.iterdir()), [])

    def test_response_paths_cannot_escape(self):
        with self.assertRaises(ValueError):
            relay.unpack({"../bad.xwire": "YQ=="}, self.root / "out")
        self.assertFalse((self.root / "bad.xwire").exists())

if __name__ == "__main__":
    unittest.main()
