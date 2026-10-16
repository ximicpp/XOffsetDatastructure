#!/usr/bin/env python3
"""Check pairing, source attribution and tamper rejection without timing code."""
import contextlib
import gzip
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import tarfile
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location("stage1", Path(__file__).with_name("benchmark_vm_stage1.py"))
BENCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BENCH)


def fixture(out):
    cases = [dict(id="test", workload="save", size=4, cycles=1, metrics=["elapsed_ms"])]
    manifests = {v: {"test.hpp": hashlib.sha256(v.encode()).hexdigest()} for v in ("D", "F")}
    metadata = dict(schema_version=BENCH.EVIDENCE_SCHEMA_VERSION,
                    cases=cases, contrasts=BENCH.CONTRASTS, runs=3, source_manifests=manifests,
                    binary_sha256={"D": "binary-d", "F": "binary-f"},
                    variant_labels={"D": "D_label", "F": "F_label"}, runner_sha256=BENCH.sha(BENCH.__file__))
    with tarfile.open(out / "source_snapshots.tar.gz", "w:gz") as archive:
        for variant in ("D", "F"):
            data = variant.encode()
            info = tarfile.TarInfo(f"snapshots/{variant}/test.hpp")
            info.size = len(data)
            archive.addfile(info, io.BytesIO(data))
        archive.add(BENCH.__file__, arcname="snapshots/benchmark_vm_stage1.py")
    metadata["archive_sha256"] = BENCH.sha(out / "source_snapshots.tar.gz")
    rows = []
    common = dict(semantic_checksum=123, request_count=1, size=4, requested_cycles=1,
                  used_end=64, wire_size=152, final_logical_bytes=64, final_accessible_bytes=16384,
                  initial_logical_bytes=1024, initial_reserved_bytes=1024, container_moves=0,
                  container_copy_bytes_est=0, allocation_failures=0, growth_count=0)
    for contrast in BENCH.CONTRASTS:
        for pair, (before, after) in enumerate(((1, 1), (100, 2), (10, 20))):
            for position, (endpoint, elapsed) in enumerate((("before", before), ("after", after))):
                binary = BENCH.CONTRASTS[contrast][endpoint]
                rows.append(dict(common, window=1, case_id="test", contrast=contrast, pair=pair,
                                 endpoint=endpoint, elapsed_ms=elapsed, phase="measured",
                                 execution_position=position, variant=metadata["variant_labels"][binary],
                                 binary_sha256=metadata["binary_sha256"][binary]))
    target = out / "window_01"
    target.mkdir()
    raw = "".join(json.dumps(row) + "\n" for row in rows).encode()
    (target / "raw.jsonl.gz").write_bytes(gzip.compress(raw))
    BENCH.write_json(target / "metadata.json", dict(window=1, status="completed", runtime_identity_validated=True,
                     measured_rows=12, warmup_rows=0,
                     raw_sha256=hashlib.sha256(raw).hexdigest()))
    BENCH.write_json(out / "metadata.json", metadata)
    BENCH.write_json(out / "summary.json", BENCH.summarize(metadata, rows))
    return metadata, rows


class MeasurementEvidenceTests(unittest.TestCase):
    def test_pair_ratios_do_not_use_ratio_of_medians(self):
        with tempfile.TemporaryDirectory() as scratch:
            metadata, rows = fixture(Path(scratch))
            summary = BENCH.summarize(metadata, rows)
            contrast = summary["results"][0]["windows"][0]["contrasts"]["DF"]
            self.assertEqual(contrast["paired_before_over_after"]["median"], 1)
            self.assertEqual(contrast["before_ms"]["median"] / contrast["after_ms"]["median"], 5)

    def test_incomplete_window_is_rejected(self):
        with tempfile.TemporaryDirectory() as scratch:
            metadata, rows = fixture(Path(scratch))
            with self.assertRaisesRegex(RuntimeError, "incomplete"):
                BENCH.summarize(metadata, rows[:-1])

    def test_audit_and_raw_tamper(self):
        with tempfile.TemporaryDirectory() as scratch:
            out = Path(scratch)
            fixture(out)
            with contextlib.redirect_stdout(io.StringIO()):
                BENCH.audit(out)
            path = out / "window_01/raw.jsonl.gz"
            path.write_bytes(gzip.compress(gzip.decompress(path.read_bytes()) + b"\n"))
            # Invalid raw JSON is also a hard failure, never a skipped sample.
            with self.assertRaises((RuntimeError, json.JSONDecodeError)):
                BENCH.audit(out)

    def test_same_semantics_wrong_binary_is_rejected(self):
        with tempfile.TemporaryDirectory() as scratch:
            out = Path(scratch)
            _, rows = fixture(out)
            rows[0]["binary_sha256"] = "unexpected-binary"
            (out / "window_01/raw.jsonl.gz").write_bytes(gzip.compress(
                "".join(json.dumps(r) + "\n" for r in rows).encode()))
            with self.assertRaisesRegex(RuntimeError, "attribution"):
                BENCH.audit(out)

    def test_archive_tamper_is_rejected(self):
        with tempfile.TemporaryDirectory() as scratch:
            out = Path(scratch)
            fixture(out)
            with (out / "source_snapshots.tar.gz").open("ab") as stream:
                stream.write(b"tamper")
            with self.assertRaisesRegex(RuntimeError, "archive hash"):
                BENCH.audit(out)

    def test_final_identity_failure_cannot_be_resumed_or_audited(self):
        with tempfile.TemporaryDirectory() as scratch:
            out = Path(scratch)
            metadata, rows = fixture(out)
            shutil.rmtree(out / "window_01")
            (out / "summary.json").unlink()
            metadata.update(warmups=0, minimum_window_gap_seconds=0)
            BENCH.write_json(out / "metadata.json", metadata)
            reply = dict(rows[0], workload="save", address_changes=0,
                         memory_live={"available": True})
            args = SimpleNamespace(output=out, quiet_note="mocked test; no real timing")
            with mock.patch.object(BENCH, "command", return_value=SimpleNamespace(stdout=json.dumps(reply))), \
                    mock.patch.object(BENCH, "host_state", return_value={}), \
                    mock.patch.object(BENCH, "verify_runtime", side_effect=[None, RuntimeError("runtime changed")]) as verify, \
                    contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(RuntimeError, "runtime changed"):
                    BENCH.run_window(args)
                self.assertEqual(verify.call_count, 2)
            state = json.loads((out / "window_01/metadata.json").read_text())
            self.assertEqual(state["status"], "failed")
            self.assertFalse(state["runtime_identity_validated"])
            self.assertNotIn("finished_epoch", state)
            self.assertTrue((out / "window_01/raw.jsonl").is_file())
            self.assertFalse((out / "window_01/raw.jsonl.gz").exists())
            self.assertFalse((out / "summary.json").exists())
            # Restoring the runtime must not rehabilitate the failed window.
            with mock.patch.object(BENCH, "verify_runtime", return_value=None):
                for operation in (lambda: BENCH.read_rows(out), lambda: BENCH.run_window(args), lambda: BENCH.audit(out)):
                    with self.assertRaisesRegex(RuntimeError, "failed or incomplete window"):
                        operation()
            self.assertFalse((out / "window_02").exists())

    def test_unfinished_window_with_gzip_is_rejected(self):
        with tempfile.TemporaryDirectory() as scratch:
            out = Path(scratch)
            fixture(out)
            path = out / "window_01/metadata.json"
            state = json.loads(path.read_text())
            state.update(status="running", runtime_identity_validated=False)
            BENCH.write_json(path, state)
            with self.assertRaisesRegex(RuntimeError, "failed or incomplete window"):
                BENCH.read_rows(out)

    def test_success_publishes_completion_only_after_final_check(self):
        with tempfile.TemporaryDirectory() as scratch:
            out = Path(scratch)
            metadata, rows = fixture(out)
            shutil.rmtree(out / "window_01")
            (out / "summary.json").unlink()
            metadata.update(warmups=0, minimum_window_gap_seconds=0)
            BENCH.write_json(out / "metadata.json", metadata)
            reply = dict(rows[0], workload="save", address_changes=0,
                         memory_live={"available": True})
            checks = []

            def verify(*_):
                checks.append(True)
                if len(checks) == 2:
                    state = json.loads((out / "window_01/metadata.json").read_text())
                    self.assertEqual(state["status"], "running")
                    self.assertFalse(state["runtime_identity_validated"])
                    self.assertFalse((out / "window_01/raw.jsonl.gz").exists())

            with mock.patch.object(BENCH, "command", return_value=SimpleNamespace(stdout=json.dumps(reply))), \
                    mock.patch.object(BENCH, "host_state", return_value={}), \
                    mock.patch.object(BENCH, "verify_runtime", side_effect=verify), \
                    contextlib.redirect_stdout(io.StringIO()):
                BENCH.run_window(SimpleNamespace(output=out, quiet_note="mocked test"))
                BENCH.audit(out)
            self.assertEqual(len(checks), 2)
            self.assertEqual(len(BENCH.read_rows(out)), 12)
            state = json.loads((out / "window_01/metadata.json").read_text())
            self.assertEqual(state["status"], "completed")
            self.assertTrue(state["runtime_identity_validated"])

    def test_legacy_evidence_requires_frozen_auditor(self):
        with tempfile.TemporaryDirectory() as scratch:
            out = Path(scratch)
            metadata, _ = fixture(out)
            metadata["schema_version"] = 1
            BENCH.write_json(out / "metadata.json", metadata)
            for operation in (lambda: BENCH.read_rows(out), lambda: BENCH.audit(out)):
                with self.assertRaisesRegex(RuntimeError, "frozen runner"):
                    operation()


if __name__ == "__main__":
    unittest.main()
