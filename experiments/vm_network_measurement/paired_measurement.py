#!/usr/bin/env python3
"""Preregister, schedule and audit paired windows without executing a benchmark.

This module has no compiler/network/process launcher. A reviewed adapter must
collect normalized producer stdout under one accepted run_job.py timing lease.
Its self-tests are synthetic protocol tests, never native timing evidence.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import math
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import shutil
import statistics
import sys

sys.dont_write_bytecode = True
BASELINE = {
    "xoffsetdatastructure.hpp": "9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2",
    "xoffset_virtual_memory.hpp": "22f181b52c11ce442a5576ffc0dc35af1828f77deb4424f387d0acfa658239ad",
}
OUTSIDE = ("cold_clear_ns", "final_clear_ns", "pool_prepare_ns", "warmup_ns",
           "connection_setup_ns", "owner_release_ns", "buffer_destruction_ns", "other_excluded_ns")
IDENTITY_KEYS = ("input_sha256", "output_sha256", "input_bytes", "output_bytes",
                 "semantic_checksum", "elements", "schema_id", "processing_id")


class ProtocolError(ValueError):
    pass


def require(value, message):
    if not value:
        raise ProtocolError(message)


def integer(value, minimum=0):
    return type(value) is int and value >= minimum


def digest(value):
    return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{64}", value) is not None


def identifier(value):
    return isinstance(value, str) and re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,95}", value) is not None


def sha(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def canonical(value):
    return (json.dumps(value, sort_keys=True, indent=2, allow_nan=False) + "\n").encode()


def value_sha(value):
    return hashlib.sha256(canonical(value)).hexdigest()


def _pairs(items):
    result = {}
    for key, value in items:
        require(key not in result, "duplicate JSON key: " + key)
        result[key] = value
    return result


def parse(data):
    def invalid(value):
        raise ProtocolError("nonfinite JSON constant: " + value)
    def finite(value):
        number = float(value)
        require(math.isfinite(number), "nonfinite JSON number: " + value)
        return number
    return json.loads(data, object_pairs_hook=_pairs, parse_constant=invalid, parse_float=finite)


def read(path):
    return parse(Path(path).read_bytes())


def write_new(path, value):
    with Path(path).open("xb") as stream:
        stream.write(canonical(value))


def safe_relative(value):
    require(isinstance(value, str) and value and not any(c in value for c in '\\:<>"|?*')
            and not any(ord(c) < 32 for c in value), "invalid relative path")
    path = PurePosixPath(value)
    require(bool(path.parts) and not path.is_absolute() and value == path.as_posix()
            and all(p not in (".", "..") for p in path.parts), "unsafe relative path")
    require(not PureWindowsPath(value).drive and all(not re.fullmatch(r"(?i:CON|PRN|AUX|NUL|CONIN\$|CONOUT\$|COM[1-9¹²³]|LPT[1-9¹²³])", p.split(".", 1)[0].rstrip(" "))
            and not p.endswith((" ", ".")) for p in path.parts), "unsafe Windows relative path")
    return path


def safe_file(root, relative):
    path = Path(root)
    for part in safe_relative(relative).parts:
        path /= part
        require(not path.is_symlink(), "symlink in evidence path")
    require(path.is_file(), "missing evidence file: " + relative)
    return path


def inventory(root, excluded=()):
    root = Path(root)
    require(root.is_dir() and not root.is_symlink(), "unsafe evidence root")
    result = {}
    for path in sorted(root.rglob("*")):
        safe_relative(path.relative_to(root).as_posix())
        require(not path.is_symlink(), "symlink in archive")
        if path.is_file():
            name = path.relative_to(root).as_posix()
            if name not in excluded:
                result[name] = {"sha256": sha(path), "bytes": path.stat().st_size}
        else:
            require(path.is_dir(), "nonregular archive entry")
    return result


def template(profile="network"):
    require(profile in ("network", "default"), "unknown profile")
    if profile == "network":
        workloads = [dict(id=str(b), nominal_bytes=b, elements=b // 4, iterations=n)
                     for b, n in ((4096, 32), (65536, 16), (1048576, 4), (16777216, 1))]
        comparisons = [dict(id="none-cold", left="F-none", right="F-cold", calibration="F-none"),
                       dict(id="none-warm", left="F-none", right="F-warm", calibration="F-none")]
        conditions = {"F-" + p: dict(variant="F", pool=p, profile="timing") for p in ("none", "cold", "warm")}
        boundary = "client request start through full response verified load and exact value validation"
    else:
        workloads = [dict(id=i, nominal_bytes=b, elements=e, iterations=1) for i, b, e in
                     (("save_first_64m", 67108864, 0), ("save_repeated_64m", 67108864, 0),
                      ("vector_tail_1m", 4000000, 1000000))]
        comparisons = [dict(id="D-F", left="D-none", right="F-none", calibration="F-none")]
        conditions = {v + "-none": dict(variant=v, pool="none", profile="timing") for v in ("D", "F")}
        boundary = "historical first save / repeated save / tail append exact harness boundary; adapter approval pending"
    return dict(schema_version=1, profile=profile, window_id="UNBOUND", window_ordinal=1,
                purpose="screening", evidence_class="unbound", ready=False,
                baseline_headers=BASELINE, conditions=conditions, workloads=workloads,
                comparisons=comparisons, artifacts={}, identities={}, expected_data={}, invocations={}, metric_boundary=boundary,
                controls=dict(connection="one persistent TCP loopback connection, one in-flight request" if profile == "network" else "single-thread fixed historical harness",
                              max_wire_bytes=33554432, owner_capacity_bytes=33554432,
                              max_inflight_bytes=67108864, pool_idle_reserved_bytes=67108864,
                              schema_id="file-reference-vector-string-v1", processing_id="deterministic-reference-v1"),
                policy=dict(measured_pairs=8, calibration_pairs_per_anchor=4, warmup_pairs=2,
                            noise_relative_limit=.05 if profile == "network" else .01,
                            dispersion_relative_limit=.05 if profile == "network" else .01,
                            minimum_confirmation_windows=3, minimum_window_gap_seconds=60,
                            adoption_gain_threshold=.05, default_regression_budget=.02,
                            max_processes=400, max_window_seconds=900, child_timeout_seconds=180,
                            retry_failed_calibration=False),
                primary_metric="reclamation_adjusted_mean_ns",
                exclusions=["connection setup", "pool preparation", "warmup", "buffer destruction", "other explicitly reported exclusions"],
                limits=["synthetic reference only", "no production p99", "no business benefit or adoption from one window"])


def validate_spec(spec, bound=False):
    require(isinstance(spec, dict) and type(spec.get("schema_version")) is int and spec["schema_version"] == 1, "unknown schema")
    require(spec.get("profile") in ("network", "default"), "unknown profile")
    require(identifier(spec.get("window_id")) and integer(spec.get("window_ordinal"), 1), "invalid window identity")
    require(spec.get("baseline_headers") == BASELINE, "baseline identity differs")
    require(spec.get("purpose") in ("calibration", "screening", "confirmation"), "invalid purpose")
    require(spec.get("evidence_class") in ("unbound", "synthetic-framework-test", "native"), "invalid evidence class")
    require(type(spec.get("ready")) is bool, "invalid readiness")
    conditions = spec.get("conditions")
    require(isinstance(conditions, dict) and bool(conditions), "empty conditions")
    for name, condition in conditions.items():
        require(identifier(name) and isinstance(condition, dict) and identifier(condition.get("variant")), "bad condition")
        require(condition.get("pool") in ("none", "cold", "warm") and condition.get("profile") == "timing", "diagnostic in timing window")
    workloads = spec.get("workloads")
    require(isinstance(workloads, list) and bool(workloads), "empty workloads")
    names = []
    for work in workloads:
        require(isinstance(work, dict) and identifier(work.get("id")), "bad workload")
        names.append(work["id"])
        require(integer(work.get("nominal_bytes"), 1) and integer(work.get("elements")) and integer(work.get("iterations"), 1), "invalid workload bounds")
    require(len(names) == len(set(names)), "duplicate workload")
    comparisons = spec.get("comparisons")
    require(isinstance(comparisons, list) and bool(comparisons), "empty comparisons")
    names = []
    for comp in comparisons:
        require(isinstance(comp, dict) and identifier(comp.get("id")), "bad comparison")
        names.append(comp["id"])
        require(all(comp.get(k) in conditions for k in ("left", "right", "calibration")), "unknown comparison condition")
        require(comp["left"] != comp["right"], "comparison must differ")
    require(len(names) == len(set(names)), "duplicate comparison")
    policy = spec.get("policy", {})
    require(policy.get("measured_pairs") == 8 and policy.get("calibration_pairs_per_anchor") == 4
            and policy.get("warmup_pairs") == 2, "pair policy differs")
    for key in ("noise_relative_limit", "dispersion_relative_limit"):
        require(type(policy.get(key)) in (int, float) and math.isfinite(policy[key]) and 0 < policy[key] <= .05, "invalid noise budget")
    require(policy.get("minimum_confirmation_windows") == 3 and integer(policy.get("minimum_window_gap_seconds"), 60)
            and policy.get("retry_failed_calibration") is False, "confirmation/retry policy differs")
    require(policy.get("adoption_gain_threshold") == .05 and policy.get("default_regression_budget") == .02, "adoption budget differs")
    for key in ("max_processes", "max_window_seconds", "child_timeout_seconds"):
        require(integer(policy.get(key), 1), "invalid execution bound")
    require(spec.get("primary_metric") == "reclamation_adjusted_mean_ns" and isinstance(spec.get("metric_boundary"), str)
            and bool(spec["metric_boundary"]), "missing metric boundary")
    controls = spec.get("controls", {})
    for key in ("connection", "schema_id", "processing_id"):
        require(isinstance(controls.get(key), str) and bool(controls[key]), "missing control")
    for key in ("max_wire_bytes", "owner_capacity_bytes", "max_inflight_bytes", "pool_idle_reserved_bytes"):
        require(integer(controls.get(key), 1), "missing hard budget")
    artifacts = spec.get("artifacts")
    require(isinstance(artifacts, dict), "invalid artifact map")
    for name, item in artifacts.items():
        safe_relative(name)
        require(isinstance(item, dict) and digest(item.get("sha256")) and integer(item.get("bytes")), "invalid artifact identity")
    if bound:
        require(spec["ready"] is True and spec["evidence_class"] != "unbound", "unbound preregistration")
        require(spec["evidence_class"] == "synthetic-framework-test",
                "native adapter is not bound/accepted yet; generic protocol tests only")
        require(bool(artifacts), "missing frozen artifacts")
        require("protocol/paired_measurement.py" in artifacts, "missing frozen protocol source")
        require(set(spec.get("expected_data", {})) == {w["id"] for w in workloads}, "missing preregistered workload data")
        for work in workloads:
            validate_data_identity(spec, work, spec["expected_data"][work["id"]])
        invocations = spec.get("invocations", {})
        require(isinstance(invocations, dict) and set(invocations) == {j["id"] for j in schedule(spec)}, "missing frozen invocations")
        for argv in invocations.values():
            require(isinstance(argv, list) and argv and all(isinstance(a, str) and a for a in argv), "invalid frozen argv")
        identities = spec.get("identities", {})
        require(set(identities) == set(conditions), "missing condition identities")
        for name, identity in identities.items():
            require(isinstance(identity, dict) and identity.get("binary") in artifacts
                    and identity.get("source_manifest") in artifacts
                    and identity.get("toolchain_manifest") in artifacts, "unfrozen execution identity")
        if spec["evidence_class"] == "native":
            require(spec.get("approved_contract") in artifacts and spec.get("approved_gate") in artifacts,
                    "native execution requires frozen parent-approved contract/gate")
    return spec


def schedule(spec):
    validate_spec(spec)
    jobs = []
    def pair(work, comp, phase, anchor, n):
        order = "AB" if (n + spec["window_ordinal"]) % 2 == 0 else "BA"
        for position, side in enumerate(("left", "right") if order == "AB" else ("right", "left")):
            condition = comp["calibration"] if phase == "calibration" else comp[side]
            jobs.append(dict(id=f"j{len(jobs):06d}", workload=work["id"], comparison=comp["id"],
                             phase=phase, anchor=anchor, pair=n, order=order, side=side,
                             position=position, condition=condition, iterations=work["iterations"]))
    for work in spec["workloads"]:
        for comp in spec["comparisons"]:
            for n in range(1, 3):
                pair(work, comp, "warmup", "warmup", n)
            for anchor, indices in (("pre", range(1, 5)), ("mid", range(5, 9)), ("post", ())):
                for n in range(1, 5):
                    pair(work, comp, "calibration", anchor, n)
                if spec["purpose"] != "calibration":
                    for n in indices:
                        pair(work, comp, "comparison", "measured", n)
    require(len(jobs) <= spec["policy"]["max_processes"], "process budget exceeded")
    return jobs


def validate_report(spec, job, report):
    require(isinstance(report, dict) and type(report.get("schema_version")) is int
            and report["schema_version"] == 1 and report.get("status") == "passed", "producer did not pass")
    require(report.get("job_id") == job["id"] and report.get("condition") == job["condition"], "wrong report condition")
    require(report.get("profile") == "timing" and report.get("phase_timers") is False, "phase probes in primary timing")
    require(type(report.get("iterations")) is int and report["iterations"] == job["iterations"], "wrong iteration count")
    identity = report.get("data_identity", {})
    work = next(w for w in spec["workloads"] if w["id"] == job["workload"])
    validate_data_identity(spec, work, identity)
    require(identity == spec.get("expected_data", {}).get(job["workload"]), "data differs from preregistered workload identity")
    require(canonical(report.get("controls")) == canonical(spec["controls"]), "comparison controls differ")
    samples = report.get("samples_ns")
    require(isinstance(samples, list) and len(samples) == job["iterations"]
            and all(integer(n, 1) for n in samples), "invalid raw samples")
    require(integer(report.get("primary_total_ns"), 1) and report["primary_total_ns"] == sum(samples), "primary sum differs")
    outside = report.get("outside_ns", {})
    require(set(outside) == set(OUTSIDE) and all(integer(n) for n in outside.values()), "incomplete outside costs")
    pool = spec["conditions"][job["condition"]]["pool"]
    require(pool == "cold" or outside["cold_clear_ns"] == 0, "unexpected cold cleanup")
    require(pool == "warm" or outside["warmup_ns"] == 0, "unexpected warmup")
    require(report.get("metric_boundary") == spec["metric_boundary"], "metric boundary differs")
    require(report.get("verified_iterations") == job["iterations"] and type(report["verified_iterations"]) is int,
            "missing exact-value validation")
    require(report.get("outputs_retained") is True, "output evidence must be retained")
    return report


def validate_data_identity(spec, work, identity):
    require(isinstance(identity, dict), "invalid data identity")
    require(set(identity) == set(IDENTITY_KEYS), "incomplete semantic/data identity")
    for key in ("input_sha256", "output_sha256"):
        require(digest(identity[key]), "invalid data hash")
    for key in ("input_bytes", "output_bytes"):
        require(integer(identity[key], 1) and identity[key] <= spec["controls"]["max_wire_bytes"], "wire size exceeds budget")
    require(integer(identity["elements"]) and identity["elements"] == work["elements"], "element count differs")
    require(isinstance(identity["semantic_checksum"], str) and bool(identity["semantic_checksum"]), "empty checksum")
    for key in ("schema_id", "processing_id"):
        require(identity[key] == spec["controls"][key], "schema/processing differs")


def means(report):
    n = report["iterations"]
    raw = report["primary_total_ns"] / n
    reclaimed = raw + sum(report["outside_ns"][k] for k in ("cold_clear_ns", "final_clear_ns", "owner_release_ns")) / n
    lifecycle = raw + sum(report["outside_ns"].values()) / n
    return dict(primary_mean_ns=raw, reclamation_adjusted_mean_ns=reclaimed,
                declared_lifecycle_mean_ns=lifecycle)


def paired_stats(jobs, reports):
    require(len(jobs) >= 4 and len(jobs) % 2 == 0, "incomplete pair group")
    pairs = []
    for offset in range(0, len(jobs), 2):
        a, b = jobs[offset:offset + 2]
        require(a["pair"] == b["pair"] and a["side"] != b["side"] and a["position"] == 0 and b["position"] == 1,
                "missing/duplicate/reordered pair")
        left = reports[a["id"]] if a["side"] == "left" else reports[b["id"]]
        right = reports[b["id"]] if b["side"] == "right" else reports[a["id"]]
        lm, rm = means(left), means(right)
        pairs.append(dict(pair=a["pair"], order=a["order"], left=lm, right=rm,
                          ratios={key: lm[key] / rm[key] for key in lm}))
    result = dict(pairs=pairs, metrics={})
    for metric in pairs[0]["ratios"]:
        values = [p["ratios"][metric] for p in pairs]
        logs = [math.log(v) for v in values]
        median_log = statistics.median(logs)
        strata = {order: statistics.median(p["ratios"][metric] for p in pairs if p["order"] == order) for order in ("AB", "BA")}
        result["metrics"][metric] = dict(median_left_over_right=statistics.median(values),
            range_left_over_right=[min(values), max(values)], order_stratum_medians=strata,
            median_absolute_log_ratio=statistics.median(abs(v) for v in logs),
            log_mad=statistics.median(abs(v - median_log) for v in logs),
            log_iqr=quantile(logs, .75) - quantile(logs, .25))
    return result


def quantile(values, q):
    values = sorted(values)
    p = (len(values) - 1) * q
    i = int(p)
    return values[i] + (values[min(i + 1, len(values) - 1)] - values[i]) * (p - i)


def calibration_pass(stats, policy):
    # Both location and spread are checked; a symmetric noisy distribution must fail.
    metric = stats["metrics"]["reclamation_adjusted_mean_ns"]
    bound = math.log1p(policy["noise_relative_limit"])
    spread = math.log1p(policy["dispersion_relative_limit"])
    return (all(abs(math.log(v)) <= bound for v in [metric["median_left_over_right"], *metric["order_stratum_medians"].values()])
            and metric["median_absolute_log_ratio"] <= bound and metric["log_mad"] <= spread
            and metric["log_iqr"] <= 2 * spread)


def summarize(spec, jobs, reports):
    require(canonical(jobs) == canonical(schedule(spec)) and set(reports) == {j["id"] for j in jobs}, "exact job set/order differs")
    identities = {}
    for job in jobs:
        report = validate_report(spec, job, reports[job["id"]])
        identity = report["data_identity"]
        require(identity == identities.setdefault(job["workload"], identity), "same-workload data/semantics differ")
    rows = []
    for work in spec["workloads"]:
        for comp in spec["comparisons"]:
            selected = [j for j in jobs if j["workload"] == work["id"] and j["comparison"] == comp["id"]]
            anchors = {}
            for anchor in ("pre", "mid", "post"):
                group = [j for j in selected if j["phase"] == "calibration" and j["anchor"] == anchor]
                stats = paired_stats(group, reports)
                anchors[anchor] = dict(qualified=calibration_pass(stats, spec["policy"]), statistics=stats)
            group = [j for j in selected if j["phase"] == "comparison"]
            rows.append(dict(workload=work["id"], comparison=comp["id"],
                             aa_qualified=all(a["qualified"] for a in anchors.values()),
                             calibration=anchors, comparison_statistics=paired_stats(group, reports) if group else None))
    return dict(schema_version=1, evidence_class=spec["evidence_class"], purpose=spec["purpose"],
                process_count=len(jobs), ratio_direction="left / right; greater than one means right faster",
                rows=rows, all_aa_qualified=all(r["aa_qualified"] for r in rows),
                adoption_allowed=False, confirmation_windows_required=3,
                reason="single window; synthetic framework tests cannot establish performance" if spec["evidence_class"] != "native"
                       else "single preregistered window; independent confirmation and parent review required")


def create_bundle(root, spec, artifacts):
    """Freeze a new bundle before collection; no path is ever replaced."""
    validate_spec(spec, bound=True)
    require(set(artifacts) == set(spec["artifacts"]), "frozen artifact set differs")
    require(spec["artifacts"]["protocol/paired_measurement.py"]["sha256"] == sha(__file__),
            "executed protocol source differs from frozen protocol")
    root = Path(root)
    root.mkdir(parents=True, exist_ok=False)
    write_new(root / "preregister.json", spec)
    write_new(root / "schedule.json", schedule(spec))
    (root / "artifacts").mkdir()
    (root / "commands").mkdir()
    for name, original in artifacts.items():
        expected = spec["artifacts"][name]
        require(Path(original).is_file() and not Path(original).is_symlink(), "unsafe source artifact")
        require(sha(original) == expected["sha256"] and Path(original).stat().st_size == expected["bytes"], "source artifact changed")
        target = root / "artifacts" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        with Path(original).open("rb") as src, target.open("xb") as dst:
            shutil.copyfileobj(src, dst)
    return root


def record_observation(root, job, stdout, stderr, receipt):
    """Record one producer receipt before validation, preserving failed stdout."""
    root = Path(root)
    require(not (root / "result.json").exists(), "bundle already sealed")
    spec = read(root / "preregister.json")
    jobs = schedule(spec)
    recorded = sorted((root / "commands").glob("*.receipt.json"))
    require(len(recorded) < len(jobs) and canonical(job) == canonical(jobs[len(recorded)]), "unexpected/repeated execution")
    require(isinstance(stdout, bytes) and isinstance(stderr, bytes), "raw output must be bytes")
    target = root / "commands" / job["id"]
    for suffix, data in (("stdout", stdout), ("stderr", stderr)):
        with target.with_suffix("." + suffix).open("xb") as stream:
            stream.write(data)
    write_new(target.with_suffix(".receipt.json"), dict(job=job, **receipt))
    require(receipt.get("returncode") == 0 and type(receipt.get("returncode")) is int
            and receipt.get("timed_out") is False, "failed producer; raw evidence retained")
    report = parse(stdout)
    validate_report(spec, job, report)
    return report


def validate_receipt(spec, job, receipt, previous_end):
    require(canonical(receipt.get("job")) == canonical(job) and receipt.get("returncode") == 0 and type(receipt.get("returncode")) is int
            and receipt.get("timed_out") is False, "unsuccessful/mismatched receipt")
    argv = receipt.get("argv")
    require(isinstance(argv, list) and argv and all(isinstance(a, str) and a for a in argv), "missing exact argv")
    require(argv == spec["invocations"][job["id"]], "executed argv differs from preregistration")
    require(receipt.get("binary_sha256") == spec["artifacts"][spec["identities"][job["condition"]]["binary"]]["sha256"], "receipt binary attribution differs")
    start, end = receipt.get("started_monotonic_ns"), receipt.get("finished_monotonic_ns")
    require(integer(start, 1) and integer(end, start) and start >= previous_end, "overlapping/reordered process intervals")
    require(end - start <= spec["policy"]["child_timeout_seconds"] * 10**9, "child timeout bound exceeded")
    return end


def collect(root):
    root = Path(root)
    spec = validate_spec(read(root / "preregister.json"), bound=True)
    jobs = schedule(spec)
    require(canonical(read(root / "schedule.json")) == canonical(jobs), "schedule changed")
    reports, previous_end, first_start = {}, 0, None
    expected_names = {j["id"] + "." + ext for j in jobs for ext in ("stdout", "stderr", "receipt.json")}
    require(set(p.name for p in (root / "commands").iterdir()) == expected_names, "missing/extra command evidence")
    for job in jobs:
        prefix = "commands/" + job["id"]
        receipt = read(safe_file(root, prefix + ".receipt.json"))
        previous_end = validate_receipt(spec, job, receipt, previous_end)
        if first_start is None:
            first_start = receipt["started_monotonic_ns"]
        safe_file(root, prefix + ".stderr")
        reports[job["id"]] = parse(safe_file(root, prefix + ".stdout").read_bytes())
        report = validate_report(spec, job, reports[job["id"]])
        wall = receipt["finished_monotonic_ns"] - receipt["started_monotonic_ns"]
        require(report["primary_total_ns"] + sum(report["outside_ns"].values()) <= wall,
                "declared disjoint timing costs exceed enclosing child duration")
    require(previous_end - first_start <= spec["policy"]["max_window_seconds"] * 10**9, "window time budget exceeded")
    for name, identity in spec["artifacts"].items():
        path = safe_file(root, "artifacts/" + name)
        require(sha(path) == identity["sha256"] and path.stat().st_size == identity["bytes"], "frozen artifact changed")
    require(spec["artifacts"]["protocol/paired_measurement.py"]["sha256"] == sha(__file__),
            "audit with the exact frozen protocol source")
    for job in jobs:
        data = reports[job["id"]]["data_identity"]
        # Full bytes remain in a deduplicated content-addressed archive. Each
        # producer validates each iteration and returns its exact output digest.
        for direction in ("input", "output"):
            name = "data/" + data[direction + "_sha256"]
            require(name in spec["artifacts"] and spec["artifacts"][name]["sha256"] == data[direction + "_sha256"]
                    and spec["artifacts"][name]["bytes"] == data[direction + "_bytes"], "missing retained full data/output bytes")
    return spec, jobs, reports


def seal(root):
    root = Path(root)
    spec, jobs, reports = collect(root)
    summary = summarize(spec, jobs, reports)
    write_new(root / "summary.json", summary)
    result = dict(status="completed", kind="paired-window", preregister_sha256=sha(root / "preregister.json"),
                  evidence_class=spec["evidence_class"], files=inventory(root, ("result.json", "COMPLETE")),
                  adoption_allowed=False)
    write_new(root / "result.json", result)
    with (root / "COMPLETE").open("x") as stream:
        stream.write(sha(root / "result.json") + "\n")
    return audit(root)


def mark_failed(root, error):
    root = Path(root)
    require(not (root / "result.json").exists() and not (root / "COMPLETE").exists(), "cannot replace sealed evidence")
    result = dict(status="failed", kind="paired-window", error=str(error), adoption_allowed=False,
                  files=inventory(root, ("result.json", "COMPLETE")))
    write_new(root / "result.json", result)
    return result


def audit(root):
    """Read-only offline verification. Does not access original build paths."""
    root = Path(root)
    result = read(safe_file(root, "result.json"))
    require(result.get("kind") == "paired-window" and result.get("adoption_allowed") is False, "wrong result kind/adoption claim")
    require(canonical(inventory(root, ("result.json", "COMPLETE"))) == canonical(result.get("files")), "archive inventory/hash mismatch")
    if result.get("status") == "failed":
        require(not (root / "COMPLETE").exists() and bool(result.get("error")), "failed bundle has completion claim")
        return dict(status="failed_preserved", adoption_allowed=False, error=result["error"])
    require(result.get("status") == "completed", "incomplete bundle")
    require(safe_file(root, "COMPLETE").read_text().strip() == sha(root / "result.json"), "completion identity differs")
    require(sha(root / "preregister.json") == result.get("preregister_sha256"), "preregistration changed")
    spec, jobs, reports = collect(root)
    require(result.get("evidence_class") == spec["evidence_class"], "evidence class changed")
    summary = summarize(spec, jobs, reports)
    require(canonical(read(root / "summary.json")) == canonical(summary), "summary does not recompute")
    return dict(status="passed", evidence_class=spec["evidence_class"], processes=len(jobs),
                all_aa_qualified=summary["all_aa_qualified"], adoption_allowed=False)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="action", required=True)
    p = commands.add_parser("template")
    p.add_argument("--profile", choices=("network", "default"), required=True)
    p.add_argument("--out", type=Path, required=True)
    p = commands.add_parser("schedule")
    p.add_argument("spec", type=Path)
    p = commands.add_parser("audit")
    p.add_argument("path", type=Path)
    args = parser.parse_args(argv)
    if args.action == "template":
        write_new(args.out, template(args.profile))
    elif args.action == "schedule":
        print(json.dumps(schedule(read(args.spec)), indent=2))
    else:
        print(json.dumps(audit(args.path), sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, TypeError, StopIteration) as error:
        print("measurement protocol rejected: " + str(error), file=sys.stderr)
        sys.exit(1)
