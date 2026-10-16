#!/usr/bin/env python3
"""Case-specific D022 supplement: recover one checker error, then collect 25 cases.

The immutable D018 build, original failed collection and consumed claim remain
unchanged. There is no build, timing, retry or general resume command here.
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys

sys.dont_write_bytecode = True
BUILD = "8cfb16df844a8f95e26171a8de43fadebf4af4e5dcd99af9b5b819e0f54f45b4"
BASE = "a1499ef8da25141eac00b25f8bb82446dd4a0a5a49f911cc945b82096b9c4ac9"
D022 = "0113de64a4e83f3c4d10a710b92be9a70ccc4cadaf69807ab747e4ca3fbf865d"
CHECKER = "2a380368741b1ecf0261c66d8d19ec463024befee9476e851339897fd1f1c83d"
TESTS = "56199206a26c811b327f0ab9012edf38b7f94f11e9542ae63ad2891feac09e56"
ERRATA_AUDIT = "03f6c19040b4cfe9ea80fa1703b6d884cbaffa2272906a49290c72dd182fdcb6"
FIRST = "diagnostic-r256-o8-t1"
FIRST_STDOUT = "fcbbd56c56124520e187f095e9c294d4160aa8ad877fd49264c4827d3beb1149"
OLD_ERROR = "AuditError: owners[0].used_end: conservation/extent mismatch"


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1048576), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, Path(path).resolve())
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def original(build):
    build = Path(build).resolve()
    require(sha(build / "result.json") == BUILD, "only immutable build-v4 is accepted")
    runner = build / "common/run_owner_scaling.py"
    require(sha(runner) == BASE, "original build runner changed")
    # Keep the real __file__: its unchanged self-identity checks remain active.
    base = load(runner, "d018_immutable_base")
    record = base.audit_build(build)
    return base, record


def toolkit(build, path, executing=False):
    base, built = original(build)
    path = Path(path).resolve()
    record = base.basic_audit(path, "toolkit")
    require(record["build_result_sha256"] == BUILD and record["base_runner_sha256"] == BASE and
            record["binaries"] == built["binaries"] and
            record["source_sha256"] == sha(Path(build) / "common/owner_scaling.cpp"), "toolkit build binding differs")
    for name, digest in (("layout-correction.json", D022), ("audit_owner_scaling.py", CHECKER),
                         ("test_audit_owner_scaling.py", TESTS), ("alignment-errata-audit.json", ERRATA_AUDIT)):
        require(record["files"].get(name) == digest, "toolkit correction identity differs: " + name)
    require(record.get("native_executions") == 0 and record.get("cpp_builds") == 0,
            "toolkit claims native execution/build")
    if executing:
        require(Path(__file__).resolve() == path / "continue_owner_scaling.py" and
                sha(__file__) == record["files"]["continue_owner_scaling.py"], "execute the frozen toolkit script")
    checker = load(path / "audit_owner_scaling.py", "d022_corrected_checker")
    require(checker.EFFECTIVE_ALIGNMENT == 8 and checker.BLOCK_STRIDE == 24,
            "corrected checker profile differs")
    return base, built, record, checker


def freeze(args, out, record, base, built):
    repo = Path(built["repo_root"])
    active = repo / "experiments/vm_owner_scaling"
    sources = {"continue_owner_scaling.py": Path(__file__),
               **{name: active / name for name in ("audit_owner_scaling.py", "test_audit_owner_scaling.py",
                   "contract.json", "alignment-discrepancy-review.json", "alignment-errata-audit.json")},
               "layout-correction.json": repo / "docs/coordination/vm-followup/contracts/W1-owner-scaling-v1/layout-correction.json"}
    sources.update({"w2/" + name: Path(args.build) / "common/w2" / name
                    for name in ("audit_events.py", "event_kinds.json")})
    before = {name: sha(source) for name, source in sources.items()}
    for name, digest in (("layout-correction.json", D022), ("audit_owner_scaling.py", CHECKER),
                         ("test_audit_owner_scaling.py", TESTS), ("alignment-errata-audit.json", ERRATA_AUDIT)):
        require(before[name] == digest, "correction source identity differs: " + name)
    decision = base.read(sources["layout-correction.json"])
    require(decision["decision"] == "D022" and decision["recompile_required"] is False and
            decision["first_case_retry_authorized"] is False and
            decision["existing_record"]["sha256"] == FIRST_STDOUT, "D022 authority differs")
    base.evidence_size(repo, sum(p.stat().st_size for p in sources.values()) + 4194304)
    for name, source in sources.items():
        target = out / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    require(before == {name: sha(source) for name, source in sources.items()}, "source changed during toolkit freeze")
    record.update(build_result_sha256=BUILD, base_runner_sha256=BASE, binaries=built["binaries"],
                  source_sha256=sha(Path(args.build) / "common/owner_scaling.cpp"),
                  native_executions=0, cpp_builds=0, decision="D022")
    base.finish(out, record)
    toolkit(args.build, out)


def queue_auditor(base, build):
    path = Path(build) / "provenance/queue-runner.py"
    require(sha(path) == base.QUEUE_SHA, "queue auditor identity differs")
    return load(path, "d018_queue_auditor")


def queue_binding(base, built, receipt, artifact, failed=False):
    request = base.read(Path(receipt) / "request.json")
    result = base.read(Path(receipt) / "result.json")
    co = artifact["coordination"]
    require(request == co["request"] and request["run_id"] == co["metadata"]["run_id"] and
            request["owner"] == "W1" and request["runner_sha256"] == co["runner_sha256"] == base.QUEUE_SHA and
            0 < request["timeout_seconds"] <= 180 and request["source_headers"] == base.BASELINE and
            request["lock_dir"] == str(Path(built["repo_root"]) / "build/vm-coordination") and
            co["metadata"]["status"] == "running" and co["metadata"]["owner"] == "W1" and
            co["metadata"]["supervisor_pid"] in co["ancestors"], "queue provenance differs")
    require(result["status"] == ("failed" if failed else "complete"), "unexpected queue completion state")


def expected_argv(built, case, base):
    profile, owners, threads, requests, control = base.cases()[case]
    return [str(Path(built["build_root"]) / ("owner_scaling_" + profile)), "--owners", str(owners),
            "--threads", str(threads), "--requests", str(requests), "--control", control]


def audit_child(base, checker, built, group, artifact, case, attempts, claim_state):
    profile, owners, threads, requests, control = base.cases()[case]
    argv = expected_argv(built, case, base)
    commands = [command for command in artifact["commands"] if command["argv"] == argv]
    require(len(commands) == 1, "native command missing/duplicate")
    command = commands[0]
    require(command.get("returncode") == (0 if control == "none" else 2) and
            command["timeout_seconds"] == 30 and not command.get("timed_out", False) and
            command["child_file_limit_bytes"] == command["log_file_limit_bytes"] == base.LOG_LIMIT,
            "native exit/deadline/file limit differs")
    claim_path = Path(attempts) / (case + ".json")
    claim = base.read(claim_path)
    require(claim["case_id"] == case and claim["status"] == claim_state and claim["argv"] == argv and
            claim["build_result_sha256"] == artifact["build_result_sha256"] == BUILD and
            claim["collection_root"] == artifact["collection_root"] and
            artifact["build_root"] == built["build_root"] and
            artifact["attempt_root"] == str(Path(built["repo_root"]) / "build/vm-owner-scaling/attempts-d018"),
            "consumed attempt provenance differs")
    if claim_state == "audited":
        require(claim["returncode"] == command["returncode"] and
                claim["stdout_sha256"] == sha(Path(group) / (case + ".stdout")) and
                claim["stderr_sha256"] == sha(Path(group) / (case + ".stderr")), "audited attempt hashes differ")
    child = base.read(Path(group) / (case + ".stdout"))
    summary = checker.audit_record(child, expected_profile=profile,
                                   expected_condition=(owners, threads, requests), expected_control=control)
    return child, summary


def recovery_proof(base, built, checker, build, group, attempts, receipt):
    group, receipt = Path(group), Path(receipt)
    artifact = base.read(group / "result.json")
    require(artifact["kind"] == "collect" and artifact["status"] == "failed" and
            artifact["reason"] == OLD_ERROR and artifact["executed_runner_sha256"] == BASE and
            artifact["timing_evidence"] is False and artifact["cases"] == {} and
            len(artifact["commands"]) == 1, "not the unique original checker-only failure")
    require(set(base.inventory(group)) == {"result.json", FIRST + ".stdout", FIRST + ".stderr"} and
            sha(group / (FIRST + ".stdout")) == FIRST_STDOUT, "original first evidence changed")
    audited_queue = queue_auditor(base, build).audit(receipt)
    require(audited_queue["job_status"] == "failed", "original failed receipt was relabelled")
    queue_binding(base, built, receipt, artifact, failed=True)
    result = base.read(receipt / "result.json")
    require(result["command_returncode"] == 1 and result["reason"] == "command_exit" and
            result["cleanup"] == {"live": [], "process_groups": [], "zombies": []}, "original queue failure differs")
    child, summary = audit_child(base, checker, built, group, artifact, FIRST, attempts, "claimed")
    return {"group_files": base.inventory(group), "queue_files": base.inventory(receipt),
            "claim_sha256": sha(Path(attempts) / (FIRST + ".json")), "audit": summary,
            "queue_run_id": audited_queue["run_id"], "child_returncode": 0, "original_collector_returncode": 1}, child


def verify_recovery(args, base, built, checker):
    root = Path(args.recovery).resolve()
    record = base.basic_audit(root, "recover")
    require(record["native_executions"] == 0 and record["historical_queue_status"] == "failed" and
            record["decision"] == "recovered_checker_error_not_historical_pass" and
            record["toolkit_result_sha256"] == sha(Path(args.toolkit) / "result.json") and
            record["checker_sha256"] == CHECKER and record["executed_runner_sha256"] == sha(__file__) and
            record["base_runner_sha256"] == BASE, "recovery/toolkit identity differs")
    group, receipt = (root / record["relative_inputs"][name] for name in ("group", "receipt"))
    proof, child = recovery_proof(base, built, checker, args.build, group, args.attempts, receipt)
    require(proof == record["proof"], "post-hoc recovery proof changed")
    return record, proof, child


def recover(args, out, record, base, built, checker):
    proof, _ = recovery_proof(base, built, checker, args.build, args.group, args.attempts, args.receipt)
    record.update(native_executions=0, historical_queue_status="failed",
                  decision="recovered_checker_error_not_historical_pass", proof=proof,
                  relative_inputs={"group": os.path.relpath(Path(args.group).resolve(), out),
                                   "receipt": os.path.relpath(Path(args.receipt).resolve(), out)})
    base.finish(out, record)


def collect(args, out, record, base, built, checker):
    verify_recovery(args, base, built, checker)
    selected = args.case_ids.split(",")
    require(1 <= len(selected) <= 4 and FIRST not in selected and len(set(selected)) == len(selected) and
            set(selected) <= set(base.cases()) - {FIRST}, "only one to four remaining distinct D018 cases")
    require(Path(args.build).resolve() == Path(built["build_root"]), "native collection requires original binary location")
    base.TASK_REPO = Path(built["repo_root"])
    # The sole substitution is the reviewed workload checker, not provenance.
    base.load_auditor = lambda _directory: checker
    args.repo_root = Path(built["repo_root"])
    record["recovery_result_sha256"] = sha(Path(args.recovery) / "result.json")
    base.collect(args, out, record)


def final_audit(args, base, built, checker):
    recovery, proof, first = verify_recovery(args, base, built, checker)
    require(len(args.groups) == 7 and len(set(map(str, args.groups))) == 7, "expected seven remaining collection groups")
    qa = queue_auditor(base, args.build)
    receipts = {}
    for path in args.receipts:
        result = qa.audit(path)
        require(result["run_id"] not in receipts, "duplicate queue receipt")
        receipts[result["run_id"]] = (path, result)
    require(len(receipts) == 9 and proof["queue_run_id"] in receipts, "expected build plus eight collection receipts")
    require(receipts[proof["queue_run_id"]][1]["job_status"] == "failed", "historical failure was relabelled")
    require(base.inventory(receipts[proof["queue_run_id"]][0]) == proof["queue_files"],
            "supplied recovered receipt differs from the original failed receipt")
    reports, used = {FIRST: first}, {proof["queue_run_id"]}
    artifacts = [(Path(args.build), built)] + [(Path(group), base.basic_audit(group, "collect")) for group in args.groups]
    for path, artifact in artifacts:
        run_id = artifact["coordination"]["metadata"]["run_id"]
        require(run_id not in used and run_id in receipts, "missing/duplicate group receipt")
        used.add(run_id)
        receipt, outcome = receipts[run_id]
        require(outcome["job_status"] == "complete", "only the original checker-error receipt may be failed")
        queue_binding(base, built, receipt, artifact)
        if artifact["kind"] == "build":
            continue
        require(artifact["executed_runner_sha256"] == sha(__file__) and artifact["base_runner_sha256"] == BASE and
                artifact["toolkit_result_sha256"] == sha(Path(args.toolkit) / "result.json") and
                artifact["checker_sha256"] == CHECKER and
                artifact["recovery_result_sha256"] == sha(Path(args.recovery) / "result.json"), "collection tooling differs")
        require(1 <= len(artifact["cases"]) <= 4 and len(artifact["commands"]) == len(artifact["cases"]), "collection process count differs")
        for case, stored in artifact["cases"].items():
            require(case in base.cases() and case not in reports, "unknown/duplicate case")
            child, summary = audit_child(base, checker, built, path, artifact, case, args.attempts, "audited")
            require(summary == stored, "stored corrected workload audit differs")
            reports[case] = child
    require(used == set(receipts) and set(reports) == set(base.cases()), "missing/extra receipt or case")
    require(set(base.inventory(args.attempts)) == {case + ".json" for case in base.cases()}, "missing/extra consumed attempts")
    for requests in (256, 8192):
        require(len({r["checksum"] for r in reports.values() if r["control"] == "none" and
                     r["condition"]["requests"] == requests}) == 1, "cross-condition/profile checksum mismatch")
    variants = {}
    bad = copy.deepcopy(first); bad["condition"]["requests"] *= 2
    variants["wrong_request_total"] = bad
    bad = copy.deepcopy(first); events = bad["trace"]["events"]
    del events[next(i for i, event in enumerate(events) if event["kind"] == "registry_lookup_hit")]
    for sequence, event in enumerate(events, 1): event["sequence"] = sequence
    variants["missing_lookup"] = bad
    bad = copy.deepcopy(first); bad["owners"][0]["destroyed"] = False
    variants["residual_owner"] = bad
    negatives = {}
    for name, bad in variants.items():
        try: checker.audit_record(bad, expected_profile="diagnostic", expected_condition=(8, 1, 256))
        except ValueError as error: negatives[name] = {"rejected": True, "reason": str(error)}
        else: raise ValueError("negative copy accepted: " + name)
    return {"schema_version": 1, "status": "passed", "timing_evidence": False, "native_executions": 0,
            "workload_processes": 26, "successful_processes": 18, "expected_failure_processes": 8,
            "recovered_original_cases": [FIRST], "historical_failed_queue_remains_failed": True,
            "case_ids": sorted(reports), "negative_checks": negatives, "binaries": built["binaries"],
            "build_result_sha256": BUILD, "toolkit_result_sha256": sha(Path(args.toolkit) / "result.json"),
            "recovery_result_sha256": sha(Path(args.recovery) / "result.json"), "checker_sha256": CHECKER}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    for command in ("freeze", "recover", "collect", "audit"):
        p = sub.add_parser(command)
        p.add_argument("--build", type=Path, required=True)
        if command != "freeze": p.add_argument("--toolkit", type=Path, required=True)
        if command != "audit": p.add_argument("--out", type=Path, required=True)
        if command in ("recover", "collect", "audit"): p.add_argument("--attempts", type=Path, required=True)
        if command in ("collect", "audit"): p.add_argument("--recovery", type=Path, required=True)
        if command == "recover":
            p.add_argument("--group", type=Path, required=True); p.add_argument("--receipt", type=Path, required=True)
        if command == "collect": p.add_argument("--case-ids", required=True)
        if command == "audit":
            p.add_argument("--groups", type=Path, nargs="+", required=True)
            p.add_argument("--receipts", type=Path, nargs="+", required=True)
    args = parser.parse_args()
    args.build = args.build.resolve()
    base, built = original(args.build)
    if args.command != "freeze":
        args.toolkit = args.toolkit.resolve()
        base, built, tools, checker = toolkit(args.build, args.toolkit, executing=True)
    if args.command == "audit":
        print(json.dumps(final_audit(args, base, built, checker), sort_keys=True)); return
    repo = Path(built["repo_root"])
    destination = args.out.resolve()
    require(any(destination.is_relative_to(repo / suffix) for suffix in
                ("build/vm-owner-scaling", "docs/validation/vm-owner-scaling")), "output outside D018 evidence paths")
    base.TASK_REPO = repo
    base.evidence_size(repo)
    out = base.fresh(destination)
    record = {"schema_version": 1, "kind": "toolkit" if args.command == "freeze" else args.command,
              "status": "running", "started_at": base.now(), "timing_evidence": False,
              "executed_runner_sha256": sha(__file__), "base_runner_sha256": BASE}
    if args.command != "freeze":
        record.update(toolkit_result_sha256=sha(args.toolkit / "result.json"), checker_sha256=CHECKER)
    try:
        if args.command == "freeze": freeze(args, out, record, base, built)
        elif args.command == "recover": recover(args, out, record, base, built, checker)
        else: collect(args, out, record, base, built, checker)
    except Exception as error:
        record.update(status="failed", reason=type(error).__name__ + ": " + str(error), finished_at=base.now())
        base.write(out / "result.json", record)
        raise
    print(json.dumps({"status": "passed", "kind": record["kind"], "out": str(out)}))


if __name__ == "__main__":
    main()
