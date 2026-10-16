#!/usr/bin/env python3
"""Compare two frozen VM stages without modifying the original experiment.

Example: --before build/vm_improvements/snapshots/v1 --after
build/vm_improvements/snapshots/b --suite transport --wire-compat exact.
Use --prepare-only to compile while other checks run, then --resume OUTPUT to
start sequential measurements once the machine is otherwise idle.
"""
import argparse
import csv
import datetime as dt
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import time

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("vm_historical_helpers", ROOT / "tools/run_vm_perf_compare.py")
H = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(H)
MIB = 1024 * 1024
VARIANTS = ("before", "after")


def matrix(suites):
    cases = []
    def add(workload, size, cycles=1, threads=1, order="sequential", reserve=0, suite="containers"):
        cases.append(dict(id=f"{workload}_{size}_c{cycles}_t{threads}_{order}_r{reserve}",
                          workload=workload, size=size, cycles=cycles, threads=threads,
                          order=order, reserve=reserve, suite=suite))
    if "transport" in suites:
        for size in (1024, 64*1024, 8*MIB, 64*MIB):
            for work in ("save", "load"):
                add(work, size, suite="transport")
                add(work, size, cycles=64 if size <= 64*1024 else 4, suite="transport")
    if "owner" in suites:
        for size in (1, 100, 1000, 5000):
            for threads in (1, 2, 4, 8):
                for order in ("sequential", "random"):
                    add("owner", size, threads=threads, order=order, reserve=64*1024, suite="owner")
    if "containers" in suites:
        for work in ("vector_tail", "vector_growth", "vector_interleaved", "nested_interleaved"):
            add(work, 1_000_000)
        add("string_tail", 8*MIB)
    if "diagnostics" in suites:
        for reserve in (0, 64*1024):
            add("churn", 1000, cycles=8, reserve=reserve, suite="diagnostics")
        for n in (1000, 5000):
            add("fragmentation", n, suite="diagnostics")
            add("fragmentation_containers", n, suite="diagnostics")
        for size in (8*MIB, 64*MIB):
            add("shrink_regrow", size, cycles=8, suite="diagnostics")
    return cases


def freeze_and_compile(args, out):
    common = [args.compiler, "-O3", "-DNDEBUG", "-std=c++26", "-freflection",
              "-fexpansion-statements", "-stdlib=libc++", "-pthread"]
    if platform.system() == "Darwin":
        common += ["-arch", platform.machine(), "-isysroot", args.sdk, "-mmacosx-version-min=10.15"]
    deps = {}
    for name in ("boost", "typelayout"):
        path = ROOT / "external" / name
        deps[name] = H.output(["git", "-C", path, "rev-parse", "HEAD"])
        if H.output(["git", "-C", path, "status", "--porcelain"]):
            raise RuntimeError(f"dependency {name} is dirty")
    for lib in H.BOOST_LIBS:
        common += ["-isystem", ROOT / "external/boost/libs" / lib / "include"]
    common += ["-isystem", ROOT / "external/typelayout/include"]
    manifests, commands = {}, {}
    for variant in VARIANTS:
        source = getattr(args, variant).resolve()
        snap = out / "snapshots" / variant
        snap.mkdir(parents=True)
        for name in ("xoffsetdatastructure.hpp", "xoffset_virtual_memory.hpp"):
            shutil.copy2(source / name, snap / name)
        for name in ("bench_vm_improvements.cpp", "bench_vm_compare.cpp"):
            shutil.copy2(ROOT / "tools" / name, snap / name)
        argv = common + ["-I", snap, snap / "bench_vm_improvements.cpp",
                         "-L/usr/local/lib", "-Wl,-rpath,/usr/local/lib", "-o", out / variant]
        commands[variant] = [str(x) for x in argv]
        print(f"Compiling {variant}: {source}", flush=True)
        try:
            result = H.command(argv, text=True)
        except subprocess.CalledProcessError as error:
            (out / f"compile_{variant}.log").write_text(error.stdout + error.stderr)
            raise
        (out / f"compile_{variant}.log").write_text(result.stdout + result.stderr)
        manifests[variant] = {p.name: H.sha(p) for p in snap.iterdir()}
        manifests[variant]["binary_sha256"] = H.sha(out / variant)
    for name in ("run_vm_improvements.py", "run_vm_perf_compare.py"):
        shutil.copy2(ROOT / "tools" / name, out / "snapshots" / name)
    return deps, manifests, commands


def trial(binary, case, path=None):
    argv = [binary]
    for key in ("workload", "size", "cycles", "threads", "order", "reserve"):
        argv += ["--" + key, str(case[key])]
    if path is not None:
        argv += ["--wire-path", path]
    start = time.monotonic()
    try:
        proc = H.command(argv, text=True, timeout=300)
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"trial failed {argv}:\n{error.stderr}") from error
    row = json.loads(proc.stdout)
    for key in ("workload", "size", "threads", "order"):
        if row[key] != case[key]:
            raise RuntimeError(f"harness returned wrong {key}")
    if row["address_changes"]:
        raise RuntimeError("stable VM base moved")
    if not case["workload"].startswith("verify_") and not row["memory_live"]["available"]:
        raise RuntimeError("live memory measurement unavailable")
    row["variant"] = binary.name
    row["process_wall_ms"] = (time.monotonic() - start) * 1000
    return row


def fixtures(out, cases, metadata):
    result = {}
    evidence = []
    for size in sorted({c["size"] for c in cases if c["workload"] == "load"}):
        case = dict(workload="prepare_transport", size=size, cycles=1,
                    threads=1, order="sequential", reserve=0)
        paths, rows = [], []
        for variant in VARIANTS:
            path = out / f"transport_{size}_{variant}.wire"
            rows.append(trial(out / variant, case, path))
            paths.append(path)
        if rows[0]["semantic_checksum"] != rows[1]["semantic_checksum"]:
            raise RuntimeError("transport fixture values differ")
        exact = H.sha(paths[0]) == H.sha(paths[1])
        if metadata["wire_compat"] == "exact" and not exact:
            raise RuntimeError("transport fixture bytes differ")
        result[size] = paths[0]
        evidence.append(dict(kind="transport", size=size, exact=exact,
                             files={v: dict(sha256=H.sha(p), size=p.stat().st_size)
                                    for v, p in zip(VARIANTS, paths)}))
    for case in (c for c in cases if c["suite"] == "containers"):
        hashes = []
        paths = []
        for writer in VARIANTS:
            path = out / f"{case['id']}_{writer}.wire"
            generated = trial(out / writer, case, path)
            hashes.append(generated["semantic_checksum"])
            paths.append(path)
            for reader in VARIANTS:
                checked = trial(out / reader, dict(case, workload="verify_" + case["workload"]), path)
                if checked["semantic_checksum"] != generated["semantic_checksum"]:
                    raise RuntimeError("container cross-reader semantic mismatch")
        exact = H.sha(paths[0]) == H.sha(paths[1])
        if hashes[0] != hashes[1]:
            raise RuntimeError("container fixture values differ")
        if metadata["wire_compat"] == "exact" and not exact:
            raise RuntimeError("container fixture bytes differ in exact-wire mode")
        evidence.append(dict(kind="container", case_id=case["id"], exact=exact,
                             both_writers_read_mutate_save_reload_by_both_readers=True,
                             files={v: dict(sha256=H.sha(p), size=p.stat().st_size)
                                    for v, p in zip(VARIANTS, paths)}))
    metadata["fixture_validation"] = evidence
    return result


def summarize(cases, rows):
    fields = ("elapsed_ms", "create_ms", "allocate_ms", "free_ms", "destroy_ms",
              "operation_p50_ms", "operation_p99_ms", "operation_max_ms", "growth_max_ms",
              "first_ms", "repeated_ms", "repeated_mean_ms", "growth_count", "allocation_failures",
              "container_moves", "container_copy_bytes_est", "used_end", "free_bytes", "cached_free_bytes",
              "live_payload_bytes", "wire_size", "final_logical_bytes", "final_accessible_bytes",
              "total_reserved_mapping_bytes", "request_count", "address_changes")
    summaries = []
    for case in cases:
        chosen = [r for r in rows if r["case_id"] == case["id"] and r["phase"] == "measured"]
        item = dict(case, variants={})
        pairs = {}
        for row in chosen:
            pairs.setdefault(row["pair"], {})[row["variant"]] = row
        for variant in VARIANTS:
            group = [r for r in chosen if r["variant"] == variant]
            values = {f: H.distribution([r[f] for r in group]) for f in fields}
            for name in ("memory_before", "memory_live", "memory_after_validation", "memory_after_cleanup"):
                values[name] = {f: H.distribution([r[name][f] for r in group])
                                for f in ("rss_bytes", "virtual_bytes", "peak_rss_bytes")}
                values[name]["available"] = all(r[name]["available"] for r in group)
            for name in ("rss_bytes", "virtual_bytes"):
                values[f"live_minus_before_{name}"] = H.distribution([
                    r["memory_live"][name] - r["memory_before"][name] for r in group])
            values["semantic_checksum"] = group[0]["semantic_checksum"]
            values["timing_scope"] = group[0]["timing_scope"]
            item["variants"][variant] = values
        item["paired_before_over_after"] = H.distribution([
            pair["before"]["elapsed_ms"] / pair["after"]["elapsed_ms"] for pair in pairs.values()])
        summaries.append(item)
    return summaries


def write_summaries(out, summaries):
    (out / "summary.json").write_text(json.dumps(summaries, indent=2) + "\n")
    with (out / "summary.csv").open("w", newline="") as stream:
        columns = ["case_id", "before_ms", "after_ms", "ratio", "ratio_q25", "ratio_q75",
                   "before_allocate_ms", "after_allocate_ms", "before_free_ms", "after_free_ms",
                   "before_used_end", "after_used_end", "before_container_moves", "after_container_moves",
                   "before_live_rss_mib", "after_live_rss_mib"]
        writer = csv.DictWriter(stream, columns); writer.writeheader()
        for case in summaries:
            ratio = case["paired_before_over_after"]
            row = dict(case_id=case["id"], ratio=ratio["median"], ratio_q25=ratio["q25"], ratio_q75=ratio["q75"])
            for variant in VARIANTS:
                data = case["variants"][variant]
                row[variant + "_ms"] = data["elapsed_ms"]["median"]
                for name in ("allocate_ms", "free_ms", "used_end", "container_moves"):
                    row[variant + "_" + name] = data[name]["median"]
                row[variant + "_live_rss_mib"] = data["memory_live"]["rss_bytes"]["median"] / MIB
            writer.writerow(row)


def verify_manifests(out, metadata):
    for name, key in (("run_vm_improvements.py", "runner_sha256"),
                      ("run_vm_perf_compare.py", "historical_runner_sha256")):
        expected = metadata[key]
        if H.sha(ROOT / "tools" / name) != expected or H.sha(out / "snapshots" / name) != expected:
            raise RuntimeError(f"prepared runner changed: {name}; prepare a fresh comparison")
    for variant, manifest in metadata["manifests"].items():
        for name, expected in manifest.items():
            path = out / variant if name == "binary_sha256" else out / "snapshots" / variant / name
            if H.sha(path) != expected:
                raise RuntimeError(f"prepared snapshot changed: {path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path)
    parser.add_argument("--after", type=Path)
    parser.add_argument("--suite", default="all", help="comma-separated transport,owner,containers,diagnostics")
    parser.add_argument("--case-filter", help="keep case IDs containing this string")
    parser.add_argument("--case-ids", help="comma-separated exact case IDs; selects a finite guard matrix")
    parser.add_argument("--wire-compat", choices=("exact", "semantic"), default="exact")
    parser.add_argument("--runs", type=int, default=7)
    parser.add_argument("--warmups", type=int, default=1)
    parser.add_argument("--compiler", default="/usr/local/bin/clang++")
    parser.add_argument("--sdk", default="/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--resume", type=Path)
    args = parser.parse_args()
    if args.runs < 1 or args.warmups < 0:
        parser.error("runs must be positive; warmups nonnegative")
    if args.resume:
        out = args.resume.resolve()
        metadata = json.loads((out / "metadata.json").read_text())
        if (out / "raw.jsonl").exists():
            raise RuntimeError("prepared directory already contains trials; choose a new output directory")
        verify_manifests(out, metadata)
        cases = metadata["cases"]
    else:
        if not args.before or not args.after:
            parser.error("--before and --after snapshot directories are required")
        allowed = {"transport", "owner", "containers", "diagnostics"}
        suites = allowed if args.suite == "all" else set(args.suite.split(","))
        if not suites <= allowed:
            parser.error("unknown suite")
        cases = matrix(suites)
        if args.case_filter:
            cases = [c for c in cases if args.case_filter in c["id"]]
        if args.case_ids:
            requested = set(args.case_ids.split(","))
            unknown = requested - {c["id"] for c in cases}
            if unknown:
                parser.error("unknown or filtered case IDs: " + ", ".join(sorted(unknown)))
            cases = [c for c in cases if c["id"] in requested]
        if not cases:
            parser.error("no cases selected")
        stamp = dt.datetime.now().astimezone().strftime("%Y%m%dT%H%M%S%z")
        out = (args.output_dir or ROOT / "build/vm_improvements/results" / stamp).resolve()
        out.mkdir(parents=True, exist_ok=False)
        deps, manifests, commands = freeze_and_compile(args, out)
        metadata = dict(prepared_at=dt.datetime.now().astimezone().isoformat(),
                        before_source=str(args.before.resolve()), after_source=str(args.after.resolve()),
                        branch=H.output(["git", "branch", "--show-current"]), head=H.output(["git", "rev-parse", "HEAD"]),
                        compiler_version=H.output([args.compiler, "--version"]),
                        platform=platform.platform(), machine=platform.machine(),
                        page_size=os.sysconf("SC_PAGE_SIZE"), cpu_count=os.cpu_count(),
                        runs=args.runs, warmups=args.warmups, cases=cases, wire_compat=args.wire_compat,
                        manifests=manifests, compile_commands=commands, submodules=deps,
                        runner_sha256=H.sha(__file__), historical_runner_sha256=H.sha(ROOT / "tools/run_vm_perf_compare.py"),
                        schedule="fresh child each sample; sequential cases and pairs; alternating before-first/after-first",
                        validation="full values outside timer; semantic hash and request counts across stages; wire equality only in exact mode",
                        memory="Mach TASK_VM_INFO or proc smaps_rollup; RSS is sampled resident memory, not commit charge",
                        latency="small-sample operation/block timing; not a production p99 estimate",
                        owner_probes="not instrumented; measured count and thread scaling, phase wall times",
                        historical_counter_limits="zeroed_new_bytes and enable estimates cover explicit grow only; container_copy_bytes_est counts old live bytes only when the container address changes",
                        diagnostics="current policy only; no pool or reclaim prototype is claimed")
        (out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    if args.prepare_only:
        print(f"Prepared; run later with --resume {out}", flush=True)
        return
    metadata["started_at"] = dt.datetime.now().astimezone().isoformat()
    metadata["load_average_start"] = os.getloadavg()
    if platform.system() == "Darwin":
        metadata["host"] = {key: H.output(["sysctl", "-n", key]) for key in
                            ("hw.memsize", "hw.physicalcpu", "hw.logicalcpu", "machdep.cpu.brand_string")}
        metadata["power_source"] = H.output(["pmset", "-g", "batt"])
        metadata["swap_start"] = H.output(["sysctl", "-n", "vm.swapusage"])
    fixture_paths = fixtures(out, cases, metadata)
    (out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    rows = []
    with (out / "raw.jsonl").open("w") as raw:
        for index, case in enumerate(cases):
            print(f"[{index + 1}/{len(cases)}] {case['id']}", flush=True)
            expected = None
            wire_expected = None
            for phase, count in (("warmup", metadata["warmups"]), ("measured", metadata["runs"])):
                for pair in range(count):
                    variants = VARIANTS if pair % 2 == 0 else tuple(reversed(VARIANTS))
                    for position, variant in enumerate(variants):
                        row = trial(out / variant, case, fixture_paths.get(case["size"]) if case["workload"] == "load" else None)
                        identity = (row["semantic_checksum"], row["request_count"], row["size"], row["requested_cycles"])
                        if expected is None:
                            expected = identity
                        elif expected != identity:
                            raise RuntimeError(f"semantic/request mismatch in {case['id']}: {identity} vs {expected}")
                        if metadata["wire_compat"] == "exact":
                            wire = (row["wire_checksum"], row["wire_size"])
                            if wire_expected is None:
                                wire_expected = wire
                            elif wire != wire_expected:
                                raise RuntimeError(f"wire mismatch in exact mode: {case['id']}")
                        row.update(case_id=case["id"], phase=phase, pair=pair,
                                   execution_position=position, sequence=len(rows))
                        rows.append(row); raw.write(json.dumps(row) + "\n"); raw.flush()
            summary = summarize([case], rows)[0]
            print("  median before={:.4f} ms after={:.4f} ms".format(
                summary["variants"]["before"]["elapsed_ms"]["median"],
                summary["variants"]["after"]["elapsed_ms"]["median"]), flush=True)
    write_summaries(out, summarize(cases, rows))
    verify_manifests(out, metadata)
    metadata["finished_at"] = dt.datetime.now().astimezone().isoformat()
    metadata["load_average_end"] = os.getloadavg()
    if platform.system() == "Darwin":
        metadata["swap_end"] = H.output(["sysctl", "-n", "vm.swapusage"])
    metadata["raw_sha256"] = H.sha(out / "raw.jsonl")
    metadata["measured_trials"] = sum(r["phase"] == "measured" for r in rows)
    metadata["warmup_trials"] = sum(r["phase"] == "warmup" for r in rows)
    with (out / "raw.jsonl").open("rb") as source, gzip.open(out / "raw.jsonl.gz", "wb") as target:
        shutil.copyfileobj(source, target)
    with tarfile.open(out / "source_snapshots.tar.gz", "w:gz") as archive:
        archive.add(out / "snapshots", arcname="snapshots")
    metadata["archive_sha256"] = H.sha(out / "source_snapshots.tar.gz")
    (out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"Results: {out}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.SubprocessError) as error:
        print(f"Comparison failed: {error}", file=sys.stderr)
        sys.exit(1)
