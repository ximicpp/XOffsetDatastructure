#!/usr/bin/env python3
"""Summarize multi-process runs of bench/cppcon2024/complexity_bench.cpp."""
import statistics
import sys
from collections import defaultdict

LIBS = [
    "XOffset",
    "zpp::bits",
    "FlatBuffers",
    "cista raw",
    "cista offset",
    "Cap'n Proto",
    "MessagePack",
    "Protocol Buffers",
]
XO_VARIANTS = ["XOffset", "XOffset (compacted)", "XOffset (unreserved)"]


def main() -> int:
    if len(sys.argv) < 2:
        sys.stderr.write("usage: summarize_complexity.py run1.txt [run2.txt ...]\n")
        return 1

    headers = []
    vals = defaultdict(list)
    for idx, path in enumerate(sys.argv[1:]):
        with open(path, encoding="utf-8") as fh:
            for line in fh:
                if line.startswith("#"):
                    if idx == 0:
                        headers.append(line.strip())
                    continue
                if not line.strip():
                    continue
                w = line[:10].strip()
                m = line[10:27].strip()
                rest = line[27:].rsplit(maxsplit=1)
                lib, v = rest[0].strip(), float(rest[1])
                vals[(w, m, lib)].append(v)

    n_runs = len(sys.argv) - 1
    print(f"# Realistic RPG & iso-byte structural complexity summary (median of {n_runs} processes)\n")
    for h in headers:
        print(f"- `{h[2:].strip()}`")

    def med(w: str, m: str, lib: str):
        vs = vals.get((w, m, lib))
        return statistics.median(vs) if vs else None

    def fmt(v) -> str:
        return "-" if v is None else f"{v:,.1f}"

    def ratio(v, base) -> str:
        return "-" if (v is None or base is None or base == 0) else f"{v/base:.1f}x"

    print("\n## 1A. L1d-resident iso-byte (3 KB): Flat-3KB vs Rich-3KB (`trusted_reuse`, ns)\n")
    print("| Library | Flat-3KB | Rich-3KB | Rich/Flat | Flat ratio | Rich ratio | Rich varied | varied ratio | Rich fresh | fresh ratio |")
    print("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    xf3, xr3 = med("Flat-3KB", "trusted_reuse", "XOffset"), med("Rich-3KB", "trusted_reuse", "XOffset")
    xv3, xfr3 = med("Rich-3KB", "varied_reuse", "XOffset"), med("Rich-3KB", "fresh_trusted", "XOffset")
    for lib in LIBS:
        f3, r3 = med("Flat-3KB", "trusted_reuse", lib), med("Rich-3KB", "trusted_reuse", lib)
        v3, fr3 = med("Rich-3KB", "varied_reuse", lib), med("Rich-3KB", "fresh_trusted", lib)
        print(f"| {lib} | {f3:,.1f} | {r3:,.1f} | {r3/f3:.2f}x | {f3/xf3:.1f}x | {r3/xr3:.1f}x | {v3:,.1f} | {v3/xv3:.1f}x | {fr3:,.1f} | {fr3/xfr3:.1f}x |")

    print("\n## 1B. Commercial character (12 KB): Flat-12KB vs Rich-12KB (`trusted_reuse`, ns)\n")
    print("| Library | Flat-12KB | Rich-12KB | Rich/Flat | Flat ratio | Rich ratio | Rich varied | varied ratio | Rich fresh | fresh ratio |")
    print("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    xf12, xr12 = med("Flat-12KB", "trusted_reuse", "XOffset"), med("Rich-12KB", "trusted_reuse", "XOffset")
    xv12, xfr12 = med("Rich-12KB", "varied_reuse", "XOffset"), med("Rich-12KB", "fresh_trusted", "XOffset")
    for lib in LIBS:
        f12, r12 = med("Flat-12KB", "trusted_reuse", lib), med("Rich-12KB", "trusted_reuse", lib)
        v12, fr12 = med("Rich-12KB", "varied_reuse", lib), med("Rich-12KB", "fresh_trusted", lib)
        print(f"| {lib} | {f12:,.1f} | {r12:,.1f} | {r12/f12:.2f}x | {f12/xf12:.1f}x | {r12/xr12:.1f}x | {v12:,.1f} | {v12/xv12:.1f}x | {fr12:,.1f} | {fr12/xfr12:.1f}x |")

    print("\n## 2. Server-to-server handoff (`Receive -> Mutate -> Send`, 16 varied players) & In-place `rw_rich` (ns)\n")
    print("| Library | Rich-3KB hop (trusted) | ratio | Rich-3KB hop (checked) | Rich-12KB hop (trusted) | ratio | Rich-12KB hop (checked) | ratio | Rich-12KB rw_rich |")
    print("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    xht3 = med("Rich-3KB", "hop_trusted", "XOffset")
    xht12, xhc12 = med("Rich-12KB", "hop_trusted", "XOffset"), med("Rich-12KB", "hop_checked", "XOffset")
    for lib in LIBS:
        ht3, hc3 = med("Rich-3KB", "hop_trusted", lib), med("Rich-3KB", "hop_checked", lib)
        ht12, hc12 = med("Rich-12KB", "hop_trusted", lib), med("Rich-12KB", "hop_checked", lib)
        rw12 = med("Rich-12KB", "rw_rich", lib)
        print(
            f"| {lib} | {fmt(ht3)} | {ratio(ht3, xht3)} | {fmt(hc3)} | {fmt(ht12)} | {ratio(ht12, xht12)} | "
            f"{fmt(hc12)} | {ratio(hc12, xhc12)} | {fmt(rw12)} |"
        )

    print("\n## 3. Equal-check tier: `checked_reuse`, `varied_checked`, and `fresh_checked` (ns)\n")
    print("| Library | Rich-3KB checked | Rich-3KB varied | Rich-3KB fresh | Rich-12KB checked | Rich-12KB varied | ratio | Rich-12KB fresh | ratio |")
    print("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    xvc12, xfc12 = med("Rich-12KB", "varied_checked", "XOffset"), med("Rich-12KB", "fresh_checked", "XOffset")
    for lib in LIBS:
        vc12, fc12 = med("Rich-12KB", "varied_checked", lib), med("Rich-12KB", "fresh_checked", lib)
        print(
            f"| {lib} | {fmt(med('Rich-3KB','checked_reuse',lib))} | {fmt(med('Rich-3KB','varied_checked',lib))} | "
            f"{fmt(med('Rich-3KB','fresh_checked',lib))} | {fmt(med('Rich-12KB','checked_reuse',lib))} | "
            f"{fmt(vc12)} | {ratio(vc12, xvc12)} | {fmt(fc12)} | {ratio(fc12, xfc12)} |"
        )

    print("\n## 4. XOffset arena layout variants (reserved vs compacted vs unreserved)\n")
    print("| Workload | Variant | wire_raw / zstd3 | trusted_reuse | varied_reuse | fresh_trusted | fresh_checked | hop_trusted | hop_checked | varied_checked |")
    print("| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    for w in ("Rich-3KB", "Rich-12KB"):
        for v in XO_VARIANTS:
            wr, wz = med(w, "wire_raw", v), med(w, "wire_zstd3", v)
            print(
                f"| {w} | {v} | {wr:,.0f} / {wz:,.0f} | {fmt(med(w,'trusted_reuse',v))} | {fmt(med(w,'varied_reuse',v))} | "
                f"{fmt(med(w,'fresh_trusted',v))} | {fmt(med(w,'fresh_checked',v))} | "
                f"{fmt(med(w,'hop_trusted',v))} | {fmt(med(w,'hop_checked',v))} | {fmt(med(w,'varied_checked',v))} |"
            )

    print("\n## 5. Wire sizes (raw / zstd-3 bytes) and send/recv split (ns)\n")
    print("| Library | Flat-3KB raw/z3 | Rich-3KB raw/z3 | Flat-12KB raw/z3 | Rich-12KB raw/z3 | Rich-3KB send/recv_t/recv_c | Rich-12KB send/recv_t/recv_c |")
    print("| --- | ---: | ---: | ---: | ---: | ---: | ---: |")
    for lib in LIBS:
        def wz(w):
            return f"{med(w,'wire_raw',lib):,.0f}/{med(w,'wire_zstd3',lib):,.0f}"

        def sp(w):
            return f"{fmt(med(w,'send_only',lib))}/{fmt(med(w,'recv_trusted',lib))}/{fmt(med(w,'recv_checked',lib))}"

        print(f"| {lib} | {wz('Flat-3KB')} | {wz('Rich-3KB')} | {wz('Flat-12KB')} | {wz('Rich-12KB')} | {sp('Rich-3KB')} | {sp('Rich-12KB')} |")

    print("\n## 6. Read-only zero-copy view (send + view without unpack, ns)\n")
    print("| Workload | Tier | XOffset | FlatBuffers | cista raw | cista offset | Cap'n Proto |")
    print("| --- | --- | ---: | ---: | ---: | ---: | ---: |")
    capnp = "Cap'n Proto"
    for w in ("Rich-3KB", "Rich-12KB"):
        for m in ("view_admit", "view_bounds"):
            print(
                f"| {w} | `{m}` | {fmt(med(w,m,'XOffset'))} | {fmt(med(w,m,'FlatBuffers'))} | "
                f"{fmt(med(w,m,'cista raw'))} | {fmt(med(w,m,'cista offset'))} | {fmt(med(w,m,capnp))} |"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
