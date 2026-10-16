"""Stage III evidence rejects incomplete, changed, or semantically unequal runs.

All executables are inert fixture files. Subprocess execution is mocked except
for the explicit Python -O audit check; no allocator benchmark is run here.
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


SCRIPT = Path(__file__).resolve().parents[1] / "run_vm_allocator.py"
SPEC = importlib.util.spec_from_file_location("stage3_allocator_runner", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("could not load Stage III runner")
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)


class AllocatorEvidenceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.case = copy.deepcopy(RUNNER.cases("micro")[0])

    def tearDown(self) -> None:
        self.temp.cleanup()

    def sample(self, elapsed: float = 10.0) -> dict:
        return {
            "workload": "lookup", "shape": "head", "order": "forward",
            "size": 8, "rounds": 16, "operations": 128,
            "semantic_checksum": 713, "elapsed_ms": elapsed,
            "diagnostic": False,
        }

    def prepared(self) -> Path:
        out = self.root / "prepared"
        out.mkdir()
        for variant in ("F", "A-core"):
            (out / variant).mkdir()
            (out / variant / "micro").write_text("inert frozen " + variant)
        shutil.copy2(SCRIPT, out / "runner.py")
        RUNNER.write(out / "prepared.json", {
            "status": "prepared", "variants": ["F", "A-core"],
            "targets": ["micro"], "files": RUNNER.manifest(out),
            "external_files": {}, "dependencies": {}, "deps": str(out),
        })
        return out

    def rows(self, prep: Path) -> list[dict]:
        rows = []
        for phase, count in (("warmup", 1), ("measured", 8)):
            for pair in range(count):
                order = ("before", "after") if pair % 2 == 0 else ("after", "before")
                for position, label in enumerate(order):
                    variant = "F" if label == "before" else "A-core"
                    rows.append({
                        "case": self.case["id"], "phase": phase, "pair": pair,
                        "position": position, "label": label, "variant": variant,
                        "sequence": len(rows),
                        "command": [str(prep / variant / "micro")] + self.case["args"],
                        "sample": self.sample(10.0 if label == "before" else 8.0),
                    })
        return rows

    def seal(self, out: Path, state: dict | None = None, rows: list[dict] | None = None) -> None:
        if state is None:
            state = json.loads((out / "result.json").read_text())
        if rows is not None:
            (out / "raw.jsonl").write_text("".join(json.dumps(r) + "\n" for r in rows))
            state["samples"] = len(rows)
        state["raw_sha256"] = RUNNER.sha(out / "raw.jsonl")
        RUNNER.write(out / "result.json", state)
        (out / "COMPLETE").write_text(RUNNER.sha(out / "result.json") + "\n")

    def evidence(self, status: str = "complete") -> Path:
        prep = self.prepared()
        out = self.root / "window"
        out.mkdir()
        state = {
            "status": status, "suite": "micro", "before": "F", "after": "A-core",
            "cases": [self.case], "pairs": 8, "warmup_pairs": 1,
            "prepared": str(prep), "prepared_sha256": RUNNER.sha(prep / "prepared.json"),
            "environment_before": {"fixture": True}, "environment_after": {"fixture": True},
        }
        self.seal(out, state, self.rows(prep))
        return out

    def changed_rows(self, mutation) -> Path:
        out = self.evidence()
        rows = [json.loads(line) for line in (out / "raw.jsonl").read_text().splitlines()]
        mutation(rows)
        self.seal(out, rows=rows)
        return out

    def assert_rejected(self, out: Path) -> None:
        with self.assertRaises((RuntimeError, ValueError, KeyError, TypeError)):
            RUNNER.audit(out)

    def test_complete_eight_balanced_pairs(self) -> None:
        result = RUNNER.audit(self.evidence())
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]["paired_before_over_after"]["median"], 1.25)
        self.assertEqual(set(result[0]["order_groups"]), {"0", "1"})

    def test_failed_state_rejected_even_if_resealed(self) -> None:
        self.assert_rejected(self.evidence("failed"))

    def test_running_state_rejected_even_if_resealed(self) -> None:
        self.assert_rejected(self.evidence("running"))

    def test_completion_marker_is_required(self) -> None:
        out = self.evidence()
        (out / "COMPLETE").unlink()
        self.assert_rejected(out)

    def test_changed_result_cannot_redeclare_workload(self) -> None:
        out = self.evidence()
        state = json.loads((out / "result.json").read_text())
        state["finished"] = "changed after completion"
        RUNNER.write(out / "result.json", state)
        self.assert_rejected(out)

    def test_changed_raw_is_rejected_before_parsing(self) -> None:
        out = self.evidence()
        with (out / "raw.jsonl").open("a") as f:
            f.write("corruption\n")
        self.assert_rejected(out)

    def test_truncated_raw_remains_rejected_with_updated_hash(self) -> None:
        out = self.changed_rows(lambda rows: rows.pop())
        self.assert_rejected(out)

    def test_duplicate_sample_cannot_replace_missing_pair(self) -> None:
        def duplicate(rows):
            rows[-1] = copy.deepcopy(rows[-2])
            rows[-1]["sequence"] = len(rows) - 1
        self.assert_rejected(self.changed_rows(duplicate))

    def test_broken_sequence_rejected(self) -> None:
        self.assert_rejected(self.changed_rows(lambda rows: rows[-1].update(sequence=0)))

    def test_ab_ba_order_rejected_when_position_matches_reordering(self) -> None:
        def reorder(rows):
            rows[2], rows[3] = rows[3], rows[2]
            for i in (2, 3):
                rows[i]["sequence"] = i
                rows[i]["position"] = i - 2
        self.assert_rejected(self.changed_rows(reorder))

    def test_wrong_position_rejected(self) -> None:
        self.assert_rejected(self.changed_rows(lambda rows: rows[0].update(position=1)))

    def test_variant_label_mismatch_rejected(self) -> None:
        self.assert_rejected(self.changed_rows(lambda rows: rows[0].update(variant="A-core")))

    def test_unequal_semantic_values_rejected(self) -> None:
        self.assert_rejected(self.changed_rows(lambda rows: rows[-1]["sample"].update(semantic_checksum=999)))

    def test_unequal_operation_counts_rejected(self) -> None:
        self.assert_rejected(self.changed_rows(lambda rows: rows[-1]["sample"].update(operations=127)))

    def test_elapsed_nan_rejected(self) -> None:
        self.assert_rejected(self.changed_rows(lambda rows: rows[-1]["sample"].update(elapsed_ms=float("nan"))))

    def test_elapsed_zero_rejected(self) -> None:
        self.assert_rejected(self.changed_rows(lambda rows: rows[-1]["sample"].update(elapsed_ms=0)))

    def test_boolean_elapsed_rejected(self) -> None:
        self.assert_rejected(self.changed_rows(lambda rows: rows[-1]["sample"].update(elapsed_ms=True)))

    def test_workload_parameters_must_match_case_even_if_checksums_match(self) -> None:
        self.assert_rejected(self.changed_rows(lambda rows: rows[-1]["sample"].update(order="reverse")))

    def test_every_sample_cannot_report_the_same_wrong_case_parameters(self) -> None:
        def wrong_case(rows):
            for row in rows:
                row["sample"]["order"] = "reverse"
        self.assert_rejected(self.changed_rows(wrong_case))

    def test_every_sample_cannot_complete_less_than_registered_work(self) -> None:
        def less_work(rows):
            for row in rows:
                row["sample"]["operations"] = 127
        self.assert_rejected(self.changed_rows(less_work))

    def test_diagnostic_samples_cannot_be_timing_evidence(self) -> None:
        self.assert_rejected(self.changed_rows(lambda rows: rows[-1]["sample"].update(diagnostic=True)))

    def test_recorded_command_must_match_frozen_variant_and_case(self) -> None:
        def change_command(rows):
            rows[-1]["command"][-1] = "1"
        self.assert_rejected(self.changed_rows(change_command))

    def test_unknown_case_cannot_be_declared_and_resealed(self) -> None:
        out = self.evidence()
        state = json.loads((out / "result.json").read_text())
        state["cases"][0]["id"] = "invented-case"
        rows = [json.loads(line) for line in (out / "raw.jsonl").read_text().splitlines()]
        for row in rows:
            row["case"] = "invented-case"
        self.seal(out, state, rows)
        self.assert_rejected(out)

    def test_declared_case_arguments_cannot_change_after_registration(self) -> None:
        out = self.evidence()
        state = json.loads((out / "result.json").read_text())
        state["cases"][0]["args"][-1] = "1"
        self.seal(out, state)
        self.assert_rejected(out)

    def test_duplicate_declared_cases_cannot_hide_outside_rows(self) -> None:
        out = self.evidence()
        state = json.loads((out / "result.json").read_text())
        state["cases"].append(copy.deepcopy(state["cases"][0]))
        rows = [json.loads(line) for line in (out / "raw.jsonl").read_text().splitlines()]
        outside = copy.deepcopy(rows)
        for row in outside:
            row["case"] = "outside-declared-matrix"
            row["sequence"] += len(rows)
        self.seal(out, state, rows + outside)
        self.assert_rejected(out)

    def test_unknown_case_row_rejected(self) -> None:
        self.assert_rejected(self.changed_rows(lambda rows: rows[-1].update(case="outside")))

    def test_frozen_prepared_manifest_identity_checked_offline(self) -> None:
        out = self.evidence()
        prep = Path(json.loads((out / "result.json").read_text())["prepared"])
        meta = json.loads((prep / "prepared.json").read_text())
        meta["compiler"] = "changed compiler identity"
        RUNNER.write(prep / "prepared.json", meta)
        self.assert_rejected(out)

    def test_frozen_prepared_binary_hash_checked_offline(self) -> None:
        out = self.evidence()
        prep = Path(json.loads((out / "result.json").read_text())["prepared"])
        (prep / "F/micro").write_text("changed binary")
        self.assert_rejected(out)

    def test_offline_audit_uses_relocated_prepared_override(self) -> None:
        out = self.evidence()
        original = Path(json.loads((out / "result.json").read_text())["prepared"])
        relocated = self.root / "relocated"
        shutil.move(original, relocated)
        self.assertEqual(len(RUNNER.audit(out, prepared_override=relocated)), 1)

    def test_offline_audit_does_not_require_original_external_runtime(self) -> None:
        out = self.evidence()
        state = json.loads((out / "result.json").read_text())
        prep = Path(state["prepared"])
        meta = json.loads((prep / "prepared.json").read_text())
        meta["external_files"] = {str(self.root / "absent-runtime"): "0" * 64}
        RUNNER.write(prep / "prepared.json", meta)
        state["prepared_sha256"] = RUNNER.sha(prep / "prepared.json")
        self.seal(out, state)
        self.assertEqual(len(RUNNER.audit(out)), 1)

    def test_prepared_status_must_be_prepared(self) -> None:
        prep = self.prepared()
        state = json.loads((prep / "prepared.json").read_text())
        state["status"] = "failed"
        RUNNER.write(prep / "prepared.json", state)
        with self.assertRaisesRegex(RuntimeError, "preparation incomplete"):
            RUNNER.prepared(prep)

    def args(self, prep: Path, out: Path) -> argparse.Namespace:
        return argparse.Namespace(prepared=prep, out=out, suite="micro", before="F",
                                  after="A-core", filter=self.case["id"])

    def test_runtime_binary_mutation_cannot_publish_completion(self) -> None:
        prep = self.prepared()
        out = self.root / "runtime"
        def changed_during_execution(argv, timeout=300):
            (prep / "F/micro").write_text("changed during execution")
            return argparse.Namespace(stdout=json.dumps(self.sample()), stderr="", returncode=0)
        with mock.patch.object(RUNNER, "environment", return_value={}), \
             mock.patch.object(RUNNER, "run", side_effect=changed_during_execution):
            with self.assertRaises(RuntimeError):
                RUNNER.window(self.args(prep, out))
        self.assertEqual(json.loads((out / "result.json").read_text())["status"], "failed")
        self.assertFalse((out / "COMPLETE").exists())

    def test_runtime_binary_and_manifest_mutation_cannot_rename_the_freeze(self) -> None:
        prep = self.prepared()
        out = self.root / "runtime"
        def changed_during_execution(argv, timeout=300):
            (prep / "F/micro").write_text("changed during execution")
            meta = json.loads((prep / "prepared.json").read_text())
            meta["files"]["F/micro"] = RUNNER.sha(prep / "F/micro")
            RUNNER.write(prep / "prepared.json", meta)
            return argparse.Namespace(stdout=json.dumps(self.sample()), stderr="", returncode=0)
        with mock.patch.object(RUNNER, "environment", return_value={}), \
             mock.patch.object(RUNNER, "run", side_effect=changed_during_execution):
            with self.assertRaises(RuntimeError):
                RUNNER.window(self.args(prep, out))
        self.assertEqual(json.loads((out / "result.json").read_text())["status"], "failed")
        self.assertFalse((out / "COMPLETE").exists())

    def test_empty_filter_publishes_failed_state(self) -> None:
        prep = self.prepared()
        out = self.root / "runtime"
        args = self.args(prep, out)
        args.filter = "unknown-case"
        with mock.patch.object(RUNNER, "environment", return_value={}):
            with self.assertRaises(RuntimeError):
                RUNNER.window(args)
        self.assertEqual(json.loads((out / "result.json").read_text())["status"], "failed")
        self.assertFalse((out / "COMPLETE").exists())

    def test_failed_command_preserves_identity_and_output(self) -> None:
        prep = self.prepared()
        out = self.root / "runtime"
        expected = [str(prep / "F/micro")] + self.case["args"]
        failure = subprocess.CalledProcessError(19, expected, output="partial stdout\n", stderr="failure stderr\n")
        with mock.patch.object(RUNNER, "environment", return_value={}), \
             mock.patch.object(RUNNER, "run", side_effect=failure):
            with self.assertRaises(subprocess.CalledProcessError):
                RUNNER.window(self.args(prep, out))
        state = json.loads((out / "result.json").read_text())
        self.assertEqual(state["status"], "failed")
        self.assertEqual(state["failed_command"]["argv"], expected)
        self.assertEqual(state["failed_command"]["returncode"], 19)
        self.assertIn("partial stdout", state["failed_command"]["stdout"])
        self.assertIn("failure stderr", state["failed_command"]["stderr"])
        self.assertIn("partial stdout", (out / "failed-command.log").read_text())
        self.assertIn("failure stderr", (out / "failed-command.log").read_text())
        self.assertFalse((out / "COMPLETE").exists())

    def test_timeout_preserves_partial_outputs_and_command(self) -> None:
        prep = self.prepared()
        out = self.root / "runtime"
        expected = [str(prep / "F/micro")] + self.case["args"]
        failure = subprocess.TimeoutExpired(expected, 300, output=b"partial before timeout\n", stderr=b"timeout stderr\n")
        with mock.patch.object(RUNNER, "environment", return_value={}), \
             mock.patch.object(RUNNER, "run", side_effect=failure):
            with self.assertRaises(subprocess.TimeoutExpired):
                RUNNER.window(self.args(prep, out))
        state = json.loads((out / "result.json").read_text())
        self.assertEqual(state["status"], "failed")
        self.assertEqual(state["failed_command"]["argv"], expected)
        self.assertEqual(state["failed_command"]["timeout_seconds"], 300)
        self.assertIn("partial before timeout", state["failed_command"]["stdout"])
        self.assertIn("timeout stderr", state["failed_command"]["stderr"])
        self.assertFalse((out / "COMPLETE").exists())

    def test_validation_failure_preserves_exact_failed_target(self) -> None:
        prep = self.prepared()
        meta = json.loads((prep / "prepared.json").read_text())
        (prep / "F/model").write_text("inert model")
        meta["targets"] = ["model"]
        meta["variants"] = ["F"]
        meta["files"]["F/model"] = RUNNER.sha(prep / "F/model")
        RUNNER.write(prep / "prepared.json", meta)
        out = self.root / "validation"
        expected = [str(prep / "F/model"), "10000", "8"]
        failure = subprocess.CalledProcessError(7, expected, output="seed=713 step=19\n", stderr="model failed\n")
        args = argparse.Namespace(prepared=prep, out=out, operations=10000, seeds=8, targets="model")
        with mock.patch.object(RUNNER, "run", side_effect=failure):
            with self.assertRaises(subprocess.CalledProcessError):
                RUNNER.validate(args)
        state = json.loads((out / "result.json").read_text())
        self.assertEqual(state["status"], "failed")
        self.assertEqual(state["failed_command"]["argv"], expected)
        self.assertEqual(state["failed_command"]["returncode"], 7)
        self.assertIn("seed=713 step=19", state["failed_command"]["stdout"])
        self.assertIn("model failed", (out / "failed-command.log").read_text())
        self.assertFalse((out / "COMPLETE").exists())

    def test_default_validation_rejects_missing_mandatory_targets(self) -> None:
        prep = self.prepared()
        out = self.root / "validation"
        args = argparse.Namespace(prepared=prep, out=out, operations=10000, seeds=8)
        with self.assertRaisesRegex(RuntimeError, "required validation targets missing"):
            RUNNER.validate(args)
        self.assertEqual(json.loads((out / "result.json").read_text())["status"], "failed")
        self.assertFalse((out / "COMPLETE").exists())

    def test_validation_rejects_empty_actual_execution_set(self) -> None:
        prep = self.prepared()
        meta = json.loads((prep / "prepared.json").read_text())
        meta["targets"] = ["model"]
        meta["variants"] = []
        RUNNER.write(prep / "prepared.json", meta)
        out = self.root / "validation"
        args = argparse.Namespace(prepared=prep, out=out, operations=10000, seeds=8, targets="model")
        with self.assertRaisesRegex(RuntimeError, "empty validation selection"):
            RUNNER.validate(args)
        self.assertEqual(json.loads((out / "result.json").read_text())["status"], "failed")
        self.assertFalse((out / "COMPLETE").exists())

    def test_subprocess_wrapper_has_timeout_and_failure_check(self) -> None:
        with mock.patch.object(RUNNER.subprocess, "run", return_value=argparse.Namespace()) as process:
            RUNNER.run(["inert-test"], timeout=17)
        self.assertEqual(process.call_args.kwargs["timeout"], 17)
        self.assertTrue(process.call_args.kwargs["check"])
        self.assertTrue(process.call_args.kwargs["capture_output"])

    def validation_evidence(self) -> Path:
        prep = self.prepared()
        meta = json.loads((prep / "prepared.json").read_text())
        meta["targets"] = ["model", "failures", "policies"]
        for variant in meta["variants"]:
            for target in meta["targets"]:
                binary = prep / variant / target
                binary.write_text("inert " + variant + " " + target)
                meta["files"][f"{variant}/{target}"] = RUNNER.sha(binary)
        RUNNER.write(prep / "prepared.json", meta)
        out = self.root / "validation"
        out.mkdir()
        commands = []
        for variant in meta["variants"]:
            for target in meta["targets"]:
                log = f"{variant}-{target}.log"
                (out / log).write_text('{"status":"passed"}\n')
                argv = [str(prep / variant / target)]
                if target == "model":
                    argv += ["10000", "8"]
                commands.append({"variant": variant, "target": target, "argv": argv,
                                 "returncode": 0, "log": log, "log_sha256": RUNNER.sha(out / log)})
        state = {"status": "complete", "prepared": str(prep),
                 "prepared_sha256": RUNNER.sha(prep / "prepared.json"),
                 "required_targets": meta["targets"], "operations": 10000, "seeds": 8,
                 "actual_targets": [[c["variant"], c["target"]] for c in commands], "commands": commands}
        self.seal_validation(out, state)
        return out

    def seal_validation(self, out: Path, state: dict) -> None:
        RUNNER.write(out / "result.json", state)
        (out / "COMPLETE").write_text(RUNNER.sha(out / "result.json") + "\n")

    def test_complete_validation_inventory_audits(self) -> None:
        result = RUNNER.audit_validation(self.validation_evidence())
        self.assertEqual(result, {"status": "audited", "commands": 6})

    def test_validation_missing_per_variant_mandatory_command_rejected(self) -> None:
        out = self.validation_evidence()
        state = json.loads((out / "result.json").read_text())
        state["commands"].pop(0)
        state["actual_targets"].pop(0)
        self.seal_validation(out, state)
        with self.assertRaises(RuntimeError):
            RUNNER.audit_validation(out)

    def test_validation_inventory_must_match_command_log(self) -> None:
        out = self.validation_evidence()
        state = json.loads((out / "result.json").read_text())
        state["commands"].pop()
        self.seal_validation(out, state)
        with self.assertRaises(RuntimeError):
            RUNNER.audit_validation(out)

    def test_duplicate_validation_command_rejected(self) -> None:
        out = self.validation_evidence()
        state = json.loads((out / "result.json").read_text())
        state["commands"].append(copy.deepcopy(state["commands"][0]))
        self.seal_validation(out, state)
        with self.assertRaises(RuntimeError):
            RUNNER.audit_validation(out)

    def test_validation_changed_log_rejected(self) -> None:
        out = self.validation_evidence()
        (out / "F-model.log").write_text("mutated model output")
        with self.assertRaises(RuntimeError):
            RUNNER.audit_validation(out)

    def test_validation_wrong_argv_rejected(self) -> None:
        out = self.validation_evidence()
        state = json.loads((out / "result.json").read_text())
        state["commands"][0]["argv"][-1] = "1"
        self.seal_validation(out, state)
        with self.assertRaises(RuntimeError):
            RUNNER.audit_validation(out)

    def test_validation_nonzero_returncode_rejected(self) -> None:
        out = self.validation_evidence()
        state = json.loads((out / "result.json").read_text())
        state["commands"][0]["returncode"] = 1
        self.seal_validation(out, state)
        with self.assertRaises(RuntimeError):
            RUNNER.audit_validation(out)

    def test_optimized_python_validation_audit_rejects_missing_inventory(self) -> None:
        out = self.validation_evidence()
        valid = subprocess.run([sys.executable, "-O", str(SCRIPT), "audit-validation", str(out)],
                               capture_output=True, text=True, timeout=30)
        self.assertEqual(valid.returncode, 0, valid.stderr)
        state = json.loads((out / "result.json").read_text())
        state["commands"] = []
        self.seal_validation(out, state)
        invalid = subprocess.run([sys.executable, "-O", str(SCRIPT), "audit-validation", str(out)],
                                 capture_output=True, text=True, timeout=30)
        self.assertNotEqual(invalid.returncode, 0)

    def test_optimized_python_accepts_valid_and_rejects_semantic_corruption(self) -> None:
        out = self.evidence()
        valid = subprocess.run([sys.executable, "-O", str(SCRIPT), "audit", str(out)],
                               capture_output=True, text=True, timeout=30)
        self.assertEqual(valid.returncode, 0, valid.stderr)
        rows = [json.loads(line) for line in (out / "raw.jsonl").read_text().splitlines()]
        rows[-1]["sample"]["semantic_checksum"] = 999
        self.seal(out, rows=rows)
        invalid = subprocess.run([sys.executable, "-O", str(SCRIPT), "audit", str(out)],
                                 capture_output=True, text=True, timeout=30)
        self.assertNotEqual(invalid.returncode, 0)


if __name__ == "__main__":
    unittest.main()
