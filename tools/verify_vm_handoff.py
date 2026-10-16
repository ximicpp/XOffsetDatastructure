#!/usr/bin/env python3
"""Verify the published initial handoff snapshot, without changing any files.

Later legitimate development can cause a mismatch: explain those differences;
do not reset or overwrite the working tree to make this check pass. This tool
needs only Python 3.9+ and does not download dependencies, build, or invoke Git.
"""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import stat
import sys


MANIFEST = "handoff/manifest.json"
MAX_REPORTED_ERRORS = 10


class VerificationError(Exception):
    """Invalid manifest or a file that does not match the initial snapshot."""


def _relative_path(value):
    if not isinstance(value, str) or not value or "\\" in value or "\x00" in value:
        raise VerificationError("path must be a nonempty relative POSIX path")
    parts = value.split("/")
    if (PurePosixPath(value).is_absolute() or PureWindowsPath(value).drive
            or any(part in ("", ".", "..") for part in parts) or ":" in value):
        raise VerificationError("path must be canonical and relative (no traversal or drive)")
    return value


def _is_link(info):
    # Windows directory junctions are reparse points, not always POSIX symlinks.
    return (stat.S_ISLNK(info.st_mode)
            or bool(getattr(info, "st_file_attributes", 0)
                    & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)))


def _regular_file(root, relative):
    """Reject links in the file and each ancestor inside the selected root."""
    root_info = root.lstat()
    if _is_link(root_info) or not stat.S_ISDIR(root_info.st_mode):
        raise VerificationError("selected root must be a directory, not a link")
    parts = relative.split("/")
    current = root
    for index, part in enumerate(parts):
        current = current / part
        info = current.lstat()
        if _is_link(info):
            raise VerificationError("symlink or reparse point is forbidden")
        if index < len(parts) - 1:
            if not stat.S_ISDIR(info.st_mode):
                raise VerificationError("path ancestor is not a directory")
        elif not stat.S_ISREG(info.st_mode):
            raise VerificationError("entry is not a regular file")
    return current


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise VerificationError("duplicate JSON key: {!r}".format(key))
        result[key] = value
    return result


def _invalid_constant(value):
    raise VerificationError("invalid JSON constant: " + value)


def _load_manifest(root):
    path = _regular_file(root, MANIFEST)
    with path.open("r", encoding="utf-8") as stream:
        manifest = json.load(stream, object_pairs_hook=_unique_object,
                             parse_constant=_invalid_constant)
    if not isinstance(manifest, dict):
        raise VerificationError("manifest must be an object")
    version = manifest.get("schema_version")
    if type(version) is not int or version != 1:
        raise VerificationError("schema_version must be integer 1")
    if not isinstance(manifest.get("files"), dict) or not manifest["files"]:
        raise VerificationError("files must be a nonempty object")
    return manifest


def _record(value):
    if not isinstance(value, dict):
        raise VerificationError("file record must be an object")
    digest = value.get("sha256")
    if not isinstance(digest, str) or re.fullmatch(r"[0-9a-fA-F]{64}", digest) is None:
        raise VerificationError("sha256 must contain exactly 64 hexadecimal characters")
    size = value.get("bytes")
    if type(size) is not int or size < 0:
        raise VerificationError("bytes must be a nonnegative integer")
    if "role" in value and not isinstance(value["role"], str):
        raise VerificationError("optional role must be a string")
    return digest.lower(), size


def verify(root):
    """Return manifest and verified byte count, or raise VerificationError."""
    manifest = _load_manifest(root)
    errors = []
    error_count = 0
    total_bytes = 0
    for relative, record in manifest["files"].items():
        try:
            _relative_path(relative)
            if relative == MANIFEST:
                raise VerificationError("manifest must not list itself")
            expected_digest, expected_size = _record(record)
            path = _regular_file(root, relative)
            digest = hashlib.sha256()
            size = 0
            with path.open("rb") as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    size += len(chunk)
                    digest.update(chunk)
            if size != expected_size:
                raise VerificationError("size mismatch: expected {}, found {}".format(
                    expected_size, size))
            if digest.hexdigest() != expected_digest:
                raise VerificationError("SHA-256 mismatch")
            total_bytes += size
        except (VerificationError, OSError, ValueError) as error:
            error_count += 1
            if len(errors) < MAX_REPORTED_ERRORS:
                errors.append("{!r}: {}".format(relative, error))
    if error_count:
        suffix = ""
        if error_count > len(errors):
            suffix = "\n... {} more error(s) omitted".format(error_count - len(errors))
        raise VerificationError("{} file error(s):\n{}{}".format(
            error_count, "\n".join(errors), suffix))
    return manifest, total_bytes


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1],
                        help="repository root (default: parent of this tools directory)")
    args = parser.parse_args(argv)
    try:
        manifest, total_bytes = verify(args.root.absolute())
    except (VerificationError, OSError, ValueError) as error:
        print("Handoff verification FAILED: {}".format(error), file=sys.stderr)
        print("This checks the published initial snapshot. Explain legitimate later "
              "changes; do not reset or overwrite files to make it pass.", file=sys.stderr)
        return 1
    print("Verified initial handoff snapshot: {} files, {} bytes.".format(
        len(manifest["files"]), total_bytes))
    for key in ("git_base", "handoff_branch"):
        print("{}={}".format(key, json.dumps(manifest.get(key), ensure_ascii=True)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
