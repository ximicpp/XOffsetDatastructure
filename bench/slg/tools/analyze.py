#!/usr/bin/env python3
"""Analyse slg_bench --mode bench output.

Per scenario (A battle round trip, B migration, C shard snapshot, D report reads)
and per stage: median time per format, and for a chosen subject format the paired
per-cycle ratio subject/baseline (geometric mean, and a one-sided upper bound that is
Bonferroni-corrected over all scenario x baseline comparisons). Cycle 0 is warm-up.

Usage: analyze.py raw.jsonl [--subject xoffset] [--alpha 0.05] [--md out.md]
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
    scen = {"A": "战斗服往返", "B": "跨服迁移", "C": "分区快照", "D": "战报只读"}
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
    p(f"# SLG 原型结果\n\n规模 {meta.get('scale')}，seed {meta.get('seed')}，{meta.get('cycles')} 个周期（丢弃第 0 个）。单位 ms，为每个周期整批消息的合计。\n")
    p("## 各阶段中位数（ms）\n")
    metrics = sorted({m for f in formats for m in ns[f]})
    p("| 阶段 | " + " | ".join(formats) + " |")
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
        k = len(scen) * len(base)
        p(f"\n## {a.subject} ÷ 基线（场景合计，逐周期配对）\n")
        p(f"几何均值 / 单侧 {100 * (1 - a.alpha):.0f}% 上界，Bonferroni 校正 k={k}。<1 表示 {a.subject} 更快。\n")
        p("| 场景 | " + " | ".join(base) + " |")
        p("| --- |" + " --- |" * len(base))
        for s, label in scen.items():
            cells = []
            for b in base:
                x = ns[a.subject].get(s + ".total", {}); y = ns[b].get(s + ".total", {})
                cs = sorted(set(x) & set(y))
                lr = [math.log(x[c] / y[c]) for c in cs if x[c] > 0 and y[c] > 0]
                if len(lr) < 3: cells.append(""); continue
                mean, sd = statistics.mean(lr), statistics.stdev(lr)
                t = t_ppf(1 - a.alpha / k, len(lr) - 1)
                cells.append(f"{math.exp(mean):.3f} / {math.exp(mean + t * sd / math.sqrt(len(lr))):.3f}")
            p(f"| {s} {label} | " + " | ".join(cells) + " |")
        if "flatbuffers" in formats and "D.read_direct" in ns["flatbuffers"]:
            x = ns[a.subject].get("D.read", {}); y = ns["flatbuffers"]["D.read_direct"]
            cs = sorted(set(x) & set(y))
            if cs:
                r = math.exp(statistics.mean(math.log(x[c] / y[c]) for c in cs))
                p(f"\nD 对照：{a.subject} 读取 ÷ FlatBuffers 原地读取（不解包）= {r:.3f}（几何均值，仅参考）。")

    p("\n## wire 字节（每周期合计的中位数）\n")
    bm = sorted({m for f in formats for m in by[f]})
    p("| 消息 | " + " | ".join(formats) + " |")
    p("| --- |" + " ---: |" * len(formats))
    for m in bm:
        p(f"| {m} | " + " | ".join(f"{statistics.median(by[f][m].values()):.0f}" if m in by[f] else "" for f in formats) + " |")
    text = "\n".join(out) + "\n"
    if a.md:
        open(a.md, "w").write(text)
    sys.stdout.write(text)


if __name__ == "__main__":
    main()
