#!/usr/bin/env python3
"""Run the unchanged historical growth/append harness against two VM stages."""
import argparse
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
import tarfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("historical", ROOT / "tools/run_vm_perf_compare.py")
H = importlib.util.module_from_spec(spec)
spec.loader.exec_module(H)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--compiler", default="/usr/local/bin/clang++")
    parser.add_argument("--sdk", default="/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk")
    args = parser.parse_args()
    out = args.output_dir.resolve(); out.mkdir(parents=True, exist_ok=False)
    commands, manifests = {}, {}
    common = [args.compiler, "-O3", "-DNDEBUG", "-std=c++26", "-freflection",
              "-fexpansion-statements", "-stdlib=libc++", "-DXOFFSET_BENCH_VM=1"]
    if platform.system() == "Darwin":
        common += ["-arch", platform.machine(), "-isysroot", args.sdk, "-mmacosx-version-min=10.15"]
    for lib in H.BOOST_LIBS:
        common += ["-isystem", ROOT / "external/boost/libs" / lib / "include"]
    common += ["-isystem", ROOT / "external/typelayout/include"]
    for variant in ("before", "after"):
        snap = out / "snapshots" / variant; snap.mkdir(parents=True)
        for filename in ("xoffsetdatastructure.hpp", "xoffset_virtual_memory.hpp"):
            shutil.copy2(getattr(args, variant) / filename, snap / filename)
        shutil.copy2(ROOT / "tools/bench_vm_compare.cpp", snap)
        argv = common + ["-I", snap, snap / "bench_vm_compare.cpp", "-L/usr/local/lib",
                         "-Wl,-rpath,/usr/local/lib", "-o", out / variant]
        print(f"Compiling {variant}", flush=True)
        proc = H.command(argv, text=True)
        (out / f"compile_{variant}.log").write_text(proc.stdout + proc.stderr)
        commands[variant] = list(map(str, argv))
        manifests[variant] = {p.name: H.sha(p) for p in snap.iterdir()}
        manifests[variant]["binary_sha256"] = H.sha(out / variant)
    shutil.copy2(__file__, out / "snapshots")
    shutil.copy2(ROOT / "tools/run_vm_perf_compare.py", out / "snapshots")
    cases = [dict(workload="grow_equal_reserve", size=128*1024*1024, reserve=0, cycles=8),
             dict(workload="append_preallocated", size=10_000_000, reserve=0, cycles=8)]
    metadata = dict(started_at=dt.datetime.now().astimezone().isoformat(),
                    before_source=str(args.before.resolve()), after_source=str(args.after.resolve()),
                    manifests=manifests, compile_commands=commands, cases=cases, runs=7, warmups=1,
                    compiler_version=H.output([args.compiler, "--version"]),
                    platform=platform.platform(), page_size=os.sysconf("SC_PAGE_SIZE"),
                    submodules={name: H.output(["git", "-C", ROOT / "external" / name, "rev-parse", "HEAD"])
                                for name in ("boost", "typelayout")},
                    runner_sha256=H.sha(__file__), load_average_start=os.getloadavg(),
                    validation="unchanged historical harness; complete values checked outside timers; checksum/input counts identical; VM bases stable; structural counts may differ",
                    scope="append_preallocated means preallocated arena, not a vector reserve before the loop",
                    schedule="sequential fresh child processes; alternating before-first/after-first within each case")
    if platform.system() == "Darwin":
        metadata["swap_start"] = H.output(["sysctl", "-n", "vm.swapusage"])
    rows = []
    with (out / "raw.jsonl").open("w") as stream:
        for case in cases:
            expected = None
            for phase, count in (("warmup", 1), ("measured", 7)):
                for pair in range(count):
                    variants = ("before", "after") if pair % 2 == 0 else ("after", "before")
                    for position, variant in enumerate(variants):
                        argv = [out / variant]
                        for key, value in case.items():
                            argv += ["--" + key, str(value)]
                        row = json.loads(H.command(argv, text=True, timeout=300).stdout)
                        identity = (row["checksum"], row["size"], row["operation_count"], row["total_requested_payload_bytes"])
                        if expected is None: expected = identity
                        if identity != expected or row["address_changes"]:
                            raise RuntimeError("guard semantic values/input count/base stability mismatch")
                        row.update(variant=variant, case_id=case["workload"], phase=phase,
                                   pair=pair, execution_position=position, sequence=len(rows))
                        rows.append(row); stream.write(json.dumps(row) + "\n"); stream.flush()
            print(case["workload"], "complete", flush=True)
    summaries = []
    for case in cases:
        chosen = [r for r in rows if r["case_id"] == case["workload"] and r["phase"] == "measured"]
        item = dict(case, variants={})
        for variant in ("before", "after"):
            group = [r for r in chosen if r["variant"] == variant]
            item["variants"][variant] = {field: H.distribution([r[field] for r in group])
                for field in ("elapsed_ms", "growth_max_ms", "allocation_failures", "growth_count",
                              "final_logical_bytes", "final_accessible_bytes", "address_changes")}
        pairs = {}
        for row in chosen: pairs.setdefault(row["pair"], {})[row["variant"]] = row
        item["paired_before_over_after"] = H.distribution([
            pair["before"]["elapsed_ms"] / pair["after"]["elapsed_ms"] for pair in pairs.values()])
        summaries.append(item)
    (out / "summary.json").write_text(json.dumps(summaries, indent=2) + "\n")
    metadata.update(finished_at=dt.datetime.now().astimezone().isoformat(),
                    measured_trials=28, warmup_trials=4, raw_sha256=H.sha(out / "raw.jsonl"),
                    load_average_end=os.getloadavg())
    if platform.system() == "Darwin": metadata["swap_end"] = H.output(["sysctl", "-n", "vm.swapusage"])
    with (out / "raw.jsonl").open("rb") as source, gzip.open(out / "raw.jsonl.gz", "wb") as target:
        shutil.copyfileobj(source, target)
    with tarfile.open(out / "source_snapshots.tar.gz", "w:gz") as archive:
        archive.add(out / "snapshots", arcname="snapshots")
    metadata["archive_sha256"] = H.sha(out / "source_snapshots.tar.gz")
    (out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"Results: {out}", flush=True)


if __name__ == "__main__":
    main()
