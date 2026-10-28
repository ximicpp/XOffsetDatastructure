#!/usr/bin/env python3
"""Wine adapter tests; these mocks are not platform execution evidence."""
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
import wine_wire_runner

class WineRunnerTests(unittest.TestCase):
    def test_mounts_corpus_and_preserves_failure(self):
        with tempfile.TemporaryDirectory(prefix="xsg-wine-test-") as tmp:
            root = Path(tmp).resolve()
            binary = root / "fixture.exe"
            binary.write_bytes(b"fixture")
            directory = root / "wire"
            args = ["wine_wire_runner.py", "--image", "sha256:" + "a" * 64,
                    "--", str(binary), "--write", str(directory)]
            with mock.patch.object(sys, "argv", args), mock.patch.object(
                    wine_wire_runner.subprocess, "run", return_value=SimpleNamespace(returncode=17)) as run:
                self.assertEqual(wine_wire_runner.main(), 17)
            command = run.call_args.args[0]
            self.assertIn("--network=none", command)
            self.assertIn(str(root) + ":/xsg/data", command)
            self.assertEqual(command[-1], "Z:\\xsg\\data\\wire")

if __name__ == "__main__":
    unittest.main()
