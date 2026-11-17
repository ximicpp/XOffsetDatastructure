#!/usr/bin/env python3
"""Analysis of the premises sweep (premises_bench JSONL, one file per process).

usage: sweep_analyze.py RAW_DIR OUT_JSON > tables.md
Ratios are XOffset ÷ rival, paired within a process (same process, cells shuffled), then
median over processes with an exact order-statistic 95% CI. Verdict per project rule on the
median (win < 0.95, tie 0.95-1.05, loss > 1.05); "firm" when the whole CI is in that band.
"""
import glob
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from stats import median, median_ci, bootstrap, verdict  # noqa: E402

SIZES = ["0", "1/64", "1/16", "1/8", "1/4", "1/2", "1", "2", "4", "16", "64", "256"]
TRUSTED = ["zpp_bits", "cista_raw_unchecked", "cista_offset_cast", "flatbuffers_noverify", "protobuf", "cereal"]
CHECKED = ["zpp_bits", "cista_raw_checked", "cista_raw_deep", "flatbuffers_verify", "protobuf", "cereal"]
SHORT = {"zpp_bits": "zpp", "cista_raw_unchecked": "cista raw U", "cista_offset_cast": "cista off CAST",
         "cista_raw_checked": "cista raw chk", "cista_raw_deep": "cista deep", "flatbuffers_noverify": "FB",
         "flatbuffers_verify": "FB+Verifier", "protobuf": "pb", "cereal": "cereal",
         "flatbuffers_view_noverify": "FB view", "flatbuffers_view_verify": "FB view+Verifier"}
DIRTS = ["clean", "rw1", "rw100000", "g0.25", "g0.5", "g1", "g2", "g4", "g8"]


def load(raw):
    ns, info = {}, {}
    for path in sorted(glob.glob(os.path.join(raw, "p*.jsonl"))):
        for line in open(path):
            r = json.loads(line)
            key = (r["group"], r["fmt"], r["size"], r["dirt"])
            ns.setdefault(key, {})[r["proc"]] = r["ns"]
            info[key] = {k: v for k, v in r.items() if k.startswith("bytes")}
    return ns, info


def fmt_ns(v):
    if v >= 1e6:
        return f"{v / 1e6:.2f} ms"
    if v >= 1e4:
        return f"{v / 1e3:.1f} µs"
    if v >= 1e3:
        return f"{v / 1e3:.2f} µs"
    return f"{v:.0f} ns" if v >= 100 else f"{v:.1f} ns"


def ratio_series(ns, xo_key, rival_keys):
    """Per-process ratio XO ÷ min over rival_keys (the strongest rival in that process)."""
    xo = ns.get(xo_key, {})
    out = []
    for p, x in xo.items():
        rv = [ns[k][p] for k in rival_keys if k in ns and p in ns[k]]
        if len(rv) == len(rival_keys):
            out.append(x / min(rv))
    return out


def summarize(ratios):
    m = median(ratios)
    lo, hi, cov = median_ci(ratios)
    v, sure = verdict(m, lo, hi)
    wins = sum(1 for r in ratios if r < 1) / len(ratios)
    return {"median": m, "lo": lo, "hi": hi, "cov": cov, "verdict": v, "firm": sure, "n": len(ratios), "faster_share": wins}


def cell(s):
    tag = s["verdict"] + ("" if s["firm"] else "?")
    return f"{s['median']:.3f} [{s['lo']:.3f}–{s['hi']:.3f}] {tag}"


def interp_cross(xs, ys, level):
    """First x (log-interpolated) where ys crosses `level` going from above to below or
    below to above along xs; returns the first crossing from the small end."""
    for i in range(len(xs) - 1):
        y0, y1 = ys[i], ys[i + 1]
        if (y0 - level) * (y1 - level) <= 0 and y0 != y1:
            t = (level - y0) / (y1 - y0)
            lx = math.log(xs[i]) + t * (math.log(xs[i + 1]) - math.log(xs[i]))
            return math.exp(lx)
    return None


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    dirt_only = "--dirt-only" in sys.argv  # a run with --filter: only the dirt and churn sections
    raw, out_json = args[0], args[1]
    ns, info = load(raw)
    procs = sorted({p for d in ns.values() for p in d})
    res = {"procs": len(procs), "size": {}, "dirt": {}, "churn": {}, "cross": {}}
    print(f"# Premises sweep: {len(procs)} processes\n")
    sizes = [s for s in SIZES if ("size", "zpp_bits", s, "clean") in info]
    zpp_bytes = {s: info[("size", "zpp_bits", s, "clean")]["bytes"] for s in sizes}
    if not dirt_only:
        size_sections(ns, info, res, zpp_bytes)
    dirt_sections(ns, info, res, zpp_bytes, procs)
    json.dump(res, open(out_json, "w"), indent=1, default=str)


def size_sections(ns, info, res, zpp_bytes):
    # ---------------- size sweep
    xo_bytes = {s: info[("size", "xo_admit", s, "clean")]["bytes"] for s in SIZES}
    print("## 1. Size sweep, clean sender: XOffset ÷ rival (median [95% CI] verdict)\n")
    for tier, xo_fmt, rivals in (("trusted", "xo_admit", TRUSTED), ("checked", "xo_full", CHECKED)):
        print(f"### Tier {tier}: {xo_fmt}\n")
        hdr = "| k | zpp B | XO B | XO zstd | XO ns | " + " | ".join(SHORT[r] for r in rivals) + " | ÷ strongest |"
        print(hdr)
        print("|" + " --- |" * (hdr.count("|") - 1))
        res["size"][tier] = {}
        for s in SIZES:
            xk = ("size", xo_fmt, s, "clean")
            row = {"xo_ns": median(list(ns[xk].values())), "zpp_bytes": zpp_bytes[s], "xo_bytes": xo_bytes[s]}
            cells = []
            for r in rivals:
                rs = summarize(ratio_series(ns, xk, [("size", r, s, "clean")]))
                rs["rival_ns"] = median(list(ns[("size", r, s, "clean")].values()))
                row[r] = rs
                cells.append(cell(rs))
            st = summarize(ratio_series(ns, xk, [("size", r, s, "clean") for r in rivals]))
            row["strongest"] = st
            # which rival is strongest most often
            row["strongest_name"] = min(rivals, key=lambda r: row[r]["rival_ns"])
            res["size"][tier][s] = row
            xz = info[xk]["bytes_z"]
            print(f"| {s} | {zpp_bytes[s]:.0f} | {xo_bytes[s]:.0f} | {xz:.0f} | {fmt_ns(row['xo_ns'])} | " + " | ".join(cells) +
                  f" | {cell(st)} ({SHORT[row['strongest_name']]}) |")
        print()
    # rival absolute ns
    print("### Absolute ns (median over processes)\n")
    allf = ["xo_admit", "xo_full", "xo_view_admit", "xo_view_bounds", "xo_compact_admit"] + sorted(set(TRUSTED + CHECKED)) + ["flatbuffers_view_noverify", "flatbuffers_view_verify"]
    print("| k | " + " | ".join(SHORT.get(f, f) for f in allf) + " |")
    print("|" + " --- |" * (len(allf) + 1))
    for s in SIZES:
        print(f"| {s} | " + " | ".join(fmt_ns(median(list(ns[("size", f, s, "clean")].values()))) for f in allf) + " |")
    print()
    # cost of full validation relative to admit, and zpp
    print("### Cost of XOffset's full check: xo_full ÷ xo_admit, and xo_full ÷ zpp_bits\n")
    print("| k | full ÷ admit | full ÷ zpp |")
    print("| --- | --- | --- |")
    for s in SIZES:
        a = summarize(ratio_series(ns, ("size", "xo_full", s, "clean"), [("size", "xo_admit", s, "clean")]))
        z = summarize(ratio_series(ns, ("size", "xo_full", s, "clean"), [("size", "zpp_bits", s, "clean")]))
        print(f"| {s} | {a['median']:.2f} [{a['lo']:.2f}–{a['hi']:.2f}] | {cell(z)} |")
    print()
    # read-only views (report only)
    print("### Read-only receive (report only): XOffset view ÷ FlatBuffers view\n")
    print("| k | view admit ÷ FB view | view bounds ÷ FB view+Verifier | view bounds ÷ zpp |")
    print("| --- | --- | --- | --- |")
    res["views"] = {}
    for s in SIZES:
        a = summarize(ratio_series(ns, ("size", "xo_view_admit", s, "clean"), [("size", "flatbuffers_view_noverify", s, "clean")]))
        b = summarize(ratio_series(ns, ("size", "xo_view_bounds", s, "clean"), [("size", "flatbuffers_view_verify", s, "clean")]))
        c = summarize(ratio_series(ns, ("size", "xo_view_bounds", s, "clean"), [("size", "zpp_bits", s, "clean")]))
        res["views"][s] = {"admit_vs_fb": a, "bounds_vs_fbv": b, "bounds_vs_zpp": c}
        print(f"| {s} | {cell(a)} | {cell(b)} | {cell(c)} |")
    print()

    # ---------------- size crossovers (bootstrap over processes)
    print("## 2. Size crossovers (message bytes = zpp_bits encoding of the state)\n")
    print("Where the median ratio crosses 0.95 (win/tie line) and 1.00, interpolated in log size; 95% bootstrap CI over processes.\n")
    print("| Tier | Rival | ratio at k=0 | crosses 0.95 at (bytes) | crosses 1.00 at (bytes) |")
    print("| --- | --- | --- | --- | --- |")
    xs = [max(zpp_bytes[s], 1) for s in SIZES]
    for tier, xo_fmt, rivals in (("trusted", "xo_admit", TRUSTED + ["strongest"]), ("checked", "xo_full", CHECKED + ["strongest"])):
        base = TRUSTED if tier == "trusted" else CHECKED
        for r in rivals:
            keys = base if r == "strongest" else [r]
            per = {s: dict(zip(*_paired(ns, ("size", xo_fmt, s, "clean"), [("size", k, s, "clean") for k in keys]))) for s in SIZES}
            common = sorted(set.intersection(*[set(per[s]) for s in SIZES]))
            def curve(sample):
                return [median([per[s][p] for p in sample]) for s in SIZES]
            def cross(level):
                def f(sample):
                    c = interp_cross(xs, curve(sample), level)
                    return c if c is not None else float("nan")
                return f
            c95 = interp_cross(xs, curve(common), 0.95)
            c100 = interp_cross(xs, curve(common), 1.0)
            b95 = bootstrap(cross(0.95), common) if c95 else None
            b100 = bootstrap(cross(1.0), common) if c100 else None
            r0 = curve(common)[0]
            name = "strongest" if r == "strongest" else SHORT[r]
            s95 = f"{c95:.0f} [{b95[0]:.0f}–{b95[1]:.0f}] ({b95[2]:.0%} of resamples cross)" if c95 else "no crossing (always below 0.95)" if r0 < 0.95 else "no crossing"
            s100 = f"{c100:.0f} [{b100[0]:.0f}–{b100[1]:.0f}] ({b100[2]:.0%})" if c100 else ("no crossing (always below 1)" if r0 < 1 else "no crossing (above 1 at every size)")
            res["cross"][f"size/{tier}/{r}"] = {"r0": r0, "c95": c95, "b95": b95, "c100": c100, "b100": b100}
            print(f"| {tier} | {name} | {r0:.3f} | {s95} | {s100} |")
    print()



def dirt_sections(ns, info, res, zpp_bytes, procs):
    # ---------------- dirt sweep
    print("## 3. Dirtiness: the sender's buffer after churn (logical state unchanged)\n")
    for s in [s for s in SIZES if ("dirt", "xo_admit", s, "rw1") in ns]:
        c0 = info[("size", "xo_admit", s, "clean")]["bytes_compact0"]
        print(f"### k = {s} (compact wire {c0:.0f} B, zpp {zpp_bytes[s]:.0f} B)\n")
        print("| dirt | wire B | r = wire ÷ compact | zstd B | zero_runs B | admit ÷ zpp | admit ÷ strongest trusted | full ÷ zpp | full ÷ strongest checked | zero_runs admit ÷ zpp | compact+admit ÷ zpp | admit ns |")
        print("| --- |" + " --- |" * 11)
        res["dirt"][s] = {}
        for d in DIRTS:
            g = "size" if d == "clean" else "dirt"
            k = lambda f: (g, f, s, d)
            if k("xo_admit") not in ns:
                continue
            inf = info[k("xo_admit")]
            zk = [("size", "zpp_bits", s, "clean")]
            a_z = summarize(ratio_series(ns, k("xo_admit"), zk))
            a_s = summarize(ratio_series(ns, k("xo_admit"), [("size", r, s, "clean") for r in TRUSTED]))
            f_z = summarize(ratio_series(ns, k("xo_full"), zk))
            f_s = summarize(ratio_series(ns, k("xo_full"), [("size", r, s, "clean") for r in CHECKED]))
            zr = summarize(ratio_series(ns, (g, "xo_admit_zero_runs", s, d), zk)) if (g, "xo_admit_zero_runs", s, d) in ns else None
            cp = summarize(ratio_series(ns, k("xo_compact_admit"), zk))
            row = {"wire": inf["bytes"], "r": inf["bytes"] / c0, "zstd": inf["bytes_z"], "zero_runs_bytes": inf.get("bytes_zero_runs"),
                   "admit_zpp": a_z, "admit_strongest": a_s, "full_zpp": f_z, "full_strongest": f_s, "zero_runs_zpp": zr, "compact_zpp": cp,
                   "admit_ns": median(list(ns[k("xo_admit")].values()))}
            res["dirt"][s][d] = row
            print(f"| {d} | {inf['bytes']:.0f} | {row['r']:.2f} | {inf['bytes_z']:.0f} | {inf.get('bytes_zero_runs', 0):.0f} | {cell(a_z)} | {cell(a_s)} | {cell(f_z)} | {cell(f_s)} | "
                  f"{cell(zr) if zr else '-'} | {cell(cp)} | {fmt_ns(row['admit_ns'])} |")
        print()
        # linear fit XO ns = a + b * wire over dirt points, per process; crossover wire bytes
        for tier, xo_fmt, rivals in (("trusted", "xo_admit", TRUSTED), ("checked", "xo_full", CHECKED)):
            for target_name, target_keys in (("zpp", ["zpp_bits"]), ("strongest", rivals)):
                cross_r = []
                fits = []
                for p in procs:
                    pts = []
                    for d in DIRTS:
                        g = "size" if d == "clean" else "dirt"
                        key = (g, xo_fmt, s, d)
                        if key in ns and p in ns[key]:
                            pts.append((info[key]["bytes"], ns[key][p]))
                    if len(pts) < 4:
                        continue
                    n = len(pts)
                    mx = sum(x for x, _ in pts) / n
                    my = sum(y for _, y in pts) / n
                    sxx = sum((x - mx) ** 2 for x, _ in pts)
                    sxy = sum((x - mx) * (y - my) for x, y in pts)
                    b = sxy / sxx
                    a = my - b * mx
                    ss_tot = sum((y - my) ** 2 for _, y in pts)
                    ss_res = sum((y - a - b * x) ** 2 for x, y in pts)
                    fits.append((a, b, 1 - ss_res / ss_tot if ss_tot else 1))
                    if not all(p in ns[("size", r, s, "clean")] for r in target_keys):
                        continue
                    rv = min(ns[("size", r, s, "clean")][p] for r in target_keys)
                    w95 = (0.95 * rv - a) / b
                    w100 = (1.0 * rv - a) / b
                    cross_r.append((w95 / c0, w100 / c0))
                if not cross_r:
                    continue
                r95 = [x for x, _ in cross_r]
                r100 = [y for _, y in cross_r]
                m95, m100 = median(r95), median(r100)
                ci95, ci100 = median_ci(r95), median_ci(r100)
                fa = median([f[0] for f in fits]); fb = median([f[1] for f in fits]); fr2 = median([f[2] for f in fits])
                res["cross"][f"dirt/{s}/{tier}/{target_name}"] = {"r95": m95, "r95_ci": ci95[:2], "r100": m100, "r100_ci": ci100[:2], "fit": [fa, fb, fr2]}
                print(f"- {tier}, vs {target_name}: XO ns ≈ {fa:.0f} + {fb * 1000:.1f} × KB of wire (median R² {fr2:.3f}); tie line 0.95 at r = {m95:.2f} [{ci95[0]:.2f}–{ci95[1]:.2f}], parity 1.00 at r = {m100:.2f} [{ci100[0]:.2f}–{ci100[1]:.2f}]")
        print()

    # ---------------- churn
    print("## 4. Random churn (100,000 edits; every rival measured on the same final state)\n")
    print("| k | wire B | compact B | r | zstd B | compact zstd B | zpp B | admit ÷ zpp | admit ÷ strongest trusted | full ÷ zpp | full ÷ strongest checked | zero_runs admit ÷ zpp | compact+admit ÷ zpp |")
    print("| --- |" + " --- |" * 12)
    for s in [s for s in SIZES if ("churn", "xo_admit", s, "churn100000") in ns]:
        k = lambda f: ("churn", f, s, "churn100000")
        inf = info[k("xo_admit")]
        zk = [k("zpp_bits")]
        a_z = summarize(ratio_series(ns, k("xo_admit"), zk))
        a_s = summarize(ratio_series(ns, k("xo_admit"), [k(r) for r in TRUSTED]))
        f_z = summarize(ratio_series(ns, k("xo_full"), zk))
        f_s = summarize(ratio_series(ns, k("xo_full"), [k(r) for r in CHECKED]))
        zr = summarize(ratio_series(ns, k("xo_admit_zero_runs"), zk))
        cp = summarize(ratio_series(ns, k("xo_compact_admit"), zk))
        res["churn"][s] = {"wire": inf["bytes"], "compact0": inf["bytes_compact0"], "r": inf["bytes"] / inf["bytes_compact0"], "zstd": inf["bytes_z"],
                           "zpp_bytes": info[k("zpp_bits")]["bytes"], "admit_zpp": a_z, "admit_strongest": a_s, "full_zpp": f_z, "full_strongest": f_s,
                           "zero_runs_zpp": zr, "compact_zpp": cp,
                           "strongest_trusted": min(TRUSTED, key=lambda r: median(list(ns[k(r)].values()))),
                           "strongest_checked": min(CHECKED, key=lambda r: median(list(ns[k(r)].values())))}
        print(f"| {s} | {inf['bytes']:.0f} | {inf['bytes_compact0']:.0f} | {inf['bytes'] / inf['bytes_compact0']:.2f} | {inf['bytes_z']:.0f} | {inf['bytes_compact_z']:.0f} | "
              f"{info[k('zpp_bits')]['bytes']:.0f} | {cell(a_z)} | {cell(a_s)} ({SHORT[res['churn'][s]['strongest_trusted']]}) | {cell(f_z)} | {cell(f_s)} ({SHORT[res['churn'][s]['strongest_checked']]}) | {cell(zr)} | {cell(cp)} |")
    print()


def _paired(ns, xo_key, rival_keys):
    xo = ns.get(xo_key, {})
    ps, rs = [], []
    for p, x in xo.items():
        rv = [ns[k][p] for k in rival_keys if k in ns and p in ns[k]]
        if len(rv) == len(rival_keys):
            ps.append(p)
            rs.append(x / min(rv))
    return ps, rs


if __name__ == "__main__":
    main()
