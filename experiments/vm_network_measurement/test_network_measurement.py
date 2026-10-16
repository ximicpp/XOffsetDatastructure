#!/usr/bin/env python3
"""Synthetic raw-fixture validation only; no native workloads or processes."""
from __future__ import annotations

import copy
import datetime as dt
import hashlib
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
import shutil
from types import SimpleNamespace

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import network_measurement as N
P = N.P


def digest(data):
    return hashlib.sha256(data).hexdigest()


def put(value, keys, replacement):
    for key in keys[:-1]:
        value = value[key]
    value[keys[-1]] = replacement


def snapshot(root):
    return {p.relative_to(root).as_posix(): ("directory",) if p.is_dir() else (p.read_bytes(), p.stat().st_mtime_ns)
            for p in Path(root).rglob("*")}


class NetworkAdapterTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="vm-network-adapter-synthetic-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.input = self.root / "input.wire"
        self.input.write_bytes(b"synthetic-input-" + b"a" * 128)
        self.output_bytes = b"synthetic-output-" + b"b" * 256
        self.spec = N.preregistration()
        self.spec.update(window_id="synthetic-raw-fixture", evidence_class="synthetic-framework-test", ready=True, purpose="screening",
                         collector_runtime=N.collector_runtime_identity())
        self.spec["workloads"] = [dict(id="tiny", nominal_bytes=32, elements=1, iterations=2)]
        self.spec["comparisons"] = [dict(id="none-cold", left="F-none", right="F-cold", calibration="F-none")]
        self.spec["controls"].update(max_wire_bytes=4096, owner_capacity_bytes=4096, max_inflight_bytes=16384,
                                    pool_idle_reserved_bytes=2 * (4096 + N.CHUNK))
        self.spec["expected_data"] = {"tiny": dict(input_sha256=P.sha(self.input), output_sha256=digest(self.output_bytes),
                                                  input_bytes=self.input.stat().st_size, output_bytes=len(self.output_bytes),
                                                  semantic_checksum="123", elements=1,
                                                  schema_id=self.spec["controls"]["schema_id"], processing_id=self.spec["controls"]["processing_id"])}
        self.spec["producer_identity"] = {key: digest(("synthetic-" + key).encode()) for key in N.PRODUCER_KEYS}
        self.jobs = P.schedule(self.spec)
        self.spec.update(execution_root="/recorded/window", build_execution_root="/recorded/build",
                         queue_receipt_out="/recorded/queue-receipt",
                         retained_output_bytes_expected=sum(j["iterations"] * len(self.output_bytes) for j in self.jobs))
        self.spec["paths"] = dict(binary="/recorded/build/net_reference",
                                  inputs={"tiny": "/recorded/window/data/" + P.sha(self.input)},
                                  outputs={j["id"]: "/recorded/window/commands/" + j["id"] + "/outputs" for j in self.jobs})
        self.spec["invocations"] = {j["id"]: N.argv(self.spec, j) for j in self.jobs}
        self.job = self.jobs[0]
        self.outputs = self.root / "outputs"
        self.outputs.mkdir()
        for index in range(2):
            (self.outputs / ("output-%d.wire" % index)).write_bytes(self.output_bytes)
        self.stderr = b"synthetic stderr \x00\xff"

    def raw(self, job=None):
        job = job or self.job
        pool = self.spec["conditions"][job["condition"]]["pool"]
        identity, controls = self.spec["expected_data"]["tiny"], self.spec["controls"]
        samples = [dict(end_to_end_ns=100, client_owner_release_outside_ns=2, server_completion_wait_outside_ns=3,
                        input_bytes=identity["input_bytes"], output_bytes=identity["output_bytes"],
                        **{field: None for field in N.PHASE_FIELDS}) for _ in range(job["iterations"])]
        n = len(samples)
        return dict(schema_version=1, contract_version="1.0.0", status="passed", command="run", diagnostic=False, timing_evidence=False,
                    pool=pool, iterations=n, element_count=1, semantic_checksum="123", input_bytes_per_iteration=identity["input_bytes"],
                    output_bytes_per_iteration=identity["output_bytes"], max_wire_bytes=controls["max_wire_bytes"],
                    owner_capacity_bytes=controls["owner_capacity_bytes"], max_inflight_bytes=controls["max_inflight_bytes"],
                    reserved_inflight_peak_bytes=4 * controls["max_wire_bytes"], connection_count=1, max_inflight_requests=1,
                    timeout_ms=self.spec["timeout_ms"], end_to_end_ns=100 * n, pool_prepare_outside_ns=13,
                    connection_setup_outside_ns=17, warmup_outside_ns=11 if pool == "warm" else 0,
                    cold_clear_outside_ns=7 if pool == "cold" else 0, final_clear_outside_ns=5,
                    client_owner_release_outside_ns=2 * n, server_completion_wait_outside_ns=3 * n,
                    pool_measured_hits=2 * n if pool == "warm" else 0, pool_measured_misses=2 * n if pool == "cold" else 0,
                    pool_warmup_hits=0, pool_warmup_misses=2 if pool == "warm" else 0,
                    pool_cached_blocks_before_final_clear=0 if pool == "none" else 2,
                    pool_cached_reserved_bytes_before_final_clear=0 if pool == "none" else 8192,
                    pool_cached_accessible_bytes_before_final_clear=0 if pool == "none" else 4096,
                    samples=samples, output_files=["output-%d.wire" % i for i in range(n)])

    def receipt(self, stdout, job=None):
        job = job or self.job
        return dict(job=job, argv=N.argv(self.spec, job), producer_identity=copy.deepcopy(self.spec["producer_identity"]),
                    returncode=0, timed_out=False, stdout_sha256=digest(stdout), stdout_bytes=len(stdout),
                    stderr_sha256=digest(self.stderr), stderr_bytes=len(self.stderr),
                    started_monotonic_ns=1_000_000, finished_monotonic_ns=2_000_000)

    def normalize(self, raw=None, receipt=None, job=None, stdout=None):
        job = job or self.job
        stdout = stdout if stdout is not None else P.canonical(raw if raw is not None else self.raw(job))
        receipt = receipt if receipt is not None else self.receipt(stdout, job)
        return N.normalize(self.spec, job, stdout, self.stderr, receipt, self.input, self.outputs)

    def queue(self):
        root = self.root / "synthetic-queue"
        root.mkdir()
        gate = dict(window_command=["synthetic-window-collector", "no-process-executed"],
                    queue_lock_dir=N.PRODUCTION_QUEUE, queue_timeout_seconds=900, queue_receipt_out=self.spec["queue_receipt_out"])
        request = dict(schema_version=1, run_id="synthetic-queue-id", owner="W3", kind="timing", runner_sha256=N.QUEUE_SHA,
                       source_headers=P.BASELINE, command=gate["window_command"], lock_dir=gate["queue_lock_dir"],
                       out=gate["queue_receipt_out"], timeout_seconds=900)
        result = dict(schema_version=1, run_id="synthetic-queue-id", owner="W3", kind="timing", status="complete", command_pid=1001,
                      command_returncode=0, cleanup=dict(live=[]), command_elapsed_seconds=.1,
                      started_at="2026-10-15T12:00:00+00:00", finished_at="2026-10-15T12:00:01+00:00")
        P.write_new(root / "request.json", request)
        P.write_new(root / "result.json", result)
        (root / "stdout.log").write_bytes(b"synthetic queue stdout")
        (root / "stderr.log").write_bytes(b"")
        self.reseal_queue(root)
        return root, gate

    def reseal_queue(self, root):
        files = P.inventory(root, ("manifest.json",))
        (root / "manifest.json").write_bytes(P.canonical(dict(schema_version=1, files=[dict(path=k, **v) for k, v in files.items()])))

    def collection_fixture(self):
        """Synthetic collector unit fixture with the parent gate mocked only.

        Actual native gates require real accepted W1 artifacts and remain
        unavailable. The synthetic evidence_class survives through summarize.
        """
        root = self.root / "synthetic-window"
        root.mkdir()
        P.write_new(root / "preregister.json", self.spec)
        P.write_new(root / "schedule.json", self.jobs)
        (root / "data").mkdir()
        (root / "data" / P.sha(self.input)).write_bytes(self.input.read_bytes())
        (root / "data" / digest(self.output_bytes)).write_bytes(self.output_bytes)
        queue_root, gate = self.queue()
        shutil.copytree(queue_root, root / "queue")
        (root / "commands").mkdir()
        for index, job in enumerate(self.jobs):
            directory = root / "commands" / job["id"]
            directory.mkdir()
            (directory / "outputs").mkdir()
            for iteration in range(job["iterations"]):
                (directory / "outputs" / ("output-%d.wire" % iteration)).write_bytes(self.output_bytes)
            stdout = P.canonical(self.raw(job))
            receipt = self.receipt(stdout, job)
            receipt.update(started_monotonic_ns=1_000_000 + index * 2_000_000,
                           finished_monotonic_ns=2_000_000 + index * 2_000_000,
                           started_at=(dt.datetime(2026, 10, 15, 12, tzinfo=dt.timezone.utc) + dt.timedelta(milliseconds=index)).isoformat(),
                           finished_at=(dt.datetime(2026, 10, 15, 12, tzinfo=dt.timezone.utc) + dt.timedelta(milliseconds=index + 1)).isoformat(),
                           coordination=dict(explicit_flag=True, lock_was_held=True, lock_path=N.PRODUCTION_QUEUE + "/queue.lock",
                                             ancestor_pids=[1001, 1000],
                                             request_sha256=P.sha(root / "queue/request.json"), collector_argv=gate["window_command"],
                                             lock_metadata=dict(status="running", owner="W3", kind="timing", run_id="synthetic-queue-id",
                                                                supervisor_pid=1000, out=gate["queue_receipt_out"], started_at="2026-10-15T12:00:00+00:00")))
            (directory / "stdout").write_bytes(stdout)
            (directory / "stderr").write_bytes(self.stderr)
            P.write_new(directory / "receipt.json", receipt)
            normalized = N.normalize(self.spec, job, stdout, self.stderr, receipt, self.input, directory / "outputs")
            P.write_new(directory / "normalized.json", normalized)
        P.write_new(root / "collection.json", dict(schema_version=1, window_id=self.spec["window_id"],
                                                   approval_sha256="synthetic-mocked-gate", coordination=receipt["coordination"],
                                                   native_result_pending=True, timing_evidence=False))
        P.write_new(root / "collection-finished.json", dict(status="collected", jobs=len(self.jobs), queue_completion_pending=True,
                                                            timing_evidence=False, adoption_allowed=False))
        P.write_new(root / "environment-before.json", self.environment("before"))
        P.write_new(root / "environment-after.json", self.environment("after"))
        return root, gate

    def environment(self, stage):
        empty = dict(base64="", sha256=digest(b""), bytes=0)
        return dict(schema="w1-window-environment", schema_version=1, stage=stage,
                    captured_at="2026-10-15T12:00:00+00:00" if stage == "before" else "2026-10-15T12:00:00.100000+00:00",
                    monotonic_ns=500_000 if stage == "before" else 100_000_000,
                    host=dict(system="synthetic", machine="fixture", cpu_count=1),
                    load_average=dict(status="unavailable", reason="synthetic; no real probes"),
                    probes={name: dict(command=command, status="unavailable", reason="synthetic; not executed",
                                       returncode=None, timed_out=False, raw_complete=True,
                                       stdout=copy.deepcopy(empty), stderr=copy.deepcopy(empty))
                            for name, command in N.ENVIRONMENT_POLICY["commands"].items()})

    def launch_fixture(self):
        root, gate = self.collection_fixture()
        context = P.read(root / "collection.json")["coordination"]
        for path in (root / "commands").iterdir():
            shutil.rmtree(path)
        (root / "collection.json").unlink()
        (root / "collection-finished.json").unlink()
        (root / "environment-before.json").unlink()
        (root / "environment-after.json").unlink()
        self.spec["execution_root"] = str(root)
        self.spec["paths"]["inputs"] = {"tiny": str(root / "data" / P.sha(self.input))}
        self.spec["paths"]["outputs"] = {j["id"]: str(root / "commands" / j["id"] / "outputs") for j in self.jobs}
        self.spec["invocations"] = {j["id"]: N.argv(self.spec, j) for j in self.jobs}
        (root / "preregister.json").write_bytes(P.canonical(self.spec))
        gate["preregister_sha256"] = P.sha(root / "preregister.json")
        return root, gate, context

    def test_unbound_public_preregistration_matches_template_and_is_not_runtime_approval(self):
        public = P.read(Path(__file__).with_name("network-preregister.json"))
        self.assertTrue(N.eq(public, N.preregistration()))
        N.validate_preregistration(public)
        self.assertFalse(public["ready"])
        self.assertEqual(public["purpose"], "calibration")
        self.assertEqual(len(P.schedule(public)), 224)
        self.assertTrue(all(j["condition"] == "F-none" for j in N.schedule(public)))
        self.assertEqual(public["controls"]["max_inflight_bytes"], 4 * public["controls"]["max_wire_bytes"])
        self.assertEqual(public["controls"]["pool_idle_reserved_bytes"], 2 * (public["controls"]["owner_capacity_bytes"] + N.CHUNK))
        self.assertEqual(public["policy"]["absolute_anchor_drift_relative_limit"], .01)
        with self.assertRaises(P.ProtocolError):
            N.validate_preregistration(public, bound=True)

    def test_exact_raw_schema_is_derived_from_accepted_contract(self):
        contract = P.read(Path(__file__).parents[2] / "docs/coordination/vm-followup/contracts/W1-revision2/contract.json")
        self.assertEqual(N.RUN_FIELDS, set(contract["json"]["common"]) | (set(contract["json"]["run"]) - {"exact_arrays"}))
        self.assertEqual(N.SAMPLE_FIELDS, set(contract["json"]["sample"]))
        self.assertEqual(contract["timing"]["unit"], "ns")

    def test_valid_raw_retains_full_per_iteration_identity_but_cannot_claim_native_timing(self):
        report = self.normalize()
        self.assertEqual(report["raw_rtt_samples_ns"], [100, 100])
        self.assertEqual([v["iteration"] for v in report["output_identities"]], [0, 1])
        self.assertTrue(all(v["sha256"] == digest(self.output_bytes) and v["bytes"] == len(self.output_bytes)
                            for v in report["output_identities"]))
        self.assertEqual(report["evidence_class"], "synthetic-framework-test")
        self.assertFalse(report["timing_evidence"])
        self.assertFalse(report["whole_lifecycle_available"])
        self.assertIn("persistent worker creation and final join", report["unmeasured_costs"])
        self.assertFalse(report["overlapping_stage_costs_added"])

    def test_cold_and_warm_pool_contracts_and_exact_reclamation_metric(self):
        for pool in ("none", "cold", "warm"):
            with self.subTest(pool=pool):
                job = dict(self.job, condition="F-" + pool)
                # Warm is a registered condition; a registered job is required.
                if pool == "warm":
                    self.spec["comparisons"][0]["right"] = "F-warm"
                jobs = P.schedule(self.spec)
                self.spec["invocations"] = {j["id"]: N.argv(self.spec, j) for j in jobs}
                job = next(j for j in jobs if j["condition"] == "F-" + pool)
                report = self.normalize(job=job)
                costs = N.costs(report)
                outside = self.raw(job)
                self.assertEqual(costs["raw_rtt_mean_ns"], 100)
                self.assertEqual(costs["reclamation_adjusted_mean_ns"], (200 + sum(outside[k] for k in N.RECLAMATION)) / 2)
                self.assertEqual(costs["declared_disjoint_mean_ns"], (200 + sum(outside[k] for k in N.OUTSIDE)) / 2)
                self.assertEqual(report["outside_ns"]["server_completion_wait_outside_ns"], 6)
                self.assertEqual(report["outside_ns"]["client_owner_release_outside_ns"], 4)

    def test_profile_units_schema_boolean_nonfinite_and_unknown_costs_fail_closed(self):
        changes = [(("schema_version",), True), (("contract_version",), "2.0.0"), (("diagnostic",), True),
                   (("timing_evidence",), True), (("samples", 0, "request_send_ns"), 0),
                   (("samples", 0, "response_send_ns"), 1_000_000), (("samples", 0, "end_to_end_ns"), True),
                   (("samples", 0, "end_to_end_ns"), float("inf")), (("pool_prepare_outside_ns",), True),
                   (("end_to_end_ns",), 201), (("client_owner_release_outside_ns",), 1),
                   (("server_completion_wait_outside_ns",), 7)]
        for keys, value in changes:
            with self.subTest(keys=keys, value=value):
                raw = self.raw()
                put(raw, keys, value)
                with self.assertRaises((P.ProtocolError, ValueError)):
                    self.normalize(raw=raw)
        for name in ("unit", "server_join_outside_ns", "client_string_destruction_outside_ns", "source_sha256"):
            with self.subTest(extra=name):
                raw = self.raw()
                raw[name] = "ms" if name == "unit" else 0
                with self.assertRaises(P.ProtocolError):
                    self.normalize(raw=raw)
        for field in N.OUTSIDE:
            raw = self.raw()
            del raw[field]
            with self.assertRaises(P.ProtocolError):
                self.normalize(raw=raw)

    def test_accepted_budgets_and_control_identity_are_not_relaxed(self):
        for field in ("max_wire_bytes", "owner_capacity_bytes", "max_inflight_bytes", "reserved_inflight_peak_bytes",
                      "connection_count", "max_inflight_requests", "timeout_ms", "iterations", "input_bytes_per_iteration", "output_bytes_per_iteration"):
            with self.subTest(field=field):
                raw = self.raw()
                raw[field] += 1
                with self.assertRaises(P.ProtocolError):
                    self.normalize(raw=raw)
        for keys, replacement in ((("controls", "max_inflight_bytes"), 16383),
                                   (("controls", "pool_idle_reserved_bytes"), 139263),
                                   (("controls", "owner_capacity_bytes"), 4095), (("timing_unit",), "ms"),
                                   (("gate_revision",), True), (("contract_sha256",), "a" * 64),
                                   (("policy", "absolute_anchor_drift_relative_limit"), .05),
                                   (("policy", "absolute_anchor_drift_relative_limit"), True)):
            spec = copy.deepcopy(self.spec)
            put(spec, keys, replacement)
            with self.assertRaises(P.ProtocolError):
                N.validate_preregistration(spec, bound=True)

    def test_explicit_output_budget_and_approved_path_roots_reject_rebinding(self):
        mutations = [(("max_retained_output_bytes",), self.spec["retained_output_bytes_expected"] - 1),
                     (("max_retained_output_bytes",), True),
                     (("retained_output_bytes_expected",), self.spec["retained_output_bytes_expected"] - 1),
                     (("paths", "binary"), "/other/build/net_reference"),
                     (("paths", "inputs", "tiny"), "/other/window/data/" + P.sha(self.input)),
                     (("paths", "outputs", self.job["id"]), "/other/window/commands/" + self.job["id"] + "/outputs"),
                     (("queue_receipt_out",), "/recorded/window/queue")]
        mutations += [(("expected_data", "tiny", "semantic_checksum"), str(2**64)),
                      (("expected_data", "tiny", "input_bytes"), 87),
                      (("expected_data", "tiny", "output_bytes"), self.spec["controls"]["owner_capacity_bytes"] + 89),
                      (("collector_runtime", "schema_version"), True),
                      (("collector_runtime", "interpreter", "sha256"), "invalid")]
        for keys, value in mutations:
            with self.subTest(keys=keys):
                spec = copy.deepcopy(self.spec)
                put(spec, keys, value)
                spec["invocations"] = {j["id"]: N.argv(spec, j) for j in self.jobs}
                with self.assertRaises(P.ProtocolError):
                    N.validate_preregistration(spec, bound=True)

    def test_pool_hits_retention_and_outside_cost_mode_are_verified(self):
        for field, value in (("pool_measured_hits", 1), ("pool_measured_misses", 1), ("pool_warmup_misses", 2),
                             ("pool_cached_blocks_before_final_clear", 1), ("pool_cached_reserved_bytes_before_final_clear", 1),
                             ("cold_clear_outside_ns", 1), ("warmup_outside_ns", 1)):
            with self.subTest(field=field):
                raw = self.raw()
                raw[field] = value
                with self.assertRaises(P.ProtocolError):
                    self.normalize(raw=raw)
        cold = next(j for j in self.jobs if j["condition"] == "F-cold")
        for field, value in (("pool_cached_reserved_bytes_before_final_clear", self.spec["controls"]["pool_idle_reserved_bytes"] + 1),
                             ("pool_cached_accessible_bytes_before_final_clear", 8193), ("pool_cached_blocks_before_final_clear", 1)):
            raw = self.raw(cold)
            raw[field] = value
            with self.assertRaises(P.ProtocolError):
                self.normalize(raw=raw, job=cold)

    def test_every_output_and_input_full_bytes_are_verified_not_reported_lengths(self):
        (self.outputs / "output-1.wire").write_bytes(b"x" * len(self.output_bytes))
        with self.assertRaisesRegex(P.ProtocolError, "per-iteration"):
            self.normalize()
        (self.outputs / "output-1.wire").write_bytes(self.output_bytes)
        self.input.write_bytes(b"x" * self.input.stat().st_size)
        with self.assertRaisesRegex(P.ProtocolError, "full input"):
            self.normalize()

    def test_output_inventory_order_length_symlinks_and_reused_directories_rejected(self):
        for names in (["output-1.wire", "output-0.wire"], ["output-0.wire"], ["output-0.wire", "../output-1.wire"]):
            raw = self.raw()
            raw["output_files"] = names
            with self.assertRaises(P.ProtocolError):
                self.normalize(raw=raw)
        (self.outputs / "extra.wire").write_bytes(b"extra")
        with self.assertRaises(P.ProtocolError):
            self.normalize()
        (self.outputs / "extra.wire").unlink()
        (self.outputs / "output-1.wire").unlink()
        (self.outputs / "output-1.wire").symlink_to(self.outputs / "output-0.wire")
        with self.assertRaises(P.ProtocolError):
            self.normalize()
        spec = copy.deepcopy(self.spec)
        spec["paths"]["outputs"][self.jobs[1]["id"]] = spec["paths"]["outputs"][self.jobs[0]["id"]]
        spec["invocations"] = {j["id"]: N.argv(spec, j) for j in self.jobs}
        with self.assertRaisesRegex(P.ProtocolError, "output reuse"):
            N.validate_preregistration(spec, bound=True)

    def test_stdout_stderr_source_binary_and_cli_receipts_are_independently_bound(self):
        stdout = P.canonical(self.raw())
        original = self.receipt(stdout)
        changes = [(("stdout_sha256",), "a" * 64), (("stdout_bytes",), len(stdout) + 1),
                   (("stderr_sha256",), "a" * 64), (("stderr_bytes",), 0), (("returncode",), False),
                   (("timed_out",), True), (("job", "pair"), True), (("argv",), ["wrong", "run"])]
        changes += [(("producer_identity", key), "a" * 64) for key in N.PRODUCER_KEYS]
        for keys, value in changes:
            receipt = copy.deepcopy(original)
            put(receipt, keys, value)
            with self.subTest(keys=keys):
                with self.assertRaises(P.ProtocolError):
                    self.normalize(stdout=stdout, receipt=receipt)
        for raw_bytes in (stdout + b"progress\n", stdout[:-1], b'{"schema_version":1,' + stdout[1:],
                          b'{"cost":1e999}\n', b'{"bad":"\xff"}\n'):
            with self.assertRaises((P.ProtocolError, UnicodeError)):
                self.normalize(stdout=raw_bytes)

    def test_semantic_data_substitution_and_disjoint_cost_overrun_rejected(self):
        for keys, value in ((("semantic_checksum",), "124"), (("element_count",), 2),
                            (("samples", 1, "output_bytes"), len(self.output_bytes) - 1)):
            raw = self.raw()
            put(raw, keys, value)
            with self.assertRaises(P.ProtocolError):
                self.normalize(raw=raw)
        stdout = P.canonical(self.raw())
        receipt = self.receipt(stdout)
        receipt["finished_monotonic_ns"] = receipt["started_monotonic_ns"] + 200
        with self.assertRaisesRegex(P.ProtocolError, "disjoint costs"):
            self.normalize(stdout=stdout, receipt=receipt)

    def test_specialized_summary_counts_server_residual_wait_in_calibration(self):
        reports = {}
        for job in self.jobs:
            report = self.normalize(job=job)
            if job["phase"] == "calibration" and job["anchor"] == "post" and job["side"] == "right":
                report["outside_ns"]["server_completion_wait_outside_ns"] += 200
            reports[job["id"]] = report
        summary = N.summarize(self.spec, self.jobs, reports)
        self.assertFalse(summary["all_aa_qualified"])
        self.assertFalse(summary["rows"][0]["calibration"]["post"]["qualified"])
        self.assertTrue(summary["rows"][0]["calibration"]["pre"]["qualified"])
        self.assertFalse(summary["adoption_allowed"])
        self.assertFalse(summary["timing_evidence"])
        self.assertFalse(summary["whole_lifecycle_available"])

    def test_synchronous_both_side_absolute_anchor_drift_fails_despite_all_local_aa_passing(self):
        reports = {}
        for job in self.jobs:
            raw = self.raw(job)
            if job["phase"] == "calibration":
                sample_ns = {"pre": 100, "mid": 110, "post": 120}[job["anchor"]]
                raw["end_to_end_ns"] = 2 * sample_ns
                for sample in raw["samples"]:
                    sample["end_to_end_ns"] = sample_ns
            reports[job["id"]] = self.normalize(raw=raw, job=job)
        summary = N.summarize(self.spec, self.jobs, reports)
        row = summary["rows"][0]
        self.assertTrue(summary["all_local_aa_qualified"])
        self.assertTrue(summary["all_aa_qualified"])
        self.assertFalse(summary["all_stable_windows_qualified"])
        self.assertFalse(row["stable_window_qualified"])
        self.assertFalse(row["anchor_drift"]["qualified"])
        self.assertEqual(row["anchor_drift"]["anchor_medians_ns"], {"pre": 107.5, "mid": 117.5, "post": 127.5})
        for anchor in row["calibration"].values():
            self.assertEqual(len(anchor["absolute_adjusted_cost_ns"]["left"]), 4)
            self.assertEqual(len(anchor["absolute_adjusted_cost_ns"]["right"]), 4)
            self.assertEqual(len(anchor["absolute_adjusted_cost_ns"]["pooled"]), 8)
        self.assertFalse(summary["adoption_allowed"])

    def test_cancelling_side_anchor_drift_cannot_hide_behind_constant_pooled_median(self):
        reports = {}
        for job in self.jobs:
            raw = self.raw(job)
            if job["phase"] == "calibration":
                levels = {"pre": {"left": 100, "right": 104}, "mid": {"left": 102, "right": 102}, "post": {"left": 104, "right": 100}}
                value = levels[job["anchor"]][job["side"]]
                raw["end_to_end_ns"] = value * 2
                for sample in raw["samples"]:
                    sample["end_to_end_ns"] = value
            reports[job["id"]] = self.normalize(raw=raw, job=job)
        summary = N.summarize(self.spec, self.jobs, reports)
        drift = summary["rows"][0]["anchor_drift"]
        self.assertTrue(summary["all_local_aa_qualified"])
        self.assertEqual(drift["side_maximum_over_minimum"]["pooled"], 1)
        self.assertGreater(drift["side_maximum_over_minimum"]["left"], 1.01)
        self.assertGreater(drift["side_maximum_over_minimum"]["right"], 1.01)
        self.assertFalse(summary["all_stable_windows_qualified"])

    def test_synthetic_data_cannot_be_native_gate_or_native_seal(self):
        P.write_new(self.root / "preregister.json", self.spec)
        before = snapshot(self.root)
        for function in (N.collect_native, N.seal_native):
            with self.assertRaisesRegex(P.ProtocolError, "synthetic/unbound"):
                function(self.root, "a" * 64)
        self.assertEqual(snapshot(self.root), before)

    def test_parent_approval_requires_external_pin_and_actual_acceptance(self):
        spec = copy.deepcopy(self.spec)
        spec["evidence_class"] = "native"
        P.write_new(self.root / "preregister.json", spec)
        approval = copy.deepcopy(N.APPROVAL_SCHEMA)
        approval.update(accepted_inputs={}, preregister_sha256=P.sha(self.root / "preregister.json"),
                        window_id=spec["window_id"], execution_root=spec["execution_root"], queue_receipt_out=spec["queue_receipt_out"])
        P.write_new(self.root / "approval.json", approval)
        before = snapshot(self.root)
        with self.assertRaisesRegex(P.ProtocolError, "pinned outside"):
            N.validate_parent_gate(self.root, spec, "a" * 64)
        for field, value in (("status", "proposed"), ("native_execution_authorized", False),
                             ("source_and_functionality_accepted", False), ("schema_version", True),
                             ("contract_sha256", "a" * 64), ("runner_sha256", "a" * 64),
                             ("window_id", "another-window"), ("execution_root", "/other/window"),
                             ("queue_receipt_out", "/other/queue")):
            gate = copy.deepcopy(approval)
            gate[field] = value
            (self.root / "approval.json").write_bytes(P.canonical(gate))
            snapshot_before = snapshot(self.root)
            with self.assertRaises(P.ProtocolError):
                N.validate_parent_gate(self.root, spec, P.sha(self.root / "approval.json"))
            self.assertEqual(snapshot(self.root), snapshot_before)
        (self.root / "approval.json").write_bytes(P.canonical(approval))
        with self.assertRaisesRegex(P.ProtocolError, "accepted build/function/data/source inventory"):
            N.validate_parent_gate(self.root, spec, P.sha(self.root / "approval.json"))

    def test_queue_receipt_validation_is_read_only_and_does_not_make_evidence_claim(self):
        root, gate = self.queue()
        before = snapshot(root)
        metadata = N.validate_queue(root, self.spec, gate)
        self.assertEqual(metadata["run_id"], "synthetic-queue-id")
        self.assertNotIn("evidence_class", metadata)
        self.assertEqual(snapshot(root), before)
        (root / "stdout.log").write_bytes(b"tampered")
        with self.assertRaisesRegex(P.ProtocolError, "queue manifest"):
            N.validate_queue(root, self.spec, gate)

    def test_queue_timeout_accepts_W4_float_for_integer_policy_but_rejects_booleans_and_bad_numbers(self):
        root, gate = self.queue()
        request = P.read(root / "request.json")
        request["timeout_seconds"] = 900.0
        (root / "request.json").write_bytes(P.canonical(request))
        self.reseal_queue(root)
        N.validate_queue(root, self.spec, gate)
        for value in (True, False, 0, -1, 900.1, float("inf"), float("nan")):
            with self.subTest(value=value):
                bad = copy.deepcopy(request)
                bad["timeout_seconds"] = value
                with self.assertRaises(P.ProtocolError):
                    N.validate_queue_request(bad, gate)

    def test_resealed_queue_request_and_result_tamper_rejected(self):
        root, gate = self.queue()
        original_request, original_result = P.read(root / "request.json"), P.read(root / "result.json")
        changes = [("request.json", ("owner",), "W1"), ("request.json", ("kind",), "build"),
                   ("request.json", ("runner_sha256",), "a" * 64), ("request.json", ("command",), ["different"]),
                   ("request.json", ("lock_dir",), "/custom/lock"), ("request.json", ("timeout_seconds",), 901),
                   ("request.json", ("out",), "/recorded/another-queue-window"),
                   ("request.json", ("source_headers", "xoffsetdatastructure.hpp"), "a" * 64),
                   ("result.json", ("status",), "failed"), ("result.json", ("run_id",), "wrong"),
                   ("result.json", ("schema_version",), True), ("result.json", ("command_pid",), True),
                   ("result.json", ("command_returncode",), False), ("result.json", ("cleanup", "live"), [99]),
                   ("result.json", ("command_elapsed_seconds",), 901),
                   ("result.json", ("started_at",), "2026-10-15T12:00:00")]
        for filename, keys, value in changes:
            with self.subTest(filename=filename, keys=keys):
                request, result = copy.deepcopy(original_request), copy.deepcopy(original_result)
                put(request if filename == "request.json" else result, keys, value)
                (root / "request.json").write_bytes(P.canonical(request))
                (root / "result.json").write_bytes(P.canonical(result))
                self.reseal_queue(root)
                before = snapshot(root)
                with self.assertRaises(P.ProtocolError):
                    N.validate_queue(root, self.spec, gate)
                self.assertEqual(snapshot(root), before)

    def test_active_context_rejects_successful_unrelated_no_command_and_not_ancestor_queues(self):
        root, gate = self.queue()
        request_bytes = (root / "request.json").read_bytes()
        metadata = dict(supervisor_pid=1000, status="running", owner="W3", kind="timing", run_id="synthetic-queue-id",
                        out=gate["queue_receipt_out"], started_at="2026-10-15T12:00:00+00:00")
        context = N.validate_active_context(request_bytes, metadata, [1001, 1000], gate["window_command"], gate)
        self.assertEqual(context["request_sha256"], digest(request_bytes))
        for command in ([], ["unrelated-successful-command"]):
            with self.assertRaises(P.ProtocolError):
                N.validate_active_context(request_bytes, metadata, [1001, 1000], command, gate)
        with self.assertRaises(P.ProtocolError):
            N.validate_active_context(request_bytes, metadata, [1001], gate["window_command"], gate)
        request = P.parse(request_bytes)
        request["command"] = []
        with self.assertRaises(P.ProtocolError):
            N.validate_active_context(P.canonical(request), metadata, [1001, 1000], gate["window_command"], gate)

    def test_matching_testlock_suffix_is_rejected_even_with_matching_approved_request(self):
        root, gate = self.queue()
        gate["queue_lock_dir"] = "/test/build/vm-coordination"
        request = P.read(root / "request.json")
        request["lock_dir"] = gate["queue_lock_dir"]
        (root / "request.json").write_bytes(P.canonical(request))
        self.reseal_queue(root)
        with self.assertRaises(P.ProtocolError):
            N.validate_queue(root, self.spec, gate)

    def test_collector_branch_recomputes_synthetic_raw_without_granting_native_status(self):
        root, gate = self.collection_fixture()
        before = snapshot(root)
        with mock.patch.object(N, "validate_parent_gate", return_value=gate):
            summary = N.collect_native(root, "synthetic-mocked-gate")
        self.assertEqual(summary["evidence_class"], "synthetic-framework-test")
        self.assertFalse(summary["timing_evidence"])
        self.assertFalse(summary["adoption_allowed"])
        self.assertEqual(snapshot(root), before)

    def test_collector_rejects_lease_split_ancestry_order_timestamp_and_normalized_tamper(self):
        root, gate = self.collection_fixture()
        first_path = root / "commands" / self.jobs[0]["id"] / "receipt.json"
        second_path = root / "commands" / self.jobs[1]["id"] / "receipt.json"
        original_first, original_second = P.read(first_path), P.read(second_path)
        changes = [(("coordination", "lock_was_held"), False),
                   (("coordination", "lock_path"), "/other/queue.lock"),
                   (("coordination", "lock_metadata", "run_id"), "other-window"),
                   (("coordination", "lock_metadata", "out"), "/other/queue"),
                   (("coordination", "ancestor_pids"), [1000, 2000]),
                   (("started_at",), "2026-10-15T11:59:59+00:00")]
        with mock.patch.object(N, "validate_parent_gate", return_value=gate):
            for keys, value in changes:
                with self.subTest(keys=keys):
                    receipt = copy.deepcopy(original_first)
                    put(receipt, keys, value)
                    first_path.write_bytes(P.canonical(receipt))
                    before = snapshot(root)
                    with self.assertRaises(P.ProtocolError):
                        N.collect_native(root, "synthetic-mocked-gate")
                    self.assertEqual(snapshot(root), before)
            first_path.write_bytes(P.canonical(original_first))
            for keys, value in ((("coordination", "lock_metadata", "supervisor_pid"), 2000),
                                (("started_monotonic_ns",), 1_500_000)):
                receipt = copy.deepcopy(original_second)
                put(receipt, keys, value)
                second_path.write_bytes(P.canonical(receipt))
                with self.assertRaises(P.ProtocolError):
                    N.collect_native(root, "synthetic-mocked-gate")
            second_path.write_bytes(P.canonical(original_second))
            normalized_path = root / "commands" / self.jobs[0]["id"] / "normalized.json"
            normalized = P.read(normalized_path)
            normalized["raw_rtt_total_ns"] += 1
            normalized_path.write_bytes(P.canonical(normalized))
            with self.assertRaisesRegex(P.ProtocolError, "does not recompute"):
                N.collect_native(root, "synthetic-mocked-gate")

    def test_collector_detects_resealed_queue_request_mutation_via_live_context_digest(self):
        root, gate = self.collection_fixture()
        request_path = root / "queue/request.json"
        request = P.read(request_path)
        # This field is not a request-policy field. It still changes the exact
        # live request identity captured by every producer receipt.
        request["cwd"] = "/other/original/working-directory"
        request_path.write_bytes(P.canonical(request))
        self.reseal_queue(root / "queue")
        with mock.patch.object(N, "validate_parent_gate", return_value=gate):
            with self.assertRaisesRegex(P.ProtocolError, "start/request context"):
                N.collect_native(root, "synthetic-mocked-gate")

    def test_runtime_launcher_gate_failure_executes_nothing_and_changes_no_files(self):
        root, gate, context = self.launch_fixture()
        before = snapshot(root)
        with mock.patch.object(N, "validate_parent_gate", side_effect=P.ProtocolError("failed source/build prerequisite")), \
             mock.patch.object(N, "active_queue") as queue, mock.patch.object(N.subprocess, "run") as producer:
            with self.assertRaisesRegex(P.ProtocolError, "failed source/build"):
                N.collect_window(root, "synthetic-mocked-gate")
        producer.assert_not_called()
        queue.assert_not_called()
        self.assertEqual(snapshot(root), before)

    def test_runtime_launcher_mock_execution_preserves_exact_argv_raw_and_all_outputs_once(self):
        root, gate, context = self.launch_fixture()

        def fake_run(invocation, **kwargs):
            self.assertEqual(kwargs["stdin"], N.subprocess.DEVNULL)
            directory = Path(invocation[invocation.index("--out-dir") + 1])
            self.assertFalse(directory.exists())
            directory.mkdir()
            job = next(j for j in self.jobs if j["id"] == directory.parent.name)
            self.assertEqual(invocation, self.spec["invocations"][job["id"]])
            for index in range(job["iterations"]):
                (directory / ("output-%d.wire" % index)).write_bytes(self.output_bytes)
            kwargs["stdout"].write(P.canonical(self.raw(job)))
            kwargs["stderr"].write(self.stderr)
            return SimpleNamespace(returncode=0)

        with mock.patch.object(N, "validate_parent_gate", return_value=gate), \
             mock.patch.object(N, "active_queue", return_value=context), \
             mock.patch.object(N, "environment_snapshot", side_effect=self.environment), \
             mock.patch.object(N, "validate_live_inputs", return_value=self.spec["producer_identity"]) as live, \
             mock.patch.object(N.subprocess, "run", side_effect=fake_run) as producer:
            result = N.collect_window(root, "synthetic-mocked-gate")
            self.assertEqual(producer.call_count, len(self.jobs))
            self.assertEqual(sum(call.kwargs.get("full_environment", False) for call in live.call_args_list), 2)
            self.assertFalse(result["timing_evidence"])
            self.assertFalse(result["adoption_allowed"])
            self.assertTrue(result["queue_completion_pending"])
            self.assertFalse((root / "COMPLETE").exists())
            for job in self.jobs:
                directory = root / "commands" / job["id"]
                self.assertEqual((directory / "stderr").read_bytes(), self.stderr)
                self.assertEqual(P.read(directory / "receipt.json")["argv"], self.spec["invocations"][job["id"]])
                self.assertEqual(P.read(directory / "normalized.json")["evidence_class"], "synthetic-framework-test")
            with self.assertRaisesRegex(P.ProtocolError, "cannot be reused"):
                N.collect_window(root, "synthetic-mocked-gate")
            self.assertEqual(producer.call_count, len(self.jobs))

    def test_runtime_launcher_mock_failure_retains_failed_raw_prefix_and_stops(self):
        root, gate, context = self.launch_fixture()
        raw = b"partial failure stdout\x00\xff"

        def fake_run(invocation, **kwargs):
            kwargs["stdout"].write(raw)
            kwargs["stderr"].write(self.stderr)
            return SimpleNamespace(returncode=2)

        with mock.patch.object(N, "validate_parent_gate", return_value=gate), \
             mock.patch.object(N, "active_queue", return_value=context), \
             mock.patch.object(N, "environment_snapshot", side_effect=self.environment), \
             mock.patch.object(N, "validate_live_inputs", return_value=self.spec["producer_identity"]), \
             mock.patch.object(N.subprocess, "run", side_effect=fake_run) as producer:
            with self.assertRaisesRegex(P.ProtocolError, "raw prefix retained"):
                N.collect_window(root, "synthetic-mocked-gate")
        self.assertEqual(producer.call_count, 1)
        directory = root / "commands" / self.jobs[0]["id"]
        self.assertEqual((directory / "stdout").read_bytes(), raw)
        self.assertEqual(P.read(directory / "receipt.json")["returncode"], 2)
        self.assertTrue((root / "collection-failed.json").exists())
        self.assertFalse((root / "COMPLETE").exists())

    def test_environment_snapshots_are_bounded_edges_with_raw_hashes_and_explicit_unavailability(self):
        process = SimpleNamespace(returncode=0, stdout=b"synthetic bounded probe stdout", stderr=b"")
        with mock.patch.object(N.subprocess, "run", return_value=process) as probe, \
             mock.patch.object(N.os, "getloadavg", return_value=(1.0, 2.0, 3.0)):
            snapshot_value = N.environment_snapshot("before")
        self.assertEqual(probe.call_count, len(N.ENVIRONMENT_POLICY["commands"]))
        self.assertIs(N.validate_environment(snapshot_value, "before", self.spec), snapshot_value)
        for call in probe.call_args_list:
            self.assertEqual(call.kwargs["timeout"], 3)
        snapshot_value["probes"]["power"]["stdout"]["sha256"] = "a" * 64
        with self.assertRaisesRegex(P.ProtocolError, "raw probe identity"):
            N.validate_environment(snapshot_value, "before", self.spec)
        unavailable = self.environment("after")
        N.validate_environment(unavailable, "after", self.spec)
        unavailable["probes"]["thermal"]["reason"] = ""
        with self.assertRaises(P.ProtocolError):
            N.validate_environment(unavailable, "after", self.spec)

    def test_prepare_fixture_rejects_unaccepted_build_before_any_execution_or_output(self):
        out = self.root / "unapproved-preparation"
        with mock.patch.object(N, "active_queue") as queue, mock.patch.object(N.subprocess, "run") as producer:
            with self.assertRaisesRegex(P.ProtocolError, "parent-accepted W1 G1"):
                N.prepare_fixtures(out, self.root / "build", self.root / "missing-G1", "a" * 64,
                                   self.root / "queue")
        queue.assert_not_called()
        producer.assert_not_called()
        self.assertFalse(out.exists())

    def test_real_accepted_G1_build_identity_is_readable_without_executing_native_code(self):
        production = Path(N.PRODUCTION_QUEUE).parents[1]
        build = production / "build/vm-network/timing-v2"
        acceptance = production / "docs/coordination/vm-followup/W1-G1-acceptance.json"
        before = P.inventory(build)
        with mock.patch.object(N.subprocess, "run") as producer:
            identity = N.accepted_preparation_build(build, acceptance, N.G1_SHA)
        producer.assert_not_called()
        self.assertEqual(identity["binary_sha256"], N.G1_BINARY_SHA)
        self.assertEqual(P.inventory(build), before)

    def test_prepare_fixture_mocked_eight_calls_discover_canonical_full_data_without_timing_claims(self):
        out = self.root / "prepared-fixtures"
        acceptance = self.root / "synthetic-G1.json"
        acceptance.write_bytes(P.canonical(dict(status="accepted_with_explicit_sanitizer_prerequisite", contract_sha256=N.CONTRACT_SHA,
                                                binaries=dict(timing=dict(sha256=self.spec["producer_identity"]["binary_sha256"])),
                                                synthetic_framework_test=True)))
        started_at = (dt.datetime.now(dt.timezone.utc) - dt.timedelta(seconds=1)).isoformat()
        context = dict(explicit_flag=True, lock_was_held=True, synthetic_framework_test=True,
                       lock_path=N.PRODUCTION_QUEUE + "/queue.lock", request_sha256="a" * 64,
                       collector_argv=[N.sys.executable, *N.sys.argv], ancestor_pids=[1001, 1000],
                       lock_metadata=dict(status="running", owner="W3", kind="functional", supervisor_pid=1000,
                                          run_id="synthetic-preparation", out=str(self.root / "queue-receipt"), started_at=started_at))
        template = N.preregistration()
        # Four case labels, tiny equal synthetic payloads. Actual 4 KiB..16 MiB
        # fixtures can only be generated after the real acceptance gate passes.
        for work in template["workloads"]:
            work["elements"] = 1
        last_input = {}

        def fake_run(invocation, **kwargs):
            if invocation[1] == "fixture":
                target = Path(invocation[invocation.index("--output") + 1])
                target.write_bytes(self.input.read_bytes())
                raw = dict(schema_version=1, contract_version="1.0.0", status="passed", command="fixture",
                           diagnostic=False, timing_evidence=False, element_count=1, frame_bytes=target.stat().st_size,
                           semantic_checksum="122")
            else:
                directory = Path(invocation[invocation.index("--out-dir") + 1])
                directory.mkdir()
                (directory / "output-0.wire").write_bytes(self.output_bytes)
                raw = self.raw()
                raw.update(iterations=1, end_to_end_ns=100, client_owner_release_outside_ns=2,
                           server_completion_wait_outside_ns=3, samples=raw["samples"][:1], output_files=["output-0.wire"],
                           max_wire_bytes=template["controls"]["max_wire_bytes"],
                           owner_capacity_bytes=template["controls"]["owner_capacity_bytes"],
                           max_inflight_bytes=template["controls"]["max_inflight_bytes"],
                           reserved_inflight_peak_bytes=4 * template["controls"]["max_wire_bytes"])
            kwargs["stdout"].write(P.canonical(raw))
            kwargs["stderr"].write(self.stderr)
            return SimpleNamespace(returncode=0)

        with mock.patch.object(N, "accepted_preparation_build", return_value=self.spec["producer_identity"]), \
             mock.patch.object(N, "active_queue", return_value=context), \
             mock.patch.object(N, "validate_live_inputs", return_value=self.spec["producer_identity"]), \
             mock.patch.object(N, "preregistration", side_effect=lambda: copy.deepcopy(template)), \
             mock.patch.object(N.subprocess, "run", side_effect=fake_run) as producer:
            result = N.prepare_fixtures(out, self.root / "accepted-build", acceptance, P.sha(acceptance),
                                        self.root / "queue-receipt")
        self.assertEqual(producer.call_count, 8)
        self.assertEqual(result["native_commands"], 8)
        self.assertFalse(result["timing_evidence"])
        self.assertTrue(result["queue_completion_pending"])
        record = P.read(out / "fixture-preparation.json")
        self.assertEqual(len(record["expected_data"]), 4)
        for case in record["cases"]:
            identity = record["expected_data"][case["id"]]
            self.assertEqual(P.sha(case["canonical_input"]), identity["input_sha256"])
            self.assertEqual(P.sha(case["canonical_output"]), identity["output_sha256"])
        self.assertFalse((out / "COMPLETE").exists())
        with mock.patch.object(N, "G1_SHA", P.sha(acceptance)), \
             mock.patch.object(N, "G1_BINARY_SHA", self.spec["producer_identity"]["binary_sha256"]), \
             mock.patch.object(N, "preregistration", side_effect=lambda: copy.deepcopy(template)):
            relocated = self.root / "relocated-fixture-archive"
            shutil.move(str(out), str(relocated))
            before = snapshot(relocated)
            result = N.audit_fixture_preparation(relocated, require_queue=False)
            self.assertFalse(result["functional_queue_verified"])
            self.assertFalse(result["timing_evidence"])
            self.assertFalse(result["original_absolute_paths_accessed"])
            self.assertEqual(snapshot(relocated), before)
            queue = dict(run_id="synthetic-preparation", request_sha256="a" * 64, command_pid=1001, started_at=started_at,
                         manifest_sha256="b" * 64, result_sha256="c" * 64,
                         begin=N._timestamp(started_at), end=dt.datetime.now(dt.timezone.utc) + dt.timedelta(seconds=1), elapsed=30.0,
                         request=dict(out=str(self.root / "queue-receipt")))
            with mock.patch.object(N, "validate_queue", return_value=queue):
                checked = N.audit_fixture_preparation(relocated)
                self.assertTrue(checked["functional_queue_verified"])
            for changed_queue in (dict(queue, elapsed=.000000001),
                                  dict(queue, begin=dt.datetime.now(dt.timezone.utc) + dt.timedelta(days=1)),
                                  dict(queue, started_at="2050-01-01T00:00:00+00:00")):
                with self.subTest(functional_queue_enclosure=changed_queue):
                    with mock.patch.object(N, "validate_queue", return_value=changed_queue):
                        with self.assertRaises(P.ProtocolError):
                            N.audit_fixture_preparation(relocated)

            def rehash_preparation():
                metadata = P.read(relocated / "fixture-preparation.json")
                metadata["files"] = P.inventory(relocated, ("fixture-preparation.json",))
                (relocated / "fixture-preparation.json").write_bytes(P.canonical(metadata))

            plan_path = relocated / "preregister.json"
            original_plan = plan_path.read_bytes()
            for field, value in (("schema_version", True), ("adoption_allowed", True)):
                with self.subTest(resealed_plan=field):
                    plan = P.parse(original_plan)
                    plan[field] = value
                    plan_path.write_bytes(P.canonical(plan))
                    rehash_preparation()
                    with self.assertRaisesRegex(P.ProtocolError, "fixture plan differs"):
                        N.audit_fixture_preparation(relocated, require_queue=False)
            plan_path.write_bytes(original_plan)
            fixture_receipt = relocated / "cases/4096/fixture-command/receipt.json"
            original_receipt = fixture_receipt.read_bytes()
            for changes in (dict(started_monotonic_ns=-20, finished_monotonic_ns=-10),
                            dict(started_at="2050-01-01T00:00:00+00:00", finished_at="2040-01-01T00:00:00+00:00"),
                            dict(started_at="2026-10-15T12:00:00")):
                with self.subTest(resealed_receipt=changes):
                    receipt = P.parse(original_receipt)
                    receipt.update(changes)
                    fixture_receipt.write_bytes(P.canonical(receipt))
                    rehash_preparation()
                    before = snapshot(relocated)
                    with self.assertRaises((P.ProtocolError, ValueError)):
                        N.audit_fixture_preparation(relocated, require_queue=False)
                    self.assertEqual(snapshot(relocated), before)
            fixture_receipt.write_bytes(original_receipt)
            adapter_path = relocated / "cases/4096/commands/j000000/adapter-receipt.json"
            original_path = adapter_path.with_name("receipt.json")
            original_adapter, original_run = adapter_path.read_bytes(), original_path.read_bytes()
            receipt = P.parse(original_adapter)
            receipt["started_monotonic_ns"] = P.parse(original_receipt)["started_monotonic_ns"]
            adapter_path.write_bytes(P.canonical(receipt))
            original_path.write_bytes(P.canonical({k: v for k, v in receipt.items() if k != "job"}))
            rehash_preparation()
            with self.assertRaisesRegex(P.ProtocolError, "overlapping"):
                N.audit_fixture_preparation(relocated, require_queue=False)
            adapter_path.write_bytes(original_adapter)
            original_path.write_bytes(original_run)
            rehash_preparation()
            # Build a tiny synthetic historical-format archive using only the
            # exact reviewed c53 *tool source*, not any native fixture data.
            protocol_path = relocated / "protocol/network_measurement.py"
            c53_path = Path(N.PRODUCTION_QUEUE).parents[1] / "build/vm-network-measurement/network-tools-v1/network_measurement.py"
            c53_source = c53_path.read_bytes()
            self.assertEqual(digest(c53_source), N.C53_FUNCTIONAL_PROTOCOL_SHA)
            protocol_path.write_bytes(c53_source)
            (relocated / "protocol/collector-runtime.json").unlink()
            plan = P.read(plan_path)
            del plan["collector_runtime"]
            plan_path.write_bytes(P.canonical(plan))
            for case in record["cases"]:
                case_spec_path = relocated / "cases" / case["id"] / "case-spec.json"
                case_spec = P.read(case_spec_path)
                del case_spec["collector_runtime"]
                case_spec_path.write_bytes(P.canonical(case_spec))
            rehash_preparation()
            before = snapshot(relocated)
            unreviewed_paired = SimpleNamespace(__file__="/unreviewed/paired_measurement.py")
            with mock.patch.object(N.subprocess, "run") as producer, \
                 mock.patch.dict(sys.modules, {"paired_measurement": unreviewed_paired}):
                legacy = N.audit_fixture_preparation(relocated, require_queue=False,
                                                      accepted_execution_protocol_sha256=N.C53_FUNCTIONAL_PROTOCOL_SHA)
                self.assertIs(sys.modules["paired_measurement"], unreviewed_paired)
            producer.assert_not_called()
            self.assertEqual(legacy["execution_protocol_sha256"], N.C53_FUNCTIONAL_PROTOCOL_SHA)
            self.assertEqual(legacy["auditor_protocol_sha256"], P.sha(N.__file__))
            self.assertEqual(legacy["collector_runtime_provenance"]["status"], "unrecorded_in_functional_v1")
            self.assertFalse(legacy["collector_runtime_provenance"]["retrospective_identity_inferred"])
            self.assertFalse(legacy["collector_runtime_provenance"]["timing_runtime_requirement_waived"])
            self.assertEqual(snapshot(relocated), before)
            for source_pin in (None, "f" * 64):
                with self.assertRaisesRegex(P.ProtocolError, "unreviewed"):
                    N.audit_fixture_preparation(relocated, require_queue=False, accepted_execution_protocol_sha256=source_pin)
            plan["collector_runtime"] = self.spec["collector_runtime"]
            plan_path.write_bytes(P.canonical(plan))
            rehash_preparation()
            with self.assertRaisesRegex(P.ProtocolError, "remain unrecorded"):
                N.audit_fixture_preparation(relocated, require_queue=False,
                                            accepted_execution_protocol_sha256=N.C53_FUNCTIONAL_PROTOCOL_SHA)
            del plan["collector_runtime"]
            plan_path.write_bytes(P.canonical(plan))
            rehash_preparation()
            path = relocated / "cases/4096/commands/j000000/outputs/output-0.wire"
            path.write_bytes(b"x" * len(self.output_bytes))
            metadata = P.read(relocated / "fixture-preparation.json")
            metadata["files"] = P.inventory(relocated, ("fixture-preparation.json",))
            (relocated / "fixture-preparation.json").write_bytes(P.canonical(metadata))
            with self.assertRaisesRegex(P.ProtocolError, "per-iteration full output"):
                N.audit_fixture_preparation(relocated, require_queue=False,
                                            accepted_execution_protocol_sha256=N.C53_FUNCTIONAL_PROTOCOL_SHA)

    def test_live_original_binary_runtime_source_and_dependency_mutations_rejected(self):
        root, build = self.root / "live-window", self.root / "original-build"
        root.mkdir()
        (root / "data").mkdir()
        (root / "data" / P.sha(self.input)).write_bytes(self.input.read_bytes())
        (build / "source").mkdir(parents=True)
        (build / "source/reference.cpp").write_bytes(b"synthetic source; not compiled")
        (build / "net_reference").write_bytes(b"synthetic binary bytes; not executable")
        source = {k: v["sha256"] for k, v in P.inventory(build / "source").items()}
        P.write_new(build / "source-inventory.json", source)
        runtime = self.root / "runtime.dylib"
        compiler = self.root / "compiler.stub"
        runtime.write_bytes(b"synthetic runtime bytes")
        compiler.write_bytes(b"synthetic compiler bytes; never executed")
        sdk = self.root / "SDK"
        sdk.mkdir()
        (sdk / "SDKSettings.json").write_bytes(b"synthetic SDK identity")
        deps = self.root / "deps"
        deps.mkdir()
        (deps / "header.hpp").write_bytes(b"synthetic dependency bytes")
        P.write_new(build / "toolchain.json", dict(runtime={str(runtime): P.sha(runtime)}, compiler=str(compiler), compiler_sha256=P.sha(compiler),
                                                    sdk=str(sdk), sdk_settings_sha256=P.sha(sdk / "SDKSettings.json")))
        P.write_new(build / "dependencies.json", dict(root=str(deps), files={"header.hpp": P.sha(deps / "header.hpp")}))
        spec = copy.deepcopy(self.spec)
        spec.update(execution_root=str(root), build_execution_root=str(build))
        spec["paths"] = dict(binary=str(build / "net_reference"), inputs={"tiny": str(root / "data" / P.sha(self.input))},
                             outputs={j["id"]: str(root / "commands" / j["id"] / "outputs") for j in self.jobs})
        spec["invocations"] = {j["id"]: N.argv(spec, j) for j in self.jobs}
        spec["producer_identity"] = dict(binary_sha256=P.sha(build / "net_reference"), source_inventory_sha256=P.sha(build / "source-inventory.json"),
                                          toolchain_sha256=P.sha(build / "toolchain.json"), dependencies_sha256=P.sha(build / "dependencies.json"))
        P.write_new(root / "preregister.json", spec)
        P.write_new(root / "schedule.json", self.jobs)
        gate = dict(preregister_sha256=P.sha(root / "preregister.json"),
                    accepted_inputs={"protocol/network_measurement.py": dict(sha256=P.sha(N.__file__)),
                                     "protocol/paired_measurement.py": dict(sha256=P.sha(P.__file__))})
        loader_env = {key: "" for key in ("LD_PRELOAD", "LD_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES", "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH")}
        with mock.patch.dict(N.os.environ, loader_env):
            N.validate_live_inputs(root, spec, gate, full_environment=True)
            for field, replacement in (("version", "unapproved Python version"), ("sha256", "a" * 64)):
                with self.subTest(collector_runtime=field):
                    changed = copy.deepcopy(spec)
                    changed["collector_runtime"]["interpreter"][field] = replacement
                    with self.assertRaisesRegex(P.ProtocolError, "interpreter bytes/version"):
                        N.validate_live_inputs(root, changed, gate, full_environment=True)
            for path, reason in ((build / "net_reference", "executed binary"), (build / "source/reference.cpp", "source bytes"),
                                 (runtime, "runtime bytes"), (deps / "header.hpp", "dependency bytes")):
                with self.subTest(path=path.name):
                    original = path.read_bytes()
                    path.write_bytes(original + b" changed")
                    with self.assertRaisesRegex(P.ProtocolError, reason):
                        N.validate_live_inputs(root, spec, gate, full_environment=True)
                    if path == build / "net_reference":
                        before = snapshot(root)
                        with mock.patch.object(N, "validate_parent_gate", return_value=gate), \
                             mock.patch.object(N, "active_queue", return_value={}), mock.patch.object(N.subprocess, "run") as producer:
                            with self.assertRaisesRegex(P.ProtocolError, "executed binary"):
                                N.collect_window(root, "synthetic-mocked-gate")
                        producer.assert_not_called()
                        self.assertEqual(snapshot(root), before)
                    path.write_bytes(original)

    def test_failed_native_protocol_preserves_partial_bytes_without_completion_claim(self):
        root, gate = self.collection_fixture()
        for job in self.jobs[1:]:
            shutil.rmtree(root / "commands" / job["id"])
        directory = root / "commands" / self.jobs[0]["id"]
        raw = b"partial failed binary stdout\x00\xff"
        (directory / "stdout").write_bytes(raw)
        (directory / "normalized.json").unlink()
        queue_result = P.read(root / "queue/result.json")
        queue_result.update(status="failed", command_returncode=2)
        (root / "queue/result.json").write_bytes(P.canonical(queue_result))
        self.reseal_queue(root / "queue")
        with mock.patch.object(N, "validate_parent_gate", return_value=gate):
            result = N.mark_failed_native(root, "synthetic-mocked-gate", "synthetic producer failure")
            self.assertEqual(result["status"], "failed_preserved")
            self.assertEqual(result["raw_job_prefix"], 1)
            self.assertFalse(result["timing_evidence"])
            self.assertFalse((root / "COMPLETE").exists())
            self.assertEqual((directory / "stdout").read_bytes(), raw)
            before = snapshot(root)
            N.audit_native(root, "synthetic-mocked-gate")
            self.assertEqual(snapshot(root), before)
            with self.assertRaises(P.ProtocolError):
                N.mark_failed_native(root, "synthetic-mocked-gate", "replacement")

    def test_busy_zero_execution_failure_needs_no_commands_directory(self):
        root, gate = self.collection_fixture()
        shutil.rmtree(root / "commands")
        (root / "collection.json").unlink()
        (root / "collection-finished.json").unlink()
        path = root / "queue/result.json"
        result = P.read(path)
        result.update(status="busy", command_pid=None, command_returncode=None, cleanup=None,
                      started_at=None, finished_at="2026-10-15T12:00:01+00:00")
        path.write_bytes(P.canonical(result))
        self.reseal_queue(root / "queue")
        with mock.patch.object(N, "validate_parent_gate", return_value=gate):
            result = N.mark_failed_native(root, "synthetic-mocked-gate", "queue busy; zero producer execution")
            self.assertEqual(result["raw_job_prefix"], 0)
            self.assertEqual(result["queue_status"], "busy")
            self.assertEqual(result["status"], "failed_preserved")
        self.assertFalse((root / "COMPLETE").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
