#!/usr/bin/env python3
"""Create, audit, or extract the local source distribution of the XOFFSET_VERSION release.

No Git command, network request, compiler, or package installer is executed.
The source archive includes the exact dependency headers/licenses that CMake
locks. A caller still supplies its compiler, SDK, CMake and standard library.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import re
import stat
import tarfile
import zlib

VERSION_PATTERN = re.compile(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-(alpha|beta|rc)\.(0|[1-9][0-9]*))?")


def read_version(source):
    """The release identity is the XOFFSET_VERSION file of the tree this script belongs to."""
    full = (source / "XOFFSET_VERSION").read_text(encoding="utf-8").strip()
    match = VERSION_PATTERN.fullmatch(full)
    if not match:
        raise ValueError("XOFFSET_VERSION must be MAJOR.MINOR.PATCH with at most one prerelease identifier, without leading zeros")
    return full, ".".join(match.group(1, 2, 3)), ".".join(match.group(4, 5)) if match.group(4) else ""


VERSION, VERSION_NUMERIC, VERSION_PRERELEASE = read_version(Path(__file__).resolve().parents[1])
PACKAGE_NAME = "xoffsetdatastructure-" + VERSION
MANIFEST_NAME = "SOURCE_MANIFEST.json"
SCHEMA = "xoffset-source/v1"
INSTALL_SCHEMA = "xoffset-install/v1"
INSTALL_MANIFEST_NAME = "INSTALL_MANIFEST.json"
MAX_MEMBERS = 10000
MAX_FILE_BYTES = 16 * 1024 * 1024
MAX_TOTAL_BYTES = 128 * 1024 * 1024
MAX_ARCHIVE_BYTES = 64 * 1024 * 1024
MAX_TAR_BYTES = MAX_TOTAL_BYTES + MAX_MEMBERS * 1024
BOOST = ("container", "core", "config", "throw_exception", "static_assert",
         "container_hash", "type_traits", "winapi", "intrusive", "predef")
DEPENDENCY_TREES = ("external/typelayout/include",) + tuple(
    "external/boost/libs/" + name + "/include" for name in BOOST)
DEPENDENCY_LICENSES = ("external/boost/LICENSE_1_0.txt", "external/typelayout/LICENSE",
                       "external/typelayout/LICENSE_1_0.txt")
EXPECTED_REVISIONS = {"boost": "da041154c6bac1a4aa98254a7d6819059e8ac0b0",
                      "typelayout": "fe295929d0a5bfa7809fb737a6eb3db279599346"}
FULL_OWN_HEADERS = ('xoffsetdatastructure.hpp', 'receive_future.hpp', 'receive_raw.hpp', 'receive_raw_fwd.hpp', 'validation_scratch.hpp', 'xoffset_admission.hpp', 'xoffset_core_lifetime.hpp', 'xoffset_fixed_string.hpp', 'xoffset_schema.hpp', 'xoffset_schema_describe.hpp', 'xoffset_schema_reflect.hpp', 'xoffset_snapshot_index.hpp', 'xoffset_snapshot_index_budget.hpp', 'xoffset_validation_scratch.hpp', 'xoffset_verified_paths.hpp', 'xoffset_verified_receive.hpp', 'xoffset_schema_names.hpp', 'xoffset_handshake.hpp')
OPTIONAL_RUNTIME_SOURCES = ('validation_scratch.cpp', 'snapshot_index_runtime.cpp')
ROOT_FILES = ("CMakeLists.txt", "README.md", "XOFFSET_VERSION", "CHANGELOG.md", "LICENSE",
              "build.sh", "xoffsetdatastructure.hpp", "xoffset_virtual_memory.hpp", "xoffset_slot_storage.hpp",
              "Dockerfile", "docker-compose.yml", ".dockerignore")
DOC_FILES = ("docs/ALPHA_COMPATIBILITY.md", "docs/PACKAGING.md", "docs/VALIDATION_RUNNER.md")
TOOLCHAIN_FILES = ("toolchains/README.md", "toolchains/vm-validation.json")
TOOL_FILES = ("tools/CMakeLists.txt", "tools/bench_compare.cpp", "tools/export_signatures.cpp",
              "tools/check_compat.cpp", "tools/check_compat_baselines.hpp.in",
              "tools/check_signature_matrix.cpp", "tools/check_signature_regeneration.cmake",
              "tools/signature_catalog.hpp", "tools/signature_target_matrix.hpp",
              "tools/signature_type_list.hpp", "tools/signature_type_names.hpp",
              "tools/validate_vm.py", "tools/validate_vm_package.py", "tools/prepare_vm_cmake.py",
              "tools/prepare_vm_linux_current.py", "tools/check_vm_toolchain.py",
              "tools/prepare_release.py")
SIGNATURE_FILES = tuple("tools/sigs/" + name + ".sig.hpp" for name in (
    "arm64_android_clang", "arm64_ios_clang", "x86_64_linux_clang", "x86_64_windows_clang"))
TOOL_TESTS = ("tools/tests/test_validate_vm_package.py", "tools/tests/test_prepare_vm_cmake.py",
              "tools/tests/test_prepare_release.py")
COPIED_TREES = ("cmake", "tests", "examples", "include")
OPTIONAL_FILES = ("toolchains/build_tools.cmake",)
ROOT_FILES = ROOT_FILES + tuple(h for h in FULL_OWN_HEADERS if h not in ROOT_FILES) + OPTIONAL_RUNTIME_SOURCES
REQUIRED_FILES = ROOT_FILES + DOC_FILES + TOOLCHAIN_FILES + TOOL_FILES + SIGNATURE_FILES + TOOL_TESTS
FORBIDDEN_PARTS = {".git", ".svn", "build", "dist", "__pycache__", ".pytest_cache"}
GENERATED_HEADERS = {"check_compat_baselines.hpp"}
# Developer-only subdirectories behind options that default to OFF (XOFFSET_BUILD_DEMOS).
UNDISTRIBUTED_SUBDIRECTORIES = {"demo/mapstate"}
SCOPE = {
    "offline_build": "full/storage CMake and package validation use bundled dependency bytes; caller provides compatible compiler, SDK, CMake and standard library",
    "included_python_tests": list(TOOL_TESTS),
    "excluded": "Git metadata, build outputs, private toolchains/runtimes, experiments, historical research/evidence and fixture-dependent developer tests",
    "validation_limits": "validate_vm.py native full/storage are supported entry points; historical wire bundles and full developer-test discovery are outside this distribution",
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def json_bytes(value):
    return (json.dumps(value, indent=2, sort_keys=True, ensure_ascii=True) + "\n").encode("utf-8")


def parse_json(data):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate JSON key: " + key)
            result[key] = value
        return result
    return json.loads(data.decode("utf-8"), object_pairs_hook=unique)


def reparse(path):
    info = path.lstat()
    return stat.S_ISLNK(info.st_mode) or bool(getattr(info, "st_file_attributes", 0) & 0x400)


def no_links(path):
    """Reject links/junctions before resolving away their identity."""
    path = Path(os.path.abspath(path))
    for part in reversed((path,) + tuple(path.parents)):
        if (part.exists() or part.is_symlink()) and reparse(part):
            raise ValueError("symbolic link or reparse point is not allowed: " + str(part))
    return path


def regular_file(path):
    path = no_links(path)
    if not path.is_file() or not stat.S_ISREG(path.stat().st_mode):
        raise ValueError("required regular file unavailable: " + str(path))
    if path.stat().st_size > MAX_FILE_BYTES:
        raise ValueError("file exceeds distribution size limit: " + str(path))
    return path


def valid_name(name):
    if (not isinstance(name, str) or not name or any(c in name for c in '\\:<>"|?*') or
            any(ord(c) < 32 or ord(c) == 127 for c in name)):
        raise ValueError("invalid archive path: " + repr(name))
    parts = name.split("/")
    if any(part in ("", ".", "..") for part in parts) or PurePosixPath(name).is_absolute():
        raise ValueError("archive path escapes or is noncanonical: " + name)
    # Windows extraction must not create aliases/devices/alternate data streams.
    for part in parts:
        if part.endswith((" ", ".")) or part.split(".", 1)[0].upper() in {
                "CON", "PRN", "AUX", "NUL", *("COM" + str(i) for i in range(1, 10)),
                *("LPT" + str(i) for i in range(1, 10))}:
            raise ValueError("nonportable archive path: " + name)
    return name


def validate_path_set(names):
    """Preserve every file and implicit directory spelling on Windows too."""
    files = set(names)
    spelling = {}
    for name in sorted(files):
        valid_name(name)
        parts = name.split("/")
        for length in range(1, len(parts) + 1):
            prefix = "/".join(parts[:length])
            folded = prefix.casefold()
            if folded in spelling and spelling[folded] != prefix:
                raise ValueError("case-colliding file or directory path: " + prefix)
            spelling[folded] = prefix
            if length < len(parts) and prefix in files:
                raise ValueError("archive file is also a directory prefix: " + prefix)


def payload_name_allowed(name):
    valid_name(name)
    if any(part in FORBIDDEN_PARTS for part in name.split("/")) or name.endswith((".pyc", ".pyo")):
        return False
    if name in REQUIRED_FILES + OPTIONAL_FILES:
        return True
    if any(name.startswith(tree + "/") for tree in COPIED_TREES):
        return True
    return (name in DEPENDENCY_LICENSES or
            any(name.startswith(tree + "/") for tree in DEPENDENCY_TREES))


def normalized_mode(name):
    return 0o755 if name == "build.sh" or (name.startswith("tools/") and name.count("/") == 1 and name.endswith(".py")) else 0o644


def validate_new_output(out, source=None):
    out = no_links(out)
    if out.exists():
        raise ValueError("output already exists: " + str(out))
    if source:
        source = no_links(source)
        if out == source or out in source.parents:
            raise ValueError("output cannot equal or contain source")
        for name in COPIED_TREES + ("tools", "docs", "toolchains", "external"):
            tree = source / name
            if out == tree or tree in out.parents:
                raise ValueError("output is inside a copied source tree: " + name)
    return out


def dependency_lock(payload):
    path = "cmake/XOffsetDependencyLock.cmake"
    try:
        text = payload[path].decode("utf-8")
    except KeyError as exc:
        raise ValueError("dependency content lock is missing") from exc
    trees = {}
    licenses = {}
    for name, expected in re.findall(r'"([^"|]+)\|([0-9a-f]{64})"', text):
        target = trees if name in DEPENDENCY_TREES else licenses if name in DEPENDENCY_LICENSES else None
        if target is None or name in target:
            raise ValueError("unexpected or duplicate dependency-lock path: " + name)
        target[name] = expected
    if set(trees) != set(DEPENDENCY_TREES) or set(licenses) != set(DEPENDENCY_LICENSES):
        raise ValueError("dependency lock does not declare the exact 11 trees and 3 licenses")
    for tree, expected in trees.items():
        members = sorted(name for name in payload if name.startswith(tree + "/"))
        if not members:
            raise ValueError("dependency include tree is empty: " + tree)
        records = "".join(name[len(tree) + 1:] + " " + sha(payload[name]) + "\n" for name in members)
        if sha(records.encode("utf-8")) != expected:
            raise ValueError("dependency content mismatch: " + tree)
    for name, expected in licenses.items():
        if name not in payload or sha(payload[name]) != expected:
            raise ValueError("dependency license mismatch: " + name)
    revisions = {}
    for key in ("BOOST", "TYPELAYOUT"):
        matches = re.findall(r"set\(XOFFSET_" + key + r"_REVISION\s+([0-9a-f]{40})\)", text)
        if len(matches) != 1:
            raise ValueError("dependency revision unavailable: " + key)
        revisions[key.lower()] = matches[0]
    if revisions != EXPECTED_REVISIONS:
        raise ValueError("dependency revisions differ from the release inputs")
    return {"lock_sha256": sha(payload[path]), "revisions": revisions,
            "include_trees": trees, "licenses": licenses}


def check_cpp_and_cmake_closure(payload):
    """Check literal local includes and literal CMake source/subdirectory inputs."""
    include_roots = ("", "tools") + DEPENDENCY_TREES
    for name, data in payload.items():
        if name.startswith("external/") or not name.endswith((".hpp", ".h", ".cpp", ".cmake", "CMakeLists.txt")):
            continue
        text = data.decode("utf-8", errors="replace")
        if name.endswith((".hpp", ".h", ".cpp")):
            for include in re.findall(r'^\s*#\s*include\s*"([^"\n]+)"', text, re.MULTILINE):
                options = [posixpath.normpath(posixpath.join(posixpath.dirname(name), include))]
                options += [posixpath.normpath(posixpath.join(root, include)) for root in include_roots]
                if include not in GENERATED_HEADERS and not any(option in payload for option in options):
                    raise ValueError("local C++ include missing from source closure: " + name + " -> " + include)
        if name.endswith((".cmake", "CMakeLists.txt")):
            parent = posixpath.dirname(name)
            for directory in re.findall(r"add_subdirectory\(\s*([A-Za-z0-9_./-]+)\s*\)", text):
                if posixpath.normpath(posixpath.join(parent, directory)) in UNDISTRIBUTED_SUBDIRECTORIES:
                    continue
                if posixpath.normpath(posixpath.join(parent, directory, "CMakeLists.txt")) not in payload:
                    raise ValueError("CMake subdirectory missing: " + name + " -> " + directory)
            for source in re.findall(r"add_executable\(\s*[^\s)]+\s+([A-Za-z0-9_./-]+\.(?:cpp|cc|cxx))", text):
                if posixpath.normpath(posixpath.join(parent, source)) not in payload:
                    raise ValueError("CMake source missing: " + name + " -> " + source)


def validate_payload(payload):
    if len(payload) > MAX_MEMBERS or sum(len(data) for data in payload.values()) > MAX_TOTAL_BYTES:
        raise ValueError("source payload exceeds distribution limits")
    missing = set(REQUIRED_FILES) - set(payload)
    if missing:
        raise ValueError("source closure is missing: " + ", ".join(sorted(missing)))
    validate_path_set(payload)
    for name, data in payload.items():
        if not payload_name_allowed(name):
            raise ValueError("file is outside source distribution closure: " + name)
        if len(data) > MAX_FILE_BYTES:
            raise ValueError("source member exceeds size limit: " + name)
    if payload["XOFFSET_VERSION"].decode("utf-8").strip() != VERSION:
        raise ValueError("XOFFSET_VERSION differs from " + VERSION)
    check_cpp_and_cmake_closure(payload)
    return dependency_lock(payload)


def collect_source(source):
    source = no_links(source)
    if not source.is_dir():
        raise ValueError("source directory unavailable: " + str(source))
    payload = {}

    def add(name):
        valid_name(name)
        payload[name] = regular_file(source / name).read_bytes()

    def walk(relative):
        directory = no_links(source / relative)
        if not directory.is_dir():
            raise ValueError("source tree unavailable: " + relative)
        for child in sorted(directory.iterdir(), key=lambda path: path.name):
            name = relative + "/" + child.name
            no_links(child)
            if child.name in FORBIDDEN_PARTS or child.name == ".DS_Store" or child.name.endswith((".pyc", ".pyo")):
                continue
            if child.is_dir():
                walk(name)
            else:
                add(name)

    for name in REQUIRED_FILES + DEPENDENCY_LICENSES:
        add(name)
    for name in OPTIONAL_FILES:
        if (source / name).exists() or (source / name).is_symlink():
            add(name)
    for tree in COPIED_TREES + DEPENDENCY_TREES:
        if tree == "include" and not (source / tree).exists() and not (source / tree).is_symlink():
            continue
        walk(tree)
    lock = validate_payload(payload)
    return source, payload, lock


def source_manifest(payload, lock):
    return {"schema": SCHEMA, "version": VERSION, "archive_root": PACKAGE_NAME,
            "scope": SCOPE, "dependency_lock": lock,
            "files": {name: {"bytes": len(data), "sha256": sha(data), "mode": normalized_mode(name)}
                      for name, data in sorted(payload.items())}}


def canonical_tar(payload, manifest_data, archive_root=PACKAGE_NAME, manifest_name=MANIFEST_NAME):
    buffer = io.BytesIO()
    members = dict(payload)
    members[manifest_name] = manifest_data
    with tarfile.open(fileobj=buffer, mode="w", format=tarfile.GNU_FORMAT) as bundle:
        for name, data in sorted(members.items()):
            member = tarfile.TarInfo(archive_root + "/" + name)
            member.size = len(data)
            member.mode = normalized_mode(name)
            member.mtime = member.uid = member.gid = 0
            member.uname = member.gname = ""
            bundle.addfile(member, io.BytesIO(data))
    return buffer.getvalue()


def canonical_gzip(tar_data):
    buffer = io.BytesIO()
    with gzip.GzipFile(fileobj=buffer, mode="wb", filename="", mtime=0, compresslevel=9) as stream:
        stream.write(tar_data)
    return buffer.getvalue()


def write_artifacts(payload, manifest_data, out, archive_root=PACKAGE_NAME, manifest_name=MANIFEST_NAME):
    out = validate_new_output(out)
    tar_data = canonical_tar(payload, manifest_data, archive_root, manifest_name)
    archive_data = canonical_gzip(tar_data)
    if len(tar_data) > MAX_TAR_BYTES or len(archive_data) > MAX_ARCHIVE_BYTES:
        raise ValueError("generated archive exceeds distribution size limits")
    out.mkdir(parents=True, exist_ok=False)
    archive_name = archive_root + ".tar.gz"
    sidecar_name = archive_root + ".manifest.json"
    for name, data in ((archive_name, archive_data), (sidecar_name, manifest_data)):
        with (out / name).open("xb") as stream:
            stream.write(data)
    checksums = sha(archive_data) + "  " + archive_name + "\n" + sha(manifest_data) + "  " + sidecar_name + "\n"
    with (out / "SHA256SUMS").open("x", encoding="ascii", newline="\n") as stream:
        stream.write(checksums)
    audit(out / archive_name, out / sidecar_name, out / "SHA256SUMS")
    return {"status": "created", "version": VERSION, "archive": str(out / archive_name),
            "archive_sha256": sha(archive_data), "manifest_sha256": sha(manifest_data),
            "file_count": len(payload), "payload_bytes": sum(map(len, payload.values()))}


def create(source, out):
    source = no_links(source)
    out = validate_new_output(out, source)
    source, payload, lock = collect_source(source)
    manifest_data = json_bytes(source_manifest(payload, lock))
    # Re-enumeration also catches additions/removals during packaging.
    if collect_source(source)[1] != payload:
        raise ValueError("source changed while packaging")
    return write_artifacts(payload, manifest_data, out)


def install_required(component):
    if component not in ("full", "storage"):
        raise ValueError("install component must be full or storage")
    base = {"include/xoffset_virtual_memory.hpp", "include/xoffset_slot_storage.hpp",
            "share/XOffsetDatastructure/licenses/LICENSE",
            "share/XOffsetDatastructure/VERSION", "share/XOffsetDatastructure/CHANGELOG.md",
            "share/XOffsetDatastructure/ALPHA_COMPATIBILITY.md"}
    base.update("lib/cmake/XOffsetDatastructure/" + name for name in (
        "XOffsetDatastructureConfig.cmake", "XOffsetDatastructureConfigVersion.cmake", "XOffsetTargets.cmake"))
    if component == "full":
        base.update("include/" + h for h in FULL_OWN_HEADERS)
        base.add("share/XOffsetDatastructure/xoffset-third-party-manifest.txt")
        base.update("share/XOffsetDatastructure/licenses/" + name.removeprefix("external/") for name in DEPENDENCY_LICENSES)
    return base


def installed_dependency_headers(payload):
    text = payload["share/XOffsetDatastructure/xoffset-third-party-manifest.txt"].decode("utf-8")
    if text.splitlines()[:2] != ["Boost " + EXPECTED_REVISIONS["boost"],
                               "TypeLayout " + EXPECTED_REVISIONS["typelayout"]]:
        raise ValueError("installed dependency revisions differ")
    headers = {}
    trees = {}
    tree = None
    license_rows = {}
    for line in text.splitlines():
        if not line or re.fullmatch(r"(?:Boost|TypeLayout) [0-9a-f]{40}", line):
            continue
        match = re.fullmatch(r"\[([^]]+)\] ([0-9a-f]{64})", line)
        if match:
            tree, expected = match.groups()
            if tree not in DEPENDENCY_TREES or tree in trees:
                raise ValueError("invalid installed dependency tree: " + tree)
            trees[tree] = {"expected": expected, "files": {}}
            continue
        match = re.fullmatch(r"(.+) ([0-9a-f]{64})", line)
        if not match:
            raise ValueError("invalid installed dependency manifest record")
        name, digest = match.groups()
        valid_name(name)
        if name in DEPENDENCY_LICENSES:
            if name in license_rows:
                raise ValueError("duplicate installed license record")
            license_rows[name] = digest
            installed = "share/XOffsetDatastructure/licenses/" + name.removeprefix("external/")
        else:
            if tree is None or name in trees[tree]["files"]:
                raise ValueError("missing tree or duplicate installed header")
            trees[tree]["files"][name] = digest
            installed = "include/xoffset-deps/" + name
            if installed in headers and headers[installed] != digest:
                raise ValueError("conflicting installed dependency header")
            headers[installed] = digest
        if installed not in payload or sha(payload[installed]) != digest:
            raise ValueError("installed dependency bytes differ: " + installed)
    if set(trees) != set(DEPENDENCY_TREES) or set(license_rows) != set(DEPENDENCY_LICENSES):
        raise ValueError("installed dependency manifest is incomplete")
    for name, details in trees.items():
        records = "".join(path + " " + digest + "\n" for path, digest in sorted(details["files"].items()))
        if not details["files"] or sha(records.encode()) != details["expected"]:
            raise ValueError("installed dependency tree hash differs: " + name)
    return headers


def validate_install_payload(payload, component):
    required = install_required(component)
    validate_path_set(payload)
    for name in payload:
        valid_name(name)
        if any(part in FORBIDDEN_PARTS for part in name.split("/")):
            raise ValueError("forbidden installed package member: " + name)
    if required - set(payload):
        raise ValueError("installed package is missing: " + ", ".join(sorted(required - set(payload))))
    if payload["share/XOffsetDatastructure/VERSION"].decode("utf-8").strip() != VERSION:
        raise ValueError("installed VERSION differs from " + VERSION)
    # Parse CMake semantics with universal newlines, while every manifest and
    # archive continues to bind the untouched platform-specific bytes.
    config = payload["lib/cmake/XOffsetDatastructure/XOffsetDatastructureConfig.cmake"].decode("utf-8").replace("\r\n", "\n")
    declarations = {
        "XOffsetDatastructure_VERSION": '"' + VERSION + '"',
        "XOffsetDatastructure_VERSION_NUMERIC": '"' + VERSION_NUMERIC + '"',
        "XOffsetDatastructure_VERSION_PRERELEASE": '"' + VERSION_PRERELEASE + '"',
        "XOffsetDatastructure_IS_PRERELEASE": "TRUE" if VERSION_PRERELEASE else "FALSE",
        "XOffsetDatastructure_VERSION_COMPATIBILITY": '"exact-full-version-only"',
        "XOffsetDatastructure_full_FOUND": "ON" if component == "full" else "OFF",
        "XOffsetDatastructure_storage_FOUND": "TRUE",
    }
    for name, expected in declarations.items():
        actual = re.findall(r"^set\(" + name + r"\s+([^\n]+)\)$", config, re.MULTILINE)
        if actual != [expected]:
            raise ValueError("installed component/version metadata differs: " + name)
    version_config = payload["lib/cmake/XOffsetDatastructure/XOffsetDatastructureConfigVersion.cmake"].decode("utf-8").replace("\r\n", "\n")
    if re.findall(r'^set\(PACKAGE_VERSION\s+([^\n]+)\)$', version_config, re.MULTILINE) != ['"' + VERSION + '"']:
        raise ValueError("installed ConfigVersion identity differs")
    targets = payload["lib/cmake/XOffsetDatastructure/XOffsetTargets.cmake"].decode("utf-8")
    expected_targets = {"XOffset::storage", "XOffset::xoffset"} if component == "full" else {"XOffset::storage"}
    actual_targets = re.findall(r"add_library\(([^\s)]+)\s+INTERFACE\s+IMPORTED\)", targets)
    if set(actual_targets) != expected_targets or len(actual_targets) != len(expected_targets):
        raise ValueError("installed component target inventory differs")
    expected_files = required | (set(installed_dependency_headers(payload)) if component == "full" else set())
    if set(payload) != expected_files:
        raise ValueError("unexpected installed package files: " + ", ".join(sorted(set(payload) - expected_files)))
    if len(payload) > MAX_MEMBERS or sum(map(len, payload.values())) > MAX_TOTAL_BYTES:
        raise ValueError("installed package exceeds size limits")


def install_manifest(payload, component):
    validate_install_payload(payload, component)
    return {"schema": INSTALL_SCHEMA, "version": VERSION, "component": component,
            "archive_root": PACKAGE_NAME + "-" + component,
            "scope": "header+CMake installation only; packaging verifies metadata/content inventory and does not replace producer/consumer relocation validation",
            "files": {name: {"bytes": len(data), "sha256": sha(data), "mode": normalized_mode(name)}
                      for name, data in sorted(payload.items())}}


def collect_install_prefix(prefix):
    payload = {}
    for directory, directories, files in os.walk(prefix, followlinks=False):
        for name in directories + files:
            no_links(Path(directory) / name)
        for name in files:
            path = regular_file(Path(directory) / name)
            payload[path.relative_to(prefix).as_posix()] = path.read_bytes()
    return payload


def create_install_package(prefix, component, out):
    """Package an already validated standard-layout full/storage prefix locally."""
    prefix = no_links(prefix)
    if not prefix.is_dir():
        raise ValueError("install prefix unavailable")
    out = validate_new_output(out)
    if out == prefix or prefix in out.parents or out in prefix.parents:
        raise ValueError("install archive output and prefix must be separate")
    payload = collect_install_prefix(prefix)
    manifest_data = json_bytes(install_manifest(payload, component))
    if collect_install_prefix(prefix) != payload:
        raise ValueError("install prefix changed while packaging")
    return write_artifacts(payload, manifest_data, out, PACKAGE_NAME + "-" + component, INSTALL_MANIFEST_NAME)


def read_archive(archive):
    archive = no_links(archive)
    if not archive.is_file() or archive.stat().st_size > MAX_ARCHIVE_BYTES:
        raise ValueError("archive unavailable or exceeds size limit")
    compressed = archive.read_bytes()
    if len(compressed) < 18 or compressed[:4] != b"\x1f\x8b\x08\x00" or compressed[4:8] != b"\0\0\0\0":
        raise ValueError("archive gzip metadata is not normalized")
    decoder = zlib.decompressobj(wbits=31)
    try:
        raw = decoder.decompress(compressed, MAX_TAR_BYTES + 1)
    except zlib.error as exc:
        raise ValueError("invalid gzip stream: " + str(exc)) from exc
    if len(raw) > MAX_TAR_BYTES or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise ValueError("truncated, oversized, concatenated or trailing gzip data")
    members = {}
    folded = set()
    archive_root = None
    try:
        with tarfile.open(fileobj=io.BytesIO(raw), mode="r:") as bundle:
            for member in bundle:
                valid_name(member.name)
                if not member.isfile() or member.name in members or member.name.casefold() in folded:
                    raise ValueError("duplicate, linked, special or non-regular archive member: " + member.name)
                candidate_root = member.name.split("/", 1)[0]
                if candidate_root not in (PACKAGE_NAME, PACKAGE_NAME + "-full", PACKAGE_NAME + "-storage"):
                    raise ValueError("unexpected archive root: " + member.name)
                if archive_root is None:
                    archive_root = candidate_root
                if not member.name.startswith(archive_root + "/"):
                    raise ValueError("mixed archive roots")
                if member.size < 0 or member.size > MAX_FILE_BYTES or len(members) >= MAX_MEMBERS + 1:
                    raise ValueError("archive member or count exceeds size limit")
                if (member.uid, member.gid, member.mtime, member.uname, member.gname) != (0, 0, 0, "", ""):
                    raise ValueError("archive member metadata is not normalized: " + member.name)
                name = member.name[len(archive_root) + 1:]
                if member.mode != normalized_mode(name):
                    raise ValueError("archive member mode is not normalized: " + member.name)
                members[member.name] = bundle.extractfile(member).read()
                folded.add(member.name.casefold())
    except tarfile.TarError as exc:
        raise ValueError("invalid tar stream: " + str(exc)) from exc
    validate_path_set(members)
    manifest_name = MANIFEST_NAME if archive_root == PACKAGE_NAME else INSTALL_MANIFEST_NAME
    manifest_key = str(archive_root) + "/" + manifest_name
    if manifest_key not in members:
        raise ValueError("archive source manifest is missing")
    manifest_data = members.pop(manifest_key)
    payload = {name[len(archive_root) + 1:]: data for name, data in members.items()}
    if archive_root == PACKAGE_NAME:
        actual_manifest = source_manifest(payload, validate_payload(payload))
    else:
        actual_manifest = install_manifest(payload, archive_root.removeprefix(PACKAGE_NAME + "-"))
    if parse_json(manifest_data) != actual_manifest or manifest_data != json_bytes(actual_manifest):
        raise ValueError("archive members, lengths or hashes differ from manifest")
    if raw != canonical_tar(payload, manifest_data, archive_root, manifest_name):
        raise ValueError("archive has noncanonical headers, ordering, padding or hidden members")
    return compressed, payload, manifest_data


def _audit_loaded(loaded, manifest=None, checksums=None):
    """Bind external pins and reported identity to the exact loaded payload."""
    compressed, payload, manifest_data = loaded
    metadata = parse_json(manifest_data)
    archive_root = metadata["archive_root"]
    if manifest is not None and regular_file(manifest).read_bytes() != manifest_data:
        raise ValueError("external manifest differs from archive manifest")
    if checksums is not None:
        expected = (sha(compressed) + "  " + archive_root + ".tar.gz\n" +
                    sha(manifest_data) + "  " + archive_root + ".manifest.json\n").encode("ascii")
        if regular_file(checksums).read_bytes() != expected:
            raise ValueError("SHA256SUMS differs from archive/manifest identities")
    return {"status": "passed", "version": VERSION, "archive_sha256": sha(compressed),
            "manifest_sha256": sha(manifest_data), "file_count": len(payload),
            "payload_bytes": sum(map(len, payload.values())), "archive_root": archive_root,
            "kind": "source" if metadata["schema"] == SCHEMA else "install",
            "component": metadata.get("component")}


def audit(archive, manifest=None, checksums=None):
    return _audit_loaded(read_archive(archive), manifest, checksums)


def extract(archive, out, manifest=None, checksums=None):
    out = validate_new_output(out)
    loaded = read_archive(archive)
    record = _audit_loaded(loaded, manifest, checksums)
    _, archive_payload, manifest_data = loaded
    payload = dict(archive_payload)
    payload[MANIFEST_NAME if record["kind"] == "source" else INSTALL_MANIFEST_NAME] = manifest_data
    out.mkdir(parents=True, exist_ok=False)
    root = out / record["archive_root"]
    for name, data in sorted(payload.items()):
        valid_name(name)
        destination = root.joinpath(*PurePosixPath(name).parts)
        destination.parent.mkdir(parents=True, exist_ok=True)
        no_links(destination.parent)
        with destination.open("xb") as stream:
            stream.write(data)
        destination.chmod(normalized_mode(name))
        os.utime(destination, (0, 0))
    record.update(status="extracted", source_dir=str(root))
    return record


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    create_parser = sub.add_parser("create", help="snapshot current working files and locked dependency bytes")
    create_parser.add_argument("--source-dir", type=Path, required=True)
    create_parser.add_argument("--out", type=Path, required=True)
    prefix_parser = sub.add_parser("pack-prefix", help="archive an already validated standard-layout installation")
    prefix_parser.add_argument("--prefix", type=Path, required=True)
    prefix_parser.add_argument("--component", choices=("full", "storage"), required=True)
    prefix_parser.add_argument("--out", type=Path, required=True)
    for command in ("audit", "extract"):
        entry = sub.add_parser(command, help="offline exact-member archive " + command)
        entry.add_argument("--archive", type=Path, required=True)
        entry.add_argument("--manifest", type=Path)
        entry.add_argument("--checksums", type=Path)
        if command == "extract":
            entry.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "create":
            result = create(args.source_dir, args.out)
        elif args.command == "pack-prefix":
            result = create_install_package(args.prefix, args.component, args.out)
        elif args.command == "audit":
            result = audit(args.archive, args.manifest, args.checksums)
        else:
            result = extract(args.archive, args.out, args.manifest, args.checksums)
    except (OSError, ValueError) as exc:
        print(json.dumps({"status": "failed", "reason": str(exc)}, sort_keys=True))
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
