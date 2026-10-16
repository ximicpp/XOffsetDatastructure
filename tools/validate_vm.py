#!/usr/bin/env python3
"""Reproducible VM validation; see docs/VM_VALIDATION_RUNNER.md for examples."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import time
import xml.etree.ElementTree as ET

BOOST = ("container", "core", "config", "throw_exception", "static_assert",
         "container_hash", "type_traits", "winapi", "intrusive", "predef")
STORAGE = ("test_virtual_memory_storage", "test_vm_pool")
SANITIZED = ("test_nested", "test_compaction", "test_xbuffer_api", "test_xhandle",
             "test_error_paths", "test_complex_nesting", "test_adaptive_reservation",
             "test_main_containers", "test_runtime_regressions", "test_mutation_failures",
             "test_wire_validation", "test_stable_storage", *STORAGE,
             "test_storage_policies", "test_vm_lifecycle", "test_vm_transport",
             "test_owner_registry", "test_tail_expansion", "test_allocator_model",
             "test_allocator_failures")
THREADED = ("test_stable_storage", "test_vm_lifecycle", "test_vm_transport",
            "test_owner_registry", "test_tail_expansion", "test_storage_policies",
            "test_vm_pool", "test_virtual_memory_storage")


class Unavailable(Exception):
    """Invalid or missing prerequisites: no validation claim is possible."""


class Failed(Exception):
    """A command or validation actually failed."""


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def file_manifest(root):
    return {p.relative_to(root).as_posix(): {"sha256": sha(p), "bytes": p.stat().st_size}
            for p in sorted(root.rglob("*")) if p.is_file()}


def require_file(path, what):
    if path is None or not Path(path).is_file():
        raise Unavailable(f"{what} is missing: {path}")
    return Path(path).resolve()


def require_dir(path, what):
    if path is None or not Path(path).is_dir():
        raise Unavailable(f"{what} is missing: {path}")
    return Path(path).resolve()


def executable(value, what):
    found = shutil.which(str(value)) if value else None
    if not found:
        raise Unavailable(f"{what} is not executable: {value}")
    return str(Path(found).absolute())


def extract_archive(archive, destination):
    """Extract regular files/directories only, rejecting escapes and collisions."""
    archive = require_file(archive, "archive")
    try:
        with tarfile.open(archive, "r:*") as bundle:
            members = bundle.getmembers()
            seen = set()
            for member in members:
                name = PurePosixPath(member.name)
                if (not member.name or name.is_absolute() or ".." in name.parts
                        or "\\" in member.name or ":" in member.name
                        or not (member.isfile() or member.isdir())):
                    raise Unavailable(f"unsafe archive member: {member.name!r}")
                if str(name) in seen:
                    raise Unavailable(f"duplicate archive member: {member.name!r}")
                seen.add(str(name))
            destination.mkdir()
            for member in members:
                target = destination.joinpath(*PurePosixPath(member.name).parts)
                if member.isdir():
                    target.mkdir(parents=True, exist_ok=True)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with bundle.extractfile(member) as source, target.open("xb") as output:
                        shutil.copyfileobj(source, output)
                    target.chmod(0o755 if member.mode & 0o111 else 0o644)
    except (tarfile.TarError, OSError) as exc:
        raise Unavailable(f"cannot extract {archive}: {exc}") from exc


def source_root(extracted):
    if (extracted / "CMakeLists.txt").is_file():
        return extracted
    candidates = [p for p in extracted.iterdir()
                  if p.is_dir() and (p / "CMakeLists.txt").is_file()]
    if len(candidates) == 1 and len(list(extracted.iterdir())) == 1:
        return candidates[0]
    raise Unavailable("source archive must contain CMakeLists.txt at its root or in one wrapper directory")


def stage_source(args, out):
    if args.source_archive:
        extract_archive(args.source_archive, out / "source-archive")
        source = source_root(out / "source-archive")
    else:
        original = require_dir(args.source_dir, "source directory")
        require_file(original / "CMakeLists.txt", "source CMakeLists.txt")
        for name in ("tests", "tools", "examples", "cmake", "include"):
            if out == original / name or original / name in out.parents:
                raise Unavailable("--out must be outside source directories copied into the snapshot")
        source = out / "source"
        source.mkdir()
        # Deliberately snapshot the build surface, excluding build trees, evidence
        # archives, and dependencies (the latter are an explicit separate input).
        for path in original.iterdir():
            if path.is_file() and not path.name.startswith("."):
                shutil.copy2(path, source / path.name)
        for name in ("tests", "tools", "examples", "cmake", "include"):
            if (original / name).is_dir():
                shutil.copytree(original / name, source / name,
                                ignore=shutil.ignore_patterns("__pycache__", "*.pyc", ".DS_Store"))
    require_file(source / "xoffset_virtual_memory.hpp", "OS header")
    if args.suite == "full" or args.mode == "wire":
        require_file(source / "xoffsetdatastructure.hpp", "main header")
    if (source / "external").exists():
        raise Unavailable("source must exclude external dependencies; supply them with --deps")
    manifest = file_manifest(source)
    write_json(out / "source-manifest.json", manifest)
    if args.source_manifest:
        expected = json.loads(require_file(args.source_manifest, "source manifest").read_text())
        if manifest != expected:
            raise Failed("source manifest differs from --source-manifest (see source-manifest.json)")
    return source


def dependency_manifest(root, out):
    root = require_dir(root, "--deps dependency root")
    includes = [root / "boost/libs" / name / "include" for name in BOOST]
    includes.append(root / "typelayout/include")
    for path in includes:
        require_dir(path, "dependency include directory")
        if not any(p.is_file() and p.suffix.lower() in (".h", ".hpp", ".hh", ".hxx")
                   for p in path.rglob("*")):
            raise Unavailable(f"dependency include directory has no headers: {path}")
    require_file(root / "typelayout/include/boost/typelayout.hpp", "TypeLayout header")
    files = {}
    for directory in includes:
        files.update({str(directory.relative_to(root) / name): entry
                      for name, entry in file_manifest(directory).items()})
    write_json(out / "dependencies.json", {"root": str(root), "files": files})
    return root, includes


class Runner:
    def __init__(self, out):
        self.out = out
        self.commands = []

    def run(self, command, name, env=None, prerequisite=False):
        command = list(map(str, command))
        print("RUN", name, flush=True)
        started = time.time()
        log = self.out / (name + ".log")
        try:
            with log.open("w") as stream:
                stream.write(json.dumps(command) + "\n")
                stream.flush()
                result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, env=env)
        except OSError as exc:
            raise Unavailable(f"cannot execute {command[0]}: {exc}") from exc
        self.commands.append({"argv": command, "log": log.name, "returncode": result.returncode,
                              "started_epoch": started, "finished_epoch": time.time()})
        write_json(self.out / "commands.json", self.commands)
        output = log.read_text(errors="replace").split("\n", 1)[1]
        if result.returncode:
            problem = f"command exited {result.returncode}: {log}\n{output[-4000:]}"
            raise (Unavailable if prerequisite else Failed)(problem)
        return output


def linux_runtime_manifest(directory):
    """A narrow Docker mount must contain both builtin headers and runtime libs."""
    root = require_dir(directory, "Linux compiler resource directory")
    for name in ("lib", "include"):
        require_dir(root / name, f"Linux compiler resource {name} directory")
    files = {}

    def visit(path, relative, ancestors):
        try:
            resolved = path.resolve(strict=True)
        except (OSError, RuntimeError) as exc:
            raise Unavailable(f"broken or cyclic runtime link: {path}: {exc}") from exc
        if resolved != root and root not in resolved.parents:
            raise Unavailable(f"runtime link escapes --runtime-dir: {path}; materialize it inside the runtime directory")
        if path.is_symlink() and Path(os.readlink(path)).is_absolute():
            raise Unavailable(f"absolute runtime link will break inside the container: {path}; materialize it first")
        if path.is_dir():
            if resolved in ancestors:
                raise Unavailable(f"cyclic runtime directory link: {path}")
            for child in sorted(path.iterdir()):
                visit(child, relative / child.name, ancestors | {resolved})
        elif path.is_file():
            files[relative.as_posix()] = {"sha256": sha(path), "bytes": path.stat().st_size}
        else:
            raise Unavailable(f"runtime input is not a regular file or directory: {path}")

    visit(root, Path(), set())
    for name in ("lib", "include"):
        if not any(path.startswith(name + "/") for path in files):
            raise Unavailable(f"Linux compiler resource {name} directory is empty: {root / name}")
    return files


def cmake_bundle_manifest(directory):
    """Keep a separately mounted CMake distribution self-contained and auditable."""
    root = require_dir(directory, "--cmake-root")
    for path in root.rglob("*"):
        if path.is_symlink():
            raise Unavailable("--cmake-root must contain materialized files, not symlinks")
    require_file(root / "bin/cmake", "CMake bundle executable")
    require_file(root / "bin/ctest", "CTest bundle executable")
    if not list((root / "share").glob("cmake-*/Modules/CMake.cmake")):
        raise Unavailable("--cmake-root must include CMake's share/cmake-*/Modules")
    return file_manifest(root)


def sanitizer_flags(args):
    if args.sanitizer == "none":
        return [], []
    kind = "address,undefined" if args.sanitizer == "address-undefined" else "thread"
    flags = [f"-fsanitize={kind}", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"]
    links = []
    if args.runtime_dir:
        runtime = require_dir(args.runtime_dir, "sanitizer runtime directory")
        if args.sanitizer == "thread" and args.mode != "linux" and sys.platform == "darwin":
            require_file(runtime / "libclang_rt.tsan_osx_dynamic.dylib", "Apple TSan runtime")
            flags += ["-fno-sanitize-link-runtime"]
            links += [f"-L{runtime}", "-lclang_rt.tsan_osx_dynamic", f"-Wl,-rpath,{runtime}"]
        else:
            require_dir(runtime / "lib", "compiler resource runtime lib directory")
            if args.mode == "linux":
                args.linux_runtime_files = linux_runtime_manifest(runtime)
            flags += ["-resource-dir=" + ("/runtime" if args.mode == "linux" else str(runtime))]
    elif args.mode == "linux":
        raise Unavailable("Linux sanitizer validation requires explicit --runtime-dir (compiler resource directory)")
    return flags, links


def storage_driver(source, out):
    if "XOFFSET_STORAGE_ONLY" in (source / "CMakeLists.txt").read_text():
        return source, "source-cmake"
    # Frozen F archives predate storage-only CMake. Keep the archived source intact.
    driver = out / "storage-driver"
    driver.mkdir()
    lines = ["cmake_minimum_required(VERSION 3.15)", "project(FrozenStorage LANGUAGES CXX)",
             "enable_testing()", "find_package(Threads REQUIRED)"]
    for target in STORAGE:
        require_file(source / "tests" / (target + ".cpp"), "storage test")
        lines += [f'add_executable({target} "${{VM_SOURCE}}/tests/{target}.cpp")',
                  f"target_compile_features({target} PRIVATE cxx_std_17)",
                  f"target_link_libraries({target} PRIVATE Threads::Threads)",
                  f"target_compile_options({target} PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)",
                  f"add_test(NAME {target} COMMAND {target})"]
    (driver / "CMakeLists.txt").write_text("\n".join(lines) + "\n")
    return driver, "maintained-legacy-storage-driver"


def verify_ctest_results(path, inventory):
    root = ET.parse(path).getroot()
    cases = root.findall(".//testcase")
    names = sorted(case.attrib.get("name", "") for case in cases)
    expected = sorted(test["name"] for test in inventory)
    bad_counts = any(int(suite.attrib.get(key, "0")) != 0
                     for suite in ([root] + root.findall(".//testsuite"))
                     for key in ("failures", "errors", "skipped", "disabled"))
    bad_cases = any(case.find(status) is not None for case in cases for status in ("skipped", "failure", "error"))
    bad_status = any(case.attrib.get("status", "run") != "run" for case in cases)
    if not expected or names != expected or bad_counts or bad_cases or bad_status:
        raise Failed("CTest results differ from inventory or include zero/skipped/failed cases")
    return len(cases)


def validate_cmake_args(arguments):
    # Extra arguments are cache data only. -C/-U, toolchain/project injection,
    # compiler flags and suite controls can invalidate the requested claim.
    for argument in arguments:
        match = re.fullmatch(r"-D([A-Za-z_][A-Za-z_0-9]*)(?::(?:BOOL|FILEPATH|PATH|STRING|INTERNAL))?=(.*)", argument, re.DOTALL)
        if not match:
            raise Unavailable("--cmake-arg accepts only -DNAME[:TYPE]=VALUE cache definitions")
        name = match.group(1).upper()
        if name.startswith(("CMAKE_", "CTEST_", "XOFFSET_")) or name in ("VM_SOURCE", "BUILD_TESTING"):
            raise Unavailable(f"--cmake-arg cannot override runner/build control {match.group(1)}")


def verify_build_inventory(build, suite, all_tests, out):
    path = build / "test-inventory.json"
    if not path.exists():
        return None  # Historical F source predates generated CMake inventories.
    inventory = json.loads(path.read_text())
    expected_mode = "storage-only" if suite == "storage" else "full"
    if inventory.get("mode") != expected_mode:
        raise Failed(f"build inventory mode {inventory.get('mode')!r} differs from requested {expected_mode}")
    expected = sum((inventory.get(name, []) for name in
                    ("runtime_test_programs", "subprocess_scenarios", "tooling_checks")), [])
    if sorted(expected) != sorted(t["name"] for t in all_tests) or inventory.get("ctest_count") != len(all_tests):
        raise Failed("generated build inventory differs from registered CTest tests")
    write_json(out / "build-test-inventory.json", inventory)
    return inventory


def cmake_suite(args, source, out, runner, deps):
    flags, links = sanitizer_flags(args)
    build = out / "build"
    build.mkdir()
    driver, driver_kind = storage_driver(source, out) if args.suite == "storage" else (source, "source-cmake")
    prefix = []
    compiler = args.compiler
    if args.mode == "linux":
        docker = executable(args.docker, "Docker")
        if not args.image or not re.fullmatch(r"[^\s@]+@sha256:[0-9a-f]{64}", args.image):
            raise Unavailable("--image must be a registry reference pinned with @sha256:<64 hex digits>")
        inspect = runner.run([docker, "image", "inspect", args.image], "image-inspect", prerequisite=True)
        write_json(out / "container-image.json", json.loads(inspect))
        prefix = [docker, "run", "--rm", "--pull=never", "--network=none", "--cpus", str(args.jobs),
                  "--platform", args.platform, "--mount", f"type=bind,src={source},dst=/src,readonly",
                  "--mount", f"type=bind,src={out},dst=/validation"]
        if deps:
            (source / "external").mkdir()
            prefix += ["--mount", f"type=bind,src={deps},dst=/src/external,readonly"]
        if args.runtime_dir:
            prefix += ["--mount", f"type=bind,src={args.runtime_dir.resolve()},dst=/runtime,readonly"]
        if args.cmake_root:
            prefix += ["--mount", f"type=bind,src={args.cmake_root.resolve()},dst=/cmake,readonly"]
        prefix += ["-e", "ASAN_OPTIONS=detect_leaks=1:halt_on_error=1",
                   "-e", "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1",
                   "-e", "TSAN_OPTIONS=halt_on_error=1", args.image]
        compiler = compiler or "/opt/p2996-toolchain/bin/clang++"
        source_arg = "/src" if driver == source else "/validation/storage-driver"
        build_arg = "/validation/build"
        cmake, ctest = ("/cmake/bin/cmake", "/cmake/bin/ctest") if args.cmake_root else ("cmake", "ctest")
    else:
        compiler = executable(compiler, "--compiler")
        cmake, ctest = executable(args.cmake, "CMake"), executable(args.ctest, "CTest")
        source_arg, build_arg = str(driver), str(build)
        if deps:
            (source / "external").symlink_to(deps, target_is_directory=True)
    version = runner.run(prefix + [compiler, "--version"], "compiler", prerequisite=True)
    # Probe actual compiler resource location; manifests also record every explicit flag.
    resource = runner.run(prefix + [compiler, "-print-resource-dir"], "compiler-resource", prerequisite=True) if "clang" in version.lower() else None
    runner.run(prefix + [cmake, "--version"], "cmake-version", prerequisite=True)
    runner.run(prefix + [ctest, "--version"], "ctest-version", prerequisite=True)
    if args.sdk:
        flags += ["-isysroot", str(require_dir(args.sdk, "SDK"))]
    for directory in args.library_dir:
        directory = require_dir(directory, "library directory")
        links += [f"-L{directory}", f"-Wl,-rpath,{directory}"]
    configure = [cmake, "-S", source_arg, "-B", build_arg, "-DCMAKE_BUILD_TYPE=Debug",
                 f"-DCMAKE_CXX_COMPILER={compiler}",
                 f"-DXOFFSET_STORAGE_ONLY={'ON' if args.suite == 'storage' else 'OFF'}"]
    if args.sdk:
        configure += [f"-DCMAKE_OSX_SYSROOT={args.sdk.resolve()}"]
    if driver != source:
        configure += [f"-DVM_SOURCE={'/src' if args.mode == 'linux' else source}"]
    if flags:
        configure += ["-DCMAKE_CXX_FLAGS=" + shlex.join(flags)]
    if links:
        configure += ["-DCMAKE_EXE_LINKER_FLAGS=" + shlex.join(links)]
    if args.mode == "linux" and args.suite == "full":
        configure += ["-DCMAKE_BUILD_RPATH=/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu"]
    configure += args.cmake_arg
    write_json(out / "toolchain.json", {"compiler": compiler, "version": version, "resource_dir": resource,
               "sdk": str(args.sdk) if args.sdk else None, "runtime_dir": str(args.runtime_dir) if args.runtime_dir else None,
               "runtime_files": (getattr(args, "linux_runtime_files", None) or file_manifest(args.runtime_dir)) if args.runtime_dir else None,
               "cmake_root": str(args.cmake_root) if args.cmake_root else None,
               "cmake_files": getattr(args, "cmake_files", None),
               "driver": driver_kind, "configure": configure, "image": args.image, "platform": args.platform})
    runner.run(prefix + configure, "configure")
    targets = args.targets.split(",") if args.targets else []
    if not targets and args.suite == "full" and args.sanitizer != "none":
        targets = list(THREADED if args.sanitizer == "thread" else SANITIZED)
    if targets and any(not re.fullmatch(r"[A-Za-z0-9_]+", target) for target in targets):
        raise Unavailable("--targets requires comma-separated CMake target names")
    selection = ["-R", "^(" + "|".join(map(re.escape, targets)) + ")($|_)"] if targets else []
    inventory = json.loads(runner.run(prefix + [ctest, "--test-dir", build_arg, "-C", "Debug", "--show-only=json-v1"], "inventory"))
    all_tests = inventory.get("tests", [])
    verify_build_inventory(build, args.suite, all_tests, out)
    tests = [test for test in all_tests if not selection or re.search(selection[1], test["name"])]
    if not tests:
        raise Failed("CTest selected zero tests")
    for target in targets:
        if not any(t["name"] == target for t in tests):
            raise Unavailable(f"requested target has no matching CTest case: {target}")
    inventory["tests"] = tests
    scope = {"scope": "targets" if targets else "all", "targets": targets,
             "selected_count": len(tests), "registered_count": len(all_tests),
             "selected_names": [test["name"] for test in tests]}
    inventory["selection"] = scope
    write_json(out / "test-inventory.json", inventory)
    build_command = [cmake, "--build", build_arg, "--config", "Debug", "--parallel", str(args.jobs)]
    if targets:
        build_command += ["--target"] + targets
    runner.run(prefix + build_command, "build")
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:halt_on_error=1" if sys.platform != "darwin" else "detect_leaks=0:halt_on_error=1",
               UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1", TSAN_OPTIONS="halt_on_error=1")
    junit_arg = "/validation/ctest.xml" if prefix else str(out / "ctest.xml")
    runner.run(prefix + [ctest, "--test-dir", build_arg, "-C", "Debug", "--output-on-failure", "--output-junit", junit_arg] + selection, "ctest", env=env)
    passed = verify_ctest_results(out / "ctest.xml", tests)
    last = build / "Testing/Temporary/LastTest.log"
    if last.exists():
        shutil.copy2(last, out / "last-test.log")
    if args.cmake_root and cmake_bundle_manifest(args.cmake_root) != args.cmake_files:
        raise Failed("CMake bundle changed during validation")
    return {"tests_passed": passed, "suite": args.suite, "sanitizer": args.sanitizer,
            "selection": scope,
            "leak_detection": args.sanitizer == "address-undefined" and (prefix != [] or sys.platform != "darwin")}


def wire_suite(args, source, out, runner, includes):
    if args.suite != "full" or args.sanitizer != "none":
        raise Unavailable("wire uses --suite full and --sanitizer none")
    if args.targets or args.cmake_arg:
        raise Unavailable("wire does not use CMake --targets or --cmake-arg")
    compiler = executable(args.compiler, "--compiler")
    bundle = out / "wire-inputs"
    extract_archive(args.wire_bundle, bundle)
    harness = require_file(bundle / "wire_matrix.cpp", "preserved wire harness")
    write_json(out / "wire-input-manifest.json", file_manifest(bundle))
    variants = {label: bundle / label for label in args.wire_variants.split(",")}
    if len(variants) != len(args.wire_variants.split(",")) or "current" in variants or not variants:
        raise Unavailable("wire variants must be unique historical labels, excluding current")
    for label, root in variants.items():
        if not re.fullmatch(r"[A-Za-z0-9_]+", label):
            raise Unavailable(f"invalid wire variant label: {label}")
        require_file(root / "xoffsetdatastructure.hpp", f"wire variant {label}")
    variants["current"] = source
    frames = sorted(bundle.glob("*_phase[012].bin"))
    if not frames:
        raise Unavailable("wire bundle contains no saved phase0/phase1/phase2 frames")
    flags = [compiler, "-std=c++26", "-freflection", "-fexpansion-statements", "-stdlib=libc++", "-UNDEBUG", "-O2", "-pthread"]
    for include in includes:
        flags += ["-isystem", str(include)]
    if args.sdk:
        flags += ["-isysroot", str(require_dir(args.sdk, "SDK"))]
    for directory in args.library_dir:
        directory = require_dir(directory, "library directory")
        flags += [f"-L{directory}", f"-Wl,-rpath,{directory}"]
    version = runner.run([compiler, "--version"], "compiler", prerequisite=True)
    write_json(out / "toolchain.json", {"compiler": compiler, "version": version, "flags": flags})
    binaries = {}
    for label, root in variants.items():
        binary = out / ("wire_" + label)
        runner.run(flags + ["-I" + str(root), str(harness), "-o", str(binary)], "compile-" + label)
        binaries[label] = binary
    operations = []

    def invoke(label, operation, path, phase=None, destination=None, historical=False):
        command = [binaries[label], operation, path]
        if phase is not None:
            command += [str(phase)]
        if destination is not None:
            command += [destination]
        name = f"operation-{len(operations):04}"
        runner.run(command, name)
        operations.append({"variant": label, "operation": operation, "input": str(path), "phase": phase,
                           "output": str(destination) if destination else None, "historical": historical, "log": name + ".log"})

    for frame in frames:
        phase = int(re.search(r"_phase([012])\.bin$", frame.name).group(1))
        invoke("current", "check", frame, phase, historical=True)
    for writer in variants:
        seed = out / f"{writer}_phase0.bin"
        invoke(writer, "seed", seed)
        for reader in variants:
            invoke(reader, "check", seed, 0)
            one = out / f"{writer}_{reader}_phase1.bin"
            invoke(reader, "mutate", seed, 0, one)
            for reader2 in variants:
                invoke(reader2, "check", one, 1)
                two = out / f"{writer}_{reader}_{reader2}_phase2.bin"
                invoke(reader2, "mutate", one, 1, two)
                for final_reader in variants:
                    invoke(final_reader, "check", two, 2)
    write_json(out / "test-inventory.json", {"variants": list(variants), "operations": operations})
    write_json(out / "wire-output-manifest.json", {p.name: {"sha256": sha(p), "bytes": p.stat().st_size} for p in out.glob("*.bin")})
    return {"operations_passed": len(operations), "historical_frames_passed": len(frames), "variants": list(variants)}


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("mode", choices=("native", "linux", "wire"))
    source = p.add_mutually_exclusive_group(required=True)
    source.add_argument("--source-dir", type=Path)
    source.add_argument("--source-archive", type=Path)
    p.add_argument("--source-manifest", type=Path, help="Exact expected file/sha256/bytes JSON manifest")
    p.add_argument("--out", type=Path, required=True, help="Fresh output directory; never overwritten")
    p.add_argument("--suite", choices=("full", "storage"), default="full")
    p.add_argument("--deps", type=Path, help="Explicit external root containing boost/libs and typelayout/include")
    p.add_argument("--compiler", help="Native compiler executable or compiler path inside Linux image")
    p.add_argument("--sdk", type=Path)
    p.add_argument("--library-dir", type=Path, action="append", default=[])
    p.add_argument("--runtime-dir", type=Path, help="Compiler resource directory, or native Apple TSan dylib directory")
    p.add_argument("--sanitizer", choices=("none", "address-undefined", "thread"), default="none")
    p.add_argument("--jobs", type=int, default=2)
    p.add_argument("--targets", help="Comma-separated CMake test target names (subprocess cases included)")
    p.add_argument("--image", help="Already-pulled registry image pinned by @sha256 digest")
    p.add_argument("--platform", default="linux/amd64", help="Docker execution platform; recorded in manifest")
    p.add_argument("--docker", default="docker")
    p.add_argument("--cmake", default="cmake")
    p.add_argument("--ctest", default="ctest")
    p.add_argument("--cmake-root", type=Path, help="Explicit self-contained CMake distribution mounted read-only in Linux mode")
    p.add_argument("--cmake-arg", action="append", default=[], help="Extra configure argument; use --cmake-arg=-D...")
    p.add_argument("--wire-bundle", type=Path, help="Explicit archive containing historical headers, frames and wire_matrix.cpp")
    p.add_argument("--wire-variants", default="v0,v1,d", help="Historical bundle directory labels; current source is added")
    return p


def main(argv=None):
    args = parser().parse_args(argv)
    out = args.out.resolve()
    try:
        out.mkdir(parents=True, exist_ok=False)
    except OSError as exc:
        print(f"UNAVAILABLE: output must be a fresh directory: {out}: {exc}", file=sys.stderr)
        return 2
    manifest = {"status": "running", "started_epoch": time.time(), "argv": sys.argv[1:] if argv is None else argv,
                "host": platform.platform(), "runner_sha256": sha(Path(__file__)),
                "inputs": {name: str(value.resolve()) for name, value in vars(args).items() if isinstance(value, Path)}}
    write_json(out / "result.json", manifest)
    code = 0
    try:
        shutil.copy2(__file__, out / "validate_vm.py")
        if args.compiler and Path(args.compiler).name.lower() in ("cl", "cl.exe", "clang-cl", "clang-cl.exe"):
            raise Unavailable("this runner supports Clang/GNU-style drivers; use standalone CMake/CI for Windows MSVC storage validation")
        if args.jobs < 1:
            raise Unavailable("--jobs must be positive")
        validate_cmake_args(args.cmake_arg)
        if args.mode == "linux" and (args.sdk or args.library_dir):
            raise Unavailable("--sdk and --library-dir are native paths; Linux uses its pinned image toolchain")
        if args.mode != "linux" and args.image:
            raise Unavailable("--image applies only to Linux container validation")
        if args.cmake_root:
            if args.mode != "linux":
                raise Unavailable("--cmake-root applies only to Linux; use --cmake and --ctest for native validation")
            args.cmake_files = cmake_bundle_manifest(args.cmake_root)
        if args.runtime_dir and args.sanitizer == "none":
            raise Unavailable("--runtime-dir applies only to sanitizer validation")
        source = stage_source(args, out)
        deps, includes = dependency_manifest(args.deps, out) if args.suite == "full" else (None, [])
        for key in ("source_archive", "wire_bundle", "source_manifest"):
            value = getattr(args, key)
            if value:
                manifest.setdefault("input_sha256", {})[key] = sha(require_file(value, key))
        runner = Runner(out)
        details = wire_suite(args, source, out, runner, includes) if args.mode == "wire" else cmake_suite(args, source, out, runner, deps)
        manifest.update(status="passed", **details)
    except Unavailable as exc:
        code = 2
        manifest.update(status="unavailable", reason=str(exc))
    except (Failed, OSError, ValueError, ET.ParseError) as exc:
        code = 1
        manifest.update(status="failed", reason=str(exc))
    except Exception as exc:
        code = 1
        manifest.update(status="failed", reason=f"unexpected {type(exc).__name__}: {exc}")
    except KeyboardInterrupt:
        code = 130
        manifest.update(status="interrupted", reason="validation interrupted")
    finally:
        manifest["finished_epoch"] = time.time()
        write_json(out / "result.json", manifest)
    print(f"{manifest['status'].upper()}: {out / 'result.json'}")
    if "reason" in manifest:
        print(manifest["reason"], file=sys.stderr)
    return code


if __name__ == "__main__":
    sys.exit(main())
