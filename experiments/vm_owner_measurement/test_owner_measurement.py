"""Synthetic D023 checks only: these tests never launch a native workload.

The synthetic native records model the D022 Apple arm64 alignment contract.
Loading the producer's pure semantic checker here does not assert acceptance of
any current native binary or turn these fixtures into measurement evidence.
"""

import copy
import importlib.util
import math
import os
from pathlib import Path
import shutil
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock


HERE = Path(__file__).resolve().parent


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


audit = load_module("owner_measurement_audit_tests", HERE / "audit_owner_measurement.py")
producer = load_module("owner_measurement_synthetic_producer_checker",
                       HERE.parent / "vm_owner_scaling" / "audit_owner_scaling.py")
collector = load_module("owner_measurement_collector_tests", HERE / "collect_owner_measurement.py")
REPO = HERE.parents[1]
SCRATCH = REPO / "build/vm-owner-measurement/test-scratch"
PHASES = ("create", "allocate", "free", "destroy")
CONDITIONS = (("A", 8, 1), ("B", 64, 1), ("C", 256, 1),
              ("D", 256, 2), ("E", 256, 4), ("F", 256, 8))


def expected_schedule():
    """Independent literal implementation of the frozen rotate/reverse rule."""
    slots = []
    for position, (condition, owners, threads) in enumerate(CONDITIONS):
        slots.append(dict(index=len(slots), stage="prerun", block=None,
                          position=position, condition=condition, owners=owners,
                          threads=threads, requests=8192, control="none"))
    for block in range(6):
        order = CONDITIONS[block:] + CONDITIONS[:block]
        for position, (condition, owners, threads) in enumerate(order + tuple(reversed(order))):
            slots.append(dict(index=len(slots), stage="measured", block=block,
                              position=position, condition=condition, owners=owners,
                              threads=threads, requests=8192, control="none"))
    return slots


def synthetic_native(slot, times=None):
    """Complete artificial R8192 timing record; never native execution output."""
    c, t, r = slot["owners"], slot["threads"], 8192
    q = r // c
    checksum = 14695981039346656037
    for k in range(r):
        checksum = ((checksum ^ (0x195A926A5CC24873 ^ k)) * 1099511628211) & ((1 << 64) - 1)
    owners = []
    for i in range(c):
        end = 48 + 24 * q
        owners.append(dict(index=i, worker=i % t, core=str(0x70000000 + 256 * i),
                           base=str(0x10000000 + 0x20000 * i), logical=65536,
                           used_end=end, free_before=65536 - end,
                           free_after=65536 - end + 24 * q, created=True,
                           allocated=q, freed=q, destroyed=True))
    return dict(schema_version=1, profile="timing", timing_evidence=False,
                condition=dict(owners=c, threads=t, requests=r, per_owner=q),
                control="none", status="ok", reason=None,
                counts=dict(threads_started=t, threads_joined=t, created=c,
                            allocated=r, freed=r, destroyed=c, validated=r),
                checksum=str(checksum),
                phases={phase: dict(wall_ns=(times or {}).get(phase, 10000)) for phase in PHASES},
                owners=owners,
                validation=dict(values=True, alignment=True, ranges=True,
                                nonoverlap=True, conservation=True), trace=None)


_NATIVE_TEMPLATES = {condition: synthetic_native(dict(owners=c, threads=t))
                     for condition, c, t in CONDITIONS}


def synthetic_records(time_for=None):
    records = []
    for slot in expected_schedule():
        report = copy.deepcopy(_NATIVE_TEMPLATES[slot["condition"]])
        for phase in PHASES:
            report["phases"][phase]["wall_ns"] = 10000 if time_for is None else time_for(slot, phase)
        records.append(dict(slot=copy.deepcopy(slot), report=report))
    return records


def cell(result, condition="A", phase="create"):
    return next(item for item in result["cells"] if item["condition"] == condition and item["phase"] == phase)


def comparison(result, condition="A", phase="create"):
    return next(item for item in result["comparisons"]
                if item["condition"] == condition and item["phase"] == phase)


def set_pair(records, *, condition="A", phase="create", block=0, first=10000, second=10000):
    matches = [record for record in records if record["slot"]["stage"] == "measured"
               and record["slot"]["condition"] == condition and record["slot"]["block"] == block]
    assert len(matches) == 2
    matches[0]["report"]["phases"][phase]["wall_ns"] = first
    matches[1]["report"]["phases"][phase]["wall_ns"] = second


class ScheduleTests(unittest.TestCase):
    def test_exact_fixed_78_order_and_pairs(self):
        actual = audit.schedule()
        self.assertEqual(actual, expected_schedule())
        self.assertEqual(len(actual), 78)
        self.assertEqual("".join(s["condition"] for s in actual[:6]), "ABCDEF")
        for block in range(6):
            sequence = actual[6 + 12 * block:18 + 12 * block]
            order = "ABCDEF"[block:] + "ABCDEF"[:block]
            self.assertEqual("".join(s["condition"] for s in sequence), order + order[::-1])
            for condition, owners, threads in CONDITIONS:
                pair = [s for s in sequence if s["condition"] == condition]
                self.assertEqual(len(pair), 2)
                self.assertEqual(pair[0]["position"] + pair[1]["position"], 11)
                self.assertTrue(all(s["owners"] == owners and s["threads"] == threads for s in pair))

    def test_schedule_returns_independent_mutable_values(self):
        first = audit.schedule()
        first[0]["requests"] = 0
        first.pop()
        self.assertEqual(audit.schedule(), expected_schedule())


class NativeAuditTests(unittest.TestCase):
    def reject(self, report, slot=None, checker=producer):
        with self.assertRaises(audit.AuditError):
            audit.audit_native(report, slot or audit.schedule()[0], checker)

    def test_all_six_synthetic_records_use_the_real_pure_semantic_checker(self):
        for slot in audit.schedule()[:6]:
            with self.subTest(condition=slot["condition"]):
                record = synthetic_native(slot)
                original = copy.deepcopy(record)
                result = audit.audit_native(record, slot, producer)
                self.assertEqual(result["status"], "passed")
                self.assertIs(result["timing_evidence"], False)
                self.assertEqual(result["phases"], record["phases"])
                self.assertEqual(record, original)

    def test_checker_receives_exact_frozen_profile_condition_and_control(self):
        calls = []

        def checker(report, **kwargs):
            calls.append((report, kwargs))
            return dict(status="passed", timing_evidence=False)

        slot = audit.schedule()[4]
        record = synthetic_native(slot)
        audit.audit_native(record, slot, SimpleNamespace(audit_record=checker))
        self.assertEqual(len(calls), 1)
        self.assertIs(calls[0][0], record)
        self.assertEqual(calls[0][1], dict(expected_profile="timing", expected_condition=(256, 4, 8192),
                                         expected_control="none"))

    def test_workload_semantics_do_not_accept_a_partial_or_different_run(self):
        for field, value in (("profile", "diagnostic"), ("timing_evidence", True),
                             ("control", "fail_allocate"), ("status", "expected_failure")):
            with self.subTest(field=field):
                record = synthetic_native(audit.schedule()[0])
                record[field] = value
                self.reject(record)
        for key, value in (("requests", 256), ("owners", 64), ("threads", 2)):
            record = synthetic_native(audit.schedule()[0])
            record["condition"][key] = value
            self.reject(record)
        for owner_field, value in (("worker", 2), ("used_end", 1), ("destroyed", False)):
            record = synthetic_native(audit.schedule()[0])
            record["owners"][0][owner_field] = value
            self.reject(record)

    def test_nonpositive_and_non_uint64_times_in_every_phase_are_rejected(self):
        for phase in PHASES:
            for value in (0, -1, True, False, None, 1.5, "10000", math.nan,
                          math.inf, -math.inf, 1 << 64):
                with self.subTest(phase=phase, value=value):
                    record = synthetic_native(audit.schedule()[0])
                    record["phases"][phase]["wall_ns"] = value
                    self.reject(record)

    def test_uint64_positive_endpoint_is_permitted(self):
        record = synthetic_native(audit.schedule()[0], {phase: (1 << 64) - 1 for phase in PHASES})
        self.assertEqual(audit.audit_native(record, audit.schedule()[0], producer)["status"], "passed")

    def test_arbitrary_or_malformed_slot_is_rejected(self):
        for field, value in (("index", True), ("index", 78), ("stage", "foreign"),
                             ("condition", "G"), ("requests", 256), ("owners", 64),
                             ("control", "fail_create"), ("position", 7)):
            with self.subTest(field=field):
                slot = copy.deepcopy(audit.schedule()[0])
                slot[field] = value
                self.reject(synthetic_native(audit.schedule()[0]), slot)


class StatisticsTests(unittest.TestCase):
    def reject(self, records):
        with self.assertRaises(audit.AuditError):
            audit.summarize(records)

    def test_all_24_cells_and_all_24_comparisons_are_retained(self):
        result = audit.summarize(synthetic_records())
        self.assertEqual(result["native_processes"], 78)
        self.assertEqual(result["prerun_processes"], 6)
        self.assertEqual(result["measured_processes"], 72)
        self.assertEqual(result["samples_per_condition"], 12)
        self.assertEqual(result["reference_condition"], "C")
        expected = [(condition, phase) for condition, _, _ in CONDITIONS for phase in PHASES]
        self.assertEqual([(c["condition"], c["phase"]) for c in result["cells"]], expected)
        self.assertEqual([(c["condition"], c["phase"]) for c in result["comparisons"]], expected)
        for item in result["cells"]:
            self.assertEqual(item["sample_count"], 12)
            self.assertEqual(len(item["pairs"]), 6)
            self.assertTrue(item["qualified"])
        for item in result["comparisons"]:
            self.assertEqual(item["reference_condition"], "C")
            self.assertEqual(item["ratios"], [1.0] * 6)
            self.assertEqual(item["direction"], "unresolved")

    def test_missing_duplicate_extra_foreign_and_wrong_order_samples_rejected(self):
        cases = []
        records = synthetic_records()
        cases.append(records[:-1])
        records = synthetic_records()
        records.append(copy.deepcopy(records[-1]))
        cases.append(records)
        records = synthetic_records()
        records[20] = copy.deepcopy(records[19])
        cases.append(records)
        records = synthetic_records()
        records[6], records[7] = records[7], records[6]
        cases.append(records)
        records = synthetic_records()
        records[30]["slot"]["condition"] = "G"
        cases.append(records)
        records = synthetic_records()
        records[30]["slot"]["extra"] = "unfrozen"
        cases.append(records)
        for records in cases:
            with self.subTest(indices=[r["slot"]["index"] for r in records]):
                self.reject(records)

    def test_summary_rechecks_every_sample_native_profile_and_times(self):
        for index in (0, 6, 77):
            for field, value in (("profile", "diagnostic"), ("timing_evidence", True), ("control", "fail_create")):
                records = synthetic_records()
                records[index]["report"][field] = value
                self.reject(records)
            for value in (0, -1, True, 0.5, math.nan, math.inf, "100", 1 << 64):
                records = synthetic_records()
                records[index]["report"]["phases"]["destroy"]["wall_ns"] = value
                self.reject(records)
            records = synthetic_records()
            records[index]["report"]["condition"]["requests"] = 256
            self.reject(records)
        records = synthetic_records()
        del records[-1]["report"]["phases"]["free"]
        self.reject(records)

    def test_preruns_do_not_enter_the_descriptive_statistics(self):
        records = synthetic_records()
        for record in records[:6]:
            for phase in PHASES:
                record["report"]["phases"][phase]["wall_ns"] = (1 << 64) - 1
        result = audit.summarize(records)
        for item in result["cells"]:
            self.assertEqual(item["metrics"]["batch_ns"]["block_values"], [10000.0] * 6)

    def test_geometric_pairing_aa_dispersion_and_anchor_values(self):
        records = synthetic_records()
        for block in range(6):
            # Six exact square roots: 200, 220, 240, 260, 280, 300.
            set_pair(records, block=block, first=100 + 10 * block, second=400 + 40 * block)
        item = cell(audit.summarize(records))
        expected_g = [200.0 + 20.0 * b for b in range(6)]
        self.assertEqual([p["g_ns"] for p in item["pairs"]], expected_g)
        self.assertAlmostEqual(item["aa"], 4.0)
        self.assertAlmostEqual(item["block_dispersion"], 1.5)
        anchors = [math.sqrt(expected_g[2 * e] * expected_g[2 * e + 1]) for e in range(3)]
        for actual, expected in zip(item["anchors_ns"], anchors):
            self.assertAlmostEqual(actual, expected)
        self.assertAlmostEqual(item["anchor_drift"], anchors[-1] / anchors[0])
        self.assertFalse(item["qualified"])
        self.assertEqual(item["status"], "uncertain")
        indices = [[r["slot"]["index"] for r in records
                    if r["slot"]["stage"] == "measured" and r["slot"]["condition"] == "A"
                    and r["slot"]["block"] == b] for b in range(6)]
        self.assertEqual([p["indices"] for p in item["pairs"]], indices)

    def test_aa_110_percent_inclusive_boundary_both_directions(self):
        for first, second, expected in ((10000, 11000, True), (11000, 10000, True),
                                        (10000, 11001, False), (11001, 10000, False)):
            with self.subTest(first=first, second=second):
                records = synthetic_records()
                set_pair(records, first=first, second=second)
                item = cell(audit.summarize(records))
                self.assertEqual(item["checks"]["aa"], expected)
                self.assertEqual(item["qualified"], expected)

    def test_block_and_anchor_110_percent_boundaries_use_exact_integer_inputs(self):
        for endpoint, expected in ((11000, True), (11001, False)):
            records = synthetic_records()
            for block in (0, 1):
                set_pair(records, block=block, first=endpoint, second=endpoint)
            item = cell(audit.summarize(records))
            self.assertEqual(item["checks"]["aa"], True)
            self.assertEqual(item["checks"]["block_dispersion"], expected)
            self.assertEqual(item["checks"]["anchor_drift"], expected)
            self.assertEqual(item["qualified"], expected)

    def test_metrics_preserve_batch_and_use_the_fixed_work_denominators(self):
        result = audit.summarize(synthetic_records(lambda slot, phase: 8192000))
        for condition, owners, _ in CONDITIONS:
            for phase in PHASES:
                metrics = cell(result, condition, phase)["metrics"]
                self.assertEqual(metrics["batch_ns"]["median"], 8192000.0)
                if phase in ("create", "destroy"):
                    self.assertEqual(set(metrics), {"batch_ns", "ns_per_owner"})
                    self.assertAlmostEqual(metrics["ns_per_owner"]["median"], 8192000 / owners)
                else:
                    self.assertEqual(set(metrics), {"batch_ns", "ns_per8192_request", "requests_per_second"})
                    self.assertAlmostEqual(metrics["ns_per8192_request"]["median"], 1000.0)
                    self.assertAlmostEqual(metrics["requests_per_second"]["median"], 1000000.0)

    def test_reference_c_ratios_and_median_min_max(self):
        def times(slot, phase):
            if slot["stage"] == "prerun" or slot["condition"] != "A":
                return 10000
            return (5000, 6000, 7000, 8000, 9000, 10000)[slot["block"]]

        result = audit.summarize(synthetic_records(times))
        actual = comparison(result)
        self.assertEqual(actual["ratios"], [0.5, 0.6, 0.7, 0.8, 0.9, 1.0])
        self.assertAlmostEqual(actual["median"], 0.75)
        self.assertEqual(actual["min"], 0.5)
        self.assertEqual(actual["max"], 1.0)
        self.assertFalse(actual["qualified"])
        # Dispersion makes A uncertain; describing all ratios must remain possible.
        self.assertEqual(len(actual["ratios"]), 6)

    def test_comparison_requires_both_case_and_reference_cells_qualified(self):
        for condition in ("A", "C"):
            records = synthetic_records()
            set_pair(records, condition=condition, phase="free", first=10000, second=12000)
            result = audit.summarize(records)
            self.assertFalse(comparison(result, "A", "free")["qualified"])
            self.assertTrue(comparison(result, "A", "create")["qualified"])
            self.assertEqual(len(result["cells"]), 24)
            self.assertEqual(len(result["comparisons"]), 24)

    def test_plus_minus_ten_percent_is_direction_unresolved_inclusively(self):
        for numerator, direction in ((8999, "lower"), (9000, "unresolved"),
                                     (10000, "unresolved"), (11000, "unresolved"), (11001, "higher")):
            with self.subTest(numerator=numerator):
                result = audit.summarize(synthetic_records(
                    lambda slot, phase: numerator if slot["condition"] == "A" else 10000))
                actual = comparison(result)
                self.assertTrue(actual["qualified"])
                self.assertEqual(actual["ratio_directions"], [direction] * 6)
                self.assertEqual(actual["median_direction"], direction)
                aggregate = {"lower": "all_lower", "higher": "all_higher", "unresolved": "unresolved"}[direction]
                self.assertEqual(actual["direction"], aggregate)

    def test_mixed_direction_retains_every_block_including_extreme(self):
        records = synthetic_records()
        for block, value in enumerate((8900, 9000, 10000, 11000, 11100, 1000000)):
            set_pair(records, block=block, first=value, second=value)
        result = audit.summarize(records)
        actual = comparison(result)
        self.assertEqual(actual["ratios"], [0.89, 0.9, 1.0, 1.1, 1.11, 100.0])
        self.assertEqual(actual["direction"], "mixed")
        self.assertFalse(actual["qualified"])
        self.assertEqual(cell(result)["sample_count"], 12)
        self.assertEqual(cell(result)["metrics"]["batch_ns"]["max"], 1000000.0)

    def test_nonuniform_central_pair_median_obeys_inclusive_direction_boundaries(self):
        for values, expected in (
                ((5000, 6000, 8000, 10000, 13000, 15000), "unresolved"),
                ((5000, 6000, 7999, 10000, 13000, 15000), "lower"),
                ((5000, 6000, 10000, 12000, 13000, 15000), "unresolved"),
                ((5000, 6000, 10000, 12001, 13000, 15000), "higher")):
            with self.subTest(values=values):
                records = synthetic_records()
                for block, value in enumerate(values):
                    set_pair(records, block=block, first=value, second=value)
                actual = comparison(audit.summarize(records))
                self.assertEqual(actual["median_direction"], expected)
                self.assertEqual(len(actual["ratios"]), 6)

    def test_output_has_no_confidence_interval_bootstrap_percentile_or_tail_metric(self):
        result = audit.summarize(synthetic_records())

        def keys(value):
            if isinstance(value, dict):
                for key, child in value.items():
                    yield key
                    yield from keys(child)
            elif isinstance(value, list):
                for child in value:
                    yield from keys(child)

        forbidden = {"confidence_interval", "ci", "bootstrap", "p95", "p99", "tail_latency",
                     "outliers", "outlier_removal", "lookup_latency", "pure_registry_cost"}
        self.assertFalse(forbidden & set(keys(result)))


class CollectorAuthorityTests(unittest.TestCase):
    """No mocked action is invoked: these exercise offline authority gates."""

    def setUp(self):
        SCRATCH.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="authority-", dir=SCRATCH)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.proposal_root = self.root / "proposal"
        self.proposal_root.mkdir()
        collector.write(self.proposal_root / "proposal.json", {"synthetic": "approval-test-only"}, exclusive=True)
        self.proposal = dict(run_id="synthetic-proposal-run",
                             python={"executable": "/synthetic/python"},
                             producer=dict(binding_sha256="1" * 64, parent_acceptance_sha256="2" * 64),
                             execution=dict(repository=str(REPO),
                                            collector=str(self.proposal_root / "tools/collect_owner_measurement.py"),
                                            proposal=str(self.proposal_root / "proposal.json"),
                                            approval=str(self.root / "approval.json"),
                                            queue_receipt=str(self.root / "receipt"),
                                            window=str(self.root / "window")))
        self.approval = dict(schema="owner-measurement-approval-v1", decision="SYNTHETIC-WINDOW-APPROVAL",
                             status="approved", approved_by="parent", native_execution_authorized=True,
                             producer_functional_parent_accepted=True,
                             proposal_sha256=collector.sha(self.proposal_root / "proposal.json"),
                             design_sha256=collector.DESIGN_SHA,
                             producer_binding_sha256="1" * 64,
                             producer_parent_acceptance_sha256="2" * 64,
                             run_id="synthetic-proposal-run", formal_execution_role="W3", max_windows=1)

    def write_approval(self, approval=None):
        path = Path(self.proposal["execution"]["approval"])
        collector.write(path, self.approval if approval is None else approval)
        return path, collector.sha(path)

    def request(self):
        e = self.proposal["execution"]
        return dict(schema_version=1, run_id="synthetic-queue-run", owner="W3", kind="timing",
                    timeout_seconds=900, runner_sha256=collector.QUEUE_SHA,
                    lock_dir=str(REPO / "build/vm-coordination"), cwd=str(REPO), out=e["queue_receipt"],
                    command=collector.collector_argv(self.proposal, "a" * 64),
                    source_headers=dict(collector.BASELINE))

    def receipt(self):
        queue = load_module("owner_measurement_real_pure_queue_checker",
                            REPO / "experiments/vm_coordination/run_job.py")
        out = Path(self.proposal["execution"]["queue_receipt"])
        out.mkdir()
        request = self.request()
        result = dict(schema_version=1, run_id=request["run_id"], owner="W3", kind="timing",
                      status="complete", reason="command_exit", command_returncode=0,
                      started_at="synthetic-start", finished_at="synthetic-finish",
                      cleanup=dict(live=[], process_groups=[]))
        collector.write(out / "request.json", request, exclusive=True)
        collector.write(out / "result.json", result, exclusive=True)
        (out / "stdout.log").write_text("synthetic collector stdout\n")
        (out / "stderr.log").write_text("")
        collector.write(out / "manifest.json", dict(schema_version=1, files=queue.manifest(out)), exclusive=True)
        captured = dict(request=copy.deepcopy(request), metadata=dict(
            status="running", supervisor_pid=12345, run_id=request["run_id"], owner="W3",
            kind="timing", out=str(out), started_at="synthetic-start"))
        return queue, out, captured

    def assert_receipt_rejects(self, out, captured):
        with self.assertRaises(ValueError):
            collector.verify_receipt(out, self.proposal, "a" * 64, captured,
                                     queue_source=REPO / "experiments/vm_coordination/run_job.py")

    def test_exact_independent_parent_approval_is_required(self):
        path, digest = self.write_approval()
        with mock.patch.object(collector, "audit_proposal", return_value=self.proposal) as check:
            proposal, approved = collector.verify_approval(self.proposal_root, path, digest)
        check.assert_called_once_with(self.proposal_root, True)
        self.assertEqual(approved, self.approval)
        self.assertEqual(proposal, self.proposal)

    def test_wrong_approval_or_foreign_proposal_sha_is_rejected_before_tool_import(self):
        with mock.patch.object(collector, "audit_proposal") as proposal_audit:
            path, _ = self.write_approval()
            with self.assertRaisesRegex(ValueError, "approval SHA mismatch"):
                collector.verify_approval(self.proposal_root, path, "0" * 64)
            proposal_audit.assert_not_called()
            changed = copy.deepcopy(self.approval)
            changed["proposal_sha256"] = "f" * 64
            path, digest = self.write_approval(changed)
            with self.assertRaisesRegex(ValueError, "foreign proposal"):
                collector.verify_approval(self.proposal_root, path, digest)
            proposal_audit.assert_not_called()

    def test_execute_child_preserves_mocked_process_logs_exit_and_both_timeout_paths(self):
        argv = ["/synthetic/never-executed", "--requests", "8192"]
        raw_stdout, raw_stderr = b'{"synthetic":true}\n', b"synthetic stderr\n"
        for mode in ("normal", "nonzero", "pipe_timeout", "wait_timeout"):
            with self.subTest(mode=mode):
                directory = self.root / mode
                directory.mkdir()
                fake = SimpleNamespace(returncode=None)
                for name, raw in (("stdout", raw_stdout), ("stderr", raw_stderr)):
                    read_fd, write_fd = os.pipe()
                    os.write(write_fd, raw)
                    os.close(write_fd)
                    setattr(fake, name, os.fdopen(read_fd, "rb"))
                fake.poll = mock.Mock(side_effect=lambda: fake.returncode)

                def kill():
                    fake.returncode = -9

                def wait(timeout):
                    if mode == "wait_timeout" and fake.returncode is None:
                        raise collector.subprocess.TimeoutExpired(argv, timeout)
                    if fake.returncode is None:
                        fake.returncode = 7 if mode == "nonzero" else 0
                    return fake.returncode

                fake.kill = mock.Mock(side_effect=kill)
                fake.wait = mock.Mock(side_effect=wait)
                with mock.patch.object(collector.subprocess, "Popen", return_value=fake) as popen:
                    if mode == "pipe_timeout":
                        # First read sees the prewritten bytes; the next loop
                        # exceeds the bound.  No real clock wait or child runs.
                        with mock.patch.object(collector.time, "monotonic", side_effect=[0, 0, 31]):
                            with self.assertRaises(TimeoutError):
                                collector.execute_child(argv, directory)
                    elif mode == "wait_timeout":
                        with self.assertRaises(collector.subprocess.TimeoutExpired):
                            collector.execute_child(argv, directory)
                    elif mode == "nonzero":
                        with self.assertRaisesRegex(ValueError, "native child failed"):
                            collector.execute_child(argv, directory)
                    else:
                        collector.execute_child(argv, directory)
                popen.assert_called_once()
                self.assertEqual(popen.call_args.args, (argv,))
                self.assertEqual(popen.call_args.kwargs["env"], collector.NATIVE_ENV)
                self.assertEqual(popen.call_args.kwargs["stdout"], collector.subprocess.PIPE)
                self.assertEqual(popen.call_args.kwargs["stderr"], collector.subprocess.PIPE)
                self.assertTrue(callable(popen.call_args.kwargs["preexec_fn"]))
                command = collector.read(directory / "command.json")
                self.assertEqual(command["argv"], argv)
                self.assertEqual(command["environment"], collector.NATIVE_ENV)
                self.assertEqual(command["timeout_seconds"], 30)
                self.assertEqual(command["timed_out"], mode.endswith("timeout"))
                expected_rc = -9 if mode.endswith("timeout") else 7 if mode == "nonzero" else 0
                self.assertEqual(command["returncode"], expected_rc)
                self.assertIn("finished_at", command)
                self.assertEqual((directory / "stdout.json").read_bytes(), raw_stdout)
                self.assertEqual((directory / "stderr.log").read_bytes(), raw_stderr)
                self.assertTrue(fake.stdout.closed and fake.stderr.closed)
                if mode.endswith("timeout"):
                    fake.kill.assert_called_once_with()
                    self.assertEqual(fake.wait.call_args.kwargs["timeout"], 5)
                    self.assertIn("error_type", command)
                else:
                    fake.kill.assert_not_called()
                    self.assertGreater(fake.wait.call_args.kwargs["timeout"], 0)
                    self.assertLessEqual(fake.wait.call_args.kwargs["timeout"], 30)

    def test_dynamic_loader_overrides_fail_before_binding_queue_or_process(self):
        self.proposal["execution"]["collector"] = str(Path(collector.__file__).resolve())
        for name in ("DYLD_INSERT_LIBRARIES", "LD_PRELOAD"):
            with self.subTest(name=name), \
                    mock.patch.object(collector, "verify_approval", return_value=(self.proposal, self.approval)), \
                    mock.patch.object(collector, "python_identity", return_value=self.proposal["python"]), \
                    mock.patch.object(collector, "verify_binding") as binding, \
                    mock.patch.object(collector, "active_queue") as queue, \
                    mock.patch.object(collector.subprocess, "Popen") as popen, \
                    mock.patch.dict(collector.os.environ, {name: "/synthetic/forbidden"}, clear=True):
                with self.assertRaisesRegex(ValueError, "loader override environment"):
                    collector.collect(self.proposal_root, Path(self.proposal["execution"]["approval"]), "a" * 64)
                binding.assert_not_called()
                queue.assert_not_called()
                popen.assert_not_called()

    def test_approval_sha_wrong_fields_roles_and_bound_identity_fail(self):
        changes = (("status", "proposed"), ("approved_by", "W2"),
                   ("native_execution_authorized", False), ("producer_functional_parent_accepted", False),
                   ("proposal_sha256", "3" * 64), ("design_sha256", "3" * 64),
                   ("producer_binding_sha256", "3" * 64), ("producer_parent_acceptance_sha256", "3" * 64),
                   ("run_id", "foreign"), ("formal_execution_role", "W2"), ("max_windows", 2),
                   ("max_windows", True), ("decision", "D023"), ("decision", "D018"), ("decision", "D022"))
        with mock.patch.object(collector, "audit_proposal", return_value=self.proposal):
            path, digest = self.write_approval()
            with self.assertRaises(ValueError):
                collector.verify_approval(self.proposal_root, path, "0" * 64)
            for field, value in changes:
                with self.subTest(field=field, value=value):
                    approval = copy.deepcopy(self.approval)
                    approval[field] = value
                    path, digest = self.write_approval(approval)
                    with self.assertRaises(ValueError):
                        collector.verify_approval(self.proposal_root, path, digest)
            for change in ("extra", "missing"):
                approval = copy.deepcopy(self.approval)
                if change == "extra":
                    approval["informal_permission"] = True
                else:
                    del approval["producer_functional_parent_accepted"]
                path, digest = self.write_approval(approval)
                with self.assertRaises(ValueError):
                    collector.verify_approval(self.proposal_root, path, digest)

    def test_queue_request_exact_owner_timeout_lock_identity_and_argv(self):
        request = self.request()
        collector.verify_request(request, self.proposal, "a" * 64)
        for field, value in (("owner", "W2"), ("kind", "functional"), ("timeout_seconds", 901),
                             ("runner_sha256", "0" * 64), ("lock_dir", str(self.root / "isolated-lock")),
                             ("cwd", str(self.root)), ("out", str(self.root / "foreign")),
                             ("command", request["command"] + ["--extra"]),
                             ("source_headers", {}), ("run_id", "")):
            with self.subTest(field=field):
                changed = copy.deepcopy(request)
                changed[field] = value
                with self.assertRaises(ValueError):
                    collector.verify_request(changed, self.proposal, "a" * 64)

    def test_real_w4_offline_audit_and_d023_receipt_linkage(self):
        queue, out, captured = self.receipt()
        self.assertEqual(queue.audit(out)["status"], "audit_passed")
        result = collector.verify_receipt(out, self.proposal, "a" * 64, captured,
                                          queue_source=REPO / "experiments/vm_coordination/run_job.py")
        self.assertEqual(result["run_id"], "synthetic-queue-run")
        self.assertEqual(result["manifest_sha256"], collector.sha(out / "manifest.json"))
        self.assertEqual(result["request_sha256"], collector.sha(out / "request.json"))

    def test_receipt_raw_byte_integrity_is_checked_before_wrapper_linkage(self):
        queue, out, captured = self.receipt()
        (out / "stdout.log").write_text("tampered synthetic bytes\n")
        with self.assertRaises(ValueError):
            queue.audit(out)
        self.assert_receipt_rejects(out, captured)

    def test_receipt_self_consistent_but_wrong_work_or_failed_result_is_rejected(self):
        queue, out, captured = self.receipt()
        original_request = collector.read(out / "request.json")
        original_result = collector.read(out / "result.json")
        mutations = (("request", "owner", "W2"), ("request", "command", ["foreign"]),
                     ("request", "run_id", "foreign"), ("result", "owner", "W2"),
                     ("result", "kind", "functional"), ("result", "reason", "timeout"),
                     ("result", "status", "error"), ("result", "command_returncode", 1),
                     ("result", "cleanup", dict(live=[], process_groups=[123])))
        for kind, field, value in mutations:
            with self.subTest(kind=kind, field=field):
                request, result = copy.deepcopy(original_request), copy.deepcopy(original_result)
                (request if kind == "request" else result)[field] = value
                collector.write(out / "request.json", request)
                collector.write(out / "result.json", result)
                collector.write(out / "manifest.json", dict(schema_version=1, files=queue.manifest(out)))
                self.assert_receipt_rejects(out, captured)

    def test_foreign_captured_queue_is_rejected_despite_valid_receipt(self):
        queue, out, captured = self.receipt()
        for field, value in (("run_id", "foreign"), ("owner", "W2"),
                             ("status", "complete"), ("kind", "diagnostic"), ("out", "foreign")):
            changed = copy.deepcopy(captured)
            changed["metadata"][field] = value
            self.assert_receipt_rejects(out, changed)
        changed = copy.deepcopy(captured)
        changed["request"]["run_id"] = "foreign"
        self.assert_receipt_rejects(out, changed)

    def test_receipt_rejects_changed_queue_auditor_source(self):
        _, out, captured = self.receipt()
        fake = self.root / "queue-runner.py"
        fake.write_text("raise AssertionError('changed code must not be executed')\n")
        with self.assertRaises(ValueError):
            collector.verify_receipt(out, self.proposal, "a" * 64, captured, queue_source=fake)

    def test_unbound_template_is_reviewable_and_collect_cannot_start_a_child(self):
        out = self.root / "unbound-proposal"
        marker = REPO / "build/vm-owner-measurement/native-window.json"
        original_marker = marker.read_bytes() if marker.exists() else None
        with mock.patch.object(collector.subprocess, "Popen") as popen:
            proposal = collector.prepare(REPO, out)
            checked = collector.audit_proposal(out)
            self.assertEqual(proposal, checked)
            self.assertEqual(proposal["status"], "unapproved_template")
            self.assertIs(proposal["native_execution_authorized"], False)
            self.assertIs(proposal["producer_final_binding_pending"], True)
            with self.assertRaises(ValueError):
                collector.audit_proposal(out, True)
            with self.assertRaises(ValueError):
                collector.collect(out, self.root / "missing-approval.json", "0" * 64)
            popen.assert_not_called()
        self.assertEqual(marker.read_bytes() if marker.exists() else None, original_marker)

    def test_frozen_template_tool_design_schedule_and_completion_tampering_fail(self):
        original = self.root / "unbound-original"
        collector.prepare(REPO, original)
        for relative in ("tools/audit_owner_measurement.py", "tools/collect_owner_measurement.py",
                         "tools/queue-runner.py", "design.json", "schedule.json", "COMPLETE"):
            with self.subTest(relative=relative):
                out = self.root / ("tamper-" + relative.replace("/", "-"))
                shutil.copytree(original, out)
                path = out / relative
                path.write_bytes(path.read_bytes() + b"\nsynthetic tampering\n")
                with self.assertRaises(ValueError):
                    collector.audit_proposal(out)

    def test_producer_reference_wrong_sha_and_design_only_acceptance_fail(self):
        target = self.root / "reference.json"
        collector.write(target, {"status": "synthetic"}, exclusive=True)
        relative = target.relative_to(REPO).as_posix()
        self.assertEqual(collector.reference(REPO, {"path": relative, "sha256": collector.sha(target)}), target)
        with self.assertRaises(ValueError):
            collector.reference(REPO, {"path": relative, "sha256": "0" * 64})
        for unsafe in ("../foreign.json", "/foreign.json", "./relative.json"):
            with self.assertRaises(ValueError):
                collector.safe_relative(unsafe)
        for decision, status in (("D018", "accepted_design_implementation_dispatched"),
                                 ("D022", "erratum_accepted"),
                                 ("D023", "design_frozen_tool_implementation_authorized")):
            collector.write(target, dict(decision=decision, status=status))
            ref = dict(path=relative, sha256=collector.sha(target))
            binding = {"schema": "owner-measurement-producer-binding-v1",
                       **{field: ref for field in collector.REF_FIELDS}}
            with self.assertRaises(ValueError):
                collector.verify_binding(REPO, binding)

    def test_non_original_or_nonfunctional_build_cannot_bind(self):
        # Synthetic references reach the build-profile gate only.  They are not
        # a producer bundle, and no synthetic acceptance can authorize a child.
        parent_path = self.root / "synthetic-final-acceptance.json"
        build_path = self.root / "synthetic-build-result.json"
        other = self.root / "synthetic-unused-reference.json"
        collector.write(parent_path, dict(decision="SYNTHETIC", status="passed"), exclusive=True)
        collector.write(other, {"synthetic": True}, exclusive=True)
        for key, value in (("kind", "diagnostic"), ("status", "failed"),
                           ("timing_evidence", True), ("schema_version", 2)):
            with self.subTest(key=key):
                build = dict(schema_version=1, kind="build", status="passed", timing_evidence=False)
                build[key] = value
                collector.write(build_path, build)
                paths = {field: other for field in collector.REF_FIELDS}
                paths.update(parent_acceptance=parent_path, build_result=build_path)
                binding = {"schema": "owner-measurement-producer-binding-v1",
                           **{field: {"path": path.relative_to(REPO).as_posix(), "sha256": collector.sha(path)}
                              for field, path in paths.items()}}
                with self.assertRaises(ValueError):
                    collector.verify_binding(REPO, binding)


def synthetic_producer_authority():
    """Small in-memory D027 linkage graph; no real producer artifacts are read."""
    binding = {"schema": "owner-measurement-producer-binding-v1",
               **{name: dict(path="synthetic/" + name, sha256=str(index + 1) * 64)
                  for index, name in enumerate(collector.REF_FIELDS)}}
    binding["parent_acceptance"]["sha256"] = collector.D027_SHA
    binding["layout_correction"]["sha256"] = collector.D022_SHA
    binaries = dict(timing="a" * 64, diagnostic="b" * 64)
    source = "c" * 64
    failures = ("fail_allocate", "fail_create", "fail_verify", "partial_startup")
    cases = sorted(
        [profile + "-" + control for profile in ("timing", "diagnostic") for control in failures]
        + [f"{profile}-r{requests}-o{owners}-t{threads}"
           for profile, requests in (("diagnostic", 256), ("timing", 256), ("timing", 8192))
           for _, owners, threads in CONDITIONS])
    acceptance = dict(decision="D027", status="accepted_scoped_functionality_and_build",
                      workload_processes=26, successful_processes=18, expected_failure_processes=8,
                      duplicate_or_missing_cases=False, formal_timing_execution_authorized=False,
                      timing_evidence=False, checker_sha256=binding["checker"]["sha256"],
                      binaries=copy.deepcopy(binaries), harness_sha256=source, case_ids=cases,
                      reviewed_files_sha256={binding[name]["path"]: binding[name]["sha256"]
                                             for name in ("build_result", "functional_result", "toolkit_result", "recovery_result")})
    build = dict(binaries=copy.deepcopy(binaries), files={"common/owner_scaling.cpp": source})
    toolkit = dict(status="passed", kind="toolkit", decision="D022", timing_evidence=False,
                   build_result_sha256=binding["build_result"]["sha256"],
                   binaries=copy.deepcopy(binaries), source_sha256=source,
                   files={"audit_owner_scaling.py": binding["checker"]["sha256"],
                          "contract.json": binding["contract"]["sha256"],
                          "layout-correction.json": collector.D022_SHA})
    functional = dict(status="passed", timing_evidence=False, workload_processes=26,
                      successful_processes=18, expected_failure_processes=8, case_ids=list(cases),
                      **{name + "_sha256": binding[name]["sha256"]
                         for name in ("build_result", "checker", "toolkit_result", "recovery_result")})
    return binding, acceptance, build, functional, toolkit


class ProducerAuthorityTests(unittest.TestCase):
    def test_complete_synthetic_d027_graph_passes_without_mutation(self):
        graph = synthetic_producer_authority()
        original = copy.deepcopy(graph)
        self.assertEqual(len(graph[1]["case_ids"]), 26)
        with mock.patch.object(collector.subprocess, "Popen") as popen:
            self.assertIsNone(collector.producer_authority(*graph))
            popen.assert_not_called()
        self.assertEqual(graph, original)

    def test_source_binaries_checker_contract_and_layout_each_require_exact_linkage(self):
        mutations = (
            (2, ("files", "common/owner_scaling.cpp")), (4, ("source_sha256",)),
            (1, ("harness_sha256",)), (2, ("binaries", "timing")),
            (4, ("binaries", "diagnostic")), (1, ("binaries", "timing")),
            (0, ("checker", "sha256")), (1, ("checker_sha256",)),
            (4, ("files", "audit_owner_scaling.py")), (0, ("contract", "sha256")),
            (4, ("files", "contract.json")), (0, ("layout_correction", "sha256")),
            (4, ("files", "layout-correction.json")),
        )
        for index, path in mutations:
            with self.subTest(index=index, path=path):
                graph = synthetic_producer_authority()
                target = graph[index]
                for key in path[:-1]:
                    target = target[key]
                target[path[-1]] = "f" * 64
                with self.assertRaises(ValueError):
                    collector.producer_authority(*graph)

    def test_functional26_counts_cases_and_recovery_checker_links_are_exact(self):
        changes = (("status", "failed"), ("timing_evidence", True), ("workload_processes", 25),
                   ("successful_processes", 17), ("expected_failure_processes", 7),
                   ("build_result_sha256", "f" * 64), ("checker_sha256", "f" * 64),
                   ("toolkit_result_sha256", "f" * 64), ("recovery_result_sha256", "f" * 64))
        for field, value in changes:
            with self.subTest(field=field):
                graph = synthetic_producer_authority()
                graph[3][field] = value
                with self.assertRaises(ValueError):
                    collector.producer_authority(*graph)
        for error in ("missing", "duplicate", "extra", "foreign", "order"):
            with self.subTest(error=error):
                graph = synthetic_producer_authority()
                cases = graph[3]["case_ids"]
                if error == "missing":
                    cases.pop()
                elif error == "duplicate":
                    cases[-1] = cases[0]
                elif error == "extra":
                    cases.append("synthetic-foreign-case")
                elif error == "foreign":
                    cases[-1] = "synthetic-foreign-case"
                else:
                    cases[0], cases[1] = cases[1], cases[0]
                # Also reject jointly reported missing/duplicate/extra sets,
                # rather than relying only on mismatched case-list identity.
                if error in ("missing", "duplicate", "extra"):
                    graph[1]["case_ids"] = list(cases)
                with self.assertRaises(ValueError):
                    collector.producer_authority(*graph)

    def test_d027_authority_scope_and_every_reviewed_reference_are_required(self):
        changes = (("decision", "D018"), ("status", "accepted_design_implementation_dispatched"),
                   ("workload_processes", 27), ("successful_processes", 19),
                   ("expected_failure_processes", 9), ("duplicate_or_missing_cases", True),
                   ("formal_timing_execution_authorized", True), ("timing_evidence", True))
        for field, value in changes:
            with self.subTest(field=field):
                graph = synthetic_producer_authority()
                graph[1][field] = value
                with self.assertRaises(ValueError):
                    collector.producer_authority(*graph)
        graph = synthetic_producer_authority()
        graph[0]["parent_acceptance"]["sha256"] = "f" * 64
        with self.assertRaises(ValueError):
            collector.producer_authority(*graph)
        for name in ("build_result", "functional_result", "toolkit_result", "recovery_result"):
            for error in ("unreviewed_path", "unreviewed_hash", "missing_review"):
                with self.subTest(name=name, error=error):
                    graph = synthetic_producer_authority()
                    ref = graph[0][name]
                    if error == "unreviewed_path":
                        ref["path"] = "synthetic/unreviewed"
                    elif error == "unreviewed_hash":
                        ref["sha256"] = "f" * 64
                    else:
                        del graph[1]["reviewed_files_sha256"][ref["path"]]
                    with self.assertRaises(ValueError):
                        collector.producer_authority(*graph)
        for field, value in (("status", "failed"), ("kind", "build"), ("decision", "D018"),
                             ("timing_evidence", True), ("build_result_sha256", "f" * 64)):
            graph = synthetic_producer_authority()
            graph[4][field] = value
            with self.subTest(toolkit_field=field), self.assertRaises(ValueError):
                collector.producer_authority(*graph)


if __name__ == "__main__":
    unittest.main()
