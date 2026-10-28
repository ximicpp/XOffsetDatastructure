#!/usr/bin/env python3
"""Device mesh demo: Markdown report of one mesh run (tools/run_mesh.sh output).

  mesh_report.py OUT_DIR [--columns "col1 col2 ..."]

Reads OUT_DIR/hub/{devices,jobs,hops,matrix}.csv and summary.txt (xo_hub --out),
OUT_DIR/devices/<name>/exit and build.txt, and OUT_DIR/selftest/<column>.txt.
Prints: the result line, the devices, the sender x receiver matrix of pair
jobs, pair jobs by type, scale and save mode, the chains, the evil cases, the
data part sizes, per-device timings and the failures. Exit 1 when the
hub's summary is not RESULT PASS or a device did not exit 0.
"""
import argparse
import csv
import os
import statistics
import sys
from collections import defaultdict


def read_csv(path):
    if not os.path.exists(path):
        return []
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def read_text(path, default=""):
    try:
        with open(path, errors="replace") as f:
            return f.read().strip()
    except OSError:
        return default


def us(ns_values):
    vals = [int(v) for v in ns_values if v not in ("", None)]
    return f"{statistics.median(vals) / 1000:.1f}" if vals else ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--columns", default="")
    a = ap.parse_args()
    out = a.out
    hub = os.path.join(out, "hub")
    devices = read_csv(os.path.join(hub, "devices.csv"))
    jobs = read_csv(os.path.join(hub, "jobs.csv"))
    hops = read_csv(os.path.join(hub, "hops.csv"))
    summary = read_text(os.path.join(hub, "summary.txt"))
    result = "RESULT PASS" if summary.endswith("RESULT PASS") else "RESULT FAIL"
    lines = summary.splitlines()
    p = []
    p.append(f"# Device mesh run: {lines[0] if lines else os.path.basename(out)}")
    p.append("")
    device_dirs = sorted(os.listdir(os.path.join(out, "devices"))) if os.path.isdir(os.path.join(out, "devices")) else []
    exits = {d: read_text(os.path.join(out, "devices", d, "exit"), "none") for d in device_dirs}
    bad_exit = [d for d, rc in exits.items() if rc != "0"]
    ok = result == "RESULT PASS" and not bad_exit
    by_kind = defaultdict(lambda: [0, 0])
    for j in jobs:
        by_kind[j["kind"]][1] += 1
        if j["result"] == "pass":
            by_kind[j["kind"]][0] += 1
    kinds = ", ".join(f"{k} {v[0]}/{v[1]}" for k, v in sorted(by_kind.items()))
    p.append(f"**{'PASS' if ok else 'FAIL'}**: {len(devices)} devices, {len(jobs)} jobs ({kinds}); "
             f"hub `{result}`" + (f"; devices without exit 0: {', '.join(bad_exit)}" if bad_exit else "") + ".")
    p.append("")

    # Devices
    hop_by_dev = defaultdict(list)
    for h in hops:
        hop_by_dev[h["device"]].append(h)
    selftests = {}
    for c in a.columns.split():
        for tag in (c, c + "-noexc"):
            t = read_text(os.path.join(out, "selftest", f"{tag}.txt"))
            res = [ln for ln in t.splitlines() if ln.startswith("RESULT")]
            ext = [ln for ln in t.splitlines() if ln.startswith("exit ")]
            selftests[tag] = ((res[-1] if res else "no RESULT line"), (ext[-1] if ext else "exit ?"))
    p.append("## Devices")
    p.append("")
    p.append("| id | device | jobs passed | hops ok | exit | median us verify / check / mutate / save | build |")
    p.append("| --- | --- | --- | --- | --- | --- | --- |")
    job_dev = defaultdict(lambda: [0, 0])
    for j in jobs:
        for name in set(j["route"].split(">")):
            job_dev[name][1] += 1
            if j["result"] == "pass":
                job_dev[name][0] += 1
    for d in devices:
        name = d["name"]
        hs = hop_by_dev.get(name, [])
        okh = [h for h in hs if h["verdict"] == "ok"]
        t = " / ".join(us(h[k] for h in okh) or "-" for k in ("verify_ns", "check_ns", "mutate_ns", "save_ns"))
        jp = job_dev[name]
        p.append(f"| {d['id']} | {name} | {jp[0]}/{jp[1]} | {len(okh)}/{len(hs)} | {exits.get(name, '?')} | {t} | "
                 f"`{d['build']}` |")
    p.append("")
    if selftests:
        p.append("Self-tests (`xmesh_selftest`, `xmesh_selftest_noexc` on each column):")
        p.append("")
        p.append("| device | result | exit |")
        p.append("| --- | --- | --- |")
        for tag, (res, ext) in selftests.items():
            p.append(f"| {tag} | {res} | {ext.split()[-1]} |")
        p.append("")
        bad_self = [t for t, (r, e) in selftests.items() if not r.startswith("RESULT PASS") or e != "exit 0"]
        if bad_self:
            ok = False
            p[2] = p[2].replace("**PASS**", "**FAIL**") + f" Self-test failed: {', '.join(bad_self)}."

    # Matrix
    names = [d["name"] for d in devices]
    cells = defaultdict(lambda: [0, 0])
    for j in jobs:
        if j["kind"] != "pairs":
            continue
        r = j["route"].split(">")
        cells[(r[0], r[1])][1] += 1
        if j["result"] == "pass":
            cells[(r[0], r[1])][0] += 1
    if cells:
        p.append("## Pairs: sender (row) to receiver (column), passed/jobs")
        p.append("")
        p.append("Each pair job: the sender makes state 0, the receiver checks, changes and sends it back, the sender "
                 "checks it again; every type, scale and save mode.")
        p.append("")
        idx = {n: i + 1 for i, n in enumerate(names)}
        p.append("| sender \\ receiver | " + " | ".join(str(idx[n]) for n in names) + " |")
        p.append("| --- |" + " --- |" * len(names))
        for s in names:
            row = []
            for r in names:
                if s == r:
                    row.append("-")
                else:
                    c = cells.get((s, r))
                    row.append(f"{c[0]}/{c[1]}" if c else "")
            p.append(f"| {idx[s]} {s} | " + " | ".join(row) + " |")
        p.append("")

        combo = defaultdict(lambda: [0, 0])
        for j in jobs:
            if j["kind"] == "pairs":
                k = (j["type"], j["scale"], j["save_mode"])
                combo[k][1] += 1
                if j["result"] == "pass":
                    combo[k][0] += 1
        types = sorted({k[0] for k in combo})
        cols = sorted({(k[1], k[2]) for k in combo}, key=lambda x: (x[0] != "small", x[1] != "verified"))
        p.append("## Pairs by type, scale and save mode")
        p.append("")
        p.append("| type | " + " | ".join(f"{s} {m}" for s, m in cols) + " |")
        p.append("| --- |" + " --- |" * len(cols))
        for t in types:
            p.append(f"| {t} | " + " | ".join(
                f"{combo[(t, s, m)][0]}/{combo[(t, s, m)][1]}" if (t, s, m) in combo else "" for s, m in cols) + " |")
        p.append("")

    chains = [j for j in jobs if j["kind"] == "chain"]
    if chains:
        p.append("## Chains (every device once, back to the first)")
        p.append("")
        p.append("| job | type | scale | save mode | stops | result | ms |")
        p.append("| --- | --- | --- | --- | --- | --- | --- |")
        for j in chains:
            p.append(f"| {j['job']} | {j['type']} | {j['scale']} | {j['save_mode']} | {len(j['route'].split('>'))} | "
                     f"{j['result']} | {j['ms']} |")
        p.append("")

    evil = [j for j in jobs if j["kind"] == "evil"]
    if evil:
        verdict_of = {}
        for h in hops:
            if h["hop"] == "1":
                verdict_of[h["job"]] = h["verdict"]
        per = defaultdict(lambda: [0, 0, set()])
        for j in evil:
            e = per[j["evil"]]
            e[1] += 1
            if j["result"] == "pass":
                e[0] += 1
            if j["job"] in verdict_of:
                e[2].add(verdict_of[j["job"]])
        p.append("## Evil cases (route[0] sends a bad object, route[1] must refuse it)")
        p.append("")
        p.append("| case | refused as expected | receivers' verdicts |")
        p.append("| --- | --- | --- |")
        for case, (np_, nt, vs) in sorted(per.items()):
            p.append(f"| {case} | {np_}/{nt} | {', '.join(sorted(vs))} |")
        p.append("")

    # Sizes of state 0 (route[0]'s sent bytes)
    job_info = {j["job"]: j for j in jobs}
    sizes = defaultdict(list)
    grown = defaultdict(list)
    for h in hops:
        j = job_info.get(h["job"])
        if not j or j["kind"] == "evil" or h["verdict"] != "ok":
            continue
        if h["hop"] == "0":
            sizes[(j["type"], j["scale"], j["save_mode"])].append(int(h["sent_bytes"] or 0))
        elif h["hop"] == "1":
            grown[(j["type"], j["scale"])].append(int(h["grown_bytes"] or 0))
    if sizes:
        p.append("## Data part of state 0 (bytes, as route[0] sends it)")
        p.append("")
        p.append("| type | scale | save mode | bytes | arena growth at hop 1 (median) |")
        p.append("| --- | --- | --- | --- | --- |")
        for (t, s, m), v in sorted(sizes.items()):
            g = grown.get((t, s), [])
            p.append(f"| {t} | {s} | {m} | {min(v)}" + (f"..{max(v)}" if max(v) != min(v) else "") +
                     f" | {int(statistics.median(g)) if g else ''} |")
        p.append("")

    fails = [j for j in jobs if j["result"] != "pass"]
    if fails:
        p.append(f"## Failures ({len(fails)}, first 40)")
        p.append("")
        for j in fails[:40]:
            p.append(f"- job {j['job']} {j['kind']} {j['type']} {j['scale']} {j['save_mode']} {j['evil']} "
                     f"`{j['route']}`: {j['reason']}")
        p.append("")
    p.append("Hub summary:")
    p.append("")
    p.append("```")
    p.extend(lines)
    p.append("```")
    print("\n".join(p))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
