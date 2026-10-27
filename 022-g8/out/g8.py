#!/usr/bin/env python3
# g8.py RAWDIR POINT [POINT2]: the G8 checkpoint tables from slg_bench raw files named NNN_<point>.jsonl.
# Scenario totals as bench/slg/tools/analyze.py (sum of the scenario's stages per cycle, read_direct left out,
# cycle 0 dropped). Per process: the mean over cycles of log(xoffset / baseline). G8 is judged on the process
# level: the mean over processes, one-sided 95% upper bound with Bonferroni k = 4 scenarios x 5 baselines = 20,
# t with (processes - 1) df. Reported beside it: the bound over all cycles pooled (S1 protocol on more data) and
# the worst single-process bound (S1 protocol as run on one process).
import glob, json, math, os, statistics as st, sys
from collections import defaultdict

def betacf(a, b, x):
    qab, qap, qam, c, d = a + b, a + 1.0, a - 1.0, 1.0, 1.0 - (a + b) * x / (a + 1.0)
    d = 1.0 / (d if abs(d) > 1e-300 else 1e-300); h = d
    for i in range(1, 201):
        m2 = 2 * i; aa = i * (b - i) * x / ((qam + m2) * (a + m2))
        d = 1.0 + aa * d; d = 1.0 / (d if abs(d) > 1e-300 else 1e-300); c = 1.0 + aa / c if abs(1.0 + aa / c) > 1e-300 else 1e-300; h *= d * c
        aa = -(a + i) * (qab + i) * x / ((a + m2) * (qap + m2))
        d = 1.0 + aa * d; d = 1.0 / (d if abs(d) > 1e-300 else 1e-300); c = 1.0 + aa / c if abs(1.0 + aa / c) > 1e-300 else 1e-300
        de = d * c; h *= de
        if abs(de - 1.0) < 3e-14: break
    return h
def betainc(a, b, x):
    if x <= 0: return 0.0
    if x >= 1: return 1.0
    lbt = math.lgamma(a + b) - math.lgamma(a) - math.lgamma(b) + a * math.log(x) + b * math.log(1 - x)
    return math.exp(lbt) * betacf(a, b, x) / a if x < (a + 1) / (a + b + 2) else 1.0 - math.exp(lbt) * betacf(b, a, 1 - x) / b
def t_cdf(t, df):
    p = 0.5 * betainc(df / 2.0, 0.5, df / (df + t * t)); return 1 - p if t > 0 else p
def t_ppf(q, df):
    lo, hi = -50.0, 50.0
    for _ in range(200):
        mid = (lo + hi) / 2
        if t_cdf(mid, df) < q: lo = mid
        else: hi = mid
    return (lo + hi) / 2

R, points = sys.argv[1], sys.argv[2:]
SC = 'ABCD'; BL = ['zpp_bits', 'cista', 'cereal', 'protobuf', 'flatbuffers']; K = len(SC) * len(BL)

def load(f):  # -> ns[format][metric][cycle], tot[format][scenario][cycle]
    ns = defaultdict(lambda: defaultdict(dict))
    for l in open(f):
        r = json.loads(l)
        if 'ns' not in r or r.get('cycle', 0) < 1: continue
        ns[r['format']][r['metric']][r['cycle']] = r['ns']
    tot = defaultdict(lambda: defaultdict(lambda: defaultdict(float)))
    for fm, ms in ns.items():
        for m, v in ms.items():
            if m[0] in SC and m[1] == '.' and not m.endswith('read_direct'):
                for c, x in v.items(): tot[fm][m[0]][c] += x
    return ns, tot

def lr(x, y):
    cs = sorted(set(x) & set(y)); return [math.log(x[c] / y[c]) for c in cs if x[c] > 0 and y[c] > 0]
def ub_mean(v, q):  # one-sided upper bound of the mean of v at level q
    return st.mean(v) + t_ppf(q, len(v) - 1) * st.stdev(v) / math.sqrt(len(v)) if len(v) > 1 else float('nan')
def ci2(v, q):  # two-sided interval at level q
    h = t_ppf(1 - (1 - q) / 2, len(v) - 1) * st.stdev(v) / math.sqrt(len(v)) if len(v) > 1 else float('nan')
    return st.mean(v) - h, st.mean(v) + h
e = math.exp
data = {p: [load(f) for f in sorted(glob.glob(os.path.join(R, f'*_{p}.jsonl')))] for p in points}
q20 = 1 - 0.05 / K
verdict = None
for p in points:
    runs = data[p]; n = len(runs)
    print(f'## {p}: xoffset ÷ 基线，场景合计（{n} 个进程，每个 40 个周期）\n')
    print(f'进程级几何均值 / 单侧 95% 上界（Bonferroni k={K}，df={n - 1}）。G8 看 A、B、C 对 zpp_bits 和 cista 的这个上界是否 ≤1.0。\n')
    print('| 场景 | ' + ' | '.join(BL) + ' |\n| --- |' + ' --- |' * len(BL))
    res = {}
    for s in SC:
        cells = []
        for b in BL:
            per = [lr(tot['xoffset'][s], tot[b][s]) for ns, tot in runs]
            pm = [st.mean(v) for v in per if len(v) > 2]
            allc = [x for v in per for x in v]
            single = [e(ub_mean(v, q20)) for v in per if len(v) > 2]
            res[(s, b)] = (e(st.mean(pm)), e(ub_mean(pm, q20)), e(ub_mean(allc, q20)), max(single), st.median(e(x) for x in allc))
            cells.append(f'{res[(s, b)][0]:.3f} / {res[(s, b)][1]:.3f}')
        print(f'| {s} | ' + ' | '.join(cells) + ' |')
    print(f'\n同一组数据的另两种上界（只报）：全部周期合并后的上界（S1 口径用在更多数据上）；单个进程按 S1 口径算出的上界里最大的一个。每格是逐周期比值的中位数 / 合并上界 / 单进程最大上界。\n')
    print('| 场景 | ' + ' | '.join(BL) + ' |\n| --- |' + ' --- |' * len(BL))
    for s in SC: print(f'| {s} | ' + ' | '.join(f'{res[(s, b)][4]:.3f} / {res[(s, b)][2]:.3f} / {res[(s, b)][3]:.3f}' for b in BL) + ' |')
    if p == points[0]:
        miss = [f'{s}÷{b} {res[(s, b)][1]:.3f}' for s in 'ABC' for b in ('zpp_bits', 'cista') if not res[(s, b)][1] <= 1.0]
        thin = [f'{s}÷{b} {res[(s, b)][1]:.3f}' for s in 'ABC' for b in ('zpp_bits', 'cista') if 0.95 < res[(s, b)][1] <= 1.0]
        verdict = ('met' if not miss else 'not met: ' + ', '.join(miss)) + (f' (margin < 0.05: {", ".join(thin)})' if thin else '')
    # per stage, xoffset / zpp_bits and / cista (report only: unadjusted one-sided 95% bound on the process level)
    for b in BL:
        detail = b in ('zpp_bits', 'cista')
        if not detail: continue
        print(f'\n### {p}: 各阶段 xoffset ÷ {b}（只报）\n')
        print('每个进程取各阶段 40 个周期的中位数（µs）和逐周期 log 比值的均值；表里是进程间的中位数、几何均值和单侧 95% 上界（不做多重校正）。\n')
        print(f'| 阶段 | xoffset µs | {b} µs | 逐周期比值中位数 | 几何均值 | 上界 | Δ µs/周期 |\n| --- | ---: | ---: | ---: | ---: | ---: | ---: |')
        stages = sorted({m for ns, tot in runs for m in ns['xoffset'] if not m.endswith('read_direct')}, key=lambda m: (m[0] not in SC, m))
        for m in stages:
            pm, xs, ys, allc = [], [], [], []
            for ns, tot in runs:
                x, y = ns['xoffset'].get(m, {}), ns[b].get(m, {})
                v = lr(x, y)
                if len(v) < 3: continue
                pm.append(st.mean(v)); allc += v; xs.append(st.median(x.values()) / 1e3); ys.append(st.median(y.values()) / 1e3)
            if len(pm) < 2: continue
            print(f'| {m} | {st.median(xs):.1f} | {st.median(ys):.1f} | {st.median(e(x) for x in allc):.3f} | {e(st.mean(pm)):.3f} | {e(ub_mean(pm, 0.95)):.3f} | {st.median(xs) - st.median(ys):+.1f} |')
    print()
if len(points) == 2:
    a, b = points
    print(f'## {b} ÷ {a}：xoffset 场景合计（进程级，Welch 双侧 95% k=4，只报）\n\n| 场景 | {b}÷{a} | 区间 |\n| --- | --- | --- |')
    for s in SC:
        va = [st.mean(math.log(x) for x in tot['xoffset'][s].values()) for ns, tot in data[a]]
        vb = [st.mean(math.log(x) for x in tot['xoffset'][s].values()) for ns, tot in data[b]]
        d = st.mean(vb) - st.mean(va); sa, sb = st.variance(va) / len(va), st.variance(vb) / len(vb); se = math.sqrt(sa + sb)
        df = (sa + sb) ** 2 / (sa ** 2 / (len(va) - 1) + sb ** 2 / (len(vb) - 1)) if sa + sb > 0 else 99
        h = t_ppf(1 - 0.05 / 4 / 2, df) * se
        print(f'| {s} | {e(d):.4f} | [{e(d - h):.4f}, {e(d + h):.4f}] |')
    print()
print(f'G8: {verdict}')
