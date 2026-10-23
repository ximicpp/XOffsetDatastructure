#!/usr/bin/env python3
"""Summarise reloc_bench JSONL: per comparison the geometric mean of the
per-round ratios XOffset / baseline and its Bonferroni one-sided 95% upper bound.

usage: analyze.py raw.jsonl [raw2.jsonl ...] [--bound 1.05] [--alpha 0.05] [--json out.json]

Rounds of several files are pooled (each file one run of the same binary).
For a comparison with n rounds, r_i = xoffset_i / baseline_i and
  GM = exp(mean ln r),  UB = exp(mean ln r + t(1 - alpha/k, n - 1) * sd(ln r) / sqrt(n))
where k is the number of G9 comparisons (cases whose JSONL names a primary
baseline, against that baseline). Reported comparisons (std::map / std::set,
reused keys, undeclared ".plain" records) use the same alpha/k so that their
bounds read on the same scale; they are not part of the G9 verdict.
"""
import argparse
import json
import math
import statistics
import sys


def betacf(a, b, x):
    # Continued fraction of the incomplete beta function (modified Lentz).
    tiny = 1e-300
    qab, qap, qam = a + b, a + 1.0, a - 1.0
    c, d = 1.0, 1.0 - qab * x / qap
    d = 1.0 / (d if abs(d) > tiny else tiny)
    h = d
    for m in range(1, 300):
        m2 = 2 * m
        aa = m * (b - m) * x / ((qam + m2) * (a + m2))
        d = 1.0 + aa * d
        d = 1.0 / (d if abs(d) > tiny else tiny)
        c = 1.0 + aa / c
        c = c if abs(c) > tiny else tiny
        h *= d * c
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2))
        d = 1.0 + aa * d
        d = 1.0 / (d if abs(d) > tiny else tiny)
        c = 1.0 + aa / c
        c = c if abs(c) > tiny else tiny
        delta = d * c
        h *= delta
        if abs(delta - 1.0) < 1e-15:
            break
    return h


def betainc(a, b, x):
    if x <= 0.0:
        return 0.0
    if x >= 1.0:
        return 1.0
    lbt = math.lgamma(a + b) - math.lgamma(a) - math.lgamma(b) + a * math.log(x) + b * math.log1p(-x)
    if x < (a + 1.0) / (a + b + 2.0):
        return math.exp(lbt) * betacf(a, b, x) / a
    return 1.0 - math.exp(lbt) * betacf(b, a, 1.0 - x) / b


def t_cdf(t, df):
    x = df / (df + t * t)
    tail = 0.5 * betainc(df / 2.0, 0.5, x)
    return 1.0 - tail if t >= 0 else tail


def t_quantile(p, df):
    lo, hi = 0.0, 1.0
    while t_cdf(hi, df) < p:
        hi *= 2.0
    for _ in range(200):
        mid = (lo + hi) / 2.0
        if t_cdf(mid, df) < p:
            lo = mid
        else:
            hi = mid
    return (lo + hi) / 2.0


def load(paths):
    rounds, skipped, meta = {}, {}, []
    for path in paths:
        with open(path) as f:
            for line in f:
                if not line.strip():
                    continue
                j = json.loads(line)
                if "meta" in j:
                    meta.append(j["meta"])
                elif "skipped" in j:
                    skipped[j["case"]] = j["skipped"]
                else:
                    rounds.setdefault(j["case"], []).append(j)
    return meta, rounds, skipped


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="+")
    ap.add_argument("--bound", type=float, default=1.05, help="G9 limit on the upper bound (default 1.05)")
    ap.add_argument("--alpha", type=float, default=0.05)
    ap.add_argument("--json", default="", help="also write the table as JSON")
    a = ap.parse_args()
    meta, rounds, skipped = load(a.files)
    if not rounds:
        sys.exit("no rounds")

    comps = []  # (case, baseline, is_g9)
    for case, rs in rounds.items():
        primary = rs[0].get("primary")
        for b in rs[0]["ns"]:
            if b != "xoffset":
                comps.append((case, b, b == primary))
    k = sum(1 for c in comps if c[2])
    q = 1.0 - a.alpha / max(k, 1)

    rows = []
    for case, b, g9 in comps:
        rs = rounds[case]
        ops = rs[0]["ops"]
        lr = [math.log(r["ns"]["xoffset"] / r["ns"][b]) for r in rs if r["ns"][b] > 0 and r["ns"]["xoffset"] > 0]
        n = len(lr)
        m = statistics.fmean(lr)
        sd = statistics.stdev(lr) if n > 1 else float("nan")
        ub = math.exp(m + t_quantile(q, n - 1) * sd / math.sqrt(n)) if n > 1 else float("nan")
        rows.append({
            "case": case, "baseline": b, "g9": g9, "n": n,
            "xoffset_ns_op": statistics.median(r["ns"]["xoffset"] for r in rs) / ops,
            "baseline_ns_op": statistics.median(r["ns"][b] for r in rs) / ops,
            "gm": math.exp(m), "ub": ub,
            "median_ratio": statistics.median(math.exp(x) for x in lr),
            "verdict": ("pass" if ub <= a.bound else "fail") if g9 else "report",
        })

    compilers = sorted({m.get("compiler", "?") for m in meta})
    print(f"files: {len(a.files)}  compiler: {', '.join(compilers)}")
    print(f"G9 comparisons k = {k}; one-sided {100 * (1 - a.alpha):.0f}% UB, Bonferroni t quantile {q:.5f}; limit UB <= {a.bound}")
    print()
    print(f"| {'case':28s} | {'baseline':14s} | {'role':6s} | {'n':>4s} | {'xo ns/op':>9s} | {'base ns/op':>10s} | {'GM':>6s} | {'UB':>6s} | verdict |")
    print(f"| {'-' * 28} | {'-' * 14} | {'-' * 6} | {'-' * 3}: | {'-' * 8}: | {'-' * 9}: | {'-' * 5}: | {'-' * 5}: | ------- |")
    for r in rows:
        print(f"| {r['case']:28s} | {r['baseline']:14s} | {'G9' if r['g9'] else 'report':6s} | {r['n']:4d} | "
              f"{r['xoffset_ns_op']:9.2f} | {r['baseline_ns_op']:10.2f} | {r['gm']:6.3f} | {r['ub']:6.3f} | {r['verdict']:7s} |")
    for case, why in sorted(skipped.items()):
        print(f"| {case:28s} | {'':14s} | {'':6s} | {'':>4s} | {'':>9s} | {'':>10s} | {'':>6s} | {'':>6s} | skipped: {why} |")
    g9 = [r for r in rows if r["g9"]]
    fails = [r for r in g9 if r["verdict"] == "fail"]
    print()
    print(f"G9: {len(g9) - len(fails)} of {len(g9)} comparisons have UB <= {a.bound}"
          + (f"; above: {', '.join(r['case'] for r in fails)}" if fails else ""))
    if skipped:
        print(f"skipped cases: {len(skipped)} (not judged)")
    if a.json:
        with open(a.json, "w") as f:
            json.dump({"k": k, "alpha": a.alpha, "bound": a.bound, "compilers": compilers,
                       "rows": rows, "skipped": skipped}, f, indent=1)


if __name__ == "__main__":
    main()
