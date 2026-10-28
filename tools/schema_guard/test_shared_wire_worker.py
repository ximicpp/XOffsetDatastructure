#!/usr/bin/env python3
"""Shared Windows VM queue tests; mocked calls are not platform evidence."""
import json
from pathlib import Path
import tempfile
import sys
import unittest
from unittest import mock

import shared_wire_worker as shared

class SharedWorkerTests(unittest.TestCase):
    def test_identity_response_is_written_atomically(self):
        with tempfile.TemporaryDirectory(prefix="xsg-shared-test-") as tmp:
            root = Path(tmp)
            ident = "a" * 32
            name = ident + ".request.json"
            request = {"version": 1, "id": ident, "binary_sha256": "1" * 64,
                       "operation": "--xsg-identify"}
            (root / name).write_text(json.dumps(request))
            expected = {"version": 1, "id": ident, "binary_sha256": "1" * 64,
                        "exit_code": 0, "stdout": "identity", "stderr": "", "files": {}}
            with mock.patch.object(shared.worker, "execute", return_value=expected):
                self.assertEqual(shared.process(root, name, {}), 0)
            self.assertEqual(json.loads((root / (ident + ".response.json")).read_text()), expected)
            self.assertEqual(list(root.glob("*.pending")), [])

    def test_mismatched_request_id_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="xsg-shared-test-") as tmp:
            root = Path(tmp)
            name = "a" * 32 + ".request.json"
            (root / name).write_text(json.dumps({"id": "b" * 32}))
            with mock.patch.object(shared.worker, "execute") as execute:
                with self.assertRaises(ValueError):
                    shared.process(root, name, {})
                execute.assert_not_called()
            self.assertEqual(list(root.glob("*.response.json")), [])

    def test_once_returns_failure_for_failed_request(self):
        with tempfile.TemporaryDirectory(prefix="xsg-shared-test-") as tmp:
            root = Path(tmp)
            fixture = root / "fixture.exe"
            fixture.write_bytes(b"fixture")
            (root / ("a" * 32 + ".request.json")).write_text("{}")
            args = ["shared_wire_worker.py", "--queue", str(root), "--native-fixture", str(fixture), "--once"]
            with mock.patch.object(sys, "argv", args), mock.patch.object(shared, "process", return_value=1):
                self.assertEqual(shared.main(), 1)

if __name__ == "__main__":
    unittest.main()
