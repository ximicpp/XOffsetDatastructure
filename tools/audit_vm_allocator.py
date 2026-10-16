#!/usr/bin/env python3
"""Offline audit for a Stage III allocator evidence bundle (standard library only).

The bundle's manifest.json is a direct relative-file -> SHA-256 mapping and
covers every file except itself and __pycache__.  index.json declares archive,
prepared, timing, correctness, failed_runs and optionally diagnostics.  All
index paths are relative POSIX paths inside the extracted archive.  The frozen
runner.py audits timing/correctness; optional diagnostic records use the frozen
collect_vm_allocator_diagnostics.py.  No compiler, binary or external dependency
is executed by this front end.

--validation-dir additionally verifies that directory's direct hash manifest.
--check-worktree requires --validation-dir and compares baseline headers plus
its final-worktree-manifest.json against --worktree (default: current directory).
The latter can be a direct relative-path hash map, or {"files": {...},
"worktree_root": "/original/root"}; absolute keys must be inside that original
root (or inside --worktree if no original root is recorded).
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import sys
import tarfile
import tempfile


HASH = re.compile(r"[0-9a-fA-F]{64}\Z")
REQUIRED_HEADERS = {"xoffsetdatastructure.hpp", "xoffset_virtual_memory.hpp"}


class AuditError(RuntimeError):
    """Evidence is incomplete, inconsistent or unsafe to inspect."""


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise AuditError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def read_json(path: Path):
    try:
        return json.loads(path.read_text(), object_pairs_hook=_unique_pairs)
    except (OSError, ValueError) as error:
        raise AuditError(f"cannot read JSON {path}: {error}") from error


def relative(value: str) -> PurePosixPath:
    if not isinstance(value, str) or not value or "\\" in value or "\x00" in value:
        raise AuditError(f"unsafe relative path: {value!r}")
    parts = value.split("/")
    if value.startswith("/") or any(part in ("", ".", "..") for part in parts) or ":" in parts[0]:
        raise AuditError(f"unsafe relative path: {value!r}")
    return PurePosixPath(*parts)


def below(root: Path, value: str) -> Path:
    return root.joinpath(*relative(value).parts)


def inventory(root: Path) -> set[str]:
    """Inventory without following symlinks, including ignored cache nodes."""
    if not root.is_dir():
        raise AuditError(f"missing evidence directory: {root}")
    files = set()
    for directory, directories, names in os.walk(root, followlinks=False):
        parent = Path(directory)
        for name in directories + names:
            node = parent / name
            if node.is_symlink():
                raise AuditError(f"symlink in evidence directory: {node}")
            if not node.is_dir() and not node.is_file():
                raise AuditError(f"nonregular evidence entry: {node}")
        directories[:] = [name for name in directories if name != "__pycache__"]
        for name in names:
            path = parent / name
            if "__pycache__" not in path.relative_to(root).parts:
                files.add(path.relative_to(root).as_posix())
    return files


def hash_mapping(value, context: str) -> dict[str, str]:
    if not isinstance(value, dict) or not value:
        raise AuditError(f"empty/invalid hash mapping: {context}")
    for name, digest in value.items():
        relative(name)
        if not isinstance(digest, str) or HASH.fullmatch(digest) is None:
            raise AuditError(f"invalid SHA-256 for {context}: {name}")
    return value


def verify_mapping(root: Path, mapping: dict[str, str], context: str) -> None:
    for name, expected in mapping.items():
        path = below(root, name)
        if not path.resolve().is_relative_to(root.resolve()):
            raise AuditError(f"{context} path escapes its directory: {name}")
        if not path.is_file() or path.is_symlink() or sha(path) != expected.lower():
            raise AuditError(f"missing/changed {context} file: {name}")


def verify_directory_manifest(root: Path) -> int:
    actual = inventory(root)
    mapping = hash_mapping(read_json(root / "manifest.json"), str(root / "manifest.json"))
    if "manifest.json" in mapping:
        raise AuditError("manifest.json must exclude itself")
    declared = set(mapping)
    if actual - {"manifest.json"} != declared:
        missing = sorted(declared - actual)
        extra = sorted(actual - declared - {"manifest.json"})
        raise AuditError(f"manifest inventory differs: missing={missing[:8]}, extra={extra[:8]}")
    verify_mapping(root, mapping, "bundle")
    return len(mapping)


def safe_extract(archive: Path, destination: Path, maximum_bytes: int = 8 * 1024**3) -> dict:
    """Extract regular files/directories only; never delegate to extractall."""
    seen = set()
    total = 0
    count = 0
    try:
        with tarfile.open(archive, "r:gz") as stream:
            for member in stream:
                name = member.name[:-1] if member.isdir() and member.name.endswith("/") else member.name
                normalized = str(relative(name))
                if normalized in seen:
                    raise AuditError(f"duplicate archive member: {normalized}")
                seen.add(normalized)
                count += 1
                if count > 200000:
                    raise AuditError("archive member limit exceeded")
                if not member.isdir() and not member.isfile():
                    raise AuditError(f"archive member is not a regular file/directory: {normalized}")
                target = below(destination, normalized)
                if member.isdir():
                    target.mkdir(parents=True, exist_ok=True)
                    continue
                if member.size < 0 or member.size > maximum_bytes - total:
                    raise AuditError("archive extraction byte budget exceeded")
                total += member.size
                target.parent.mkdir(parents=True, exist_ok=True)
                source = stream.extractfile(member)
                if source is None:
                    raise AuditError(f"missing archive member data: {normalized}")
                with source, target.open("xb") as output:
                    shutil.copyfileobj(source, output, 1024 * 1024)
                if target.stat().st_size != member.size:
                    raise AuditError(f"truncated archive member: {normalized}")
    except (OSError, tarfile.TarError) as error:
        raise AuditError(f"cannot safely extract archive: {error}") from error
    return {"members": count, "regular_bytes": total}


def load_module(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise AuditError(f"cannot load frozen auditor: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    # __pycache__ is intentionally outside the manifest.  Compile the verified
    # source bytes directly, so an ignored, timestamp-matching .pyc cannot
    # replace the frozen auditor's code.
    exec(compile(path.read_bytes(), str(path), "exec"), module.__dict__)
    return module


def verify_prepared(path: Path) -> None:
    actual = inventory(path)
    state = read_json(path / "prepared.json")
    if not isinstance(state, dict) or state.get("status") != "prepared":
        raise AuditError(f"incomplete prepared source set: {path}")
    mapping = hash_mapping(state.get("files"), str(path / "prepared.json"))
    if set(mapping) != actual - {"prepared.json"}:
        raise AuditError(f"prepared inventory differs: {path}")
    verify_mapping(path, mapping, "prepared")


def validate_index(index) -> None:
    if not isinstance(index, dict):
        raise AuditError("invalid evidence index")
    relative(index.get("archive"))
    prepared = index.get("prepared")
    if not isinstance(prepared, dict) or not prepared:
        raise AuditError("missing prepared source inventory")
    for key, path in prepared.items():
        if not isinstance(key, str) or not key:
            raise AuditError("invalid prepared identity")
        relative(path)
    if len(set(prepared.values())) != len(prepared):
        raise AuditError("duplicate prepared source paths")
    if "random_inputs" in index:
        random_inputs = index["random_inputs"]
        if not isinstance(random_inputs, dict):
            raise AuditError("invalid random-input index")
        relative(random_inputs.get("file"))
        relative(random_inputs.get("manifest"))
        if random_inputs["file"] == random_inputs["manifest"]:
            raise AuditError("random-input data and manifest paths must differ")
    seen = set()
    total = 0
    for category in ("timing", "correctness", "failed_runs", "diagnostics"):
        records = index.get(category, [] if category == "diagnostics" else None)
        if not isinstance(records, list):
            raise AuditError(f"missing/invalid index category: {category}")
        for record in records:
            if not isinstance(record, dict):
                raise AuditError(f"invalid index record: {category}")
            path = str(relative(record.get("path")))
            if path in seen:
                raise AuditError(f"duplicate indexed run: {path}")
            seen.add(path)
            total += 1
            if category == "failed_runs":
                if record.get("status") != "failed" or not isinstance(record.get("reason"), str) or not record["reason"].strip():
                    raise AuditError("failed run must have status=failed and an explicit reason")
            if category != "failed_runs" or "prepared" in record:
                if record.get("prepared") not in prepared:
                    raise AuditError(f"unresolved prepared identity: {path}")
    if total == 0:
        raise AuditError("empty run inventory")


def audit_random_inputs(path: Path, manifest_path: Path, maximum_bytes: int = 8 * 1024**3) -> dict:
    """Replay packed trace inputs without loading the full decompressed file."""
    manifest = read_json(manifest_path)
    if (not isinstance(manifest, dict) or manifest.get("algorithm") != "xorshift64(13,7,17)" or
        manifest.get("byte_order") != "little" or type(manifest.get("word_bytes")) is not int or
        manifest["word_bytes"] != 8):
        raise AuditError("unsupported random-input encoding")
    seeds = manifest.get("seeds")
    operations = manifest.get("operations_per_seed")
    digests = manifest.get("sha256_per_seed")
    mask = (1 << 64) - 1
    if (not isinstance(seeds, list) or not seeds or
        any(type(seed) is not int or seed < 0 or seed > mask for seed in seeds) or
        type(operations) is not int or operations <= 0 or
        not isinstance(digests, list) or len(digests) != len(seeds) or
        any(not isinstance(value, str) or HASH.fullmatch(value) is None for value in digests)):
        raise AuditError("invalid random-input seed/count/digest inventory")
    expected_bytes = len(seeds) * operations * 8
    if (type(manifest.get("uncompressed_bytes")) is not int or
        manifest["uncompressed_bytes"] != expected_bytes or expected_bytes > maximum_bytes):
        raise AuditError("random-input byte count/budget mismatch")
    try:
        with gzip.open(path, "rb") as stream:
            for seed_index, seed in enumerate(seeds):
                state = seed
                digest = hashlib.sha256()
                completed = 0
                while completed < operations:
                    words = min(8192, operations - completed)
                    packed = stream.read(words * 8)
                    if len(packed) != words * 8:
                        raise AuditError(f"truncated random inputs at seed {seed_index}, word {completed}")
                    digest.update(packed)
                    for position, (actual,) in enumerate(struct.iter_unpack("<Q", packed)):
                        state = (state ^ (state << 13)) & mask
                        state ^= state >> 7
                        state = (state ^ (state << 17)) & mask
                        if actual != state:
                            raise AuditError(f"random-input word differs at seed {seed_index}, word {completed + position}")
                    completed += words
                if digest.hexdigest() != digests[seed_index].lower():
                    raise AuditError(f"random-input seed digest mismatch: {seed_index}")
            if stream.read(1):
                raise AuditError("extra bytes after declared random inputs")
    except (OSError, EOFError) as error:
        raise AuditError(f"invalid/truncated random-input gzip: {error}") from error
    return {"status": "audited", "seeds": len(seeds), "operations_per_seed": operations,
            "words": len(seeds) * operations}


def verify_failed(path: Path, record: dict) -> None:
    inventory(path)
    state = read_json(path / "result.json")
    if not isinstance(state, dict) or state.get("status") != record["status"]:
        raise AuditError(f"failed-run status differs from index: {record['path']}")
    if (path / "COMPLETE").exists():
        raise AuditError(f"failed run incorrectly has a completion marker: {record['path']}")
    for command in state.get("commands", []):
        if "log" in command:
            log = below(path, command["log"])
            if not log.is_file():
                raise AuditError(f"failed-run referenced log missing: {log}")
            if "log_sha256" in command and sha(log) != command["log_sha256"]:
                raise AuditError(f"failed-run referenced log changed: {log}")
    failure = state.get("failed_command")
    if failure is not None:
        log = path / "failed-command.log"
        if not log.is_file() or log.read_text() != failure.get("stdout", "") + failure.get("stderr", ""):
            raise AuditError(f"failed-command log missing/changed: {path}")


def verify_run_inventory(extracted: Path, index: dict) -> None:
    declared = {below(extracted, record["path"]) for category in ("timing", "correctness", "failed_runs", "diagnostics")
                for record in index.get(category, [])}
    prepared_roots = [below(extracted, path) for path in index["prepared"].values()]
    for result in extracted.rglob("result.json"):
        if any(result.is_relative_to(root) for root in prepared_roots):
            continue
        if result.parent not in declared:
            raise AuditError(f"archive has unindexed result: {result.relative_to(extracted)}")


def baseline_headers(bundle: Path, extracted: Path, index: dict) -> dict:
    value = index.get("baseline_headers")
    if value is None:
        candidates = [bundle / "preregister.json"]
        if "main" in index["prepared"]:
            candidates.append(below(extracted, index["prepared"]["main"]) / "preregister.json")
        for candidate in candidates:
            if candidate.is_file():
                value = read_json(candidate).get("baseline_headers")
                if value is not None:
                    break
    value = hash_mapping(value, "baseline headers")
    if not REQUIRED_HEADERS <= set(value):
        raise AuditError("baseline identity omits production headers")
    return value


def verify_worktree(worktree: Path, validation: Path, baseline: dict) -> int:
    worktree = worktree.resolve()
    verify_mapping(worktree, baseline, "baseline header")
    document = read_json(validation / "final-worktree-manifest.json")
    mapping = document.get("files") if isinstance(document, dict) and "files" in document else document
    original_root = document.get("worktree_root") if isinstance(document, dict) and "files" in document else None
    if not isinstance(mapping, dict) or not mapping:
        raise AuditError("empty/invalid final worktree manifest")
    normalized = {}
    for name, digest in mapping.items():
        if not isinstance(name, str) or not isinstance(digest, str) or HASH.fullmatch(digest) is None:
            raise AuditError("invalid final worktree manifest entry")
        path = Path(name)
        if path.is_absolute():
            anchor = Path(original_root) if original_root else worktree
            try:
                name = path.relative_to(anchor).as_posix()
            except ValueError as error:
                raise AuditError(f"absolute worktree path outside recorded root: {name}") from error
        name = str(relative(name))
        if name in normalized:
            raise AuditError(f"duplicate normalized worktree path: {name}")
        local = below(worktree, name)
        if not local.resolve().is_relative_to(worktree):
            raise AuditError(f"worktree path escapes repository: {name}")
        normalized[name] = digest
    verify_mapping(worktree, normalized, "final worktree")
    return len(normalized)


def audit(bundle: Path, validation_dir: Path | None = None, check_worktree: bool = False,
          worktree: Path | None = None, maximum_extract_bytes: int = 8 * 1024**3) -> dict:
    bundle = Path(bundle).resolve()
    bundle_files = verify_directory_manifest(bundle)
    index = read_json(bundle / "index.json")
    validate_index(index)
    required = {"index.json", "runner.py", index["archive"]}
    if index.get("diagnostics"):
        required.add("collect_vm_allocator_diagnostics.py")
    if "random_inputs" in index:
        required.update(index["random_inputs"][key] for key in ("file", "manifest"))
    declared = read_json(bundle / "manifest.json")
    if not required <= set(declared):
        raise AuditError("manifest omits an index/runner/archive input")
    if check_worktree and validation_dir is None:
        raise AuditError("--check-worktree requires --validation-dir")
    validation_dir = Path(validation_dir).resolve() if validation_dir else None
    validation_files = verify_directory_manifest(validation_dir) if validation_dir else 0
    result = {"status": "audited", "bundle_files": bundle_files,
              "prepared_sets": len(index["prepared"]), "timing_windows": 0,
              "timing_cases": 0, "correctness_runs": 0, "correctness_commands": 0,
              "diagnostic_runs": 0, "diagnostic_samples": 0, "failed_runs": 0,
              "validation_files": validation_files}
    if "random_inputs" in index:
        result["random_inputs"] = audit_random_inputs(below(bundle, index["random_inputs"]["file"]),
                                                     below(bundle, index["random_inputs"]["manifest"]),
                                                     maximum_extract_bytes)
    # Imported auditors are frozen bundle inputs.  Disable bytecode writes so
    # a read-only audit never mutates the supplied archive directory.
    prior_bytecode = sys.dont_write_bytecode
    absent_module = object()
    prior_runner_alias = sys.modules.get("run_vm_allocator", absent_module)
    sys.dont_write_bytecode = True
    try:
        runner = load_module(bundle / "runner.py", "_stage3_frozen_runner_" + sha(bundle / "runner.py")[:16])
        # The collector imports this historical module name.  Bind that import
        # to the bundled runner, never an ambient working-tree installation.
        sys.modules["run_vm_allocator"] = runner
        collector = (load_module(bundle / "collect_vm_allocator_diagnostics.py", "_stage3_frozen_diagnostics")
                     if index.get("diagnostics") else None)
        with tempfile.TemporaryDirectory(prefix="xoffset-stage3-audit-") as temporary:
            extracted = Path(temporary)
            result["archive"] = safe_extract(below(bundle, index["archive"]), extracted, maximum_extract_bytes)
            prepared_paths = {key: below(extracted, path) for key, path in index["prepared"].items()}
            for path in prepared_paths.values():
                verify_prepared(path)
            verify_run_inventory(extracted, index)
            for record in index["timing"]:
                summary = runner.audit(below(extracted, record["path"]), prepared_paths[record["prepared"]])
                if not isinstance(summary, list) or not summary:
                    raise AuditError("timing auditor returned no cases")
                result["timing_windows"] += 1
                result["timing_cases"] += len(summary)
            for record in index["correctness"]:
                summary = runner.audit_validation(below(extracted, record["path"]), prepared_paths[record["prepared"]])
                if not isinstance(summary, dict) or summary.get("status") != "audited":
                    raise AuditError("correctness auditor did not confirm success")
                commands = summary.get("commands")
                if type(commands) is not int or commands <= 0:
                    raise AuditError("correctness auditor returned no commands")
                result["correctness_runs"] += 1
                result["correctness_commands"] += commands
            for record in index.get("diagnostics", []):
                summary = collector.audit(below(extracted, record["path"]), prepared_paths[record["prepared"]])
                if not isinstance(summary, dict) or summary.get("status") != "audited":
                    raise AuditError("diagnostic auditor did not confirm success")
                samples = summary.get("samples")
                if summary.get("formal_timing_forbidden") is not True or type(samples) is not int or samples <= 0:
                    raise AuditError("diagnostic auditor returned no classified samples")
                result["diagnostic_runs"] += 1
                result["diagnostic_samples"] += samples
            if index.get("diagnostics"):
                if not callable(getattr(collector, "compare_timing", None)):
                    raise AuditError("diagnostic collector lacks mandatory timing crosschecks")
                timing_records = [(below(extracted, record["path"]), prepared_paths[record["prepared"]])
                                  for record in index["timing"]]
                result["crosschecks"] = []
                for record in index["diagnostics"]:
                    summary = collector.compare_timing(below(extracted, record["path"]), timing_records,
                                                       prepared_override=prepared_paths[record["prepared"]])
                    if (not isinstance(summary, dict) or summary.get("status") != "audited" or
                        type(summary.get("diagnostic_samples")) is not int or summary["diagnostic_samples"] <= 0 or
                        type(summary.get("matched_timing_samples")) is not int or summary["matched_timing_samples"] <= 0 or
                        summary.get("timing_values_compared") is not False):
                        raise AuditError("diagnostic/timing crosscheck did not confirm logical evidence")
                    result["crosschecks"].append(summary)
            for record in index["failed_runs"]:
                verify_failed(below(extracted, record["path"]), record)
                result["failed_runs"] += 1
            if check_worktree:
                result["worktree_files"] = verify_worktree(Path(worktree or Path.cwd()), validation_dir,
                                                          baseline_headers(bundle, extracted, index))
            # Ensure no imported auditor mutated the frozen prepared inputs.
            for path in prepared_paths.values():
                verify_prepared(path)
    finally:
        sys.dont_write_bytecode = prior_bytecode
        if prior_runner_alias is absent_module:
            sys.modules.pop("run_vm_allocator", None)
        else:
            sys.modules["run_vm_allocator"] = prior_runner_alias
    verify_directory_manifest(bundle)
    if validation_dir:
        verify_directory_manifest(validation_dir)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--validation-dir", type=Path)
    parser.add_argument("--check-worktree", action="store_true")
    parser.add_argument("--worktree", type=Path, default=Path.cwd())
    parser.add_argument("--maximum-extract-bytes", type=int, default=8 * 1024**3)
    args = parser.parse_args()
    if args.maximum_extract_bytes <= 0:
        parser.error("--maximum-extract-bytes must be positive")
    print(json.dumps(audit(args.bundle, args.validation_dir, args.check_worktree,
                           args.worktree, args.maximum_extract_bytes), indent=2, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"allocator evidence audit failed: {error}", file=sys.stderr)
        sys.exit(1)
