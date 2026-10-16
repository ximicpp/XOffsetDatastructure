#!/usr/bin/env python3
"""Exercise the configure-time rejection guard in isolated source copies.

Requires the same working P2996 compiler/dependencies as a full build. This
does not edit the repository headers or run any deliberately unsafe binary.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def cmake_path(path):
    value = Path(path).resolve().as_posix()
    if "]==]" in value:
        raise ValueError("Unsupported CMake bracket delimiter in path")
    return "[==[" + value + "]==]"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", required=True, type=Path, help="New evidence directory")
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--cmake-arg", action="append", default=[])
    args = parser.parse_args()
    source = args.source.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    original = (source / "xoffsetdatastructure.hpp").read_bytes()
    original_hash = hashlib.sha256(original).hexdigest()
    boost_components = (
        "container", "core", "config", "throw_exception", "static_assert",
        "container_hash", "type_traits", "winapi", "intrusive", "predef",
    )
    results = []
    expected_messages = {
        "baseline": "expected rejection diagnostic verified",
        "missing_dependency": "Polymorphic rejection positive control failed",
        "disabled_guard": "Polymorphic rejection unexpectedly compiled and linked",
        "wrong_diagnostic": "Polymorphic rejection failed for an unexpected reason",
    }
    for case, expected in expected_messages.items():
        fixture = output / case / "source"
        (fixture / "tests").mkdir(parents=True)
        for name in ("xoffsetdatastructure.hpp", "xoffset_virtual_memory.hpp"):
            shutil.copyfile(source / name, fixture / name)
        for name in ("test_polymorphic_positive.cpp", "test_polymorphic_rejection.cpp"):
            shutil.copyfile(source / "tests" / name, fixture / "tests" / name)
        typelayout = source / "external/typelayout/include"
        if case == "missing_dependency":
            # Shadow the dependency with a header whose required input is absent.
            # This guarantees failure even if TypeLayout is installed globally.
            typelayout = fixture / "missing-typelayout"
            (typelayout / "boost").mkdir(parents=True)
            (typelayout / "boost/typelayout.hpp").write_text(
                '#include "xoffset_intentionally_missing_dependency.hpp"\n')
        elif case == "disabled_guard":
            header = (fixture / "xoffsetdatastructure.hpp").read_text()
            guard = 'T* make() {\n            static_assert(is_byte_copy_safe_v<T>,'
            if header.count(guard) != 1:
                raise RuntimeError("Expected exactly one XBuffer::make safety guard")
            header = header.replace(guard, 'T* make() {\n            static_assert(true,', 1)
            (fixture / "xoffsetdatastructure.hpp").write_text(header)
        elif case == "wrong_diagnostic":
            (fixture / "tests/test_polymorphic_rejection.cpp").write_text(
                '#error "unrelated failure must not satisfy the rejection check"\n')
        boost_paths = "\n    ".join(
            cmake_path(source / "external/boost/libs" / item / "include")
            for item in boost_components)
        (fixture / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.13)\n"
            "project(XOffsetGuardFixture LANGUAGES CXX)\n"
            f"include({cmake_path(source / 'cmake/XOffsetBuildChecks.cmake')})\n"
            "find_package(Threads REQUIRED)\n"
            "get_filename_component(CLANG_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)\n"
            "get_filename_component(CLANG_INSTALL_DIR ${CLANG_BIN_DIR} DIRECTORY)\n"
            f"set(BOOST_INCLUDE_DIRS\n    {boost_paths})\n"
            f"set(TYPELAYOUT_INCLUDE_DIRS {cmake_path(typelayout)})\n"
            "add_library(xoffset_probe_settings INTERFACE IMPORTED)\n"
            "xoffset_configure_full_interface(xoffset_probe_settings)\n"
            "xoffset_check_polymorphic_rejection()\n")
        command = [args.cmake, "-S", str(fixture), "-B", str(output / case / "build"),
                   "-DCMAKE_CXX_COMPILER=" + args.compiler, *args.cmake_arg]
        completed = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, check=False)
        (output / case / "configure.log").write_text(
            json.dumps(command) + "\n" + completed.stdout)
        success = ((completed.returncode == 0) == (case == "baseline")
                   and expected in " ".join(completed.stdout.split()))
        results.append({"case": case, "passed": success, "returncode": completed.returncode,
                        "expected_message": expected, "command": command})
        print(f"{case}: {'PASS' if success else 'FAIL'}", flush=True)
    unchanged = hashlib.sha256((source / "xoffsetdatastructure.hpp").read_bytes()).hexdigest() == original_hash
    summary = {"source_header_sha256": original_hash, "source_header_unchanged": unchanged,
               "results": results, "passed": unchanged and all(item["passed"] for item in results)}
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    if not summary["passed"]:
        raise SystemExit("Compile guard self-check failed; see " + str(output / "summary.json"))


if __name__ == "__main__":
    main()
