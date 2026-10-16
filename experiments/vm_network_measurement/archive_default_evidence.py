#!/usr/bin/env python3
"""Copy verified W3 default prerequisites to a fresh portable evidence folder."""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import sys

sys.dont_write_bytecode = True
import default_measurement as D
import paired_measurement as P


def archive(functional, preregister, coordination, out):
    functional, preregister, coordination, out = map(Path, (functional, preregister, coordination, out))
    spec = D.validate_prereg(P.read(preregister))
    P.require(P.sha(functional / "result.json") == spec["functional_result_sha256"], "functional identity differs")
    out.mkdir(parents=True, exist_ok=False)
    try:
        shutil.copytree(functional, out / "functional")
        for name in ("queue-default-freeze-v1", "queue-default-build-audit-v1", "queue-default-functional-audit-v1", "queue-default-preregister-v1"):
            shutil.copytree(coordination / name, out / "coordination" / name)
        shutil.copy2(preregister, out / "preregister.json")
        P.require(P.sha(out / "preregister.json") == P.sha(preregister), "copied preregistration differs")
        result = D.audit(out / "functional")
        P.require(result["status"] == "passed" and result["kind"] == "default-functional", "copied functional evidence did not pass")
        record = dict(schema_version=1, kind="default-native-prerequisites", status="ready_for_parent_review",
                      functional_result_sha256=P.sha(out / "functional/result.json"),
                      build_result_sha256=P.sha(out / "functional/build/result.json"),
                      preregister_sha256=P.sha(out / "preregister.json"),
                      native_functional_processes=6, formal_timing_executed=False,
                      adoption_allowed=False, audit="portable copied functional archive passed",
                      files=P.inventory(out, ("EVIDENCE.json", "SEALED")))
        P.write_new(out / "EVIDENCE.json", record)
        with (out / "SEALED").open("x") as target:
            target.write(P.sha(out / "EVIDENCE.json") + "\n")
        return dict(status="ready_for_parent_review", native_functional_processes=6,
                    formal_timing_executed=False, evidence_sha256=P.sha(out / "EVIDENCE.json"))
    except Exception as error:
        P.write_new(out / "archive-failure.json", dict(status="failed", error=str(error), formal_timing_executed=False))
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("functional", "preregister", "coordination", "out"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    print(P.canonical(archive(args.functional, args.preregister, args.coordination, args.out)).decode(), end="")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
