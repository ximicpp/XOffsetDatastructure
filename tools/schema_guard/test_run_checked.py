#!/usr/bin/env python3
"""Check the full-matrix command wrapper's fail-closed behavior."""
import importlib.util
from pathlib import Path
from types import SimpleNamespace
from unittest import TestCase, main, mock
import tempfile

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("run_checked", HERE / "run_checked.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class PreRunGateTests(TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="xsg-run-test-")
        self.addCleanup(self.temp.cleanup)
        self.args = ["--reports", self.temp.name, "--clang", "/fake/compiler",
                     "--profiles", "/fake/profiles", "--ndk", "/fake/ndk",
                     "--", "example-command", "arg with spaces"]

    def test_failure_does_not_start_command(self):
        with mock.patch.object(module.subprocess, "run", return_value=SimpleNamespace(returncode=2)) as run:
            self.assertEqual(module.main(self.args), 2)
        self.assertEqual(run.call_count, 1)
        cmd = run.call_args.args[0]
        self.assertNotIn("--target", cmd)
        self.assertIn("--profiles", cmd)

    def test_success_starts_original_argv_and_propagates_exit(self):
        with mock.patch.object(module.subprocess, "run", side_effect=[
            SimpleNamespace(returncode=0), SimpleNamespace(returncode=0),
            SimpleNamespace(returncode=7)]) as run:
            self.assertEqual(module.main(self.args), 7)
        self.assertEqual(run.call_count, 3)
        self.assertEqual(run.call_args.args[0], ["example-command", "arg with spaces"])

    def test_runtime_failure_blocks_command(self):
        with mock.patch.object(module.subprocess, "run", side_effect=[
            SimpleNamespace(returncode=0), SimpleNamespace(returncode=2)]) as run:
            self.assertEqual(module.main(self.args), 2)
        self.assertEqual(run.call_count, 2)
        self.assertIn("runtime_matrix.py", run.call_args.args[0][1])

    def test_custom_catalog_without_wire_fixture_is_blocked(self):
        with mock.patch.object(module.subprocess, "run", return_value=SimpleNamespace(returncode=0)) as run:
            args = ["--catalog", "/different/catalog.hpp"] + self.args
            self.assertEqual(module.main(args), 2)
        self.assertEqual(run.call_count, 1)

    def test_each_run_gets_new_report_directory(self):
        with mock.patch.object(module.subprocess, "run", return_value=SimpleNamespace(returncode=2)) as run:
            module.main(self.args)
            first = run.call_args.args[0]
            module.main(self.args)
            second = run.call_args.args[0]
        self.assertNotEqual(first[first.index("--out") + 1], second[second.index("--out") + 1])

if __name__ == "__main__":
    main(verbosity=2)
