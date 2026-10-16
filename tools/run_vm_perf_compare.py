#!/usr/bin/env python3
"""Compare a frozen 07d6499 vector header with a frozen local VM header.

Each sample is a fresh process. Runs are strictly sequential, and the order of
the two variants alternates within every case. Only harness timers are reported
as workload time; process startup and post-timing validation are excluded.
"""
import argparse
import csv
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import statistics as stats
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BASELINE = "07d649909d7f43ca0d11b85accb6d9dab991e284"
MIB = 1024 * 1024
VARIANTS = ("vector_07d6499", "stable_vm")
BOOST_LIBS = ("container", "core", "config", "throw_exception", "static_assert",
              "container_hash", "type_traits", "winapi", "intrusive", "predef")


def command(argv, **kwargs):
    return subprocess.run([str(x) for x in argv], cwd=ROOT, check=True,
                          capture_output=True, **kwargs)


def output(argv):
    return command(argv, text=True).stdout.strip()


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def matrix():
    cases = []
    def add(workload, sizes, unit, reserve=0, cycles=8):
        for size in sizes:
            cases.append(dict(id=f"{workload}_{size}", workload=workload,
                              size=size, size_unit=unit, reserve=reserve,
                              cycles=cycles))
    add("grow_default", [64*MIB, 128*MIB], "bytes")
    add("grow_equal_reserve", [64*MIB, 128*MIB], "bytes")
    add("append_growth", [1_000_000, 10_000_000], "elements")
    add("append_preallocated", [1_000_000, 10_000_000], "elements")
    add("many_default", [1000, 5000], "buffers")
    add("many_equal_reserve", [1000, 5000], "buffers", reserve=64*1024)
    add("save_verified", [8*MIB, 64*MIB], "bytes")
    add("load_verified", [8*MIB, 64*MIB], "bytes")
    add("save_grow_cycles", [8*MIB, 64*MIB], "bytes")
    return cases


def freeze_and_compile(args, out):
    submodules = {}
    for name in ("boost", "typelayout"):
        path = ROOT / "external" / name
        expected = output(["git", "ls-tree", BASELINE, f"external/{name}"]).split()[2]
        actual = output(["git", "-C", path, "rev-parse", "HEAD"])
        dirty = output(["git", "-C", path, "status", "--porcelain"])
        if expected != actual or dirty:
            raise RuntimeError(f"{name}: dependencies must be clean at baseline {expected}")
        submodules[name] = actual
    common = [args.compiler, "-O3", "-DNDEBUG", "-std=c++26", "-freflection",
              "-fexpansion-statements", "-stdlib=libc++"]
    if platform.system() == "Darwin":
        common += ["-arch", platform.machine(), "-isysroot", args.sdk,
                   "-mmacosx-version-min=10.15"]
    for lib in BOOST_LIBS:
        common += ["-isystem", str(ROOT / "external/boost/libs" / lib / "include")]
    common += ["-isystem", str(ROOT / "external/typelayout/include")]
    link = ["-L/usr/local/lib", "-Wl,-rpath,/usr/local/lib"]
    binaries, manifests, compile_commands = {}, {}, {}
    harness_bytes = (ROOT / "tools/bench_vm_compare.cpp").read_bytes()
    for index, variant in enumerate(VARIANTS):
        snap = out / "snapshots" / variant
        snap.mkdir(parents=True)
        header = snap / "xoffsetdatastructure.hpp"
        if index == 0:
            header.write_bytes(command(["git", "show", f"{BASELINE}:xoffsetdatastructure.hpp"]).stdout)
        else:
            shutil.copy2(ROOT / "xoffsetdatastructure.hpp", header)
            shutil.copy2(ROOT / "xoffset_virtual_memory.hpp", snap)
        source = snap / "bench_vm_compare.cpp"
        source.write_bytes(harness_bytes)
        binary = out / variant
        argv = common + [f"-DXOFFSET_BENCH_VM={index}", "-I", str(snap), str(source)] + link + ["-o", str(binary)]
        print(f"Compiling {variant}", flush=True)
        try:
            result = command(argv, text=True)
        except subprocess.CalledProcessError as exc:
            (out / f"compile_{variant}.log").write_text(exc.stdout + exc.stderr)
            raise
        (out / f"compile_{variant}.log").write_text(result.stdout + result.stderr)
        binaries[variant] = binary
        manifests[variant] = {p.name: sha(p) for p in snap.iterdir()}
        manifests[variant]["binary_sha256"] = sha(binary)
        compile_commands[variant] = argv
    return binaries, submodules, manifests, compile_commands


def trial(binary, case, wire_path=None):
    argv = [binary, "--workload", case["workload"], "--size", str(case["size"]),
            "--reserve", str(case["reserve"]), "--cycles", str(case["cycles"])]
    if wire_path:
        argv += ["--wire-path", wire_path]
    start = time.monotonic()
    try:
        proc = command(argv, text=True, timeout=300)
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(f"trial failed: {argv}\n{exc.stderr}") from exc
    row = json.loads(proc.stdout)
    if row["variant"] != Path(binary).name:
        raise RuntimeError("harness returned wrong variant")
    if row["workload"] != case["workload"] or row["size"] != case["size"]:
        raise RuntimeError("harness returned wrong workload/size")
    if not row["memory_before"]["available"] or not row["memory_live"]["available"]:
        raise RuntimeError("memory measurements are unavailable")
    row["process_wall_ms"] = (time.monotonic() - start) * 1000
    return row


def quantile(values, q):
    ordered = sorted(values)
    position = (len(ordered)-1)*q
    low = int(position)
    high = min(low+1, len(ordered)-1)
    return ordered[low] + (ordered[high]-ordered[low])*(position-low)


def distribution(values):
    return dict(median=stats.median(values), mean=stats.mean(values),
                min=min(values), max=max(values), q25=quantile(values, .25),
                q75=quantile(values, .75))


def summarize(cases, rows):
    summaries = []
    fields = ("elapsed_ms", "create_ms", "allocate_ms", "free_ms", "destroy_ms",
              "growth_max_ms", "operation_max_ms", "operation_p50_ms", "operation_p99_ms",
              "address_changes", "relocation_copy_bytes_est", "newly_exposed_bytes",
              "zeroed_new_bytes", "allocation_failures", "growth_count",
              "initial_reserved_bytes", "final_reserved_bytes", "final_logical_bytes",
              "final_accessible_bytes", "total_reserved_mapping_bytes", "wire_size",
              "vm_enable_calls_est", "vm_enabled_bytes_est")
    for case in cases:
        selected = [r for r in rows if r["case_id"] == case["id"] and r["phase"] == "measured"]
        groups = {v: [r for r in selected if r["variant"] == v] for v in VARIANTS}
        summary = dict(case)
        summary["variants"] = {}
        for variant, group in groups.items():
            values = {f: distribution([r[f] for r in group]) for f in fields}
            for name in ("memory_before", "memory_after", "memory_live", "memory_peak",
                         "memory_after_validation"):
                values[name] = {f: distribution([r[name][f] for r in group])
                                for f in ("rss_bytes", "virtual_bytes", "peak_rss_bytes")}
            for name in ("rss_bytes", "virtual_bytes"):
                values[f"live_minus_before_{name}"] = distribution([
                    r["memory_live"][name]-r["memory_before"][name] for r in group])
            values["timing_scope"] = group[0]["timing_scope"]
            values["operation_unit"] = group[0]["operation_unit"]
            values["checksum"] = group[0]["checksum"]
            summary["variants"][variant] = values
        pairs = {}
        for row in selected:
            pairs.setdefault(row["pair"], {})[row["variant"]] = row
        ratios = [p[VARIANTS[0]]["elapsed_ms"] / p[VARIANTS[1]]["elapsed_ms"] for p in pairs.values()]
        summary["paired_baseline_over_vm"] = distribution(ratios)
        summaries.append(summary)
    return summaries


def save_summary(out, summaries):
    (out / "summary.json").write_text(json.dumps(summaries, ensure_ascii=False, indent=2)+"\n")
    with (out / "summary.csv").open("w", newline="") as stream:
        fields = ["case_id", "size", "size_unit", "baseline_ms", "vm_ms", "paired_speed_ratio",
                  "ratio_q25", "ratio_q75", "baseline_growth_max_ms", "vm_growth_max_ms",
                  "baseline_rss_live_mib", "vm_rss_live_mib", "baseline_rss_delta_mib",
                  "vm_rss_delta_mib", "baseline_virtual_delta_mib", "vm_virtual_delta_mib",
                  "baseline_address_changes", "vm_address_changes", "baseline_copy_est_mib",
                  "vm_copy_est_mib"]
        writer = csv.DictWriter(stream, fields)
        writer.writeheader()
        for case in summaries:
            b, v = (case["variants"][variant] for variant in VARIANTS)
            ratio = case["paired_baseline_over_vm"]
            row = dict(case_id=case["id"], size=case["size"], size_unit=case["size_unit"],
                       baseline_ms=b["elapsed_ms"]["median"], vm_ms=v["elapsed_ms"]["median"],
                       paired_speed_ratio=ratio["median"], ratio_q25=ratio["q25"], ratio_q75=ratio["q75"])
            for prefix, data in (("baseline", b), ("vm", v)):
                row[prefix+"_growth_max_ms"] = data["growth_max_ms"]["median"]
                row[prefix+"_rss_live_mib"] = data["memory_live"]["rss_bytes"]["median"]/MIB
                row[prefix+"_rss_delta_mib"] = data["live_minus_before_rss_bytes"]["median"]/MIB
                row[prefix+"_virtual_delta_mib"] = data["live_minus_before_virtual_bytes"]["median"]/MIB
                row[prefix+"_address_changes"] = data["address_changes"]["median"]
                row[prefix+"_copy_est_mib"] = data["relocation_copy_bytes_est"]["median"]/MIB
            writer.writerow(row)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runs", type=int, default=7)
    parser.add_argument("--warmups", type=int, default=1)
    parser.add_argument("--compiler", default="/usr/local/bin/clang++")
    parser.add_argument("--sdk", default="/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--workloads", help="comma-separated workload filter")
    args = parser.parse_args()
    if args.runs < 2 or args.warmups < 0:
        parser.error("--runs must be >=2 and --warmups >=0")
    stamp = dt.datetime.now().astimezone().strftime("%Y%m%dT%H%M%S%z")
    out = (args.output_dir or ROOT / "build/perf_vm" / stamp).resolve()
    out.mkdir(parents=True, exist_ok=False)
    cases = matrix()
    if args.workloads:
        allowed = set(args.workloads.split(","))
        cases = [c for c in cases if c["workload"] in allowed]
        if not cases:
            parser.error("no matching workloads")
    binaries, deps, manifests, compile_commands = freeze_and_compile(args, out)
    shutil.copy2(__file__, out / "snapshots/run_vm_perf_compare.py")
    metadata = dict(started_at=dt.datetime.now().astimezone().isoformat(), baseline_commit=BASELINE,
                    branch=output(["git", "branch", "--show-current"]),
                    head=output(["git", "rev-parse", "HEAD"]),
                    worktree_status=output(["git", "status", "--porcelain"]),
                    worktree_diff_sha256=hashlib.sha256(command(["git", "diff", "HEAD"]).stdout).hexdigest(),
                    compiler_version=output([args.compiler, "--version"]),
                    platform=platform.platform(), machine=platform.machine(),
                    runner_sha256=sha(__file__), load_average_at_start=os.getloadavg(),
                    page_size=os.sysconf("SC_PAGE_SIZE"), cpu_count=os.cpu_count(),
                    submodules=deps, manifests=manifests, compile_commands=compile_commands,
                    runs=args.runs, warmups=args.warmups, cases=cases,
                    schedule="cases in listed order; each pair alternates baseline-first/VM-first; no concurrent trials",
                    timer="std::chrono::steady_clock; harness elapsed_ms only",
                    memory="Mach TASK_VM_INFO on Darwin; RSS before/live/after and lifetime peak; not commit charge",
                    output_directory=str(out))
    if platform.system() == "Darwin":
        metadata["host"] = {f: output(["sysctl", "-n", f]) for f in
                            ("hw.memsize", "hw.physicalcpu", "hw.logicalcpu", "machdep.cpu.brand_string")}
        metadata["os_version"] = output(["sw_vers"])
        metadata["power_source"] = output(["pmset", "-g", "batt"])
        metadata["swap_at_start"] = output(["sysctl", "-n", "vm.swapusage"])
    (out / "metadata.json").write_text(json.dumps(metadata, ensure_ascii=False, indent=2)+"\n")
    fixtures = {}
    for case in cases:
        if case["workload"] != "load_verified":
            continue
        paths = []
        fixture_case = dict(case, workload="prepare_wire")
        for variant in VARIANTS:
            path = out / f"wire_{case['size']}_{variant}.bin"
            trial(binaries[variant], fixture_case, str(path))
            paths.append(path)
        if paths[0].read_bytes() != paths[1].read_bytes():
            raise RuntimeError(f"wire fixtures differ for {case['size']} bytes")
        fixtures[case["id"]] = paths[0]
        metadata.setdefault("wire_fixtures", {})[str(case["size"])] = dict(
            sha256=sha(paths[0]), size=paths[0].stat().st_size, byte_identical=True)
    rows = []
    with (out / "raw.jsonl").open("w") as raw:
        for case_index, case in enumerate(cases):
            print(f"[{case_index+1}/{len(cases)}] {case['id']}", flush=True)
            expected = None
            for phase, count in (("warmup", args.warmups), ("measured", args.runs)):
                for pair in range(count):
                    order = VARIANTS if pair % 2 == 0 else tuple(reversed(VARIANTS))
                    for position, variant in enumerate(order):
                        row = trial(binaries[variant], case, fixtures.get(case["id"]))
                        identity = (row["checksum"], row["wire_size"], row["allocation_failures"],
                                    row["growth_count"], row["final_logical_bytes"],
                                    row["operation_count"], row["newly_exposed_bytes"],
                                    row["total_requested_payload_bytes"], row["requested_cycles"])
                        if expected is None:
                            expected = identity
                        elif identity != expected:
                            raise RuntimeError(f"correctness/operation-count mismatch in {case['id']}: {identity} vs {expected}")
                        if variant == "stable_vm" and row["address_changes"] != 0:
                            raise RuntimeError("stable VM moved its base address")
                        row.update(case_id=case["id"], phase=phase, pair=pair,
                                   execution_position=position, sequence=len(rows))
                        rows.append(row)
                        raw.write(json.dumps(row, ensure_ascii=False)+"\n")
                        raw.flush()
            pair_rows = [r for r in rows if r["case_id"] == case["id"] and r["phase"] == "measured"]
            med = {v: stats.median(r["elapsed_ms"] for r in pair_rows if r["variant"] == v) for v in VARIANTS}
            print(f"  median: vector={med[VARIANTS[0]]:.3f} ms, VM={med[VARIANTS[1]]:.3f} ms", flush=True)
    summaries = summarize(cases, rows)
    save_summary(out, summaries)
    metadata["finished_at"] = dt.datetime.now().astimezone().isoformat()
    metadata["load_average_at_end"] = os.getloadavg()
    if platform.system() == "Darwin":
        metadata["swap_at_end"] = output(["sysctl", "-n", "vm.swapusage"])
    metadata["raw_sha256"] = sha(out / "raw.jsonl")
    metadata["measured_trials"] = sum(r["phase"] == "measured" for r in rows)
    metadata["validation"] = "all trials checked complete payloads; cross-variant identities matched; VM bases stable"
    (out / "metadata.json").write_text(json.dumps(metadata, ensure_ascii=False, indent=2)+"\n")
    print(f"Results: {out}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.SubprocessError) as error:
        print(f"Comparison failed: {error}", file=sys.stderr)
        sys.exit(1)
