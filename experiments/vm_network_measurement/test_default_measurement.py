#!/usr/bin/env python3
"""Lightweight synthetic/offline bridge tests; never native timing evidence."""
import copy
import gzip
from pathlib import Path
import shutil
import subprocess
import struct
import sys
import tempfile
import unittest
from unittest import mock

sys.dont_write_bytecode = True
import default_measurement as D
import paired_measurement as P

ROOT = Path(__file__).resolve().parents[2]


def inherited_rows():
    # Existing reports are parser fixtures only, never new performance evidence.
    result = {}
    with gzip.open(ROOT / "docs/perf/2026-10-15-vm-stage1/window_01/raw.jsonl.gz", "rt") as stream:
        for line in stream:
            row = P.parse(line)
            if row["case_id"] not in result:
                native = {k: v for k, v in row.items() if k in D.RAW_FIELDS}
                # Historical runner overwrote this serializer field with its
                # D/F variant label; reconstruct the frozen serializer value.
                native["variant"] = "stable_vm"
                result[row["case_id"]] = native
    return result


class DefaultTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = inherited_rows()
        cls.meta = P.read(ROOT / "build/vm-network-measurement/default-source-v1/historical-metadata.json")

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name)

    def report(self, case=0):
        return copy.deepcopy(self.rows[D.CASES[case]["id"]])

    def test_exact_historical_native_reports_map_all_selected_metrics(self):
        for case in D.CASES:
            row = self.rows[case["id"]]
            normalized = D.native_report(P.canonical(row), case)
            self.assertEqual(normalized["metrics_ms"], {k: row[k] for k in case["metrics"]})
            self.assertNotIn("primary_total_ns", normalized)

    def test_native_duplicate_nonfinite_extra_and_trailing_output_rejected(self):
        raw = P.canonical(self.report())
        for value in (raw[:-2] + b',"size":67108864}\n', raw + b'noise', raw.replace(b'"first_ms": ', b'"first_ms": NaN,"x": ')):
            with self.subTest(value=value[-60:]), self.assertRaises((ValueError, TypeError)):
                D.native_report(value, D.CASES[0])
        row = self.report()
        row["extra"] = 1
        with self.assertRaises(P.ProtocolError):
            D.native_report(P.canonical(row), D.CASES[0])

    def test_native_bool_counters_bad_capacity_memory_or_samples_rejected(self):
        for key, value in (("size", True), ("initial_reserved_bytes", 33554432),
                           ("address_changes", 1), ("operation_samples_ms", []), ("first_ms", 0),
                           ("timing_scope", "other"), ("threads", 2), ("wire_checksum", -1),
                           ("wire_checksum", 2**64), ("wire_size", 4), ("used_end", 0),
                           ("final_logical_bytes", 0), ("semantic_checksum", 0)):
            row = self.report()
            row[key] = value
            with self.subTest(key=key), self.assertRaises(P.ProtocolError):
                D.native_report(P.canonical(row), D.CASES[0])
        row = self.report()
        row["memory_live"]["available"] = False
        with self.assertRaises(P.ProtocolError):
            D.native_report(P.canonical(row), D.CASES[0])

    def test_repeated_mapping_cannot_drop_first_or_change_denominator(self):
        for key in ("first_ms", "repeated_ms", "repeated_mean_ms", "elapsed_ms"):
            row = self.report(1)
            row[key] *= 2
            with self.subTest(key=key), self.assertRaises(P.ProtocolError):
                D.native_report(P.canonical(row), D.CASES[1])

    def test_compile_flags_unchanged_except_three_relocations(self):
        for variant in ("D", "F"):
            old = self.meta["compile_commands"][variant]
            new = D.compile_command(self.meta, variant, self.path / variant, self.path / "bin" / variant)
            self.assertEqual(sum(a != b for a, b in zip(old, new)), 3)
            self.assertEqual(new[0], "/usr/local/bin/clang++")
            self.assertIn("-freflection", new)
            self.assertIn("-arch", new)
        bad = copy.deepcopy(self.meta)
        bad["compile_commands"]["D"].extend(["-I", "other"])
        with self.assertRaises(P.ProtocolError):
            D.compile_command(bad, "D", self.path, self.path / "D")

    def test_schedule_exact_budget_balanced_abba_and_cases(self):
        jobs = D.calibration_schedule()
        self.assertEqual(len(jobs), 78)
        self.assertEqual(len({j["id"] for j in jobs}), 78)
        self.assertEqual(sum(j["phase"] == "warmup" for j in jobs), 6)
        self.assertEqual({j["variant"] for j in jobs}, {"F"})
        for anchor in D.POLICY["anchors"]:
            for case in D.CASES:
                group = [j for j in jobs if j["anchor"] == anchor and j["case_id"] == case["id"]]
                self.assertEqual("".join(j["side"] for j in group), "ABBAABBA")
        self.assertEqual([j["variant"] for j in D.functional_schedule()], ["D", "F"] * 3)

    def spec(self):
        return dict(schema_version=1, kind="default-calibration-preregistration", status="proposed_for_parent_review",
                    purpose="calibration_only", evidence_class="native", timing_authorized=False,
                    window_id="default-aa-1", calibration_output=str(self.path / "calibration"),
                    cases=list(D.CASES), policy=copy.deepcopy(D.POLICY), schedule=D.calibration_schedule(),
                    bridge_sha256="1"*64, contract_sha256="2"*64, paired_primitives_sha256="3"*64,
                    source_prepare_sha256="4"*64, functional_result_sha256="5"*64, build_result_sha256="6"*64,
                    adoption_allowed=False)

    def test_prereg_rejects_boolean_policy_tampering_unbalanced_order_extra_job(self):
        spec = self.spec()
        D.validate_prereg(spec)
        for mutate in (lambda s: s["policy"].update(child_timeout_seconds=True),
                       lambda s: s["schedule"][0].update(position=True),
                       lambda s: s["schedule"].append(s["schedule"][0]),
                       lambda s: s.update(timing_authorized=True),
                       lambda s: s.update(purpose="comparison")):
            changed = copy.deepcopy(spec)
            mutate(changed)
            with self.assertRaises(P.ProtocolError):
                D.validate_prereg(changed)

    def test_approval_external_pin_gate_runs_before_queue_or_native_mutation(self):
        spec = self.spec()
        prereg = self.path / "prereg.json"
        approval = self.path / "approval.json"
        P.write_new(prereg, spec)
        P.write_new(approval, {})
        with mock.patch.object(D, "active_queue") as queue, mock.patch.object(D, "execute") as execute:
            with self.assertRaises(P.ProtocolError):
                D.calibrate(self.path / "functional", prereg, approval, "f"*64, self.path / "out", self.path / "out/queue")
            queue.assert_not_called()
            execute.assert_not_called()
        self.assertFalse((self.path / "out").exists())

    def test_parent_approval_binds_all_immutable_result_and_protocol_hashes(self):
        spec = self.spec()
        prereg = self.path / "prereg.json"
        P.write_new(prereg, spec)
        approval = dict(schema_version=1, kind="vm-default-calibration-approval", status="accepted", parent_chat=D.PARENT,
                        approved_action="calibration_only", approval_id="parent-review-1", preregister_sha256=P.sha(prereg),
                        **{k: spec[k] for k in ("bridge_sha256", "contract_sha256", "functional_result_sha256", "build_result_sha256")})
        D.approval_check(approval, spec, prereg)
        for key in ("parent_chat", "approved_action", "preregister_sha256", "bridge_sha256", "contract_sha256", "functional_result_sha256", "build_result_sha256"):
            changed = dict(approval, **{key: "wrong"})
            with self.subTest(key=key), self.assertRaises(P.ProtocolError):
                D.approval_check(changed, spec, prereg)

    def test_failed_process_keeps_exact_raw_bytes_and_unsuccessful_receipt(self):
        def failed(argv, **kwargs):
            kwargs["stdout"].write(b'partial\xffstdout')
            kwargs["stderr"].write(b'error\x00')
            return subprocess.CompletedProcess(argv, 9)
        with mock.patch.object(D.subprocess, "run", side_effect=failed), self.assertRaises(P.ProtocolError):
            D.execute(["synthetic-not-executed"], self.path / "child", 1)
        self.assertEqual((self.path / "child.stdout").read_bytes(), b'partial\xffstdout')
        self.assertEqual((self.path / "child.stderr").read_bytes(), b'error\x00')
        receipt = P.read(self.path / "child.receipt.json")
        self.assertEqual(receipt["returncode"], 9)
        self.assertFalse(receipt["timed_out"])
        with self.assertRaises(FileExistsError):
            D.execute(["synthetic-not-executed"], self.path / "child", 1)

    def test_timeout_keeps_partial_bytes_and_timeout_state(self):
        def timeout(argv, **kwargs):
            kwargs["stdout"].write(b'{partial')
            raise subprocess.TimeoutExpired(argv, 1)
        with mock.patch.object(D.subprocess, "run", side_effect=timeout), self.assertRaises(P.ProtocolError):
            D.execute(["synthetic-not-executed"], self.path / "timeout", 1)
        self.assertEqual((self.path / "timeout.stdout").read_bytes(), b'{partial')
        receipt = P.read(self.path / "timeout.receipt.json")
        self.assertTrue(receipt["timed_out"])
        self.assertIsNone(receipt["returncode"])

    def synthetic_reports(self, ratios=None):
        jobs = D.calibration_schedule()
        reports = {}
        for job in jobs:
            case = next(c for c in D.CASES if c["id"] == job["case_id"])
            value = 1 if ratios is None or job["side"] == "B" else ratios[job["pair"]]
            reports[job["id"]] = dict(metrics_ms={m: value for m in case["metrics"]})
        return jobs, reports

    def test_stats_clean_synthetic_qualified_without_adoption(self):
        jobs, reports = self.synthetic_reports()
        result = D.summarize(jobs, reports)
        self.assertTrue(result["all_aa_qualified"])
        self.assertFalse(result["adoption_allowed"])
        self.assertEqual(len(result["rows"]), 4)

    def test_symmetric_noise_and_order_bias_cannot_hide_behind_median(self):
        for ratios in ([.8, 1.2, .8, 1.2], [1.03, .97, 1.03, .97], [1, 1, 1.1, .9]):
            jobs, reports = self.synthetic_reports(ratios)
            with self.subTest(ratios=ratios):
                self.assertFalse(D.summarize(jobs, reports)["all_aa_qualified"])

    def test_mid_or_post_only_noise_invalidates_calibration(self):
        jobs, reports = self.synthetic_reports()
        for job in jobs:
            if job["anchor"] == "post" and job["side"] == "A":
                reports[job["id"]]["metrics_ms"] = {k: 2 for k in reports[job["id"]]["metrics_ms"]}
        self.assertFalse(D.summarize(jobs, reports)["all_aa_qualified"])
        with self.assertRaises(P.ProtocolError):
            D.summarize(jobs[:-1], reports)

    def test_synchronous_both_side_drift_rejected_when_all_local_ratios_equal_one(self):
        jobs, reports = self.synthetic_reports()
        for job in jobs:
            if job["phase"] == "measured":
                scale = {"pre": 1, "mid": 1.1, "post": 1.2}[job["anchor"]]
                reports[job["id"]]["metrics_ms"] = {k: scale for k in reports[job["id"]]["metrics_ms"]}
        summary = D.summarize(jobs, reports)
        self.assertFalse(summary["all_aa_qualified"])
        for row in summary["rows"]:
            self.assertTrue(all(a["qualified"] for a in row["anchors"].values()))
            self.assertFalse(row["drift_qualified"])
            self.assertEqual(row["pooled_anchor_max_over_min"], 1.2)
            self.assertEqual(row["side_anchor_max_over_min"], {"A": 1.2, "B": 1.2})

    def test_failed_bundle_portable_readonly_and_tamper_rejected(self):
        out = self.path / "failed"
        out.mkdir()
        (out / "partial.stdout").write_bytes(b'bad\xff')
        D.seal(out, "default-functional", "failed", error="synthetic test", performance_evidence=False)
        moved = self.path / "moved"
        shutil.copytree(out, moved)
        shutil.rmtree(out)
        before = P.inventory(moved)
        self.assertEqual(D.audit(moved)["status"], "failed_preserved")
        self.assertEqual(P.inventory(moved), before)
        (moved / "partial.stdout").write_bytes(b'changed')
        with self.assertRaises(P.ProtocolError):
            D.audit(moved)

    def test_include_byte_change_updates_identity_and_symlink_directory_rejected(self):
        include = self.path / "include"
        include.mkdir()
        header = include / "a.hpp"
        header.write_bytes(b'a')
        first = D.tree_identity(include)
        header.write_bytes(b'b')
        self.assertNotEqual(D.tree_identity(include)["manifest_sha256"], first["manifest_sha256"])
        (include / "link").symlink_to(include, target_is_directory=True)
        with self.assertRaises(P.ProtocolError):
            D.tree_identity(include)

    def test_read_samples_portable_raw_mapping_and_wire_hash_tamper(self):
        # Fabricated V1-shaped 4MiB fixture tests plumbing, never a real graph
        # or native performance claim. Full graph checks belong to C++/queue.
        out = self.path / "synthetic"
        (out / "commands").mkdir(parents=True)
        (out / "queue").mkdir()
        P.write_new(out / "queue/request.json", dict(out="/old/queue"))
        build = out / "build"
        (build / "bin").mkdir(parents=True)
        (build / "bin/F").write_bytes(b'synthetic-no-execution')
        case = D.CASES[2]
        row = self.report(2)
        job = dict(id="j000", case_id=case["id"], variant="F")
        prefix = out / "commands/j000"
        wire = struct.pack("<8sHHIQQQQQQQIIII", b"XOFFV1\0\0", 1, 0, 88,
                           row["used_end"], row["used_end"], 64, 128, 1, 2, 0, 0, 0x01020304, 0, 0)
        prefix.with_suffix(".wire").write_bytes(wire + bytes(row["used_end"]))
        row["wire_checksum"] = D.fnv_wire(prefix.with_suffix(".wire"))
        prefix.with_suffix(".stdout").write_bytes(P.canonical(row))
        prefix.with_suffix(".stderr").write_bytes(b'')
        normalized = D.native_report(P.canonical(row), case)
        normalized.update(wire_sha256=P.sha(prefix.with_suffix(".wire")), binary_sha256=P.sha(build / "bin/F"))
        normalized["wire_header"] = D.validate_wire(prefix.with_suffix(".wire"), normalized, {}, True)
        P.write_new(prefix.with_suffix(".normalized.json"), normalized)
        receipt = dict(argv=D.native_argv(Path('/old/build/bin/F'), case, Path('/old/commands/j000.wire')),
            returncode=0, timed_out=False, timeout_seconds=30, started_monotonic_ns=100,
            finished_monotonic_ns=100+10**9)
        P.write_new(prefix.with_suffix(".receipt.json"), receipt)
        self.assertEqual(D.read_samples(out, [job], build)["j000"], normalized)
        bad = copy.deepcopy(receipt)
        bad["argv"][0] = "/unrelated/build/bin/F"
        prefix.with_suffix(".receipt.json").write_bytes(P.canonical(bad))
        with self.assertRaises(P.ProtocolError):
            D.read_samples(out, [job], build)
        prefix.with_suffix(".receipt.json").write_bytes(P.canonical(receipt))
        canonical = {(case["id"], "F"): D.wire_identity(normalized)}
        D.read_samples(out, [job], build, canonical)
        canonical[(case["id"], "F")]["wire_sha256"] = "f"*64
        with self.assertRaises(P.ProtocolError):
            D.read_samples(out, [job], build, canonical)
        prefix.with_suffix(".wire").write_bytes(b'evil')
        with self.assertRaises(P.ProtocolError):
            D.read_samples(out, [job], build)

    def test_failed_prerequisites_rejected_before_queue_native_or_new_output(self):
        for kind, action in (("default-frozen", "build"), ("default-build", "functional"), ("default-functional", "preregister")):
            failed = self.path / kind
            failed.mkdir()
            (failed / "tempting-partial-source-or-binary").write_bytes(b'partial')
            D.seal(failed, kind, "failed", error="synthetic", performance_evidence=False)
            with mock.patch.object(D, "active_queue") as queue, mock.patch.object(D, "execute") as execute:
                with self.assertRaises(P.ProtocolError):
                    if action == "build":
                        D.build(failed, self.path / "out", self.path / "out/queue")
                    elif action == "functional":
                        D.functional(failed, self.path / "out", self.path / "out/queue")
                    else:
                        D.preregister(failed, self.path / "prereg.json", "one", self.path / "out")
                queue.assert_not_called()
                execute.assert_not_called()
            self.assertFalse((self.path / "out").exists())
            self.assertFalse((self.path / "prereg.json").exists())

    def test_environment_invalid_utf8_and_unavailable_probes_are_explicit(self):
        with mock.patch.object(D.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, b'process\xff', b'')), \
             mock.patch.object(D.os, "getloadavg", side_effect=OSError("unavailable")):
            result = D.environment(self.path)
        self.assertEqual(result["load_average"]["status"], "unavailable")
        self.assertIn("\ufffd", result["process_snapshot"]["stdout"])
        self.assertEqual(result["client_timezone"], "Asia/Shanghai")

    def test_tiny_wire_wrong_header_and_checksum_mismatch_rejected(self):
        path = self.path / "wire"
        path.write_bytes(b'wire')
        report = D.native_report(P.canonical(self.report()), D.CASES[0])
        report["wire_sha256"] = P.sha(path)
        with self.assertRaises(P.ProtocolError):
            D.validate_wire(path, report, {}, True)

    def test_successful_unrelated_or_missing_queue_command_rejected(self):
        for command in (None, ["python3", "/other/script.py", "functional"]):
            queue = self.path / ("queue-none" if command is None else "queue-other")
            queue.mkdir()
            request = dict(schema_version=1, runner_sha256=D.QUEUE_SHA, owner="W3", kind="functional", run_id="synthetic",
                           lock_dir=D.ACCEPTED_LOCK_DIR, out="/old/queue", cwd="/old", timeout_seconds=300)
            if command:
                request["command"] = command
            result = dict(schema_version=1, owner="W3", kind="functional", run_id="synthetic", status="complete",
                          command_returncode=0, started_at="synthetic", finished_at="synthetic", cleanup=dict(live=[]))
            P.write_new(queue / "request.json", request)
            P.write_new(queue / "result.json", result)
            P.write_new(queue / "manifest.json", dict(schema_version=1, files=[dict(path=n, **v) for n, v in P.inventory(queue).items()]))
            with self.assertRaises(P.ProtocolError):
                D.queue_audit(queue, "functional")

    def test_queue_unsuccessful_receipt_cannot_qualify_native(self):
        queue = self.path / "queue"
        queue.mkdir()
        request = dict(runner_sha256=D.QUEUE_SHA, owner="W3", kind="timing", run_id="synthetic", lock_dir=D.ACCEPTED_LOCK_DIR)
        result = dict(run_id="synthetic", status="failed", command_returncode=1, cleanup=dict(live=[]))
        P.write_new(queue / "request.json", request)
        P.write_new(queue / "result.json", result)
        files = [dict(path=name, **identity) for name, identity in P.inventory(queue).items()]
        P.write_new(queue / "manifest.json", dict(schema_version=1, files=files))
        with self.assertRaises(P.ProtocolError):
            D.queue_audit(queue, "timing")

    def test_queue_active_context_pins_full_request_against_manifest_resealing(self):
        queue = self.path / "queue"
        queue.mkdir()
        argv = ["/recorded/python", "/recorded/default_measurement.py", "functional", "--build", "/old/build", "--out", "/old", "--queue", "/old/queue"]
        request = dict(schema_version=1, runner_sha256=D.QUEUE_SHA, owner="W3", kind="functional", run_id="synthetic",
                       lock_dir=D.ACCEPTED_LOCK_DIR, out="/old/queue", cwd="/old", timeout_seconds=300, command=argv)
        result = dict(schema_version=1, owner="W3", kind="functional", run_id="synthetic", status="complete",
                      command_returncode=0, started_at="synthetic", finished_at="synthetic", cleanup=dict(live=[]))
        P.write_new(queue / "request.json", request)
        P.write_new(queue / "result.json", result)
        P.write_new(self.path / "execution-context.json", dict(queue_run_id="synthetic", command=argv,
                    queue_request_sha256=P.sha(queue / "request.json"), bridge_sha256=P.sha(D.__file__),
                    requested_interpreter=argv[0], interpreter=dict(sha256="1"*64), bridge_path=argv[1]))
        def manifest():
            return dict(schema_version=1, files=[dict(path=n, **v) for n, v in P.inventory(queue, ("manifest.json",)).items()])
        P.write_new(queue / "manifest.json", manifest())
        D.queue_audit(queue, "functional")
        request["command"][0] = "/unrelated/python"
        (queue / "request.json").write_bytes(P.canonical(request))
        (queue / "manifest.json").write_bytes(P.canonical(manifest()))
        with self.assertRaises(P.ProtocolError):
            D.queue_audit(queue, "functional")


if __name__ == "__main__":
    unittest.main()
