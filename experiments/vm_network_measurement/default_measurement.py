#!/usr/bin/env python3
"""Exact historical D/F bridge. Native work requires the accepted outer queue.

Calibration is disabled without a separately supplied parent approval bound to
this bridge, the proposed contract, the preregistration, and reviewed artifacts.
Offline audit executes no producer and does not consult original absolute paths.
"""
from __future__ import annotations

import argparse
import datetime as dt
import fcntl
import hashlib
import math
import os
from pathlib import Path
import shutil
import statistics
import struct
import subprocess
import sys
import time

sys.dont_write_bytecode = True
import paired_measurement as P
import prepare_default as S

ROOT = Path(__file__).resolve().parents[2]
ACCEPTED_LOCK_DIR = "/Users/fanchensu/XOffsetDatastructure/build/vm-coordination"
QUEUE_SHA = "6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909"
PARENT = "01a130de-d9f3-7501-9d1e-8e84a403f430"
CASES = (
    dict(id="save_first_64m", workload="save", size=67108864, cycles=1, metrics=["first_ms"]),
    dict(id="save_repeated_64m", workload="save", size=67108864, cycles=4, metrics=["first_ms", "repeated_mean_ms"]),
    dict(id="vector_tail_1m", workload="vector_tail", size=1000000, cycles=1, metrics=["elapsed_ms"]),
)
POLICY = dict(noise_relative_limit=.01, dispersion_relative_limit=.01,
              drift_relative_limit=.01,
              target_regression_resolution=.02, calibration_pairs_per_anchor=4,
              anchors=["pre", "mid", "post"], warmup_pairs_per_case=1,
              max_processes=78, child_timeout_seconds=30, max_window_seconds=900,
              max_retained_wire_bytes=6 * 1024**3, retry_failed_calibration=False,
              required_independent_confirmation_windows=3, minimum_window_gap_seconds=60)
IDENTITY = ("semantic_checksum", "request_count", "size", "requested_cycles", "used_end", "wire_size",
            "final_logical_bytes", "final_accessible_bytes", "initial_logical_bytes", "initial_reserved_bytes",
            "container_moves", "container_copy_bytes_est", "allocation_failures", "growth_count")
INTEGER_FIELDS = ("size requested_reserve_bytes requested_cycles initial_logical_bytes initial_reserved_bytes "
    "final_logical_bytes final_reserved_bytes final_accessible_bytes final_reserved_mapping_bytes "
    "total_reserved_mapping_bytes total_logical_bytes vm_enable_calls_est vm_enabled_bytes_est "
    "total_requested_payload_bytes setup_pretouched_bytes operation_count growth_count address_changes "
    "relocation_copy_bytes_est newly_exposed_bytes zeroed_new_bytes allocation_failures checksum wire_size "
    "wire_reserved_bytes improvement_schema_version threads semantic_checksum wire_checksum request_count "
    "container_moves container_copy_bytes_est used_end free_bytes cached_free_bytes live_payload_bytes").split()
FLOAT_FIELDS = ("elapsed_ms operation_p50_ms operation_p95_ms operation_p99_ms operation_max_ms growth_max_ms "
                "create_ms allocate_ms free_ms destroy_ms first_ms repeated_ms repeated_mean_ms").split()
MEMORY_FIELDS = ("memory_before memory_after memory_live memory_peak memory_after_validation memory_after_cleanup").split()
STRING_FIELDS = ("variant workload policy operation_unit timing_scope order owner_probe_note").split()
RAW_FIELDS = set(INTEGER_FIELDS + FLOAT_FIELDS + MEMORY_FIELDS + STRING_FIELDS +
                 ["schema_version", "save_base_changed", "operation_samples_ms", "growth_samples_ms", "owner_probe_count"])


def contract_path():
    return Path(__file__).with_name("default-contract.json")


def copy_file(source, target):
    target = Path(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    with Path(source).open("rb") as src, target.open("xb") as dst:
        shutil.copyfileobj(src, dst)


def file_id(path):
    path = Path(path)
    P.require(path.is_file(), "missing identity file: " + str(path))
    return dict(sha256=P.sha(path), bytes=path.stat().st_size)


def equal(left, right):
    return P.canonical(left) == P.canonical(right)


def require_passed_artifact(path, kind):
    result = audit(path)
    P.require(result.get("status") == "passed" and result.get("kind") == kind, "required successful artifact: " + kind)
    return result


def environment(path):
    result = dict(utc=dt.datetime.now(dt.timezone.utc).isoformat(),
                  client_time=dt.datetime.now(dt.timezone(dt.timedelta(hours=8))).isoformat(),
                  client_timezone="Asia/Shanghai", platform=sys.platform, uname=list(os.uname()))
    for key, probe in (("load_average", lambda: list(os.getloadavg())),
                       ("disk", lambda: dict(zip(("total_bytes", "used_bytes", "free_bytes"), shutil.disk_usage(path))))):
        try:
            result[key] = dict(status="available", value=probe())
        except (OSError, AttributeError) as error:
            result[key] = dict(status="unavailable", error=str(error))
    def command_probe(argv):
        try:
            process = subprocess.run(argv, capture_output=True, timeout=5)
            return dict(status="available" if process.returncode == 0 else "error", argv=argv, returncode=process.returncode,
                stdout=process.stdout.decode("utf-8", errors="replace"), stderr=process.stderr.decode("utf-8", errors="replace"))
        except (OSError, subprocess.TimeoutExpired) as error:
            return dict(status="unavailable", argv=argv, error=str(error))
    result["process_snapshot"] = command_probe(["ps", "-axo", "pid,comm,%cpu,%mem"])
    result["cpu_logical_count"] = os.cpu_count()
    if sys.platform == "darwin":
        result["cpu_topology"] = command_probe(["sysctl", "-n", "machdep.cpu.brand_string", "hw.physicalcpu", "hw.logicalcpu", "hw.memsize"])
        result["power_source"] = command_probe(["pmset", "-g", "batt"])
        result["thermal"] = command_probe(["pmset", "-g", "therm"])
    else:
        for key in ("cpu_topology", "power_source", "thermal"):
            result[key] = dict(status="unavailable", error="Darwin sysctl/pmset probes are not applicable to this platform")
    return result


def audit_environment(root):
    records = [P.read(P.safe_file(root, "environment-" + phase + ".json")) for phase in ("before", "after")]
    for record in records:
        P.require(record.get("client_timezone") == "Asia/Shanghai" and isinstance(record.get("platform"), str)
                  and isinstance(record.get("uname"), list) and len(record["uname"]) == 5
                  and all(isinstance(v, str) for v in record["uname"]), "environment identity missing")
        P.require(dt.datetime.fromisoformat(record["utc"]).utcoffset() == dt.timedelta(0)
                  and dt.datetime.fromisoformat(record["client_time"]).utcoffset() == dt.timedelta(hours=8), "environment timezone differs")
        for key in ("load_average", "disk", "process_snapshot", "cpu_topology", "power_source", "thermal"):
            probe = record[key]
            P.require(probe.get("status") in ("available", "unavailable", "error"), "missing environment probe state")
            if probe["status"] == "unavailable":
                P.require(isinstance(probe.get("error"), str) and probe["error"], "unavailable environment probe lacks reason")
        if record["load_average"]["status"] == "available":
            values = record["load_average"]["value"]
            P.require(isinstance(values, list) and len(values) == 3 and all(number(v) for v in values), "invalid load snapshot")
        if record["disk"]["status"] == "available":
            P.require(set(record["disk"]["value"]) == {"total_bytes", "used_bytes", "free_bytes"}
                      and all(P.integer(v) for v in record["disk"]["value"].values()), "invalid disk snapshot")
    P.require(dt.datetime.fromisoformat(records[0]["utc"]) <= dt.datetime.fromisoformat(records[1]["utc"]), "environment snapshots reversed")


def tree_identity(path):
    """Hash actual compiler include bytes, retaining symlink target identities."""
    path = Path(path)
    P.require(path.is_dir(), "missing include root: " + str(path))
    files = {}
    for entry in sorted(path.rglob("*")):
        if entry.is_file():
            name = entry.relative_to(path).as_posix()
            files[name] = dict(**file_id(entry), resolved_path=str(entry.resolve()))
        else:
            P.require(entry.is_dir() and not entry.is_symlink(), "unsupported include symlink directory")
    P.require(files, "empty include root")
    return dict(path=str(path), files=files, manifest_sha256=P.value_sha(files))


def runtime_identity(metadata):
    """Read-only byte identities. Invoked before/after work, never inside native timers.

    SDKSettings is an identity marker, not a claim to hash every OS/SDK file.
    Include roots hash every regular include file; no git cleanliness inference.
    """
    runtime = {}
    for name, identity in metadata["runtime_files"].items():
        path = Path(identity["path"])
        runtime[name] = dict(path=str(path), resolved_path=str(path.resolve()), **file_id(path))
    sdk = Path(metadata["sdk"]["path"])
    command = metadata["compile_commands"]["D"]
    roots = [command[i + 1] for i, token in enumerate(command) if token == "-isystem"]
    # These toolchain implicit include roots are fixed by the inherited compiler.
    compiler_root = Path(command[0]).parent.parent
    roots += [str(compiler_root / "include/c++/v1")]
    resource_roots = list((compiler_root / "lib/clang").glob("*/include"))
    P.require(len(resource_roots) == 1, "review required: ambiguous compiler resource include roots")
    roots += [str(resource_roots[0])]
    return dict(runtime_files=runtime, sdk=dict(path=str(sdk), settings=file_id(sdk / "SDKSettings.json")),
                include_roots=[tree_identity(root) for root in roots],
                platform=sys.platform, machine=os.uname().machine, page_size=os.sysconf("SC_PAGE_SIZE"),
                limitations=["SDK identity uses SDKSettings, not a complete SDK/OS content manifest",
                            "dynamic system libraries and OS implementation are host identity, not bundled runtime files"])


def compile_command(metadata, variant, source, binary):
    """Only relocate the exact historical -I, translation unit, and -o operands."""
    P.require(variant in ("D", "F"), "unknown build variant")
    old = metadata["compile_commands"][variant]
    P.require(old.count("-I") == old.count("-o") == 1, "ambiguous inherited compile command")
    command = list(old)
    include_index, output_index = old.index("-I") + 1, old.index("-o") + 1
    old_source = str(Path(old[include_index]) / "bench_vm_improvements.cpp")
    P.require(old.count(old_source) == 1, "missing inherited translation unit")
    command[include_index] = str(Path(source).absolute())
    command[old.index(old_source)] = str((Path(source) / "bench_vm_improvements.cpp").absolute())
    command[output_index] = str(Path(binary).absolute())
    return command


def freeze(prepared, out):
    S.audit(prepared)
    out = Path(out)
    out.mkdir(parents=True, exist_ok=False)
    shutil.copytree(prepared, out / "prepared")
    metadata = P.read(out / "prepared/historical-metadata.json")
    P.require(equal(metadata["cases"], list(CASES)), "different historical default cases")
    for path in (Path(__file__), Path(P.__file__), Path(S.__file__), contract_path(), ROOT / "experiments/vm_coordination/run_job.py"):
        copy_file(path, out / "protocol" / path.name)
    P.require(P.sha(out / "protocol/run_job.py") == QUEUE_SHA, "queue source changed")
    P.write_new(out / "runtime.json", runtime_identity(metadata))
    seal(out, "default-frozen", "completed", executed_cpp=False, performance_evidence=False)
    return audit(out)


def payload_inventory(root):
    return {name: value for name, value in P.inventory(root, ("result.json",)) .items()
            if not name.startswith("queue/")}


def seal(root, kind, status, **fields):
    P.write_new(Path(root) / "result.json", dict(schema_version=1, kind=kind, status=status,
                adoption_allowed=False, files=payload_inventory(root), **fields))


def active_queue(queue, kind):
    """Require this direct child to hold W4's real default queue, never a nested lock."""
    queue = Path(queue).resolve()
    request = P.read(queue / "request.json")
    lockdir = Path(ACCEPTED_LOCK_DIR)
    P.require(request.get("owner") == "W3" and request.get("kind") == kind
              and request.get("runner_sha256") == QUEUE_SHA
              and Path(request.get("lock_dir", "")) == lockdir
              and Path(request.get("out", "")) == queue, "wrong accepted queue request")
    command = request.get("command", [])
    expected = [str(Path(__file__).resolve()), *sys.argv[1:]]
    P.require(len(command) >= 3 and Path(shutil.which(command[0]) or command[0]).resolve() == Path(sys.executable).resolve()
              and (command[1:] == expected or command[1:] == ["-B", *expected]), "queue did not launch this exact bridge command")
    P.require(Path(request.get("cwd", "")) == Path.cwd(), "queue cwd differs")
    fd = os.open(lockdir / "queue.lock", os.O_RDONLY)
    try:
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            pass
        else:
            fcntl.flock(fd, fcntl.LOCK_UN)
            raise P.ProtocolError("queue not held")
        lock = P.parse(os.read(fd, 65536))
    finally:
        os.close(fd)
    P.require(lock.get("status") == "running" and lock.get("run_id") == request["run_id"]
              and lock.get("owner") == "W3" and lock.get("kind") == kind
              and lock.get("supervisor_pid") == os.getppid(), "not the active queue direct child")
    P.require(P.sha(ROOT / "experiments/vm_coordination/run_job.py") == QUEUE_SHA, "queue changed")
    return request


def queue_audit(queue, expected_kind):
    queue = Path(queue)
    manifest = P.read(P.safe_file(queue, "manifest.json"))
    actual = [dict(path=name, **identity) for name, identity in P.inventory(queue, ("manifest.json",)).items()]
    P.require(equal(manifest, dict(schema_version=1, files=actual)), "queue inventory differs")
    request, result = P.read(queue / "request.json"), P.read(queue / "result.json")
    P.require(type(request.get("schema_version")) is int and request["schema_version"] == 1
              and type(result.get("schema_version")) is int and result["schema_version"] == 1
              and result.get("owner") == "W3" and result.get("kind") == expected_kind
              and request.get("runner_sha256") == QUEUE_SHA and request.get("owner") == "W3"
              and request.get("kind") == expected_kind and request.get("run_id") == result.get("run_id")
              and request.get("lock_dir") == ACCEPTED_LOCK_DIR, "queue identity differs")
    P.require(result.get("status") == "complete" and type(result.get("command_returncode")) is int
              and result["command_returncode"] == 0 and result.get("started_at") and result.get("finished_at")
              and result.get("cleanup", {}).get("live") == [], "unsuccessful queue job")
    command = request.get("command", [])
    context = P.read(P.safe_file(queue.parent, "execution-context.json"))
    P.require(context.get("queue_run_id") == request["run_id"] and context.get("command") == command
              and context.get("queue_request_sha256") == P.sha(queue / "request.json")
              and context.get("bridge_sha256") == P.sha(__file__)
              and isinstance(context.get("interpreter"), dict) and P.digest(context["interpreter"].get("sha256"))
              and context.get("requested_interpreter") == command[0], "archived interpreter/bridge context differs")
    args = command[1:]
    if args[:1] == ["-B"]:
        args = args[1:]
    expected_action = {"build": "build", "functional": "functional", "timing": "calibrate"}[expected_kind]
    P.require(isinstance(command, list) and all(isinstance(a, str) and a for a in command)
              and len(args) >= 6 and args[0] == context.get("bridge_path") and Path(args[0]).is_absolute()
              and Path(args[0]).name == "default_measurement.py" and args[1] == expected_action
              and (len(args)-2) % 2 == 0 and Path(request.get("cwd", "")).is_absolute()
              and number(request.get("timeout_seconds"), True), "queue command metadata missing")
    flags = dict(zip(args[2::2], args[3::2]))
    required = {"build": {"--frozen", "--out", "--queue"}, "functional": {"--build", "--out", "--queue"},
                "timing": {"--functional", "--preregister", "--approval", "--approval-sha256", "--out", "--queue"}}[expected_kind]
    def original_absolute(value):
        return os.path.normpath(os.path.join(request["cwd"], value))
    P.require(set(flags) == required and len(flags) * 2 == len(args)-2
              and original_absolute(flags["--out"]) == str(Path(request["out"]).parent)
              and original_absolute(flags["--queue"]) == request["out"], "queue command is not bound to its original output")
    return request


def start_run(out, queue, kind):
    out, queue = Path(out).resolve(), Path(queue).resolve()
    P.require(queue == out / "queue", "queue evidence must be the new bundle's queue directory")
    request = active_queue(queue, kind)
    P.require(out.is_dir() and {p.name for p in out.iterdir()} == {"queue"}, "output must contain only active queue")
    P.write_new(out / "execution-context.json", dict(queue_run_id=request["run_id"], command=request["command"],
        queue_request_sha256=P.sha(queue / "request.json"),
        bridge_path=str(Path(__file__).resolve()), bridge_sha256=P.sha(__file__), requested_interpreter=request["command"][0],
        interpreter=dict(path=sys.executable, resolved_path=str(Path(sys.executable).resolve()), **file_id(sys.executable)),
        python_version=sys.version))
    (out / "commands").mkdir()
    return out


def execute(argv, prefix, timeout):
    """Always write raw bytes and the unsuccessful receipt before raising."""
    prefix = Path(prefix)
    start = time.monotonic_ns()
    receipt = dict(argv=list(map(str, argv)), started_monotonic_ns=start,
                   returncode=None, timed_out=False, timeout_seconds=timeout)
    with prefix.with_suffix(".stdout").open("xb") as stdout, prefix.with_suffix(".stderr").open("xb") as stderr:
        try:
            process = subprocess.run(receipt["argv"], stdout=stdout, stderr=stderr, timeout=timeout,
                                     stdin=subprocess.DEVNULL)  # Inherit the W4 owned session.
            receipt["returncode"] = process.returncode
        except subprocess.TimeoutExpired:
            receipt["timed_out"] = True
        except OSError as error:
            receipt["error"] = str(error)
        finally:
            receipt["finished_monotonic_ns"] = time.monotonic_ns()
            P.write_new(prefix.with_suffix(".receipt.json"), receipt)
    P.require(type(receipt["returncode"]) is int and receipt["returncode"] == 0 and receipt["timed_out"] is False, "child failed; raw bytes/receipt retained")
    return receipt


def build(frozen, out, queue):
    require_passed_artifact(frozen, "default-frozen")
    out = start_run(out, queue, "build")
    try:
        shutil.copytree(frozen, out / "frozen")
        meta = P.read(out / "frozen/prepared/historical-metadata.json")
        P.require(equal(runtime_identity(meta), P.read(out / "frozen/runtime.json")), "current runtime differs from freeze")
        (out / "bin").mkdir()
        for variant in ("D", "F"):  # Two sequential compilations; never more than two.
            argv = compile_command(meta, variant, out / "frozen/prepared/source" / variant, out / "bin" / variant)
            execute(argv, out / "commands" / variant, 300)
        execute([meta["compile_commands"]["D"][0], "--version"], out / "commands/compiler-version", 10)
        P.require(equal(runtime_identity(meta), P.read(out / "frozen/runtime.json")), "runtime changed while building")
        P.write_new(out / "binaries.json", {v: file_id(out / "bin" / v) for v in ("D", "F")})
        seal(out, "default-build", "completed", executed_cpp=False, compiled_cpp=True, performance_evidence=False)
    except Exception as error:
        seal(out, "default-build", "failed", error=str(error), performance_evidence=False)
        raise
    return dict(status="built_pending_queue_completion", performance_evidence=False)


def native_argv(binary, case, wire):
    return list(map(str, [binary, "--workload", case["workload"], "--size", case["size"], "--cycles", case["cycles"],
                         "--threads", 1, "--order", "sequential", "--reserve", 0, "--wire-path", wire]))


def number(value, positive=False):
    return type(value) in (int, float) and math.isfinite(value) and (value > 0 if positive else value >= 0)


def native_report(raw, case):
    row = P.parse(raw)
    P.require(isinstance(row, dict) and set(row) == RAW_FIELDS, "unexpected exact native JSON field set")
    P.require(type(row["schema_version"]) is int and row["schema_version"] == 1
              and row["improvement_schema_version"] == 1, "wrong native schema")
    P.require(all(P.integer(row[k]) for k in INTEGER_FIELDS), "invalid integer/counter")
    P.require(all(row[k] < 2**64 for k in ("semantic_checksum", "wire_checksum", "checksum")), "checksum outside uint64")
    P.require(all(number(row[k]) for k in FLOAT_FIELDS), "invalid metric")
    P.require(all(isinstance(row[k], str) and row[k] for k in STRING_FIELDS), "invalid native string")
    P.require(row["variant"] == "stable_vm" and row["workload"] == case["workload"]
              and row["size"] == case["size"] and row["requested_cycles"] == case["cycles"]
              and row["threads"] == 1 and row["order"] == "sequential" and row["requested_reserve_bytes"] == 0,
              "different native invocation")
    P.require(type(row["save_base_changed"]) is bool and row["save_base_changed"] is False
              and row["address_changes"] == 0 and row["owner_probe_count"] is None
              and row["owner_probe_note"] == "not_instrumented; scaling_and_latency_measured", "wrong movement/counter semantics")
    for key in MEMORY_FIELDS:
        value = row[key]
        P.require(isinstance(value, dict) and set(value) == {"available", "rss_bytes", "virtual_bytes", "peak_rss_bytes", "rss_source"}
                  and type(value["available"]) is bool and all(P.integer(value[k]) for k in ("rss_bytes", "virtual_bytes", "peak_rss_bytes"))
                  and isinstance(value["rss_source"], str), "invalid native memory record")
    P.require(row["memory_live"]["available"] is True, "live memory sample unavailable")
    for key in ("operation_samples_ms", "growth_samples_ms"):
        P.require(isinstance(row[key], list) and all(number(v, True) for v in row[key]), "invalid native samples")
    samples = row["operation_samples_ms"]
    used_end = 67109072 if case["workload"] == "save" else 4194544
    semantic = 15740387299717957747 if case["workload"] == "save" else 11399947380475585011
    P.require(row["used_end"] == used_end and row["wire_size"] == used_end + 88
              and row["semantic_checksum"] == semantic
              and used_end <= row["final_logical_bytes"] <= row["final_accessible_bytes"] <= row["final_reserved_bytes"]
              and row["final_reserved_bytes"] == row["initial_reserved_bytes"]
              and row["live_payload_bytes"] == (67108864 if case["workload"] == "save" else 4000000),
              "wrong deterministic semantic/layout identity")
    count = case["cycles"] if case["workload"] == "save" else math.ceil(case["size"] / 4096)
    P.require(len(samples) == row["operation_count"] == count and len(row["growth_samples_ms"]) == row["growth_count"], "sample count differs")
    P.require(math.isclose(sum(samples), row["elapsed_ms"], rel_tol=1e-9, abs_tol=1e-8), "elapsed/sample sum differs")
    if case["workload"] == "save":
        P.require(row["timing_scope"] == "save_verified_normalization_and_output; output_destruction_excluded"
                  and row["policy"] == "equal_explicit_capacity; first_and_repeated_operations_in_one_process"
                  and row["operation_unit"] == "one_save_verified" and row["request_count"] == case["cycles"]
                  and row["initial_logical_bytes"] == row["initial_reserved_bytes"] == 135266304
                  and row["setup_pretouched_bytes"] == 135266304, "different save scope/capacity")
        P.require(math.isclose(row["first_ms"], samples[0], rel_tol=1e-9, abs_tol=1e-8)
                  and math.isclose(row["repeated_ms"], sum(samples[1:]), rel_tol=1e-9, abs_tol=1e-8)
                  and math.isclose(row["repeated_mean_ms"], sum(samples[1:]) / max(1, len(samples)-1), rel_tol=1e-9, abs_tol=1e-8), "first/repeated mapping differs")
    else:
        P.require(row["timing_scope"] == "container_operations_including_relocation_and_explicit_retry; excludes_setup_validation_wire"
                  and row["policy"] == "equal_preallocated_logical_extent"
                  and row["operation_unit"] == "up_to_4096_elements; string_reserve_geometric"
                  and row["request_count"] == 1000000 and row["initial_logical_bytes"] == row["initial_reserved_bytes"] == 33554432,
                  "different vector scope/capacity")
    P.require(all(number(row[k], True) for k in case["metrics"]), "nonpositive selected metric")
    return dict(schema_version=1, kind="default-normalized-native", case_id=case["id"],
                metrics_ms={k: row[k] for k in case["metrics"]},
                work_identity={k: row[k] for k in IDENTITY},
                wire_checksum=row["wire_checksum"], wire_bytes=row["wire_size"],
                native_scope=row["timing_scope"], counter_limit="owner probe unavailable; copy fields marked est remain estimates")


def calibration_schedule():
    jobs = []
    for phase, anchor in [("warmup", "warmup"), *[("measured", a) for a in POLICY["anchors"]]]:
        cases = list(CASES)
        rotation = 0 if anchor == "warmup" else POLICY["anchors"].index(anchor)
        cases = cases[rotation:] + cases[:rotation]
        for case in cases:
            for pair in range(1 if phase == "warmup" else 4):
                order = "AB" if pair % 2 == 0 else "BA"
                for position, side in enumerate(order):
                    jobs.append(dict(id=f"j{len(jobs):03d}", phase=phase, anchor=anchor, case_id=case["id"],
                                     pair=pair, order=order, position=position, side=side, variant="F"))
    P.require(len(jobs) == POLICY["max_processes"], "wrong calibration budget")
    return jobs


def functional_schedule():
    return [dict(id=f"j{i*2+j:03d}", case_id=case["id"], variant=variant)
            for i, case in enumerate(CASES) for j, variant in enumerate(("D", "F"))]


def execute_samples(out, jobs, build_path, deadline, canonical_wires=None):
    expected = {}
    expected_wires = {}
    checksum_cache = {}
    total_bytes = 0
    for job in jobs:
        P.require(time.monotonic() < deadline, "absolute window budget expired")
        case = next(c for c in CASES if c["id"] == job["case_id"])
        prefix = out / "commands" / job["id"]
        wire = prefix.with_suffix(".wire")
        argv = native_argv(build_path / "bin" / job["variant"], case, wire)
        execute(argv, prefix, min(POLICY["child_timeout_seconds"], deadline - time.monotonic()))
        normalized = native_report(prefix.with_suffix(".stdout").read_bytes(), case)
        P.require(wire.is_file() and wire.stat().st_size == normalized["wire_bytes"], "wire output missing/size differs")
        # Native harness already computes FNV over every byte and performs full
        # value/readback checks. SHA binds retained exact bytes independently.
        normalized["wire_sha256"] = P.sha(wire)
        normalized["binary_sha256"] = P.sha(build_path / "bin" / job["variant"])
        normalized["wire_header"] = validate_wire(wire, normalized, checksum_cache, canonical_wires is None)
        P.write_new(prefix.with_suffix(".normalized.json"), normalized)
        P.require(equal(wire_identity(normalized), expected_wires.setdefault(job["case_id"], wire_identity(normalized))), "D/F canonical full wire differs")
        if canonical_wires is not None:
            P.require(equal(wire_identity(normalized), canonical_wires[(job["case_id"], job["variant"])]), "wire differs from approved functional bytes/checksum")
        P.require(equal(normalized["work_identity"], expected.setdefault(job["case_id"], normalized["work_identity"])), "D/F semantic/work/layout differs")
        total_bytes += normalized["wire_bytes"]
        P.require(total_bytes <= POLICY["max_retained_wire_bytes"], "retained output budget exceeded")


def functional(build_path, out, queue):
    require_passed_artifact(build_path, "default-build")
    out = start_run(out, queue, "functional")
    try:
        shutil.copytree(build_path, out / "build")
        meta = P.read(out / "build/frozen/prepared/historical-metadata.json")
        P.require(equal(runtime_identity(meta), P.read(out / "build/frozen/runtime.json")), "runtime differs before functional")
        P.write_new(out / "schedule.json", functional_schedule())
        P.write_new(out / "environment-before.json", environment(out))
        execute_samples(out, functional_schedule(), out / "build", time.monotonic() + 240)
        P.write_new(out / "environment-after.json", environment(out))
        P.require(equal(runtime_identity(meta), P.read(out / "build/frozen/runtime.json")), "runtime differs after functional")
        seal(out, "default-functional", "completed", executed_cpp=True, performance_evidence=False)
    except Exception as error:
        if not (out / "environment-after.json").exists():
            P.write_new(out / "environment-after.json", environment(out))
        seal(out, "default-functional", "failed", error=str(error), performance_evidence=False)
        raise
    return dict(status="functional_pending_queue_completion", performance_evidence=False)


def preregister(functional_path, out, window_id, window_out):
    require_passed_artifact(functional_path, "default-functional")
    P.require(P.identifier(window_id), "invalid window id")
    spec = dict(schema_version=1, kind="default-calibration-preregistration", status="proposed_for_parent_review",
                purpose="calibration_only", evidence_class="native", timing_authorized=False, window_id=window_id,
                calibration_output=str(Path(window_out).resolve()),
                cases=list(CASES), policy=POLICY, schedule=calibration_schedule(),
                bridge_sha256=P.sha(__file__), contract_sha256=P.sha(contract_path()),
                paired_primitives_sha256=P.sha(P.__file__), source_prepare_sha256=P.sha(S.__file__),
                functional_result_sha256=P.sha(Path(functional_path) / "result.json"),
                build_result_sha256=P.sha(Path(functional_path) / "build/result.json"),
                boundary="exact native fields in contract; wire file writes/readback/value checks/destruction outside native timers",
                adoption_allowed=False)
    P.write_new(out, spec)
    return dict(status=spec["status"], preregister_sha256=P.sha(out), processes=len(spec["schedule"]), timing_authorized=False)


def validate_prereg(spec):
    P.require(spec.get("schema_version") == 1 and type(spec.get("schema_version")) is int
              and spec.get("kind") == "default-calibration-preregistration"
              and spec.get("status") == "proposed_for_parent_review" and spec.get("purpose") == "calibration_only"
              and spec.get("timing_authorized") is False and spec.get("adoption_allowed") is False
              and spec.get("evidence_class") == "native" and P.identifier(spec.get("window_id"))
              and isinstance(spec.get("calibration_output"), str) and Path(spec["calibration_output"]).is_absolute(), "wrong calibration preregistration")
    P.require(P.canonical(spec.get("cases")) == P.canonical(list(CASES)) and P.canonical(spec.get("policy")) == P.canonical(POLICY)
              and P.canonical(spec.get("schedule")) == P.canonical(calibration_schedule()), "calibration schedule/policy changed")
    for k in ("bridge_sha256", "contract_sha256", "paired_primitives_sha256", "source_prepare_sha256", "functional_result_sha256", "build_result_sha256"):
        P.require(P.digest(spec.get(k)), "missing preregistered identity")
    return spec


def approval_check(approval, spec, prereg_path):
    P.require(type(approval.get("schema_version")) is int and approval.get("schema_version") == 1 and approval.get("kind") == "vm-default-calibration-approval"
              and approval.get("status") == "accepted" and approval.get("parent_chat") == PARENT
              and approval.get("approved_action") == "calibration_only" and P.identifier(approval.get("approval_id"))
              and approval.get("preregister_sha256") == P.sha(prereg_path), "parent timing approval missing/mismatched")
    for key in ("bridge_sha256", "contract_sha256", "functional_result_sha256", "build_result_sha256"):
        P.require(approval.get(key) == spec[key], "approval identity differs: " + key)


def calibrate(functional_path, prereg_path, approval_path, approval_sha256, out, queue):
    spec = validate_prereg(P.read(prereg_path))
    P.require(P.digest(approval_sha256) and P.sha(approval_path) == approval_sha256, "external approval SHA pin differs")
    approval_check(P.read(approval_path), spec, prereg_path)  # Must precede native work or output mutation.
    P.require(str(Path(out).resolve()) == spec["calibration_output"], "approval can execute only at its one preregistered output")
    P.require(spec["bridge_sha256"] == P.sha(__file__) and spec["contract_sha256"] == P.sha(contract_path())
              and spec["paired_primitives_sha256"] == P.sha(P.__file__) and spec["source_prepare_sha256"] == P.sha(S.__file__), "approved bridge sources changed")
    require_passed_artifact(functional_path, "default-functional")
    P.require(P.sha(Path(functional_path) / "result.json") == spec["functional_result_sha256"]
              and P.sha(Path(functional_path) / "build/result.json") == spec["build_result_sha256"], "approved native artifacts differ")
    out = start_run(out, queue, "timing")
    try:
        shutil.copytree(functional_path, out / "functional")
        copy_file(prereg_path, out / "preregister.json")
        copy_file(approval_path, out / "approval.json")
        build_path = out / "functional/build"
        functional_reports = read_samples(out / "functional", functional_schedule(), build_path)
        canonical_wires = {(j["case_id"], j["variant"]): wire_identity(functional_reports[j["id"]]) for j in functional_schedule()}
        meta = P.read(build_path / "frozen/prepared/historical-metadata.json")
        P.require(equal(runtime_identity(meta), P.read(build_path / "frozen/runtime.json")), "runtime differs before calibration")
        P.write_new(out / "environment-before.json", environment(out))
        execute_samples(out, spec["schedule"], build_path, time.monotonic() + POLICY["max_window_seconds"], canonical_wires)
        P.write_new(out / "environment-after.json", environment(out))
        P.require(equal(runtime_identity(meta), P.read(build_path / "frozen/runtime.json")), "runtime differs after calibration")
        summary = summarize(spec["schedule"], read_samples(out, spec["schedule"], build_path, canonical_wires))
        P.write_new(out / "summary.json", summary)
        seal(out, "default-calibration", "completed", executed_cpp=True, performance_evidence=True,
             interpretation="calibration_only; no D/F comparison/adoption; parent review required")
    except Exception as error:
        if not (out / "environment-after.json").exists():
            P.write_new(out / "environment-after.json", environment(out))
        seal(out, "default-calibration", "failed", error=str(error), performance_evidence=False)
        raise
    return dict(status="calibration_pending_queue_completion", adoption_allowed=False)


def wire_identity(report):
    return {k: report[k] for k in ("wire_sha256", "wire_checksum", "wire_bytes", "wire_header")}


def fnv_wire(path):
    result = 1469598103934665603
    with Path(path).open("rb") as source:
        for data in iter(lambda: source.read(1024 * 1024), b""):
            for byte in data:
                result = ((result ^ byte) * 1099511628211) & (2**64-1)
    return result


def validate_wire(path, report, checksum_cache, verify_fnv):
    """Check the native V1 envelope; full graph/value verification is in C++.

    Compute slow FNV only once per distinct functional SHA, never for each of
    the 78 calibration outputs. Calibration pins the entire canonical tuple.
    """
    P.require(Path(path).stat().st_size == report["wire_bytes"], "full wire size differs")
    with Path(path).open("rb") as source:
        raw = source.read(88)
    P.require(len(raw) == 88, "native V1 header truncated")
    values = struct.unpack("<8sHHIQQQQQQQIIII", raw)
    magic, major, minor, header_size, used, reserved, root, allocator, root_type, schema_lo, schema_hi, flags, endian, crc, reserved0 = values
    P.require((magic, major, minor, header_size, flags, endian, reserved0) ==
              (b"XOFFV1\0\0", 1, 0, 88, 0, 0x01020304, 0)
              and used == reserved == report["work_identity"]["used_end"]
              and used + 88 == report["wire_bytes"] and root < used and allocator < used
              and root_type > 0 and schema_lo > 0 and schema_hi == 0, "wrong native V1 envelope/layout/schema")
    if verify_fnv:
        digest = report["wire_sha256"]
        if digest not in checksum_cache:
            checksum_cache[digest] = fnv_wire(path)
        P.require(checksum_cache[digest] == report["wire_checksum"], "native FNV checksum does not bind retained full wire")
    return dict(root_offset=root, allocator_offset=allocator, root_type_id=root_type,
                schema_hash_lo=schema_lo, schema_hash_hi=schema_hi, crc32c=crc)


def read_samples(out, jobs, build_path, canonical_wires=None):
    request = P.read(P.safe_file(out, "queue/request.json"))
    original_root = Path(request["out"]).parent
    P.require(original_root.is_absolute() and Path(request["out"]).name == "queue", "missing original execution root")
    original_build = original_root / build_path.relative_to(out)
    P.require(set(p.name for p in (out / "commands").iterdir()) ==
              {job["id"] + ext for job in jobs for ext in (".stdout", ".stderr", ".receipt.json", ".wire", ".normalized.json")}, "missing/extra native evidence")
    reports, expected, previous_end, first_start, total_bytes = {}, {}, 0, None, 0
    checksum_cache, expected_wires = {}, {}
    for job in jobs:
        prefix = out / "commands" / job["id"]
        case = next(c for c in CASES if c["id"] == job["case_id"])
        receipt = P.read(prefix.with_suffix(".receipt.json"))
        normalized = native_report(prefix.with_suffix(".stdout").read_bytes(), case)
        binary_sha = P.sha(build_path / "bin" / job["variant"])
        normalized.update(wire_sha256=P.sha(prefix.with_suffix(".wire")), binary_sha256=binary_sha)
        normalized["wire_header"] = validate_wire(prefix.with_suffix(".wire"), normalized, checksum_cache, canonical_wires is None)
        P.require(equal(wire_identity(normalized), expected_wires.setdefault(job["case_id"], wire_identity(normalized))), "D/F full wire differs in archive")
        P.require(equal(P.read(prefix.with_suffix(".normalized.json")), normalized), "normalized mapping differs from native raw")
        if canonical_wires is not None:
            P.require(equal(wire_identity(normalized), canonical_wires[(job["case_id"], job["variant"])]), "archive wire differs from functional canonical output")
        # Exact original argv is reconstructed from accepted queue provenance;
        # the old absolute paths are compared as strings, never reopened.
        argv = receipt.get("argv", [])
        P.require(argv == native_argv(original_build / "bin" / job["variant"], case,
                                      original_root / "commands" / (job["id"] + ".wire")), "native argv differs")
        P.require(type(receipt.get("returncode")) is int and receipt["returncode"] == 0 and receipt.get("timed_out") is False,
                  "native child failed")
        start, end = receipt.get("started_monotonic_ns"), receipt.get("finished_monotonic_ns")
        P.require(P.integer(start, 1) and P.integer(end, start) and start >= previous_end
                  and number(receipt.get("timeout_seconds"), True) and receipt["timeout_seconds"] <= POLICY["child_timeout_seconds"], "invalid native process interval")
        P.require(end-start <= (receipt["timeout_seconds"] + 1) * 1e9
                  and P.parse(prefix.with_suffix(".stdout").read_bytes())["elapsed_ms"] * 1e6 <= end-start, "native timer exceeds enclosing process interval")
        first_start = start if first_start is None else first_start
        previous_end = end
        wire = prefix.with_suffix(".wire")
        P.require(wire.stat().st_size == normalized["wire_bytes"], "retained full output length differs")
        P.require(equal(normalized["work_identity"], expected.setdefault(job["case_id"], normalized["work_identity"])), "native common work identity differs")
        total_bytes += wire.stat().st_size
        reports[job["id"]] = normalized
    P.require(previous_end-first_start <= POLICY["max_window_seconds"] * 1e9 and total_bytes <= POLICY["max_retained_wire_bytes"], "native run budget exceeded")
    return reports


def ratio_stats(jobs, reports, metric):
    ratios, orders = [], {"AB": [], "BA": []}
    absolute = {"A": [], "B": []}
    for i in range(0, len(jobs), 2):
        first, second = jobs[i:i+2]
        P.require(first["pair"] == second["pair"] and first["position"] == 0 and second["position"] == 1
                  and first["order"] == second["order"] and first["side"] != second["side"], "incomplete native pair")
        sides = {j["side"]: reports[j["id"]]["metrics_ms"][metric] for j in (first, second)}
        ratio = sides["A"] / sides["B"]
        for side in ("A", "B"):
            absolute[side].append(sides[side])
        ratios.append(ratio)
        orders[first["order"]].append(ratio)
    logs = [math.log(v) for v in ratios]
    median = statistics.median(logs)
    return dict(paired_A_over_B=ratios, median_A_over_B=statistics.median(ratios),
                absolute_ms={side: distribution(values) for side, values in absolute.items()},
                pooled_absolute_ms=distribution(absolute["A"] + absolute["B"]),
                order_stratum_medians={k: statistics.median(v) for k, v in orders.items()},
                median_absolute_log_ratio=statistics.median(abs(v) for v in logs),
                log_mad=statistics.median(abs(v-median) for v in logs), log_iqr=P.quantile(logs, .75)-P.quantile(logs, .25))


def distribution(values):
    P.require(values and all(number(v, True) for v in values), "empty/nonpositive absolute timing distribution")
    return dict(values=values, n=len(values), median=statistics.median(values),
                minimum=min(values), maximum=max(values), q25=P.quantile(values, .25), q75=P.quantile(values, .75))


def qualified(stats):
    bound = math.log1p(POLICY["noise_relative_limit"])
    spread = math.log1p(POLICY["dispersion_relative_limit"])
    return (all(abs(math.log(v)) <= bound for v in [stats["median_A_over_B"], *stats["order_stratum_medians"].values()])
            and stats["median_absolute_log_ratio"] <= bound and stats["log_mad"] <= spread and stats["log_iqr"] <= 2 * spread)


def summarize(jobs, reports):
    P.require(equal(jobs, calibration_schedule()) and set(reports) == {j["id"] for j in jobs}, "different calibration job set")
    rows = []
    for case in CASES:
        for metric in case["metrics"]:
            anchors = {}
            for anchor in POLICY["anchors"]:
                group = [j for j in jobs if j["case_id"] == case["id"] and j["anchor"] == anchor]
                stats = ratio_stats(group, reports, metric)
                anchors[anchor] = dict(qualified=qualified(stats), statistics=stats)
            medians = {anchor: value["statistics"]["pooled_absolute_ms"]["median"] for anchor, value in anchors.items()}
            pooled_drift = max(medians.values()) / min(medians.values())
            side_drift = {}
            for side in ("A", "B"):
                values = [value["statistics"]["absolute_ms"][side]["median"] for value in anchors.values()]
                side_drift[side] = max(values) / min(values)
            drift_ok = all(v <= 1+POLICY["drift_relative_limit"] for v in [pooled_drift, *side_drift.values()])
            rows.append(dict(case_id=case["id"], metric=metric, units="ms", anchors=anchors,
                             anchor_pooled_medians_ms=medians, pooled_anchor_max_over_min=pooled_drift,
                             side_anchor_max_over_min=side_drift, drift_qualified=drift_ok,
                             qualified=drift_ok and all(a["qualified"] for a in anchors.values())))
    return dict(schema_version=1, kind="default-calibration-summary", evidence_class="native",
                all_aa_qualified=all(r["qualified"] for r in rows), rows=rows, adoption_allowed=False,
                ratio_direction="A / B; both endpoints are the exact F binary",
                interpretation="one bounded calibration attempt; no bootstrap/equivalence/adoption claim; comparisons require a separate accepted preregistration")


def audit(out, approval_sha256=None):
    """Read-only portable audit; never runs a native binary or touches old paths."""
    out = Path(out)
    result = P.read(P.safe_file(out, "result.json"))
    P.require(result.get("schema_version") == 1 and type(result.get("schema_version")) is int
              and result.get("adoption_allowed") is False and result.get("kind") in
              ("default-frozen", "default-build", "default-functional", "default-calibration"), "wrong default bundle")
    P.require(equal(payload_inventory(out), result.get("files")), "default archive inventory differs")
    if result.get("status") == "failed":
        P.require(bool(result.get("error")) and result.get("performance_evidence") is False, "failed bundle has performance claim")
        return dict(status="failed_preserved", kind=result["kind"], adoption_allowed=False)
    P.require(result.get("status") == "completed", "incomplete default bundle")
    kind = result["kind"]
    if kind == "default-frozen":
        P.require(result.get("executed_cpp") is False and result.get("performance_evidence") is False, "freeze claims execution")
        S.audit(out / "prepared")
        P.require(P.sha(out / "protocol/default_measurement.py") == P.sha(__file__)
                  and P.sha(out / "protocol/paired_measurement.py") == P.sha(P.__file__)
                  and P.sha(out / "protocol/prepare_default.py") == P.sha(S.__file__)
                  and P.sha(out / "protocol/default-contract.json") == P.sha(contract_path())
                  and P.sha(out / "protocol/run_job.py") == QUEUE_SHA, "audit requires unchanged frozen protocol")
        P.read(out / "runtime.json")
    else:
        request = queue_audit(out / "queue", {"default-build": "build", "default-functional": "functional", "default-calibration": "timing"}[kind])
        original_root = Path(request["out"]).parent
        P.require(original_root.is_absolute() and Path(request["out"]).name == "queue", "missing original queue root")
        if kind == "default-build":
            P.require(result.get("compiled_cpp") is True and result.get("executed_cpp") is False
                      and result.get("performance_evidence") is False, "wrong build execution claim")
            require_passed_artifact(out / "frozen", "default-frozen")
            identities = P.read(out / "binaries.json")
            P.require(equal(identities, {v: file_id(out / "bin" / v) for v in ("D", "F")}), "build binary identity differs")
            meta = P.read(out / "frozen/prepared/historical-metadata.json")
            P.require(set(p.name for p in (out / "commands").iterdir()) ==
                      {name + ext for name in ("D", "F", "compiler-version") for ext in (".stdout", ".stderr", ".receipt.json")}, "extra/missing build command evidence")
            for variant in ("D", "F"):
                receipt = P.read(out / "commands" / (variant + ".receipt.json"))
                argv = receipt.get("argv", [])
                P.require(receipt.get("returncode") == 0 and type(receipt.get("returncode")) is int and receipt.get("timed_out") is False
                          and len(argv) == len(meta["compile_commands"][variant]), "build receipt failed")
                P.require(argv == compile_command(meta, variant, original_root / "frozen/prepared/source" / variant,
                                                  original_root / "bin" / variant), "exact inherited build invocation differs")
            version = P.read(out / "commands/compiler-version.receipt.json")
            P.require(version.get("argv") == [meta["compile_commands"]["D"][0], "--version"]
                      and type(version.get("returncode")) is int and version["returncode"] == 0
                      and version.get("timed_out") is False
                      and (out / "commands/compiler-version.stdout").stat().st_size > 0, "missing exact compiler version receipt")
        elif kind == "default-functional":
            P.require(result.get("executed_cpp") is True and result.get("performance_evidence") is False, "functional performance claim")
            require_passed_artifact(out / "build", "default-build")
            audit_environment(out)
            jobs = functional_schedule()
            P.require(equal(P.read(out / "schedule.json"), jobs), "functional schedule changed")
            read_samples(out, jobs, out / "build")
        else:
            P.require(result.get("executed_cpp") is True and result.get("performance_evidence") is True, "wrong calibration claim")
            P.require(P.digest(approval_sha256) and P.sha(out / "approval.json") == approval_sha256,
                      "completed calibration audit requires external approval SHA pin")
            require_passed_artifact(out / "functional", "default-functional")
            spec = validate_prereg(P.read(out / "preregister.json"))
            audit_environment(out)
            P.require(spec["calibration_output"] == str(original_root), "calibration reused outside approved output")
            queued = request["command"]
            P.require(queued[queued.index("--approval-sha256")+1] == approval_sha256,
                      "queue launch approval SHA differs from external audit pin")
            P.require(spec["bridge_sha256"] == P.sha(__file__) and spec["contract_sha256"] == P.sha(contract_path())
                      and spec["paired_primitives_sha256"] == P.sha(P.__file__) and spec["source_prepare_sha256"] == P.sha(S.__file__), "approved protocol identity differs")
            approval_check(P.read(out / "approval.json"), spec, out / "preregister.json")
            P.require(spec["functional_result_sha256"] == P.sha(out / "functional/result.json")
                      and spec["build_result_sha256"] == P.sha(out / "functional/build/result.json"), "approved result identities differ")
            native_build = out / "functional/build"
            functional_reports = read_samples(out / "functional", functional_schedule(), native_build)
            canonical_wires = {(j["case_id"], j["variant"]): wire_identity(functional_reports[j["id"]]) for j in functional_schedule()}
            reports = read_samples(out, spec["schedule"], native_build, canonical_wires)
            P.require(equal(P.read(out / "summary.json"), summarize(spec["schedule"], reports)), "native calibration summary does not recompute")
    return dict(status="passed", kind=kind, adoption_allowed=False, performance_evidence=result.get("performance_evidence"))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    subs = parser.add_subparsers(dest="action", required=True)
    p = subs.add_parser("freeze")
    p.add_argument("--prepared", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p = subs.add_parser("build")
    p.add_argument("--frozen", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--queue", type=Path, required=True)
    p = subs.add_parser("functional")
    p.add_argument("--build", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--queue", type=Path, required=True)
    p = subs.add_parser("preregister")
    p.add_argument("--functional", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--window-id", required=True)
    p.add_argument("--window-out", type=Path, required=True)
    p = subs.add_parser("calibrate")
    p.add_argument("--functional", type=Path, required=True)
    p.add_argument("--preregister", type=Path, required=True)
    p.add_argument("--approval", type=Path, required=True)
    p.add_argument("--approval-sha256", required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--queue", type=Path, required=True)
    p = subs.add_parser("audit")
    p.add_argument("path", type=Path)
    p.add_argument("--approval-sha256")
    args = parser.parse_args(argv)
    if args.action == "freeze":
        result = freeze(args.prepared, args.out)
    elif args.action == "build":
        result = build(args.frozen, args.out, args.queue)
    elif args.action == "functional":
        result = functional(args.build, args.out, args.queue)
    elif args.action == "preregister":
        result = preregister(args.functional, args.out, args.window_id, args.window_out)
    elif args.action == "calibrate":
        result = calibrate(args.functional, args.preregister, args.approval, args.approval_sha256, args.out, args.queue)
    else:
        result = audit(args.path, args.approval_sha256)
    print(P.canonical(result).decode(), end="")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, TypeError, StopIteration) as error:
        print("default measurement rejected: " + str(error), file=sys.stderr)
        sys.exit(1)
