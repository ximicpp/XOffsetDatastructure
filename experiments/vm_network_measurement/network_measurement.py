#!/usr/bin/env python3
"""W1 revision-2 gated collector, raw adapter and read-only window audit.

The parent creates approval.json *after* accepting build/functionality and the
fully bound preregistration. Its digest must be supplied out of band to seal or
audit. Root collects the entire schedule inside one accepted W4 timing job.
collect-window launches only exact approved W1 invocations after the native
gate and active timing lease pass. Audits/normalization never execute producers.
A normalized report or synthetic test cannot establish native timing/adoption.
"""
from __future__ import annotations

import argparse
import base64
import datetime as dt
import hashlib
import math
import os
import platform
from pathlib import Path
import re
import statistics
import subprocess
import sys
import time
import types

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import paired_measurement as P

CONTRACT_SHA = "496791aaf988cfe3aa72a8cdc0d120e2e0384434f8fc1bf628a2e79223191718"
SUBCASES_SHA = "e3c0d57e1b92a1f63ffdbb684967656c64f8c1813f401b393ce110344964d9b8"
QUEUE_SHA = "6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909"
PRODUCTION_QUEUE = "/Users/fanchensu/XOffsetDatastructure/build/vm-coordination"
MAX_RETAINED_OUTPUT = 2 * 1024**3
G1_SHA = "f2dbaa41ae10f331562f5b89d9ec381523d169e172c09ca85ec3487b093bd599"
G1_BINARY_SHA = "5cf8a18e11ab38676cd7a1791556233b239b8791c9b4e665642f0092dcec7f18"
G1_BUILD_SHA = "41a9eda112b0f396226d08ec1b5712ea619dfbc4fe00b28d4fbf73326d59e0cc"
G1_SOURCE_SHA = "d4235d100ca1687e7834ad0383d20864b19347dc254e09b6ae2a2649b9617dcf"
C53_FUNCTIONAL_PROTOCOL_SHA = "c53e484f9459cf0dc4e8d4e33c6b582fd8b4fa316c04023638b238d1dfa6dc59"
C53_PAIRED_PROTOCOL_SHA = "40627ac7cdad4ebec10428e4d8a74e83b790509341411e6def162c526204003a"
MAX_PAYLOAD = 0x7FFFFFF8
CHUNK = 65536
PRIMARY_BOUNDARY = "client immediately before request send through complete response receive, load_verified and exact marker/count/label/each-value verification"
PHASE_FIELDS = ("request_send_ns", "server_receive_ns", "server_load_verified_ns", "server_process_ns",
                "server_save_verified_ns", "server_owner_release_ns", "response_send_ns",
                "response_receive_ns", "client_load_verified_ns", "client_validate_ns")
RECLAMATION = ("cold_clear_outside_ns", "final_clear_outside_ns", "client_owner_release_outside_ns",
               "server_completion_wait_outside_ns")
OUTSIDE = ("pool_prepare_outside_ns", "connection_setup_outside_ns", "warmup_outside_ns", *RECLAMATION)
UNMEASURED = ("client response string destruction", "immutable client request string destruction",
              "persistent worker creation and final join", "pool/connection destruction",
              "input read/preflight/checksum", "output file writes", "process startup/JSON serialization/scheduler gaps")
PRODUCER_KEYS = ("binary_sha256", "source_inventory_sha256", "toolchain_sha256", "dependencies_sha256")
ENVIRONMENT_POLICY = dict(schema_version=1, collection="window edges only; outside producer timers", max_probe_seconds=3,
                          max_retained_bytes_per_stream=1024**2,
                          commands=dict(processes=["/bin/ps", "-axo", "pid=,ppid=,pcpu=,comm="],
                                        cpu=["/usr/sbin/sysctl", "hw.ncpu", "hw.logicalcpu", "hw.physicalcpu", "hw.cpufrequency"],
                                        power=["/usr/bin/pmset", "-g", "batt"], thermal=["/usr/bin/pmset", "-g", "therm"]))
RUN_FIELDS = set(("schema_version", "contract_version", "status", "command", "diagnostic", "timing_evidence",
                  "pool", "iterations", "element_count", "semantic_checksum", "input_bytes_per_iteration",
                  "output_bytes_per_iteration", "max_wire_bytes", "owner_capacity_bytes", "max_inflight_bytes",
                  "reserved_inflight_peak_bytes", "connection_count", "max_inflight_requests", "timeout_ms",
                  "end_to_end_ns", "pool_measured_hits", "pool_measured_misses", "pool_warmup_hits",
                  "pool_warmup_misses", "pool_cached_blocks_before_final_clear",
                  "pool_cached_reserved_bytes_before_final_clear", "pool_cached_accessible_bytes_before_final_clear",
                  "samples", "output_files", *OUTSIDE))
SAMPLE_FIELDS = set(("end_to_end_ns", "client_owner_release_outside_ns", "server_completion_wait_outside_ns",
                     "input_bytes", "output_bytes", *PHASE_FIELDS))

# Proposed runtime approval schema. No document is implicitly approved by this
# template. accepted_inputs is a complete {archive-relative-name: {sha256,bytes}}
# map of contract/, protocol/, w1-build/, w1-functional/, and data/. Parent first
# independently audits W1 archives, then pins their complete inventories here.
APPROVAL_SCHEMA = {
    "schema": "vm-network-parent-measurement-approval", "schema_version": 1,
    "status": "approved", "native_execution_authorized": True,
    "evidence_class": "native-posix-tcp-loopback", "preregister_sha256": "<digest>",
    "contract_sha256": CONTRACT_SHA, "model_subcases_sha256": SUBCASES_SHA,
    "runner_sha256": QUEUE_SHA, "accepted_inputs": "<complete frozen input inventory>",
    "build_result_sha256": "<accepted timing build result digest>",
    "functional_result_sha256": "<accepted timing smoke result digest>",
    "window_id": "<unique preregistered window id>",
    "execution_root": "<unique original absolute window archive root>",
    "queue_receipt_out": "<unique original absolute outer queue receipt root>",
    "window_command": ["<exact root-owned window collector argv>"],
    "queue_lock_dir": PRODUCTION_QUEUE,
    "queue_timeout_seconds": "<positive finite bound>",
    "source_and_functionality_accepted": True,
    "limits": ["one preregistered local POSIX loopback window; no adoption", *UNMEASURED],
}


def eq(left, right):
    return P.canonical(left) == P.canonical(right)


def positive_number_equal(left, right):
    return (type(left) in (int, float) and type(right) in (int, float)
            and math.isfinite(left) and math.isfinite(right) and left > 0 and right > 0 and left == right)


def collector_runtime_identity():
    resolved = Path(sys.executable).resolve()
    return dict(schema="w1-collector-runtime", schema_version=1,
                interpreter=dict(path=sys.executable, resolved_path=str(resolved), sha256=P.sha(resolved),
                                 bytes=resolved.stat().st_size, version=sys.version))


def validate_collector_runtime(runtime):
    P.require(isinstance(runtime, dict) and set(runtime) == {"schema", "schema_version", "interpreter"}
              and runtime["schema"] == "w1-collector-runtime" and type(runtime["schema_version"]) is int
              and runtime["schema_version"] == 1, "missing/invalid collector runtime provenance")
    interpreter = runtime["interpreter"]
    P.require(isinstance(interpreter, dict) and set(interpreter) == {"path", "resolved_path", "sha256", "bytes", "version"}
              and P.digest(interpreter.get("sha256")) and P.integer(interpreter.get("bytes"), 1)
              and isinstance(interpreter.get("version"), str) and interpreter["version"], "invalid collector interpreter identity")
    _absolute(interpreter["path"])
    _absolute(interpreter["resolved_path"])
    return runtime


def schedule(spec):
    """Network calibration never exposes a treatment, including its warmup."""
    jobs = P.schedule(spec)
    if spec["purpose"] == "calibration":
        calibration = {c["id"]: c["calibration"] for c in spec["comparisons"]}
        for job in jobs:
            if job["phase"] == "warmup":
                job["condition"] = calibration[job["comparison"]]
    return jobs


def _identity(path):
    path = Path(path)
    P.require(path.is_file() and not path.is_symlink(), "unsafe/missing full data file")
    return dict(sha256=P.sha(path), bytes=path.stat().st_size)


def _absolute(value):
    P.require(isinstance(value, str) and value and Path(value).is_absolute()
              and str(Path(value)) == value and ".." not in Path(value).parts, "noncanonical recorded execution path")
    return value


def validate_preregistration(spec, bound=False):
    P.validate_spec(spec)
    P.require(spec["profile"] == "network" and spec.get("adapter_schema") == "w1-network-revision2-v1",
              "wrong network adapter/profile")
    P.require(spec.get("contract_sha256") == CONTRACT_SHA and spec.get("model_subcases_sha256") == SUBCASES_SHA
              and type(spec.get("gate_revision")) is int and spec["gate_revision"] == 2
              and spec.get("contract_version") == "1.0.0" and spec.get("timing_unit") == "ns",
              "unaccepted contract/revision/unit")
    P.require(spec["metric_boundary"] == PRIMARY_BOUNDARY, "primary RTT boundary differs")
    P.require(eq(spec.get("environment_policy"), ENVIRONMENT_POLICY), "environment snapshot policy differs")
    drift = spec["policy"].get("absolute_anchor_drift_relative_limit")
    P.require(type(drift) in (int, float) and math.isfinite(drift) and drift == .01,
              "absolute anchor drift budget must be preregistered at one percent")
    controls = spec["controls"]
    wire, owner, inflight = (controls[k] for k in ("max_wire_bytes", "owner_capacity_bytes", "max_inflight_bytes"))
    P.require(4096 <= wire <= MAX_PAYLOAD + 88 and 4096 <= owner <= MAX_PAYLOAD
              and 4 * wire <= inflight <= 8 * (MAX_PAYLOAD + 88), "network logical/owner budget differs")
    P.require(controls["pool_idle_reserved_bytes"] == 2 * (owner + CHUNK), "two independent pool idle budgets differ")
    P.require(controls["schema_id"] == "n7.reference.file-pipeline.v1"
              and controls["processing_id"] == "fixture_value/processed_value/markers/labels/exact-values-v1"
              and controls["connection"] == "one persistent TCP IPv4 loopback connection, one in-flight request",
              "network schema/processing/connection differs")
    P.require(P.integer(spec.get("timeout_ms"), 1) and spec["timeout_ms"] <= 600000, "invalid operation timeout")
    P.require(P.integer(spec.get("max_retained_output_bytes"), 1) and spec["max_retained_output_bytes"] <= MAX_RETAINED_OUTPUT,
              "missing/excessive retained-output budget")
    for work in spec["workloads"]:
        P.require(work["iterations"] <= 100000 and work["elements"] <= (MAX_PAYLOAD - CHUNK) // 4,
                  "network workload outside accepted range")
    P.require(all(c["variant"] == "F" for c in spec["conditions"].values()), "unapproved producer variant")
    P.require(all(c["calibration"] == "F-none" for c in spec["comparisons"]), "calibration must use the F-none baseline")
    if bound:
        P.require(spec.get("ready") is True and spec["evidence_class"] in ("native", "synthetic-framework-test"),
                  "unbound native preregistration")
        validate_collector_runtime(spec.get("collector_runtime"))
        identity = spec.get("producer_identity", {})
        P.require(set(identity) == set(PRODUCER_KEYS) and all(P.digest(v) for v in identity.values()),
                  "missing frozen producer/source/toolchain/dependency identity")
        P.require(set(spec.get("expected_data", {})) == {w["id"] for w in spec["workloads"]}, "missing exact data preregistration")
        for work in spec["workloads"]:
            P.validate_data_identity(spec, work, spec["expected_data"][work["id"]])
            P.require(all(88 + 4 * work["elements"] <= spec["expected_data"][work["id"]][direction + "_bytes"]
                          <= owner + 88 for direction in ("input", "output")), "wire/vector payload does not fit separate owner capacity")
            P.require(re.fullmatch(r"0|[1-9][0-9]{0,19}", spec["expected_data"][work["id"]]["semantic_checksum"]) is not None
                      and int(spec["expected_data"][work["id"]]["semantic_checksum"]) <= 2**64 - 1,
                      "checksum is not decimal uint64")
        jobs = schedule(spec)
        execution_root = _absolute(spec.get("execution_root"))
        build_root = _absolute(spec.get("build_execution_root"))
        queue_out = _absolute(spec.get("queue_receipt_out"))
        P.require(not (Path(execution_root) == Path(queue_out) or Path(execution_root) in Path(queue_out).parents
                       or Path(queue_out) in Path(execution_root).parents), "window/queue archive paths overlap")
        retained = sum(j["iterations"] * spec["expected_data"][j["workload"]]["output_bytes"] for j in jobs)
        P.require(P.integer(spec.get("retained_output_bytes_expected"), 1)
                  and retained == spec["retained_output_bytes_expected"] <= spec["max_retained_output_bytes"],
                  "retained output schedule exceeds preregistered byte budget")
        paths = spec.get("paths", {})
        _absolute(paths.get("binary"))
        P.require(set(paths.get("inputs", {})) == {w["id"] for w in spec["workloads"]}
                  and set(paths.get("outputs", {})) == {j["id"] for j in jobs}, "execution paths incomplete")
        inputs = [_absolute(v) for v in paths["inputs"].values()]
        outputs = [_absolute(v) for v in paths["outputs"].values()]
        P.require(len(outputs) == len(set(outputs)) and not set(inputs) & set(outputs)
                  and paths["binary"] not in inputs + outputs, "output reuse/identity path overlap")
        expected_paths = dict(binary=build_root + "/net_reference",
                              inputs={w["id"]: execution_root + "/data/" + spec["expected_data"][w["id"]]["input_sha256"] for w in spec["workloads"]},
                              outputs={j["id"]: execution_root + "/commands/" + j["id"] + "/outputs" for j in jobs})
        P.require(eq(paths, expected_paths), "recorded producer/input/output path does not match approved execution roots")
        P.require(eq(spec.get("invocations"), {j["id"]: argv(spec, j) for j in jobs}), "CLI differs from accepted W1 contract")
    return spec


def argv(spec, job):
    c, paths = spec["controls"], spec["paths"]
    pool = spec["conditions"][job["condition"]]["pool"]
    return [paths["binary"], "run", "--input", paths["inputs"][job["workload"]], "--out-dir", paths["outputs"][job["id"]],
            "--pool", pool, "--iterations", str(job["iterations"]), "--max-wire", str(c["max_wire_bytes"]),
            "--owner-capacity", str(c["owner_capacity_bytes"]), "--max-inflight-bytes", str(c["max_inflight_bytes"]),
            "--timeout-ms", str(spec["timeout_ms"])]


def normalize(spec, job, stdout, stderr, receipt, input_file, output_dir):
    """Validate exact W1 stdout and each full output; never executes a producer.

    W1 does not emit source/data hashes or a unit field. Those are independently
    bound to the frozen producer receipt, full files, and accepted ns contract.
    A source identity in JSON is never trusted as proof of the executed binary.
    """
    validate_preregistration(spec, bound=True)
    P.require(any(eq(job, j) for j in schedule(spec)), "unregistered job")
    P.require(isinstance(stdout, bytes) and isinstance(stderr, bytes), "raw output must retain bytes")
    P.require(receipt.get("returncode") == 0 and type(receipt.get("returncode")) is int
              and receipt.get("timed_out") is False and eq(receipt.get("job"), job)
              and eq(receipt.get("argv"), argv(spec, job)), "unsuccessful/wrong producer invocation")
    P.require(eq(receipt.get("producer_identity"), spec["producer_identity"]), "executed producer/source identity differs")
    for name, raw in (("stdout", stdout), ("stderr", stderr)):
        P.require(receipt.get(name + "_sha256") == hashlib.sha256(raw).hexdigest()
                  and type(receipt.get(name + "_bytes")) is int and receipt[name + "_bytes"] == len(raw),
                  "raw " + name + " identity differs")
    P.require(stdout.startswith(b"{") and stdout.endswith(b"}\n"), "stdout must be one UTF-8 JSON object plus newline")
    raw = P.parse(stdout.decode("utf-8"))
    P.require(isinstance(raw, dict) and set(raw) == RUN_FIELDS, "unexpected/missing W1 raw fields/units")
    P.require(type(raw["schema_version"]) is int and raw["schema_version"] == 1
              and raw["contract_version"] == "1.0.0" and raw["command"] == "run" and raw["status"] == "passed"
              and raw["diagnostic"] is False and raw["timing_evidence"] is False, "raw status/profile/contract differs")
    pool = spec["conditions"][job["condition"]]["pool"]
    expected = spec["expected_data"][job["workload"]]
    for key, value in dict(pool=pool, iterations=job["iterations"], element_count=expected["elements"],
                           semantic_checksum=expected["semantic_checksum"], input_bytes_per_iteration=expected["input_bytes"],
                           output_bytes_per_iteration=expected["output_bytes"], connection_count=1, max_inflight_requests=1,
                           timeout_ms=spec["timeout_ms"], reserved_inflight_peak_bytes=4 * spec["controls"]["max_wire_bytes"],
                           **{k: spec["controls"][k] for k in ("max_wire_bytes", "owner_capacity_bytes", "max_inflight_bytes")}).items():
        P.require(eq(raw[key], value), "raw data/control differs: " + key)
    P.require(eq(_identity(input_file), dict(sha256=expected["input_sha256"], bytes=expected["input_bytes"])), "full input differs")
    P.require(isinstance(raw["samples"], list) and len(raw["samples"]) == job["iterations"], "sample count differs")
    output_names = ["output-" + str(i) + ".wire" for i in range(job["iterations"])]
    P.require(eq(raw["output_files"], output_names), "output order/count/name differs")
    P.require(set(P.inventory(output_dir)) == set(output_names), "full output inventory differs")
    output_identities = []
    for index, sample in enumerate(raw["samples"]):
        P.require(isinstance(sample, dict) and set(sample) == SAMPLE_FIELDS, "sample schema differs")
        P.require(all(sample[f] is None for f in PHASE_FIELDS), "diagnostic stage probes in timing sample")
        for field in ("end_to_end_ns", "client_owner_release_outside_ns", "server_completion_wait_outside_ns", "input_bytes", "output_bytes"):
            P.require(P.integer(sample[field], 1 if field == "end_to_end_ns" else 0), "invalid raw sample: " + field)
        P.require(sample["input_bytes"] == expected["input_bytes"] and sample["output_bytes"] == expected["output_bytes"], "sample bytes differ")
        identity = _identity(P.safe_file(output_dir, output_names[index]))
        P.require(eq(identity, dict(sha256=expected["output_sha256"], bytes=expected["output_bytes"])), "per-iteration full output differs")
        output_identities.append(dict(file=output_names[index], iteration=index, **identity))
    for field in (*OUTSIDE, "end_to_end_ns", "pool_measured_hits", "pool_measured_misses", "pool_warmup_hits", "pool_warmup_misses",
                  "pool_cached_blocks_before_final_clear", "pool_cached_reserved_bytes_before_final_clear", "pool_cached_accessible_bytes_before_final_clear"):
        P.require(P.integer(raw[field]), "invalid integer ns/resource field: " + field)
    P.require(raw["end_to_end_ns"] == sum(s["end_to_end_ns"] for s in raw["samples"]), "RTT sum differs")
    for field in ("client_owner_release_outside_ns", "server_completion_wait_outside_ns"):
        P.require(raw[field] == sum(s[field] for s in raw["samples"]), "outside per-sample sum differs: " + field)
    P.require(pool == "cold" or raw["cold_clear_outside_ns"] == 0, "unexpected cold clear")
    P.require(pool == "warm" or raw["warmup_outside_ns"] == 0, "unexpected warmup")
    n = job["iterations"]
    P.require(raw["pool_measured_hits"] == (2 * n if pool == "warm" else 0)
              and raw["pool_measured_misses"] == (2 * n if pool == "cold" else 0)
              and raw["pool_warmup_hits"] == 0 and raw["pool_warmup_misses"] == (2 if pool == "warm" else 0), "pool hit/miss semantics differ")
    blocks, reserved, accessible = (raw[k] for k in ("pool_cached_blocks_before_final_clear", "pool_cached_reserved_bytes_before_final_clear", "pool_cached_accessible_bytes_before_final_clear"))
    P.require(0 <= accessible <= reserved <= spec["controls"]["pool_idle_reserved_bytes"]
              and blocks == (0 if pool == "none" else 2)
              and (reserved == accessible == 0 if pool == "none" else accessible > 0), "pool retention budget differs")
    start, end = receipt.get("started_monotonic_ns"), receipt.get("finished_monotonic_ns")
    P.require(P.integer(start, 1) and P.integer(end, start)
              and end - start <= spec["policy"]["child_timeout_seconds"] * 10**9, "child interval differs")
    P.require(raw["end_to_end_ns"] + sum(raw[f] for f in OUTSIDE) <= end - start, "disjoint costs exceed child duration")
    return dict(schema="w1-network-normalized", schema_version=1, job=job, evidence_class=spec["evidence_class"],
                timing_unit="ns", data_identity=expected, raw_stdout_sha256=receipt["stdout_sha256"],
                raw_stderr_sha256=receipt["stderr_sha256"], producer_identity=spec["producer_identity"],
                raw_rtt_samples_ns=[s["end_to_end_ns"] for s in raw["samples"]],
                raw_rtt_total_ns=raw["end_to_end_ns"], outside_ns={f: raw[f] for f in OUTSIDE},
                output_identities=output_identities, iterations=n, timing_evidence=False,
                whole_lifecycle_available=False, unmeasured_costs=list(UNMEASURED),
                overlapping_stage_costs_added=False,
                pool_retention={k: raw[k] for k in raw if k.startswith("pool_") and k not in OUTSIDE})


def costs(report):
    """Exact revision-2 metric; whole response-send/destructor intervals overlap.

    server_completion_wait_outside_ns is a sequential residual wait. It retains
    its own name and is not disguised as client-owner release for P.means().
    """
    n, raw, outside = report["iterations"], report["raw_rtt_total_ns"], report["outside_ns"]
    return dict(raw_rtt_mean_ns=raw / n,
                reclamation_adjusted_mean_ns=(raw + sum(outside[k] for k in RECLAMATION)) / n,
                declared_disjoint_mean_ns=(raw + sum(outside.values())) / n)


def paired_stats(jobs, reports):
    P.require(len(jobs) >= 4 and len(jobs) % 2 == 0, "incomplete network pair group")
    pairs = []
    for index in range(0, len(jobs), 2):
        a, b = jobs[index:index + 2]
        P.require(a["pair"] == b["pair"] and a["side"] != b["side"] and a["position"] == 0 and b["position"] == 1, "network pair order differs")
        left = costs(reports[(a if a["side"] == "left" else b)["id"]])
        right = costs(reports[(a if a["side"] == "right" else b)["id"]])
        pairs.append(dict(pair=a["pair"], order=a["order"], left=left, right=right,
                          ratios={k: left[k] / right[k] for k in left}))
    metrics = {}
    for key in pairs[0]["ratios"]:
        values = [p["ratios"][key] for p in pairs]
        logs = [math.log(v) for v in values]
        center = statistics.median(logs)
        metrics[key] = dict(median_left_over_right=statistics.median(values), range_left_over_right=[min(values), max(values)],
                            order_stratum_medians={order: statistics.median(p["ratios"][key] for p in pairs if p["order"] == order) for order in ("AB", "BA")},
                            median_absolute_log_ratio=statistics.median(abs(v) for v in logs),
                            log_mad=statistics.median(abs(v - center) for v in logs), log_iqr=P.quantile(logs, .75) - P.quantile(logs, .25))
    return dict(pairs=pairs, metrics=metrics)


def summarize(spec, jobs, reports):
    P.require(eq(jobs, schedule(spec)) and set(reports) == {j["id"] for j in jobs}, "network exact job inventory differs")
    rows = []
    for work in spec["workloads"]:
        for comparison in spec["comparisons"]:
            group = [j for j in jobs if j["workload"] == work["id"] and j["comparison"] == comparison["id"]]
            anchors = {}
            for anchor in ("pre", "mid", "post"):
                stats = paired_stats([j for j in group if j["phase"] == "calibration" and j["anchor"] == anchor], reports)
                left = [p["left"]["reclamation_adjusted_mean_ns"] for p in stats["pairs"]]
                right = [p["right"]["reclamation_adjusted_mean_ns"] for p in stats["pairs"]]
                absolute = dict(left=left, right=right, pooled=left + right,
                                left_median=statistics.median(left), right_median=statistics.median(right),
                                pooled_median=statistics.median(left + right))
                anchors[anchor] = dict(qualified=P.calibration_pass(stats, spec["policy"]), statistics=stats,
                                       absolute_adjusted_cost_ns=absolute)
            measured = [j for j in group if j["phase"] == "comparison"]
            absolute_by_side = {side: {anchor: value["absolute_adjusted_cost_ns"][side + "_median"] for anchor, value in anchors.items()}
                                for side in ("left", "right", "pooled")}
            drift_ratios = {side: max(values.values()) / min(values.values()) for side, values in absolute_by_side.items()}
            absolute_medians = absolute_by_side["pooled"]
            drift_ratio = drift_ratios["pooled"]
            drift_ok = all(ratio <= 1 + spec["policy"]["absolute_anchor_drift_relative_limit"] for ratio in drift_ratios.values())
            local_ok = all(v["qualified"] for v in anchors.values())
            rows.append(dict(workload=work["id"], comparison=comparison["id"], calibration=anchors,
                             aa_qualified=local_ok, local_aa_qualified=local_ok,
                             anchor_drift=dict(anchor_medians_ns=absolute_medians, maximum_over_minimum=drift_ratio,
                                               side_anchor_medians_ns=absolute_by_side, side_maximum_over_minimum=drift_ratios,
                                               relative_limit=spec["policy"]["absolute_anchor_drift_relative_limit"], qualified=drift_ok),
                             stable_window_qualified=local_ok and drift_ok,
                             comparison_statistics=paired_stats(measured, reports) if measured else None))
    return dict(schema="w1-network-window-summary", schema_version=1, evidence_class=spec["evidence_class"],
                purpose=spec["purpose"], process_count=len(jobs), rows=rows, all_aa_qualified=all(r["aa_qualified"] for r in rows),
                all_local_aa_qualified=all(r["local_aa_qualified"] for r in rows),
                all_stable_windows_qualified=all(r["stable_window_qualified"] for r in rows),
                qualification_boundary="local A/A pair ratios and absolute pre/mid/post left/right/pooled anchor medians are separate gates",
                ratio_direction="left / right; greater than one means right faster", whole_lifecycle_available=False,
                unmeasured_costs=list(UNMEASURED), timing_evidence=False, adoption_allowed=False,
                confirmation_windows_required=3, reason="one window; synthetic tests cannot establish native timing; parent review required")


def validate_parent_gate(root, spec, approval_sha256):
    """Validate a *parent-pinned* gate, never self-approve an archive's gate."""
    root = Path(root)
    validate_preregistration(spec, bound=True)
    P.require(spec["evidence_class"] == "native", "synthetic/unbound bundle cannot be native evidence")
    path = P.safe_file(root, "approval.json")
    P.require(P.digest(approval_sha256) and P.sha(path) == approval_sha256, "parent approval must be pinned outside archive")
    gate = P.read(path)
    P.require(gate.get("schema") == APPROVAL_SCHEMA["schema"] and type(gate.get("schema_version")) is int
              and gate["schema_version"] == 1 and gate.get("status") == "approved"
              and gate.get("native_execution_authorized") is True and gate.get("source_and_functionality_accepted") is True
              and gate.get("evidence_class") == "native-posix-tcp-loopback", "parent source/functionality/runtime gate missing")
    P.require(gate.get("preregister_sha256") == P.sha(P.safe_file(root, "preregister.json"))
              and gate.get("contract_sha256") == CONTRACT_SHA and gate.get("model_subcases_sha256") == SUBCASES_SHA
              and gate.get("runner_sha256") == QUEUE_SHA, "approval identities differ")
    P.require(gate.get("window_id") == spec["window_id"] and gate.get("execution_root") == spec["execution_root"]
              and gate.get("queue_receipt_out") == spec["queue_receipt_out"], "approval targets a different window/output")
    prefixes = ("contract/", "protocol/", "w1-build/", "w1-functional/", "data/")
    expected = gate.get("accepted_inputs")
    actual = {k: v for k, v in P.inventory(root).items() if k.startswith(prefixes)}
    P.require(isinstance(expected, dict) and bool(expected) and eq(actual, expected), "accepted build/function/data/source inventory differs")
    fixed = {"contract/contract.json": CONTRACT_SHA, "contract/model_subcases.json": SUBCASES_SHA,
             "protocol/run_job.py": QUEUE_SHA, "protocol/network_measurement.py": P.sha(__file__),
             "protocol/paired_measurement.py": P.sha(P.__file__)}
    P.require(all(P.sha(P.safe_file(root, k)) == v for k, v in fixed.items()), "frozen contract/adapter/queue source differs")
    P.require(eq(P.read(P.safe_file(root, "protocol/collector-runtime.json")), validate_collector_runtime(spec["collector_runtime"])),
              "frozen collector runtime differs from preregistration")
    contract = P.read(root / "contract/contract.json")
    acceptance = P.read(P.safe_file(root, "contract/acceptance.json"))
    P.require(contract["gate_revision"] == 2 and acceptance.get("status") == "accepted_interface_only"
              and acceptance.get("sha256", {}).get("experiments/vm_network/contract.json") == CONTRACT_SHA,
              "interface acceptance differs")
    for name, expected_hash, kind in (("w1-build", gate.get("build_result_sha256"), "build"),
                                       ("w1-functional", gate.get("functional_result_sha256"), "smoke")):
        directory = root / name
        result_path = P.safe_file(directory, "result.json")
        result = P.read(result_path)
        P.require(P.sha(result_path) == expected_hash and type(result.get("schema_version")) is int
                  and result["schema_version"] == 1 and result.get("kind") == kind and result.get("status") == "passed"
                  and result.get("timing_evidence") is False, "accepted W1 result differs")
        P.require(P.safe_file(directory, "COMPLETE").read_text().strip() == P.sha(result_path), "W1 completion differs")
        actual_files = {k: v["sha256"] for k, v in P.inventory(directory, ("result.json", "COMPLETE")).items()}
        P.require(eq(actual_files, result.get("files")), "W1 full archive inventory differs")
    build, functional = P.read(root / "w1-build/result.json"), P.read(root / "w1-functional/result.json")
    P.require(build.get("measurement_profile") == "timing" and build.get("sanitizer") == "none"
              and build.get("contract_sha256") == CONTRACT_SHA and eq(build.get("baseline_headers"), P.BASELINE), "W1 build profile/source differs")
    P.require(build.get("build_root") == spec["build_execution_root"]
              and spec["paths"]["binary"] == build["build_root"] + "/net_reference", "accepted build execution path differs")
    flags = build["commands"][0]["argv"]
    P.require([v for v in flags if v.startswith(("-DN7_NET_DIAGNOSTICS", "-UN7_NET_DIAGNOSTICS"))] == ["-DN7_NET_DIAGNOSTICS=0"]
              and [v for v in flags if v.startswith("-O")] == ["-O2"]
              and not any(v.startswith(("-fsanitize", "-fno-sanitize")) for v in flags), "timing compiler flags differ")
    producer = dict(binary_sha256=P.sha(P.safe_file(root, "w1-build/net_reference")),
                    source_inventory_sha256=P.sha(P.safe_file(root, "w1-build/source-inventory.json")),
                    toolchain_sha256=P.sha(P.safe_file(root, "w1-build/toolchain.json")),
                    dependencies_sha256=P.sha(P.safe_file(root, "w1-build/dependencies.json")))
    P.require(eq(producer, spec["producer_identity"]) and build.get("binary_sha256") == producer["binary_sha256"]
              and functional.get("binary_sha256") == producer["binary_sha256"]
              and functional.get("build_result_sha256") == P.sha(root / "w1-build/result.json"), "build/functionality producer lineage differs")
    for name, digest in P.BASELINE.items():
        P.require(P.sha(P.safe_file(root, "w1-build/source/" + name)) == digest, "production F identity differs")
    P.require(P.sha(P.safe_file(root, "w1-build/source/contract.json")) == CONTRACT_SHA, "build contract differs")
    smoke = P.parse(P.safe_file(root, "w1-functional/smoke.stdout").read_bytes())
    P.require(smoke.get("status") == "passed" and smoke.get("diagnostic") is False and smoke.get("timing_evidence") is False
              and type(smoke.get("checks")) is int and smoke["checks"] == 34
              and eq(sorted(smoke.get("case_ids", [])), sorted(contract["smoke"]["case_ids"]))
              and type(smoke.get("owned_sockets_after_cases")) is int and smoke["owned_sockets_after_cases"] == 0,
              "accepted timing functional result differs")
    P.require(isinstance(gate.get("window_command"), list) and gate["window_command"]
              and all(isinstance(a, str) and a for a in gate["window_command"]), "missing approved window command")
    P.require(gate["window_command"][0] == spec["collector_runtime"]["interpreter"]["path"], "approved collector interpreter path differs")
    _absolute(gate.get("queue_lock_dir"))
    P.require(gate["queue_lock_dir"] == PRODUCTION_QUEUE, "nonproduction queue lock")
    timeout = gate.get("queue_timeout_seconds")
    P.require(type(timeout) in (int, float) and math.isfinite(timeout) and 0 < timeout <= spec["policy"]["max_window_seconds"], "unbounded queue runtime")
    return gate


def _timestamp(value):
    P.require(isinstance(value, str), "missing aware timestamp")
    value = dt.datetime.fromisoformat(value)
    P.require(value.tzinfo is not None, "naive timestamp")
    return value


def validate_queue_request(request, gate, kind="timing"):
    P.require(isinstance(request, dict) and type(request.get("schema_version")) is int and request["schema_version"] == 1
              and request.get("owner") == "W3" and request.get("kind") == kind
              and request.get("runner_sha256") == QUEUE_SHA and eq(request.get("source_headers"), P.BASELINE)
              and eq(request.get("command"), gate["window_command"]) and request.get("lock_dir") == gate["queue_lock_dir"]
              and gate["queue_lock_dir"] == PRODUCTION_QUEUE and request.get("out") == gate["queue_receipt_out"]
              and positive_number_equal(request.get("timeout_seconds"), gate["queue_timeout_seconds"]), "queue request not approved whole-window timing")
    P.require(isinstance(request.get("run_id"), str) and bool(request["run_id"]), "missing queue run id")


def validate_queue(root, spec, gate, allow_failed=False, kind="timing"):
    root = Path(root)
    manifest = P.read(P.safe_file(root, "manifest.json"))
    inventory = P.inventory(root, ("manifest.json",))
    expected = dict(schema_version=1, files=[dict(path=k, **v) for k, v in inventory.items()])
    P.require(eq(manifest, expected), "queue manifest/hash differs")
    request, result = P.read(P.safe_file(root, "request.json")), P.read(P.safe_file(root, "result.json"))
    validate_queue_request(request, gate, kind)
    P.require(type(result.get("schema_version")) is int and result["schema_version"] == 1
              and isinstance(request.get("run_id"), str) and request["run_id"] and result.get("run_id") == request["run_id"]
              and result.get("owner") == "W3" and result.get("kind") == kind, "queue result identity differs")
    if allow_failed:
        P.require(result.get("status") in ("complete", "failed", "timeout", "cancelled", "error", "busy"), "nonterminal failed queue")
        if result.get("status") == "complete":
            P.require(type(result.get("command_returncode")) is int and result["command_returncode"] == 0
                      and result.get("cleanup", {}).get("live") == [], "invalid completed failed-window queue")
        executed = P.integer(result.get("command_pid"), 1)
        if not executed:
            P.require(result.get("command_pid") is None and result.get("command_returncode") is None
                      and result["status"] in ("busy", "cancelled", "error"), "invalid zero-execution queue")
        return dict(run_id=request["run_id"], job_status=result["status"], timing_evidence=False, executed=executed)
    P.require(result.get("status") == "complete"
              and type(result.get("command_returncode")) is int and result["command_returncode"] == 0
              and P.integer(result.get("command_pid"), 1)
              and result.get("cleanup", {}).get("live") == [], "queue did not complete with owned work stopped")
    begin, end = _timestamp(result.get("started_at")), _timestamp(result.get("finished_at"))
    elapsed = result.get("command_elapsed_seconds")
    P.require(end >= begin and type(elapsed) in (int, float) and math.isfinite(elapsed)
              and 0 <= elapsed <= gate["queue_timeout_seconds"], "queue elapsed bound differs")
    return dict(run_id=request["run_id"], begin=begin, end=end, elapsed=elapsed, request=request,
                command_pid=result["command_pid"], started_at=result["started_at"], request_sha256=P.sha(root / "request.json"),
                manifest_sha256=P.sha(root / "manifest.json"), result_sha256=P.sha(root / "result.json"))


def validate_active_context(request_bytes, metadata, ancestor_pids, current_command, gate, kind="timing"):
    """Pure validation of a live lease snapshot; used by mocked unit tests too."""
    request = P.parse(request_bytes)
    validate_queue_request(request, gate, kind)
    P.require(eq(current_command, gate["window_command"]), "timing queue is running an unrelated command")
    P.require(isinstance(metadata, dict) and isinstance(ancestor_pids, list) and len(ancestor_pids) == 2
              and all(P.integer(p, 1) for p in ancestor_pids)
              and P.integer(metadata.get("supervisor_pid"), 1) and metadata["supervisor_pid"] in ancestor_pids
              and metadata["supervisor_pid"] == ancestor_pids[1]
              and metadata.get("status") == "running" and metadata.get("owner") == "W3"
              and metadata.get("kind") == kind and metadata.get("run_id") == request["run_id"]
              and metadata.get("out") == gate["queue_receipt_out"], "not an ancestor-owned whole-window timing lease")
    _timestamp(metadata.get("started_at"))
    return dict(explicit_flag=True, lock_was_held=True, lock_path=PRODUCTION_QUEUE + "/queue.lock",
                lock_metadata=metadata, ancestor_pids=ancestor_pids,
                request_sha256=hashlib.sha256(request_bytes).hexdigest(), collector_argv=current_command)


def _ancestor_pids():
    # The exact approved collector command is the direct child of the accepted
    # W4 supervisor. Do not spawn ps probes between calibration pairs.
    return [os.getpid(), os.getppid()]


def active_queue(gate, kind="timing"):
    """Require the stable OS lock and exact approved ancestor command, not a flag."""
    import fcntl
    lock = Path(PRODUCTION_QUEUE) / "queue.lock"
    P.require(lock.is_file() and not lock.is_symlink(), "production timing queue unavailable")
    fd = os.open(lock, os.O_RDWR | getattr(os, "O_NOFOLLOW", 0))
    try:
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            pass
        else:
            fcntl.flock(fd, fcntl.LOCK_UN)
            raise P.ProtocolError("timing queue is idle; collector must run under approved run_job.py")
        os.lseek(fd, 0, os.SEEK_SET)
        metadata = P.parse(os.read(fd, 65536))
    finally:
        os.close(fd)
    request_bytes = P.safe_file(Path(gate["queue_receipt_out"]), "request.json").read_bytes()
    # Queue inner argv must use this absolute sys.executable without -B/-O;
    # PYTHONDONTWRITEBYTECODE=1 provides no-bytecode behavior without argv loss.
    return validate_active_context(request_bytes, metadata, _ancestor_pids(), [sys.executable, *sys.argv], gate, kind)


def validate_live_inputs(root, spec, gate, full_environment=False):
    """Check original executed files immediately before/after every invocation."""
    root = Path(root)
    P.require(P.sha(P.safe_file(root, "preregister.json")) == gate["preregister_sha256"], "live preregistration changed")
    for name, module in (("network_measurement.py", __file__), ("paired_measurement.py", P.__file__)):
        P.require(P.sha(module) == gate["accepted_inputs"]["protocol/" + name]["sha256"], "live collector source changed")
    build_root = Path(spec["build_execution_root"])
    actual = dict(binary_sha256=P.sha(P.safe_file(build_root, "net_reference")),
                  source_inventory_sha256=P.sha(P.safe_file(build_root, "source-inventory.json")),
                  toolchain_sha256=P.sha(P.safe_file(build_root, "toolchain.json")),
                  dependencies_sha256=P.sha(P.safe_file(build_root, "dependencies.json")))
    P.require(eq(actual, spec["producer_identity"]), "live executed binary/source/toolchain identity changed")
    source_files = {name: value["sha256"] for name, value in P.inventory(build_root / "source").items()}
    P.require(eq(source_files, P.read(build_root / "source-inventory.json")), "live source bytes changed")
    if full_environment:
        P.require(eq(collector_runtime_identity(), validate_collector_runtime(spec.get("collector_runtime"))),
                  "live collector interpreter bytes/version differ from preregistration")
        toolchain = P.read(build_root / "toolchain.json")
        environment_files = dict(toolchain.get("runtime", {}))
        P.require(bool(environment_files) and all(P.digest(v) for v in environment_files.values()), "missing native runtime byte identities")
        environment_files[_absolute(toolchain.get("compiler"))] = toolchain.get("compiler_sha256")
        environment_files[_absolute(toolchain.get("sdk")) + "/SDKSettings.json"] = toolchain.get("sdk_settings_sha256")
        for path, expected_hash in environment_files.items():
            _absolute(path)
            P.require(P.digest(expected_hash) and P.sha(Path(path)) == expected_hash, "live compiler/SDK/runtime bytes changed")
        dependencies = P.read(build_root / "dependencies.json")
        dependency_root = Path(_absolute(dependencies.get("root")))
        P.require(isinstance(dependencies.get("files"), dict) and bool(dependencies["files"]), "missing dependency byte identities")
        for name, expected_hash in dependencies["files"].items():
            P.require(P.digest(expected_hash) and P.sha(P.safe_file(dependency_root, name)) == expected_hash, "live dependency bytes changed")
        P.require(all(not os.environ.get(name) for name in ("LD_PRELOAD", "LD_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES", "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH")),
                  "unregistered dynamic loader override")
    for name, value in spec["expected_data"].items():
        P.require(eq(_identity(P.safe_file(root, "data/" + value["input_sha256"])),
                     dict(sha256=value["input_sha256"], bytes=value["input_bytes"])), "live input bytes changed")
    return actual


def environment_snapshot(stage):
    """Bounded read-only host probes once at each edge, never between pairs."""
    P.require(stage in ("before", "after"), "unknown environment stage")
    snapshot = dict(schema="w1-window-environment", schema_version=1, stage=stage,
                    captured_at=dt.datetime.now(dt.timezone.utc).isoformat(), monotonic_ns=time.monotonic_ns(),
                    host=dict(system=platform.system(), machine=platform.machine(), cpu_count=os.cpu_count()), probes={})
    try:
        snapshot["load_average"] = dict(status="available", values=list(os.getloadavg()))
    except OSError as error:
        snapshot["load_average"] = dict(status="unavailable", reason=str(error))
    for name, command in ENVIRONMENT_POLICY["commands"].items():
        stdout, stderr, returncode, timed_out, reason = b"", b"", None, False, ""
        try:
            process = subprocess.run(command, capture_output=True, timeout=ENVIRONMENT_POLICY["max_probe_seconds"], check=False)
            stdout, stderr, returncode = process.stdout, process.stderr, process.returncode
            if returncode != 0:
                reason = "probe command exit " + str(returncode)
        except subprocess.TimeoutExpired as error:
            stdout, stderr, timed_out, reason = error.stdout or b"", error.stderr or b"", True, "probe deadline expired"
        except OSError as error:
            reason = str(error)
        limit = ENVIRONMENT_POLICY["max_retained_bytes_per_stream"]
        complete = len(stdout) <= limit and len(stderr) <= limit
        if not complete:
            reason = "probe output limit exceeded; retained prefix only"
        probe = dict(command=command, status="available" if not reason else "unavailable", reason=reason,
                     returncode=returncode, timed_out=timed_out, raw_complete=complete)
        for stream, data in (("stdout", stdout[:limit]), ("stderr", stderr[:limit])):
            probe[stream] = dict(base64=base64.b64encode(data).decode("ascii"), sha256=hashlib.sha256(data).hexdigest(), bytes=len(data))
        snapshot["probes"][name] = probe
    return snapshot


def validate_environment(snapshot, stage, spec):
    P.require(isinstance(snapshot, dict) and snapshot.get("schema") == "w1-window-environment"
              and type(snapshot.get("schema_version")) is int and snapshot["schema_version"] == 1
              and snapshot.get("stage") == stage and P.integer(snapshot.get("monotonic_ns"), 1), "environment snapshot schema differs")
    _timestamp(snapshot.get("captured_at"))
    host = snapshot.get("host", {})
    P.require(set(host) == {"system", "machine", "cpu_count"} and all(isinstance(host[k], str) and host[k] for k in ("system", "machine"))
              and (host["cpu_count"] is None or P.integer(host["cpu_count"], 1)), "environment CPU/host identity missing")
    load = snapshot.get("load_average", {})
    if load.get("status") == "available":
        P.require(isinstance(load.get("values"), list) and len(load["values"]) == 3
                  and all(type(v) in (int, float) and math.isfinite(v) and v >= 0 for v in load["values"]), "invalid load averages")
    else:
        P.require(load.get("status") == "unavailable" and isinstance(load.get("reason"), str) and load["reason"], "unexplained unavailable load probe")
    probes = snapshot.get("probes", {})
    P.require(set(probes) == set(spec["environment_policy"]["commands"]), "environment probe inventory differs")
    for name, probe in probes.items():
        P.require(eq(probe.get("command"), spec["environment_policy"]["commands"][name])
                  and type(probe.get("timed_out")) is bool and type(probe.get("raw_complete")) is bool, "environment command/completion differs")
        if probe.get("status") == "available":
            P.require(type(probe.get("returncode")) is int and probe["returncode"] == 0
                      and probe["timed_out"] is False and probe["raw_complete"] is True and probe.get("reason") == "", "invalid available environment probe")
        else:
            P.require(probe.get("status") == "unavailable" and isinstance(probe.get("reason"), str) and probe["reason"], "unexplained unavailable environment probe")
        for stream in ("stdout", "stderr"):
            raw = probe.get(stream, {})
            data = base64.b64decode(raw.get("base64", ""), validate=True)
            P.require(P.integer(raw.get("bytes")) and raw["bytes"] == len(data) <= spec["environment_policy"]["max_retained_bytes_per_stream"]
                      and raw.get("sha256") == hashlib.sha256(data).hexdigest(), "environment raw probe identity differs")
    return snapshot


def collect_window(root, approval_sha256):
    """Run once under one live W4 timing lease; do not seal while it is running.

    Use --parent-approval-sha256-env so approval.window_command need not contain
    its own digest (which would be circular). The parent passes the pinned digest
    through that environment variable. All producer execution is sequential.
    Root later copies the *completed* outer queue receipt, then seals or records
    failure outside the timing job. No retries or calibration-driven early reruns.
    """
    root = Path(root).absolute()
    spec = P.read(P.safe_file(root, "preregister.json"))
    gate = validate_parent_gate(root, spec, approval_sha256)  # Before any producer.
    P.require(str(root) == spec["execution_root"], "collection must use the one approved original execution root")
    P.require(not any((root / name).exists() for name in ("result.json", "COMPLETE", "collection.json", "collection-failed.json")),
              "window execution cannot be reused")
    commands = root / "commands"
    P.require(not commands.exists() or commands.is_dir() and not commands.is_symlink() and not any(commands.iterdir()),
              "window command output already exists")
    context = active_queue(gate)
    validate_live_inputs(root, spec, gate, full_environment=True)
    jobs = schedule(spec)
    P.require(eq(P.read(P.safe_file(root, "schedule.json")), jobs), "live schedule differs")
    commands.mkdir(exist_ok=True)
    P.write_new(root / "collection.json", dict(schema_version=1, window_id=spec["window_id"], approval_sha256=approval_sha256,
                                              coordination=context, native_result_pending=True, timing_evidence=False))
    current_job = None
    try:
        P.write_new(root / "environment-before.json", environment_snapshot("before"))
        for job in jobs:
            current_job = job["id"]
            before = active_queue(gate)
            P.require(eq(before, context), "timing lease/request changed within window")
            producer = validate_live_inputs(root, spec, gate)
            directory = commands / job["id"]
            directory.mkdir()
            invocation = argv(spec, job)
            P.require(not (directory / "outputs").exists(), "producer output directory must be new")
            receipt = dict(job=job, argv=invocation, producer_identity=producer, coordination=before,
                           started_at=dt.datetime.now(dt.timezone.utc).isoformat(), started_monotonic_ns=time.monotonic_ns(),
                           returncode=None, timed_out=False)
            try:
                with (directory / "stdout").open("xb") as stdout, (directory / "stderr").open("xb") as stderr:
                    process = subprocess.run(invocation, stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr,
                                             timeout=spec["policy"]["child_timeout_seconds"], check=False)
                    receipt["returncode"] = process.returncode
            except subprocess.TimeoutExpired:
                receipt["timed_out"] = True
            except OSError as error:
                receipt["launch_error"] = str(error)
            finally:
                receipt.update(finished_monotonic_ns=time.monotonic_ns(), finished_at=dt.datetime.now(dt.timezone.utc).isoformat())
                for name in ("stdout", "stderr"):
                    identity = _identity(directory / name)
                    receipt.update({name + "_sha256": identity["sha256"], name + "_bytes": identity["bytes"]})
                P.write_new(directory / "receipt.json", receipt)
            P.require(receipt["returncode"] == 0 and type(receipt["returncode"]) is int and not receipt["timed_out"],
                      "producer failed/timed out; raw prefix retained")
            validate_live_inputs(root, spec, gate)
            P.require(eq(active_queue(gate), context), "timing lease changed after producer")
            expected = spec["expected_data"][job["workload"]]
            report = normalize(spec, job, (directory / "stdout").read_bytes(), (directory / "stderr").read_bytes(), receipt,
                               root / "data" / expected["input_sha256"], directory / "outputs")
            P.write_new(directory / "normalized.json", report)
        P.write_new(root / "environment-after.json", environment_snapshot("after"))
        validate_live_inputs(root, spec, gate, full_environment=True)
        P.write_new(root / "collection-finished.json", dict(status="collected", jobs=len(jobs), queue_completion_pending=True,
                                                           timing_evidence=False, adoption_allowed=False))
    except BaseException as error:
        P.write_new(root / "collection-failed.json", dict(status="failed", job_id=current_job, error=str(error),
                                                         raw_prefix_retained=True, queue_completion_pending=True,
                                                         timing_evidence=False, adoption_allowed=False))
        raise
    return dict(status="collected", jobs=len(jobs), queue_completion_pending=True, timing_evidence=False, adoption_allowed=False)


def accepted_preparation_build(build_root, acceptance_path, acceptance_sha256):
    """Read-only acceptance check; fixture prep cannot accept an arbitrary binary."""
    build_root = Path(build_root).absolute()
    P.require(acceptance_sha256 == G1_SHA and P.sha(acceptance_path) == G1_SHA, "not the parent-accepted W1 G1 record")
    acceptance = P.read(acceptance_path)
    P.require(acceptance.get("status") == "accepted_with_explicit_sanitizer_prerequisite"
              and acceptance.get("contract_sha256") == CONTRACT_SHA
              and acceptance.get("binaries", {}).get("timing", {}).get("sha256") == G1_BINARY_SHA,
              "W1 functional acceptance differs")
    production_root = Path(PRODUCTION_QUEUE).parents[1]
    P.require(str(build_root / "net_reference") == str(production_root / acceptance["binaries"]["timing"]["path"]), "wrong original accepted timing build path")
    result = P.read(P.safe_file(build_root, "result.json"))
    P.require(P.sha(build_root / "result.json") == G1_BUILD_SHA and result.get("status") == "passed"
              and result.get("kind") == "build" and result.get("measurement_profile") == "timing"
              and result.get("sanitizer") == "none" and result.get("binary_sha256") == G1_BINARY_SHA
              and result.get("build_root") == str(build_root), "accepted timing build result differs")
    P.require(P.safe_file(build_root, "COMPLETE").read_text().strip() == G1_BUILD_SHA
              and eq({k: v["sha256"] for k, v in P.inventory(build_root, ("result.json", "COMPLETE")).items()}, result["files"]),
              "accepted build archive changed")
    P.require(P.sha(P.safe_file(build_root, "net_reference")) == G1_BINARY_SHA
              and P.sha(P.safe_file(build_root, "source/net_reference.cpp")) == G1_SOURCE_SHA
              and P.sha(P.safe_file(build_root, "source/contract.json")) == CONTRACT_SHA,
              "accepted executable/source bytes changed")
    return dict(binary_sha256=G1_BINARY_SHA, source_inventory_sha256=P.sha(build_root / "source-inventory.json"),
                toolchain_sha256=P.sha(build_root / "toolchain.json"), dependencies_sha256=P.sha(build_root / "dependencies.json"))


def prepare_fixtures(out, build_root, acceptance_path, acceptance_sha256, queue_out, queue_timeout_seconds=180.0):
    """Functional-only four fixtures + one canonical exchange each, never timing.

    Exactly eight W1 invocations run under one *functional* queue lease. The
    first full processed output supplies the canonical bytes for later, separate
    preregistration. No elapsed values from this step are performance evidence.
    """
    out, build_root = Path(out).absolute(), Path(build_root).absolute()
    producer = accepted_preparation_build(build_root, acceptance_path, acceptance_sha256)
    collector_runtime = collector_runtime_identity()
    gate = dict(window_command=[sys.executable, *sys.argv], queue_lock_dir=PRODUCTION_QUEUE,
                queue_receipt_out=_absolute(str(Path(queue_out).absolute())), queue_timeout_seconds=queue_timeout_seconds)
    context = active_queue(gate, kind="functional")
    out.mkdir(parents=True, exist_ok=False)
    with (out / "parent-G1.json").open("xb") as stream:
        stream.write(Path(acceptance_path).read_bytes())
    (out / "protocol").mkdir()
    for name, source in (("network_measurement.py", __file__), ("paired_measurement.py", P.__file__)):
        with (out / "protocol" / name).open("xb") as stream:
            stream.write(Path(source).read_bytes())
    P.write_new(out / "protocol/collector-runtime.json", collector_runtime)
    plan = dict(schema="w1-functional-fixture-preparation", schema_version=1, evidence_use="functional_only",
                cases=preregistration()["workloads"], build_execution_root=str(build_root), producer_identity=producer,
                timing_evidence=False, adoption_allowed=False, collector_runtime=collector_runtime)
    P.write_new(out / "preregister.json", plan)
    live_spec = dict(build_execution_root=str(build_root), producer_identity=producer, expected_data={}, collector_runtime=collector_runtime)
    live_gate = dict(preregister_sha256=P.sha(out / "preregister.json"),
                     accepted_inputs={"protocol/network_measurement.py": dict(sha256=P.sha(__file__)),
                                      "protocol/paired_measurement.py": dict(sha256=P.sha(P.__file__))})
    validate_live_inputs(out, live_spec, live_gate, full_environment=True)
    expected_data, cases = {}, []

    def execute(invocation, directory):
        P.require(eq(active_queue(gate, kind="functional"), context), "fixture functional lease changed")
        validate_live_inputs(out, live_spec, live_gate)
        directory.mkdir(parents=True, exist_ok=False)
        receipt = dict(argv=invocation, coordination=context, producer_identity=producer, returncode=None, timed_out=False,
                       started_monotonic_ns=time.monotonic_ns(), started_at=dt.datetime.now(dt.timezone.utc).isoformat())
        try:
            with (directory / "stdout").open("xb") as stdout, (directory / "stderr").open("xb") as stderr:
                process = subprocess.run(invocation, stdout=stdout, stderr=stderr, stdin=subprocess.DEVNULL,
                                         timeout=min(120, queue_timeout_seconds), check=False)
                receipt["returncode"] = process.returncode
        except subprocess.TimeoutExpired:
            receipt["timed_out"] = True
        finally:
            receipt.update(finished_monotonic_ns=time.monotonic_ns(), finished_at=dt.datetime.now(dt.timezone.utc).isoformat())
            for name in ("stdout", "stderr"):
                identity = _identity(directory / name)
                receipt.update({name + "_sha256": identity["sha256"], name + "_bytes": identity["bytes"]})
            P.write_new(directory / "receipt.json", receipt)
        P.require(receipt["returncode"] == 0 and type(receipt["returncode"]) is int and receipt["timed_out"] is False,
                  "fixture preparation command failed; raw bytes retained")
        validate_live_inputs(out, live_spec, live_gate)
        raw = P.parse((directory / "stdout").read_bytes())
        P.require(raw.get("status") == "passed" and raw.get("diagnostic") is False and raw.get("timing_evidence") is False
                  and type(raw.get("schema_version")) is int and raw["schema_version"] == 1
                  and raw.get("contract_version") == "1.0.0", "fixture preparation raw profile/status differs")
        return raw, receipt

    try:
        for work in plan["cases"]:
            case_root = out / "cases" / work["id"]
            case_root.mkdir(parents=True)
            original_input = case_root / "input.wire"
            fixture_argv = [str(build_root / "net_reference"), "fixture", "--output", str(original_input), "--elements", str(work["elements"])]
            fixture_raw, _ = execute(fixture_argv, case_root / "fixture-command")
            P.require(fixture_raw.get("command") == "fixture" and type(fixture_raw.get("element_count")) is int
                      and fixture_raw["element_count"] == work["elements"] and type(fixture_raw.get("frame_bytes")) is int
                      and fixture_raw["frame_bytes"] == original_input.stat().st_size, "fixture frame/count differs")
            input_identity = _identity(original_input)
            (case_root / "data").mkdir()
            canonical_input = case_root / "data" / input_identity["sha256"]
            with canonical_input.open("xb") as stream:
                stream.write(original_input.read_bytes())
            case_spec = preregistration()
            case_spec.update(window_id="fixture-" + work["id"], purpose="screening", evidence_class="native", ready=True,
                             evidence_use="functional_only", producer_identity=producer, execution_root=str(case_root),
                             build_execution_root=str(build_root), queue_receipt_out=str(Path(queue_out).absolute()), collector_runtime=collector_runtime)
            case_spec["workloads"] = [dict(work, iterations=1)]
            case_spec["comparisons"] = [case_spec["comparisons"][0]]
            jobs = schedule(case_spec)
            case_spec["paths"] = dict(binary=str(build_root / "net_reference"), inputs={work["id"]: str(canonical_input)},
                                       outputs={j["id"]: str(case_root / "commands" / j["id"] / "outputs") for j in jobs})
            case_spec["invocations"] = {j["id"]: argv(case_spec, j) for j in jobs}
            job = jobs[0]
            directory = case_root / "commands" / job["id"]
            raw, receipt = execute(argv(case_spec, job), directory)
            output_identity = _identity(directory / "outputs/output-0.wire")
            identity = dict(input_sha256=input_identity["sha256"], output_sha256=output_identity["sha256"],
                            input_bytes=input_identity["bytes"], output_bytes=output_identity["bytes"],
                            semantic_checksum=raw.get("semantic_checksum"), elements=work["elements"],
                            schema_id=case_spec["controls"]["schema_id"], processing_id=case_spec["controls"]["processing_id"])
            case_spec["expected_data"] = {work["id"]: identity}
            case_spec["retained_output_bytes_expected"] = sum(j["iterations"] * output_identity["bytes"] for j in jobs)
            receipt["job"] = job
            # Preserve the original pre-normalization receipt and add an exact
            # adapter receipt; no producer evidence is replaced.
            P.write_new(directory / "adapter-receipt.json", receipt)
            normalized = normalize(case_spec, job, (directory / "stdout").read_bytes(), (directory / "stderr").read_bytes(), receipt,
                                   canonical_input, directory / "outputs")
            P.write_new(directory / "normalized.json", normalized)
            P.write_new(case_root / "case-spec.json", case_spec)
            with (case_root / "data" / output_identity["sha256"]).open("xb") as stream:
                stream.write((directory / "outputs/output-0.wire").read_bytes())
            expected_data[work["id"]] = identity
            cases.append(dict(id=work["id"], canonical_input=str(canonical_input), canonical_output=str(case_root / "data" / output_identity["sha256"])))
        validate_live_inputs(out, live_spec, live_gate, full_environment=True)
        record = dict(schema="w1-functional-fixture-preparation", schema_version=1, status="prepared", timing_evidence=False,
                      adoption_allowed=False, queue_completion_pending=True, coordination=context, queue_gate=gate,
                      parent_G1_sha256=acceptance_sha256, producer_identity=producer, expected_data=expected_data, cases=cases,
                      files=P.inventory(out))
        P.write_new(out / "fixture-preparation.json", record)
        return dict(status="prepared", fixtures=len(cases), native_commands=8, timing_evidence=False, queue_completion_pending=True)
    except BaseException as error:
        P.write_new(out / "preparation-failed.json", dict(status="failed", error=str(error), raw_prefix_retained=True,
                                                         timing_evidence=False, adoption_allowed=False))
        raise


def audit_fixture_preparation(root, require_queue=True, accepted_execution_protocol_sha256=None, queue_root=None):
    """Offline/read-only audit using the exact frozen preparation protocol.

    An explicit exact c53 digest permits supplementary audit of that accepted
    functional-only execution. Its reviewed raw normalizer runs from the exact
    retained bytes; this auditor independently checks plan, intervals and queue.
    Missing historical interpreter provenance stays unrecorded. No timing gate
    or v2 validation is relaxed, and no original file is edited or resealed.
    require_queue=False is a unit-test/raw-data inspection mode and its
    result explicitly does not verify the native functional queue prerequisite.
    """
    root = Path(root)
    record = P.read(P.safe_file(root, "fixture-preparation.json"))
    P.require(record.get("schema") == "w1-functional-fixture-preparation" and type(record.get("schema_version")) is int
              and record["schema_version"] == 1 and record.get("status") == "prepared"
              and record.get("timing_evidence") is False and record.get("adoption_allowed") is False
              and not (root / "COMPLETE").exists(), "wrong fixture preparation scope/status")
    inventory = {k: v for k, v in P.inventory(root, ("fixture-preparation.json",)).items() if not k.startswith("queue/")}
    P.require(eq(inventory, record.get("files")), "fixture preparation inventory changed")
    P.require(record.get("parent_G1_sha256") == G1_SHA and P.sha(P.safe_file(root, "parent-G1.json")) == G1_SHA,
              "fixture preparation parent acceptance differs")
    parent = P.read(root / "parent-G1.json")
    producer = record.get("producer_identity", {})
    P.require(parent.get("status") == "accepted_with_explicit_sanitizer_prerequisite"
              and parent.get("contract_sha256") == CONTRACT_SHA
              and parent.get("binaries", {}).get("timing", {}).get("sha256") == G1_BINARY_SHA
              and producer.get("binary_sha256") == G1_BINARY_SHA
              and set(producer) == set(PRODUCER_KEYS) and all(P.digest(v) for v in producer.values()), "fixture producer acceptance differs")
    protocol_sha = P.sha(P.safe_file(root, "protocol/network_measurement.py"))
    approved_protocol = accepted_execution_protocol_sha256 or P.sha(__file__)
    P.require(approved_protocol in (P.sha(__file__), C53_FUNCTIONAL_PROTOCOL_SHA) and protocol_sha == approved_protocol
              and P.sha(P.safe_file(root, "protocol/paired_measurement.py")) == P.sha(P.__file__), "unreviewed fixture preparation protocol")
    historical = approved_protocol == C53_FUNCTIONAL_PROTOCOL_SHA
    adapter = None
    if historical:
        P.require(accepted_execution_protocol_sha256 == C53_FUNCTIONAL_PROTOCOL_SHA
                  and P.sha(P.__file__) == C53_PAIRED_PROTOCOL_SHA, "c53 supplementary audit requires explicit exact protocol pins")
        source_path = P.safe_file(root, "protocol/network_measurement.py")
        source = source_path.read_bytes()
        P.require(hashlib.sha256(source).hexdigest() == C53_FUNCTIONAL_PROTOCOL_SHA, "historical execution source changed")
        adapter = types.ModuleType("_accepted_c53_functional_adapter")
        adapter.__file__ = str(source_path)
        original_path = list(sys.path)
        paired_was_loaded = "paired_measurement" in sys.modules
        previous_paired = sys.modules.get("paired_measurement")
        try:
            sys.modules["paired_measurement"] = P
            exec(compile(source, str(source_path), "exec"), adapter.__dict__)
            P.require(adapter.P is P and P.sha(adapter.P.__file__) == C53_PAIRED_PROTOCOL_SHA,
                      "historical normalizer imported an unpinned paired protocol")
        finally:
            sys.path[:] = original_path
            if paired_was_loaded:
                sys.modules["paired_measurement"] = previous_paired
            else:
                sys.modules.pop("paired_measurement", None)
    plan = P.read(P.safe_file(root, "preregister.json"))
    P.require(plan.get("schema") == "w1-functional-fixture-preparation" and plan.get("evidence_use") == "functional_only"
              and type(plan.get("schema_version")) is int and plan["schema_version"] == 1
              and plan.get("adoption_allowed") is False and plan.get("timing_evidence") is False and eq(plan.get("producer_identity"), producer)
              and eq(plan.get("cases"), preregistration()["workloads"]), "functional fixture plan differs")
    if historical:
        P.require("collector_runtime" not in plan and not (root / "protocol/collector-runtime.json").exists(),
                  "c53 historical runtime must remain unrecorded")
    else:
        P.require(eq(validate_collector_runtime(plan.get("collector_runtime")), P.read(P.safe_file(root, "protocol/collector-runtime.json"))),
                  "functional collector runtime differs")
    expected_ids = [w["id"] for w in plan["cases"]]
    cases = record.get("cases", [])
    P.require(isinstance(cases, list) and [c.get("id") for c in cases] == expected_ids
              and set(record.get("expected_data", {})) == set(expected_ids), "fixture exact case inventory differs")
    context = record.get("coordination", {})
    queue_identity = None
    if require_queue:
        queue_directory = Path(queue_root) if queue_root is not None else root / "queue"
        queue = validate_queue(queue_directory, {}, record["queue_gate"], kind="functional")
        queue_identity = {key: queue[key] for key in ("manifest_sha256", "request_sha256", "result_sha256")}
        P.require(context.get("request_sha256") == queue["request_sha256"]
                  and eq(context.get("collector_argv"), record["queue_gate"]["window_command"])
                  and context.get("lock_was_held") is True and context.get("explicit_flag") is True
                  and context.get("lock_path") == PRODUCTION_QUEUE + "/queue.lock"
                  and eq(context.get("ancestor_pids"), [queue["command_pid"], context.get("lock_metadata", {}).get("supervisor_pid")])
                  and context.get("lock_metadata", {}).get("run_id") == queue["run_id"], "fixture live functional request identity differs")
    previous_end, previous_utc_end, first_start, first_utc_start = 0, None, None, None

    def check_interval(receipt):
        nonlocal previous_end, previous_utc_end, first_start, first_utc_start
        start, end = receipt.get("started_monotonic_ns"), receipt.get("finished_monotonic_ns")
        utc_start, utc_end = _timestamp(receipt.get("started_at")), _timestamp(receipt.get("finished_at"))
        P.require(P.integer(start, 1) and P.integer(end, start + 1) and start >= previous_end
                  and utc_start <= utc_end and (previous_utc_end is None or utc_start >= previous_utc_end),
                  "functional receipt reversed/overlapping/nonpositive interval")
        P.require(end - start <= 120 * 10**9, "functional child timeout bound exceeded")
        if require_queue:
            P.require(queue["begin"] <= utc_start <= utc_end <= queue["end"], "functional receipt outside enclosing queue")
        if first_start is None:
            first_start, first_utc_start = start, utc_start
        previous_end, previous_utc_end = end, utc_end

    for case, work in zip(cases, plan["cases"]):
        case_root = root / "cases" / case["id"]
        spec = P.read(P.safe_file(case_root, "case-spec.json"))
        P.require(spec.get("evidence_use") == "functional_only" and spec["workloads"] == [dict(work, iterations=1)]
                  and eq(spec.get("producer_identity"), producer)
                  and eq(spec.get("expected_data"), {work["id"]: record["expected_data"][work["id"]]}), "fixture case/data plan differs")
        if historical:
            P.require("collector_runtime" not in spec, "historical case runtime must remain unrecorded")
        else:
            P.require(eq(spec.get("collector_runtime"), plan["collector_runtime"]), "fixture case collector runtime differs")
        job = schedule(spec)[0]
        identity = spec["expected_data"][work["id"]]
        for direction in ("input", "output"):
            path = P.safe_file(case_root, "data/" + identity[direction + "_sha256"])
            P.require(eq(_identity(path), dict(sha256=identity[direction + "_sha256"], bytes=identity[direction + "_bytes"])), "fixture full canonical bytes differ")
            P.require(case["canonical_" + direction] == spec["execution_root"] + "/data/" + identity[direction + "_sha256"], "fixture canonical path differs")
        P.require(P.sha(P.safe_file(case_root, "input.wire")) == identity["input_sha256"], "fixture original input differs")
        fixture = case_root / "fixture-command"
        fixture_raw = P.parse(P.safe_file(fixture, "stdout").read_bytes())
        receipt = P.read(P.safe_file(fixture, "receipt.json"))
        fixture_argv = [spec["paths"]["binary"], "fixture", "--output", spec["execution_root"] + "/input.wire", "--elements", str(work["elements"])]
        P.require(eq(receipt.get("argv"), fixture_argv) and type(receipt.get("returncode")) is int and receipt["returncode"] == 0
                  and receipt.get("timed_out") is False and eq(receipt.get("coordination"), context)
                  and eq(receipt.get("producer_identity"), producer), "fixture command receipt differs")
        check_interval(receipt)
        for stream in ("stdout", "stderr"):
            raw_file = P.safe_file(fixture, stream)
            P.require(receipt.get(stream + "_sha256") == P.sha(raw_file)
                      and type(receipt.get(stream + "_bytes")) is int and receipt[stream + "_bytes"] == raw_file.stat().st_size,
                      "fixture raw stdout/stderr attribution differs")
        P.require(type(fixture_raw.get("schema_version")) is int and fixture_raw["schema_version"] == 1
                  and fixture_raw.get("contract_version") == "1.0.0" and fixture_raw.get("command") == "fixture"
                  and fixture_raw.get("status") == "passed" and fixture_raw.get("diagnostic") is False and fixture_raw.get("timing_evidence") is False
                  and type(fixture_raw.get("element_count")) is int and fixture_raw["element_count"] == work["elements"]
                  and type(fixture_raw.get("frame_bytes")) is int and fixture_raw["frame_bytes"] == identity["input_bytes"], "fixture stdout frame/profile differs")
        directory = case_root / "commands" / job["id"]
        receipt = P.read(P.safe_file(directory, "adapter-receipt.json"))
        P.require(eq({k: v for k, v in receipt.items() if k != "job"}, P.read(P.safe_file(directory, "receipt.json")))
                  and eq(receipt.get("coordination"), context), "canonical exchange original receipt differs")
        check_interval(receipt)
        normalize_raw = adapter.normalize if historical else normalize
        normalized = normalize_raw(spec, job, P.safe_file(directory, "stdout").read_bytes(), P.safe_file(directory, "stderr").read_bytes(), receipt,
                                   case_root / "data" / identity["input_sha256"], directory / "outputs")
        P.require(eq(normalized, P.read(P.safe_file(directory, "normalized.json"))), "canonical exchange normalized report differs")
    if require_queue:
        metadata = context.get("lock_metadata", {})
        P.require(metadata.get("status") == "running" and metadata.get("owner") == "W3" and metadata.get("kind") == "functional"
                  and P.integer(metadata.get("supervisor_pid"), 1) and metadata.get("out") == queue["request"]["out"]
                  and metadata.get("started_at") == queue["started_at"]
                  and previous_end - first_start <= queue["elapsed"] * 10**9
                  and (previous_utc_end - first_utc_start).total_seconds() <= queue["elapsed"],
                  "functional queue context/elapsed enclosure differs")
    return dict(status="fixture_preparation_audited", fixtures=len(cases), timing_evidence=False, adoption_allowed=False,
                functional_queue_verified=require_queue, original_absolute_paths_accessed=False,
                functional_queue_identity=queue_identity,
                execution_protocol_sha256=protocol_sha, auditor_protocol_sha256=P.sha(__file__), paired_protocol_sha256=P.sha(P.__file__),
                collector_runtime_provenance=dict(status="unrecorded_in_functional_v1" if historical else "recorded",
                                                   retrospective_identity_inferred=False, timing_runtime_requirement_waived=False))


def collect_native(root, approval_sha256):
    root = Path(root)
    spec = P.read(P.safe_file(root, "preregister.json"))
    gate = validate_parent_gate(root, spec, approval_sha256)
    queue = validate_queue(root / "queue", spec, gate)
    collection = P.read(P.safe_file(root, "collection.json"))
    P.require(collection.get("approval_sha256") == approval_sha256 and collection.get("window_id") == spec["window_id"]
              and collection.get("coordination", {}).get("request_sha256") == queue["request_sha256"]
              and eq(collection.get("coordination", {}).get("collector_argv"), gate["window_command"])
              and not (root / "collection-failed.json").exists(), "collection start/request context differs")
    jobs = schedule(spec)
    environment_before = validate_environment(P.read(P.safe_file(root, "environment-before.json")), "before", spec)
    environment_after = validate_environment(P.read(P.safe_file(root, "environment-after.json")), "after", spec)
    P.require(eq(environment_before["host"], environment_after["host"]), "host identity changed during window")
    P.require(eq(P.read(P.safe_file(root, "collection-finished.json")), dict(status="collected", jobs=len(jobs), queue_completion_pending=True,
                                                                          timing_evidence=False, adoption_allowed=False)), "collection did not finish exact schedule")
    P.require(eq(P.read(P.safe_file(root, "schedule.json")), jobs), "native schedule differs")
    P.require(set(p.name for p in (root / "commands").iterdir()) == {j["id"] for j in jobs}, "native command inventory differs")
    reports, previous_end, first_start, supervisor = {}, 0, None, None
    for job in jobs:
        directory = root / "commands" / job["id"]
        receipt = P.read(P.safe_file(directory, "receipt.json"))
        coordination = receipt.get("coordination", {})
        metadata = coordination.get("lock_metadata", {})
        P.require(coordination.get("lock_was_held") is True and coordination.get("explicit_flag") is True
                  and eq(coordination, collection["coordination"])
                  and coordination.get("lock_path") == gate["queue_lock_dir"] + "/queue.lock"
                  and metadata.get("status") == "running" and metadata.get("owner") == "W3"
                  and metadata.get("kind") == "timing" and metadata.get("run_id") == queue["run_id"]
                  and metadata.get("out") == queue["request"].get("out")
                  and metadata.get("started_at") == queue["started_at"]
                  and type(metadata.get("supervisor_pid")) is int
                  and eq(coordination.get("ancestor_pids"), [queue["command_pid"], metadata["supervisor_pid"]]), "child outside single timing lease")
        if supervisor is None:
            supervisor = metadata["supervisor_pid"]
        P.require(supervisor == metadata["supervisor_pid"], "window uses multiple queue supervisors")
        begin, end = _timestamp(receipt.get("started_at")), _timestamp(receipt.get("finished_at"))
        P.require(queue["begin"] <= begin <= end <= queue["end"], "child outside enclosing queue timestamps")
        expected_data = spec["expected_data"][job["workload"]]
        report = normalize(spec, job, P.safe_file(directory, "stdout").read_bytes(), P.safe_file(directory, "stderr").read_bytes(), receipt,
                           P.safe_file(root, "data/" + expected_data["input_sha256"]), directory / "outputs")
        P.require(eq(report, P.read(P.safe_file(directory, "normalized.json"))), "normalized report does not recompute from raw/full bytes")
        start, finish = receipt["started_monotonic_ns"], receipt["finished_monotonic_ns"]
        P.require(start >= previous_end, "child jobs overlap/reorder")
        previous_end = finish
        if first_start is None:
            first_start = start
        expected_names = {"stdout", "stderr", "receipt.json", "normalized.json"} | {"outputs/output-%d.wire" % i for i in range(job["iterations"])}
        P.require(set(P.inventory(directory)) == expected_names, "raw/normalized/output case set differs")
        for direction in ("input", "output"):
            data = _identity(P.safe_file(root, "data/" + expected_data[direction + "_sha256"]))
            P.require(eq(data, dict(sha256=expected_data[direction + "_sha256"], bytes=expected_data[direction + "_bytes"])), "retained canonical data differs")
        reports[job["id"]] = report
    P.require(previous_end - first_start <= min(spec["policy"]["max_window_seconds"], queue["elapsed"]) * 10**9,
              "window exceeds enclosing elapsed/process budget")
    first_receipt = P.read(root / "commands" / jobs[0]["id"] / "receipt.json")
    last_receipt = P.read(root / "commands" / jobs[-1]["id"] / "receipt.json")
    P.require(queue["begin"] <= _timestamp(environment_before["captured_at"]) <= _timestamp(first_receipt["started_at"])
              and _timestamp(last_receipt["finished_at"]) <= _timestamp(environment_after["captured_at"]) <= queue["end"]
              and environment_before["monotonic_ns"] <= first_start <= previous_end <= environment_after["monotonic_ns"],
              "environment snapshots do not bracket producer window")
    return summarize(spec, jobs, reports)


def seal_native(root, approval_sha256):
    """Root calls only after the outer queue has completed and was archived."""
    root = Path(root)
    P.require(not (root / "result.json").exists() and not (root / "COMPLETE").exists(), "native output already used")
    summary = collect_native(root, approval_sha256)
    P.write_new(root / "summary.json", summary)
    P.write_new(root / "result.json", dict(schema_version=1, schema="w1-native-network-window", status="audited",
                                           approval_sha256=approval_sha256, files=P.inventory(root, ("result.json", "COMPLETE")),
                                           adoption_allowed=False, timing_claim_qualified=summary["all_stable_windows_qualified"]))
    with (root / "COMPLETE").open("x") as stream:
        stream.write(P.sha(root / "result.json") + "\n")
    return audit_native(root, approval_sha256)


def mark_failed_native(root, approval_sha256, error):
    """Preserve an attempted window, including invalid/partial producer bytes.

    The root archives the outer terminal queue receipt before calling this.
    Failure audit verifies frozen gate + queue + raw prefix inventory, and makes
    no claim that partial or invalid producer JSON is a successful observation.
    """
    root = Path(root)
    P.require(not (root / "result.json").exists() and not (root / "COMPLETE").exists(), "native output already used")
    spec = P.read(P.safe_file(root, "preregister.json"))
    gate = validate_parent_gate(root, spec, approval_sha256)
    validate_queue(root / "queue", spec, gate, allow_failed=True)
    P.require(isinstance(error, str) and error, "missing native failure reason")
    P.write_new(root / "result.json", dict(schema_version=1, schema="w1-native-network-window", status="failed", error=error,
                                           approval_sha256=approval_sha256, files=P.inventory(root, ("result.json", "COMPLETE")),
                                           adoption_allowed=False, timing_claim_qualified=False))
    return audit_native(root, approval_sha256)


def audit_native(root, approval_sha256):
    """Offline/read-only; original absolute paths are checked as descriptions."""
    root = Path(root)
    result = P.read(P.safe_file(root, "result.json"))
    P.require(type(result.get("schema_version")) is int and result["schema_version"] == 1
              and result.get("schema") == "w1-native-network-window" and result.get("status") in ("audited", "failed")
              and result.get("adoption_allowed") is False and result.get("approval_sha256") == approval_sha256,
              "wrong native result/parent approval")
    P.require(eq(P.inventory(root, ("result.json", "COMPLETE")), result.get("files")), "native full inventory differs")
    if result["status"] == "failed":
        P.require(not (root / "COMPLETE").exists() and isinstance(result.get("error"), str) and result["error"]
                  and result.get("timing_claim_qualified") is False, "failed window has success claim")
        spec = P.read(P.safe_file(root, "preregister.json"))
        gate = validate_parent_gate(root, spec, approval_sha256)
        queue = validate_queue(root / "queue", spec, gate, allow_failed=True)
        jobs = schedule(spec)
        commands = root / "commands"
        P.require(not commands.exists() or commands.is_dir() and not commands.is_symlink(), "unsafe failed-window raw directory")
        names = sorted(p.name for p in commands.iterdir()) if commands.exists() else []
        P.require(names == [j["id"] for j in jobs[:len(names)]], "failed command evidence is not an exact schedule prefix")
        P.require(queue["executed"] or not names, "zero-execution queue has producer evidence")
        return dict(status="failed_preserved", timing_evidence=False, adoption_allowed=False, error=result["error"],
                    queue_status=queue["job_status"], raw_job_prefix=len(names))
    P.require(P.safe_file(root, "COMPLETE").read_text().strip() == P.sha(root / "result.json"), "native completion differs")
    summary = collect_native(root, approval_sha256)
    P.require(eq(P.read(P.safe_file(root, "summary.json")), summary)
              and eq(result.get("timing_claim_qualified"), summary["all_stable_windows_qualified"]), "native summary does not recompute")
    return dict(status="audited", evidence_class="native-posix-tcp-loopback", all_aa_qualified=summary["all_aa_qualified"],
                stable_window_qualified=summary["all_stable_windows_qualified"],
                adoption_allowed=False, whole_lifecycle_available=False, independent_confirmation_windows_required=3)


def preregistration():
    spec = P.template("network")
    spec.update(adapter_schema="w1-network-revision2-v1", contract_sha256=CONTRACT_SHA,
                model_subcases_sha256=SUBCASES_SHA, gate_revision=2, contract_version="1.0.0", timing_unit="ns",
                metric_boundary=PRIMARY_BOUNDARY, timeout_ms=5000, purpose="calibration", producer_identity={},
                expected_data={}, paths={}, invocations={}, parent_approval_schema=APPROVAL_SCHEMA,
                runtime_status="waiting_accepted_W1_timing_build_and_functionality_and_parent_preregistration_gate",
                execution_root=None, build_execution_root=None, queue_receipt_out=None,
                max_retained_output_bytes=MAX_RETAINED_OUTPUT, retained_output_bytes_expected=None,
                collector_runtime=None,
                environment_policy=ENVIRONMENT_POLICY,
                whole_lifecycle_available=False, unmeasured_costs=list(UNMEASURED),
                exclusions=["pool preparation", "connection setup including worker creation", "warmup", *UNMEASURED])
    spec["controls"].update(connection="one persistent TCP IPv4 loopback connection, one in-flight request",
                            schema_id="n7.reference.file-pipeline.v1",
                            processing_id="fixture_value/processed_value/markers/labels/exact-values-v1",
                            max_inflight_bytes=4 * spec["controls"]["max_wire_bytes"],
                            pool_idle_reserved_bytes=2 * (spec["controls"]["owner_capacity_bytes"] + CHUNK))
    spec["policy"]["absolute_anchor_drift_relative_limit"] = .01
    return spec


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="action", required=True)
    p = commands.add_parser("check-preregister")
    p.add_argument("path", type=Path)
    p.add_argument("--bound", action="store_true")
    p = commands.add_parser("approval-schema")
    p = commands.add_parser("audit")
    p.add_argument("path", type=Path)
    p.add_argument("--parent-approval-sha256", required=True)
    p = commands.add_parser("collect-window")
    p.add_argument("path", type=Path)
    p.add_argument("--parent-approval-sha256-env", required=True,
                   help="environment variable carrying the parent's out-of-band pinned approval digest")
    p = commands.add_parser("prepare-fixtures")
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--accepted-build", type=Path, required=True)
    p.add_argument("--acceptance", type=Path, required=True)
    p.add_argument("--acceptance-sha256", required=True)
    p.add_argument("--queue-out", type=Path, required=True)
    p.add_argument("--queue-timeout-seconds", type=float, default=180.0)
    p = commands.add_parser("audit-fixtures")
    p.add_argument("path", type=Path)
    p.add_argument("--accepted-execution-protocol-sha256")
    p.add_argument("--queue-root", type=Path)
    args = parser.parse_args()
    if args.action == "check-preregister":
        validate_preregistration(P.read(args.path), bound=args.bound)
        result = dict(status="preregistration_validated", ready=P.read(args.path)["ready"], native_executed=False)
    elif args.action == "approval-schema":
        result = APPROVAL_SCHEMA
    elif args.action == "collect-window":
        result = collect_window(args.path, os.environ.get(args.parent_approval_sha256_env))
    elif args.action == "prepare-fixtures":
        result = prepare_fixtures(args.out, args.accepted_build, args.acceptance, args.acceptance_sha256,
                                  args.queue_out, args.queue_timeout_seconds)
    elif args.action == "audit-fixtures":
        result = audit_fixture_preparation(args.path, accepted_execution_protocol_sha256=args.accepted_execution_protocol_sha256,
                                           queue_root=args.queue_root)
    else:
        result = audit_native(args.path, args.parent_approval_sha256)
    print(P.canonical(result).decode(), end="")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, StopIteration) as error:
        print("network measurement rejected: " + str(error), file=sys.stderr)
        sys.exit(1)
