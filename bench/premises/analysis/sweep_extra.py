#!/usr/bin/env python3
"""Extra tables for the premises sweep: bytes, XOffset cost vs wire bytes, dirt crossovers
by interpolation between observed points.  usage: sweep_extra.py RAW_DIR > extra.md"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from stats import median, median_ci, bootstrap  # noqa: E402
from sweep_analyze import load, SIZES, TRUSTED, CHECKED, SHORT, DIRTS, RELAYOUT_DIRTS, CHURNS, fmt_ns  # noqa: E402


def main():
    ns, info = load(sys.argv[1])
    procs = sorted({p for d in ns.values() for p in d})

    print("## Bytes (report only): zstd-3, XOffset ÷ rival\n")
    rivals = ["zpp_bits", "cista_raw_unchecked", "flatbuffers_noverify", "protobuf", "cereal"]
    print("| k | XO raw | XO zstd | " + " | ".join(f"{SHORT[r]} zstd" for r in rivals) + " | " + " | ".join(f"÷ {SHORT[r]}" for r in rivals) + " | XO raw ÷ zpp raw |")
    print("|" + " --- |" * (3 + 2 * len(rivals) + 1))
    for s in [s for s in SIZES if ("size", "xo_admit", s, "clean") in info]:
        xi = info[("size", "xo_admit", s, "clean")]
        rz = [info[("size", r, s, "clean")]["bytes_z"] for r in rivals]
        zr = info[("size", "zpp_bits", s, "clean")]["bytes"]
        print(f"| {s} | {xi['bytes']:.0f} | {xi['bytes_z']:.0f} | " + " | ".join(f"{v:.0f}" for v in rz) + " | " +
              " | ".join(f"{xi['bytes_z'] / v:.2f}" for v in rz) + f" | {xi['bytes'] / zr:.2f} |")
    print()
    print("Dirty and churned senders (zstd-3):\n")
    print("| k | dirt | XO wire | XO zstd | zero_runs | compact (slack 1/8) zstd | zpp zstd | XO zstd ÷ zpp zstd | compact zstd ÷ zpp zstd |")
    print("| --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    for s in [s for s in SIZES if ("dirt", "xo_admit", s, "rw1") in ns]:
        for d in DIRTS + RELAYOUT_DIRTS + CHURNS:
            g = "size" if d == "clean" else ("churn" if d.startswith("churn") else "dirt")
            k = (g, "xo_admit", s, d)
            if k not in info:
                continue
            xi = info[k]
            zk = ("churn", "zpp_bits", s, "churn100000") if g == "churn" else ("size", "zpp_bits", s, "clean")
            zz = info[zk]["bytes_z"]
            print(f"| {s} | {d} | {xi['bytes']:.0f} | {xi['bytes_z']:.0f} | {xi['bytes_zero_runs']:.0f} | {xi['bytes_compact_z']:.0f} | {zz:.0f} | {xi['bytes_z'] / zz:.2f} | {xi['bytes_compact_z'] / zz:.2f} |")
    print()

    # XOffset cost as a function of wire bytes (all clean and dirty points)
    print("## XOffset admit send+receive cost vs wire bytes (every clean and dirty sender)\n")
    print("| source | wire B | 3 × wire (working set) | admit ns | B per ns | full ns |")
    print("| --- | ---: | ---: | ---: | ---: | ---: |")
    pts = []
    for (g, f, s, d), v in ns.items():
        if f != "xo_admit" or g == "churn":
            continue
        w = info[(g, f, s, d)]["bytes"]
        full = ns.get((g, "xo_full", s, d))
        pts.append((w, f"{g} k={s} {d}", median(list(v.values())), median(list(full.values())) if full else float("nan")))
    for w, lab, a, fu in sorted(pts):
        print(f"| {lab} | {w:.0f} | {3 * w / 1024:.0f} KiB | {fmt_ns(a)} | {w / a:.1f} | {fmt_ns(fu)} |")
    print()

    # dirt crossover by interpolation between observed points (median curve), bootstrap
    print("## Dirt crossovers by interpolation between measured points\n")
    print("Points sorted by r (wire ÷ compact); the crossing of the median ratio curve with 0.95 / 1.00, linear in r between the bracketing points; 95% bootstrap CI over processes.\n")
    print("| k | tier | rival | last point below 0.95 | first point above 1.05 | r at 0.95 [CI] | r at 1.00 [CI] |")
    print("| --- | --- | --- | --- | --- | --- | --- |")
    for s in [s for s in SIZES if ("dirt", "xo_admit", s, "rw1") in ns]:
        c0 = info[("size", "xo_admit", s, "clean")]["bytes_compact0"]
        for tier, xf, rset in (("trusted", "xo_admit", TRUSTED), ("checked", "xo_full", CHECKED)):
            for rname, rkeys in (("zpp", ["zpp_bits"]), ("strongest", rset)):
                order = []
                for d in DIRTS:
                    g = "size" if d == "clean" else "dirt"
                    order.append((info[(g, xf, s, d)]["bytes"] / c0, (g, xf, s, d), d))
                order.sort()
                # monotone envelope: keep points in r order (same r: keep both)
                per = {}
                for r, key, d in order:
                    per[key] = {p: ns[key][p] / min(ns[("size", rk, s, "clean")][p] for rk in rkeys)
                                for p in procs if p in ns[key] and all(p in ns[("size", rk, s, "clean")] for rk in rkeys)}
                common = sorted(set.intersection(*[set(v) for v in per.values()]))
                rs = [r for r, _, _ in order]

                def curve(sample):
                    return [median([per[key][p] for p in sample]) for _, key, _ in order]

                def cross(level, sample):
                    c = curve(sample)
                    for i in range(len(c) - 1):
                        if c[i] < level <= c[i + 1]:
                            if rs[i + 1] == rs[i]:
                                return rs[i]
                            return rs[i] + (level - c[i]) * (rs[i + 1] - rs[i]) / (c[i + 1] - c[i])
                    return float("nan") if c[0] < level else rs[0]

                c = curve(common)
                below = [(r, d, v) for (r, _, d), v in zip(order, c) if v < 0.95]
                above = [(r, d, v) for (r, _, d), v in zip(order, c) if v > 1.05]
                lb = max(below) if below else None
                ua = min(above) if above else None
                out = []
                for level in (0.95, 1.0):
                    m = cross(level, common)
                    lo, hi, frac = bootstrap(lambda smp: cross(level, smp), common)
                    out.append(f"{m:.2f} [{lo:.2f}–{hi:.2f}]" if not math.isnan(m) else "no crossing up to r = %.1f" % rs[-1])
                lbs = f"r={lb[0]:.2f} ({lb[1]}, {lb[2]:.2f})" if lb else "-"
                uas = f"r={ua[0]:.2f} ({ua[1]}, {ua[2]:.2f})" if ua else "-"
                print(f"| {s} | {tier} | {rname} | {lbs} | {uas} | {out[0]} | {out[1]} |")
    print()


if __name__ == "__main__":
    main()
