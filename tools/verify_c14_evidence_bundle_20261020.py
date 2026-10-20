#!/usr/bin/env python3
"""Read-only portable C14 Q32/F64 CAS integrity check (Python 3.9+, stdlib).

This reads each archived content blob once. It never expands logical hard-link
aliases, imports project modules, runs archived tools/native code, or contacts a
network. Passing is transport integrity, not platform/performance validation.
"""
import argparse
import gzip
import hashlib
import json
import re
import stat
import sys
import zipfile
from pathlib import Path, PurePosixPath

HEX = re.compile(r"[0-9a-f]{64}\Z")
MAX_VOLUME = 90 * 1024 * 1024
MAX_BLOB = 64 * 1024 * 1024


def need(ok, message):
    if not ok:
        raise ValueError(message)


def safe_name(name):
    need(isinstance(name, str) and name and "\\" not in name
         and "\x00" not in name and ":" not in name, "unsafe relative path")
    p = PurePosixPath(name)
    need(not p.is_absolute() and all(x not in ("", ".", "..") for x in name.split("/")),
         "unsafe relative path: " + name)
    return name


def regular_file(path):
    s = path.lstat()
    need(stat.S_ISREG(s.st_mode), "regular non-symlink file required: " + str(path))
    return s


def identity(path):
    regular_file(path)
    h = hashlib.sha256()
    n = 0
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            n += len(block)
            h.update(block)
    return {"bytes": n, "sha256": h.hexdigest()}


def checked_path(base, record):
    name = safe_name(record["name"])
    need("/" not in name, "bundle members must be adjacent files")
    p = base / name
    need(identity(p) == {"bytes": record["bytes"], "sha256": record["sha256"]},
         "bundle member identity mismatch: " + name)
    return p


def bounded_gzip(path, limit):
    with gzip.open(path, "rb") as stream:
        raw = stream.read(limit + 1)
    need(len(raw) <= limit, "metadata expansion limit exceeded")
    return raw


def verify(manifest_path):
    regular_file(manifest_path)
    need(manifest_path.stat().st_size <= 4 * 1024 * 1024, "oversize manifest")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    need(manifest["format"] == "c14-qf-portable-cas-v1", "unsupported bundle format")
    base = manifest_path.parent
    catalog_path = checked_path(base, manifest["catalog"])
    paths_path = checked_path(base, manifest["logical_paths"])
    catalog = json.loads(bounded_gzip(catalog_path, 64 * 1024 * 1024))
    blobs = {}
    for entry in catalog["blobs"]:
        sha = entry["sha256"]
        need(HEX.fullmatch(sha) is not None and sha not in blobs, "duplicate/invalid CAS key")
        need(isinstance(entry["bytes"], int) and 0 <= entry["bytes"] <= MAX_BLOB,
             "invalid blob size")
        need(entry["member"] == "cas/sha256/" + sha, "invalid CAS member path")
        safe_name(entry["volume"])
        blobs[sha] = entry

    scopes = {}
    for entry in manifest["scopes"]:
        key = entry["key"]
        need(isinstance(key, str) and re.fullmatch(r"[A-Za-z0-9_-]+", key)
             and key not in scopes, "duplicate/invalid scope")
        scopes[key] = entry
    references = set()
    logical_keys = set()
    counts = {key: [0, 0] for key in scopes}
    raw_paths = bounded_gzip(paths_path, 128 * 1024 * 1024)
    for line in raw_paths.splitlines():
        entry = json.loads(line)
        scope, relative, sha = entry["scope"], safe_name(entry["path"]), entry["sha256"]
        need(scope in scopes and (scope, relative) not in logical_keys,
             "unknown scope or duplicate logical path")
        need(sha in blobs and entry["bytes"] == blobs[sha]["bytes"],
             "invalid logical CAS reference")
        need(isinstance(entry["mode"], int) and 0 <= entry["mode"] <= 0o7777,
             "invalid original mode")
        logical_keys.add((scope, relative))
        references.add(sha)
        counts[scope][0] += 1
        counts[scope][1] += entry["bytes"]
    need(references == set(blobs), "orphan/unreferenced CAS blob")
    for key, (count, total) in counts.items():
        need(count == scopes[key]["files"] and total == scopes[key]["logical_bytes"],
             "scope count/size mismatch: " + key)

    volume_names = set()
    verified = set()
    archive_bytes = 0
    for record in manifest["volumes"]:
        name = record["name"]
        need(name not in volume_names and record["bytes"] < MAX_VOLUME, "duplicate/oversize volume")
        volume_names.add(name)
        path = checked_path(base, record)
        archive_bytes += record["bytes"]
        with zipfile.ZipFile(path, "r") as archive:
            local = set()
            for info in archive.infolist():
                member = safe_name(info.filename)
                need(member not in local and not info.is_dir(), "duplicate/directory ZIP member")
                local.add(member)
                need(info.compress_type in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED)
                     and not info.flag_bits & 1, "unsupported/encrypted ZIP member")
                mode = info.external_attr >> 16
                need(stat.S_ISREG(mode), "nonregular ZIP member")
                need(member.startswith("cas/sha256/"), "unexpected ZIP member")
                sha = member[len("cas/sha256/"):]
                need(sha in blobs and sha not in verified, "unknown/repeated CAS content")
                expected = blobs[sha]
                need(expected["volume"] == name and expected["member"] == member
                     and expected["bytes"] == info.file_size <= MAX_BLOB,
                     "ZIP/catalog mapping mismatch")
                h, size = hashlib.sha256(), 0
                with archive.open(info, "r") as stream:
                    for block in iter(lambda: stream.read(1024 * 1024), b""):
                        size += len(block)
                        need(size <= expected["bytes"], "CAS expansion exceeds declared bytes")
                        h.update(block)
                need(size == expected["bytes"] and h.hexdigest() == sha,
                     "CAS content checksum mismatch")
                verified.add(sha)
    need(verified == set(blobs), "missing CAS content")
    logical_bytes = sum(x[1] for x in counts.values())
    unique_bytes = sum(x["bytes"] for x in blobs.values())
    summary = manifest["summary"]
    need(summary["logical_files"] == len(logical_keys)
         and summary["logical_bytes"] == logical_bytes
         and summary["cas_blobs"] == len(blobs)
         and summary["cas_bytes"] == unique_bytes
         and summary["zip_bytes"] == archive_bytes, "summary mismatch")
    return {"status": "passed", "format": manifest["format"],
            "manifest": identity(manifest_path), "logical_files": len(logical_keys),
            "logical_bytes": logical_bytes, "cas_blobs": len(blobs),
            "cas_bytes": unique_bytes, "volumes": len(volume_names),
            "zip_bytes": archive_bytes, "source_execution": False,
            "logical_aliases_expanded": False,
            "platform_or_performance_acceptance": False}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--manifest", type=Path,
                   default=Path(__file__).resolve().parents[1] /
                   "handoff/mac-20261020/evidence/BUNDLE.json")
    a = p.parse_args()
    try:
        result = verify(a.manifest)
    except (OSError, ValueError, KeyError, TypeError, zipfile.BadZipFile) as exc:
        print(json.dumps({"status": "failed", "error": str(exc)}, ensure_ascii=False))
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
