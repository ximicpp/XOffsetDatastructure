#!/usr/bin/env python3
"""Freeze, build, functionally smoke, and offline-audit the N7 TCP reference.

prepare and audit never compile or execute C++. Build and smoke must be wrapped
by experiments/vm_coordination/run_job.py after the parent releases the resource
gate. They require --coordinated-execution and prove that their ancestor queue
supervisor currently holds the shared lock. No benchmark/matrix command exists.

Builds use a frozen source tree and new output directory. Audits read only the
archive, including copied parent manifests, and are safe after relocation.
"""
from __future__ import annotations

import argparse
import ast
import datetime as dt
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1] if HERE.name == "vm_network" else Path.cwd()
BASELINE = {
    "xoffsetdatastructure.hpp": "9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2",
    "xoffset_virtual_memory.hpp": "22f181b52c11ce442a5576ffc0dc35af1828f77deb4424f387d0acfa658239ad",
}
NETWORK_FILES = ("net_reference.cpp", "frame_model.hpp", "frame_model_tests.hpp",
                 "model_subcases.json", "contract.json", "run_network.py", "test_run_network.py")
SOURCE_FILES = set(BASELINE) | {"LICENSE", "toolchains/vm-validation.json"} | set(NETWORK_FILES)
BOOST = ("container", "core", "config", "throw_exception", "static_assert",
         "container_hash", "type_traits", "winapi", "intrusive", "predef")
PROGRESS_FIELDS = ("read_bytes", "written_bytes", "receive_allocations", "published",
                   "terminal_count", "active_bytes", "active_queue_frames")
SANITIZER_RUNTIMES = ("lib/darwin/libclang_rt.asan_osx_dynamic.dylib",
                      "lib/darwin/libclang_rt.ubsan_osx_dynamic.dylib")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def now():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def write(path, value):
    Path(path).write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def safe_relative(name):
    require(isinstance(name, str) and bool(name) and "\\" not in name and ":" not in name,
            "unsafe manifest path")
    path = PurePosixPath(name)
    require(bool(path.parts) and not path.is_absolute() and ".." not in path.parts and path.as_posix() == name,
            "unsafe manifest path")
    return path


def files(root):
    root = Path(root)
    require(root.is_dir() and not root.is_symlink(), "invalid evidence directory")
    inventory = {}
    for path in sorted(root.rglob("*")):
        require(not path.is_symlink(), "symlink in evidence: " + str(path))
        require(path.is_dir() or path.is_file(), "nonregular evidence: " + str(path))
        if path.is_file():
            inventory[path.relative_to(root).as_posix()] = sha(path)
    return inventory


def check_files(root, expected):
    require(isinstance(expected, dict), "invalid file inventory")
    for name, digest in expected.items():
        safe_relative(name)
        path = Path(root) / name
        require(isinstance(digest, str) and len(digest) == 64, "invalid file digest")
        require(path.is_file() and not path.is_symlink() and sha(path) == digest,
                "missing/changed input: " + str(path))


def fresh(path):
    path = Path(os.path.abspath(path))
    require(not path.exists() and not path.is_symlink(), "output must be new: " + str(path))
    path.mkdir(parents=True, exist_ok=False)
    return path


def contract(path):
    result = read(path)
    require(result.get("schema") == "vm-network-reference-contract" and
            result.get("schema_version") == 1, "unsupported network contract")
    require(result.get("baseline_headers") == BASELINE, "contract baseline differs")
    require(result.get("profiles", {}).get("macro") == "N7_NET_DIAGNOSTICS", "profile macro differs")
    for profile, value in (("timing", 0), ("diagnostic", 1)):
        require(result["profiles"][profile]["value"] == value, "profile value differs")
    smoke = result["smoke"]
    ids = smoke["case_ids"]
    require(isinstance(ids, list) and ids and all(isinstance(x, str) and x for x in ids)
            and len(ids) == len(set(ids)), "invalid/duplicate contract case IDs")
    require(type(smoke["checks"]) is int and smoke["checks"] == len(ids), "contract check count differs")
    model, tcp = smoke["model_case_ids"], smoke["tcp_case_ids"]
    require(len(model) == len(set(model)) and len(tcp) == len(set(tcp)) and
            not set(model) & set(tcp) and set(model) | set(tcp) == set(ids),
            "contract model/TCP partition differs")
    expected = smoke.get("expected_reasons")
    if expected is not None:
        require(set(expected) == set(ids) and all(isinstance(v, str) and v for v in expected.values()),
                "expected reason mapping incomplete")
    if "subcases" in smoke:
        rows = smoke["subcases"]
        require(isinstance(rows, list) and rows, "empty subcase inventory")
        seen = set()
        for row in rows:
            require(isinstance(row, dict) and isinstance(row.get("id"), str) and row["id"] and
                    row["id"] not in seen and row.get("parent_id") in ids,
                    "invalid/duplicate contract subcase")
            seen.add(row["id"])
            require(isinstance(row.get("expected_reason"), str) and bool(row["expected_reason"]),
                    "subcase expected reason missing")
            for field in PROGRESS_FIELDS:
                require("expected_" + field in row, "subcase expected counter missing")
        require(type(smoke.get("subcase_count")) is int and smoke["subcase_count"] == len(rows),
                "contract subcase count differs")
        require({row["parent_id"] for row in rows} == set(ids), "contract group has no subcase")
    return result


def expected_counter(value, symbols):
    """Resolve only frozen integer arithmetic, never eval or arbitrary code."""
    if type(value) in (int, bool):
        return value
    require(isinstance(value, str), "invalid expected counter expression")

    def visit(node):
        if isinstance(node, ast.Expression):
            return visit(node.body)
        if isinstance(node, ast.Constant) and type(node.value) is int:
            return node.value
        if isinstance(node, ast.Name) and node.id in symbols:
            return symbols[node.id]
        if isinstance(node, ast.BinOp) and isinstance(node.op, (ast.Add, ast.Sub, ast.Mult, ast.FloorDiv)):
            left, right = visit(node.left), visit(node.right)
            if isinstance(node.op, ast.Add):
                return left + right
            if isinstance(node.op, ast.Sub):
                return left - right
            if isinstance(node.op, ast.Mult):
                return left * right
            require(right > 0, "invalid expected counter divisor")
            return left // right
        raise ValueError("unsupported expected counter expression")

    return visit(ast.parse(value, mode="eval"))


def validate_subcases(report, smoke_contract):
    frozen = smoke_contract.get("subcases")
    if frozen is None:
        return
    rows = report.get("subcases")
    require(isinstance(rows, list) and len(rows) == len(frozen), "incomplete subcase details")
    require(type(report.get("owned_sockets_after_cases")) is int and report["owned_sockets_after_cases"] == 0,
            "owned socket cleanup not proved")
    expected_rows = {row["id"]: row for row in frozen}
    require(len(expected_rows) == len(frozen), "duplicate frozen subcase ID")
    symbols = {}
    declarations = smoke_contract.get("progress_symbols", {})
    require(isinstance(declarations, dict), "invalid progress symbol declarations")
    for name, source in declarations.items():
        require(isinstance(name, str) and name.isidentifier() and isinstance(source, str), "invalid progress symbol")
        field = source.removeprefix("smoke.")
        require("." not in field and type(report.get(field)) is int and report[field] >= 0,
                "missing/noninteger progress symbol: " + name)
        symbols[name] = report[field]
    seen = set()
    for row in rows:
        require(isinstance(row, dict) and isinstance(row.get("id"), str) and row["id"] in expected_rows
                and row["id"] not in seen, "invalid/duplicate/extra subcase ID")
        case_id = row["id"]
        seen.add(case_id)
        expected = expected_rows[case_id]
        require(row.get("parent_id") == expected["parent_id"], "subcase parent differs: " + case_id)
        scope = expected.get("scope", "injected_model" if expected["parent_id"].startswith("model.") else (
            "filesystem" if expected["parent_id"].startswith("fixture.") else "actual_tcp"))
        require(row.get("scope") == scope, "subcase scope differs: " + case_id)
        require(row.get("expected_reason") == expected["expected_reason"] and
                row.get("actual_reason") == expected["expected_reason"], "subcase reason differs: " + case_id)
        for field in PROGRESS_FIELDS:
            target = expected_counter(expected["expected_" + field], symbols)
            required_type = bool if field == "published" else int
            require(type(target) is required_type and (field == "published" or target >= 0),
                    "invalid frozen subcase counter: " + case_id)
            require(type(row.get(field)) is required_type and row[field] == target and
                    type(row.get("expected_" + field)) is required_type and row["expected_" + field] == target,
                    "subcase progress/publication/resource differs: " + case_id + ": " + field)
    require(seen == set(expected_rows), "missing subcase ID")


def validate_smoke(report, frozen_contract, diagnostic):
    require(isinstance(report, dict) and report.get("schema_version") == frozen_contract["schema_version"]
            and report.get("contract_version") == frozen_contract["contract_version"],
            "smoke contract identity differs")
    require(report.get("command") == "smoke" and report.get("status") == "passed" and
            report.get("diagnostic") is diagnostic and report.get("timing_evidence") is False,
            "smoke status/profile/scope differs")
    expected = frozen_contract["smoke"]
    case_ids = report.get("case_ids", [])
    require(type(report.get("checks")) is int and report["checks"] == expected["checks"] and
            isinstance(case_ids, list) and len(case_ids) == len(set(case_ids)) and
            set(case_ids) == set(expected["case_ids"]), "missing/duplicate/extra smoke case IDs")
    cases = report.get("cases")
    require(isinstance(cases, list) and len(cases) == len(case_ids), "incomplete smoke case details")
    seen = set()
    for row in cases:
        require(isinstance(row, dict) and isinstance(row.get("id"), str) and
                row["id"] in case_ids and row["id"] not in seen, "invalid/duplicate smoke row")
        case_id = row["id"]
        seen.add(case_id)
        scope = "injected_model" if case_id in expected["model_case_ids"] else (
            "filesystem" if case_id.startswith("fixture.") else "actual_tcp")
        require(row.get("scope") == scope, "model/native scope mismatch: " + case_id)
        if "subcases" in expected:
            count = sum(child["parent_id"] == case_id for child in expected["subcases"])
            require(set(row) == {"id", "scope", "status", "subcase_count"} and row.get("status") == "passed"
                    and type(row.get("subcase_count")) is int and row["subcase_count"] == count,
                    "group status/subcase count differs: " + case_id)
        else:
            require(isinstance(row.get("reason"), str) and bool(row["reason"]), "missing case reason")
            if "expected_reasons" in expected:
                require(row["reason"] == expected["expected_reasons"][case_id], "case reason mismatch: " + case_id)
            require(row.get("status", "passed") == "passed", "case did not pass: " + case_id)
            for field in ("read_bytes", "written_bytes", "receive_allocations"):
                require(type(row.get(field)) is int and row[field] >= 0, "invalid progress counter: " + case_id)
            require(type(row.get("published")) is bool, "invalid publication field: " + case_id)
    require(seen == set(case_ids), "case details differ from case ID list")
    validate_subcases(report, expected)


def validate_smoke_files(report, cases):
    for field, name in (("fixture_frame_bytes", "input.wire"), ("backpressure_frame_bytes", "backpressure.wire")):
        path = Path(cases) / name
        require(path.is_file() and not path.is_symlink() and type(report.get(field)) is int and
                report[field] == path.stat().st_size, "reported frame size differs from frozen file: " + field)
    require(type(report.get("invalid_graph_frame_bytes")) is int and
            report["invalid_graph_frame_bytes"] == report["fixture_frame_bytes"],
            "invalid graph mutation must retain input frame size")


def parse_binary_stdout(raw, frozen_contract, command, diagnostic, returncode):
    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, "duplicate binary JSON field: " + key)
            result[key] = value
        return result

    def reject_constant(value):
        raise ValueError("non-JSON binary numeric constant: " + value)

    report = json.loads(raw, object_pairs_hook=unique_object, parse_constant=reject_constant)
    require(isinstance(report, dict) and type(report.get("schema_version")) is int and
            report["schema_version"] == frozen_contract["schema_version"] and
            report.get("contract_version") == frozen_contract["contract_version"] and
            report.get("command") == command and report.get("diagnostic") is diagnostic and
            report.get("timing_evidence") is False, "binary stdout contract/profile differs")
    require(report.get("status") == ("passed" if returncode == 0 else "failed"),
            "binary stdout status differs from exit code")
    if returncode != 0:
        require(all(isinstance(report.get(key), str) and report[key] for key in ("reason", "error")),
                "binary failure stdout missing reason/error")
    return report


def finish(out, record, kind):
    record.update(status="passed", kind=kind, finished_at=now(), timing_evidence=False)
    inventory = files(out)
    inventory.pop("result.json", None)
    inventory.pop("COMPLETE", None)
    record["files"] = inventory
    write(out / "result.json", record)
    (out / "COMPLETE").write_text(sha(out / "result.json") + "\n", encoding="ascii")


def check_profile(record):
    profile = record.get("measurement_profile")
    require(profile in ("diagnostic", "timing"), "unknown build profile")
    command = record["commands"][0]["argv"]
    require(all(isinstance(arg, str) for arg in command), "invalid compiler argument")
    flags = []
    for index, arg in enumerate(command):
        if arg in ("-D", "-U"):
            if index + 1 < len(command) and command[index + 1].split("=", 1)[0] == "N7_NET_DIAGNOSTICS":
                flags.append(arg + " " + command[index + 1])
        elif arg.startswith(("-D", "-U")) and arg[2:].split("=", 1)[0] == "N7_NET_DIAGNOSTICS":
            flags.append(arg)
    require(flags == ["-DN7_NET_DIAGNOSTICS=" + ("1" if profile == "diagnostic" else "0")],
            "profile differs from compiler flags")
    for prefix, expected in (("-O", "-O2"), ("-std=", "-std=c++26"), ("-stdlib=", "-stdlib=libc++")):
        require([arg for arg in command if arg.startswith(prefix)] == [expected],
                "compiler option missing/overridden: " + prefix)
    for flag in ("-freflection", "-fexpansion-statements", "-pthread"):
        require(command.count(flag) == 1, "frozen compiler flag missing/duplicated: " + flag)
    require("-fno-reflection" not in command and "-fno-expansion-statements" not in command,
            "reflection profile overridden")
    sanitizer = record.get("sanitizer")
    require(sanitizer in ("none", "address-undefined"), "unknown sanitizer profile")
    sanitize_flags = [arg for arg in command if arg.startswith(("-fsanitize=", "-fno-sanitize"))]
    require(sanitize_flags == ([] if sanitizer == "none" else ["-fsanitize=address,undefined"]),
            "sanitizer differs from compiler flags")


def recorded_absolute(value, label):
    require(isinstance(value, str) and bool(value), "missing recorded path: " + label)
    path = Path(value)
    require(path.is_absolute() and ".." not in path.parts and str(path) == value,
            "noncanonical recorded path: " + label)
    return path


def validate_sanitizer_resource(identity, sanitizer):
    if identity is None:
        return
    require(sanitizer == "address-undefined" and isinstance(identity, dict),
            "explicit sanitizer resource requires address-undefined profile")
    recorded_absolute(identity.get("root"), "sanitizer_resource.root")
    require(set(identity.get("runtime", {})) == set(SANITIZER_RUNTIMES), "sanitizer runtime inventory differs")
    headers = identity.get("builtin_headers")
    require(isinstance(headers, dict) and bool(headers), "sanitizer resource builtin headers unavailable")
    require(all(name.startswith("include/") for name in headers), "invalid builtin header inventory")
    for name, digest in {**identity["runtime"], **headers}.items():
        safe_relative(name)
        require(isinstance(digest, str) and len(digest) == 64 and all(c in "0123456789abcdef" for c in digest),
                "invalid sanitizer resource digest")


def capture_sanitizer_resource(root):
    root = Path(os.path.abspath(root))
    include = root / "include"
    require(include.is_dir(), "sanitizer resource builtin include directory unavailable")
    for name in SANITIZER_RUNTIMES:
        require((root / name).is_file(), "sanitizer prerequisite runtime unavailable: " + str(root / name))
    identity = {"root": str(root), "runtime": {name: sha(root / name) for name in SANITIZER_RUNTIMES},
                "builtin_headers": {path.relative_to(root).as_posix(): sha(path)
                                    for path in sorted(include.rglob("*")) if path.is_file()}}
    validate_sanitizer_resource(identity, "address-undefined")
    return identity


def check_sanitizer_resource_live(identity):
    if identity is not None:
        require(capture_sanitizer_resource(identity["root"]) == identity,
                "sanitizer runtime/builtin headers changed or unavailable")


def compiler_argv(build_root, measurement_profile, sanitizer, toolchain, dependency_root):
    """Reconstruct the exact frozen invocation using historical absolute paths.

    These paths describe the original build, never the archive's current
    location. Offline audit reads no original source, dependency or toolchain.
    A relocated build's source/ hashes are still checked against its inventory.
    """
    root = recorded_absolute(build_root, "build_root")
    compiler = recorded_absolute(toolchain.get("compiler"), "compiler")
    sdk = recorded_absolute(toolchain.get("sdk"), "sdk")
    library = recorded_absolute(toolchain.get("library"), "library")
    deps = recorded_absolute(dependency_root, "dependencies.root")
    require(measurement_profile in ("timing", "diagnostic") and sanitizer in ("none", "address-undefined"),
            "unsupported compiler profile")
    resource = toolchain.get("sanitizer_resource")
    validate_sanitizer_resource(resource, sanitizer)
    argv = [str(compiler), "-O2", "-std=c++26", "-freflection", "-fexpansion-statements", "-stdlib=libc++",
            "-DN7_NET_DIAGNOSTICS=" + ("1" if measurement_profile == "diagnostic" else "0"),
            "-pthread", "-isysroot", str(sdk), "-mmacosx-version-min=10.15", "-I", str(root / "source")]
    if sanitizer == "address-undefined":
        argv += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g"]
        if resource is not None:
            argv += ["-resource-dir", resource["root"]]
    includes = [deps / "boost/libs" / name / "include" for name in BOOST]
    includes.append(deps / "typelayout/include")
    for directory in includes:
        argv += ["-isystem", str(directory)]
    argv += [str(root / "source/net_reference.cpp"), "-L" + str(library), "-Wl,-rpath," + str(library),
             "-o", str(root / "net_reference")]
    return argv


def validate_receipt(command, log_name, out):
    require(isinstance(command, dict) and command.get("returncode") == 0 and
            isinstance(command.get("argv"), list) and command["argv"], "missing/failed execution receipt")
    permission = command.get("coordination", {})
    metadata = permission.get("lock_metadata", {})
    require(permission.get("explicit_flag") is True and permission.get("lock_was_held") is True and
            metadata.get("status") == "running" and metadata.get("owner") in ("W1", "W3", "W4") and
            type(metadata.get("supervisor_pid")) is int and
            metadata["supervisor_pid"] in permission.get("ancestor_pids", []),
            "missing queue ownership evidence")
    require((out / (log_name + ".stdout")).is_file() and (out / (log_name + ".stderr")).is_file(),
            "missing command logs")


def validate_toolchain(out, result):
    lock = read(out / "source/toolchains/vm-validation.json")
    profile = lock["profiles"]["macos-p2996"]
    toolchain, dependencies = read(out / "toolchain.json"), read(out / "dependencies.json")
    require(toolchain.get("profile") == "macos-p2996" and toolchain.get("host_system") == "Darwin" and
            toolchain.get("compiler_sha256") == profile["compiler_binary_sha256_observed"] and
            profile["compiler_commit"] in toolchain.get("version", "") and
            "Target: " + profile["target"] in toolchain.get("version", "") and
            Path(toolchain.get("sdk", "")).name == profile["sdk"], "toolchain declaration differs")
    runtime = toolchain.get("runtime", {})
    require(isinstance(runtime, dict), "invalid runtime declaration")
    expected_runtime = {name: digest for name, digest in profile["runtime_sha256_observed"].items() if "tsan" not in name}
    library = recorded_absolute(toolchain.get("library"), "library")
    require(runtime == {str(library / name): digest for name, digest in expected_runtime.items()},
            "runtime declaration differs")
    require(dependencies.get("commits") == lock["dependencies"] and isinstance(dependencies.get("files"), dict),
            "dependency declaration differs")
    for name, digest in dependencies["files"].items():
        safe_relative(name)
        require(isinstance(digest, str) and len(digest) == 64, "dependency digest invalid")
    argv = result["commands"][0]["argv"]
    expected_argv = compiler_argv(result.get("build_root"), result["measurement_profile"], result["sanitizer"],
                                  toolchain, dependencies.get("root"))
    require(argv == expected_argv, "compiler invocation differs from frozen flags/source/output")


def audit(out, kind=None):
    out = Path(out)
    result = read(out / "result.json")
    require(result.get("status") == "passed" and result.get("schema_version") == 1,
            "result did not pass/unsupported schema")
    require((out / "COMPLETE").read_text(encoding="ascii").strip() == sha(out / "result.json"),
            "completion identity mismatch")
    require(result.get("kind") in ("prepared", "build", "smoke") and
            result.get("timing_evidence") is False, "unknown kind or invalid timing claim")
    if kind:
        require(result["kind"] == kind, "wrong artifact kind")
    check_files(out, result["files"])
    actual = files(out)
    actual.pop("result.json", None)
    actual.pop("COMPLETE", None)
    require(actual == result["files"], "evidence inventory differs")
    if result["kind"] in ("prepared", "build"):
        require(result.get("baseline_headers") == BASELINE, "baseline declaration differs")
        source_files = files(out / "source")
        require(set(source_files) == SOURCE_FILES and source_files == read(out / "source-inventory.json"),
                "frozen source inventory differs")
        for name, digest in BASELINE.items():
            require(source_files[name] == digest, "artifact is not accepted F")
        require(source_files["run_network.py"] == result["executed_runner_sha256"], "runner differs from frozen source")
        frozen_contract = contract(out / "source/contract.json")
        require(source_files["contract.json"] == result["contract_sha256"] and
                frozen_contract["contract_version"] == result["contract_version"], "contract identity differs")
    if result["kind"] == "build":
        commands = result.get("commands", [])
        require(len(commands) == 1, "build requires one compile receipt")
        validate_receipt(commands[0], "compile", out)
        check_profile(result)
        parent = read(out / "prepared-result.json")
        require(sha(out / "prepared-result.json") == result.get("prepared_result_sha256") and
                parent.get("status") == "passed" and parent.get("kind") == "prepared" and
                parent.get("baseline_headers") == BASELINE, "prepared identity differs")
        check_files(out, parent["files"])
        require(parent["contract_sha256"] == result["contract_sha256"] and
                parent["executed_runner_sha256"] == result["executed_runner_sha256"], "prepared lineage differs")
        require(result["binary_sha256"] == sha(out / "net_reference"), "binary identity differs")
        validate_toolchain(out, result)
    if result["kind"] == "smoke":
        build_result = audit(out / "build", "build")
        require(sha(out / "build/result.json") == result.get("build_result_sha256") and
                build_result["binary_sha256"] == result.get("binary_sha256") and
                build_result["executed_runner_sha256"] == result.get("executed_runner_sha256"),
                "smoke build identity differs")
        commands = result.get("commands", [])
        require(len(commands) == 1, "smoke requires one execution receipt")
        validate_receipt(commands[0], "smoke", out)
        argv = commands[0]["argv"]
        executed_build = recorded_absolute(result.get("executed_build_root"), "executed_build_root")
        smoke_root = recorded_absolute(result.get("smoke_root"), "smoke_root")
        require(argv == [str(executed_build / "net_reference"), "smoke", "--out-dir", str(smoke_root / "cases")],
                "smoke invocation differs from historical build/output roots")
        require(result.get("executed_binary_sha256_before") == build_result["binary_sha256"] and
                result.get("executed_binary_sha256_after") == build_result["binary_sha256"],
                "executed binary before/after differs from copied build")
        report = read(out / "smoke.json")
        frozen_contract = contract(out / "build/source/contract.json")
        diagnostic = build_result["measurement_profile"] == "diagnostic"
        require(parse_binary_stdout((out / "smoke.stdout").read_text(), frozen_contract, "smoke", diagnostic, 0) == report,
                "parsed report differs from stdout")
        validate_smoke(report, frozen_contract, diagnostic)
        if "subcases" in frozen_contract["smoke"]:
            validate_smoke_files(report, out / "cases")
    return result


def coordinated(args):
    import fcntl
    require(args.coordinated_execution, "build/smoke require run_job.py and --coordinated-execution")
    path = args.queue_lock.absolute()
    require(path.is_file() and not path.is_symlink(), "shared queue lock unavailable")
    fd = os.open(path, os.O_RDWR)
    try:
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            pass
        else:
            fcntl.flock(fd, fcntl.LOCK_UN)
            raise ValueError("shared queue is idle; invoke through run_job.py")
        metadata = json.loads(os.read(fd, 65536))
    finally:
        os.close(fd)
    require(metadata.get("status") == "running" and metadata.get("owner") in ("W1", "W3", "W4"),
            "queue is not owned by an allowed network executor")
    ancestors, pid = [], os.getpid()
    while pid > 1 and pid not in ancestors:
        ancestors.append(pid)
        probe = subprocess.run(["ps", "-o", "ppid=", "-p", str(pid)], capture_output=True,
                               text=True, timeout=5, check=True)
        pid = int(probe.stdout.strip())
    require(metadata.get("supervisor_pid") in ancestors, "held queue belongs to another process tree")
    return {"explicit_flag": True, "lock_was_held": True, "lock_path": str(path),
            "lock_metadata": metadata, "ancestor_pids": ancestors, "checked_at": now()}


def execute(args, out, record, argv, name, timeout, binary_contract=None, binary_diagnostic=None):
    permission = coordinated(args)
    command = {"argv": [str(value) for value in argv], "started_at": now(), "coordination": permission}
    stdout = stderr = ""
    try:
        process = subprocess.run(command["argv"], capture_output=True, text=True, timeout=timeout)
        command["returncode"] = process.returncode
        stdout, stderr = process.stdout, process.stderr
    except subprocess.TimeoutExpired as error:
        command["timeout_seconds"] = timeout
        def decode(value):
            return value.decode(errors="replace") if isinstance(value, bytes) else value or ""
        stdout, stderr = decode(error.stdout), decode(error.stderr)
        raise
    finally:
        command["finished_at"] = now()
        (out / (name + ".stdout")).write_text(stdout)
        (out / (name + ".stderr")).write_text(stderr)
        record.setdefault("commands", []).append(command)
        write(out / "result.json", record)
    if binary_contract is not None:
        parse_binary_stdout(stdout, binary_contract, name, binary_diagnostic, process.returncode)
        command["binary_stdout_contract_validated"] = True
        write(out / "result.json", record)
    require(process.returncode == 0, name + " failed; see " + str(out / (name + ".stderr")))
    return stdout


def prepare(args, out, record):
    root = args.source_root.absolute()
    network = root / "experiments/vm_network"
    sources = {name: root / name for name in (*BASELINE, "LICENSE", "toolchains/vm-validation.json")}
    sources.update({name: network / name for name in NETWORK_FILES})
    for name, path in sources.items():
        require(path.is_file() and not path.is_symlink(), "source unavailable: " + str(path))
    before = {name: sha(path) for name, path in sources.items()}
    for name, digest in BASELINE.items():
        require(before[name] == digest, "current source is not accepted F: " + name)
    frozen_contract = contract(network / "contract.json")
    lock = read(root / "toolchains/vm-validation.json")
    require(lock["baseline"]["headers"] == BASELINE, "toolchain baseline differs")
    for name, source in sources.items():
        target = out / "source" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    require(files(out / "source") == before and before == {name: sha(path) for name, path in sources.items()},
            "source changed during preparation")
    require(before["run_network.py"] == record["executed_runner_sha256"], "executed runner differs from source")
    write(out / "source-inventory.json", before)
    record.update(baseline_headers=BASELINE, contract_sha256=before["contract.json"],
                  contract_version=frozen_contract["contract_version"], source_root=str(root))
    finish(out, record, "prepared")


def capture_dependencies(root, lock):
    include_dirs = [root / "boost/libs" / name / "include" for name in BOOST]
    include_dirs.append(root / "typelayout/include")
    commits = {}
    for name, expected in lock["dependencies"].items():
        prefix = ["git", "-C", str(root / name)]
        commit = subprocess.check_output(prefix + ["rev-parse", "HEAD"], text=True).strip()
        dirty = subprocess.check_output(prefix + ["status", "--porcelain"], text=True).strip()
        require(commit == expected and not dirty, "dependency revision/clean state mismatch: " + name)
        commits[name] = commit
    content = {}
    for directory in include_dirs:
        require(directory.is_dir(), "dependency include missing: " + str(directory))
        for path in sorted(directory.rglob("*")):
            if path.is_file():
                content[path.relative_to(root).as_posix()] = sha(path)
    return include_dirs, {"root": str(root), "commits": commits, "files": content}


def build(args, out, record):
    coordinated(args)
    resource_dir = args.sanitizer_resource_dir
    require(resource_dir is None or args.sanitizer == "address-undefined",
            "--sanitizer-resource-dir requires --sanitizer address-undefined")
    parent = audit(args.prepared, "prepared")
    require(parent["executed_runner_sha256"] == record["executed_runner_sha256"],
            "runner changed since prepare; execute frozen source/run_network.py")
    shutil.copytree(args.prepared / "source", out / "source")
    shutil.copy2(args.prepared / "source-inventory.json", out / "source-inventory.json")
    shutil.copy2(args.prepared / "result.json", out / "prepared-result.json")
    lock = read(out / "source/toolchains/vm-validation.json")
    require(platform.system() == "Darwin", "initial build profile requires native macOS P2996")
    compiler, sdk, library = (Path(os.path.abspath(path)) for path in (args.compiler, args.sdk, args.library))
    dependency_root = Path(os.path.abspath(args.deps))
    profile = lock["profiles"]["macos-p2996"]
    version = subprocess.check_output([str(compiler), "--version"], text=True, timeout=15)
    require(profile["compiler_commit"] in version and "Target: " + profile["target"] in version,
            "compiler profile mismatch")
    require(sha(compiler) == profile["compiler_binary_sha256_observed"], "compiler binary differs from F profile")
    require(sdk.name == profile["sdk"] and sdk.is_dir(), "explicit SDK mismatch")
    require(str(read(sdk / "SDKSettings.json")["Version"]) == profile["sdk"].removeprefix("MacOSX").removesuffix(".sdk"),
            "SDK content version mismatch")
    _, dependencies = capture_dependencies(dependency_root, lock)
    toolchain = {"profile": "macos-p2996", "host_system": platform.system(),
                 "host_machine": platform.machine(), "compiler": str(compiler),
                 "compiler_sha256": sha(compiler), "version": version, "sdk": str(sdk), "library": str(library),
                 "sdk_settings_sha256": sha(sdk / "SDKSettings.json"), "runtime": {}}
    toolchain["sanitizer_resource"] = capture_sanitizer_resource(resource_dir) if resource_dir is not None else None
    for name, expected in profile["runtime_sha256_observed"].items():
        if "tsan" in name:
            continue
        path = library / name
        require(sha(path) == expected, "runtime differs from F profile")
        toolchain["runtime"][str(path)] = expected
    record.update(measurement_profile=args.profile, sanitizer=args.sanitizer, build_root=str(out),
                  baseline_headers=BASELINE, contract_sha256=parent["contract_sha256"],
                  contract_version=parent["contract_version"], prepared_result_sha256=sha(args.prepared / "result.json"))
    write(out / "dependencies.json", dependencies)
    write(out / "toolchain.json", toolchain)
    argv = compiler_argv(record["build_root"], args.profile, args.sanitizer, toolchain, dependencies["root"])
    execute(args, out, record, argv, "compile", args.timeout_seconds)
    check_files(dependency_root, dependencies["files"])
    _, after_dependencies = capture_dependencies(dependency_root, lock)
    require(after_dependencies == dependencies, "dependencies changed during build")
    require(sha(compiler) == toolchain["compiler_sha256"] and sha(sdk / "SDKSettings.json") == toolchain["sdk_settings_sha256"],
            "toolchain changed during build")
    for path, digest in toolchain["runtime"].items():
        require(sha(path) == digest, "runtime changed during build")
    check_sanitizer_resource_live(toolchain["sanitizer_resource"])
    audit(args.prepared, "prepared")
    record["binary_sha256"] = sha(out / "net_reference")
    finish(out, record, "build")


def smoke(args, out, record):
    coordinated(args)
    parent = audit(args.build, "build")
    require(parent["executed_runner_sha256"] == record["executed_runner_sha256"],
            "runner changed since build; execute frozen source/run_network.py")
    # A copied build keeps the evidence portable; execute the original binary
    # to preserve its recorded path, then prove both copies retain its identity.
    shutil.copytree(args.build, out / "build")
    executed_build = Path(os.path.abspath(args.build))
    before = sha(executed_build / "net_reference")
    require(before == parent["binary_sha256"] == sha(out / "build/net_reference"),
            "runtime binary differs from copied frozen build before smoke")
    record.update(executed_build_root=str(executed_build), smoke_root=str(out), executed_binary_sha256_before=before)
    toolchain = read(args.build / "toolchain.json")
    for path, digest in toolchain["runtime"].items():
        require(sha(path) == digest, "runtime changed before smoke")
    check_sanitizer_resource_live(toolchain.get("sanitizer_resource"))
    frozen_contract = contract(out / "build/source/contract.json")
    diagnostic = parent["measurement_profile"] == "diagnostic"
    raw = execute(args, out, record, [executed_build / "net_reference", "smoke", "--out-dir", out / "cases"],
                  "smoke", args.timeout_seconds, binary_contract=frozen_contract, binary_diagnostic=diagnostic)
    report = parse_binary_stdout(raw, frozen_contract, "smoke", diagnostic, 0)
    validate_smoke(report, frozen_contract, diagnostic)
    if "subcases" in frozen_contract["smoke"]:
        validate_smoke_files(report, out / "cases")
    write(out / "smoke.json", report)
    audit(args.build, "build")
    after = sha(executed_build / "net_reference")
    require(after == before == sha(out / "build/net_reference"), "runtime binary changed during smoke")
    for path, digest in toolchain["runtime"].items():
        require(sha(path) == digest, "runtime changed during smoke")
    check_sanitizer_resource_live(toolchain.get("sanitizer_resource"))
    record.update(build_result_sha256=sha(out / "build/result.json"), binary_sha256=parent["binary_sha256"],
                  executed_binary_sha256_after=after,
                  measurement_profile=parent["measurement_profile"], sanitizer=parent["sanitizer"],
                  contract_sha256=parent["contract_sha256"], contract_version=parent["contract_version"])
    finish(out, record, "smoke")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    p = sub.add_parser("prepare", help="offline snapshot of current hash-validated F and network sources")
    p.add_argument("--source-root", type=Path, default=ROOT)
    p.add_argument("--out", type=Path, required=True)
    p = sub.add_parser("build")
    p.add_argument("--prepared", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--profile", choices=("diagnostic", "timing"), required=True)
    p.add_argument("--sanitizer", choices=("none", "address-undefined"), default="none")
    p.add_argument("--sanitizer-resource-dir", type=Path,
                   help="explicit external compiler-rt resource directory; address-undefined validation only")
    p.add_argument("--deps", type=Path, default=ROOT / "external")
    p.add_argument("--compiler", default="/usr/local/bin/clang++")
    p.add_argument("--sdk", type=Path, default=Path("/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk"))
    p.add_argument("--library", type=Path, default=Path("/usr/local/lib"))
    p.add_argument("--timeout-seconds", type=positive, default=180)
    p.add_argument("--coordinated-execution", action="store_true")
    p.add_argument("--queue-lock", type=Path, default=ROOT / "build/vm-coordination/queue.lock")
    p = sub.add_parser("smoke")
    p.add_argument("--build", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--timeout-seconds", type=positive, default=120)
    p.add_argument("--coordinated-execution", action="store_true")
    p.add_argument("--queue-lock", type=Path, default=ROOT / "build/vm-coordination/queue.lock")
    p = sub.add_parser("audit", help="read-only archive audit; does not inspect original source/toolchain paths")
    p.add_argument("path", type=Path)
    args = parser.parse_args(argv)
    if args.action == "build":
        require(args.sanitizer_resource_dir is None or args.sanitizer == "address-undefined",
                "--sanitizer-resource-dir requires --sanitizer address-undefined")
    if args.action == "audit":
        result = audit(args.path)
        print(json.dumps({"status": "passed", "kind": result["kind"], "files": len(result["files"])}))
        return 0
    if args.action in ("build", "smoke"):
        coordinated(args)
    out = fresh(args.out)
    record = {"schema_version": 1, "status": "running", "started_at": now(), "action": args.action,
              "timing_evidence": False, "executed_runner_sha256": sha(__file__)}
    write(out / "result.json", record)
    try:
        globals()[args.action](args, out, record)
        require(sha(__file__) == record["executed_runner_sha256"], "executed runner changed during action")
        audit(out, args.action if args.action != "prepare" else "prepared")
    except BaseException as error:
        record.update(status="failed", error=type(error).__name__ + ": " + str(error), finished_at=now())
        write(out / "result.json", record)
        (out / "COMPLETE").unlink(missing_ok=True)
        raise
    print(json.dumps({"status": "passed", "kind": record["kind"], "out": str(out)}))
    return 0


def positive(value):
    import math
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise argparse.ArgumentTypeError("must be finite and positive")
    return number


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print(json.dumps({"status": "failed", "reason": type(error).__name__, "error": str(error)}), file=sys.stderr)
        sys.exit(2)
