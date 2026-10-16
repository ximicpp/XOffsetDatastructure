"""Diagnostic evidence audit controls; allocator executables remain inert files.

The fixture covers the collector's declared matrix. Only Python auditors run,
including an explicit optimized-Python check; no C++ build or benchmark runs.
"""
from __future__ import annotations

import argparse
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


TOOLS = Path(__file__).resolve().parents[1]
SCRIPT = TOOLS / "collect_vm_allocator_diagnostics.py"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location("stage3_allocator_diagnostics", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("could not load allocator diagnostic collector")
COLLECTOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COLLECTOR)
R = COLLECTOR.R

MICRO_COUNTERS = {
    "host_allocation_calls", "host_requested_bytes", "host_metadata_peak_bytes",
    "core_scans", "core_moved_bytes", "core_new", "core_whole", "core_split", "wire_scans",
}
PROCESS_COUNTERS = {
    "core_scans", "core_moved_bytes", "core_new", "core_whole", "core_split",
    "wire_scans", "wire_split", "wire_reuse", "wire_retire", "wire_flush",
    "wire_transfer_blocks", "expand_attempts", "expand_existing", "expand_grown",
    "element_migration_bytes", "metadata_allocation_calls", "metadata_allocated_bytes",
    "max_owner_metadata_capacity",
}


class DiagnosticEvidenceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self) -> None:
        self.temp.cleanup()

    def prepared(self) -> Path:
        out = self.root / "prepared"
        out.mkdir()
        for variant in ("F", "A-core"):
            (out / variant / "source").mkdir(parents=True)
            for target in ("diag", "fragment_diag", "trace_diag", "micro", "fragment", "trace"):
                (out / variant / target).write_text("inert " + variant + " " + target)
            for name in ("xoffsetdatastructure.hpp", "xoffset_virtual_memory.hpp"):
                (out / variant / "source" / name).write_text("frozen " + variant + " " + name)
        shutil.copy2(TOOLS / "run_vm_allocator.py", out / "runner.py")
        R.write(out / "prepared.json", {
            "status": "prepared", "variants": ["F", "A-core"],
            "targets": ["diag", "fragment_diag", "trace_diag", "micro", "fragment", "trace"],
            "files": R.manifest(out), "external_files": {}, "dependencies": {}, "deps": str(out),
        })
        return out

    def sample(self, suite: str, case: dict) -> dict:
        sample = {"semantic_checksum": 713, "operations": 128, "elapsed_ms": 10.0,
                  "diagnostic": True, "used_end": 16384, "logical_bytes": 65536}
        for flag, value in zip(case["args"][::2], case["args"][1::2]):
            key = flag[2:].replace("-", "_")
            if key == "reserve":
                key = "requested_reserve_bytes"
            sample[key] = (value == "1") if key == "enabled" else (
                int(value) if value.isdecimal() else value)
        if suite == "micro":
            sample["operations"] = sample["rounds"] * (
                sample["size"] if sample["shape"] in ("head", "dense") else 1)
            sample.update({key: 1 for key in MICRO_COUNTERS})
        elif suite == "fragment":
            sample["metrics"] = {"used_end": 16384, "logical_bytes": 65536,
                                 "live_payload_bytes": 4096, "pressure_successes": 7,
                                 "pressure_verified_bytes": 7168, "pressure_used_end": 32768}
        else:
            sample["trace_checksum"] = 917
            sample.update({
                "core_free_bytes": 64, "wire_free_bytes": 128, "wire_physical_bytes": 256,
                "wire_blocks": 1, "accessible_bytes": 65536, "reserved_bytes": 65536,
                "primary_offset": 256, "secondary_offset": 512, "primary_physical_bytes": 256,
                "secondary_physical_bytes": 256, "primary_capacity": 64, "secondary_capacity": 64,
                "allocation_retries": 0, "observed_growth_operations": 0, "retired_values_checked": 0,
            })
        return sample

    def rows(self, prep: Path) -> list[dict]:
        rows = []
        for variant, suite, case in COLLECTOR.matrix({"variants": ["F", "A-core"]}):
            target = {"micro": "diag", "fragment": "fragment_diag", "trace": "trace_diag"}[suite]
            schema = MICRO_COUNTERS if suite == "micro" else PROCESS_COUNTERS
            rows.append({
                "variant": variant, "suite": suite, "case": case["id"], "sequence": len(rows),
                "command": [str(prep / variant / target)] + case["args"],
                "diagnostic": True, "sample": self.sample(suite, case),
                "counters": {key: 1 for key in schema},
            })
        return rows

    def seal(self, out: Path, state: dict | None = None, rows: list[dict] | None = None) -> None:
        if state is None:
            state = json.loads((out / "result.json").read_text())
        if rows is not None:
            (out / "raw.jsonl").write_text("".join(json.dumps(row) + "\n" for row in rows))
            state["samples"] = len(rows)
        state["raw_sha256"] = R.sha(out / "raw.jsonl")
        R.write(out / "result.json", state)
        (out / "COMPLETE").write_text(R.sha(out / "result.json") + "\n")

    def evidence(self) -> Path:
        prep = self.prepared()
        out = self.root / "diagnostics"
        out.mkdir()
        self.seal(out, {
            "status": "complete", "formal_timing_forbidden": True,
            "prepared": str(prep), "prepared_sha256": R.sha(prep / "prepared.json"),
        }, self.rows(prep))
        return out

    def changed_rows(self, change) -> Path:
        out = self.evidence()
        rows = [json.loads(line) for line in (out / "raw.jsonl").read_text().splitlines()]
        change(rows)
        self.seal(out, rows=rows)
        return out

    def assert_rejected(self, out: Path) -> None:
        with self.assertRaises((RuntimeError, ValueError, KeyError, TypeError)):
            COLLECTOR.audit(out)

    def timing_window(self, prep: Path, suite: str) -> Path:
        out = self.root / ("timing-" + suite)
        out.mkdir()
        rows = []
        cases = R.cases(suite)
        for case in cases:
            for phase, pairs in (("warmup", 1), ("measured", 8)):
                for pair in range(pairs):
                    order = ("before", "after") if pair % 2 == 0 else ("after", "before")
                    for position, label in enumerate(order):
                        variant = "F" if label == "before" else "A-core"
                        sample = self.sample(suite, case)
                        sample["diagnostic"] = False
                        rows.append({
                            "case": case["id"], "phase": phase, "pair": pair,
                            "position": position, "label": label, "variant": variant,
                            "sequence": len(rows),
                            "command": [str(prep / variant / suite)] + case["args"],
                            "sample": sample,
                        })
        self.seal(out, {
            "status": "complete", "suite": suite, "before": "F", "after": "A-core",
            "cases": cases, "pairs": 8, "warmup_pairs": 1,
            "prepared": str(prep), "prepared_sha256": R.sha(prep / "prepared.json"),
        }, rows)
        return out

    def comparison_fixture(self, changed_header: str | None = None):
        diagnostic = self.evidence()
        prep = Path(json.loads((diagnostic / "result.json").read_text())["prepared"])
        if changed_header is not None:
            cloned = self.root / "timing-prepared"
            shutil.copytree(prep, cloned)
            relative = "A-core/source/" + changed_header
            (cloned / relative).write_text("different pristine candidate header")
            meta = json.loads((cloned / "prepared.json").read_text())
            meta["files"][relative] = R.sha(cloned / relative)
            R.write(cloned / "prepared.json", meta)
            prep = cloned
        records = [(self.timing_window(prep, suite), prep) for suite in ("micro", "fragment", "trace")]
        return diagnostic, records

    def change_window_rows(self, out: Path, change) -> None:
        rows = [json.loads(line) for line in (out / "raw.jsonl").read_text().splitlines()]
        change(rows)
        self.seal(out, rows=rows)

    @staticmethod
    def first(rows: list[dict], suite: str) -> dict:
        return next(row for row in rows if row["suite"] == suite)

    def test_declared_schemas_are_exact_and_distinct(self) -> None:
        self.assertEqual(set(COLLECTOR.counter_schema("micro")), MICRO_COUNTERS)
        self.assertEqual(set(COLLECTOR.counter_schema("fragment")), PROCESS_COUNTERS)
        self.assertEqual(set(COLLECTOR.counter_schema("trace")), PROCESS_COUNTERS)
        self.assertEqual(len(PROCESS_COUNTERS), 18)

    def test_complete_matrix_audits(self) -> None:
        result = COLLECTOR.audit(self.evidence())
        self.assertEqual(result, {"status": "audited", "samples": 222,
                                  "formal_timing_forbidden": True})

    def test_empty_counter_dictionary_rejected_in_each_suite(self) -> None:
        for suite in ("micro", "fragment", "trace"):
            with self.subTest(suite=suite), tempfile.TemporaryDirectory() as temp:
                self.root = Path(temp)
                self.assert_rejected(self.changed_rows(lambda rows: self.first(rows, suite).update(counters={})))

    def test_missing_counter_rejected_in_each_suite(self) -> None:
        for suite in ("micro", "fragment", "trace"):
            with self.subTest(suite=suite), tempfile.TemporaryDirectory() as temp:
                self.root = Path(temp)
                self.assert_rejected(self.changed_rows(
                    lambda rows: self.first(rows, suite)["counters"].pop("core_scans")))

    def test_extra_counter_rejected_in_each_suite(self) -> None:
        for suite in ("micro", "fragment", "trace"):
            with self.subTest(suite=suite), tempfile.TemporaryDirectory() as temp:
                self.root = Path(temp)
                self.assert_rejected(self.changed_rows(
                    lambda rows: self.first(rows, suite)["counters"].update(undeclared_counter=1)))

    def test_negative_boolean_float_and_string_counter_rejected(self) -> None:
        for suite in ("micro", "fragment", "trace"):
            for value in (-1, True, False, 1.0, "1", None):
                with self.subTest(suite=suite, value=value), tempfile.TemporaryDirectory() as temp:
                    self.root = Path(temp)
                    self.assert_rejected(self.changed_rows(
                        lambda rows: self.first(rows, suite)["counters"].update(core_scans=value)))

    def test_missing_counter_record_rejected(self) -> None:
        self.assert_rejected(self.changed_rows(lambda rows: rows[0].pop("counters")))

    def test_nonmapping_counter_record_rejected(self) -> None:
        self.assert_rejected(self.changed_rows(lambda rows: rows[0].update(counters=[])))

    def test_counter_schema_cannot_be_borrowed_from_other_suite(self) -> None:
        self.assert_rejected(self.changed_rows(
            lambda rows: rows[0].update(counters={key: 1 for key in PROCESS_COUNTERS})))

    def test_missing_matrix_row_rejected_even_after_reseal(self) -> None:
        self.assert_rejected(self.changed_rows(lambda rows: rows.pop()))

    def test_wrong_semantic_work_rejected_even_after_reseal(self) -> None:
        self.assert_rejected(self.changed_rows(
            lambda rows: rows[-1]["sample"].update(semantic_checksum=999)))

    def test_diagnostic_classification_must_be_true(self) -> None:
        self.assert_rejected(self.changed_rows(lambda rows: rows[0].update(diagnostic=False)))

    def test_wrong_command_rejected(self) -> None:
        def change(rows):
            rows[0]["command"][-1] = "1"
        self.assert_rejected(self.changed_rows(change))

    def test_raw_hash_and_prepared_binary_hash_checked(self) -> None:
        out = self.evidence()
        (out / "raw.jsonl").write_text((out / "raw.jsonl").read_text() + "\n")
        self.assert_rejected(out)
        # Restore and reseal the original matrix, then change its frozen executable.
        state = json.loads((out / "result.json").read_text())
        prep = Path(state["prepared"])
        self.seal(out, rows=self.rows(prep))
        (prep / "F" / "diag").write_text("changed diagnostic executable")
        self.assert_rejected(out)

    def test_counter_loss_during_collection_cannot_publish_completion(self) -> None:
        prep = self.prepared()
        out = self.root / "collected"
        case = copy.deepcopy(R.cases("micro")[0])
        sample = self.sample("micro", case)
        for name in MICRO_COUNTERS:
            sample.pop(name)
        with mock.patch.object(COLLECTOR, "matrix", return_value=[("F", "micro", case)]), \
                mock.patch.object(R, "run", return_value=argparse.Namespace(
                    stdout=json.dumps(sample), stderr="", returncode=0)):
            with self.assertRaises((RuntimeError, ValueError, KeyError, TypeError)):
                COLLECTOR.collect(argparse.Namespace(prepared=prep, out=out))
        self.assertEqual(json.loads((out / "result.json").read_text())["status"], "failed")
        self.assertFalse((out / "COMPLETE").exists())

    def test_python_optimized_accepts_valid_and_rejects_missing_counter(self) -> None:
        out = self.evidence()
        valid = subprocess.run([sys.executable, "-O", str(SCRIPT), "audit", str(out)],
                               capture_output=True, text=True, timeout=30)
        self.assertEqual(valid.returncode, 0, valid.stderr)
        rows = [json.loads(line) for line in (out / "raw.jsonl").read_text().splitlines()]
        rows[0]["counters"].pop("core_scans")
        self.seal(out, rows=rows)
        invalid = subprocess.run([sys.executable, "-O", str(SCRIPT), "audit", str(out)],
                                 capture_output=True, text=True, timeout=30)
        self.assertNotEqual(invalid.returncode, 0)

    def test_matching_formal_layouts_audit_all_counterparts(self) -> None:
        diagnostic, records = self.comparison_fixture()
        result = COLLECTOR.compare_timing(diagnostic, records)
        self.assertEqual(result, {
            "status": "audited", "diagnostic_samples": 222, "matched_timing_samples": 1998,
            "timing_values_compared": False,
        })

    def test_formal_times_and_rss_are_excluded_from_layout_comparison(self) -> None:
        diagnostic, records = self.comparison_fixture()
        for window, _ in records:
            def change(rows):
                for index, row in enumerate(rows):
                    row["sample"]["elapsed_ms"] = 1000.0 + index
                    row["sample"]["memory"] = {"rss_bytes": 123456 + index}
            self.change_window_rows(window, change)
        self.assertFalse(COLLECTOR.compare_timing(diagnostic, records)["timing_values_compared"])

    def test_changed_formal_layout_rejected_after_valid_timing_audit(self) -> None:
        diagnostic, records = self.comparison_fixture()
        window, prep = records[0]
        self.change_window_rows(window, lambda rows: rows[0]["sample"].update(used_end=16385))
        R.audit(window, prepared_override=prep)
        with self.assertRaisesRegex(RuntimeError, "logical layout differs"):
            COLLECTOR.compare_timing(diagnostic, records)

    def test_changed_formal_semantics_rejected_even_if_each_window_is_consistent(self) -> None:
        diagnostic, records = self.comparison_fixture()
        window, prep = records[0]
        case = R.cases("micro")[0]["id"]
        def change(rows):
            for row in rows:
                if row["case"] == case:
                    row["sample"]["semantic_checksum"] = 999
        self.change_window_rows(window, change)
        R.audit(window, prepared_override=prep)
        with self.assertRaisesRegex(RuntimeError, "logical layout differs"):
            COLLECTOR.compare_timing(diagnostic, records)

    def test_pressure_success_difference_rejected_outside_common_work_identity(self) -> None:
        diagnostic, records = self.comparison_fixture()
        window, prep = records[1]
        self.change_window_rows(window,
                                lambda rows: rows[0]["sample"]["metrics"].update(pressure_successes=8))
        R.audit(window, prepared_override=prep)
        with self.assertRaisesRegex(RuntimeError, "logical layout differs"):
            COLLECTOR.compare_timing(diagnostic, records)

    def test_trace_physical_offset_difference_rejected(self) -> None:
        diagnostic, records = self.comparison_fixture()
        window, prep = records[2]
        self.change_window_rows(window, lambda rows: rows[0]["sample"].update(primary_offset=257))
        R.audit(window, prepared_override=prep)
        with self.assertRaisesRegex(RuntimeError, "logical layout differs"):
            COLLECTOR.compare_timing(diagnostic, records)

    def test_missing_formal_suite_cannot_cover_all_diagnostics(self) -> None:
        diagnostic, records = self.comparison_fixture()
        with self.assertRaisesRegex(RuntimeError, "missing formal layout crosschecks"):
            COLLECTOR.compare_timing(diagnostic, records[:-1])

    def test_each_pristine_candidate_header_must_match_timing_source(self) -> None:
        for name in ("xoffsetdatastructure.hpp", "xoffset_virtual_memory.hpp"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temp:
                self.root = Path(temp)
                diagnostic, records = self.comparison_fixture(changed_header=name)
                with self.assertRaisesRegex(RuntimeError, "candidate sources differ"):
                    COLLECTOR.compare_timing(diagnostic, records)

    def test_python_optimized_comparison_rejects_changed_layout(self) -> None:
        diagnostic, records = self.comparison_fixture()
        program = (
            "import sys,json;from pathlib import Path;sys.path.insert(0,sys.argv[1]);"
            "import collect_vm_allocator_diagnostics as c;"
            "print(json.dumps(c.compare_timing(Path(sys.argv[2]),"
            "[(Path(w),Path(p)) for w,p in json.loads(sys.argv[3])])))"
        )
        command = [sys.executable, "-O", "-c", program, str(TOOLS), str(diagnostic),
                   json.dumps([(str(w), str(p)) for w, p in records])]
        valid = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertEqual(valid.returncode, 0, valid.stderr)
        window, _ = records[0]
        self.change_window_rows(window, lambda rows: rows[0]["sample"].update(used_end=16385))
        invalid = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertNotEqual(invalid.returncode, 0)


if __name__ == "__main__":
    unittest.main()
