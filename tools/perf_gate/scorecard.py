#!/usr/bin/env python3
"""SLG scorecard from bench/slg JSONL: 20 main cells, XOffset ÷ the strongest rival.

usage: scorecard.py RAW_DIR [--bytes-dir DIR] [--xo xoffset_trusted]
                    [--rivals zpp_bits,cista_unchecked] [--shared-rivals zpp_bits,zpp_bits_dirty]
                    [--main-tag head] [--d3-tag d3]
                    [--env env.json] [--baseline scorecard.json] [--json OUT]

RAW_DIR holds one JSONL per process, <tag>_<format>_<batch>.jsonl
(tools/perf_gate/repro_scorecard.sh writes them so):
  <main-tag>  every format, SLG_NATIVE_OFF=1 (application-level rows)
  <d3-tag>    SLG_D3=1 with native on (the shared-delta D* rows): XOffset, and the
              rivals that have a shared encoding (--shared-rivals: zpp_bits, and the
              diagnostic format zpp_bits_dirty, whose U rows are zpp_bits')
Every process of one batch runs the same seed, so cycle c of batch b carries the
same messages in every format. A cell's ratio is taken per (batch, cycle >= 1)
on the per-cycle sum of the cell's keys, averaged in log space within a batch;
the point estimate is the geometric mean over batches. The D cells pair
XOffset's D* rows (d3 processes) with every approach a rival has (susu, 4.0
plan: scored against the rival's strongest approach): each rival's per-client
U* rows (main processes), and each shared-encoding rival's own D* rows (its d3
processes, the same keys as XOffset's; column "<format> shared"). A run without
a rival's d3 processes (an older tree) scores the D cells against U* only.

Per cell the scorecard names the strongest rival (the one XOffset is slowest
against) and classes the ratio: win < 0.95, tie 0.95-1.05, loss > 1.05.
UL.send (client uplink send) is report-only (susu, 4.0 plan). Bytes are
report-only (wire format rule); they are zstd-3 sizes when --bytes-dir points
at a run of the SLG_WITH_COMPRESSION=1 build, raw wire sizes otherwise.
"""
import argparse
import glob
import json
import math
import os
import re
import statistics
import sys

WIN, LOSS = 0.95, 1.05
A_KEYS = ["A.req_encode", "A.req_decode", "A.apply", "A.res_encode", "A.res_decode", "A.cleanup"]
B_KEYS = ["B.out_encode", "B.in_decode", "B.install"]
S_KEYS = ["S.save_encode", "S.store_recv", "S.load_recv", "S.load_install"]
SD_KEYS = ["SD.save_encode", "SD.store_recv"]

# (group, name, label, xo tag kind, xo keys, rival keys, kind). kind: main | report.
CELLS = [
    ("server to server", "A", "battle request and result", "main", A_KEYS, A_KEYS, "main"),
    ("server to server", "B", "server migration", "main", B_KEYS, B_KEYS, "main"),
    ("server to server", "S", "save and load", "main", S_KEYS, S_KEYS, "main"),
    ("server to server", "SD", "delta save", "main", SD_KEYS, SD_KEYS, "main"),
    ("server send", "L.send", "login full state", "main", ["L.send"], ["L.send"], "main"),
    ("server send", "EB.send", "browse full state", "main", ["EB.send"], ["EB.send"], "main"),
    ("server send", "LD.send", "own delta", "main", ["LD.send"], ["LD.send"], "main"),
    ("server send", "UP.send", "view delta, peace", "main", ["UP.send"], ["UP.send"], "main"),
    ("server send", "UC.send", "view delta, conflict", "main", ["UC.send"], ["UC.send"], "main"),
    ("server send", "DP", "shared delta, peace: publish + send ÷ rival UP.send or its shared encoding DP", "d3", ["DP.pub", "DP.send"], ["UP.send"], "main"),
    ("server send", "DC", "shared delta, conflict: publish + send ÷ rival UC.send or its shared encoding DC", "d3", ["DC.pub", "DC.send"], ["UC.send"], "main"),
    ("client receive", "L.recv", "login full state", "main", ["L.recv"], ["L.recv"], "main"),
    ("client receive", "EBW.recv", "browse full state (warm view)", "main", ["EBW.recv"], ["EBW.recv"], "main"),
    ("client receive", "LDW.recv", "own delta (warm view)", "main", ["LDW.recv"], ["LDW.recv"], "main"),
    ("client receive", "UPW.recv", "view delta, peace (warm view)", "main", ["UPW.recv"], ["UPW.recv"], "main"),
    ("client receive", "UCW.recv", "view delta, conflict (warm view)", "main", ["UCW.recv"], ["UCW.recv"], "main"),
    ("client receive", "DPTW.recv", "shared delta, peace ÷ rival UPW.recv or its shared encoding DPTW", "d3", ["DPTW.recv"], ["UPW.recv"], "main"),
    ("client receive", "DCTW.recv", "shared delta, conflict ÷ rival UCW.recv or its shared encoding DCTW", "d3", ["DCTW.recv"], ["UCW.recv"], "main"),
    ("uplink", "UL.recv", "server receives orders (bounds check)", "main", ["UL.recv"], ["UL.recv"], "main"),
    ("uplink", "UL.send", "client sends orders (report only)", "main", ["UL.send"], ["UL.send"], "report"),
    # Report-only rows outside the 20 cells.
    ("report only", "US.send", "view delta, siege", "main", ["US.send"], ["US.send"], "report"),
    ("report only", "DS", "shared delta, siege: publish + send ÷ rival US.send or its shared encoding DS", "d3", ["DS.pub", "DS.send"], ["US.send"], "report"),
    ("report only", "USW.recv", "view delta, siege (warm view)", "main", ["USW.recv"], ["USW.recv"], "report"),
    ("report only", "DSTW.recv", "shared delta, siege ÷ rival USW.recv or its shared encoding DSTW", "d3", ["DSTW.recv"], ["USW.recv"], "report"),
    ("report only", "EB.recv", "browse full state (cold)", "main", ["EB.recv"], ["EB.recv"], "report"),
    ("report only", "LD recv+apply", "own delta (cold)", "main", ["LD.recv", "LD.apply"], ["LD.recv", "LD.apply"], "report"),
    ("report only", "UP recv+apply", "view delta, peace (cold)", "main", ["UP.recv", "UP.apply"], ["UP.recv", "UP.apply"], "report"),
    ("report only", "UC recv+apply", "view delta, conflict (cold)", "main", ["UC.recv", "UC.apply"], ["UC.recv", "UC.apply"], "report"),
]
BYTE_ROWS = [  # (label, xo tag kind, xo key, rival key)
    ("A request", "main", "A.req", "A.req"), ("A result", "main", "A.res", "A.res"),
    ("B migration", "main", "B.out", "B.out"), ("S save", "main", "S.player", "S.player"),
    ("SD delta save", "main", "SD.modules", "SD.modules"),
    ("L login", "main", "L.player{z}", "L.player{z}"), ("EB browse", "main", "EB.window{z}", "EB.window{z}"),
    ("LD own delta", "main", "LD.patch{z}", "LD.patch{z}"),
    ("UP view peace", "main", "UP.delta{z}", "UP.delta{z}"), ("UC view conflict", "main", "UC.delta{z}", "UC.delta{z}"),
    ("DP shared peace ÷ rival UP", "d3", "DP.delta{z}", "UP.delta{z}"),
    ("DC shared conflict ÷ rival UC", "d3", "DC.delta{z}", "UC.delta{z}"),
    ("UL uplink orders", "main", "UL.order{z}", "UL.order{z}"),
]
# One-sided 95% Student t quantiles by degrees of freedom (>= 30: normal).
T95 = {1: 6.314, 2: 2.920, 3: 2.353, 4: 2.132, 5: 2.015, 6: 1.943, 7: 1.895, 8: 1.860,
       9: 1.833, 10: 1.812, 11: 1.796, 12: 1.782, 13: 1.771, 14: 1.761, 15: 1.753,
       16: 1.746, 17: 1.740, 18: 1.734, 19: 1.729, 20: 1.725, 25: 1.708}


def t95(df):
    if df >= 30:
        return 1.645
    return T95.get(df) or T95[max(k for k in T95 if k <= df)]


def load_file(path):
    """({metric: {cycle: ns}}, {metric: {cycle: bytes}}) for cycles >= 1."""
    ns, by = {}, {}
    with open(path) as f:
        for line in f:
            try:
                rec = json.loads(line)
            except ValueError:
                continue
            if "metric" not in rec or rec.get("cycle", 0) < 1:
                continue
            if "ns" in rec:
                ns.setdefault(rec["metric"], {})[rec["cycle"]] = rec["ns"]
            elif "bytes" in rec:
                by.setdefault(rec["metric"], {})[rec["cycle"]] = rec["bytes"]
    return ns, by


def load(directory, tag, fmt):
    """{batch: (ns, bytes)}"""
    pat = re.compile(re.escape(f"{tag}_{fmt}_") + r"(\d+)\.jsonl$")
    out = {}
    for p in glob.glob(os.path.join(directory, f"{tag}_{fmt}_*.jsonl")):
        m = pat.search(os.path.basename(p))
        if m:
            out[int(m.group(1))] = load_file(p)
    return out


def cycle_sum(ns, keys, c):
    total = 0
    for k in keys:
        v = ns.get(k, {}).get(c)
        if v is None or v <= 0:
            return None
        total += v
    return total


def ratio(xo, xkeys, rival, rkeys):
    """GM over batches of the per-batch mean log ratio; min/max batch; one-sided 95% upper."""
    logs = []
    for b in sorted(set(xo) & set(rival)):
        xn, rn = xo[b][0], rival[b][0]
        cyc = set(xn.get(xkeys[0], {})) & set(rn.get(rkeys[0], {}))
        pairs = []
        for c in sorted(cyc):
            a, r = cycle_sum(xn, xkeys, c), cycle_sum(rn, rkeys, c)
            if a and r:
                pairs.append(math.log(a / r))
        if pairs:
            logs.append(statistics.fmean(pairs))
    if not logs:
        return None
    gm = math.exp(statistics.fmean(logs))
    upper = float("inf")
    if len(logs) > 1:
        upper = math.exp(math.log(gm) + t95(len(logs) - 1) * statistics.stdev(logs) / math.sqrt(len(logs)))
    return {"gm": gm, "lo": math.exp(min(logs)), "hi": math.exp(max(logs)), "upper": upper, "batches": len(logs)}


def classify(gm):
    if gm < WIN:
        return "win"
    return "loss" if gm > LOSS else "tie"


CLASS_LABEL = {"win": "win", "tie": "tie", "loss": "loss"}


def shared_col(r):
    """Column name of rival r's shared-encoding approach (its D* rows)."""
    return f"{r} shared"


def evaluate(raw, xo_name, rivals, main_tag, d3_tag, shared_rivals=()):
    xo = {"main": load(raw, main_tag, xo_name), "d3": load(raw, d3_tag, xo_name)}
    rv = {r: load(raw, main_tag, r) for r in rivals}
    sv = {r: load(raw, d3_tag, r) for r in shared_rivals}
    sv = {r: v for r, v in sv.items() if v}
    rows = []
    for group, name, label, xkind, xkeys, rkeys, kind in CELLS:
        per = {}
        for r in rivals:
            res = ratio(xo[xkind], xkeys, rv[r], rkeys) if xo[xkind] and rv[r] else None
            if res:
                per[r] = res
        if xkind == "d3":
            # The rival's shared encoding: its own D* rows, the same keys as XOffset's.
            for r, run in sv.items():
                res = ratio(xo["d3"], xkeys, run, xkeys) if xo["d3"] else None
                if res:
                    per[shared_col(r)] = res
        best = max(per, key=lambda r: per[r]["gm"]) if per else None
        cls = classify(per[best]["gm"]) if best else "missing"
        rows.append({"group": group, "cell": name, "label": label, "kind": kind,
                     "rivals": per, "best": best, "class": cls})
    batches = {"xoffset": len(xo["main"]), "xoffset_d3": len(xo["d3"])}
    batches.update({r: len(rv[r]) for r in rivals})
    batches.update({shared_col(r): len(v) for r, v in sv.items()})
    return rows, batches


def byte_rows(directory, xo_name, rivals, main_tag, d3_tag, zstd, shared_rivals=()):
    if not directory:
        return []
    z = "_zstd" if zstd else ""
    xo = {"main": load(directory, main_tag, xo_name), "d3": load(directory, d3_tag, xo_name)}
    rv = {r: load(directory, main_tag, r) for r in rivals}
    sv = {r: load(directory, d3_tag, r) for r in shared_rivals}

    def med(run, key):
        xs = [v for b in run for v in run[b][1].get(key, {}).values()]
        return statistics.median(xs) if xs else None

    out = []
    for label, xkind, xkey, rkey in BYTE_ROWS:
        # Class-1 messages are not compressed by the bench; their sizes stay raw.
        xk, rk = xkey.replace("{z}", z), rkey.replace("{z}", z)
        x = med(xo[xkind], xk)
        if x is None:
            continue
        vals = {r: med(rv[r], rk) for r in rivals}
        if xkind == "d3":
            vals.update({shared_col(r): med(v, xk) for r, v in sv.items() if v and med(v, xk) is not None})
        out.append({"label": label, "xo_key": xk, "rival_key": rk, "xo": x, "rivals": vals})
    return out


def fmt_ratio(res):
    if not res:
        return ""
    return f"{res['gm']:.3f} [{res['lo']:.2f}–{res['hi']:.2f}]"


def render(rows, batches, rivals, brows, zstd, env, baseline):
    out = []
    # Shared-encoding columns (D cells only) after the rivals' own.
    rivals = list(rivals) + sorted({c for r in rows for c in r["rivals"] if c not in rivals})
    main = [r for r in rows if r["kind"] == "main"]
    counts = {c: sum(1 for r in main if r["class"] == c) for c in ("win", "tie", "loss", "missing")}
    out.append("# SLG scorecard\n")
    if env:
        src = env.get("source", {})
        out.append(f"- Commit: `{src.get('ref', '?')}` = `{src.get('commit', '?')}`")
        tc = env.get("toolchain", {})
        out.append(f"- Compiler: {tc.get('compiler', '?')}, image `{tc.get('image', '?')}`")
        if tc.get("extra_cxxflags"):
            out.append(f"- Extra compile flags: `{tc['extra_cxxflags']}` (EXTRA_CXXFLAGS, the same for every format)")
        deps = env.get("deps", {})
        out.append("- Rivals: " + ", ".join(f"{k} `{v.get('commit', '?')[:12]}`" for k, v in deps.items()))
        host = env.get("host", {})
        out.append(f"- Machine: {host.get('cpu_model', '?')}, {host.get('nproc', '?')} cores, pinned to CPU {env.get('run', {}).get('cpu', '?')}")
        run = env.get("run", {})
        out.append(f"- Run: {run.get('scale', 'M')} scale, {run.get('batches', '?')} batches × {run.get('cycles', '?')} cycles, "
                   f"seed {run.get('seed', '?')}, one process per format per batch, formats interleaved")
    out.append(f"- Batches: " + ", ".join(f"{k} {v}" for k, v in batches.items()))
    out.append(f"- Ratio = xoffset_trusted ÷ the strongest rival (the one XOffset is slowest against), geometric mean over batches; brackets are the batch min–max. "
               f"Rivals of the D cells include their shared-encoding approach (the \"shared\" columns, 4.0 W1). "
               f"Win < {WIN:.2f}, tie {WIN:.2f}–{LOSS:.2f}, loss > {LOSS:.2f}.\n")
    out.append(f"**Main cells {len(main)}: win {counts['win']}, tie {counts['tie']}, loss {counts['loss']}"
               + (f", missing {counts['missing']}" if counts['missing'] else "") + ".** UL.send is report only and not counted.\n")
    has_base = bool(baseline)
    head = "| Group | Cell | Scenario | ÷ strongest rival | Strongest rival | Verdict | " + " | ".join(f"÷ {r}" for r in rivals)
    sep = "| --- | --- | --- | ---: | --- | --- | " + " | ".join("---:" for _ in rivals)
    if has_base:
        head += " | Baseline | Change"
        sep += " | ---: | ---:"
    out.append(head + " |")
    out.append(sep + " |")
    base_map = {b["cell"]: b for b in (baseline or {}).get("cells", [])}
    for r in rows:
        best = r["rivals"].get(r["best"]) if r["best"] else None
        verdict = CLASS_LABEL.get(r["class"], "missing")
        if r["kind"] == "report" and best:
            verdict = f"report only ({verdict})"
        line = (f"| {r['group']} | {r['cell']} | {r['label']} | {best['gm']:.3f} | {r['best']} | {verdict} | "
                if best else f"| {r['group']} | {r['cell']} | {r['label']} | | | missing | ")
        line += " | ".join(fmt_ratio(r["rivals"].get(x)) for x in rivals)
        if has_base:
            b = base_map.get(r["cell"])
            if b and b.get("gm") and best:
                line += f" | {b['gm']:.3f} | {best['gm'] / b['gm'] - 1:+.1%}"
            else:
                line += " | | "
        out.append(line + " |")
    if brows:
        what = "after zstd-3" if zstd else "uncompressed (no compression build in this run)"
        out.append(f"\n## Bytes (report only, per-cycle median, {what}; A/B/S/SD not compressed)\n")
        out.append("| Message | Key | XOffset | " + " | ".join(rivals) + " | " + " | ".join(f"÷ {r}" for r in rivals) + " |")
        out.append("| --- | --- | ---: | " + " | ".join("---:" for _ in rivals) + " | " + " | ".join("---:" for _ in rivals) + " |")
        for b in brows:
            vals = [b["rivals"].get(r) for r in rivals]
            out.append(f"| {b['label']} | `{b['xo_key']}` | {b['xo']:.0f} | "
                       + " | ".join(f"{v:.0f}" if v else "" for v in vals) + " | "
                       + " | ".join(f"{b['xo'] / v:.2f}" if v else "" for v in vals) + " |")
    return "\n".join(out) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("raw")
    ap.add_argument("--bytes-dir", help="run of the SLG_WITH_COMPRESSION=1 build (zstd-3 bytes)")
    ap.add_argument("--xo", default="xoffset_trusted")
    ap.add_argument("--rivals", default="zpp_bits,cista_unchecked")
    ap.add_argument("--shared-rivals", default="zpp_bits,zpp_bits_dirty",
                    help="formats whose d3-tag D* rows are a rival's shared encoding (absent ones are skipped)")
    ap.add_argument("--main-tag", default="head")
    ap.add_argument("--d3-tag", default="d3")
    ap.add_argument("--env", help="env.json written by repro_scorecard.sh")
    ap.add_argument("--baseline", help="scorecard.json (or a {cells:[{cell,gm}]} file) to diff against")
    ap.add_argument("--json")
    a = ap.parse_args(argv)
    rivals = [r for r in a.rivals.split(",") if r]
    shared = [r for r in a.shared_rivals.split(",") if r]
    rows, batches = evaluate(a.raw, a.xo, rivals, a.main_tag, a.d3_tag, shared)
    zstd = bool(a.bytes_dir)
    brows = byte_rows(a.bytes_dir or a.raw, a.xo, rivals, a.main_tag, a.d3_tag, zstd, shared)
    env = baseline = None
    if a.env:
        with open(a.env) as f:
            env = json.load(f)
    if a.baseline:
        with open(a.baseline) as f:
            baseline = json.load(f)
    sys.stdout.write(render(rows, batches, rivals, brows, zstd, env, baseline))
    if a.json:
        cells = []
        for r in rows:
            best = r["rivals"].get(r["best"]) if r["best"] else None
            cells.append({"group": r["group"], "cell": r["cell"], "kind": r["kind"], "best": r["best"],
                          "gm": best["gm"] if best else None, "class": r["class"], "rivals": r["rivals"]})
        with open(a.json, "w") as f:
            json.dump({"cells": cells, "batches": batches, "bytes": brows, "zstd_bytes": zstd,
                       "thresholds": {"win_below": WIN, "loss_above": LOSS}}, f, indent=1, ensure_ascii=False)
    missing = [r for r in rows if r["kind"] == "main" and r["class"] == "missing"]
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
