#!/usr/bin/env python3
"""Bind a new network calibration or selected pool-screening proposal.

Root runs bind under its validation queue after review. Inputs remain read-only;
all copies and records are exclusively created in one new window directory.
The independent parent later supplies approval.json and its out-of-band digest.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import shutil
import sys

sys.dont_write_bytecode = True
ENVIRONMENT_DIGEST = "VM_NETWORK_APPROVAL_SHA256"
PREFIXES = ("contract/", "protocol/", "w1-build/", "w1-functional/", "data/")
CALIBRATION_JOBS = 112
CALIBRATION_CONDITIONS = {name: dict(variant="F", pool="none", profile="timing") for name in ("F-none", "F-none-B")}
CALIBRATION_CONTRAST = [dict(id="F-AA", left="F-none", right="F-none-B", calibration="F-none")]
W1_MANIFEST_MEMBER = "docs/validation/vm-network/final-v2/manifest.json"
W1_BUILD_RESULT_MEMBER = "build/vm-network/timing-v2/result.json"
W1_FUNCTIONAL_RESULT_MEMBER = "build/vm-network/smoke-timing-v2/result.json"
MODES = ("calibration", "pool-screening-1m")
SCREENING_WORKLOAD = dict(id="1048576", nominal_bytes=1048576, elements=262144, iterations=4)
SCREENING_JOBS = 88
SCREENING_OUTPUT_BYTES = 369222656
SELECTION_SCOPE = "prior one-MiB pass selects scope only; each new contrast requires its own three-anchor A/A and left/right/pooled drift gates"


def require(value, message):
    if not value:
        raise ValueError(message)


def sha(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as source:
        for data in iter(lambda: source.read(1024 * 1024), b""):
            result.update(data)
    return result.hexdigest()


def identity(path):
    path = Path(path)
    require(path.is_file() and not path.is_symlink(), "missing/unsafe input: " + str(path))
    return dict(sha256=sha(path), bytes=path.stat().st_size)


def absolute(path):
    path = Path(path)
    require(path.is_absolute() and str(path) == os.path.normpath(str(path)) and ".." not in path.parts,
            "path must be original canonical absolute path")
    for ancestor in (path, *path.parents):
        require(not ancestor.is_symlink(), "symlink in source/output path")
    return path


def disjoint_new_outputs(out, queue, sources):
    out, queue = absolute(out), absolute(queue)
    require(not out.exists() and not queue.exists(), "window/queue output already exists")
    for left, right in ((out, queue), *((target, source) for target in (out, queue) for source in sources)):
        left, right = absolute(left), absolute(right)
        require(left != right and left not in right.parents and right not in left.parents,
                "output overlaps another output or accepted source")
    return out, queue


def load_protocol(tools, network_sha256, paired_sha256):
    """Load only externally pinned frozen Python bytes, without bytecode writes."""
    tools = absolute(tools)
    require(identity(tools / "network_measurement.py")["sha256"] == network_sha256
            and identity(tools / "paired_measurement.py")["sha256"] == paired_sha256,
            "frozen network/paired source differs from external pin")
    old_path = list(sys.path)
    old_paired = sys.modules.get("paired_measurement")
    try:
        module_spec = importlib.util.spec_from_file_location("paired_measurement", tools / "paired_measurement.py")
        paired = importlib.util.module_from_spec(module_spec)
        sys.modules["paired_measurement"] = paired
        module_spec.loader.exec_module(paired)
        module_spec = importlib.util.spec_from_file_location("_bound_frozen_network", tools / "network_measurement.py")
        network = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(network)
    finally:
        sys.path[:] = old_path
        if old_paired is None:
            sys.modules.pop("paired_measurement", None)
        else:
            sys.modules["paired_measurement"] = old_paired
    require(hasattr(network, "collector_runtime_identity"), "network protocol lacks reviewed collector runtime pin")
    return network


def verify_tools(N, tools):
    P = N.P
    manifest_path = tools / "source-inventory.json"
    manifest = P.read(manifest_path)
    require(isinstance(manifest, dict) and {"network_measurement.py", "paired_measurement.py"} <= set(manifest),
            "missing frozen source inventory")
    for name, digest in manifest.items():
        require(P.digest(digest) and sha(P.safe_file(tools, name)) == digest, "frozen tool inventory differs")
    require(set(P.inventory(tools)) == set(manifest) | {"source-inventory.json"}, "extra/missing frozen tool files")
    return manifest


def archive_result(N, root, kind):
    P = N.P
    result = P.read(P.safe_file(root, "result.json"))
    require(type(result.get("schema_version")) is int and result["schema_version"] == 1
            and result.get("kind") == kind and result.get("status") == "passed"
            and result.get("timing_evidence") is False, "unsuccessful W1 archive prerequisite")
    require(P.safe_file(root, "COMPLETE").read_text().strip() == sha(root / "result.json"), "W1 completion digest differs")
    actual = {name: entry["sha256"] for name, entry in P.inventory(root, ("result.json", "COMPLETE")).items()}
    require(N.eq(actual, result.get("files")), "W1 full archive inventory differs")
    return result


def accepted_w1_members(N, g1_path, manifest_path):
    """Derive exact accepted result digests from the externally pinned G1 chain."""
    P = N.P
    require(sha(g1_path) == N.G1_SHA, "unaccepted G1 record")
    g1 = P.read(g1_path)
    expected = g1.get("evidence_sha256", {}).get(W1_MANIFEST_MEMBER)
    require(P.digest(expected) and sha(manifest_path) == expected, "W1 manifest is not pinned by G1")
    manifest = P.read(manifest_path)
    require(type(manifest.get("schema_version")) is int and manifest["schema_version"] == 1
            and isinstance(manifest.get("members"), dict), "invalid accepted W1 manifest")
    result = dict(manifest_sha256=expected)
    for kind, member in (("build", W1_BUILD_RESULT_MEMBER), ("functional", W1_FUNCTIONAL_RESULT_MEMBER)):
        value = manifest["members"].get(member)
        require(isinstance(value, dict) and set(value) == {"sha256", "bytes"}
                and P.digest(value["sha256"]) and P.integer(value["bytes"], 1), "accepted W1 result member missing/invalid")
        result[kind] = dict(path=member, **value)
    require(result["build"]["sha256"] == N.G1_BUILD_SHA, "G1/manifest accepted build digest differs")
    return result


def validate_w1_archives(N, build_root, functional_root, contract, producer, accepted_members, original_build_root=None):
    P = N.P
    for root, kind in ((build_root, "build"), (functional_root, "functional")):
        require(N.eq(identity(P.safe_file(root, "result.json")),
                     {k:accepted_members[kind][k] for k in ("sha256", "bytes")}), "W1 result is not the G1-manifest accepted member")
    build = archive_result(N, build_root, "build")
    functional = archive_result(N, functional_root, "smoke")
    original_build_root = str(original_build_root or build_root)
    require(sha(build_root / "result.json") == N.G1_BUILD_SHA and build.get("build_root") == original_build_root
            and build.get("measurement_profile") == "timing" and build.get("sanitizer") == "none"
            and build.get("contract_sha256") == N.CONTRACT_SHA and N.eq(build.get("baseline_headers"), P.BASELINE),
            "wrong accepted timing build/profile")
    require(sha(P.safe_file(build_root, "source/net_reference.cpp")) == N.G1_SOURCE_SHA
            and sha(P.safe_file(build_root, "source/contract.json")) == N.CONTRACT_SHA
            and N.eq({k: e["sha256"] for k, e in P.inventory(build_root / "source").items()}, P.read(build_root / "source-inventory.json")),
            "wrong frozen producer/source inventory")
    flags = build["commands"][0]["argv"]
    require([v for v in flags if v.startswith(("-DN7_NET_DIAGNOSTICS", "-UN7_NET_DIAGNOSTICS"))] == ["-DN7_NET_DIAGNOSTICS=0"]
            and [v for v in flags if v.startswith("-O")] == ["-O2"]
            and not any(v.startswith(("-fsanitize", "-fno-sanitize")) for v in flags), "diagnostic/sanitizer in timing build")
    actual = {"binary_sha256": sha(P.safe_file(build_root, "net_reference")),
              "source_inventory_sha256": sha(P.safe_file(build_root, "source-inventory.json")),
              "toolchain_sha256": sha(P.safe_file(build_root, "toolchain.json")),
              "dependencies_sha256": sha(P.safe_file(build_root, "dependencies.json"))}
    require(N.eq(actual, producer) and actual["binary_sha256"] == N.G1_BINARY_SHA
            and build.get("binary_sha256") == N.G1_BINARY_SHA
            and functional.get("binary_sha256") == N.G1_BINARY_SHA
            and functional.get("build_result_sha256") == N.G1_BUILD_SHA
            and functional.get("executed_build_root") == original_build_root
            and functional.get("measurement_profile") == "timing" and functional.get("sanitizer") == "none",
            "W1 timing functionality producer lineage differs")
    for name, digest in P.BASELINE.items():
        require(sha(P.safe_file(build_root, "source/" + name)) == digest, "wrong F source")
    smoke = P.parse(P.safe_file(functional_root, "smoke.stdout").read_bytes())
    require(type(smoke.get("schema_version")) is int and smoke["schema_version"] == 1
            and smoke.get("contract_version") == "1.0.0" and smoke.get("command") == "smoke"
            and smoke.get("status") == "passed" and smoke.get("diagnostic") is False and smoke.get("timing_evidence") is False
            and type(smoke.get("checks")) is int and smoke["checks"] == 34
            and N.eq(sorted(smoke.get("case_ids", [])), sorted(contract["smoke"]["case_ids"]))
            and type(smoke.get("owned_sockets_after_cases")) is int and smoke["owned_sockets_after_cases"] == 0,
            "wrong W1 timing smoke cases/profile")
    return build, functional


def verify_fixture_sources(N, fixtures, queue, producer, execution_protocol_sha256):
    P = N.P
    # Supplemental audit keeps the original executed protocol and unrecorded
    # historical interpreter provenance intact. The live queue is a sibling.
    first = N.audit_fixture_preparation(fixtures, require_queue=True, queue_root=queue,
                                      accepted_execution_protocol_sha256=execution_protocol_sha256)
    require(first.get("status") == "fixture_preparation_audited" and first.get("timing_evidence") is False
            and first.get("functional_queue_verified") is True,
            "fixture preparation audit failed")
    record = P.read(P.safe_file(fixtures, "fixture-preparation.json"))
    require(N.eq(record["producer_identity"], producer), "fixture/build producer differs")
    if (fixtures / "queue").exists():
        require(N.eq(P.inventory(fixtures / "queue"), P.inventory(queue)), "fixture embedded/sibling queues differ")
    return record, first


def copy_file_checked(N, source, target):
    target = Path(target)
    before = identity(source)
    target.parent.mkdir(parents=True, exist_ok=True)
    with Path(source).open("rb") as src, target.open("xb") as dst:
        shutil.copyfileobj(src, dst)
    require(N.eq(before, identity(source)) and N.eq(before, identity(target)), "source changed during copy")


def copy_tree_checked(N, source, target):
    before = N.P.inventory(source)
    shutil.copytree(source, target)
    require(N.eq(before, N.P.inventory(source)) and N.eq(before, N.P.inventory(target)), "archive changed during copy")


def accepted_inputs(N, root):
    return {name: value for name, value in N.P.inventory(root).items() if name.startswith(PREFIXES)}


def validate_selection(N, acceptance_path, acceptance_sha256, window):
    """Range selection only: D021 pins the prior result/summary, not a new run."""
    P = N.P
    require(P.digest(acceptance_sha256) and sha(acceptance_path) == acceptance_sha256, "selection acceptance external SHA differs")
    acceptance = P.read(acceptance_path)
    require(type(acceptance.get("schema_version")) is int and acceptance["schema_version"] == 1
            and acceptance.get("status") == "accepted_executed_partial_qualification"
            and acceptance.get("comparison_execution_authorized") is False
            and type(acceptance.get("processes")) is int and acceptance["processes"] == 112
            and acceptance.get("all_stable_windows_qualified") is False, "selection is not accepted limited-range preparation")
    reviewed = acceptance.get("reviewed_sha256", {})
    require(set(reviewed) == {"result.json", "summary.json"} and all(P.digest(v) for v in reviewed.values()), "selection reviewed identities missing")
    for name, digest in reviewed.items():
        require(sha(P.safe_file(window, name)) == digest, "selected prior result/summary differs")
    previous, summary = P.read(window / "result.json"), P.read(window / "summary.json")
    require(type(previous.get("schema_version")) is int and previous["schema_version"] == 1
            and previous.get("schema") == "w1-native-network-window" and previous.get("status") == "audited"
            and previous.get("adoption_allowed") is False and previous.get("timing_claim_qualified") is False
            and P.digest(acceptance.get("approval_sha256")) and previous.get("approval_sha256") == acceptance["approval_sha256"]
            and N.eq(previous.get("files", {}).get("summary.json"), identity(window / "summary.json")), "selection prior sealed result lineage differs")
    require(summary.get("evidence_class") == "native" and summary.get("purpose") == "calibration"
            and type(summary.get("process_count")) is int and summary["process_count"] == 112
            and summary.get("adoption_allowed") is False and summary.get("all_stable_windows_qualified") is False,
            "selection prior summary scope differs")
    expected_sizes = [w["nominal_bytes"] for w in N.preregistration()["workloads"]]
    rows, accepted_rows = summary.get("rows", []), acceptance.get("rows", [])
    require(isinstance(rows, list) and len(rows) == 4 and {r.get("workload") for r in rows} == {str(v) for v in expected_sizes}
            and isinstance(accepted_rows, list) and len(accepted_rows) == 4
            and all(type(r.get("nominal_bytes")) is int for r in accepted_rows)
            and {r["nominal_bytes"] for r in accepted_rows} == set(expected_sizes), "selection exact prior size set differs")
    require(all(type(r.get("stable_window_qualified")) is bool for r in rows + accepted_rows)
            and {r["workload"] for r in rows if r["stable_window_qualified"]} == {"1048576"}
            and {r["nominal_bytes"] for r in accepted_rows if r["stable_window_qualified"]} == {1048576}, "only one MiB may be selected")
    for row in rows:
        accepted = next(r for r in accepted_rows if str(r["nominal_bytes"]) == row["workload"])
        require(row.get("comparison") == "F-AA" and type(row.get("local_aa_qualified")) is bool
                and type(row.get("anchor_drift", {}).get("qualified")) is bool
                and N.eq(row["local_aa_qualified"], accepted.get("local_aa_qualified"))
                and N.eq(row["stable_window_qualified"], accepted["stable_window_qualified"])
                and N.eq(row["anchor_drift"]["qualified"], accepted.get("anchor_drift_qualified")), "selection accepted per-size qualification differs")
    target = next(r for r in rows if r["workload"] == "1048576")
    target_accepted = next(r for r in accepted_rows if r["nominal_bytes"] == 1048576)
    require(target["local_aa_qualified"] is True and target["anchor_drift"]["qualified"] is True
            and set(target.get("calibration", {})) == {"pre", "mid", "post"}
            and all(anchor.get("qualified") is True for anchor in target["calibration"].values())
            and N.eq(target_accepted.get("anchor_qualification"), dict(pre=True,mid=True,post=True)), "selected one-MiB local/anchor qualification incomplete")
    return dict(acceptance_sha256=acceptance_sha256, reviewed_sha256=reviewed,
                previous_approval_sha256=acceptance["approval_sha256"], selected_workload=dict(SCREENING_WORKLOAD),
                qualification_carryover=False, comparison_execution_authorized=False,
                scope=SELECTION_SCOPE)


def validate_mode(N, spec, full_fixture_data=None):
    mode = spec.get("binding_mode")
    require(mode in MODES, "unknown binding mode")
    require(spec.get("native_execution_authorized") is False and spec.get("runtime_status") == "ready_for_parent_review",
            "bound proposal cannot authorize a new native window")
    template = N.preregistration()
    for key in ("policy", "controls", "timeout_ms", "max_retained_output_bytes", "environment_policy", "primary_metric",
                "metric_boundary", "timing_unit", "exclusions", "unmeasured_costs", "whole_lifecycle_available"):
        require(N.eq(spec.get(key), template[key]), "fixed resource/threshold/boundary changed: " + key)
    if mode == "calibration":
        expected_workloads, conditions, comparisons, purpose, count = template["workloads"], CALIBRATION_CONDITIONS, CALIBRATION_CONTRAST, "calibration", 112
        require(spec.get("selection_lineage") is None, "calibration cannot inherit selection")
    else:
        expected_workloads, conditions, comparisons, purpose, count = [SCREENING_WORKLOAD], template["conditions"], template["comparisons"], "screening", 88
        require(isinstance(spec.get("selection_lineage"), dict) and spec["selection_lineage"].get("qualification_carryover") is False
                and spec["selection_lineage"].get("comparison_execution_authorized") is False, "screening selection provenance missing")
        lineage = spec["selection_lineage"]
        require(N.P.digest(lineage.get("acceptance_sha256")) and N.P.digest(lineage.get("previous_approval_sha256"))
                and set(lineage.get("reviewed_sha256", {})) == {"summary.json", "result.json"}
                and all(N.P.digest(v) for v in lineage["reviewed_sha256"].values())
                and N.eq(lineage.get("selected_workload"), SCREENING_WORKLOAD) and lineage.get("scope") == SELECTION_SCOPE,
                "screening exact selected workload/digest provenance differs")
    require(N.eq(spec["workloads"], expected_workloads) and N.eq(spec["conditions"], conditions)
            and N.eq(spec["comparisons"], comparisons) and spec["purpose"] == purpose, "exact mode workload/conditions/contrasts differ")
    expected_ids = {w["id"] for w in expected_workloads}
    require(set(spec["expected_data"]) == expected_ids, "exact mode data projection differs")
    if full_fixture_data is not None:
        require(N.eq(spec["expected_data"], {name: full_fixture_data[name] for name in expected_ids}), "projected data differs from full fixture proof")
    jobs = N.schedule(spec)
    phase_counts = {phase:sum(j["phase"] == phase for j in jobs) for phase in ("warmup", "calibration", "comparison")}
    require(len(jobs) == count and phase_counts == (dict(warmup=16,calibration=96,comparison=0) if mode == "calibration"
                                                  else dict(warmup=8,calibration=48,comparison=32)), "exact mode producer/phase budget differs")
    require(all(j["condition"] == "F-none" for j in jobs if j["phase"] == "calibration"), "A/A calibration exposes treatment")
    if mode == "calibration":
        require(all(j["condition"] == "F-none" for j in jobs), "pure F/F calibration exposes treatment")
    elif "retained_output_bytes_expected" in spec:
        require(type(spec["retained_output_bytes_expected"]) is int and spec["retained_output_bytes_expected"] == SCREENING_OUTPUT_BYTES,
                "one-MiB screening output bytes differ")
    return jobs


def make_spec(N, record, build_root, out, queue, window_id, mode="calibration", selection_lineage=None):
    P = N.P
    require(P.identifier(window_id) and window_id != "UNBOUND", "invalid window identifier")
    spec = N.preregistration()
    spec["conditions"] = {name: dict(value) for name, value in CALIBRATION_CONDITIONS.items()}
    spec["comparisons"] = [dict(value) for value in CALIBRATION_CONTRAST]
    if mode == "pool-screening-1m":
        spec["workloads"] = [dict(SCREENING_WORKLOAD)]
        template = N.preregistration()
        spec["conditions"], spec["comparisons"] = template["conditions"], template["comparisons"]
    spec.update(window_id=window_id, purpose="calibration", evidence_class="native", ready=True,
                runtime_status="ready_for_parent_review", native_execution_authorized=False,
                execution_root=str(out), build_execution_root=str(build_root), queue_receipt_out=str(queue),
                expected_data={w["id"]:dict(record["expected_data"][w["id"]]) for w in spec["workloads"]}, producer_identity=record["producer_identity"],
                collector_runtime=N.collector_runtime_identity(), collector_execution_path=str(absolute(Path(N.__file__))),
                queue_runner_execution_path=str(Path(N.PRODUCTION_QUEUE).parents[1] / "experiments/vm_coordination/run_job.py"),
                binding_design="one pure F/F calibration contrast; same binary/pool on both documentary endpoints; no cold/warm treatment; avoids duplicate future-comparison calibration blocks")
    spec["binding_mode"] = mode
    if mode == "pool-screening-1m":
        spec.update(purpose="screening", selection_lineage=selection_lineage,
                    binding_design="independent one-MiB none/cold/warm screening; prior qualification selects range only; both contrasts calibrate locally with three-anchor side drift; no adoption")
    jobs = N.schedule(spec)  # Never use generic P.schedule: it exposes treatment warmups.
    spec["retained_output_bytes_expected"] = sum(j["iterations"] * spec["expected_data"][j["workload"]]["output_bytes"] for j in jobs)
    validate_mode(N, spec, record["expected_data"])
    spec["paths"] = dict(binary=str(build_root / "net_reference"),
                         inputs={w["id"]: str(out / "data" / spec["expected_data"][w["id"]]["input_sha256"]) for w in spec["workloads"]},
                         outputs={j["id"]: str(out / "commands" / j["id"] / "outputs") for j in jobs})
    spec["invocations"] = {j["id"]: N.argv(spec, j) for j in jobs}
    spec["identities"] = {name: dict(binary="w1-build/net_reference", source_manifest="w1-build/source-inventory.json",
                                    toolchain_manifest="w1-build/toolchain.json") for name in spec["conditions"]}
    N.validate_preregistration(spec, bound=True)
    return spec, jobs


def parent_proposal(N, spec, preregister_sha256, inputs, build_sha, functional_sha, collector_path, timeout):
    require(type(timeout) in (int, float) and math.isfinite(timeout) and 0 < timeout <= spec["policy"]["max_window_seconds"],
            "queue timeout exceeds exact preregistration")
    if spec.get("binding_mode") == "pool-screening-1m":
        require(N.positive_number_equal(timeout, 900), "pool screening outer queue bound must remain 900 seconds")
    require(str(collector_path) == spec["collector_execution_path"], "collector differs from preregistered frozen tool path")
    return dict(schema=N.APPROVAL_SCHEMA["schema"], schema_version=1, status="ready_for_parent_review",
                native_execution_authorized=False, source_and_functionality_accepted=True,
                evidence_class="native-posix-tcp-loopback", preregister_sha256=preregister_sha256,
                contract_sha256=N.CONTRACT_SHA, model_subcases_sha256=N.SUBCASES_SHA, runner_sha256=N.QUEUE_SHA,
                accepted_inputs=inputs, build_result_sha256=build_sha, functional_result_sha256=functional_sha,
                window_id=spec["window_id"], execution_root=spec["execution_root"], queue_receipt_out=spec["queue_receipt_out"],
                window_command=[sys.executable, str(collector_path), "collect-window", spec["execution_root"],
                                "--parent-approval-sha256-env", ENVIRONMENT_DIGEST],
                queue_lock_dir=N.PRODUCTION_QUEUE, queue_timeout_seconds=timeout,
                limits=["one preregistered local POSIX loopback window; no adoption", *N.UNMEASURED])


def handoff_instructions(spec, proposal, runner_path, network_sha256, paired_sha256):
    root = Path(spec["execution_root"])
    protocol = root / "protocol"
    python = spec["collector_runtime"]["interpreter"]["path"]
    require(str(runner_path) == spec["queue_runner_execution_path"], "queue runner differs from preregistered accepted path")
    digest = "PARENT_ACCEPTED_APPROVAL_SHA256"
    importer = ("import os,sys;from pathlib import Path;sys.dont_write_bytecode=True;"
                "sys.path.insert(0,sys.argv[1]);import network_measurement as N;")
    return dict(status="instructions_only_no_authorization", approval_digest_environment=ENVIRONMENT_DIGEST,
        parent_first="independently review exact sources/functionality/fixtures/preregistration; write approval.json with status=approved and native_execution_authorized=true; pin its SHA256 outside this archive",
        environment={ENVIRONMENT_DIGEST:digest},
        queue_run_argv=[python, str(runner_path), "run", "--owner", "W3", "--kind", "timing", "--out", proposal["queue_receipt_out"],
                        "--timeout-seconds", str(proposal["queue_timeout_seconds"]), "--", *proposal["window_command"]],
        archive_terminal_queue_argv=[python, str(protocol / "bind_network_window.py"), "archive-queue", str(root),
            "--queue-source", proposal["queue_receipt_out"], "--network-sha256", network_sha256,
            "--paired-sha256", paired_sha256, "--parent-approval-sha256", digest],
        on_failed_outer_job="add --allow-failed to archive-queue; preserve all raw outputs and use failed_seal_argv with the actual recorded reason; never retry this window",
        seal_argv=[python, "-B", "-c", importer + "print(N.P.canonical(N.seal_native(Path(sys.argv[2]),os.environ['VM_NETWORK_APPROVAL_SHA256'])).decode(),end='')",
                   str(protocol), str(root)],
        failed_seal_argv=[python, "-B", "-c", importer + "print(N.P.canonical(N.mark_failed_native(Path(sys.argv[2]),os.environ['VM_NETWORK_APPROVAL_SHA256'],sys.argv[3])).decode(),end='')",
                          str(protocol), str(root), "ACTUAL_RECORDED_FAILURE_REASON"],
        audit_argv=[python, str(protocol / "network_measurement.py"), "audit", str(root), "--parent-approval-sha256", digest],
        finish_execution="archive-queue then seal and audit only after the outer timing job is terminal, through a separate root-owned validation queue; proposal audit is for the pre-approval state only")


def bind(args):
    tools, fixtures, fixture_queue = map(absolute, (args.tools, args.fixtures, args.fixture_queue))
    handoff_path, g1_path, interface = map(absolute, (args.w1_handoff, args.g1, args.interface_dir))
    w1_manifest_path = absolute(args.w1_manifest)
    runner = absolute(args.run_job)
    N = load_protocol(tools, args.network_sha256, args.paired_sha256)
    P = N.P
    manifest = verify_tools(N, tools)
    require(sha(__file__) == args.binder_sha256 and sha(Path(__file__).with_name("test_bind_network_window.py")) == args.binder_test_sha256
            and manifest.get("bind_network_window.py") == args.binder_sha256
            and manifest.get("test_bind_network_window.py") == args.binder_test_sha256
            and identity(tools / "bind_network_window.py") == identity(__file__), "unfrozen/unpinned executed binder/tests")
    require(sha(g1_path) == N.G1_SHA and sha(runner) == N.QUEUE_SHA, "parent G1/queue acceptance differs")
    handoff, g1 = P.read(handoff_path), P.read(g1_path)
    w1_members = accepted_w1_members(N, g1_path, w1_manifest_path)
    workspace = Path(N.PRODUCTION_QUEUE).parents[1]
    require(runner == workspace / "experiments/vm_coordination/run_job.py", "queue runner must retain accepted repository context")
    timing = handoff["frozen_builds"]["timing"]
    build_member_root = str(Path(w1_members["build"]["path"]).parent)
    functional_member_root = str(Path(w1_members["functional"]["path"]).parent)
    require(timing.get("path") == build_member_root and timing.get("smoke_path") == functional_member_root,
            "mutable handoff locator differs from accepted manifest paths")
    build_root, functional_root = absolute(workspace / build_member_root), absolute(workspace / functional_member_root)
    require(timing.get("status") == "passed" and timing.get("binary_sha256") == N.G1_BINARY_SHA
            and timing.get("result_sha256") == N.G1_BUILD_SHA
            and g1.get("status") == "accepted_with_explicit_sanitizer_prerequisite", "unaccepted W1 handoff/build")
    require(sha(P.safe_file(interface, "contract.json")) == N.CONTRACT_SHA
            and sha(P.safe_file(interface, "model_subcases.json")) == N.SUBCASES_SHA, "contract/subcase identity differs")
    acceptance = P.read(P.safe_file(interface, "acceptance.json"))
    require(acceptance.get("status") == "accepted_interface_only"
            and acceptance.get("sha256", {}).get("experiments/vm_network/contract.json") == N.CONTRACT_SHA, "interface gate differs")
    contract = P.read(interface / "contract.json")
    producer = N.accepted_preparation_build(build_root, g1_path, N.G1_SHA)
    build, functional = validate_w1_archives(N, build_root, functional_root, contract, producer, w1_members)
    record, fixture_audit = verify_fixture_sources(N, fixtures, fixture_queue, producer, args.fixture_execution_network_sha256)
    selection_lineage, selection_sources = None, []
    if args.mode == "pool-screening-1m":
        require(args.selection_acceptance is not None and args.selection_window is not None,
                "pool screening requires externally pinned selection acceptance and prior small result/summary")
        selection_acceptance, selection_window = absolute(args.selection_acceptance), absolute(args.selection_window)
        selection_lineage = validate_selection(N, selection_acceptance, args.selection_acceptance_sha256, selection_window)
        selection_sources = [selection_acceptance, selection_window]
    else:
        require(all(value is None for value in (args.selection_acceptance, args.selection_acceptance_sha256, args.selection_window)),
                "calibration mode cannot consume prior selection")
    out, queue = disjoint_new_outputs(args.out, args.queue_out,
        [tools, fixtures, fixture_queue, interface, handoff_path, g1_path, w1_manifest_path, runner, build_root, functional_root, *selection_sources])
    spec, jobs = make_spec(N, record, build_root, out, queue, args.window_id, args.mode, selection_lineage)
    spec["binding_protocol_identity"] = dict(binder_sha256=args.binder_sha256, binder_tests_sha256=args.binder_test_sha256)
    spec["w1_accepted_archive_members"] = w1_members
    # Check proposed queue bound before creating even the new destination.
    parent_proposal(N, spec, "0"*64, {}, N.G1_BUILD_SHA, sha(functional_root / "result.json"),
                    tools / "network_measurement.py", args.queue_timeout_seconds)
    out.mkdir(parents=True, exist_ok=False)
    try:
        for name in ("contract.json", "model_subcases.json", "acceptance.json"):
            copy_file_checked(N, interface / name, out / "contract" / name)
        copy_file_checked(N, g1_path, out / "contract/W1-G1-acceptance.json")
        copy_file_checked(N, w1_manifest_path, out / "contract/W1-G1-manifest.json")
        copy_file_checked(N, handoff_path, out / "contract/W1-handoff-locator.json")
        if args.mode == "pool-screening-1m":
            copy_file_checked(N, selection_acceptance, out / "contract/selection/acceptance.json")
            for name in ("result.json", "summary.json"):
                copy_file_checked(N, selection_window / name, out / "contract/selection" / name)
        for name in manifest:
            copy_file_checked(N, tools / name, out / "protocol" / name)
        copy_file_checked(N, tools / "source-inventory.json", out / "protocol/network-tools-inventory.json")
        copy_file_checked(N, runner, out / "protocol/run_job.py")
        require(identity(out / "protocol/bind_network_window.py") == identity(__file__)
                and identity(out / "protocol/test_bind_network_window.py") == identity(Path(__file__).with_name("test_bind_network_window.py")),
                "copied frozen binder/tests differ")
        P.write_new(out / "protocol/collector-runtime.json", spec["collector_runtime"])
        copy_tree_checked(N, build_root, out / "w1-build")
        copy_tree_checked(N, functional_root, out / "w1-functional")
        proof = out / "contract/fixture-preparation"
        copy_tree_checked(N, fixtures, proof)
        if not (proof / "queue").exists():
            copy_tree_checked(N, fixture_queue, proof / "queue")
        require(fixture_audit.get("functional_queue_verified") is True
                and fixture_audit.get("execution_protocol_sha256") == args.fixture_execution_network_sha256
                and fixture_audit.get("auditor_protocol_sha256") == args.network_sha256,
                "native functional fixture queue/execution/auditor provenance not verified")
        for work in spec["workloads"]:
            data = spec["expected_data"][work["id"]]
            for direction in ("input", "output"):
                digest = data[direction + "_sha256"]
                source = P.safe_file(fixtures, "cases/" + work["id"] + "/data/" + digest)
                target = out / "data" / digest
                require(N.eq(identity(source), dict(sha256=digest, bytes=data[direction + "_bytes"])), "canonical fixture bytes differ")
                if target.exists():
                    require(N.eq(identity(target), identity(source)), "content addressed identity collision")
                else:
                    copy_file_checked(N, source, target)
        inputs = accepted_inputs(N, out)
        spec["artifacts"] = inputs
        N.validate_preregistration(spec, bound=True)
        P.write_new(out / "preregister.json", spec)
        P.write_new(out / "schedule.json", jobs)
        proposal = parent_proposal(N, spec, sha(out / "preregister.json"), inputs, sha(out / "w1-build/result.json"),
                                   sha(out / "w1-functional/result.json"), tools / "network_measurement.py", args.queue_timeout_seconds)
        P.write_new(out / "parent-approval-proposal.json", proposal)
        P.write_new(out / "execution-instructions.json", handoff_instructions(spec, proposal, runner,
                                                                             args.network_sha256, args.paired_sha256))
        P.write_new(out / "binding.json", dict(schema="vm-network-window-binding", schema_version=1, status="ready_for_parent_review",
            native_execution_authorized=False, native_calls_by_binder=0, timing_evidence=False, adoption_allowed=False,
            network_sha256=args.network_sha256, paired_sha256=args.paired_sha256, fixture_audit=fixture_audit,
            binder_sha256=args.binder_sha256, binder_test_sha256=args.binder_test_sha256,
            binding_mode=args.mode,
            fixture_execution_protocol_sha256=args.fixture_execution_network_sha256,
            jobs=len(jobs), retained_output_bytes_expected=spec["retained_output_bytes_expected"],
            approval_digest_environment=ENVIRONMENT_DIGEST,
            queue_archive=dict(live_receipt_out=str(queue), copy_relative="queue", only_after_outer_job_terminal=True,
                               validator="externally pinned parent gate then exact N.validate_queue before/after exclusive copy"),
            next_gate="parent independent review of full source/functionality/data/preregistration; parent writes approval.json only after acceptance"))
        P.write_new(out / "proposal-files.json", dict(schema_version=1, status="ready_for_parent_review", files=P.inventory(out)))
        return audit_proposal(out, args.network_sha256, args.paired_sha256, args.binder_sha256, args.binder_test_sha256,
                              args.selection_acceptance_sha256)
    except BaseException as error:
        if not (out / "binding-failed.json").exists():
            P.write_new(out / "binding-failed.json", dict(status="failed", error=str(error), native_execution_authorized=False,
                                                       native_calls_by_binder=0, timing_evidence=False, partial_copy_preserved=True))
        raise


def audit_proposal(root, network_sha256, paired_sha256, binder_sha256, binder_test_sha256, selection_acceptance_sha256=None):
    root = absolute(root)
    N = load_protocol(root / "protocol", network_sha256, paired_sha256)
    P = N.P
    record = P.read(P.safe_file(root, "proposal-files.json"))
    require(type(record.get("schema_version")) is int and record["schema_version"] == 1
            and record.get("status") == "ready_for_parent_review"
            and N.eq(P.inventory(root, ("proposal-files.json",)), record.get("files")), "proposal inventory changed")
    require(not any((root / name).exists() for name in ("approval.json", "binding-failed.json", "collection.json", "result.json", "COMPLETE")),
            "proposal has approval/execution/completion claim")
    require(sha(P.safe_file(root, "protocol/bind_network_window.py")) == sha(__file__) == binder_sha256
            and sha(P.safe_file(root, "protocol/test_bind_network_window.py")) == binder_test_sha256
            and sha(P.safe_file(root, "contract/W1-G1-acceptance.json")) == N.G1_SHA
            and sha(P.safe_file(root, "contract/contract.json")) == N.CONTRACT_SHA
            and sha(P.safe_file(root, "contract/model_subcases.json")) == N.SUBCASES_SHA
            and sha(P.safe_file(root, "protocol/run_job.py")) == N.QUEUE_SHA, "proposal protocol/G1/queue identity differs")
    interface_acceptance = P.read(P.safe_file(root, "contract/acceptance.json"))
    require(interface_acceptance.get("status") == "accepted_interface_only"
            and interface_acceptance.get("sha256", {}).get("experiments/vm_network/contract.json") == N.CONTRACT_SHA,
            "proposal interface acceptance differs")
    binding = P.read(P.safe_file(root, "binding.json"))
    require(binding.get("schema") == "vm-network-window-binding" and type(binding.get("schema_version")) is int
            and binding["schema_version"] == 1 and binding.get("status") == "ready_for_parent_review"
            and binding.get("native_execution_authorized") is False and binding.get("timing_evidence") is False
            and binding.get("adoption_allowed") is False and type(binding.get("native_calls_by_binder")) is int
            and binding["native_calls_by_binder"] == 0 and binding.get("network_sha256") == network_sha256
            and binding.get("paired_sha256") == paired_sha256 and binding.get("binder_sha256") == binder_sha256
            and binding.get("binder_test_sha256") == binder_test_sha256, "binder execution/approval claim differs")
    spec = N.validate_preregistration(P.read(root / "preregister.json"), bound=True)
    require(N.eq(spec.get("binding_protocol_identity"), dict(binder_sha256=binder_sha256, binder_tests_sha256=binder_test_sha256)),
            "preregistered binder/test identities differ")
    require(spec.get("queue_runner_execution_path") == str(Path(N.PRODUCTION_QUEUE).parents[1] / "experiments/vm_coordination/run_job.py"),
            "queue runner original repository path differs")
    require(spec.get("runtime_status") == "ready_for_parent_review" and spec.get("native_execution_authorized") is False
            and binding.get("binding_mode") == spec.get("binding_mode"), "proposal is authorized or mode differs")
    jobs = validate_mode(N, spec)
    require(N.eq(P.read(root / "schedule.json"), jobs), "proposal schedule differs")
    require(type(binding.get("jobs")) is int and binding["jobs"] == len(jobs)
            and type(binding.get("retained_output_bytes_expected")) is int
            and binding["retained_output_bytes_expected"] == spec["retained_output_bytes_expected"], "binder budget record differs")
    inputs = accepted_inputs(N, root)
    require(N.eq(spec["artifacts"], inputs)
            and N.eq(P.read(root / "protocol/collector-runtime.json"), spec["collector_runtime"]), "frozen inputs/runtime differ")
    w1_members = accepted_w1_members(N, P.safe_file(root, "contract/W1-G1-acceptance.json"),
                                    P.safe_file(root, "contract/W1-G1-manifest.json"))
    require(N.eq(w1_members, spec.get("w1_accepted_archive_members"))
            and spec["build_execution_root"] == str(Path(N.PRODUCTION_QUEUE).parents[1] / Path(w1_members["build"]["path"]).parent),
            "preregistered W1 accepted member/path differs")
    build, functional = validate_w1_archives(N, root / "w1-build", root / "w1-functional",
                                            P.read(root / "contract/contract.json"), spec["producer_identity"], w1_members, spec["build_execution_root"])
    # The copied archive records the original executed root, not its moved location.
    require(build["build_root"] == spec["build_execution_root"], "original build root differs")
    require(P.digest(binding.get("fixture_execution_protocol_sha256")), "missing explicit fixture executed protocol pin")
    fixture_audit = N.audit_fixture_preparation(root / "contract/fixture-preparation", require_queue=True,
        accepted_execution_protocol_sha256=binding["fixture_execution_protocol_sha256"])
    proof = P.read(root / "contract/fixture-preparation/fixture-preparation.json")
    validate_mode(N, spec, proof["expected_data"])
    require(fixture_audit.get("functional_queue_verified") is True
            and N.eq(proof["producer_identity"], spec["producer_identity"])
            and N.eq(fixture_audit, binding.get("fixture_audit")), "proposal canonical fixture proof/provenance differs")
    if spec["binding_mode"] == "pool-screening-1m":
        selection = root / "contract/selection"
        lineage = validate_selection(N, P.safe_file(selection, "acceptance.json"), selection_acceptance_sha256, selection)
        require(N.eq(lineage, spec.get("selection_lineage")), "portable selection provenance differs from external acceptance")
    else:
        require(selection_acceptance_sha256 is None and not (root / "contract/selection").exists(), "unexpected calibration selection proof")
    require(N.eq(binding.get("queue_archive"), dict(live_receipt_out=spec["queue_receipt_out"], copy_relative="queue",
        only_after_outer_job_terminal=True,
        validator="externally pinned parent gate then exact N.validate_queue before/after exclusive copy")), "live/offline queue copy binding differs")
    for work in spec["workloads"]:
        data = spec["expected_data"][work["id"]]
        for direction in ("input", "output"):
            require(N.eq(identity(P.safe_file(root, "data/" + data[direction+"_sha256"])),
                         dict(sha256=data[direction+"_sha256"], bytes=data[direction+"_bytes"])), "full canonical data differs")
    proposal = P.read(root / "parent-approval-proposal.json")
    require(proposal.get("status") == "ready_for_parent_review" and proposal.get("native_execution_authorized") is False
            and N.eq(proposal.get("accepted_inputs"), inputs)
            and proposal.get("preregister_sha256") == sha(root / "preregister.json")
            and proposal.get("build_result_sha256") == sha(root / "w1-build/result.json")
            and proposal.get("functional_result_sha256") == sha(root / "w1-functional/result.json"), "parent proposal does not bind exact evidence")
    command = proposal.get("window_command", [])
    require(len(command) == 6 and command[2:] == ["collect-window", spec["execution_root"], "--parent-approval-sha256-env", ENVIRONMENT_DIGEST]
            and command[0] == spec["collector_runtime"]["interpreter"]["path"]
            and command[1] == spec["collector_execution_path"]
            and Path(command[1]).is_absolute() and Path(command[1]).name == "network_measurement.py", "collector command/runtime/environment differs")
    expected = parent_proposal(N, spec, sha(root / "preregister.json"), inputs, sha(root / "w1-build/result.json"),
                               sha(root / "w1-functional/result.json"), command[1], proposal["queue_timeout_seconds"])
    expected["window_command"][0] = spec["collector_runtime"]["interpreter"]["path"]
    require(N.eq(proposal, expected), "parent proposal schema/queue/limits differ")
    instructions = P.read(P.safe_file(root, "execution-instructions.json"))
    run_argv = instructions.get("queue_run_argv", [])
    require(len(run_argv) >= 2 and Path(run_argv[1]).is_absolute(), "missing exact queue execution instructions")
    require(N.eq(instructions, handoff_instructions(spec, proposal, run_argv[1], network_sha256, paired_sha256)),
            "execution/archive/seal/audit instructions differ")
    return dict(status="ready_for_parent_review", native_execution_authorized=False, native_calls_by_binder=0,
                timing_evidence=False, adoption_allowed=False, jobs=len(jobs), preregister_sha256=sha(root / "preregister.json"),
                parent_proposal_sha256=sha(root / "parent-approval-proposal.json"), original_absolute_paths_accessed=False)


def archive_queue(root, queue_source, network_sha256, paired_sha256, approval_sha256, allow_failed=False):
    """After the outer job is terminal, archive its exact sibling receipt once.

    No command is launched and no native result is sealed. N requires the live
    queue outside the execution root and its completed portable copy at /queue.
    The independently pinned approved gate binds both locations and exact argv.
    """
    require(type(allow_failed) is bool, "invalid failed-queue policy")
    root, queue_source = absolute(root), absolute(queue_source)
    N = load_protocol(root / "protocol", network_sha256, paired_sha256)
    P = N.P
    spec = N.validate_preregistration(P.read(P.safe_file(root, "preregister.json")), bound=True)
    gate = N.validate_parent_gate(root, spec, approval_sha256)
    require(sha(__file__) == gate["accepted_inputs"]["protocol/bind_network_window.py"]["sha256"],
            "archive helper differs from parent accepted binder")
    require(str(root) == spec["execution_root"] and str(queue_source) == spec["queue_receipt_out"],
            "queue archive must target approved original window and receipt")
    require(not (root / "queue").exists() and not (root / "result.json").exists()
            and not (root / "COMPLETE").exists(), "queue/native archive output already exists")
    before = N.validate_queue(queue_source, spec, gate, allow_failed=allow_failed)
    copy_tree_checked(N, queue_source, root / "queue")
    after = N.validate_queue(root / "queue", spec, gate, allow_failed=allow_failed)
    # Validated queue metadata includes aware datetime objects. The copied raw
    # bytes are canonically bound by copy_tree_checked, not JSON coercion.
    require(before == after, "queue receipt changed during portable archive copy")
    return dict(status="terminal_queue_archived", native_calls_by_binder=0, timing_evidence=False,
                adoption_allowed=False, queue_manifest_sha256=sha(root / "queue/manifest.json"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    p = sub.add_parser("bind")
    for name in ("tools", "fixtures", "fixture-queue", "w1-handoff", "g1", "w1-manifest", "interface-dir", "run-job", "out", "queue-out"):
        p.add_argument("--" + name, type=Path, required=True)
    p.add_argument("--window-id", required=True)
    p.add_argument("--mode", choices=MODES, default="calibration")
    p.add_argument("--selection-acceptance", type=Path)
    p.add_argument("--selection-acceptance-sha256")
    p.add_argument("--selection-window", type=Path)
    p.add_argument("--queue-timeout-seconds", type=float, default=900)
    p.add_argument("--network-sha256", required=True)
    p.add_argument("--paired-sha256", required=True)
    p.add_argument("--fixture-execution-network-sha256", required=True)
    p.add_argument("--binder-sha256", required=True)
    p.add_argument("--binder-test-sha256", required=True)
    p = sub.add_parser("audit")
    p.add_argument("path", type=Path)
    p.add_argument("--network-sha256", required=True)
    p.add_argument("--paired-sha256", required=True)
    p.add_argument("--binder-sha256", required=True)
    p.add_argument("--binder-test-sha256", required=True)
    p.add_argument("--selection-acceptance-sha256")
    p = sub.add_parser("archive-queue")
    p.add_argument("path", type=Path)
    p.add_argument("--queue-source", type=Path, required=True)
    p.add_argument("--network-sha256", required=True)
    p.add_argument("--paired-sha256", required=True)
    p.add_argument("--parent-approval-sha256", required=True)
    p.add_argument("--allow-failed", action="store_true")
    args = parser.parse_args()
    if args.action == "bind":
        result = bind(args)
    elif args.action == "audit":
        result = audit_proposal(args.path, args.network_sha256, args.paired_sha256, args.binder_sha256, args.binder_test_sha256,
                                args.selection_acceptance_sha256)
    else:
        result = archive_queue(args.path, args.queue_source, args.network_sha256, args.paired_sha256,
                               args.parent_approval_sha256, args.allow_failed)
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, StopIteration) as error:
        print("network binding rejected: " + str(error), file=sys.stderr)
        sys.exit(1)
