"""Synthetic offline negative controls; no C++ build or native-run evidence.

Collection-envelope tests replace only check_case with a deterministic stub so
their fixtures exercise manifest, receipt, profile, and parity checks separately
from case semantics. The check_case tests below use the real event auditor and
a small structurally valid, synthetic owner_basic trace.
"""

import copy
import contextlib
import builtins
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import observe
import generate as generate_module


REJECTION = (RuntimeError, ValueError, OSError)


def _trace(events=()):
    events = copy.deepcopy(list(events))
    for sequence, event in enumerate(events, 1):
        event["sequence"] = sequence
    return {
        "schema": "n3-events-v1",
        "diagnostic": True,
        "timing_evidence": False,
        "valid": True,
        "dropped": 0,
        "thread_count": 1,
        "events": events,
    }


def _event(kind, *, owner=100, subject=1000, size=0, value=0):
    return {
        "sequence": 0,
        "thread": 0,
        "phase": "owner_basic",
        "kind": kind,
        "owner": owner,
        "subject": subject,
        "bytes": size,
        "value": value,
    }


def _basic_report():
    return {
        "schema": "n3-probe-v1",
        "case": "owner_basic",
        "status": "passed",
        "semantic": {"owners": 1, "lookup_calls": 2, "checksum": 77},
        "trace": _trace([
            _event("registry_register"),
            _event("registry_lookup_hit"),
            _event("core_new", owner=1000, subject=1048, size=32, value=80),
            _event("registry_lookup_hit"),
            _event("core_return", owner=1000, subject=1048, size=48, value=1),
            _event("copy_bytes_string_extent", owner=1000, subject=0, size=4096),
            _event("registry_erase", value=1),
        ]),
    }


def _read(path):
    return json.loads(path.read_text())


def _reseal(path, result=None):
    """Reseal synthetic mutations to reach semantic checks beyond file hashing."""
    if result is None:
        result = _read(path / "result.json")
    result["files"] = observe.manifest(path)
    observe.write(path / "result.json", result)
    (path / "COMPLETE").write_text(observe.sha(path / "result.json") + "\n")


def _summary_stub(report, *args):
    return {"synthetic_case": report["case"]}


def _full_inventory(path):
    """Include unmanifested files, marker files, and empty directories."""
    return {
        item.relative_to(path).as_posix(): observe.sha(item) if item.is_file() else "directory"
        for item in sorted(path.rglob("*"))
    }


class ObserveTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="n3-python-controls-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def _collection(self):
        path = self.root / "collection"
        path.mkdir()
        binaries = {
            name: hashlib.sha256(("synthetic " + name).encode()).hexdigest()
            for name in ("n3_original", "n3_diagnostic")
        }
        (path / "tool").mkdir()
        tool_files = ("observe.py", "generate.py", "audit_events.py", "event_kinds.json")
        for name in tool_files:
            shutil.copy2(observe.HERE / name, path / "tool" / name)
        shutil.copy2(observe.HERE / "contract.json", path / "contract.json")
        shutil.copy2(path / "tool" / "event_kinds.json", path / "event_kinds.json")
        frozen_inputs = {"input/" + name: observe.sha(path / "tool" / name) for name in tool_files}
        frozen_inputs["input/contract.json"] = observe.sha(path / "contract.json")
        frozen_inputs.update({"input/original/" + name: digest for name, digest in observe.BASELINE.items()})
        observe.write(path / "build-result.json", {
            "schema": "n3-artifact-v1",
            "kind": "build",
            "status": "passed",
            "timing_evidence": False,
            "baseline_headers": copy.deepcopy(observe.BASELINE),
            "commands": [
                {"argv": ["/synthetic/compiler", f"-DN3_DIAGNOSTIC={profile}"], "returncode": 0}
                for profile in (0, 1)
            ],
            "files": {**binaries, **frozen_inputs},
        })
        commands = []
        summaries = {}
        for case in observe.CASES:
            diagnostic = {
                "schema": "n3-probe-v1",
                "case": case,
                "status": "passed_control" if case == "recorder_overflow" else "passed",
                "semantic": {"checksum": 1},
                "trace": _trace(),
            }
            original = copy.deepcopy(diagnostic)
            original["trace"] = None
            for profile, report in (("original", original), ("diagnostic", diagnostic)):
                observe.write(path / f"{case}-{profile}.stdout", report)
                (path / f"{case}-{profile}.stderr").write_text("")
                commands.append({
                    "argv": [f"/synthetic/build/n3_{profile}", "--case", case],
                    "returncode": 0,
                })
            summaries[case] = _summary_stub(diagnostic)
        observe.write(path / "summaries.json", summaries)
        observe.finish(path, {
            "schema": "n3-artifact-v1",
            "kind": "collection",
            "cases": list(observe.CASES),
            "commands": commands,
            "binary_hashes": binaries,
            "executed_runner_sha256": frozen_inputs["input/observe.py"],
            "build_result_sha256": observe.sha(path / "build-result.json"),
        })
        return path

    def _audit_collection(self, path):
        with mock.patch.object(observe, "check_case", side_effect=_summary_stub) as check:
            result = observe.audit(path)
        self.assertEqual(check.call_count, len(observe.CASES))
        return result

    def _reject_collection(self, path):
        with mock.patch.object(observe, "check_case", side_effect=_summary_stub):
            with self.assertRaises(REJECTION):
                observe.audit(path)

    def _replace_build_parent(self, path, parent):
        """Rebind a synthetic parent mutation, preserving collection hash validity."""
        observe.write(path / "build-result.json", parent)
        result = _read(path / "result.json")
        result["build_result_sha256"] = observe.sha(path / "build-result.json")
        _reseal(path, result)

    def _prepared(self):
        path = self.root / "prepared"
        # This is only the Python source transform and file freeze. No compiler
        # or generated program is executed.
        with contextlib.redirect_stdout(io.StringIO()):
            observe.prepare(SimpleNamespace(source=observe.ROOT, out=path))
        observe.audit(path)
        return path

    def test_synthetic_collection_envelope_is_accepted(self):
        result = self._audit_collection(self._collection())
        self.assertEqual(result["kind"], "collection")

    def test_source_hash_mismatch_rejected_before_generation(self):
        source = self.root / "source"
        source.mkdir()
        for name in observe.BASELINE:
            (source / name).write_text("synthetic changed source\n")
        with self.assertRaises(REJECTION):
            observe.prepare(SimpleNamespace(source=source, out=self.root / "prepared"))
        self.assertFalse((self.root / "prepared" / "COMPLETE").exists())

    def test_n5_transform_anchor_drift_rejected(self):
        source = self.root / "source"
        (source / "tools").mkdir(parents=True)
        for name in observe.BASELINE:
            shutil.copy2(observe.ROOT / name, source / name)
        shutil.copy2(observe.ROOT / "tools" / "run_vm_automatic_trace.py", source / "tools" / "run_vm_automatic_trace.py")
        # Keep both header hashes and the fixed N5 source hash intact. Replace
        # only the loaded transform so failure must reach the anchor checks.
        instrument = mock.Mock(side_effect=lambda text: text)

        def substitute_transform(code, namespace):
            builtins.exec(code, namespace)
            namespace["instrument"] = instrument

        with mock.patch.object(generate_module, "exec", side_effect=substitute_transform, create=True):
            with self.assertRaises(REJECTION):
                observe.generate(source)
        instrument.assert_called_once()

    def test_prepared_two_audits_do_not_mutate_any_evidence(self):
        path = self.root / "prepared"
        with contextlib.redirect_stdout(io.StringIO()):
            observe.prepare(SimpleNamespace(source=observe.ROOT, out=path))
        baseline = _full_inventory(path)
        for audit_number in (1, 2):
            with self.subTest(audit=audit_number):
                observe.audit(path)
                self.assertEqual(_full_inventory(path), baseline)

    def test_relocated_frozen_python_audit_positive_and_negative_are_read_only(self):
        source = self._prepared()
        relocated = self.root / "relocated" / "prepared"
        shutil.copytree(source, relocated)
        shutil.rmtree(source)
        environment = os.environ.copy()
        # Do not mask accidental bytecode writes by supplying -B or inherited
        # bytecode-disable/cache-redirection environment settings.
        environment.pop("PYTHONDONTWRITEBYTECODE", None)
        environment.pop("PYTHONPYCACHEPREFIX", None)
        command = [sys.executable, str(relocated / "observe.py"), "audit", "--record", str(relocated)]
        baseline = _full_inventory(relocated)
        positive = subprocess.run(command, cwd=self.root, env=environment, capture_output=True, text=True, timeout=10)
        self.assertEqual(positive.returncode, 0, positive.stderr)
        self.assertEqual(_full_inventory(relocated), baseline)
        header = relocated / "diagnostic" / "xoffsetdatastructure.hpp"
        header.write_text(header.read_text() + "\n// synthetic relocated header mutation\n")
        _reseal(relocated)
        tampered_inventory = _full_inventory(relocated)
        negative = subprocess.run(command, cwd=self.root, env=environment, capture_output=True, text=True, timeout=10)
        self.assertNotEqual(negative.returncode, 0)
        self.assertEqual(_full_inventory(relocated), tampered_inventory)

    def test_existing_output_is_protected(self):
        path = self.root / "existing"
        path.mkdir()
        sentinel = path / "keep.txt"
        sentinel.write_bytes(b"existing evidence\n")
        with mock.patch.object(observe, "generate") as generate:
            with self.assertRaises(REJECTION):
                observe.prepare(SimpleNamespace(source=observe.ROOT, out=path))
        generate.assert_not_called()
        self.assertEqual(sentinel.read_bytes(), b"existing evidence\n")
        self.assertEqual([item.name for item in path.iterdir()], ["keep.txt"])

    def test_manifest_rejects_missing_extra_or_changed_file(self):
        for mutation in ("missing", "extra", "changed"):
            with self.subTest(mutation=mutation):
                path = self.root / mutation
                path.mkdir()
                target = path / "payload.txt"
                target.write_text("synthetic payload")
                observe.finish(path, {"schema": "n3-artifact-v1", "kind": "synthetic"})
                observe.audit_files(path)  # Establish that the fixture is valid.
                if mutation == "missing":
                    target.unlink()
                elif mutation == "extra":
                    (path / "extra.txt").write_text("unrecorded")
                else:
                    target.write_text("changed payload")
                with self.assertRaises(REJECTION):
                    observe.audit_files(path)

    def test_collection_rejects_missing_extra_and_duplicate_declared_case(self):
        path = self._collection()
        self._audit_collection(path)
        baseline = _read(path / "result.json")
        case_lists = (
            list(observe.CASES[:-1]),
            [*observe.CASES, "unexpected_case"],
            [*observe.CASES[:-1], observe.CASES[0]],
        )
        for cases in case_lists:
            with self.subTest(cases=cases):
                result = copy.deepcopy(baseline)
                result["cases"] = cases
                _reseal(path, result)
                self._reject_collection(path)

    def test_collection_rejects_binary_hash_mismatch(self):
        path = self._collection()
        self._audit_collection(path)
        result = _read(path / "result.json")
        result["binary_hashes"]["n3_diagnostic"] = "0" * 64
        _reseal(path, result)
        self._reject_collection(path)

    def test_collection_rejects_resealed_wrong_parent_baseline(self):
        path = self._collection()
        self._audit_collection(path)
        baseline = _read(path / "build-result.json")
        header = next(iter(observe.BASELINE))
        for field in ("baseline_headers", "files"):
            with self.subTest(field=field):
                parent = copy.deepcopy(baseline)
                key = header if field == "baseline_headers" else "input/original/" + header
                parent[field][key] = "0" * 64
                self._replace_build_parent(path, parent)
                self._reject_collection(path)

    def test_collection_rejects_resealed_bad_parent_schema_or_timing(self):
        path = self._collection()
        self._audit_collection(path)
        baseline = _read(path / "build-result.json")
        for changes in (
            {"schema": "unexpected-artifact-v1"},
            {"timing_evidence": True},
            {"timing_evidence": 0},
            {"status": "failed"},
            {"kind": "prepared"},
        ):
            with self.subTest(changes=changes):
                parent = copy.deepcopy(baseline)
                parent.update(changes)
                self._replace_build_parent(path, parent)
                self._reject_collection(path)

    def test_collection_rejects_resealed_bad_parent_binary_profile(self):
        path = self._collection()
        self._audit_collection(path)
        baseline = _read(path / "build-result.json")
        for commands in (
            baseline["commands"][:1],
            [{"argv": ["/synthetic/compiler", "-DN3_DIAGNOSTIC=1"], "returncode": 0}, baseline["commands"][1]],
            [baseline["commands"][0], {"argv": ["/synthetic/compiler"], "returncode": 0}],
            [baseline["commands"][0], {"argv": ["/synthetic/compiler", "-DN3_DIAGNOSTIC=1", "-DN3_DIAGNOSTIC=0"], "returncode": 0}],
        ):
            with self.subTest(commands=commands):
                parent = copy.deepcopy(baseline)
                parent["commands"] = commands
                self._replace_build_parent(path, parent)
                self._reject_collection(path)

    def test_prepared_audit_rejects_resealed_generated_header_drift(self):
        path = self._prepared()
        header = path / "diagnostic" / "xoffsetdatastructure.hpp"
        header.write_text(header.read_text() + "\n// synthetic generated-header mutation\n")
        _reseal(path)
        with self.assertRaises(REJECTION):
            observe.audit(path)

    def test_prepared_audit_rejects_resealed_wrong_baseline(self):
        path = self._prepared()
        result = _read(path / "result.json")
        result["baseline_headers"][next(iter(observe.BASELINE))] = "0" * 64
        _reseal(path, result)
        with self.assertRaises(REJECTION):
            observe.audit(path)

    def test_prepared_audit_rejects_resealed_anchor_evidence_drift(self):
        path = self._prepared()
        anchors = _read(path / "anchors.json")
        anchors.pop()
        observe.write(path / "anchors.json", anchors)
        _reseal(path)
        with self.assertRaises(REJECTION):
            observe.audit(path)

    def test_collection_rejects_missing_or_extra_binary_identity(self):
        path = self._collection()
        self._audit_collection(path)
        baseline = _read(path / "result.json")
        for binaries in ({}, {"n3_original": baseline["binary_hashes"]["n3_original"]}, {
            **baseline["binary_hashes"], "unexpected_binary": "0" * 64,
        }):
            with self.subTest(binaries=binaries):
                result = copy.deepcopy(baseline)
                result["binary_hashes"] = binaries
                _reseal(path, result)
                self._reject_collection(path)

    def test_collection_rejects_resealed_extra_unknown_case_file(self):
        path = self._collection()
        self._audit_collection(path)
        observe.write(path / "unexpected_case-diagnostic.stdout", {"case": "unexpected_case"})
        _reseal(path)
        self._reject_collection(path)

    def test_collection_rejects_raw_stdout_hash_mismatch(self):
        path = self._collection()
        self._audit_collection(path)
        stdout = path / "owner_basic-diagnostic.stdout"
        stdout.write_text(stdout.read_text() + "\n")
        self._reject_collection(path)

    def test_collection_rejects_build_result_identity_mismatch(self):
        path = self._collection()
        self._audit_collection(path)
        parent_path = path / "build-result.json"
        parent = _read(parent_path)
        parent["synthetic_mutation"] = True
        observe.write(parent_path, parent)
        # Updating the ordinary inventory cannot repair the bound build-result
        # identity recorded in the collection result.
        _reseal(path)
        self._reject_collection(path)

    def test_collection_rejects_wrong_case_and_profile_receipts(self):
        path = self._collection()
        self._audit_collection(path)
        baseline = _read(path / "result.json")
        for bad_argv in (
            ["/synthetic/build/n3_diagnostic", "--case", observe.CASES[0]],
            ["/synthetic/build/n3_original", "--case", observe.CASES[1]],
            ["/synthetic/build/n3_original", "--unknown", observe.CASES[0]],
        ):
            with self.subTest(argv=bad_argv):
                result = copy.deepcopy(baseline)
                result["commands"][0]["argv"] = bad_argv
                _reseal(path, result)
                self._reject_collection(path)

    def test_collection_rejects_original_profile_with_diagnostic_trace(self):
        path = self._collection()
        self._audit_collection(path)
        original_path = path / "owner_basic-original.stdout"
        original = _read(original_path)
        original["trace"] = _trace()
        observe.write(original_path, original)
        _reseal(path)
        self._reject_collection(path)

    def test_collection_rejects_resealed_raw_semantic_mismatch(self):
        path = self._collection()
        self._audit_collection(path)
        original_path = path / "owner_basic-original.stdout"
        original = _read(original_path)
        original["semantic"]["checksum"] += 1
        observe.write(original_path, original)
        _reseal(path)
        self._reject_collection(path)

    def test_collection_rejects_recomputed_summary_mismatch(self):
        path = self._collection()
        self._audit_collection(path)
        summary_path = path / "summaries.json"
        summaries = _read(summary_path)
        summaries["owner_basic"] = {"synthetic_case": "incorrect"}
        observe.write(summary_path, summaries)
        _reseal(path)
        self._reject_collection(path)

    def test_diagnostic_artifact_cannot_masquerade_as_timing(self):
        path = self._collection()
        self._audit_collection(path)
        result = _read(path / "result.json")
        result["timing_evidence"] = True
        _reseal(path, result)
        self._reject_collection(path)

    def test_real_check_case_accepts_synthetic_basic_trace(self):
        result = observe.check_case(_basic_report())
        self.assertEqual(result["by_kind"]["registry_lookup_hit"]["count"], 2)

    def test_real_check_case_rejects_wrong_counter(self):
        report = _basic_report()
        observe.check_case(report)
        del report["trace"]["events"][1]
        for sequence, item in enumerate(report["trace"]["events"], 1):
            item["sequence"] = sequence
        # The resulting trace remains structurally consistent; the fixed-case
        # expected count is what must reject this missing lookup.
        observe.validate_trace(report["trace"])
        with self.assertRaises(REJECTION):
            observe.check_case(report)

    def test_real_check_case_rejects_wrong_owner(self):
        report = _basic_report()
        observe.check_case(report)
        report["trace"]["events"][1]["owner"] = 999
        with self.assertRaises(REJECTION):
            observe.check_case(report)

    def test_real_check_case_rejects_outstanding_nonregistry_allocation(self):
        report = _basic_report()
        report["trace"]["events"].append(_event("host_allocate", owner=1000, subject=9000, size=64, value=3))
        report["trace"]["events"][-1]["sequence"] = len(report["trace"]["events"])
        observe.validate_trace(report["trace"])
        with self.assertRaises(REJECTION):
            observe.check_case(report)

    def test_real_check_case_rejects_wrong_semantic_constant(self):
        report = _basic_report()
        report["semantic"]["checksum"] = 78
        with self.assertRaises(REJECTION):
            observe.check_case(report)

    def test_real_check_case_rejects_invalid_recorder(self):
        for changes in ({"valid": False}, {"dropped": 1}, {"valid": False, "dropped": 1}):
            with self.subTest(changes=changes):
                report = _basic_report()
                report["trace"].update(changes)
                with self.assertRaises(REJECTION):
                    observe.check_case(report)

    def test_real_check_case_rejects_diagnostic_trace_as_timing(self):
        report = _basic_report()
        report["trace"]["timing_evidence"] = True
        with self.assertRaises(REJECTION):
            observe.check_case(report)

    def test_overflow_control_requires_actual_invalid_recorder(self):
        report = {
            "schema": "n3-probe-v1", "case": "recorder_overflow",
            "status": "passed_control", "semantic": {"checksum": 17000}, "trace": _trace(),
        }
        with self.assertRaises(REJECTION):
            observe.check_case(report)
        report["trace"].update(valid=False, dropped=1)
        self.assertEqual(observe.check_case(report), {"control": "invalid_record_rejected"})


if __name__ == "__main__":
    unittest.main()
