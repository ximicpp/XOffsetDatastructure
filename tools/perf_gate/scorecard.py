#!/usr/bin/env python3
"""SLG scorecard from bench/slg JSONL: 20 main cells, XOffset ÷ the strongest rival.

usage: scorecard.py RAW_DIR [--bytes-dir DIR] [--xo xoffset_trusted]
                    [--rivals zpp_bits,cista_unchecked] [--main-tag head] [--d3-tag d3]
                    [--env env.json] [--baseline scorecard.json] [--json OUT]

RAW_DIR holds one JSONL per process, <tag>_<format>_<batch>.jsonl
(tools/perf_gate/repro_scorecard.sh writes them so):
  <main-tag>  every format, SLG_NATIVE_OFF=1 (application-level rows)
  <d3-tag>    XOffset only, SLG_D3=1 with native on (the shared-delta D* rows)
Every process of one batch runs the same seed, so cycle c of batch b carries the
same messages in every format. A cell's ratio is taken per (batch, cycle >= 1)
on the per-cycle sum of the cell's keys, averaged in log space within a batch;
the point estimate is the geometric mean over batches. The D cells pair
XOffset's D* rows (d3 processes) with each rival's per-client U* rows (main
processes): the rivals have no shared-delta path.

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
    ("服务端之间", "A", "战斗请求和结果", "main", A_KEYS, A_KEYS, "main"),
    ("服务端之间", "B", "迁服", "main", B_KEYS, B_KEYS, "main"),
    ("服务端之间", "S", "存盘和读盘", "main", S_KEYS, S_KEYS, "main"),
    ("服务端之间", "SD", "增量存盘", "main", SD_KEYS, SD_KEYS, "main"),
    ("服务端发送", "L.send", "登录全量", "main", ["L.send"], ["L.send"], "main"),
    ("服务端发送", "EB.send", "浏览全量", "main", ["EB.send"], ["EB.send"], "main"),
    ("服务端发送", "LD.send", "自身增量", "main", ["LD.send"], ["LD.send"], "main"),
    ("服务端发送", "UP.send", "视野增量，和平", "main", ["UP.send"], ["UP.send"], "main"),
    ("服务端发送", "UC.send", "视野增量，冲突", "main", ["UC.send"], ["UC.send"], "main"),
    ("服务端发送", "DP", "共享增量，和平：发布加发送 ÷ 对手 UP.send", "d3", ["DP.pub", "DP.send"], ["UP.send"], "main"),
    ("服务端发送", "DC", "共享增量，冲突：发布加发送 ÷ 对手 UC.send", "d3", ["DC.pub", "DC.send"], ["UC.send"], "main"),
    ("客户端接收", "L.recv", "登录全量", "main", ["L.recv"], ["L.recv"], "main"),
    ("客户端接收", "EBW.recv", "浏览全量（热视图）", "main", ["EBW.recv"], ["EBW.recv"], "main"),
    ("客户端接收", "LDW.recv", "自身增量（热视图）", "main", ["LDW.recv"], ["LDW.recv"], "main"),
    ("客户端接收", "UPW.recv", "视野增量，和平（热视图）", "main", ["UPW.recv"], ["UPW.recv"], "main"),
    ("客户端接收", "UCW.recv", "视野增量，冲突（热视图）", "main", ["UCW.recv"], ["UCW.recv"], "main"),
    ("客户端接收", "DPTW.recv", "共享增量，和平 ÷ 对手 UPW.recv", "d3", ["DPTW.recv"], ["UPW.recv"], "main"),
    ("客户端接收", "DCTW.recv", "共享增量，冲突 ÷ 对手 UCW.recv", "d3", ["DCTW.recv"], ["UCW.recv"], "main"),
    ("上行", "UL.recv", "服务端收指令（边界检查）", "main", ["UL.recv"], ["UL.recv"], "main"),
    ("上行", "UL.send", "客户端发指令（只报）", "main", ["UL.send"], ["UL.send"], "report"),
    # Report-only rows outside the 20 cells.
    ("只报", "US.send", "视野增量，围城", "main", ["US.send"], ["US.send"], "report"),
    ("只报", "DS", "共享增量，围城：发布加发送 ÷ 对手 US.send", "d3", ["DS.pub", "DS.send"], ["US.send"], "report"),
    ("只报", "USW.recv", "视野增量，围城（热视图）", "main", ["USW.recv"], ["USW.recv"], "report"),
    ("只报", "DSTW.recv", "共享增量，围城 ÷ 对手 USW.recv", "d3", ["DSTW.recv"], ["USW.recv"], "report"),
    ("只报", "EB.recv", "浏览全量（冷）", "main", ["EB.recv"], ["EB.recv"], "report"),
    ("只报", "LD recv+apply", "自身增量（冷）", "main", ["LD.recv", "LD.apply"], ["LD.recv", "LD.apply"], "report"),
    ("只报", "UP recv+apply", "视野增量，和平（冷）", "main", ["UP.recv", "UP.apply"], ["UP.recv", "UP.apply"], "report"),
    ("只报", "UC recv+apply", "视野增量，冲突（冷）", "main", ["UC.recv", "UC.apply"], ["UC.recv", "UC.apply"], "report"),
]
BYTE_ROWS = [  # (label, xo tag kind, xo key, rival key)
    ("A 请求", "main", "A.req", "A.req"), ("A 结果", "main", "A.res", "A.res"),
    ("B 迁服", "main", "B.out", "B.out"), ("S 存盘", "main", "S.player", "S.player"),
    ("SD 增量存盘", "main", "SD.modules", "SD.modules"),
    ("L 登录", "main", "L.player{z}", "L.player{z}"), ("EB 浏览", "main", "EB.window{z}", "EB.window{z}"),
    ("LD 自身增量", "main", "LD.patch{z}", "LD.patch{z}"),
    ("UP 视野和平", "main", "UP.delta{z}", "UP.delta{z}"), ("UC 视野冲突", "main", "UC.delta{z}", "UC.delta{z}"),
    ("DP 共享和平 ÷ 对手 UP", "d3", "DP.delta{z}", "UP.delta{z}"),
    ("DC 共享冲突 ÷ 对手 UC", "d3", "DC.delta{z}", "UC.delta{z}"),
    ("UL 上行指令", "main", "UL.order{z}", "UL.order{z}"),
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


CLASS_ZH = {"win": "赢", "tie": "平", "loss": "输"}


def evaluate(raw, xo_name, rivals, main_tag, d3_tag):
    xo = {"main": load(raw, main_tag, xo_name), "d3": load(raw, d3_tag, xo_name)}
    rv = {r: load(raw, main_tag, r) for r in rivals}
    rows = []
    for group, name, label, xkind, xkeys, rkeys, kind in CELLS:
        per = {}
        for r in rivals:
            res = ratio(xo[xkind], xkeys, rv[r], rkeys) if xo[xkind] and rv[r] else None
            if res:
                per[r] = res
        best = max(per, key=lambda r: per[r]["gm"]) if per else None
        cls = classify(per[best]["gm"]) if best else "missing"
        rows.append({"group": group, "cell": name, "label": label, "kind": kind,
                     "rivals": per, "best": best, "class": cls})
    batches = {"xoffset": len(xo["main"]), "xoffset_d3": len(xo["d3"])}
    batches.update({r: len(rv[r]) for r in rivals})
    return rows, batches


def byte_rows(directory, xo_name, rivals, main_tag, d3_tag, zstd):
    if not directory:
        return []
    z = "_zstd" if zstd else ""
    xo = {"main": load(directory, main_tag, xo_name), "d3": load(directory, d3_tag, xo_name)}
    rv = {r: load(directory, main_tag, r) for r in rivals}

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
        out.append({"label": label, "xo_key": xk, "rival_key": rk, "xo": x,
                    "rivals": {r: med(rv[r], rk) for r in rivals}})
    return out


def fmt_ratio(res):
    if not res:
        return ""
    return f"{res['gm']:.3f} [{res['lo']:.2f}–{res['hi']:.2f}]"


def render(rows, batches, rivals, brows, zstd, env, baseline):
    out = []
    main = [r for r in rows if r["kind"] == "main"]
    counts = {c: sum(1 for r in main if r["class"] == c) for c in ("win", "tie", "loss", "missing")}
    out.append("# SLG 计分卡\n")
    if env:
        src = env.get("source", {})
        out.append(f"- 提交：`{src.get('ref', '?')}` = `{src.get('commit', '?')}`")
        tc = env.get("toolchain", {})
        out.append(f"- 编译：{tc.get('compiler', '?')}，镜像 `{tc.get('image', '?')}`")
        deps = env.get("deps", {})
        out.append("- 对手：" + "，".join(f"{k} `{v.get('commit', '?')[:12]}`" for k, v in deps.items()))
        host = env.get("host", {})
        out.append(f"- 机器：{host.get('cpu_model', '?')}，{host.get('nproc', '?')} 核，钉核 {env.get('run', {}).get('cpu', '?')}")
        run = env.get("run", {})
        out.append(f"- 跑法：{run.get('scale', 'M')} 规模，{run.get('batches', '?')} 批 × {run.get('cycles', '?')} 周期，"
                   f"种子 {run.get('seed', '?')}，每个格式每批一个进程，批间交错")
    out.append(f"- 批数：" + "，".join(f"{k} {v}" for k, v in batches.items()))
    out.append(f"- 比值 = xoffset_trusted ÷ 最强对手（XOffset 比它慢得最多的那家），批间几何均值，方括号是批的最小–最大。"
               f"赢 < {WIN:.2f}，平 {WIN:.2f}–{LOSS:.2f}，输 > {LOSS:.2f}。\n")
    out.append(f"**主格 {len(main)}：赢 {counts['win']}，平 {counts['tie']}，输 {counts['loss']}"
               + (f"，缺 {counts['missing']}" if counts['missing'] else "") + "。** UL.send 只报，不计。\n")
    has_base = bool(baseline)
    head = "| 组 | 格 | 场景 | ÷ 最强对手 | 最强对手 | 判定 | " + " | ".join(f"÷ {r}" for r in rivals)
    sep = "| --- | --- | --- | ---: | --- | --- | " + " | ".join("---:" for _ in rivals)
    if has_base:
        head += " | 基线 | 差"
        sep += " | ---: | ---:"
    out.append(head + " |")
    out.append(sep + " |")
    base_map = {b["cell"]: b for b in (baseline or {}).get("cells", [])}
    for r in rows:
        best = r["rivals"].get(r["best"]) if r["best"] else None
        verdict = CLASS_ZH.get(r["class"], "缺")
        if r["kind"] == "report" and best:
            verdict = f"只报（{verdict}）"
        line = (f"| {r['group']} | {r['cell']} | {r['label']} | {best['gm']:.3f} | {r['best']} | {verdict} | "
                if best else f"| {r['group']} | {r['cell']} | {r['label']} | | | 缺 | ")
        line += " | ".join(fmt_ratio(r["rivals"].get(x)) for x in rivals)
        if has_base:
            b = base_map.get(r["cell"])
            if b and b.get("gm") and best:
                line += f" | {b['gm']:.3f} | {best['gm'] / b['gm'] - 1:+.1%}"
            else:
                line += " | | "
        out.append(line + " |")
    if brows:
        what = "zstd-3 压缩后" if zstd else "未压缩（这次没跑压缩构建）"
        out.append(f"\n## 字节（只报，每周期中位数，{what}；A/B/S/SD 不压缩）\n")
        out.append("| 消息 | 键 | XOffset | " + " | ".join(rivals) + " | " + " | ".join(f"÷ {r}" for r in rivals) + " |")
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
    ap.add_argument("--main-tag", default="head")
    ap.add_argument("--d3-tag", default="d3")
    ap.add_argument("--env", help="env.json written by repro_scorecard.sh")
    ap.add_argument("--baseline", help="scorecard.json (or a {cells:[{cell,gm}]} file) to diff against")
    ap.add_argument("--json")
    a = ap.parse_args(argv)
    rivals = [r for r in a.rivals.split(",") if r]
    rows, batches = evaluate(a.raw, a.xo, rivals, a.main_tag, a.d3_tag)
    zstd = bool(a.bytes_dir)
    brows = byte_rows(a.bytes_dir or a.raw, a.xo, rivals, a.main_tag, a.d3_tag, zstd)
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
