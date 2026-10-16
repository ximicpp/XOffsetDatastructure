#!/usr/bin/env python3
"""Archive the sealed 112-process network calibration using internal hardlinks.

No producer is launched. The copied, externally pinned N/P protocol supplies the
native audit. Original source files are never hardlinked into the new archive.
Run this one-time copy through the accepted root-owned validation queue.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import shutil
import stat
import sys

sys.dont_write_bytecode = True


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(value):
    return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{64}", value) is not None


def sha(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as source:
        for data in iter(lambda: source.read(1024 * 1024), b""):
            result.update(data)
    return result.hexdigest()


def canonical(value):
    return (json.dumps(value, sort_keys=True, indent=2, allow_nan=False) + "\n").encode()


def read(path):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, "duplicate JSON key")
            result[key] = value
        return result
    def invalid(value):
        raise ValueError("nonfinite JSON constant: " + value)
    def finite(value):
        number = float(value)
        require(math.isfinite(number), "nonfinite JSON number")
        return number
    return json.loads(Path(path).read_bytes(), object_pairs_hook=pairs, parse_constant=invalid, parse_float=finite)


def write_new(path, value):
    with Path(path).open("xb") as output:
        output.write(canonical(value))


def absolute(path):
    path = Path(path)
    require(path.is_absolute() and str(path) == os.path.normpath(str(path)) and ".." not in path.parts,
            "source/output must use canonical absolute paths")
    for ancestor in (path, *path.parents):
        require(not ancestor.is_symlink(), "symlink in source/output path")
    return path


def file_identity(path):
    path = Path(path)
    mode = path.lstat().st_mode
    require(stat.S_ISREG(mode), "only regular source/archive files are allowed")
    return dict(sha256=sha(path), bytes=path.stat().st_size, mode=stat.S_IMODE(mode))


def snapshot(root):
    root = Path(root)
    require(root.is_dir() and not root.is_symlink(), "unsafe/missing archive root")
    files, directories = {}, {".":stat.S_IMODE(root.stat().st_mode)}
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root).as_posix()
        require(not path.is_symlink(), "symlink in evidence")
        if path.is_dir():
            directories[relative] = stat.S_IMODE(path.stat().st_mode)
        else:
            files[relative] = file_identity(path)
    return dict(files=files, directories=directories)


def load_copied_protocol(protocol, network_sha256, paired_sha256):
    require(file_identity(protocol / "network_measurement.py")["sha256"] == network_sha256
            and file_identity(protocol / "paired_measurement.py")["sha256"] == paired_sha256,
            "copied N/P protocol differs from external pins")
    old_path = list(sys.path)
    had_paired = "paired_measurement" in sys.modules
    old_paired = sys.modules.get("paired_measurement")
    try:
        description = importlib.util.spec_from_file_location("paired_measurement", protocol / "paired_measurement.py")
        paired = importlib.util.module_from_spec(description)
        sys.modules["paired_measurement"] = paired
        description.loader.exec_module(paired)
        description = importlib.util.spec_from_file_location("_archived_network_protocol", protocol / "network_measurement.py")
        network = importlib.util.module_from_spec(description)
        description.loader.exec_module(network)
    finally:
        sys.path[:] = old_path
        if had_paired:
            sys.modules["paired_measurement"] = old_paired
        else:
            sys.modules.pop("paired_measurement", None)
    return network


def archive(source, out, approval_sha256, network_sha256, paired_sha256):
    source, out = absolute(source), absolute(out)
    require(source.is_dir() and not out.exists(), "source missing or archive destination already exists")
    require(source != out and source not in out.parents and out not in source.parents, "source/output paths overlap")
    require(all(digest(value) for value in (approval_sha256, network_sha256, paired_sha256)), "external digest pin missing")
    before = snapshot(source)
    files = before["files"]
    require(files.get("approval.json", {}).get("sha256") == approval_sha256, "source external parent approval differs")
    require(files.get("protocol/network_measurement.py", {}).get("sha256") == network_sha256
            and files.get("protocol/paired_measurement.py", {}).get("sha256") == paired_sha256, "source protocol pins differ")
    result = read(source / "result.json")
    result_sha256 = files["result.json"]["sha256"]
    require(type(result.get("schema_version")) is int and result["schema_version"] == 1
            and result.get("schema") == "w1-native-network-window" and result.get("status") == "audited"
            and result.get("adoption_allowed") is False and result.get("approval_sha256") == approval_sha256
            and (source / "COMPLETE").read_text().strip() == result_sha256, "source is not a sealed accepted network window")
    require(canonical({name:{k:value[k] for k in ("sha256", "bytes")} for name,value in files.items()
                       if name not in ("result.json", "COMPLETE")}) == canonical(result.get("files")), "source sealed file inventory differs")
    source_inodes = {(path.stat().st_dev, path.stat().st_ino) for path in (source / name for name in files)}
    out.mkdir(parents=True, exist_ok=False)
    canonical_copies, links = {}, []

    def copy_verified(original, target):
        original, target = Path(original), Path(target)
        name = original.relative_to(source).as_posix()
        expected = files[name]
        require(file_identity(original) == expected, "source changed before copy: " + name)
        key = (expected["sha256"], expected["bytes"], expected["mode"])
        if key in canonical_copies:
            first = canonical_copies[key]
            require(out in first.parents and source not in first.parents
                    and file_identity(first) == expected, "internal hardlink canonical changed/escaped archive")
            os.link(first, target)
            links.append(dict(path=target.relative_to(out).as_posix(), canonical=first.relative_to(out).as_posix(), **expected))
        else:
            with original.open("rb") as src, target.open("xb") as dst:
                shutil.copyfileobj(src, dst)
            shutil.copystat(original, target, follow_symlinks=False)
            canonical_copies[key] = target
        require(file_identity(original) == expected and file_identity(target) == expected, "copied source/destination SHA/bytes/mode differs")
        require((target.stat().st_dev, target.stat().st_ino) not in source_inodes, "archive unexpectedly links original source inode")
        return str(target)

    try:
        shutil.copytree(source, out / "calibration", copy_function=copy_verified)
        require(snapshot(out / "calibration") == before, "copied full bytes/modes/directory inventory differs")
        N = load_copied_protocol(out / "calibration/protocol", network_sha256, paired_sha256)
        verdict = N.audit_native(out / "calibration", approval_sha256)
        require(verdict.get("status") == "audited" and verdict.get("evidence_class") == "native-posix-tcp-loopback"
                and verdict.get("adoption_allowed") is False, "copied frozen native audit did not pass")
        require(snapshot(out / "calibration") == before, "copied native audit changed evidence")
        summary = read(out / "calibration/summary.json")
        spec = read(out / "calibration/preregister.json")
        jobs = N.schedule(spec)
        require(type(summary.get("process_count")) is int and summary["process_count"] == len(jobs) == 112
                and summary.get("purpose") == "calibration" and all(j["condition"] == "F-none" for j in jobs)
                and type(summary.get("all_stable_windows_qualified")) is bool, "not the sealed pure 112-process calibration")
        logical_wire_bytes = sum(value["bytes"] for name,value in files.items()
            if re.fullmatch(r"commands/j[0-9]+/outputs/output-[0-9]+\.wire", name))
        require(logical_wire_bytes == spec["retained_output_bytes_expected"], "logical retained output budget differs")
        require(snapshot(source) == before and file_identity(source / "result.json") == files["result.json"],
                "original source inventory/result changed during archive")
        write_new(out / "hardlink-inventory.json", dict(scope="internal archive links only; never link original source",
            verified_fields=["sha256", "bytes", "mode"], links=links))
        write_new(out / "audit.log", verdict)
        content = {(value["sha256"],value["bytes"]) for value in files.values()}
        record = dict(schema_version=1, kind="network-calibration-archive", status="audited", adoption_allowed=False,
            approval_sha256=approval_sha256, network_protocol_sha256=network_sha256, paired_protocol_sha256=paired_sha256,
            archiver_sha256=sha(__file__), source_result_sha256=result_sha256,
            copied_result_sha256=sha(out / "calibration/result.json"), source_inventory_before_after_unchanged=True,
            source_result_before_after_unchanged=True, source_hardlinks_created=0,
            native_process_count=len(jobs), all_stable_windows_qualified=summary["all_stable_windows_qualified"],
            all_aa_qualified=summary["all_aa_qualified"], logical_wire_bytes=logical_wire_bytes,
            total_logical_file_bytes=sum(value["bytes"] for value in files.values()),
            unique_content_bytes=sum(size for _,size in content),
            unique_stored_file_bytes_by_mode=sum(size for _,size,_ in canonical_copies),
            duplicate_files_using_internal_hardlinks=len(links), copied_file_count=len(files),
            files=snapshot(out)["files"])
        require(record["copied_result_sha256"] == result_sha256, "archived result differs")
        write_new(out / "EVIDENCE.json", record)
        with (out / "SEALED").open("x") as target:
            target.write(sha(out / "EVIDENCE.json") + "\n")
        require(read(out / "EVIDENCE.json") == record and (out / "SEALED").read_text().strip() == sha(out / "EVIDENCE.json"),
                "archive publication verification differs")
        return {name:record[name] for name in ("status", "native_process_count", "all_stable_windows_qualified",
                    "logical_wire_bytes", "unique_content_bytes", "duplicate_files_using_internal_hardlinks")}
    except Exception as error:
        if not (out / "SEALED").exists():
            write_new(out / "archive-failure.json", dict(status="failed", error=str(error), adoption_allowed=False,
                partial_archive_preserved=True, source_hardlinks_created=0))
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--approval-sha256", required=True)
    parser.add_argument("--network-sha256", required=True)
    parser.add_argument("--paired-sha256", required=True)
    args = parser.parse_args()
    print(canonical(archive(args.source, args.out, args.approval_sha256, args.network_sha256, args.paired_sha256)).decode(), end="")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("network archive rejected: " + str(error), file=sys.stderr)
        sys.exit(1)
