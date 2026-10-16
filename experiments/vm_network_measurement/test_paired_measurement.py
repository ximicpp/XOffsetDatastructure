#!/usr/bin/env python3
"""Synthetic, stdlib-only protocol tests; never benchmark/performance evidence.

Run with PYTHONDONTWRITEBYTECODE=1 python3 experiments/vm_network_measurement/
test_paired_measurement.py. No fixture binary is executable or executed.
"""
from __future__ import annotations

import copy
import hashlib
import importlib.util
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
MODULE_PATH = Path(__file__).with_name("paired_measurement.py").resolve()
MODULE_SPEC = importlib.util.spec_from_file_location("paired_measurement_under_test", MODULE_PATH)
pm = importlib.util.module_from_spec(MODULE_SPEC)
MODULE_SPEC.loader.exec_module(pm)


def bytes_sha(data):
    return hashlib.sha256(data).hexdigest()


def set_value(mapping, keys, value):
    for key in keys[:-1]:
        mapping = mapping[key]
    mapping[keys[-1]] = value


def snapshot(root):
    """Include directory entries as well as every file's exact bytes."""
    result = {}
    for path in sorted(Path(root).rglob("*")):
        name = path.relative_to(root).as_posix()
        if path.is_symlink():
            result[name] = ("symlink", str(path.readlink()))
        elif path.is_dir():
            result[name] = ("directory",)
        else:
            result[name] = ("file", path.read_bytes(), path.stat().st_mtime_ns)
    return result


class PairedMeasurementTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="vm-paired-synthetic-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.originals = self.root / "originals"
        self.originals.mkdir()
        self.input_bytes = b"synthetic request bytes\x00\xff"
        self.output_bytes = b"synthetic response bytes\x00\xfe"
        payloads = {
            "bin/reference.stub": b"dummy non-executable synthetic producer identity\n",
            "source/manifest.json": b'{"kind":"synthetic source manifest"}\n',
            "toolchain/manifest.json": b'{"kind":"no compiler invoked"}\n',
            "protocol/paired_measurement.py": MODULE_PATH.read_bytes(),
            "data/" + bytes_sha(self.input_bytes): self.input_bytes,
            "data/" + bytes_sha(self.output_bytes): self.output_bytes,
        }
        self.artifacts = {}
        for name, data in payloads.items():
            path = self.originals / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            self.artifacts[name] = path
        self.spec = pm.template("network")
        self.spec.update(window_id="synthetic-unit-window", evidence_class="synthetic-framework-test", ready=True)
        self.spec["workloads"] = [dict(id="tiny", nominal_bytes=32, elements=1, iterations=2)]
        self.spec["comparisons"] = [dict(id="none-cold", left="F-none", right="F-cold", calibration="F-none")]
        self.spec["conditions"] = {k: v for k, v in self.spec["conditions"].items() if k in ("F-none", "F-cold")}
        self.spec["artifacts"] = {name: dict(sha256=bytes_sha(data), bytes=len(data)) for name, data in payloads.items()}
        self.spec["identities"] = {
            condition: dict(binary="bin/reference.stub", source_manifest="source/manifest.json",
                            toolchain_manifest="toolchain/manifest.json")
            for condition in self.spec["conditions"]
        }
        self.jobs = pm.schedule(self.spec)
        self.spec["expected_data"] = {"tiny": copy.deepcopy(self.report(self.jobs[0])["data_identity"])}
        self.spec["invocations"] = {job["id"]: self.receipt(index)["argv"] for index, job in enumerate(self.jobs)}

    def report(self, job, samples=None, outside=None):
        samples = list(samples) if samples is not None else [100] * job["iterations"]
        costs = {key: 0 for key in pm.OUTSIDE}
        if outside:
            costs.update(outside)
        return dict(
            schema_version=1, status="passed", job_id=job["id"], condition=job["condition"],
            profile="timing", phase_timers=False, iterations=job["iterations"],
            data_identity=dict(input_sha256=bytes_sha(self.input_bytes), output_sha256=bytes_sha(self.output_bytes),
                               input_bytes=len(self.input_bytes), output_bytes=len(self.output_bytes),
                               semantic_checksum="synthetic-exact-values-1", elements=1,
                               schema_id=self.spec["controls"]["schema_id"],
                               processing_id=self.spec["controls"]["processing_id"]),
            controls=copy.deepcopy(self.spec["controls"]), samples_ns=samples,
            primary_total_ns=sum(samples), outside_ns=costs,
            metric_boundary=self.spec["metric_boundary"], verified_iterations=job["iterations"],
            outputs_retained=True,
        )

    def receipt(self, index, **overrides):
        result = dict(returncode=0, timed_out=False,
                      argv=["artifacts/bin/reference.stub", "synthetic-framework-test", str(index)],
                      binary_sha256=self.spec["artifacts"]["bin/reference.stub"]["sha256"],
                      started_monotonic_ns=1_000_000 + index * 1000,
                      finished_monotonic_ns=1_000_500 + index * 1000)
        result.update(overrides)
        return result

    def new_bundle(self, name="bundle", complete=False):
        root = pm.create_bundle(self.root / name, self.spec, self.artifacts)
        if complete:
            for index, job in enumerate(self.jobs):
                pm.record_observation(root, job, pm.canonical(self.report(job)), b"", self.receipt(index))
            result = pm.seal(root)
            self.assertEqual(result["status"], "passed")
        return root

    def rewrite_json(self, path, value):
        Path(path).write_bytes(pm.canonical(value))

    def reseal_inventory(self, root):
        """Simulate an attacker rehashing files: semantic audit must still reject."""
        result = pm.read(root / "result.json")
        result["files"] = pm.inventory(root, ("result.json", "COMPLETE"))
        result["preregister_sha256"] = pm.sha(root / "preregister.json")
        self.rewrite_json(root / "result.json", result)
        (root / "COMPLETE").write_text(pm.sha(root / "result.json") + "\n")

    def assert_reject_spec(self, keys, value, bound=True):
        spec = copy.deepcopy(self.spec)
        set_value(spec, keys, value)
        with self.assertRaises(pm.ProtocolError):
            pm.validate_spec(spec, bound=bound)

    def test_tiny_fixture_is_bound_synthetic_only(self):
        self.assertIs(pm.validate_spec(self.spec, bound=True), self.spec)
        self.assertEqual(len(self.spec["workloads"]), 1)
        self.assertEqual(len(self.spec["comparisons"]), 1)
        for name, path in self.artifacts.items():
            self.assertEqual(pm.sha(path), self.spec["artifacts"][name]["sha256"])
        self.assert_reject_spec(("evidence_class",), "native")

    def test_schedule_exact_ids_order_abba_and_bracketing_calibrations(self):
        pairs = [("warmup", "warmup", n) for n in (1, 2)]
        pairs += [("calibration", "pre", n) for n in range(1, 5)]
        pairs += [("comparison", "measured", n) for n in range(1, 5)]
        pairs += [("calibration", "mid", n) for n in range(1, 5)]
        pairs += [("comparison", "measured", n) for n in range(5, 9)]
        pairs += [("calibration", "post", n) for n in range(1, 5)]
        self.assertEqual(len(self.jobs), 44)
        expected = []
        for phase, anchor, number in pairs:
            sides = ("left", "right") if number % 2 else ("right", "left")
            for position, side in enumerate(sides):
                expected.append(dict(id="j%06d" % len(expected), workload="tiny", comparison="none-cold",
                                     phase=phase, anchor=anchor, pair=number,
                                     order="AB" if number % 2 else "BA", side=side, position=position,
                                     condition="F-none" if phase == "calibration" or side == "left" else "F-cold",
                                     iterations=2))
        self.assertEqual(self.jobs, expected)
        measured = [j["condition"] for j in self.jobs if j["phase"] == "comparison"]
        self.assertEqual(measured, ["F-none", "F-cold", "F-cold", "F-none"] * 4)

    def test_window_ordinal_reverses_pair_order_and_calibration_has_no_comparison(self):
        spec = copy.deepcopy(self.spec)
        spec["window_ordinal"] = 2
        reversed_jobs = pm.schedule(spec)
        for index in range(0, len(self.jobs), 2):
            self.assertEqual([j["side"] for j in reversed_jobs[index:index + 2]],
                             list(reversed([j["side"] for j in self.jobs[index:index + 2]])))
        spec["purpose"] = "calibration"
        jobs = pm.schedule(spec)
        self.assertEqual(len(jobs), 28)
        self.assertFalse(any(j["phase"] == "comparison" for j in jobs))
        self.assertEqual({j["anchor"] for j in jobs if j["phase"] == "calibration"}, {"pre", "mid", "post"})

    def test_spec_rejects_missing_identity_unknown_profiles_and_bad_bounds(self):
        mutations = [
            (("schema_version",), True), (("schema_version",), 2), (("profile",), "diagnostic"),
            (("window_id",), "../window"), (("window_ordinal",), True), (("ready",), 1),
            (("ready",), False), (("baseline_headers",), {}), (("conditions",), {}),
            (("conditions", "F-none", "profile"), "diagnostic"),
            (("conditions", "F-cold", "pool"), "unknown"), (("workloads",), []),
            (("workloads", 0, "iterations"), True), (("workloads", 0, "elements"), -1),
            (("comparisons", 0, "right"), "F-none"), (("comparisons", 0, "calibration"), "missing"),
            (("identities", "F-none", "binary"), "missing"), (("identities",), {}),
            (("expected_data",), {}), (("expected_data", "tiny", "output_sha256"), "invalid"),
            (("invocations",), {}), (("invocations", "j000000"), []),
            (("artifacts",), {}), (("metric_boundary",), ""), (("primary_metric",), "primary_mean_ns"),
            (("controls", "max_wire_bytes"), True), (("controls", "owner_capacity_bytes"), 0),
            (("controls", "max_inflight_bytes"), -1), (("controls", "pool_idle_reserved_bytes"), 0),
        ]
        for keys, value in mutations:
            with self.subTest(keys=keys, value=value):
                self.assert_reject_spec(keys, value)

    def test_spec_rejects_policy_changes_boolean_and_nonfinite_budgets(self):
        mutations = [
            ("measured_pairs", 6), ("calibration_pairs_per_anchor", 2), ("warmup_pairs", 0),
            ("noise_relative_limit", True), ("noise_relative_limit", 0),
            ("noise_relative_limit", float("nan")), ("noise_relative_limit", float("inf")),
            ("dispersion_relative_limit", .06), ("dispersion_relative_limit", float("-inf")),
            ("minimum_confirmation_windows", 1), ("minimum_window_gap_seconds", 59),
            ("minimum_window_gap_seconds", float("inf")), ("retry_failed_calibration", True),
            ("adoption_gain_threshold", 0), ("default_regression_budget", .5),
            ("max_processes", True), ("max_window_seconds", 0), ("child_timeout_seconds", -1),
        ]
        for key, value in mutations:
            with self.subTest(key=key, value=value):
                self.assert_reject_spec(("policy", key), value)
        spec = copy.deepcopy(self.spec)
        spec["policy"]["max_processes"] = len(self.jobs) - 1
        with self.assertRaises(pm.ProtocolError):
            pm.schedule(spec)

    def test_spec_rejects_duplicate_workloads_comparisons_and_traversal_artifacts(self):
        spec = copy.deepcopy(self.spec)
        spec["workloads"] *= 2
        with self.assertRaises(pm.ProtocolError):
            pm.validate_spec(spec)
        spec = copy.deepcopy(self.spec)
        spec["comparisons"] *= 2
        with self.assertRaises(pm.ProtocolError):
            pm.validate_spec(spec)
        for path in ("../escape", "/absolute", "nested/../escape", "nested//file", "./file", ".", "windows\\file",
                     "C:/drive", "name:stream", "nested/CON", "NUL.txt", "nested/COM1", "LPT9.txt",
                     "trailing.", "trailing ", "control\x00byte", "a?b", "a*b", "a|b",
                     "COM¹", "LPT²", "CONIN$", "CONOUT$"):
            with self.subTest(path=path):
                spec = copy.deepcopy(self.spec)
                spec["artifacts"][path] = dict(sha256="a" * 64, bytes=1)
                with self.assertRaises(pm.ProtocolError):
                    pm.validate_spec(spec)

    def test_report_rejects_bool_nonfinite_and_inconsistent_samples(self):
        job = self.jobs[0]
        mutations = [
            (("schema_version",), True), (("iterations",), True), (("verified_iterations",), True),
            (("samples_ns",), [True, 100]), (("samples_ns",), [float("nan"), 100]),
            (("samples_ns",), [float("inf"), 100]), (("samples_ns",), [0, 100]),
            (("samples_ns",), [100]), (("primary_total_ns",), True), (("primary_total_ns",), 199),
            (("data_identity", "input_bytes"), True), (("data_identity", "output_bytes"), 0),
            (("data_identity", "elements"), True), (("outside_ns", "owner_release_ns"), True),
            (("outside_ns", "final_clear_ns"), -1), (("outside_ns", "other_excluded_ns"), float("inf")),
        ]
        for keys, value in mutations:
            with self.subTest(keys=keys, value=value):
                report = self.report(job)
                set_value(report, keys, value)
                with self.assertRaises(pm.ProtocolError):
                    pm.validate_report(self.spec, job, report)

    def test_report_rejects_wrong_profile_identity_limits_and_outside_costs(self):
        job = self.jobs[0]
        mutations = [
            (("status",), "failed"), (("job_id",), "j999999"), (("condition",), "F-cold"),
            (("profile",), "diagnostic"), (("phase_timers",), True), (("phase_timers",), 0),
            (("data_identity", "input_sha256"), "not-a-sha"),
            (("data_identity", "schema_id"), "different-schema"),
            (("data_identity", "processing_id"), "different-processing"),
            (("data_identity", "semantic_checksum"), ""), (("data_identity", "elements"), 2),
            (("data_identity", "input_bytes"), self.spec["controls"]["max_wire_bytes"] + 1),
            (("data_identity", "output_bytes"), self.spec["controls"]["max_wire_bytes"] + 1),
            (("controls", "connection"), "new connection per request"),
            (("controls", "owner_capacity_bytes"), self.spec["controls"]["owner_capacity_bytes"] * 2),
            (("controls", "max_inflight_bytes"), float(self.spec["controls"]["max_inflight_bytes"])),
            (("outside_ns", "cold_clear_ns"), 1), (("outside_ns", "warmup_ns"), 1),
            (("metric_boundary",), "stop after transmit"), (("outputs_retained",), False),
        ]
        for keys, value in mutations:
            with self.subTest(keys=keys, value=value):
                report = self.report(job)
                set_value(report, keys, value)
                with self.assertRaises(pm.ProtocolError):
                    pm.validate_report(self.spec, job, report)
        for category in ("data_identity", "outside_ns"):
            report = self.report(job)
            report[category].pop(next(iter(report[category])))
            with self.assertRaises(pm.ProtocolError):
                pm.validate_report(self.spec, job, report)
            report = self.report(job)
            report[category]["unregistered"] = 0
            with self.assertRaises(pm.ProtocolError):
                pm.validate_report(self.spec, job, report)

    def test_cold_cleanup_can_reverse_primary_speedup_and_all_costs_remain_visible(self):
        reports = {}
        for job in self.jobs:
            cold = job["condition"] == "F-cold"
            reports[job["id"]] = self.report(job, [80, 80] if cold else [100, 100],
                                            {"cold_clear_ns": 60, "buffer_destruction_ns": 20} if cold else None)
        summary = pm.summarize(self.spec, self.jobs, reports)
        metrics = summary["rows"][0]["comparison_statistics"]["metrics"]
        self.assertEqual(metrics["primary_mean_ns"]["median_left_over_right"], 1.25)
        self.assertAlmostEqual(metrics["reclamation_adjusted_mean_ns"]["median_left_over_right"], 100 / 110)
        self.assertAlmostEqual(metrics["declared_lifecycle_mean_ns"]["median_left_over_right"], 100 / 120)
        report = self.report(self.jobs[0], outside={key: index + 1 for index, key in enumerate(pm.OUTSIDE)})
        values = pm.means(report)
        expected_reclaimed = 100 + sum(report["outside_ns"][key] for key in ("cold_clear_ns", "final_clear_ns", "owner_release_ns")) / 2
        self.assertEqual(values["reclamation_adjusted_mean_ns"], expected_reclaimed)
        self.assertEqual(values["declared_lifecycle_mean_ns"], 100 + sum(report["outside_ns"].values()) / 2)
        self.assertFalse(summary["adoption_allowed"])

    def test_symmetric_noisy_aa_fails_even_when_median_and_order_strata_are_one(self):
        reports = {j["id"]: self.report(j) for j in self.jobs}
        for job in self.jobs:
            if job["phase"] == "calibration" and job["side"] == "left":
                reports[job["id"]] = self.report(job, [80, 80] if job["pair"] <= 2 else [120, 120])
        summary = pm.summarize(self.spec, self.jobs, reports)
        self.assertFalse(summary["all_aa_qualified"])
        for anchor in summary["rows"][0]["calibration"].values():
            metric = anchor["statistics"]["metrics"]["reclamation_adjusted_mean_ns"]
            self.assertEqual(metric["median_left_over_right"], 1)
            self.assertEqual(metric["order_stratum_medians"], {"AB": 1, "BA": 1})
            self.assertFalse(anchor["qualified"])

    def test_late_calibration_failure_invalidates_whole_window(self):
        reports = {j["id"]: self.report(j) for j in self.jobs}
        for job in self.jobs:
            if job["anchor"] == "post" and job["side"] == "right":
                reports[job["id"]] = self.report(job, [125, 125])
        summary = pm.summarize(self.spec, self.jobs, reports)
        anchors = summary["rows"][0]["calibration"]
        self.assertTrue(anchors["pre"]["qualified"])
        self.assertTrue(anchors["mid"]["qualified"])
        self.assertFalse(anchors["post"]["qualified"])
        self.assertFalse(summary["all_aa_qualified"])
        self.assertFalse(summary["adoption_allowed"])

    def test_summarize_rejects_missing_extra_reordered_jobs_and_data_drift(self):
        reports = {j["id"]: self.report(j) for j in self.jobs}
        for jobs, candidate in ((self.jobs[:-1], reports), (list(reversed(self.jobs)), reports),
                                (self.jobs, {k: v for k, v in reports.items() if k != self.jobs[0]["id"]}),
                                (self.jobs, dict(reports, extra=reports[self.jobs[0]["id"]]))):
            with self.subTest(kind="job-set", jobs=len(jobs), reports=len(candidate)):
                with self.assertRaises(pm.ProtocolError):
                    pm.summarize(self.spec, jobs, candidate)
        for key, value in (("output_sha256", "a" * 64), ("semantic_checksum", "different-exact-values")):
            with self.subTest(identity=key):
                candidate = copy.deepcopy(reports)
                candidate[self.jobs[-1]["id"]]["data_identity"][key] = value
                with self.assertRaises(pm.ProtocolError):
                    pm.summarize(self.spec, self.jobs, candidate)

    def test_sealed_bundle_relocates_and_audit_does_not_touch_any_bytes_or_files(self):
        root = self.new_bundle(complete=True)
        destination = self.root / "relocated" / "archive"
        destination.parent.mkdir()
        shutil.move(str(root), str(destination))
        shutil.rmtree(self.originals)
        before = snapshot(destination)
        for _ in range(2):
            result = pm.audit(destination)
            self.assertEqual(result, dict(status="passed", evidence_class="synthetic-framework-test",
                                          processes=44, all_aa_qualified=True, adoption_allowed=False))
        self.assertEqual(snapshot(destination), before)

    def test_inventory_detects_unresealed_byte_tampering(self):
        root = self.new_bundle(complete=True)
        path = root / "commands/j000000.stdout"
        path.write_bytes(path.read_bytes() + b" ")
        before = snapshot(root)
        with self.assertRaisesRegex(pm.ProtocolError, "inventory"):
            pm.audit(root)
        self.assertEqual(snapshot(root), before)

    def test_inventory_validates_every_archive_filename_and_directory(self):
        # A filename added outside the declared artifact map must be rejected
        # before hashing, including names that cannot survive Windows relocation.
        for index, name in enumerate(("a?b", "a*b", "a|b", "COM¹", "LPT²", "CONIN$", "CONOUT$")):
            for entry_type in ("file", "directory"):
                with self.subTest(name=name, entry_type=entry_type):
                    root = self.new_bundle(name="invalid-name-%d-%s" % (index, entry_type), complete=True)
                    path = root / "artifacts" / name
                    if entry_type == "directory":
                        path.mkdir()
                        (path / "ordinary.txt").write_bytes(b"synthetic bytes")
                    else:
                        path.write_bytes(b"synthetic bytes")
                    before = snapshot(root)
                    with self.assertRaises(pm.ProtocolError):
                        pm.inventory(root)
                    with self.assertRaises(pm.ProtocolError):
                        pm.audit(root)
                    self.assertEqual(snapshot(root), before)

    def test_resealed_summary_rejects_boolean_to_integer_substitution(self):
        for field, replacement in (("all_aa_qualified", 1), ("adoption_allowed", 0)):
            with self.subTest(field=field):
                root = self.new_bundle(name="summary-bool-" + field, complete=True)
                path = root / "summary.json"
                summary = pm.read(path)
                self.assertIs(type(summary[field]), bool)
                summary[field] = replacement
                self.rewrite_json(path, summary)
                self.reseal_inventory(root)
                before = snapshot(root)
                with self.assertRaisesRegex(pm.ProtocolError, "summary does not recompute"):
                    pm.audit(root)
                self.assertEqual(snapshot(root), before)

    def test_inventory_rejects_boolean_metadata_alias_after_result_rehash(self):
        root = self.new_bundle(complete=True)
        result = pm.read(root / "result.json")
        self.assertEqual(result["files"]["commands/j000000.stderr"]["bytes"], 0)
        result["files"]["commands/j000000.stderr"]["bytes"] = False
        self.rewrite_json(root / "result.json", result)
        (root / "COMPLETE").write_text(pm.sha(root / "result.json") + "\n")
        before = snapshot(root)
        with self.assertRaisesRegex(pm.ProtocolError, "inventory/hash mismatch"):
            pm.audit(root)
        self.assertEqual(snapshot(root), before)

    def test_collect_rejects_costs_exceeding_child_duration_but_accepts_exact_boundary(self):
        for mode in ("primary-only", "primary-plus-outside"):
            with self.subTest(mode=mode):
                root = self.new_bundle(name="excess-duration-" + mode, complete=True)
                path = root / "commands/j000000.stdout"
                report = pm.read(path)
                if mode == "primary-only":
                    report["samples_ns"] = [251, 251]
                    report["primary_total_ns"] = 502
                else:
                    # 200 ns primary + 301 ns disjoint excluded costs exceeds
                    # the fixture child's 500 ns enclosing monotonic interval.
                    report["outside_ns"]["owner_release_ns"] = 100
                    report["outside_ns"]["buffer_destruction_ns"] = 201
                self.rewrite_json(path, report)
                self.reseal_inventory(root)
                before = snapshot(root)
                with self.assertRaisesRegex(pm.ProtocolError, "exceed enclosing child duration"):
                    pm.collect(root)
                with self.assertRaisesRegex(pm.ProtocolError, "exceed enclosing child duration"):
                    pm.audit(root)
                self.assertEqual(snapshot(root), before)
        root = self.new_bundle(name="exact-duration-boundary")
        for index, job in enumerate(self.jobs):
            report = self.report(job, outside={"owner_release_ns": 300} if index == 0 else None)
            pm.record_observation(root, job, pm.canonical(report), b"", self.receipt(index))
        self.assertEqual(pm.seal(root)["status"], "passed")

    def test_tamper_resealed_inventory_still_validates_reports_deeply(self):
        mutations = [
            (("samples_ns",), [True, 100]), (("primary_total_ns",), 201),
            (("profile",), "diagnostic"), (("phase_timers",), True),
            (("data_identity", "semantic_checksum"), "changed-values"),
            (("controls", "connection"), "different-connection"), (("outside_ns", "owner_release_ns"), -1),
            (("metric_boundary",), "send-complete-only"), (("verified_iterations",), 0),
        ]
        for index, (keys, value) in enumerate(mutations):
            with self.subTest(keys=keys):
                root = self.new_bundle(name="deep-report-%d" % index, complete=True)
                path = root / "commands/j000000.stdout"
                report = pm.read(path)
                set_value(report, keys, value)
                self.rewrite_json(path, report)
                self.reseal_inventory(root)
                before = snapshot(root)
                with self.assertRaises(pm.ProtocolError):
                    pm.audit(root)
                self.assertEqual(snapshot(root), before)

    def test_resealed_inventory_cannot_hide_schedule_summary_or_artifact_changes(self):
        for target in ("schedule", "summary", "artifact", "missing-full-output"):
            with self.subTest(target=target):
                root = self.new_bundle(name="deep-" + target, complete=True)
                if target == "schedule":
                    value = pm.read(root / "schedule.json")
                    value[0], value[1] = value[1], value[0]
                    self.rewrite_json(root / "schedule.json", value)
                elif target == "summary":
                    value = pm.read(root / "summary.json")
                    value["adoption_allowed"] = True
                    self.rewrite_json(root / "summary.json", value)
                elif target == "artifact":
                    (root / "artifacts/bin/reference.stub").write_bytes(b"changed dummy binary")
                else:
                    spec = pm.read(root / "preregister.json")
                    name = "data/" + bytes_sha(self.output_bytes)
                    del spec["artifacts"][name]
                    (root / "artifacts" / name).unlink()
                    self.rewrite_json(root / "preregister.json", spec)
                self.reseal_inventory(root)
                with self.assertRaises(pm.ProtocolError):
                    pm.audit(root)

    def test_resealed_all_reports_cannot_replace_preregistered_exact_data_identity(self):
        # Internal agreement alone is insufficient: all observations must agree
        # with the data/semantics frozen before any observation was collected.
        for target in ("semantic-checksum", "output-bytes"):
            with self.subTest(target=target):
                root = self.new_bundle(name="all-data-" + target, complete=True)
                for job in self.jobs:
                    path = root / "commands" / (job["id"] + ".stdout")
                    report = pm.read(path)
                    if target == "semantic-checksum":
                        report["data_identity"]["semantic_checksum"] = "other-consistent-synthetic-values"
                    else:
                        report["data_identity"]["output_sha256"] = bytes_sha(self.input_bytes)
                        report["data_identity"]["output_bytes"] = len(self.input_bytes)
                    self.rewrite_json(path, report)
                self.reseal_inventory(root)
                before = snapshot(root)
                with self.assertRaises(pm.ProtocolError):
                    pm.audit(root)
                self.assertEqual(snapshot(root), before)

    def test_resealed_protocol_artifact_still_requires_the_exact_audit_implementation(self):
        root = self.new_bundle(complete=True)
        path = root / "artifacts/protocol/paired_measurement.py"
        path.write_bytes(path.read_bytes() + b"\n# synthetic tampered protocol source\n")
        spec = pm.read(root / "preregister.json")
        spec["artifacts"]["protocol/paired_measurement.py"] = dict(sha256=pm.sha(path), bytes=path.stat().st_size)
        self.rewrite_json(root / "preregister.json", spec)
        self.reseal_inventory(root)
        with self.assertRaisesRegex(pm.ProtocolError, "frozen protocol"):
            pm.audit(root)

    def test_resealed_receipts_reject_bool_identity_overlap_and_execution_bounds(self):
        changes = [
            ("returncode", False), ("timed_out", True), ("argv", []), ("binary_sha256", "a" * 64),
            ("argv", ["different-stub", "different-flags"]),
            ("started_monotonic_ns", True), ("finished_monotonic_ns", 1),
            ("finished_monotonic_ns", 1_000_000 + (self.spec["policy"]["child_timeout_seconds"] + 1) * 10**9),
        ]
        for index, (key, value) in enumerate(changes):
            with self.subTest(key=key, value=value):
                root = self.new_bundle(name="deep-receipt-%d" % index, complete=True)
                path = root / "commands/j000000.receipt.json"
                receipt = pm.read(path)
                receipt[key] = value
                self.rewrite_json(path, receipt)
                self.reseal_inventory(root)
                with self.assertRaises(pm.ProtocolError):
                    pm.audit(root)
        root = self.new_bundle(name="overlap", complete=True)
        path = root / "commands/j000001.receipt.json"
        receipt = pm.read(path)
        receipt["started_monotonic_ns"] = 1_000_001
        self.rewrite_json(path, receipt)
        self.reseal_inventory(root)
        with self.assertRaises(pm.ProtocolError):
            pm.audit(root)
        root = self.new_bundle(name="boolean-job-alias", complete=True)
        path = root / "commands/j000000.receipt.json"
        receipt = pm.read(path)
        receipt["job"]["pair"] = True
        self.rewrite_json(path, receipt)
        self.reseal_inventory(root)
        with self.assertRaises(pm.ProtocolError):
            pm.audit(root)

    def test_resealed_receipts_reject_total_window_bound(self):
        root = self.new_bundle(complete=True)
        gap = (self.spec["policy"]["max_window_seconds"] + 1) * 10**9 // (len(self.jobs) - 1) + 1
        for index, job in enumerate(self.jobs):
            path = root / "commands" / (job["id"] + ".receipt.json")
            receipt = pm.read(path)
            receipt["started_monotonic_ns"] = 1_000_000 + index * gap
            receipt["finished_monotonic_ns"] = receipt["started_monotonic_ns"] + 500
            self.rewrite_json(path, receipt)
        self.reseal_inventory(root)
        with self.assertRaisesRegex(pm.ProtocolError, "window time"):
            pm.audit(root)

    def test_failed_producer_preserves_exact_binary_outputs_and_read_only_failed_audit(self):
        root = self.new_bundle()
        stdout, stderr = b"invalid-json\x00\xff\n", b"failure details\x00\xfe\n"
        with self.assertRaisesRegex(pm.ProtocolError, "raw evidence retained"):
            pm.record_observation(root, self.jobs[0], stdout, stderr, self.receipt(0, returncode=7))
        self.assertEqual((root / "commands/j000000.stdout").read_bytes(), stdout)
        self.assertEqual((root / "commands/j000000.stderr").read_bytes(), stderr)
        result = pm.mark_failed(root, "synthetic producer failure")
        self.assertEqual(result["status"], "failed")
        before = snapshot(root)
        self.assertEqual(pm.audit(root), dict(status="failed_preserved", adoption_allowed=False,
                                             error="synthetic producer failure"))
        self.assertEqual(snapshot(root), before)
        self.assertFalse((root / "COMPLETE").exists())

    def test_invalid_success_stdout_and_timeout_preserve_failure_evidence(self):
        cases = [(b'{"status": "passed", "status": "failed"}', self.receipt(0)),
                 (b"NaN", self.receipt(0)), (b"timeout partial bytes", self.receipt(0, timed_out=True))]
        for index, (stdout, receipt) in enumerate(cases):
            with self.subTest(index=index):
                root = self.new_bundle(name="invalid-output-%d" % index)
                with self.assertRaises(pm.ProtocolError):
                    pm.record_observation(root, self.jobs[0], stdout, b"stderr", receipt)
                self.assertEqual((root / "commands/j000000.stdout").read_bytes(), stdout)
                self.assertTrue((root / "commands/j000000.receipt.json").is_file())

    def test_duplicate_json_keys_and_nonfinite_constants_rejected_at_every_depth(self):
        for data in (b'{"a": 1, "a": 2}', b'{"nested": {"a": 1, "a": 2}}',
                     b'{"x": NaN}', b'{"x": Infinity}', b'{"x": -Infinity}', b'{"x": 1e999}'):
            with self.subTest(data=data):
                with self.assertRaises(pm.ProtocolError):
                    pm.parse(data)
        root = self.new_bundle(complete=True)
        path = root / "commands/j000000.stdout"
        raw = path.read_bytes()
        path.write_bytes(b'{"status":"passed",' + raw.lstrip()[1:])
        self.reseal_inventory(root)
        with self.assertRaisesRegex(pm.ProtocolError, "duplicate JSON key"):
            pm.audit(root)

    def test_symlinks_rejected_for_source_archive_and_ancestor_paths(self):
        link = self.root / "source-link"
        link.symlink_to(self.artifacts["bin/reference.stub"])
        artifacts = dict(self.artifacts, **{"bin/reference.stub": link})
        with self.assertRaises(pm.ProtocolError):
            pm.create_bundle(self.root / "source-symlink-bundle", self.spec, artifacts)
        root = self.new_bundle(complete=True)
        (root / "unexpected-link").symlink_to(self.artifacts["bin/reference.stub"])
        with self.assertRaisesRegex(pm.ProtocolError, "symlink"):
            pm.audit(root)
        ancestor = self.root / "ancestor-link"
        ancestor.symlink_to(self.originals, target_is_directory=True)
        with self.assertRaisesRegex(pm.ProtocolError, "symlink"):
            pm.safe_file(self.root, "ancestor-link/bin/reference.stub")
        root_link = self.root / "bundle-link"
        root_link.symlink_to(root, target_is_directory=True)
        with self.assertRaisesRegex(pm.ProtocolError, "unsafe evidence root"):
            pm.inventory(root_link)

    def test_existing_bundle_and_recorded_output_are_never_replaced(self):
        root = self.new_bundle()
        pm.record_observation(root, self.jobs[0], pm.canonical(self.report(self.jobs[0])), b"original stderr", self.receipt(0))
        before = snapshot(root)
        with self.assertRaises(FileExistsError):
            pm.create_bundle(root, self.spec, self.artifacts)
        with self.assertRaises(pm.ProtocolError):
            pm.record_observation(root, self.jobs[0], b"replacement", b"replacement", self.receipt(0))
        self.assertEqual(snapshot(root), before)

    def test_sealed_bundle_rejects_more_output_reseal_or_failure_reclassification(self):
        root = self.new_bundle(complete=True)
        before = snapshot(root)
        with self.assertRaises(pm.ProtocolError):
            pm.record_observation(root, self.jobs[0], b"replacement", b"", self.receipt(0))
        with self.assertRaises(FileExistsError):
            pm.seal(root)
        with self.assertRaises(pm.ProtocolError):
            pm.mark_failed(root, "replacement")
        self.assertEqual(snapshot(root), before)

    def test_missing_extra_and_out_of_order_collection_are_rejected(self):
        root = self.new_bundle()
        with self.assertRaises(pm.ProtocolError):
            pm.record_observation(root, self.jobs[1], pm.canonical(self.report(self.jobs[1])), b"", self.receipt(1))
        self.assertEqual(list((root / "commands").iterdir()), [])
        with self.assertRaises(pm.ProtocolError):
            pm.seal(root)
        complete = self.new_bundle(name="extra-command", complete=True)
        (complete / "commands/unexpected.stdout").write_bytes(b"extra")
        self.reseal_inventory(complete)
        with self.assertRaisesRegex(pm.ProtocolError, "command evidence"):
            pm.audit(complete)


if __name__ == "__main__":
    unittest.main(verbosity=2)
