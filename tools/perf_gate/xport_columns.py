#!/usr/bin/env python3
"""Transport-model columns of the whole-block cells (4.0 W3, report only).

usage: xport_columns.py RAW_DIR [--tag x] [--xo xoffset_trusted]
                        [--rivals zpp_bits,cista_unchecked] [--json OUT]

RAW_DIR holds <tag>_<format>_<batch>.jsonl from bench/slg processes run with
SLG_XPORT=1 (SLG_REUSE=1 SLG_CASES=demo SLG_NATIVE_OFF=1), one per format and
batch, same seed. With SLG_XPORT=1 the cells L, B, S, SD record <cell>.send+T
(the send ends with every wire byte copied into one reused transport buffer)
and <cell>.recv+T (the receive starts by reading that buffer into memory the
receiver owns), the same model for every format; see bench/slg/bench/runner.hpp.
Per cell: ms per cycle (median over batches) for every format, and XOffset's
ratio to each rival: geometric mean over batches of the per-batch mean log
ratio over cycles >= 1. Report only: not scored (a scored +T column would be a
goal change). W3 step 0 (2026-11-12) used these columns to measure XOffset's
borrowed send / direct receive prototypes; neither met its 20% gate.
"""
import argparse, glob, json, math, os, re, statistics

KEYS = [f"{c}.{s}+T" for s in ("send", "recv") for c in ("L", "B", "S", "SD")]


def load(d, tag, fmt):
    out = {}
    for p in glob.glob(os.path.join(d, f"{tag}_{fmt}_*.jsonl")):
        m = re.search(re.escape(f"{tag}_{fmt}_") + r"(\d+)\.jsonl$", os.path.basename(p))
        if not m:
            continue
        ns = {}
        for line in open(p):
            try:
                r = json.loads(line)
            except ValueError:
                continue
            if "ns" in r and r.get("cycle", 0) >= 1:
                ns.setdefault(r["metric"], {})[r["cycle"]] = r["ns"]
        out[int(m.group(1))] = ns
    return out


def ratio(a, b, key):
    logs = []
    for bt in sorted(set(a) & set(b)):
        xa, xb = a[bt].get(key, {}), b[bt].get(key, {})
        per = [math.log(xa[c] / xb[c]) for c in sorted(set(xa) & set(xb)) if xa[c] > 0 and xb[c] > 0]
        if per:
            logs.append(statistics.fmean(per))
    return math.exp(statistics.fmean(logs)) if logs else None


def ms(a, key):
    v = [statistics.fmean(a[b][key].values()) for b in a if a[b].get(key)]
    return statistics.median(v) / 1e6 if v else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("raw"); ap.add_argument("--tag", default="x")
    ap.add_argument("--xo", default="xoffset_trusted"); ap.add_argument("--rivals", default="zpp_bits,cista_unchecked")
    ap.add_argument("--json")
    a = ap.parse_args()
    rivals = [r for r in a.rivals.split(",") if r]
    data = {f: load(a.raw, a.tag, f) for f in [a.xo] + rivals}
    fmt = lambda x, p=3: "-" if x is None else f"{x:.{p}f}"
    print(f"# Transport-model columns (SLG_XPORT=1, report only)\n\nbatches: {len(data[a.xo])}\n")
    head = "| cell |" + "".join(f" {f} ms/cycle |" for f in data) + "".join(f" {a.xo} / {r} |" for r in rivals)
    print(head); print("|" + "---|" * (head.count("|") - 1))
    out = {}
    for k in KEYS:
        row = {f: ms(d, k) for f, d in data.items()}
        rat = {r: ratio(data[a.xo], data[r], k) for r in rivals}
        out[k] = {"ms": row, "ratio": rat}
        print(f"| {k} |" + "".join(f" {fmt(v)} |" for v in row.values()) + "".join(f" {fmt(v, 2)} |" for v in rat.values()))
    if a.json:
        json.dump(out, open(a.json, "w"), indent=1)


if __name__ == "__main__":
    main()
