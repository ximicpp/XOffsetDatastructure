#!/usr/bin/env python3
"""SLG cells with two-sided 95% CIs, for any XOffset format against any rival set.

usage: slg_analyze.py RAW_DIR --xo xoffset_trusted --rivals a,b,c [--shared s1,s2] [--title T] [--json OUT]
Same pairing as tools/perf_gate/scorecard.py: per (batch, cycle >= 1) log ratio of the cell's
per-cycle key sums, mean within a batch; across batches: geometric mean with a Student t
95% interval in log space (n = batches), and the batch min-max.
"""
import argparse
import json
import math
import os
import statistics
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "tools", "perf_gate"))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scorecard as sc  # noqa: E402
from stats import t975, verdict  # noqa: E402


def ratio(xo, xkeys, rival, rkeys):
    logs = []
    for b in sorted(set(xo) & set(rival)):
        xn, rn = xo[b][0], rival[b][0]
        cyc = set(xn.get(xkeys[0], {})) & set(rn.get(rkeys[0], {}))
        pairs = []
        for c in sorted(cyc):
            a, r = sc.cycle_sum(xn, xkeys, c), sc.cycle_sum(rn, rkeys, c)
            if a and r:
                pairs.append(math.log(a / r))
        if pairs:
            logs.append(statistics.fmean(pairs))
    if not logs:
        return None
    m = statistics.fmean(logs)
    h = t975(len(logs) - 1) * statistics.stdev(logs) / math.sqrt(len(logs)) if len(logs) > 1 else float("inf")
    gm, lo, hi = math.exp(m), math.exp(m - h), math.exp(m + h)
    v, firm = verdict(gm, lo, hi)
    return {"gm": gm, "lo": lo, "hi": hi, "min": math.exp(min(logs)), "max": math.exp(max(logs)), "n": len(logs), "verdict": v, "firm": firm}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("raw")
    ap.add_argument("--xo", default="xoffset_trusted")
    ap.add_argument("--rivals", required=True)
    ap.add_argument("--shared", default="")
    ap.add_argument("--main-tag", default="head")
    ap.add_argument("--d3-tag", default="d3")
    ap.add_argument("--title", default="")
    ap.add_argument("--json")
    a = ap.parse_args()
    rivals = [r for r in a.rivals.split(",") if r]
    shared = [r for r in a.shared.split(",") if r]
    xo = {"main": sc.load(a.raw, a.main_tag, a.xo), "d3": sc.load(a.raw, a.d3_tag, a.xo)}
    rv = {r: sc.load(a.raw, a.main_tag, r) for r in rivals}
    sv = {r: sc.load(a.raw, a.d3_tag, r) for r in shared}
    rows = []
    for group, name, label, xkind, xkeys, rkeys, kind in sc.CELLS:
        if not xo[xkind]:
            continue
        per = {}
        for r in rivals:
            if rv[r]:
                res = ratio(xo[xkind], xkeys, rv[r], rkeys)
                if res:
                    per[r] = res
        if xkind == "d3":
            for r, run in sv.items():
                if run:
                    res = ratio(xo["d3"], xkeys, run, xkeys)
                    if res:
                        per[r + " shared"] = res
        if not per:
            continue
        best = max(per, key=lambda r: per[r]["gm"])
        rows.append({"group": group, "cell": name, "label": label, "kind": kind, "rivals": per, "best": best, "strongest": per[best]})
    cols = rivals + [r + " shared" for r in shared]
    print(f"### {a.title or a.xo}\n")
    main_rows = [r for r in rows if r["kind"] == "main"]
    cnt = {v: sum(1 for r in main_rows if r["strongest"]["verdict"] == v) for v in ("win", "tie", "loss")}
    firm = sum(1 for r in main_rows if r["strongest"]["firm"])
    print(f"Main cells {len(main_rows)}: win {cnt['win']}, tie {cnt['tie']}, loss {cnt['loss']} (verdict on the geometric mean; "
          f"{firm} of {len(main_rows)} firm, i.e. the whole 95% CI inside the verdict band).\n")
    print("| Cell | Scenario | ÷ strongest [95% CI] | strongest | " + " | ".join(f"÷ {c}" for c in cols) + " |")
    print("| --- | --- | --- | --- | " + " | ".join("---" for _ in cols) + " |")
    for r in rows:
        s = r["strongest"]
        tag = s["verdict"] + ("" if s["firm"] else "?")
        if r["kind"] == "report":
            tag = "report (" + tag + ")"
        cells = []
        for c in cols:
            x = r["rivals"].get(c)
            cells.append(f"{x['gm']:.3f} [{x['lo']:.2f}–{x['hi']:.2f}]" if x else "")
        print(f"| {r['cell']} | {r['label']} | {s['gm']:.3f} [{s['lo']:.3f}–{s['hi']:.3f}] {tag} | {r['best']} | " + " | ".join(cells) + " |")
    print()
    if a.json:
        json.dump(rows, open(a.json, "w"), indent=1)


if __name__ == "__main__":
    main()
