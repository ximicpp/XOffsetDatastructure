#!/usr/bin/env python3
"""Read-only transfer verification; optional fresh candidate materialization.

Python 3.9+ and the standard library only. Git is used solely to read the
current index's two dependency gitlinks. No build, network, archive extraction,
benchmark, old authorization, or process-control operation is performed.
Successful verification is an integrity result, never a new-platform test.
"""

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import sys


MANIFEST = "handoff/mac-20261020/TRANSFER_MANIFEST.json"
HISTORICAL_MATERIALIZATION = "handoff/github-20261019/C14_MATERIALIZATION.json"
HISTORICAL_MATERIALIZATION_SHA256 = (
    "2ccb94204305ff51d5df2cea345e2e10067d342336ef226906ec847414362081"
)
MAIN = "xoffsetdatastructure.hpp"
BASELINE_MAIN_SHA256 = (
    "66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3"
)
CANDIDATE_MAIN_SHA256 = (
    "155f0847dff72047ac580ae8d3ead6927e52ff84eb5c9062ea2d4999980cd501"
)
PATCH_SHA256 = "02273bdee63a43031183a370ad19f18b3af5e1910e483711c0e27573e3dc8ccb"
GITLINKS = {
    "external/boost": "da041154c6bac1a4aa98254a7d6819059e8ac0b0",
    "external/typelayout": "9a6de1d73617631a9612ba9f572d6c0844a90b19",
}
RECEIPT = "MAC_CANDIDATE_MATERIALIZATION.json"
SHA256 = re.compile(r"[0-9a-f]{64}\Z")


class HandoffError(Exception):
    pass


def require(condition, message):
    if not condition:
        raise HandoffError(message)


def relative_path(value):
    require(isinstance(value, str) and value, "Expected a nonempty relative path")
    require("\\" not in value and ":" not in value and "\0" not in value,
            "Unsafe path: {!r}".format(value))
    path = PurePosixPath(value)
    require(not path.is_absolute() and path.as_posix() == value,
            "Path must be canonical and relative: {!r}".format(value))
    require(bool(path.parts) and all(part not in ("", ".", "..") for part in path.parts),
            "Path escapes or aliases the root: {!r}".format(value))
    return path


def is_link(info):
    # Reject Windows junctions/reparse points as well as POSIX symbolic links.
    return stat.S_ISLNK(info.st_mode) or bool(
        getattr(info, "st_file_attributes", 0)
        & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    )


def checked_source(root, relative):
    parts = relative_path(relative).parts
    current = root
    for number, part in enumerate(parts):
        current = current / part
        info = current.lstat()
        require(not is_link(info), "Symlink/reparse point is forbidden: " + relative)
        if number + 1 == len(parts):
            require(stat.S_ISREG(info.st_mode), "Not a regular file: " + relative)
        else:
            require(stat.S_ISDIR(info.st_mode), "Not a directory: " + relative)
    require(current.resolve().is_relative_to(root), "Path escapes root: " + relative)
    return current


def fingerprint(info):
    return (info.st_dev, info.st_ino, info.st_mode, info.st_size,
            info.st_mtime_ns, info.st_ctime_ns)


def identity(path):
    before = path.lstat()
    require(stat.S_ISREG(before.st_mode) and not is_link(before),
            "Not a regular non-symlink file: " + str(path))
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        require(fingerprint(os.fstat(stream.fileno())) == fingerprint(before),
                "File changed before reading: " + str(path))
        while True:
            chunk = stream.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
            size += len(chunk)
        require(fingerprint(os.fstat(stream.fileno())) == fingerprint(before),
                "File changed while reading: " + str(path))
    require(fingerprint(path.lstat()) == fingerprint(before),
            "File changed after reading: " + str(path))
    return {"bytes": size, "sha256": digest.hexdigest()}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate JSON key: " + key)
        result[key] = value
    return result


def read_json(path):
    with path.open("r", encoding="utf-8") as stream:
        value = json.load(stream, object_pairs_hook=unique_object)
    require(isinstance(value, dict), "Expected a JSON object: " + str(path))
    return value


def file_map(value, label, count=None):
    require(isinstance(value, dict) and value, label + " must be a nonempty object")
    if count is not None:
        require(len(value) == count, "{} must contain {} files".format(label, count))
    folded = set()
    for name, record in value.items():
        relative_path(name)
        require(name.casefold() not in folded, label + " contains case-fold aliases")
        folded.add(name.casefold())
        require(isinstance(record, dict), label + " has an invalid identity: " + name)
        require(type(record.get("bytes")) is int and record["bytes"] >= 0,
                label + " has an invalid byte count: " + name)
        require(isinstance(record.get("sha256"), str)
                and SHA256.fullmatch(record["sha256"]) is not None,
                label + " has an invalid SHA-256: " + name)
    return value


def verify_map(root, records, label):
    for name, expected in records.items():
        actual = identity(checked_source(root, name))
        require(actual == {"bytes": expected["bytes"], "sha256": expected["sha256"]},
                label + " mismatch: " + name)


def read_gitlinks(root):
    environment = os.environ.copy()
    environment["GIT_OPTIONAL_LOCKS"] = "0"
    prefix = ["git", "--literal-pathspecs", "-C", str(root)]
    result = subprocess.run(prefix + ["rev-parse", "--show-prefix"],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            env=environment, check=False, timeout=30)
    require(result.returncode == 0 and result.stdout.strip() == b"",
            "--root must be the Git working tree root")
    result = subprocess.run(prefix + ["ls-files", "--stage", "-z", "--"]
                            + list(GITLINKS), stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, env=environment,
                            check=False, timeout=30)
    require(result.returncode == 0, "Unable to read dependency gitlinks from Git index")
    found = {}
    for raw in result.stdout.split(b"\0"):
        if not raw:
            continue
        metadata, name = raw.decode("utf-8").split("\t", 1)
        mode, commit, stage = metadata.split()
        require(mode == "160000" and stage == "0" and name not in found,
                "Dependency is not an unmerged-free index gitlink: " + name)
        found[name] = commit
    require(found == GITLINKS, "Git index dependency gitlinks differ from fixed commits")
    return found


def verify(root, manifest_name):
    manifest_path = checked_source(root, manifest_name)
    manifest_identity = identity(manifest_path)
    data = read_json(manifest_path)
    require(type(data.get("schema_version")) is int and data["schema_version"] == 1,
            "Unsupported transfer manifest schema")
    require(data.get("kind") == "mac-continuation-handoff-20261020",
            "Unexpected transfer manifest kind")
    files = file_map(data.get("files"), "files")
    require(manifest_name not in files, "Manifest must not include itself")
    baseline = file_map(data.get("baseline_files"), "baseline_files", 154)
    production = file_map(data.get("production_files"), "production_files", 14)
    require(data.get("gitlinks") == GITLINKS, "Unexpected dependency gitlink identities")
    require(HISTORICAL_MATERIALIZATION in files,
            "files must include the original C14 materialization metadata")
    require(files[HISTORICAL_MATERIALIZATION]["sha256"]
            == HISTORICAL_MATERIALIZATION_SHA256,
            "Original C14 materialization metadata identity changed")
    # Verify the complete explicit transfer before interpreting candidate inputs.
    verify_map(root, files, "Transfer file")
    historical = read_json(checked_source(root, HISTORICAL_MATERIALIZATION))
    require(baseline == historical.get("non_external_files"),
            "baseline_files differs from the original 154-file C14 source")
    require(production == historical.get("production_files"),
            "production_files differs from the original 14-file C14 production set")
    require(all(name in baseline and record == baseline[name]
                for name, record in production.items()),
            "Production identities are not a subset of baseline identities")
    require(baseline.get(MAIN, {}).get("sha256") == BASELINE_MAIN_SHA256,
            "Unexpected baseline main header")
    verify_map(root, baseline, "C14 baseline file")
    candidate = data.get("candidate")
    require(isinstance(candidate, dict), "Missing candidate identity")
    require(candidate.get("main_sha256") == CANDIDATE_MAIN_SHA256
            and candidate.get("baseline_main_sha256") == BASELINE_MAIN_SHA256,
            "Unexpected candidate/baseline header identity")
    header = candidate.get("header")
    patch = candidate.get("patch")
    relative_path(header)
    relative_path(patch)
    require(header in files and files[header]["sha256"] == CANDIDATE_MAIN_SHA256,
            "Candidate header is not a verified transfer member")
    require(patch in files and files[patch]["bytes"] == 2520
            and files[patch]["sha256"] == PATCH_SHA256,
            "Combined patch is not the fixed verified transfer member")
    require(header not in baseline and patch not in baseline,
            "Candidate inputs must remain separate from the default C14 source")
    gitlinks = read_gitlinks(root)
    require(identity(manifest_path) == manifest_identity,
            "Transfer manifest changed during verification")
    return data, manifest_identity, gitlinks


def destination_path(root, argument):
    destination = Path(os.path.abspath(os.path.expanduser(argument)))
    # Check every existing component before resolving; never silently follow a
    # destination symlink, junction, or reparse point, including a dangling link.
    current = Path(destination.anchor)
    for part in destination.parts[1:]:
        current = current / part
        try:
            info = current.lstat()
        except FileNotFoundError:
            continue
        require(not is_link(info), "Destination has a symlink/reparse component: " + str(current))
        require(stat.S_ISDIR(info.st_mode), "Destination has a non-directory component: " + str(current))
    require(not os.path.lexists(str(destination)),
            "Destination already exists; it will never be overwritten: " + str(destination))
    destination = destination.resolve(strict=False)
    require(destination != root and destination not in root.parents,
            "Destination cannot be the source root or an ancestor")
    if destination.is_relative_to(root):
        parts = destination.relative_to(root).parts
        require(len(parts) >= 2 and parts[0] == "build",
                "A destination inside the repository must be a fresh child of build/")
    return destination


def materialize(root, data, manifest_identity, argument):
    destination = destination_path(root, argument)
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive creation is intentional. Failure leaves any partial copy intact
    # for inspection; it is never removed or silently reused on a later run.
    destination.mkdir(exist_ok=False)
    source_files = {}
    for name, record in data["baseline_files"].items():
        source_name = data["candidate"]["header"] if name == MAIN else name
        expected = (data["files"][source_name] if name == MAIN else record)
        source = checked_source(root, source_name)
        target = destination.joinpath(*relative_path(name).parts)
        target.parent.mkdir(parents=True, exist_ok=True)
        with source.open("rb") as incoming, target.open("xb") as outgoing:
            shutil.copyfileobj(incoming, outgoing, 1024 * 1024)
        os.chmod(str(target), stat.S_IMODE(source.stat().st_mode))
        actual = identity(target)
        require(actual == {"bytes": expected["bytes"], "sha256": expected["sha256"]},
                "Fresh candidate copy mismatch (partial directory preserved): " + name)
        source_files[name] = actual
    require(len(source_files) == 154, "Candidate source member count changed")
    verify_map(destination, source_files, "Materialized candidate")
    production = {name: source_files[name] for name in data["production_files"]}
    require(len(production) == 14, "Candidate production member count changed")
    verify_map(destination, production, "Materialized production file")
    require(identity(checked_source(root, MANIFEST)) == manifest_identity,
            "Transfer manifest changed during materialization; partial copy preserved")
    receipt = {
        "schema_version": 1,
        "kind": "fresh-mac-candidate-materialization-20261020",
        "observed_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "source_root": str(root),
        "destination": str(destination),
        "transfer_manifest": {"path": MANIFEST, **manifest_identity},
        "candidate_header_sha256": CANDIDATE_MAIN_SHA256,
        "baseline_header_sha256": BASELINE_MAIN_SHA256,
        "source_files": source_files,
        "production_files": production,
        "gitlinks_required": GITLINKS,
        "dependencies_populated": False,
        "receipt_excluded_from_source_files": RECEIPT,
        "copied_SOURCE_MANIFEST_json": {
            "status": "unchanged_historical_C14_baseline_description",
            "is_candidate_manifest": False,
            "candidate_file_identities_are_in_this_receipt": True,
        },
        "default_repository_source_modified": False,
        "new_platform_tested": False,
        "new_native_or_performance_executions": 0,
        "requires_new_platform_toolchain_and_functional_validation": True,
    }
    receipt_path = destination / RECEIPT
    with receipt_path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    return {"destination": str(destination), "source_files_verified": 154,
            "production_files_verified": 14,
            "receipt": str(receipt_path), "receipt_identity": identity(receipt_path),
            "historical_SOURCE_MANIFEST_is_candidate_manifest": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parent.parent,
                        help="Git working tree root (default: this script's repository)")
    parser.add_argument("--materialize-candidate", metavar="FRESH_DIRECTORY",
                        help="After full verification, copy 154 files into a new directory and substitute only the candidate main header")
    args = parser.parse_args(argv)
    try:
        root = args.root.resolve(strict=True)
        require(root.is_dir(), "--root must be a directory")
        data, manifest_identity, gitlinks = verify(root, MANIFEST)
        result = {
            "status": "passed_integrity_only",
            "root": str(root),
            "transfer_manifest": {"path": MANIFEST, **manifest_identity},
            "transfer_files_verified": len(data["files"]),
            "baseline_files_verified": 154,
            "production_files_verified": 14,
            "git_index_gitlinks_verified": gitlinks,
            "candidate_header_sha256": CANDIDATE_MAIN_SHA256,
            "default_repository_source_modified": False,
            "new_platform_tested": False,
            "new_native_or_performance_executions": 0,
        }
        if args.materialize_candidate:
            result["candidate_materialization"] = materialize(
                root, data, manifest_identity, args.materialize_candidate)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (HandoffError, OSError, ValueError, subprocess.SubprocessError) as error:
        print(json.dumps({"status": "failed", "error": str(error),
                          "new_platform_tested": False,
                          "new_native_or_performance_executions": 0,
                          "partial_materialization_if_any_is_preserved": True},
                         ensure_ascii=False), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
