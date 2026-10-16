#!/usr/bin/env python3
"""Archive a completed default calibration without duplicate wire storage."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import stat
import sys

sys.dont_write_bytecode = True
import default_measurement as D
import paired_measurement as P


def archive(source, out, approval_sha256):
    source, out = Path(source), Path(out)
    P.require(P.digest(approval_sha256) and P.sha(source / "approval.json") == approval_sha256, "external approval differs")
    out.mkdir(parents=True, exist_ok=False)
    copies, links = {}, []

    def copy_verified(original, target):
        original, target = Path(original), Path(target)
        P.require(original.is_file() and not original.is_symlink(), "unsafe source file")
        identity = (P.sha(original), original.stat().st_size, stat.S_IMODE(original.stat().st_mode))
        if identity in copies:
            os.link(copies[identity], target)
            links.append(dict(path=target.relative_to(out).as_posix(),
                              canonical=copies[identity].relative_to(out).as_posix(), sha256=identity[0], bytes=identity[1]))
        else:
            shutil.copy2(original, target)
            copies[identity] = target
        P.require(P.sha(target) == identity[0] and target.stat().st_size == identity[1], "copied bytes differ")
        return str(target)

    try:
        shutil.copytree(source, out / "calibration", copy_function=copy_verified)
        verdict = D.audit(out / "calibration", approval_sha256)
        P.require(verdict["status"] == "passed" and verdict["kind"] == "default-calibration", "copied calibration audit failed")
        summary = P.read(out / "calibration/summary.json")
        P.write_new(out / "hardlink-inventory.json", dict(scope="internal archive links only; original source files untouched",
                    byte_verification="each source and copied/hardlinked destination SHA256 and length checked", links=links))
        (out / "audit.log").write_bytes(P.canonical(verdict))
        record = dict(schema_version=1, kind="default-calibration-archive", status="audited",
                      approval_sha256=approval_sha256, native_processes=78,
                      all_aa_qualified=summary["all_aa_qualified"], adoption_allowed=False,
                      source_result_sha256=P.sha(source / "result.json"),
                      copied_result_sha256=P.sha(out / "calibration/result.json"),
                      unique_content_bytes=sum(v.stat().st_size for v in copies.values()),
                      logical_top_level_wire_bytes=sum(v.stat().st_size for v in (out / "calibration/commands").glob("*.wire")),
                      duplicate_files_using_internal_hardlinks=len(links),
                      files=P.inventory(out, ("EVIDENCE.json", "SEALED")))
        P.require(record["source_result_sha256"] == record["copied_result_sha256"], "archive changed original result")
        P.write_new(out / "EVIDENCE.json", record)
        with (out / "SEALED").open("x") as target:
            target.write(P.sha(out / "EVIDENCE.json") + "\n")
        return {k: record[k] for k in ("status", "native_processes", "all_aa_qualified", "unique_content_bytes", "duplicate_files_using_internal_hardlinks")}
    except Exception as error:
        P.write_new(out / "archive-failure.json", dict(status="failed", error=str(error), adoption_allowed=False))
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--approval-sha256", required=True)
    args = parser.parse_args()
    print(P.canonical(archive(args.source, args.out, args.approval_sha256)).decode(), end="")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
