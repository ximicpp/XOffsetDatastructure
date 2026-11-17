#!/usr/bin/env python3
"""Analyse slg_bench --mode bench output.

Per scenario (A battle round trip, B migration, C shard snapshot, D report reads)
and per stage: median time per format, and for a chosen subject format the paired
per-cycle ratio subject/baseline (geometric mean, and a one-sided upper bound that is
Bonferroni-corrected over all scenario x baseline comparisons). Cycle 0 is warm-up.

Usage: analyze.py raw.jsonl [--subject xoffset] [--baselines a,b,...] [--alpha 0.05] [--md out.md]

--baselines picks the comparison family (and so the Bonferroni k); the median table
still lists every format in the file. Default: every other format in the file.
"""
import argparse, json, math, statistics, sys
from collections import defaultdict


def betacf(a, b, x):
    m, eps, fpmin = 200, 3e-14, 1e-300
    qab, qap, qam = a + b, a + 1.0, a - 1.0
    c, d = 1.0, 1.0 - qab * x / qap
    d = 1.0 / (d if abs(d) > fpmin else fpmin)
    h = d
    for i in range(1, m + 1):
        m2 = 2 * i
        aa = i * (b - i) * x / ((qam + m2) * (a + m2))
        d = 1.0 + aa * d; d = 1.0 / (d if abs(d) > fpmin else fpmin)
        c = 1.0 + aa / c if abs(1.0 + aa / c) > fpmin else fpmin
        h *= d * c
        aa = -(a + i) * (qab + i) * x / ((a + m2) * (qap + m2))
        d = 1.0 + aa * d; d = 1.0 / (d if abs(d) > fpmin else fpmin)
        c = 1.0 + aa / c if abs(1.0 + aa / c) > fpmin else fpmin
        de = d * c
        h *= de
        if abs(de - 1.0) < eps:
            break
    return h


def betainc(a, b, x):
    if x <= 0: return 0.0
    if x >= 1: return 1.0
    lbt = math.lgamma(a + b) - math.lgamma(a) - math.lgamma(b) + a * math.log(x) + b * math.log(1 - x)
    if x < (a + 1) / (a + b + 2):
        return math.exp(lbt) * betacf(a, b, x) / a
    return 1.0 - math.exp(lbt) * betacf(b, a, 1 - x) / b


def t_cdf(t, df):
    x = df / (df + t * t)
    p = 0.5 * betainc(df / 2.0, 0.5, x)
    return 1 - p if t > 0 else p


def t_ppf(q, df):
    lo, hi = -50.0, 50.0
    for _ in range(200):
        mid = (lo + hi) / 2
        if t_cdf(mid, df) < q: lo = mid
        else: hi = mid
    return (lo + hi) / 2


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("raw")
    ap.add_argument("--subject", default="xoffset")
    ap.add_argument("--alpha", type=float, default=0.05)
    ap.add_argument("--baselines")
    ap.add_argument("--md")
    a = ap.parse_args()

    ns = defaultdict(lambda: defaultdict(dict))      # ns[format][metric][cycle]
    by = defaultdict(lambda: defaultdict(dict))
    meta = {}
    with open(a.raw) as f:
        for line in f:
            r = json.loads(line)
            if "meta" in r: meta = r["meta"]; continue
            if r["cycle"] == 0: continue
            if "ns" in r: ns[r["format"]][r["metric"]][r["cycle"]] = r["ns"]
            else: by[r["format"]][r["metric"]][r["cycle"]] = r["bytes"]

    formats = sorted(ns)
    scen = {"A": "battle server round trip", "B": "cross-server migration", "C": "zone snapshot", "D": "battle report read only"}
    if meta.get("cases") == "demo":  # SLG_CASES=demo (include/slg/view.hpp)
        scen = {"A": "scene<->battle", "B": "scene<->scene", "S": "scene<->storage", "SD": "scene<->storage (dirty modules only)",
                "L": "login full state", "LD": "own delta after login"}
        scen["EB"] = "map browse, full viewport"
        scen["EBW"] = "map browse, single-device warm view receive"
        if any(m.startswith("UL.") for f in formats for m in ns[f]):   # uplink.hpp (SLG_UL_OFF=1 drops it)
            scen["UL"] = "client uplink (march orders, server receive: bounds tier / each rival's own checks)"
        for r, name in (("P", "peace"), ("C", "combat"), ("S", "stress")):
            scen["U" + r] = f"static viewport delta ({name})"
        # XOffset's own delta sync (xoffset_delta.hpp): N* deltas per static
        # client, compared with each baseline's U*.
        if any(m.startswith("N") and m[2:3] == "." for f in formats for m in ns[f]):
            for r, name in (("P", "peace"), ("C", "combat"), ("S", "stress")):
                scen["N" + r] = f"static viewport delta, XOffset native ÷ rival U{r} ({name})"
            scen["LK"] = "login full state, XOffset native key ÷ rival L"
            scen["LN"] = "own delta, XOffset native ÷ rival LD"
            scen["SN"] = "save, XOffset native delta ÷ rival SD"
    # Scenario totals per cycle.
    for fmt in formats:
        for s in scen:
            cycles = set()
            for m, v in ns[fmt].items():
                if m.startswith(s + ".") and not m.endswith("read_direct"): cycles |= set(v)
            for c in cycles:
                ns[fmt][s + ".total"][c] = sum(v.get(c, 0) for m, v in ns[fmt].items()
                                              if m.startswith(s + ".") and m != s + ".total" and not m.endswith("read_direct"))
    out = []
    p = out.append
    p(f"# SLG prototype results\n\nScale {meta.get('scale')}, seed {meta.get('seed')}, {meta.get('cycles')} cycles (cycle 0 discarded). Unit ms, the total of each cycle's whole batch of messages.\n")
    p("## Median per stage (ms)\n")
    metrics = sorted({m for f in formats for m in ns[f]})
    p("| Stage | " + " | ".join(formats) + " |")
    p("| --- |" + " ---: |" * len(formats))
    for m in metrics:
        cells = []
        for f in formats:
            v = ns[f].get(m)
            cells.append(f"{statistics.median(v.values()) / 1e6:.3f}" if v else "")
        label = f"**{m}**" if m.endswith(".total") else m
        p(f"| {label} | " + " | ".join(cells) + " |")

    if a.subject in formats:
        base = [f for f in formats if f != a.subject]
        if a.baselines:
            base = [f for f in a.baselines.split(",") if f in formats and f != a.subject]
        k = len(scen) * len(base)
        p(f"\n## {a.subject} ÷ baselines (scenario totals, paired per cycle)\n")
        p(f"Geometric mean / one-sided {100 * (1 - a.alpha):.0f}% upper bound, Bonferroni correction k={k}. < 1 means {a.subject} is faster.\n")
        p("| Scenario | " + " | ".join(base) + " |")
        p("| --- |" + " --- |" * len(base))
        for s, label in scen.items():
            cells = []
            for b in base:
                x = ns[a.subject].get(s + ".total", {})
                alt = {"LK": "L", "LN": "LD", "SN": "SD"}.get(s) or ({"N": "U"}.get(s[0]) and "U" + s[1:] if len(s) == 2 else None)
                y = ns[b].get((alt if alt and s + ".total" not in ns[b] else s) + ".total", {})
                cs = sorted(set(x) & set(y))
                lr = [math.log(x[c] / y[c]) for c in cs if x[c] > 0 and y[c] > 0]
                if len(lr) < 3: cells.append(""); continue
                mean, sd = statistics.mean(lr), statistics.stdev(lr)
                t = t_ppf(1 - a.alpha / k, len(lr) - 1)
                cells.append(f"{math.exp(mean):.3f} / {math.exp(mean + t * sd / math.sqrt(len(lr))):.3f}")
            p(f"| {s} {label} | " + " | ".join(cells) + " |")
        for b in base:
            if "D.read_direct" not in ns[b]: continue
            x = ns[a.subject].get("D.read", {}); y = ns[b]["D.read_direct"]
            cs = sorted(set(x) & set(y))
            if cs:
                r = math.exp(statistics.mean(math.log(x[c] / y[c]) for c in cs))
                p(f"\nD control: {a.subject} read ÷ {b} in-place read (read directly after validation, no copy, no unpack) = {r:.3f} (geometric mean, reference only).")

    p("\n## Wire bytes (median of per-cycle totals)\n")
    bm = sorted({m for f in formats for m in by[f]})
    p("| Message | " + " | ".join(formats) + " |")
    p("| --- |" + " ---: |" * len(formats))
    for m in bm:
        p(f"| {m} | " + " | ".join(f"{statistics.median(by[f][m].values()):.0f}" if m in by[f] else "" for f in formats) + " |")
    text = "\n".join(out) + "\n"
    if a.md:
        open(a.md, "w").write(text)
    sys.stdout.write(text)


if __name__ == "__main__":
    main()
