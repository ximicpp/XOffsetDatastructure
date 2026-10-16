"""Strict, dependency-free integrity checks for the frozen Windows handoff."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re
import tarfile

ARCHIVE_SHA256 = "c1a69f06cf67db2f773368d5828798873a9faac071f40f0e4c5550507897b3a3"
MANIFEST_SHA256 = "b4db950e010cfe68958360103a5cbfe3330f565c9ec63d2b0f88897daa5d8025"
F_OS_SHA256 = "22f181b52c11ce442a5576ffc0dc35af1828f77deb4424f387d0acfa658239ad"
MAX_FILES = 1024
MAX_BYTES = 20 * 1024 * 1024


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate JSON key: " + key)
        result[key] = value
    return result


def read_json(data: bytes):
    return json.loads(data, object_pairs_hook=unique_object)


def safe_name(name: str):
    if not isinstance(name, str) or not name or "\\" in name:
        raise ValueError("Invalid portable path: " + repr(name))
    for part in name.split("/"):
        if (part in ("", ".", "..") or part.endswith((".", " "))
                or any(ord(c) < 32 or c in '<>:"|?*' for c in part)
                or re.fullmatch(r"(?i)(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])", part.split(".")[0])):
            raise ValueError("Unsafe Windows path: " + name)
    if name.split("/")[0].casefold() in ("external", "build", ".git"):
        raise ValueError("Excluded source tree: " + name)


def validate_manifest(manifest: dict):
    if not isinstance(manifest, dict) or not 0 < len(manifest) <= MAX_FILES:
        raise ValueError("Invalid source manifest size")
    seen = set()
    total = 0
    for name, entry in manifest.items():
        safe_name(name)
        if name.casefold() in seen:
            raise ValueError("Case-insensitive path collision: " + name)
        seen.add(name.casefold())
        if (not isinstance(entry, dict) or set(entry) != {"sha256", "bytes"}
                or not isinstance(entry["sha256"], str)
                or re.fullmatch("[0-9a-f]{64}", entry["sha256"]) is None
                or type(entry["bytes"]) is not int or entry["bytes"] < 0):
            raise ValueError("Invalid manifest entry: " + name)
        total += entry["bytes"]
    if total > MAX_BYTES:
        raise ValueError("Source exceeds size limit")


def frozen_manifest(data: bytes) -> dict:
    if digest(data) != MANIFEST_SHA256:
        raise ValueError("Manifest does not match frozen Stage II source")
    manifest = read_json(data)
    validate_manifest(manifest)
    if manifest.get("xoffset_virtual_memory.hpp", {}).get("sha256") != F_OS_SHA256:
        raise ValueError("Frozen F OS header identity mismatch")
    return manifest


def archive_files(path: Path, manifest: dict) -> dict[str, bytes]:
    """Never extract tar metadata, links, directories, or unchecked file bytes."""
    validate_manifest(manifest)
    if path.stat().st_size > MAX_BYTES:
        raise ValueError("Archive exceeds size limit")
    files = {}
    names = set()
    total = 0
    with tarfile.open(path, "r:gz") as archive:
        for member in archive:
            safe_name(member.name)
            key = member.name.casefold()
            if key in names or len(names) >= MAX_FILES:
                raise ValueError("Duplicate archive member or too many files")
            names.add(key)
            if not member.isfile() or member.issparse():
                raise ValueError("Only ordinary archive files are permitted")
            entry = manifest.get(member.name)
            if entry is None or member.size != entry["bytes"]:
                raise ValueError("Archive inventory/size mismatch: " + member.name)
            total += member.size
            if total > MAX_BYTES:
                raise ValueError("Archive exceeds expanded size limit")
            with archive.extractfile(member) as stream:
                data = stream.read(member.size + 1)
            if len(data) != member.size or digest(data) != entry["sha256"]:
                raise ValueError("Archive hash mismatch: " + member.name)
            files[member.name] = data
    if set(files) != set(manifest):
        raise ValueError("Archive file inventory differs from source manifest")
    return files


def tree_manifest(root: Path) -> dict:
    result = {}
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise ValueError("Symlink forbidden: " + str(path))
        if path.is_file():
            name = path.relative_to(root).as_posix()
            safe_name(name)
            data = path.read_bytes()
            result[name] = {"sha256": digest(data), "bytes": len(data)}
        elif not path.is_dir():
            raise ValueError("Special file forbidden: " + str(path))
    return result


def verify_bundle(root: Path) -> dict:
    original = (root / "source-manifest.json").read_bytes()
    source = frozen_manifest(original)
    if tree_manifest(root / "source") != source:
        raise ValueError("Source tree changed or contains unexpected files")
    expected = read_json((root / "bundle-manifest.json").read_bytes())
    if expected.get("source_archive_sha256") != ARCHIVE_SHA256:
        raise ValueError("Incorrect source archive provenance")
    if expected.get("f_os_sha256") != F_OS_SHA256:
        raise ValueError("Incorrect F OS header provenance")
    files = tree_manifest(root)
    del files["bundle-manifest.json"]
    if files != expected.get("files"):
        raise ValueError("Bundle files changed or contain unexpected files")
    return expected
