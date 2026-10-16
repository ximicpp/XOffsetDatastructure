#!/usr/bin/env python3
"""Freeze exact historical D/F sources for review; never build or run them."""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sys
import tarfile

sys.dont_write_bytecode = True
import paired_measurement as P

FILES = ("xoffsetdatastructure.hpp", "xoffset_virtual_memory.hpp", "bench_vm_compare.cpp", "bench_vm_improvements.cpp")
EXPECTED_D = {
    "xoffsetdatastructure.hpp": "896ac72ba55564bfa6177df3b89d7c192e0b2584c092fdb409c63e828bfadfb0",
    "xoffset_virtual_memory.hpp": "af90d9ea4281cba703878a1042ec63aedeb3fa1807fbe8feacd4b6dfae72b186",
}


def prepare(evidence, out):
    evidence, out = Path(evidence), Path(out)
    metadata = P.read(evidence / "metadata.json")
    archive_path = evidence / "source_snapshots.tar.gz"
    P.require(P.sha(archive_path) == metadata["archive_sha256"], "historical source archive hash differs")
    for variant, expected in (("D", EXPECTED_D), ("F", P.BASELINE)):
        P.require(all(metadata["source_manifests"][variant].get(k) == v for k, v in expected.items()), "historical variant identity differs")
    content = {}
    with tarfile.open(archive_path, "r:gz") as archive:
        members = archive.getmembers()
        names = [m.name for m in members]
        P.require(len(names) == len(set(names)), "duplicate historical archive member")
        by_name = {m.name: m for m in members}
        for variant in ("D", "F"):
            for name in FILES:
                member_name = "snapshots/" + variant + "/" + name
                member = by_name.get(member_name)
                P.require(member is not None and member.isfile() and member.size <= 8 * 1024**2, "unsafe/missing source member")
                data = archive.extractfile(member).read()
                P.require(hashlib.sha256(data).hexdigest() == metadata["source_manifests"][variant][name], "source member hash differs")
                content[variant + "/" + name] = data
    for name in ("bench_vm_compare.cpp", "bench_vm_improvements.cpp"):
        P.require(content["D/" + name] == content["F/" + name], "historical harness differs between D/F")
    out.mkdir(parents=True, exist_ok=False)
    for name, data in content.items():
        target = out / "source" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open("xb") as stream:
            stream.write(data)
    source_inventory = P.inventory(out / "source")
    with (out / "historical-metadata.json").open("xb") as target:
        target.write((evidence / "metadata.json").read_bytes())
    record = dict(schema_version=1, kind="default-source-preparation", status="ready_for_review",
                  executed_cpp=False, performance_evidence=False, historical_evidence_reexecuted=False,
                  historical_metadata_sha256=P.sha(evidence / "metadata.json"),
                  historical_archive_sha256=P.sha(archive_path), source_files=source_inventory,
                  historical_cases=metadata["cases"], inherited_build_commands=metadata["compile_commands"],
                  pending=["parent approval", "new frozen toolchain/runtime checks", "queue-owned build if needed",
                           "reviewed exact raw/normalized adapter", "single bounded calibration window before comparisons"],
                  limits=["source preparation only", "old timing results are not new measurements",
                          "do not run legacy benchmark runner against schema-1 historical metadata"])
    P.write_new(out / "preparation.json", record)
    return record


def audit(out):
    out = Path(out)
    record = P.read(P.safe_file(out, "preparation.json"))
    P.require(type(record.get("schema_version")) is int and record["schema_version"] == 1
              and record.get("status") == "ready_for_review"
              and record.get("kind") == "default-source-preparation" and record.get("executed_cpp") is False
              and record.get("performance_evidence") is False and record.get("historical_evidence_reexecuted") is False,
              "wrong preparation claim")
    metadata = P.read(P.safe_file(out, "historical-metadata.json"))
    P.require(P.sha(out / "historical-metadata.json") == record.get("historical_metadata_sha256"),
              "frozen historical metadata changed")
    expected = {v + "/" + f: dict(sha256=metadata["source_manifests"][v][f],
                                      bytes=P.safe_file(out, "source/" + v + "/" + f).stat().st_size)
                for v in ("D", "F") for f in FILES}
    P.require(P.canonical(P.inventory(out / "source")) == P.canonical(expected) == P.canonical(record.get("source_files")),
              "source preparation identity differs")
    for variant, identities in (("D", EXPECTED_D), ("F", P.BASELINE)):
        P.require(all(metadata["source_manifests"][variant].get(k) == v for k, v in identities.items()), "wrong variant")
    for f in FILES[2:]:
        P.require(expected["D/" + f] == expected["F/" + f], "different D/F harness")
    P.require(record.get("historical_archive_sha256") == metadata.get("archive_sha256")
              and P.canonical(record.get("historical_cases")) == P.canonical(metadata.get("cases"))
              and P.canonical(record.get("inherited_build_commands")) == P.canonical(metadata.get("compile_commands")),
              "historical metadata binding differs")
    return dict(status="passed", kind="default-source-preparation", executed_cpp=False,
                performance_evidence=False, source_files=len(expected))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--historical-evidence", type=Path)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--audit", type=Path)
    args = parser.parse_args()
    if args.audit:
        print(P.canonical(audit(args.audit)).decode(), end="")
        return
    if not (args.historical_evidence and args.out):
        parser.error("--historical-evidence and --out are required for prepare")
    record = prepare(args.historical_evidence, args.out)
    print(P.canonical(dict(status=record["status"], source_files=len(record["source_files"]), executed_cpp=False)).decode(), end="")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, tarfile.TarError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
