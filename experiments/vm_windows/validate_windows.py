#!/usr/bin/env python3
"""Run the prepared storage-only suite on a native Windows MSVC developer shell."""
from __future__ import annotations

import argparse
import datetime
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
from bundle_support import verify_bundle

STORAGE_TESTS = {"test_virtual_memory_storage", "test_vm_pool"}
CONSUMER_TESTS = {"consumer_transfer", "consumer_pool_handle_before_owner", "consumer_static_destruction"}


def check_inventory(configured: dict, discovered: dict):
    names = [test["name"] for test in discovered.get("tests", [])]
    if len(names) != 2 or set(names) != STORAGE_TESTS:
        raise ValueError("Actual CTest storage inventory must contain exactly the two frozen tests")
    if (configured.get("mode") != "storage-only"
            or configured.get("ctest_count") != 2
            or configured.get("runtime_test_program_count") != 2
            or set(configured.get("runtime_test_programs", [])) != STORAGE_TESTS
            or any(configured.get(k, []) for k in
                   ("subprocess_scenarios", "tooling_checks", "configure_checks"))):
        raise ValueError("Configured storage inventory mismatch")


def check_junit(path: Path, expected: set[str]):
    root = ET.parse(path).getroot()
    cases = list(root.iter("testcase"))
    if len(cases) != len(expected) or {x.get("name") for x in cases} != expected:
        raise ValueError("JUnit inventory mismatch: " + str(path))
    for suite in root.iter("testsuite"):
        if any(int(suite.get(k, "0")) for k in ("failures", "errors", "skipped", "disabled")):
            raise ValueError("JUnit contains failures or skipped tests")
    for case in cases:
        if (case.get("status", "run") not in ("run", "passed")
                or any(case.find(tag) is not None for tag in ("failure", "error", "skipped"))):
            raise ValueError("JUnit case did not pass")
    return len(cases)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--generator", choices=("NMake Makefiles", "Ninja"), default="NMake Makefiles")
    parser.add_argument("--jobs", type=int, default=1)
    args = parser.parse_args(argv)
    bundle = args.bundle.resolve(strict=True)
    # Evidence stays outside the immutable bundle, and an earlier run is never overwritten.
    if os.path.lexists(args.out):
        parser.error("--out must be a new directory")
    out = args.out.resolve()
    if out == bundle or bundle in out.parents:
        parser.error("--out must be outside the prepared bundle")
    out.mkdir(parents=True, exist_ok=False)
    record = {"status": "running", "scope": "frozen-F-windows-storage-only", "commands": [],
              "started": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "host": {"system": platform.system(), "release": platform.release(),
                       "machine": platform.machine(), "python": sys.version,
                       "python_executable": sys.executable},
              "generator": args.generator, "jobs": args.jobs}

    def save():
        (out / "result.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")

    def run(name, command):
        # Use a binary log to retain compiler diagnostics in any Windows codepage.
        log = out / (name + ".log")
        item = {"name": name, "argv": list(map(str, command)), "log": log.name}
        record["commands"].append(item)
        save()
        with log.open("wb") as stream:
            stream.write((json.dumps(item["argv"]) + "\n").encode("utf-8"))
            stream.flush()
            result = subprocess.run(item["argv"], stdout=stream, stderr=subprocess.STDOUT,
                                    env=env, check=False)
        item["returncode"] = result.returncode
        save()
        if result.returncode:
            raise RuntimeError(name + " failed; see " + str(log))
        print(name + ": passed", flush=True)
        # CMake/CTest machine-readable output is UTF-8; version probes are ASCII.
        return log.read_bytes().split(b"\n", 1)[1].decode("utf-8", errors="replace")

    save()
    env = os.environ.copy()
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    for name in ("CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "CC", "CXX"):
        env.pop(name, None)
    try:
        if os.name != "nt" or platform.system() != "Windows":
            raise ValueError("Native Windows execution is required")
        if sys.version_info < (3, 9):
            raise ValueError("Python 3.9 or newer is required")
        if args.jobs < 1:
            raise ValueError("--jobs must be positive")
        tools = {name: shutil.which(name) for name in ("cmake", "ctest", "cl", "link")}
        build_tool = "nmake" if args.generator == "NMake Makefiles" else "ninja"
        tools[build_tool] = shutil.which(build_tool)
        record["tools"] = tools
        if any(path is None for path in tools.values()):
            raise ValueError("Missing Windows build tools: " + str([n for n, p in tools.items() if p is None]))
        selected_env = {name: env.get(name, "") for name in
                        ("VSCMD_VER", "VSCMD_ARG_TGT_ARCH", "VCToolsInstallDir", "WindowsSdkDir", "INCLUDE", "LIB")}
        record["developer_environment"] = selected_env
        if (not all(selected_env[n] for n in ("VSCMD_VER", "VCToolsInstallDir", "INCLUDE", "LIB"))
                or selected_env["VSCMD_ARG_TGT_ARCH"].lower() not in ("x64", "amd64")):
            raise ValueError("Use an x64 MSVC Developer PowerShell with INCLUDE/LIB initialized")
        for name in ("cmake", "ctest"):
            version_text = run(name + "-version", [tools[name], "--version"])
            match = re.search(r"version (\d+)\.(\d+)\.(\d+)", version_text)
            if match is None or tuple(map(int, match.groups())) < (3, 21, 0):
                raise ValueError(name + " 3.21+ is required for JSON inventory and JUnit output")
        record["bundle"] = verify_bundle(bundle)
        source = bundle / "source"
        build = out / "storage-build"
        run("storage-configure", [tools["cmake"], "-S", source, "-B", build, "-G", args.generator,
                                  "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_CXX_COMPILER=" + tools["cl"],
                                  "-DXOFFSET_STORAGE_ONLY=ON", "-DXOFFSET_BUILD_TESTS=ON",
                                  "-DXOFFSET_BUILD_TOOLS=OFF", "-DXOFFSET_BUILD_EXAMPLES=OFF", "-DXOFFSET_INSTALL=ON"])
        # A foreign compiler must never be reported as a Windows MSVC pass.
        compiler_files = list((build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
        if len(compiler_files) != 1 or not re.search(
                r'set\(CMAKE_CXX_COMPILER_ID "MSVC"\)', compiler_files[0].read_text(encoding="utf-8")):
            raise ValueError("Configured compiler is not MSVC")
        run("storage-build", [tools["cmake"], "--build", build, "--config", "Release", "--parallel", str(args.jobs)])
        discovered_text = run("storage-discovery", [tools["ctest"], "--test-dir", build, "-C", "Release", "--show-only=json-v1"])
        discovered = json.loads(discovered_text)
        configured = json.loads((build / "test-inventory.json").read_text(encoding="utf-8"))
        check_inventory(configured, discovered)
        record["configured_inventory"] = configured
        (out / "storage-discovery.json").write_text(discovered_text, encoding="utf-8")
        run("storage-test", [tools["ctest"], "--test-dir", build, "-C", "Release", "--output-on-failure",
                             "--output-junit", out / "storage-tests.xml"])
        record["storage_tests"] = check_junit(out / "storage-tests.xml", STORAGE_TESTS)
        package = out / "package"
        run("package-validation", [sys.executable, source / "tools/validate_vm_package.py",
                                   "--source-dir", source, "--out", package, "--suite", "storage",
                                   "--compiler", tools["cl"], "--generator", args.generator, "--jobs", str(args.jobs)])
        package_result = json.loads((package / "result.json").read_text(encoding="utf-8"))
        if (package_result.get("status") != "passed" or package_result.get("suite") != "storage"
                or package_result.get("installed_consumer_tests") != 3
                or package_result.get("subproject_consumer_tests") != 3):
            raise ValueError("Package validation did not complete all consumer cases")
        for name in ("consumer-tests.xml", "subproject-tests.xml"):
            check_junit(package / name, CONSUMER_TESTS)
        record["package_tests"] = {"installed": 3, "subproject": 3}
        verify_bundle(bundle)
        record["status"] = "passed"
    except BaseException as exc:
        record["status"] = "interrupted" if isinstance(exc, KeyboardInterrupt) else "failed"
        record["error"] = str(exc)
        print(str(exc), file=sys.stderr)
    record["finished"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    save()
    return 0 if record["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
