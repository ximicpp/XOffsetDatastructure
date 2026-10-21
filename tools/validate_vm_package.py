#!/usr/bin/env python3
"""Build, relocate and consume a standalone XOffset installation; no downloads.

The full suite needs the explicit P2996 compiler and matching libc++. The storage
suite also supports the platform's default generator/compiler (including MSVC).
Evidence is written to a fresh --out directory, with command logs and hashes.
"""
from __future__ import annotations
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET


DEPENDENCY_ROOTS = ["external/typelayout/include"] + [
    "external/boost/libs/" + name + "/include" for name in
    ("container", "core", "config", "throw_exception", "static_assert",
     "container_hash", "type_traits", "winapi", "intrusive", "predef")]
DEPENDENCY_LICENSES = ["external/boost/LICENSE_1_0.txt", "external/typelayout/LICENSE",
                       "external/typelayout/LICENSE_1_0.txt"]
# Source path -> installed name under share/XOffsetDatastructure.
RELEASE_FILES = {"XOFFSET_VERSION": "VERSION", "CHANGELOG.md": "CHANGELOG.md",
                 "docs/ALPHA_COMPATIBILITY.md": "ALPHA_COMPATIBILITY.md"}
VERSION_PATTERN = re.compile(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)-(alpha|beta|rc)\.(0|[1-9][0-9]*)")


def read_version(source: Path) -> dict[str, str]:
    full = (source / "XOFFSET_VERSION").read_text(encoding="utf-8").strip()
    match = VERSION_PATTERN.fullmatch(full)
    if not match:
        raise ValueError("XOFFSET_VERSION must contain one prerelease identifier without leading zeros")
    return {"full": full, "numeric": ".".join(match.group(1, 2, 3)),
            "prerelease": ".".join(match.group(4, 5))}


def version_config_assertions(version: dict[str, str]) -> dict[str, str]:
    return {"XOffsetDatastructure_VERSION": '"' + version["full"] + '"',
            "XOffsetDatastructure_VERSION_NUMERIC": '"' + version["numeric"] + '"',
            "XOffsetDatastructure_VERSION_PRERELEASE": '"' + version["prerelease"] + '"',
            "XOffsetDatastructure_IS_PRERELEASE": "TRUE",
            "XOffsetDatastructure_VERSION_COMPATIBILITY": '"exact-full-version-only"'}


def version_probe_source(version: dict[str, str]) -> str:
    """Configure-only checks; numeric requests fail before dependency loading."""
    checks = "\n".join('if(NOT "${' + name + '}" STREQUAL ' + value + ')\n'
                       '  message(FATAL_ERROR "XOffset version metadata mismatch: ' + name + '")\nendif()'
                       for name, value in version_config_assertions(version).items())
    return ("cmake_minimum_required(VERSION 3.13)\n"
            "if(XOFFSET_VERSION_REQUEST)\n"
            "  project(XOffsetVersionRequest LANGUAGES NONE)\n"
            "else()\n"
            "  project(XOffsetVersionQuery LANGUAGES CXX)\n"
            "endif()\n"
            "find_package(XOffsetDatastructure ${XOFFSET_VERSION_REQUEST} CONFIG REQUIRED COMPONENTS ${XOFFSET_COMPONENT})\n"
            + checks + '\nmessage(STATUS "XOFFSET_FULL_VERSION=' + version["full"] + '")\n')


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def manifest(root: Path) -> dict[str, str]:
    return {p.relative_to(root).as_posix(): digest(p)
            for p in sorted(root.rglob("*")) if p.is_file()}


FULL_OWN_HEADERS = ('xoffsetdatastructure.hpp', 'receive_future.hpp', 'receive_raw.hpp', 'receive_raw_fwd.hpp', 'validation_scratch.hpp', 'xoffset_core_lifetime.hpp', 'xoffset_snapshot_index.hpp', 'xoffset_snapshot_index_budget.hpp', 'xoffset_validation_scratch.hpp', 'xoffset_verified_paths.hpp', 'xoffset_verified_receive.hpp')


def audit_install(prefix: Path, source: Path, suite: str, forbidden: list[Path]) -> dict:
    """Check the exact distributed header/license surface and exported interfaces."""
    version = read_version(source)
    expected = {"include/xoffset_virtual_memory.hpp": digest(source / "xoffset_virtual_memory.hpp"),
                "share/XOffsetDatastructure/licenses/LICENSE": digest(source / "LICENSE")}
    for path, installed in RELEASE_FILES.items():
        expected["share/XOffsetDatastructure/" + installed] = digest(source / path)
    config_dir = "lib/cmake/XOffsetDatastructure/"
    config = prefix / config_dir / "XOffsetDatastructureConfig.cmake"
    configured = source.parent / "producer-build/XOffsetDatastructureConfig.cmake"
    expected[config_dir + configured.name] = digest(configured)
    template = source / "cmake/XOffsetDatastructureConfigVersion.cmake.in"
    version_text = template.read_text(encoding="utf-8").replace("@XOFFSET_VERSION_FULL@", version["full"])
    expected[config_dir + "XOffsetDatastructureConfigVersion.cmake"] = hashlib.sha256(version_text.encode("utf-8")).hexdigest()
    if suite == "full":
        for own_header in FULL_OWN_HEADERS:
            expected["include/" + own_header] = digest(source / own_header)
        for tree in DEPENDENCY_ROOTS:
            for path, checksum in manifest(source / tree).items():
                key = "include/xoffset-deps/" + path
                if key in expected and expected[key] != checksum:
                    raise ValueError("Conflicting dependency header: " + path)
                expected[key] = checksum
        for path in DEPENDENCY_LICENSES:
            name = path.removeprefix("external/")
            expected["share/XOffsetDatastructure/licenses/" + name] = digest(source / path)
    actual = manifest(prefix)
    for path, checksum in expected.items():
        if actual.get(path) != checksum:
            raise ValueError("Missing or changed installed file: " + path)
    allowed_extras = {"lib/cmake/XOffsetDatastructure/XOffsetTargets.cmake"}
    if suite == "full":
        allowed_extras.add("share/XOffsetDatastructure/xoffset-third-party-manifest.txt")
        metadata = prefix / "share/XOffsetDatastructure/xoffset-third-party-manifest.txt"
        if metadata.read_bytes() != (source.parent / "producer-build/xoffset-third-party-manifest.txt").read_bytes():
            raise ValueError("Dependency manifest does not match configured inputs")
    if set(actual) != set(expected) | allowed_extras:
        raise ValueError("Unexpected or missing package files: " + str(sorted(set(actual) ^ (set(expected) | allowed_extras))))
    text = config.read_text(encoding="utf-8")
    for name, value in version_config_assertions(version).items():
        declarations = re.findall(r"^set\(" + re.escape(name) + r"\s+([^\n]+)\)$", text, re.MULTILINE)
        if declarations != [value]:
            raise ValueError("Package version metadata differs from VERSION: " + name)
    for config in (prefix / "lib/cmake/XOffsetDatastructure").glob("*.cmake"):
        text = config.read_text(encoding="utf-8")
        for path in forbidden:
            if str(path) in text or path.as_posix() in text:
                raise ValueError("Export leaks a build/source/toolchain path: " + str(path))
        # Absolute Unix/Windows paths are forbidden in interface payloads. The
        # standard generated prefix calculation itself uses relative references.
        for payload in re.findall(r'INTERFACE_[A-Z_]+\s+"([^"]*)"', text):
            for item in payload.split(";"):
                if item.startswith("/") or re.match(r"^[A-Za-z]:[/\\]", item):
                    raise ValueError("Absolute exported interface: " + item)
    return {"version": version, "files": actual, "file_count": len(actual), "dependency_header_count":
            sum(path.startswith("include/xoffset-deps/") for path in actual)}


def check_ctest_xml(path: Path) -> int:
    root = ET.parse(path).getroot()
    suites = [root] if root.tag == "testsuite" else list(root.iter("testsuite"))
    cases = list(root.iter("testcase"))
    if not cases or any(int(suite.get(key, "0")) for suite in suites for key in ("failures", "errors", "skipped", "disabled")):
        raise ValueError("CTest reported empty, skipped or failed tests")
    if any(case.get("status", "run") not in ("run", "passed") for case in cases):
        raise ValueError("CTest test case was not run")
    if any(case.find("failure") is not None or case.find("error") is not None or case.find("skipped") is not None for case in cases):
        raise ValueError("CTest test case did not pass")
    expected = {"consumer_transfer", "consumer_pool_handle_before_owner", "consumer_static_destruction"}
    if {case.get("name") for case in cases} != expected or len(cases) != len(expected):
        raise ValueError("External consumer test inventory mismatch")
    return len(cases)


def require_unchanged(root: Path, expected: dict[str, str], description: str):
    if manifest(root) != expected:
        raise ValueError(description + " changed during validation")


def validate_output_location(source: Path, out: Path):
    for directory in ("cmake", "tests", "tools", "examples", "include", "external"):
        if out == source / directory or source / directory in out.parents:
            raise ValueError("--out must be outside copied source trees: " + directory)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--suite", choices=("storage", "full"), required=True)
    parser.add_argument("--compiler")
    parser.add_argument("--sdk", type=Path)
    parser.add_argument("--library-dir", type=Path)
    parser.add_argument("--generator")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args(argv)
    source = args.source_dir.resolve(strict=True)
    out = args.out.resolve()
    validate_output_location(source, out)
    out.mkdir(parents=True, exist_ok=False)
    record = {"status": "running", "suite": args.suite, "commands": [],
              "started": datetime.datetime.now(datetime.timezone.utc).isoformat()}
    (out / "result.json").write_text(json.dumps(record, indent=2) + "\n")
    env = os.environ.copy()
    # Dependency headers must come from the package, not ambient search paths.
    for name in ("CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH"):
        env.pop(name, None)

    def run(name, command, expected_failure=False):
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                env=env, check=False)
        (out / (name + ".log")).write_text(json.dumps(command) + "\n" + result.stdout)
        record["commands"].append({"name": name, "argv": command, "returncode": result.returncode})
        if (result.returncode != 0) != expected_failure:
            raise RuntimeError(name + " returned " + str(result.returncode) + "; see its log")
        print(name + ": passed", flush=True)
        return result.stdout

    def configure(name, src, build, extra=(), expected_failure=False):
        command = ["cmake", "-S", str(src), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"]
        if args.generator:
            command += ["-G", args.generator]
        if args.compiler:
            command += ["-DCMAKE_CXX_COMPILER=" + args.compiler]
        if args.sdk:
            command += ["-DCMAKE_OSX_SYSROOT=" + str(args.sdk.resolve(strict=True))]
        if args.library_dir:
            # Caller toolchain configuration belongs here, never in the export.
            command += ["-DCMAKE_EXE_LINKER_FLAGS=-L\"" + library + "\" -Wl,-rpath,\"" + library + "\""]
        return run(name, command + list(extra), expected_failure)

    try:
        if args.library_dir:
            library = str(args.library_dir.resolve(strict=True))
            if not Path(library).is_dir():
                raise ValueError("--library-dir must be a directory")
        if args.jobs < 1:
            raise ValueError("--jobs must be positive")
        snapshot = out / "producer-source"
        snapshot.mkdir()
        for item in source.iterdir():
            if item.is_file() and not item.name.startswith("."):
                shutil.copy2(item, snapshot / item.name)
        for directory in ("cmake", "tests", "tools", "examples", "include"):
            if (source / directory).is_dir():
                shutil.copytree(source / directory, snapshot / directory,
                                ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
        # Only the required release document belongs in this source closure;
        # historical research/measurement archives are not package inputs.
        release_doc = snapshot / "docs/ALPHA_COMPATIBILITY.md"
        release_doc.parent.mkdir(exist_ok=True)
        shutil.copy2(source / "docs/ALPHA_COMPATIBILITY.md", release_doc)
        record["version"] = read_version(snapshot)
        if args.suite == "full":
            for directory in DEPENDENCY_ROOTS:
                shutil.copytree(source / directory, snapshot / directory)
            for filename in DEPENDENCY_LICENSES:
                target = snapshot / filename
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source / filename, target)
        record["source_files"] = manifest(snapshot)
        (out / "source-manifest.json").write_text(json.dumps(record["source_files"], indent=2) + "\n")
        producer = out / "producer-build"
        prefix = out / "original-prefix"
        configure("producer-configure", snapshot, producer, [
            "-DXOFFSET_STORAGE_ONLY=" + ("ON" if args.suite == "storage" else "OFF"),
            "-DXOFFSET_BUILD_TESTS=OFF", "-DXOFFSET_BUILD_TOOLS=OFF", "-DXOFFSET_BUILD_EXAMPLES=OFF",
            "-DXOFFSET_INSTALL=ON", "-DCMAKE_INSTALL_PREFIX=" + str(prefix),
            "-DCMAKE_INSTALL_LIBDIR=lib", "-DCMAKE_INSTALL_INCLUDEDIR=include", "-DCMAKE_INSTALL_DATADIR=share"])
        run("producer-build", ["cmake", "--build", str(producer), "--config", "Release", "--parallel", str(args.jobs)])
        run("producer-install", ["cmake", "--install", str(producer), "--config", "Release"])
        forbidden = [source, snapshot, producer, prefix]
        if args.compiler and ("/" in args.compiler or "\\" in args.compiler):
            forbidden.append(Path(args.compiler).absolute().parent.parent)
        if args.sdk:
            forbidden.append(args.sdk.resolve())
        if args.library_dir:
            forbidden.append(args.library_dir.resolve())
        record["package_audit"] = audit_install(prefix, snapshot, args.suite, forbidden)
        relocated = out / "relocated-prefix"
        prefix.rename(relocated)
        consumer = out / "consumer-source"
        shutil.copytree(snapshot / "tests/package_consumer", consumer)
        hidden = out / "unavailable-producer-source"
        snapshot.rename(hidden)
        external = out / "consumer-build"
        configure("consumer-configure", consumer, external, [
            "-DXOFFSET_CONSUMER_COMPONENT=" + args.suite,
            "-DXOffsetDatastructure_DIR=" + str(relocated / "lib/cmake/XOffsetDatastructure"),
            "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF", "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF"])
        run("consumer-build", ["cmake", "--build", str(external), "--config", "Release", "--parallel", str(args.jobs)])
        run("consumer-test", ["ctest", "--test-dir", str(external), "-C", "Release", "--output-on-failure",
                              "--output-junit", str(out / "consumer-tests.xml")])
        record["installed_consumer_tests"] = check_ctest_xml(out / "consumer-tests.xml")
        version_source = out / "version-consumer-source"
        version_source.mkdir()
        (version_source / "CMakeLists.txt").write_text(version_probe_source(record["version"]), encoding="utf-8")
        version_flags = ["-DXOFFSET_COMPONENT=" + args.suite,
                         "-DXOffsetDatastructure_DIR=" + str(relocated / "lib/cmake/XOffsetDatastructure"),
                         "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF", "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF"]
        configure("version-unversioned-accepted", version_source, out / "version-unversioned-build", version_flags)
        numeric = record["version"]["numeric"]
        record["version_request_rejections"] = []
        for name, request in (("same-numeric", numeric), ("different-numeric", "999.0.0"),
                              ("exact-numeric", numeric + ";EXACT"), ("range", numeric + "...<999.0.0")):
            output = configure("version-" + name + "-rejected", version_source, out / ("version-" + name + "-build"),
                               version_flags + ["-DXOFFSET_VERSION_REQUEST=" + request], expected_failure=True)
            if "not accepted" not in output or record["version"]["full"] not in output:
                raise ValueError("Numeric version rejection had an unrelated cause: " + name)
            record["version_request_rejections"].append(request)
        if args.suite == "storage":
            output = configure("missing-full-rejected", consumer, out / "missing-full-build", [
                "-DXOFFSET_CONSUMER_COMPONENT=full",
                "-DXOffsetDatastructure_DIR=" + str(relocated / "lib/cmake/XOffsetDatastructure")], expected_failure=True)
            if "storage-only installation" not in output:
                raise ValueError("Full component rejection had an unrelated cause")
        hidden.rename(snapshot)
        subproject = out / "subproject-build"
        configure("subproject-configure", consumer, subproject, [
            "-DXOFFSET_SOURCE=" + str(snapshot), "-DXOFFSET_CONSUMER_COMPONENT=" + args.suite,
            "-DXOFFSET_STORAGE_ONLY=" + ("ON" if args.suite == "storage" else "OFF")])
        run("subproject-build", ["cmake", "--build", str(subproject), "--config", "Release", "--parallel", str(args.jobs)])
        run("subproject-test", ["ctest", "--test-dir", str(subproject), "-C", "Release", "--output-on-failure",
                                "--output-junit", str(out / "subproject-tests.xml")])
        record["subproject_consumer_tests"] = check_ctest_xml(out / "subproject-tests.xml")
        require_unchanged(snapshot, record["source_files"], "Frozen source")
        require_unchanged(relocated, record["package_audit"]["files"], "Relocated package")
        record["status"] = "passed"
    except BaseException as exc:
        record["status"] = "interrupted" if isinstance(exc, KeyboardInterrupt) else "failed"
        record["error"] = str(exc)
        print(record["error"], file=sys.stderr)
    record["finished"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    (out / "result.json").write_text(json.dumps(record, indent=2) + "\n")
    return 0 if record["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
