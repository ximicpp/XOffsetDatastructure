#!/usr/bin/env python3
"""Cloud x86 and client-uplink columns of the SLG scorecard: paired ratios and
the 3.6 thresholds (docs/RELEASING.md section 2a).

usage: x86_columns.py DIR [--tag TAG] [--d3-tag D3TAG] [--xo xoffset_trusted]
                          [--rivals zpp_bits,cista_unchecked] [--json OUT]

DIR holds one bench/slg JSONL per process, named <TAG>_<format>_<batch>.jsonl
(tools/perf_gate/x86_columns.sh writes them so: every format of a batch runs
in its own process, the batches interleaved). With --d3-tag, the XOffset
processes <D3TAG>_<xo>_<batch>.jsonl (SLG_D3=1, native on) give the D cells,
paired with each rival's U* rows of the same batch. Every format of one batch runs
the same seed, so cycle c of batch b carries the same messages in every
format; the ratio of a cell is taken per (batch, cycle >= 1) and the cycle
ratios of a batch are averaged in log space. The geometric mean over batches
is the point estimate; its one-sided 95% upper bound uses the spread between
batches (separate processes, so it covers each process's own placement),
Student t with nb - 1 degrees of freedom.

Verdict per cell and rival (XOffset ÷ rival, below 1 = XOffset faster):
  main cells   pass: GM <= 1.00; warn: 1.00 < GM <= 1.05 (inside the cloud A/A
               spread: run 10 more batches and judge all of them); fail: GM > 1.05
  report       printed, no verdict: UL.send (client uplink send; report-only
               from 4.0, susu 2026-11-12) and ULF.recv (full check)
The upper bound is printed, not judged: on the cloud machine an A/A run (the
same code built twice, 10 batches) gave GMs 0.98-1.04 and upper bounds up to
1.12 (docs/RELEASING.md section 2a).
Exit status 1 when any cell fails, else 0. A cell missing from the data is
listed as missing (the run lacked that case, e.g. D cells without --d3-tag).
"""
import argparse
import glob
import json
import math
import os
import re
import statistics
import sys

# (column, cell, kind, xo run, xo keys, rival keys). Server cells time *.send,
# client cells the warm single device's receive + apply (*W.recv), as the SLG
# scorecard's main columns (tools/perf_gate/scorecard.py). The D cells pair
# XOffset's shared-delta rows (the SLG_D3=1 processes, --d3-tag) with each
# rival's per-client U* rows from the main processes: rivals have no D3 path.
CELLS = [
    ("x86 server", "L.send", "main", "main", ["L.send"], ["L.send"]),
    ("x86 server", "EB.send", "main", "main", ["EB.send"], ["EB.send"]),
    ("x86 server", "LD.send", "main", "main", ["LD.send"], ["LD.send"]),
    ("x86 server", "UP.send", "main", "main", ["UP.send"], ["UP.send"]),
    ("x86 server", "UC.send", "main", "main", ["UC.send"], ["UC.send"]),
    ("x86 server", "DP.pub+send / UP.send", "main", "d3", ["DP.pub", "DP.send"], ["UP.send"]),
    ("x86 server", "DC.pub+send / UC.send", "main", "d3", ["DC.pub", "DC.send"], ["UC.send"]),
    ("x86 client", "EBW.recv", "main", "main", ["EBW.recv"], ["EBW.recv"]),
    ("x86 client", "LDW.recv", "main", "main", ["LDW.recv"], ["LDW.recv"]),
    ("x86 client", "UPW.recv", "main", "main", ["UPW.recv"], ["UPW.recv"]),
    ("x86 client", "UCW.recv", "main", "main", ["UCW.recv"], ["UCW.recv"]),
    ("x86 client", "USW.recv", "main", "main", ["USW.recv"], ["USW.recv"]),
    ("x86 client", "DPTW.recv / UPW.recv", "main", "d3", ["DPTW.recv"], ["UPW.recv"]),
    ("x86 client", "DCTW.recv / UCW.recv", "main", "d3", ["DCTW.recv"], ["UCW.recv"]),
    ("uplink", "UL.recv", "main", "main", ["UL.recv"], ["UL.recv"]),
    ("uplink", "UL.send", "report", "main", ["UL.send"], ["UL.send"]),
    ("uplink", "ULF.recv / UL.recv", "report", "main", ["ULF.recv"], ["UL.recv"]),
]
MAIN_PASS, MAIN_FAIL = 1.00, 1.05
# One-sided 95% Student t quantiles by degrees of freedom (>= 30: normal).
T95 = {1: 6.314, 2: 2.920, 3: 2.353, 4: 2.132, 5: 2.015, 6: 1.943, 7: 1.895, 8: 1.860,
       9: 1.833, 10: 1.812, 11: 1.796, 12: 1.782, 13: 1.771, 14: 1.761, 15: 1.753,
       16: 1.746, 17: 1.740, 18: 1.734, 19: 1.729, 20: 1.725, 25: 1.708}


def t95(df):
    if df >= 30:
        return 1.645
    return T95.get(df) or T95[max(k for k in T95 if k <= df)]


def cycles(path):
    """{metric: {cycle: ns}} for cycles >= 1."""
    out = {}
    with open(path) as f:
        for line in f:
            try:
                rec = json.loads(line)
            except ValueError:
                continue
            if "ns" not in rec or rec.get("cycle", 0) < 1:
                continue
            out.setdefault(rec["metric"], {})[rec["cycle"]] = rec["ns"]
    return out


def batches(directory, tag, fmt):
    pat = re.compile(re.escape(f"{tag}_{fmt}_") + r"(\d+)\.jsonl$")
    found = {}
    for p in glob.glob(os.path.join(directory, f"{tag}_{fmt}_*.jsonl")):
        m = pat.search(os.path.basename(p))
        if m:
            found[int(m.group(1))] = cycles(p)
    return found


def cycle_sum(run, keys, c):
    total = 0
    for k in keys:
        v = run.get(k, {}).get(c)
        if v is None or v <= 0:
            return None
        total += v
    return total


def cell_ratio(xo, rival, xkeys, rkeys=None):
    """Per-batch mean log ratio over paired cycles of the summed keys."""
    rkeys = rkeys or xkeys
    logs = []
    for b in sorted(set(xo) & set(rival)):
        cyc = set(xo[b].get(xkeys[0], {})) & set(rival[b].get(rkeys[0], {}))
        pairs = []
        for c in sorted(cyc):
            a, r = cycle_sum(xo[b], xkeys, c), cycle_sum(rival[b], rkeys, c)
            if a and r:
                pairs.append(math.log(a / r))
        if pairs:
            logs.append(statistics.fmean(pairs))
    if not logs:
        return None
    gm = math.exp(statistics.fmean(logs))
    if len(logs) < 2:
        return {"gm": gm, "upper": float("inf"), "batches": len(logs)}
    se = statistics.stdev(logs) / math.sqrt(len(logs))
    return {"gm": gm, "upper": math.exp(math.log(gm) + t95(len(logs) - 1) * se), "batches": len(logs)}


def verdict(kind, r):
    if r is None:
        return "missing"
    if kind == "report":
        return "report"
    if r["gm"] > MAIN_FAIL:
        return "fail"
    return "pass" if r["gm"] <= MAIN_PASS else "warn"


def evaluate(directory, tag, xo_name, rivals, d3_tag=None):
    xo = {"main": batches(directory, tag, xo_name),
          "d3": batches(directory, d3_tag, xo_name) if d3_tag else {}}
    rows = []
    for rival in rivals:
        rv = batches(directory, tag, rival)
        for column, key, kind, run, xkeys, rkeys in CELLS:
            r = cell_ratio(xo[run], rv, xkeys, rkeys) if xo[run] and rv else None
            rows.append({"column": column, "key": key, "kind": kind, "rival": rival,
                         "ratio": r, "verdict": verdict(kind, r)})
    return rows


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("dir")
    ap.add_argument("--tag", default="head")
    ap.add_argument("--d3-tag", help="tag of the SLG_D3=1 XOffset processes (the D cells)")
    ap.add_argument("--xo", default="xoffset_trusted")
    ap.add_argument("--rivals", default="zpp_bits,cista_unchecked")
    ap.add_argument("--json")
    a = ap.parse_args(argv)
    rows = evaluate(a.dir, a.tag, a.xo, [r for r in a.rivals.split(",") if r], a.d3_tag)
    print(f"| column | key | ÷ rival | GM | one-sided 95% upper | batches | verdict |")
    print(f"| --- | --- | --- | ---: | ---: | ---: | --- |")
    for row in rows:
        r = row["ratio"]
        if r is None:
            print(f"| {row['column']} | {row['key']} | {row['rival']} | | | | missing |")
            continue
        print(f"| {row['column']} | {row['key']} | {row['rival']} | {r['gm']:.3f} | {r['upper']:.3f} | "
              f"{r['batches']} | {row['verdict']} |")
    failed = [r for r in rows if r["verdict"] == "fail"]
    warned = [r for r in rows if r["verdict"] == "warn"]
    print(f"\n{'FAIL' if failed else 'PASS'}: {len(failed)} fail, {len(warned)} warn "
          f"(main: pass GM <= {MAIN_PASS:.2f}, fail GM > {MAIN_FAIL:.2f}; UL.send and ULF.recv report only)")
    if a.json:
        with open(a.json, "w") as f:
            json.dump(rows, f, indent=1)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
