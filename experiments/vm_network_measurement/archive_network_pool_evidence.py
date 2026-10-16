#!/usr/bin/env python3
"""Archive the sealed one-MiB/88-process pool screening to NEW_OUT/window.

The exact reviewed calibration archiver supplies read-only snapshot/hash/import
helpers. Both archiver sources are retained at NEW_OUT/tools for reproduction.
Only destination-internal duplicates use hardlinks; no producer is launched.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import sys

sys.dont_write_bytecode = True
HELPER_SHA256 = "3dfdcb996fc11a8f20134d083788c83d60e33ff716b51f7735dc3d9fe5d63b85"
NETWORK_SHA256 = "93e235728b30dd85a0c658bb695eab838a00623a1496c510417963f4ac9d8b84"
PAIRED_SHA256 = "40627ac7cdad4ebec10428e4d8a74e83b790509341411e6def162c526204003a"
WORKLOAD = dict(id="1048576", nominal_bytes=1048576, elements=262144, iterations=4)
OUTPUT_BYTES = 369222656


def load_helper(path):
    path = Path(path)
    if not path.is_absolute() or not path.is_file() or any(p.is_symlink() for p in (path, *path.parents)):
        raise ValueError("helper must be an existing original absolute regular path")
    if hashlib.sha256(path.read_bytes()).hexdigest() != HELPER_SHA256:
        raise ValueError("reviewed archive helper SHA256 differs")
    description = importlib.util.spec_from_file_location("_pinned_network_archive_helper", path)
    helper = importlib.util.module_from_spec(description)
    description.loader.exec_module(helper)
    return helper


def validate_screening(N, spec, summary):
    template = N.preregistration()
    N.validate_preregistration(spec, bound=True)
    N.P.require(spec.get("binding_mode") == "pool-screening-1m" and spec.get("purpose") == "screening"
                and N.eq(spec.get("workloads"), [WORKLOAD])
                and N.eq(spec.get("conditions"), template["conditions"])
                and N.eq(spec.get("comparisons"), template["comparisons"])
                and set(spec.get("expected_data", {})) == {"1048576"}, "not the exact selected one-MiB pool screening")
    for key in ("policy", "controls", "timeout_ms", "max_retained_output_bytes", "environment_policy", "primary_metric",
                "metric_boundary", "timing_unit", "exclusions", "unmeasured_costs", "whole_lifecycle_available"):
        N.P.require(N.eq(spec.get(key), template[key]), "screening stock setup changed: " + key)
    jobs = N.schedule(spec)
    phases = {p:sum(j["phase"] == p for j in jobs) for p in ("warmup", "calibration", "comparison")}
    N.P.require(len(jobs) == 88 and phases == dict(warmup=8,calibration=48,comparison=32)
                and all(j["iterations"] == 4 and j["workload"] == "1048576" for j in jobs)
                and all(j["condition"] == "F-none" for j in jobs if j["phase"] == "calibration")
                and type(spec.get("retained_output_bytes_expected")) is int
                and spec["retained_output_bytes_expected"] == OUTPUT_BYTES, "screening exact process/output budget differs")
    N.P.require(summary.get("purpose") == "screening" and type(summary.get("process_count")) is int
                and summary["process_count"] == 88 and summary.get("adoption_allowed") is False
                and type(summary.get("all_stable_windows_qualified")) is bool, "wrong screening summary scope")
    rows = summary.get("rows", [])
    N.P.require(isinstance(rows, list) and len(rows) == 2
                and {r.get("comparison") for r in rows} == {"none-cold", "none-warm"}
                and all(r.get("workload") == "1048576" for r in rows), "exact screening comparison summary differs")
    qualification = {}
    for row in rows:
        N.P.require(type(row.get("local_aa_qualified")) is bool and type(row.get("stable_window_qualified")) is bool
                    and set(row.get("calibration", {})) == {"pre", "mid", "post"}
                    and all(type(v.get("qualified")) is bool for v in row["calibration"].values())
                    and type(row.get("anchor_drift", {}).get("qualified")) is bool, "missing complete contrast qualification")
        qualification[row["comparison"]] = dict(local_aa_qualified=row["local_aa_qualified"], calibration=row["calibration"],
            anchor_drift=row["anchor_drift"], stable_window_qualified=row["stable_window_qualified"])
    return jobs, qualification


def archive(source, out, approval_sha256, network_sha256, paired_sha256, helper_path):
    H = load_helper(helper_path)
    source, out = H.absolute(source), H.absolute(out)
    H.require(source.is_dir() and not out.exists(), "source missing or archive destination already exists")
    H.require(source != out and source not in out.parents and out not in source.parents, "source/output paths overlap")
    H.require(H.digest(approval_sha256) and network_sha256 == NETWORK_SHA256 and paired_sha256 == PAIRED_SHA256,
              "external parent approval or accepted N93/P406 pins differ")
    before = H.snapshot(source)
    files = before["files"]
    H.require(files.get("approval.json", {}).get("sha256") == approval_sha256
              and files.get("protocol/network_measurement.py", {}).get("sha256") == network_sha256
              and files.get("protocol/paired_measurement.py", {}).get("sha256") == paired_sha256, "source approval/protocol identities differ")
    result = H.read(source / "result.json")
    result_sha256 = files["result.json"]["sha256"]
    H.require(type(result.get("schema_version")) is int and result["schema_version"] == 1
              and result.get("schema") == "w1-native-network-window" and result.get("status") == "audited"
              and result.get("adoption_allowed") is False and result.get("approval_sha256") == approval_sha256
              and (source / "COMPLETE").read_text().strip() == result_sha256, "source is not a sealed audited network window")
    H.require(H.canonical({name:{k:value[k] for k in ("sha256", "bytes")} for name,value in files.items()
                           if name not in ("result.json", "COMPLETE")}) == H.canonical(result.get("files")), "source sealed inventory differs")
    source_inodes = {(p.stat().st_dev,p.stat().st_ino) for p in (source / name for name in files)}
    out.mkdir(parents=True, exist_ok=False)
    copies, links, tool_identities = {}, [], {}

    def copy_verified(original, target):
        original, target = Path(original), Path(target)
        expected = files[original.relative_to(source).as_posix()]
        H.require(H.file_identity(original) == expected, "source changed before copy")
        key = (expected["sha256"],expected["bytes"],expected["mode"])
        if key in copies:
            first = copies[key]
            H.require(out in first.parents and source not in first.parents and H.file_identity(first) == expected,
                      "internal hardlink canonical changed or escaped")
            os.link(first,target)
            links.append(dict(path=target.relative_to(out).as_posix(),canonical=first.relative_to(out).as_posix(),**expected))
        else:
            with original.open("rb") as src, target.open("xb") as dst:
                shutil.copyfileobj(src,dst)
            shutil.copystat(original,target,follow_symlinks=False)
            copies[key] = target
        H.require(H.file_identity(original) == expected and H.file_identity(target) == expected, "source/destination SHA/bytes/mode differ")
        H.require((target.stat().st_dev,target.stat().st_ino) not in source_inodes, "destination links an original source inode")
        return str(target)

    try:
        shutil.copytree(source,out / "window",copy_function=copy_verified)
        H.require(H.snapshot(out / "window") == before, "copied full bytes/modes/directory inventory differs")
        (out / "tools").mkdir()
        for original, name in ((Path(helper_path),"archive_network_evidence.py"),
                               (Path(__file__),"archive_network_pool_evidence.py")):
            expected = H.file_identity(original)
            target = out / "tools" / name
            with original.open("rb") as src, target.open("xb") as dst:
                shutil.copyfileobj(src,dst)
            shutil.copystat(original,target,follow_symlinks=False)
            H.require(H.file_identity(original) == expected and H.file_identity(target) == expected
                      and (target.stat().st_dev,target.stat().st_ino) != (original.stat().st_dev,original.stat().st_ino),
                      "archiver source copy changed or links original")
            tool_identities[name] = expected
        H.require(tool_identities["archive_network_evidence.py"]["sha256"] == HELPER_SHA256, "retained helper pin changed")
        N = H.load_copied_protocol(out / "window/protocol",network_sha256,paired_sha256)
        verdict = N.audit_native(out / "window",approval_sha256)
        H.require(verdict.get("status") == "audited" and verdict.get("evidence_class") == "native-posix-tcp-loopback"
                  and verdict.get("adoption_allowed") is False, "copied frozen N93 native audit failed")
        H.require(H.snapshot(out / "window") == before, "copied frozen audit changed evidence")
        summary, spec = H.read(out / "window/summary.json"), H.read(out / "window/preregister.json")
        jobs, qualification = validate_screening(N,spec,summary)
        logical_wire_bytes = sum(value["bytes"] for name,value in files.items()
            if re.fullmatch(r"commands/j[0-9]+/outputs/output-[0-9]+\.wire",name))
        H.require(logical_wire_bytes == OUTPUT_BYTES, "screening logical retained wire bytes differ")
        H.require(H.snapshot(source) == before and H.file_identity(source / "result.json") == files["result.json"],
                  "source inventory/result changed during archive")
        H.write_new(out / "hardlink-inventory.json",dict(scope="archive-internal links only; never original source inodes",
            verified_fields=["sha256","bytes","mode"],links=links))
        H.write_new(out / "audit.log",verdict)
        content = {(v["sha256"],v["bytes"]) for v in [*files.values(),*tool_identities.values()]}
        record = dict(schema_version=1,kind="network-pool-screening-archive",status="audited",adoption_allowed=False,
            copied_window_directory="window",approval_sha256=approval_sha256,network_protocol_sha256=network_sha256,
            paired_protocol_sha256=paired_sha256,archiver_sources=tool_identities,
            source_result_sha256=result_sha256,copied_result_sha256=H.sha(out / "window/result.json"),
            source_summary_sha256=files["summary.json"]["sha256"],copied_summary_sha256=H.sha(out / "window/summary.json"),
            source_inventory_before_after_unchanged=True,source_result_before_after_unchanged=True,source_hardlinks_created=0,
            native_process_count=len(jobs),phase_process_counts=dict(warmup=8,calibration=48,comparison=32),
            all_stable_windows_qualified=summary["all_stable_windows_qualified"],contrast_qualification=qualification,
            logical_wire_bytes=logical_wire_bytes,total_logical_window_file_bytes=sum(v["bytes"] for v in files.values()),
            unique_content_bytes=sum(size for _,size in content),
            unique_stored_file_bytes_by_mode=sum(size for _,size,_ in copies)+sum(v["bytes"] for v in tool_identities.values()),
            duplicate_files_using_internal_hardlinks=len(links),copied_window_file_count=len(files),
            summary_preserved_without_reinterpretation=True,files=H.snapshot(out)["files"])
        H.require(record["copied_result_sha256"] == result_sha256
                  and record["copied_summary_sha256"] == files["summary.json"]["sha256"], "archive changed original result/summary")
        H.write_new(out / "EVIDENCE.json",record)
        with (out / "SEALED").open("x") as target:
            target.write(H.sha(out / "EVIDENCE.json")+"\n")
        H.require(H.read(out / "EVIDENCE.json") == record and (out / "SEALED").read_text().strip() == H.sha(out / "EVIDENCE.json"),
                  "archive publication verification differs")
        return {name:record[name] for name in ("status","native_process_count","all_stable_windows_qualified",
                    "logical_wire_bytes","unique_content_bytes","duplicate_files_using_internal_hardlinks")}
    except Exception as error:
        if not (out / "SEALED").exists():
            H.write_new(out / "archive-failure.json",dict(status="failed",error=str(error),adoption_allowed=False,
                partial_archive_preserved=True,source_hardlinks_created=0))
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source",type=Path,required=True)
    parser.add_argument("--out",type=Path,required=True)
    parser.add_argument("--approval-sha256",required=True)
    parser.add_argument("--network-sha256",required=True)
    parser.add_argument("--paired-sha256",required=True)
    parser.add_argument("--helper",type=Path,default=Path(__file__).absolute().with_name("archive_network_evidence.py"))
    args = parser.parse_args()
    result = archive(args.source,args.out,args.approval_sha256,args.network_sha256,args.paired_sha256,args.helper)
    print(json.dumps(result,sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except (OSError,ValueError,KeyError,TypeError) as error:
        print("pool screening archive rejected: "+str(error),file=sys.stderr)
        sys.exit(1)
