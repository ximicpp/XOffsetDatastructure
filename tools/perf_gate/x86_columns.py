#!/usr/bin/env python3
"""Cloud x86 and client-uplink columns of the SLG scorecard: paired ratios and
the 3.6 thresholds (docs/RELEASING.md section 2a).

usage: x86_columns.py DIR [--tag TAG] [--xo xoffset_trusted]
                          [--rivals zpp_bits,cista_unchecked] [--json OUT]

DIR holds one bench/slg JSONL per process, named <TAG>_<format>_<batch>.jsonl
(tools/perf_gate/x86_columns.sh writes them so: every format of a batch runs
in its own process, the batches interleaved). Every format of one batch runs
the same seed, so cycle c of batch b carries the same messages in every
format; the ratio of a cell is taken per (batch, cycle >= 1) and the cycle
ratios of a batch are averaged in log space. The geometric mean over batches
is the point estimate; its one-sided 95% upper bound uses the spread between
batches (separate processes, so it covers each process's own placement),
Student t with nb - 1 degrees of freedom.

Verdict per cell and rival (XOffset ÷ rival, below 1 = XOffset faster):
  main cells   pass: GM <= 1.00; warn: 1.00 < GM <= 1.05 (inside the cloud A/A
               spread: run 10 more batches and judge all of them); fail: GM > 1.05
  UL.send      ceiling (4.0 R5 owns the loss): pass GM <= UL_SEND_CEILING, else fail
  report       printed, no verdict
The upper bound is printed, not judged: on the cloud machine an A/A run (the
same code built twice, 10 batches) gave GMs 0.98-1.04 and upper bounds up to
1.12 (docs/RELEASING.md section 2a).
Exit status 1 when any cell fails, else 0. A cell missing from the data is
listed as missing (the run lacked that case, e.g. D3 without SLG_D3=1).
"""
import argparse
import glob
import json
import math
import os
import re
import statistics
import sys

# (column, key, kind). Server cells time *.send, client cells the warm single
# device's receive + apply (*W.recv), as the SLG scorecard's main columns.
CELLS = [
    ("x86 server", "L.send", "main"),
    ("x86 server", "EB.send", "main"),
    ("x86 server", "LD.send", "main"),
    ("x86 server", "UP.send", "main"),
    ("x86 server", "UC.send", "main"),
    ("x86 server", "DP.send", "main"),
    ("x86 server", "DC.send", "main"),
    ("x86 client", "EBW.recv", "main"),
    ("x86 client", "LDW.recv", "main"),
    ("x86 client", "UPW.recv", "main"),
    ("x86 client", "UCW.recv", "main"),
    ("x86 client", "USW.recv", "main"),
    ("x86 client", "DPTW.recv", "main"),
    ("uplink", "UL.recv", "main"),
    ("uplink", "UL.send", "ceiling"),
    ("uplink", "ULF.recv", "report"),
]
MAIN_PASS, MAIN_FAIL, UL_SEND_CEILING = 1.00, 1.05, 1.45
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


def cell_ratio(xo, rival, key):
    """Per-batch mean log ratio over paired cycles."""
    logs = []
    for b in sorted(set(xo) & set(rival)):
        a, r = xo[b].get(key, {}), rival[b].get(key, {})
        pairs = [math.log(a[c] / r[c]) for c in a if c in r and a[c] > 0 and r[c] > 0]
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
    if kind == "ceiling":
        return "pass" if r["gm"] <= UL_SEND_CEILING else "fail"
    if r["gm"] > MAIN_FAIL:
        return "fail"
    return "pass" if r["gm"] <= MAIN_PASS else "warn"


def evaluate(directory, tag, xo_name, rivals):
    xo = batches(directory, tag, xo_name)
    rows = []
    for rival in rivals:
        rv = batches(directory, tag, rival)
        for column, key, kind in CELLS:
            r = cell_ratio(xo, rv, key) if xo and rv else None
            rows.append({"column": column, "key": key, "kind": kind, "rival": rival,
                         "ratio": r, "verdict": verdict(kind, r)})
    return rows


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("dir")
    ap.add_argument("--tag", default="head")
    ap.add_argument("--xo", default="xoffset_trusted")
    ap.add_argument("--rivals", default="zpp_bits,cista_unchecked")
    ap.add_argument("--json")
    a = ap.parse_args(argv)
    rows = evaluate(a.dir, a.tag, a.xo, [r for r in a.rivals.split(",") if r])
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
          f"(main: pass GM <= {MAIN_PASS:.2f}, fail GM > {MAIN_FAIL:.2f}; UL.send GM <= {UL_SEND_CEILING:.2f})")
    if a.json:
        with open(a.json, "w") as f:
            json.dump(rows, f, indent=1)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
