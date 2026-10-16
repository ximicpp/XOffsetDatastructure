#!/usr/bin/env python3
"""Freeze, measure and audit the small D/F regression investigation.

Preparation compiles only. Each run-window invocation performs one sequential
window. Three invocations, separated by at least 60 seconds, are required before
the summary is marked as having the planned window count. Do not run alongside
builds, sanitizers or other benchmarks. See the evidence README for limits.
"""
import argparse
import datetime as dt
import gzip
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import random
import shutil
import statistics
import subprocess
import sys
import tarfile
import time

ROOT = Path(__file__).resolve().parents[1]
BOOST_LIBS = ("container", "core", "config", "throw_exception", "static_assert",
              "container_hash", "type_traits", "winapi", "intrusive", "predef")
HEADERS = ("xoffsetdatastructure.hpp", "xoffset_virtual_memory.hpp")
HARNESS = ("bench_vm_improvements.cpp", "bench_vm_compare.cpp")
CASES = (
    dict(id="save_first_64m", workload="save", size=64 * 1024**2, cycles=1,
         metrics=["first_ms"]),
    dict(id="save_repeated_64m", workload="save", size=64 * 1024**2, cycles=4,
         metrics=["first_ms", "repeated_mean_ms"]),
    dict(id="vector_tail_1m", workload="vector_tail", size=1_000_000, cycles=1,
         metrics=["elapsed_ms"]),
)
CONTRASTS = {"AA": {"before": "F", "after": "F"},
             "DF": {"before": "D", "after": "F"}}
EVIDENCE_SCHEMA_VERSION = 2


def command(argv, **kwargs):
    return subprocess.run(list(map(str, argv)), check=True, capture_output=True,
                          text=True, **kwargs)


def output(argv):
    return command(argv).stdout.strip()


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024**2), b""):
            digest.update(block)
    return digest.hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def now():
    return dt.datetime.now().astimezone().isoformat()


def require_current_protocol(meta):
    if meta.get("schema_version") != EVIDENCE_SCHEMA_VERSION:
        raise RuntimeError("legacy or unsupported evidence protocol; audit old evidence with its frozen runner, or prepare a new experiment")


def quantile(values, fraction):
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)


def distribution(values):
    if not values or not all(math.isfinite(v) for v in values):
        raise RuntimeError("empty or nonfinite measurement distribution")
    return dict(n=len(values), median=statistics.median(values),
                q25=quantile(values, .25), q75=quantile(values, .75),
                min=min(values), max=max(values))


def median_interval(values, seed=1729):
    # Resample paired ratios, never the individual operation timings.
    # This is a descriptive percentile bootstrap, not an equivalence test.
    rng = random.Random(seed)
    medians = [statistics.median(rng.choices(values, k=len(values)))
               for _ in range(10_000)]
    return dict(low=quantile(medians, .025), high=quantile(medians, .975),
                method="paired_ratio_median_percentile_bootstrap", resamples=10_000,
                confidence=.95, seed=seed)


def host_state():
    state = dict(at=now(), load_average=list(os.getloadavg()),
                 process_snapshot=output(["ps", "-axo", "pid,ppid,pcpu,comm"]))
    if platform.system() == "Darwin":
        for name, cmd in {
            "power_source": ["pmset", "-g", "batt"],
            "thermal": ["pmset", "-g", "therm"],
            "swap": ["sysctl", "-n", "vm.swapusage"],
        }.items():
            state[name] = output(cmd)
    return state


def verify_runtime(out, meta):
    for variant, manifest in meta["source_manifests"].items():
        for name, expected in manifest.items():
            if sha(out / "snapshots" / variant / name) != expected:
                raise RuntimeError(f"source snapshot changed: {variant}/{name}")
        if sha(out / variant) != meta["binary_sha256"][variant]:
            raise RuntimeError(f"binary changed: {variant}")
    for name, identity in meta["runtime_files"].items():
        if sha(identity["path"]) != identity["sha256"]:
            raise RuntimeError(f"toolchain runtime changed: {name}")
    if sha(out / "snapshots" / Path(__file__).name) != meta["runner_sha256"]:
        raise RuntimeError("frozen runner changed")
    if sha(__file__) != meta["runner_sha256"]:
        raise RuntimeError("execute the unchanged frozen runner or prepare again")


def prepare(args):
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    # Preserve clang++'s driver name: resolving its symlink to clang-21 would
    # change automatic C++ runtime linkage even though the executable is equal.
    compiler = args.compiler.absolute()
    deps = {}
    for name, path in (("boost", args.boost), ("typelayout", args.typelayout)):
        path = path.resolve()
        if output(["git", "-C", path, "status", "--porcelain"]):
            raise RuntimeError(f"dependency is dirty: {path}")
        deps[name] = dict(path=str(path), revision=output(["git", "-C", path, "rev-parse", "HEAD"]))
    common = [str(compiler), "-O3", "-DNDEBUG", "-std=c++26", "-freflection",
              "-fexpansion-statements", "-stdlib=libc++", "-pthread"]
    sdk = None
    if platform.system() == "Darwin":
        if not args.sdk.is_dir():
            raise RuntimeError("explicit SDK directory is missing")
        common += ["-arch", platform.machine(), "-isysroot", str(args.sdk.resolve()),
                   "-mmacosx-version-min=10.15"]
        sdk = dict(path=str(args.sdk.resolve()), settings_sha256=sha(args.sdk / "SDKSettings.json"))
    for lib in BOOST_LIBS:
        common += ["-isystem", str(args.boost.resolve() / "libs" / lib / "include")]
    common += ["-isystem", str(args.typelayout.resolve() / "include")]
    link = ["-L" + str(args.runtime_lib.resolve()), "-Wl,-rpath," + str(args.runtime_lib.resolve())]
    manifests, commands, binaries = {}, {}, {}
    for variant, source in (("D", args.d_source), ("F", args.f_source)):
        snap = out / "snapshots" / variant
        snap.mkdir(parents=True)
        for name in HEADERS:
            shutil.copy2(source / name, snap / name)
        for name in HARNESS:
            shutil.copy2(args.harness / name, snap / name)
        argv = common + ["-I", str(snap), str(snap / HARNESS[0])] + link + ["-o", str(out / variant)]
        commands[variant] = argv
        print(f"Compiling {variant}", flush=True)
        result = subprocess.run(argv, capture_output=True, text=True)
        (out / f"compile_{variant}.log").write_text(result.stdout + result.stderr)
        result.check_returncode()
        manifests[variant] = {name: sha(snap / name) for name in HEADERS + HARNESS}
        binaries[variant] = sha(out / variant)
    shutil.copy2(__file__, out / "snapshots" / Path(__file__).name)
    runtime = {"compiler": dict(path=str(compiler), resolved_path=str(compiler.resolve()), sha256=sha(compiler))}
    linked = {}
    if platform.system() == "Darwin":
        for variant in ("D", "F"):
            linked[variant] = output(["otool", "-L", out / variant])
        for name in ("libc++.1.dylib", "libc++abi.1.dylib", "libunwind.1.dylib"):
            path = (args.runtime_lib / name).resolve()
            if path.exists():
                runtime[name] = dict(path=str(path), sha256=sha(path))
    metadata = dict(schema_version=EVIDENCE_SCHEMA_VERSION, prepared_at=now(), cases=list(CASES), contrasts=CONTRASTS,
                    runs=args.runs, warmups=args.warmups, required_windows=3,
                    minimum_window_gap_seconds=60, compiler_version=output([compiler, "--version"]),
                    sdk=sdk, dependencies=deps, runtime_files=runtime, linked_libraries=linked,
                    source_manifests=manifests, binary_sha256=binaries, compile_commands=commands,
                    variant_labels={v: v + "_" + m[HEADERS[0]][:12] for v, m in manifests.items()},
                    runner_sha256=sha(__file__), platform=platform.platform(), machine=platform.machine(),
                    page_size=os.sysconf("SC_PAGE_SIZE"), cpu_count=os.cpu_count(),
                    target_resolution_percent=2,
                    schedule="fresh child per sample; sequential; rotate cases by window; alternate paired endpoint order and AA/DF contrast order",
                    validation="full values and save/load checks outside timers; semantic/work/layout identity must match across all endpoints and windows; exact wire hash reported separately",
                    interval_limit="10,000 paired-ratio bootstrap resamples within each window; small samples and 3 host windows do not establish equivalence or a population confidence guarantee",
                    attribution="no source-level attribution or ablation until stable cross-window difference exceeds AA variation",
                    counter_limits="unmodified historical harness; missing or uninstrumented counters are not zero work; see README metric definitions")
    if platform.system() == "Darwin":
        metadata["host"] = {k: output(["sysctl", "-n", k]) for k in
                            ("hw.memsize", "hw.physicalcpu", "hw.logicalcpu", "machdep.cpu.brand_string")}
    write_json(out / "metadata.json", metadata)
    with tarfile.open(out / "source_snapshots.tar.gz", "w:gz") as archive:
        archive.add(out / "snapshots", arcname="snapshots")
    metadata["archive_sha256"] = sha(out / "source_snapshots.tar.gz")
    write_json(out / "metadata.json", metadata)
    print(f"Prepared without running timings: {out}", flush=True)


def identity(row):
    # Wire padding/normalization checksum is audited separately; full semantic
    # identity and common work/layout are enforced, never inferred from timing.
    keys = ("semantic_checksum", "request_count", "size", "requested_cycles",
            "used_end", "wire_size", "final_logical_bytes", "final_accessible_bytes",
            "initial_logical_bytes", "initial_reserved_bytes", "container_moves",
            "container_copy_bytes_est", "allocation_failures", "growth_count")
    return {k: row[k] for k in keys}


def read_rows(out):
    require_current_protocol(json.loads((out / "metadata.json").read_text()))
    rows = []
    # Inspect every directory, not just published gzip files. A failed or
    # interrupted window must prevent continuation rather than disappear.
    for directory in sorted(out.glob("window_*")):
        state_path = directory / "metadata.json"
        if not state_path.is_file():
            raise RuntimeError(f"incomplete window: {directory}; preserve it and prepare a new experiment")
        state = json.loads(state_path.read_text())
        if state.get("status") != "completed" or state.get("runtime_identity_validated") is not True:
            raise RuntimeError(f"failed or incomplete window: {directory}; preserve it and prepare a new experiment")
        path = directory / "raw.jsonl.gz"
        if not path.is_file():
            raise RuntimeError(f"incomplete window: missing {path}")
        with gzip.open(path, "rt") as stream:
            rows += [json.loads(line) for line in stream]
    return rows


def summarize(meta, rows):
    results = []
    windows = sorted({r["window"] for r in rows})
    for case in meta["cases"]:
        for metric in case["metrics"]:
            entry = dict(case_id=case["id"], metric=metric, windows=[])
            for window in windows:
                item = dict(window=window, contrasts={})
                for contrast in meta["contrasts"]:
                    group = [r for r in rows if r["case_id"] == case["id"] and
                             r["window"] == window and r["contrast"] == contrast and r["phase"] == "measured"]
                    pairs = {}
                    for row in group:
                        pairs.setdefault(row["pair"], {})[row["endpoint"]] = row
                    if len(group) != meta["runs"] * 2 or len(pairs) != meta["runs"]:
                        raise RuntimeError("incomplete window or duplicate rows")
                    ratios = [pair["before"][metric] / pair["after"][metric] for pair in pairs.values()]
                    data = dict(before_ms=distribution([r[metric] for r in group if r["endpoint"] == "before"]),
                                after_ms=distribution([r[metric] for r in group if r["endpoint"] == "after"]),
                                paired_before_over_after=distribution(ratios),
                                paired_median_interval=median_interval(ratios), paired_ratios=ratios)
                    data["order_strata"] = {}
                    for position in (0, 1):
                        subset = [p["before"][metric] / p["after"][metric] for p in pairs.values()
                                  if p["before"]["execution_position"] == position]
                        if subset:
                            data["order_strata"]["before_first" if position == 0 else "after_first"] = distribution(subset)
                    item["contrasts"][contrast] = data
                entry["windows"].append(item)
            results.append(entry)
    return dict(completed_windows=len(windows), planned_window_count_reached=len(windows) >= 3,
                ratio_direction="before / after; below 1 means after slower",
                results=results)


def run_window(args):
    out = args.output.resolve()
    meta = json.loads((out / "metadata.json").read_text())
    require_current_protocol(meta)
    verify_runtime(out, meta)
    previous = read_rows(out)
    window = max((r["window"] for r in previous), default=0) + 1
    if window > 1:
        last = json.loads((out / f"window_{window-1:02d}" / "metadata.json").read_text())
        gap = time.time() - last["finished_epoch"]
        if gap < meta["minimum_window_gap_seconds"]:
            raise RuntimeError(f"window gap {gap:.1f}s is below required 60s; run later")
    target = out / f"window_{window:02d}"
    target.mkdir(exist_ok=False)
    state = dict(window=window, status="running", runtime_identity_validated=False,
                 started_at=now(), started_epoch=time.time(), quiet_window_note=args.quiet_note,
                 start_host=host_state())
    write_json(target / "metadata.json", state)
    rows = []
    expected = {r["case_id"]: identity(r) for r in previous}
    cases = meta["cases"][window % 3:] + meta["cases"][:window % 3]
    with (target / "raw.jsonl").open("w") as stream:
        for case in cases:
            print(f"window {window}: {case['id']}", flush=True)
            for phase, count in (("warmup", meta["warmups"]), ("measured", meta["runs"])):
                for pair in range(count):
                    contrast_order = ("AA", "DF") if (pair + window) % 2 else ("DF", "AA")
                    for contrast in contrast_order:
                        endpoints = ("before", "after") if (pair + window) % 2 else ("after", "before")
                        for position, endpoint in enumerate(endpoints):
                            variant = meta["contrasts"][contrast][endpoint]
                            argv = [out / variant, "--workload", case["workload"], "--size", case["size"],
                                    "--cycles", case["cycles"], "--threads", 1, "--order", "sequential", "--reserve", 0]
                            start = time.monotonic()
                            proc = command(argv, timeout=300)
                            row = json.loads(proc.stdout)
                            for key in ("workload", "size"):
                                if row[key] != case[key]:
                                    raise RuntimeError(f"wrong returned {key}")
                            if row["requested_cycles"] != case["cycles"] or row["address_changes"]:
                                raise RuntimeError("wrong cycles or arena base moved")
                            if not row["memory_live"]["available"]:
                                raise RuntimeError("memory sample unavailable")
                            for metric in case["metrics"]:
                                if row[metric] <= 0 or not math.isfinite(row[metric]):
                                    raise RuntimeError("invalid timing")
                            current = identity(row)
                            if current != expected.setdefault(case["id"], current):
                                raise RuntimeError(f"common-work identity mismatch: {case['id']}")
                            row.update(window=window, case_id=case["id"], contrast=contrast, endpoint=endpoint,
                                       variant=meta["variant_labels"][variant], binary_sha256=meta["binary_sha256"][variant],
                                       phase=phase, pair=pair, execution_position=position, sequence=len(rows),
                                       process_wall_ms=(time.monotonic() - start) * 1000)
                            rows.append(row)
                            stream.write(json.dumps(row) + "\n")
                            stream.flush()
    state.update(end_host=host_state(),
                 measured_rows=sum(r["phase"] == "measured" for r in rows),
                 warmup_rows=sum(r["phase"] == "warmup" for r in rows),
                 raw_sha256=sha(target / "raw.jsonl"))
    summary = summarize(meta, previous + rows)
    try:
        verify_runtime(out, meta)
    except (OSError, RuntimeError):
        state.update(status="failed", failure="final_runtime_identity_validation")
        write_json(target / "metadata.json", state)
        raise
    with (target / "raw.jsonl").open("rb") as source, gzip.open(target / "raw.jsonl.gz", "wb") as dest:
        shutil.copyfileobj(source, dest)
    write_json(out / "summary.json", summary)
    # Publish completion last. Runtime failure, interrupted gzip/summary writes,
    # or a partial metadata write cannot become an accepted window on resume.
    state.update(finished_at=now(), finished_epoch=time.time(), status="completed",
                 runtime_identity_validated=True)
    write_json(target / "metadata.json", state)
    print(f"Completed measurement window {window}: {len(rows)} rows", flush=True)


def audit(out):
    meta = json.loads((out / "metadata.json").read_text())
    require_current_protocol(meta)
    if sha(out / "source_snapshots.tar.gz") != meta["archive_sha256"]:
        raise RuntimeError("source archive hash mismatch")
    with tarfile.open(out / "source_snapshots.tar.gz") as archive:
        expected = {f"snapshots/{variant}/{name}": value
                    for variant, files in meta["source_manifests"].items() for name, value in files.items()}
        expected["snapshots/benchmark_vm_stage1.py"] = meta["runner_sha256"]
        for name, digest in expected.items():
            if hashlib.sha256(archive.extractfile(name).read()).hexdigest() != digest:
                raise RuntimeError(f"archived source mismatch: {name}")
    rows = read_rows(out)
    seen = {}
    for row in rows:
        key = row["case_id"]
        if identity(row) != seen.setdefault(key, identity(row)):
            raise RuntimeError(f"semantic/work/layout identity mismatch: {key}")
        binary = meta["contrasts"][row["contrast"]][row["endpoint"]]
        if row["binary_sha256"] != meta["binary_sha256"][binary] or row["variant"] != meta["variant_labels"][binary]:
            raise RuntimeError("binary/variant attribution mismatch")
    for directory in sorted(out.glob("window_*")):
        info = json.loads((directory / "metadata.json").read_text())
        raw = gzip.decompress((directory / "raw.jsonl.gz").read_bytes())
        if hashlib.sha256(raw).hexdigest() != info["raw_sha256"]:
            raise RuntimeError(f"raw hash mismatch: {directory}")
        group = [r for r in rows if r["window"] == info["window"]]
        for phase, key in (("measured", "measured_rows"), ("warmup", "warmup_rows")):
            if sum(r["phase"] == phase for r in group) != info[key]:
                raise RuntimeError("row count mismatch")
    actual = summarize(meta, rows)
    if actual != json.loads((out / "summary.json").read_text()):
        raise RuntimeError("summary does not recompute exactly")
    print(f"Audit passed: {len(rows)} rows, {actual['completed_windows']} windows, source and raw hashes verified")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    prep = sub.add_parser("prepare")
    prep.add_argument("--output", type=Path, required=True)
    prep.add_argument("--d-source", type=Path, required=True)
    prep.add_argument("--f-source", type=Path, required=True)
    prep.add_argument("--harness", type=Path, default=ROOT / "tools")
    prep.add_argument("--boost", type=Path, default=ROOT / "external/boost")
    prep.add_argument("--typelayout", type=Path, default=ROOT / "external/typelayout")
    prep.add_argument("--compiler", type=Path, default=Path("/usr/local/bin/clang++"))
    prep.add_argument("--runtime-lib", type=Path, default=Path("/usr/local/lib"))
    prep.add_argument("--sdk", type=Path, default=Path("/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk"))
    prep.add_argument("--runs", type=int, default=7)
    prep.add_argument("--warmups", type=int, default=1)
    run = sub.add_parser("run-window")
    run.add_argument("--output", type=Path, required=True)
    run.add_argument("--quiet-note", required=True, help="Record how absence of competing builds/timers was established")
    check = sub.add_parser("audit")
    check.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.action == "prepare":
        if args.runs < 2 or args.warmups < 1:
            parser.error("at least two measured pairs and one warmup pair are required")
        prepare(args)
    elif args.action == "run-window":
        run_window(args)
    else:
        audit(args.output.resolve())


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"Stage 1 benchmark failed: {error}", file=sys.stderr)
        sys.exit(1)
