#!/usr/bin/env python3
"""Prepare a small Windows storage handoff from the pinned Stage II archive."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import sys

sys.dont_write_bytecode = True
from bundle_support import (ARCHIVE_SHA256, F_OS_SHA256, archive_files, digest,
                            frozen_manifest, tree_manifest, verify_bundle)


def prepare(source_archive: Path, source_manifest: Path, out: Path):
    # Reject existing files, directories and dangling symlinks without touching them.
    if os.path.lexists(out):
        raise ValueError("--out must be a new directory")
    manifest_bytes = source_manifest.read_bytes()
    manifest = frozen_manifest(manifest_bytes)
    if source_archive.stat().st_size > 20 * 1024 * 1024:
        raise ValueError("Archive exceeds size limit")
    if digest(source_archive.read_bytes()) != ARCHIVE_SHA256:
        raise ValueError("Archive does not match frozen Stage II source")
    files = archive_files(source_archive, manifest)
    if digest(files["xoffset_virtual_memory.hpp"]) != F_OS_SHA256:
        raise ValueError("F OS header identity mismatch")
    support = Path(__file__).resolve().parent
    helpers = {name: (support / name).read_bytes() for name in
               ("bundle_support.py", "validate_windows.py", "run-windows.ps1")}
    # All archive validation completes before the destination is created.
    out.mkdir(parents=True, exist_ok=False)
    for name, data in files.items():
        target = out / "source" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    (out / "source-manifest.json").write_bytes(manifest_bytes)
    for name, data in helpers.items():
        (out / name).write_bytes(data)
    record = {"schema": 1, "status": "prepared-not-windows-tested",
              "source_archive_sha256": ARCHIVE_SHA256, "f_os_sha256": F_OS_SHA256,
              "source_file_count": len(files), "source_bytes": sum(map(len, files.values())),
              "files": tree_manifest(out)}
    (out / "bundle-manifest.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    verify_bundle(out)
    return record


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-archive", type=Path, required=True)
    parser.add_argument("--source-manifest", type=Path,
                        help="defaults to source-manifest.json beside --source-archive")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        record = prepare(args.source_archive,
                         args.source_manifest or args.source_archive.with_name("source-manifest.json"),
                         args.out)
        print(json.dumps({k: v for k, v in record.items() if k != "files"}, indent=2))
        return 0
    except (OSError, ValueError) as exc:
        print(str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
