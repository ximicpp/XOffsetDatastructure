#!/usr/bin/env python3
"""W3 step 0: the transport-model columns (.send+T / .recv+T, report only) and the W3 gate.

usage: xport_gate.py RAW_DIR [--tag x] [--old xoffset_trusted] [--new xoffset_wire]
                     [--rivals zpp_bits,cista_unchecked] [--json OUT]

RAW_DIR holds <tag>_<format>_<batch>.jsonl from SLG_XPORT=1 processes (one per
format and batch, same seed). Per cell and batch the ratio is the mean log ratio
over cycles >= 1 of the per-cycle ns; the point estimate is the geometric mean
over batches. The gate (W3 design): for the whole-block group L, B, S, SD, the
group ratio of a batch is the geometric mean of its four cells' ratios; a side
(send or recv, judged separately) passes when the median over batches improves
by >= 20% and every batch by > 10%.
"""
import argparse, glob, json, math, os, re, statistics

CELLS = ["L", "B", "S", "SD"]


def load(d, tag, fmt):
    out = {}
    for p in glob.glob(os.path.join(d, f"{tag}_{fmt}_*.jsonl")):
        b = int(re.search(r"_(\d+)\.jsonl$", p).group(1))
        ns = {}
        for line in open(p):
            try:
                r = json.loads(line)
            except ValueError:
                continue
            if "ns" in r and r.get("cycle", 0) >= 1:
                ns.setdefault(r["metric"], {})[r["cycle"]] = r["ns"]
        out[b] = ns
    return out


def batch_ratios(a, b, key):
    res = {}
    for bt in sorted(set(a) & set(b)):
        xa, xb = a[bt].get(key, {}), b[bt].get(key, {})
        logs = [math.log(xa[c] / xb[c]) for c in sorted(set(xa) & set(xb)) if xa[c] > 0 and xb[c] > 0]
        if logs:
            res[bt] = math.exp(statistics.fmean(logs))
    return res


def gm(v):
    return math.exp(statistics.fmean(math.log(x) for x in v)) if v else float("nan")


def per_msg_us(a, key, count_key=None):
    v = [sum(a[b].get(key, {}).values()) / max(1, len(a[b].get(key, {}))) for b in a]
    return statistics.median(v) / 1000 if v else float("nan")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("raw"); ap.add_argument("--tag", default="x")
    ap.add_argument("--old", default="xoffset_trusted"); ap.add_argument("--new", default="xoffset_wire")
    ap.add_argument("--rivals", default="zpp_bits,cista_unchecked"); ap.add_argument("--json")
    a = ap.parse_args()
    old, new = load(a.raw, a.tag, a.old), load(a.raw, a.tag, a.new)
    rivals = {r: load(a.raw, a.tag, r) for r in a.rivals.split(",") if r}
    out = {"cells": {}, "gate": {}}
    print(f"# W3 step 0: {a.new} vs {a.old}, transport model (SLG_XPORT=1)\n")
    print(f"batches: {len(set(old) & set(new))}\n")
    hdr = "| cell | old ms/cycle | new ms/cycle | new/old (GM) | gain | min..max batch gain |" + "".join(f" {a.old}/{r} | {a.new}/{r} |" for r in rivals)
    print(hdr); print("|" + "---|" * (hdr.count("|") - 1))
    for side in ["send", "recv"]:
        group = {}
        for c in CELLS:
            k = f"{c}.{side}+T"
            br = batch_ratios(new, old, k)
            for bt, r in br.items():
                group.setdefault(bt, []).append(r)
            g = gm(list(br.values()))
            gains = [1 - r for r in br.values()]
            row = f"| {k} | {per_msg_us(old, k)/1000:.2f} | {per_msg_us(new, k)/1000:.2f} | {g:.3f} | {100*(1-g):.1f}% | {100*min(gains):.1f}..{100*max(gains):.1f}% |"
            cell = {"ratio": g, "batch": br}
            for r, rv in rivals.items():
                ro, rn = gm(list(batch_ratios(old, rv, k).values())), gm(list(batch_ratios(new, rv, k).values()))
                row += f" {ro:.2f} | {rn:.2f} |"
                cell[f"old_vs_{r}"] = ro; cell[f"new_vs_{r}"] = rn
            print(row)
            out["cells"][k] = cell
        gr = {bt: gm(v) for bt, v in group.items() if len(v) == len(CELLS)}
        gains = sorted(1 - r for r in gr.values())
        med = statistics.median(gains) if gains else float("nan")
        ok = bool(gains) and med >= 0.20 and min(gains) > 0.10
        out["gate"][side] = {"median_gain": med, "min_gain": min(gains) if gains else None, "batch_gain": {b: 1 - r for b, r in gr.items()}, "pass": ok}
    print()
    for side, g in out["gate"].items():
        print(f"- gate {side}: group L/B/S/SD median gain {100*g['median_gain']:.1f}%, worst batch {100*g['min_gain']:.1f}% -> {'PASS' if g['pass'] else 'FAIL'} (needs median >= 20% and every batch > 10%)")
    if a.json:
        json.dump(out, open(a.json, "w"), indent=1)


if __name__ == "__main__":
    main()
