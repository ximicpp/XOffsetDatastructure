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
    # The tuned layout and loop, the same for every model that can express it.
    inline = "xoffset inline"
    if g(inline, "rw") is not None and g("std inline", "rw") is not None:
        ir, ifr, irw = g(inline, "sr_reuse"), g(inline, "sr_fresh"), g(inline, "rw")
        out += ["", "### Tuned layout and loop: read/write (ns per iteration)", "",
                "| Library | Tuning applied | 2024 layout | Tuned | XOffset inline ÷ lib |",
                "| --- | --- | ---: | ---: | ---: |"]
        tuned_rw = (
            (inline, "xoffset 4.0", "inline attributes, range append, erase at position; edit() per iteration"),
            ("std inline", "std (shared)", "inline attributes, range insert, erase at position"),
            ("Protocol Buffers bulk", "Protocol Buffers", "attributes added as one range (no inline array in a message)"),
            ("cista raw inline", "cista raw", "inline attributes, range insert, erase at position"),
            ("cista offset inline", "cista offset", "inline attributes, range insert, erase at position"),
        )
        for lib, base, what in tuned_rw:
            out.append(f"| {lib} | {what} | {g(base, 'rw'):,.1f} | {g(lib, 'rw'):,.1f} | {irw / g(lib, 'rw'):.3f} |")
        out += ["", "Report-only rows:", "", "| Row | ns |", "| --- | ---: |",
                f"| xoffset inline, one edit() around all iterations of a round | {g(inline, 'rw_one_scope'):,.1f} |",
                f"| xoffset bulk (2024 layout, bulk calls, edit() per iteration) | {g('xoffset bulk', 'rw'):,.1f} |"]
        out += ["", "### Tuned layout: send + receive (ns per message)", "",
                "| Library | reuse | fresh | XOffset inline ÷ lib (reuse) | XOffset inline ÷ lib (fresh) | raw bytes | zstd-3 |",
                "| --- | ---: | ---: | ---: | ---: | ---: | ---: |"]
        for lib in (inline, "zpp::bits inline", "cista raw inline", "cista offset inline"):
            r, f = g(lib, "sr_reuse"), g(lib, "sr_fresh")
            out.append(f"| {lib} | {r:,.1f} | {f:,.1f} | {ir / r:.3f} | {ifr / f:.3f} | {g(lib, 'bytes'):,.0f} | {g(lib, 'bytes_zstd3'):,.0f} |")
        out.append(f"\nXOffset inline after the read/write rounds (report-only): save_verified {g(inline, 'bytes_after_rw'):,.0f} B "
                   f"(zstd-3 {g(inline, 'bytes_after_rw_zstd3'):,.0f}), save_compact {g(inline, 'bytes_after_rw_compact'):,.0f} B "
                   f"(zstd-3 {g(inline, 'bytes_after_rw_compact_zstd3'):,.0f}).")
        # One update and one message: what a server tick pays per changed object.
        out += ["", "### One read/write iteration + one send + receive (reuse), ns (sum of the medians above)", "",
                "| Library | 2024 layout | Tuned | XOffset ÷ lib (2024 layout) | XOffset inline ÷ lib (tuned) |",
                "| --- | ---: | ---: | ---: | ---: |"]
        x0, x1 = xrw + xr, irw + ir
        total = (
            ("xoffset", x0, x1),
            ("zpp::bits (std model)", g("std (shared)", "rw") + g("zpp::bits", "sr_reuse"), g("std inline", "rw") + g("zpp::bits inline", "sr_reuse")),
            ("Protocol Buffers", g("Protocol Buffers", "rw") + g("Protocol Buffers", "sr_reuse"), g("Protocol Buffers bulk", "rw") + g("Protocol Buffers", "sr_reuse")),
            ("cista raw", g("cista raw", "rw") + g("cista raw", "sr_reuse"), g("cista raw inline", "rw") + g("cista raw inline", "sr_reuse")),
            ("cista offset", g("cista offset", "rw") + g("cista offset", "sr_reuse"), g("cista offset inline", "rw") + g("cista offset inline", "sr_reuse")),
        )
        for lib, t0, t1 in total:
            out.append(f"| {lib} | {t0:,.1f} | {t1:,.1f} | {x0 / t0:.3f} | {x1 / t1:.3f} |")
    print("\n".join(out))


if __name__ == "__main__":
    main()
