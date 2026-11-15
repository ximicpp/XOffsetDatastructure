#!/usr/bin/env python3
"""Median of cppcon2024_bench runs, as Markdown tables next to the CppCon 2024 numbers."""
import statistics
import sys

# CppCon 2024 README (tag v1.0.0), 100k iterations in ms -> ns per iteration (x10).
# Its encode/decode loop timed an empty move for XOffset (sender buffer moved away on the
# first iteration), and its read/write loop timed 85 mt19937_64 draws per iteration too.
Y2024 = {
    # lib: (encode/decode ns, rw ns, raw bytes, zlib bytes)
    "xoffset 4.0": (0.53, 498.465, 4184, 3075),
    "zpp::bits": (4531.36, 753.239, 2916, 2769),
    "MessagePack": (9577.0, 680.446, 3222, 3064),
    "Protocol Buffers": (15780.6, 565.378, 4100, 3759),
    "FlatBuffers": (6794.42, 680.446, 3672, 3247),
    "Cap'n Proto": (2592.25, 680.446, 3656, 3205),
    "cista raw": (5315.57, 655.362, 5392, 3294),
    "cista offset": (5845.49, 692.177, 5392, 3336),
}
ORDER = list(Y2024)
STD_RW = {"zpp::bits", "MessagePack", "FlatBuffers", "Cap'n Proto"}


def load(paths):
    vals = {}
    for p in paths:
        for line in open(p, encoding="utf-8"):
            if line.startswith("#") or not line.strip():
                continue
            lib, metric, value = line.rsplit(maxsplit=2)
            vals.setdefault((lib, metric), []).append(float(value))
    return {k: statistics.median(v) for k, v in vals.items()}, len(paths)


def main():
    m, n = load(sys.argv[1:])
    g = lambda lib, metric: m.get((lib, metric))
    xr, xf, xrw = g("xoffset 4.0", "sr_reuse"), g("xoffset 4.0", "sr_fresh"), g("xoffset 4.0", "rw")
    out = [f"Median of {n} process runs (each metric: median of its rounds), ns per message / iteration.", ""]
    out += ["### Send + receive (ns per message)", "",
            "| Library | 2024 encode+decode | 4.0 reuse | 4.0 fresh | XOffset ÷ lib (reuse) | XOffset ÷ lib (fresh) |",
            "| --- | ---: | ---: | ---: | ---: | ---: |"]
    for lib in ORDER:
        r, f = g(lib, "sr_reuse"), g(lib, "sr_fresh")
        out.append(f"| {lib} | {Y2024[lib][0]:,.1f} | {r:,.1f} | {f:,.1f} | {xr / r:.3f} | {xf / f:.3f} |")
    out += ["", "Report-only rows:", "",
            "| Row | reuse | fresh |", "| --- | ---: | ---: |"]
    for lib in ("xoffset 4.0 full", "FlatBuffers view"):
        f = g(lib, "sr_fresh")
        out.append(f"| {lib} | {g(lib, 'sr_reuse'):,.1f} | {'' if f is None else f'{f:,.1f}'} |")
    out += ["", "### Read/write loop (ns per iteration)", "",
            "| Library | Containers | 2024 | 4.0 | XOffset ÷ lib |", "| --- | --- | ---: | ---: | ---: |"]
    for lib in ORDER:
        if lib in STD_RW:
            v, cont = g("std (shared)", "rw"), "std::* (shared, items = sorted vector)"
        elif lib == "xoffset 4.0":
            v, cont = xrw, "XOffset (edit() per iteration)"
        elif lib == "Protocol Buffers":
            v, cont = g(lib, "rw"), "google::protobuf message"
        else:
            v, cont = g(lib, "rw"), lib.replace("cista ", "cista::") + "::* (items = hash_map)"
        out.append(f"| {lib} | {cont} | {Y2024[lib][1]:,.1f} | {v:,.1f} | {xrw / v:.3f} |")
    out.append(f"| xoffset 4.0 plain_root (report-only) | XOffset, plain reference | | {g('xoffset 4.0 plain_root', 'rw'):,.1f} | |")
    out += ["", "### Wire size (bytes)", "",
            "| Library | 2024 raw | 2024 zlib | 4.0 raw | 4.0 zstd-3 |", "| --- | ---: | ---: | ---: | ---: |"]
    for lib in ORDER:
        out.append(f"| {lib} | {Y2024[lib][2]:,} | {Y2024[lib][3]:,} | {g(lib, 'bytes'):,.0f} | {g(lib, 'bytes_zstd3'):,.0f} |")
    out.append(f"\nXOffset after the read/write rounds (report-only): save_verified {g('xoffset 4.0', 'bytes_after_rw'):,.0f} B, "
               f"save_compact {g('xoffset 4.0', 'bytes_after_rw_compact'):,.0f} B.")
    inline = "xoffset inline"
    if g(inline, "rw") is not None:
        out.extend(["", "### XOffset Character with inline equipment attributes", "",
                    "| Metric | Original | Inline | Inline ÷ fastest rival |",
                    "| --- | ---: | ---: | ---: |"])
        for metric, rivals in (
            ("sr_reuse", ORDER[1:]),
            ("sr_fresh", ORDER[1:]),
            ("rw", ("std (shared)", "Protocol Buffers", "cista raw", "cista offset")),
        ):
            original = g("xoffset 4.0", metric)
            value = g(inline, metric)
            best = min(g(lib, metric) for lib in rivals)
            out.append(f"| {metric} (ns) | {original:.1f} | {value:.1f} | {value / best:.3f} |")
        for metric in ("bytes", "bytes_zstd3", "bytes_after_rw_zstd3"):
            original = g("xoffset 4.0", metric)
            value = g(inline, metric)
            out.append(f"| {metric} (B) | {original:.0f} | {value:.0f} | |")
    print("\n".join(out))


if __name__ == "__main__":
    main()
