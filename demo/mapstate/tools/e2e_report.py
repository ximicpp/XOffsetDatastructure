#!/usr/bin/env python3
"""Map state demo: one end-to-end table over the client targets of run_e2e.sh.

Reads OUT_ROOT/<NAME>/{selfcheck,replay}/ and OUT_ROOT/<NAME>/summary.json
(run_all_tiers.sh) for every target NAME and writes OUT_ROOT/E2E_RESULT.md
and OUT_ROOT/e2e_summary.json. Exit 0 only when every target passed and
every target's replay produced the same wire and content digests, row by
row, as A wrote into the corpus. Python 3.9 compatible.
"""
import argparse
import csv
import hashlib
import json
import math
import os
import sys

ENCODINGS = ("dense", "compact", "zstd")


def read_csv(path):
    if not os.path.isfile(path):
        return None
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def read_kv(path):
    out = {}
    if os.path.isfile(path):
        with open(path) as f:
            for line in f:
                if "=" in line:
                    k, v = line.rstrip("\n").split("=", 1)
                    out[k] = v
    return out


def read_text(path):
    if not os.path.isfile(path):
        return ""
    with open(path) as f:
        return f.read().strip()


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def fmt(x, digits=1):
    if x is None or (isinstance(x, float) and math.isnan(x)):
        return "-"
    if abs(x) >= 100:
        return "{:,.0f}".format(x)
    return "{:.{}f}".format(x, digits)


def pct(values, p):
    """Nearest rank, as summarize.py."""
    v = sorted(values)
    if not v:
        return float("nan")
    return v[max(0, int(math.ceil(p * len(v))) - 1)]


def mean(values):
    return sum(values) / len(values) if values else float("nan")


def runner(cmd):
    """How the client ran: natively, or under wine / qemu / a device bridge."""
    first = os.path.basename(cmd.split()[0]) if cmd.strip() else "-"
    if first.startswith("wine"):
        return "wine"
    if first.startswith("qemu"):
        return "qemu user mode"
    if first in ("adb", "xcrun"):
        return first + " (emulator / simulator)"
    return "native"


def offline(root, name):
    d = os.path.join(root, name)
    codes = read_kv(os.path.join(d, "exit_codes.txt"))
    rows = read_csv(os.path.join(d, "client_checks.csv")) or []
    checks = [r for r in rows if r.get("result") in ("pass", "fail", "FAIL")]
    passed = sum(1 for r in checks if r.get("result") == "pass")
    ok = codes.get("client") == "0" and checks and passed == len(checks)
    return {"present": os.path.isdir(d), "exit": codes.get("client", "-"), "checks": len(checks),
            "passed": passed, "ok": bool(ok), "meta": read_kv(os.path.join(d, "meta.txt"))}


def target_summary(out_root, name, corpus_rows):
    root = os.path.join(out_root, name)
    t = {"name": name, "root": root, "problems": []}
    t["selfcheck"] = offline(root, "selfcheck")
    t["replay"] = offline(root, "replay")
    t["build"] = (read_text(os.path.join(root, "selfcheck", "client_build.txt"))
                  or read_text(os.path.join(root, "replay", "client_build.txt")) or "-")
    t["client_cmd"] = t["selfcheck"]["meta"].get("client_cmd", "-")
    t["runner"] = t["selfcheck"]["meta"].get("runs_under") or runner(t["client_cmd"])

    # Replay: the same digests as A wrote, row by row.
    replay = read_csv(os.path.join(root, "replay", "client_replay.csv")) or []
    by_rid = {r["request_id"]: r for r in replay}
    enc_count = {e: 0 for e in ENCODINGS}
    differ = 0
    for c in corpus_rows:
        r = by_rid.get(c["request_id"])
        if (r is None or r.get("result") != "match" or r.get("wire_digest") != c["wire_digest"]
                or r.get("digest") != c["digest"] or r.get("wire_bytes") != c["wire_bytes"]):
            differ += 1
        elif c["encoding"] in enc_count:
            enc_count[c["encoding"]] += 1
    t["replay_rows"] = len(replay)
    t["replay_same"] = len(corpus_rows) - differ
    t["replay_by_encoding"] = enc_count
    t["replay_digests"] = sorted((r["request_id"], r.get("wire_digest"), r.get("digest")) for r in replay)
    if not corpus_rows or differ:
        t["problems"].append("replay: {} of {} corpus snapshots differ or missing".format(differ, len(corpus_rows)))
    if not t["selfcheck"]["ok"]:
        t["problems"].append("self-check: exit {}, {}/{} checks pass".format(
            t["selfcheck"]["exit"], t["selfcheck"]["passed"], t["selfcheck"]["checks"]))
    if not t["replay"]["ok"]:
        t["problems"].append("replay: exit {}, {}/{} checks pass".format(
            t["replay"]["exit"], t["replay"]["passed"], t["replay"]["checks"]))

    summary_path = os.path.join(root, "summary.json")
    s = None
    if os.path.isfile(summary_path):
        with open(summary_path) as f:
            s = json.load(f)
    t["tiers_ok"] = bool(s and s.get("passed"))
    verdicts = (s or {}).get("verdicts", [])
    t["verdicts"] = len(verdicts)
    t["verdicts_failed"] = sum(1 for v in verdicts if not v.get("ok"))
    t["tier_names"] = [x.get("name") for x in (s or {}).get("tiers", [])]
    evil = [v for v in verdicts if v.get("scope", "").startswith("evil")]
    t["evil_ok"] = bool(evil) and all(v.get("ok") for v in evil)
    cmp = [v for v in verdicts if v.get("scope") == "compare"]
    t["compare"] = ("-" if not cmp else
                    "{}/{} tiers same step wires".format(sum(1 for v in cmp if v.get("ok")), len(cmp)))
    if s is None:
        t["problems"].append("tiers: no summary.json")
    elif not t["tiers_ok"]:
        t["problems"].append("tiers: {} of {} verdicts failed".format(t["verdicts_failed"], t["verdicts"]))

    # Snapshot sizes and client costs by category and encoding, largest
    # tier: within one category the encodings carry comparable content.
    tiers = (s or {}).get("tiers", [])
    t["largest"] = None
    if tiers:
        big = max(tiers, key=lambda x: x.get("units", 0))
        rows = read_csv(os.path.join(root, big.get("name", ""), "client_snapshots.csv")) or []
        cells = {}
        for cat in ("normal", "full"):
            for e in ENCODINGS:
                sel = [r for r in rows if r.get("category") == cat and r.get("encoding") == e]
                col = lambda k: [float(r[k]) for r in sel if r.get(k) not in (None, "")]
                cells[cat + "/" + e] = {
                    "count": len(sel), "wire_bytes": mean(col("wire_bytes")), "net_bytes": mean(col("net_bytes")),
                    "decompress_p50": pct(col("decompress_us"), 0.5), "load_p50": pct(col("load_us"), 0.5),
                    "rtt_p50": pct(col("rtt_us"), 0.5)}
        t["largest"] = {"name": big.get("name"), "units": big.get("units"), "cells": cells}
    t["ok"] = not t["problems"]
    return t


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("root", help="OUT_ROOT of run_e2e.sh")
    ap.add_argument("names", nargs="+", help="client target names")
    ap.add_argument("--corpus", required=True, help="snapshot corpus directory (corpus.csv)")
    ap.add_argument("--label", default="")
    args = ap.parse_args()

    corpus_csv = os.path.join(args.corpus, "corpus.csv")
    corpus_rows = read_csv(corpus_csv) or []
    corpus_bytes = sum(int(r.get("net_bytes") or 0) for r in corpus_rows)
    corpus_meta = read_kv(os.path.join(args.root, "corpus_run", "meta.txt"))
    targets = [target_summary(args.root, n, corpus_rows) for n in args.names]

    # Every platform verified the same snapshots to the same digests.
    ref = targets[0]["replay_digests"] if targets else []
    same_everywhere = bool(ref) and all(t["replay_digests"] == ref for t in targets)
    passed = bool(targets) and all(t["ok"] for t in targets) and same_everywhere

    lines = ["# Map state demo: end-to-end result", ""]
    lines.append("- label: {}".format(args.label or "-"))
    lines.append("- overall: **{}** ({} client target(s))".format("PASS" if passed else "FAIL", len(targets)))
    lines.append("- servers: Linux A and B; client binaries built per target, run as the 'runs under' column says. "
                 "Timings under wine, qemu or an emulator are not device timings.")
    lines.append("- corpus: {} snapshot(s), {:,} bytes on the wire, written by A{}; corpus.csv sha256 `{}`".format(
        len(corpus_rows), corpus_bytes,
        " at commit {} on {}".format(corpus_meta.get("commit", "?"), corpus_meta.get("host", "?"))
        if corpus_meta else "",
        sha256(corpus_csv) if os.path.isfile(corpus_csv) else "-"))
    lines.append("- same replay digests on every target: **{}**".format("yes" if same_everywhere else "NO"))
    lines.append("")
    lines.append("## Targets")
    lines.append("")
    lines.append("| target | runs under | client build | self-check | replay | tiers | evil | step wires | result |")
    lines.append("| --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    for t in targets:
        lines.append("| {} | {} | `{}` | {}/{} | {}/{} same digests ({}) | {} ({} verdicts{}) | {} | {} | {} |".format(
            t["name"], t["runner"], t["build"],
            t["selfcheck"]["passed"], t["selfcheck"]["checks"],
            t["replay_same"], len(corpus_rows),
            ", ".join("{} {}".format(e, t["replay_by_encoding"][e]) for e in ENCODINGS),
            ", ".join(t["tier_names"]) or "-", t["verdicts"],
            ", {} failed".format(t["verdicts_failed"]) if t["verdicts_failed"] else "",
            "pass" if t["evil_ok"] else "**FAIL**", t["compare"],
            "PASS" if t["ok"] else "**FAIL**"))
    lines.append("")
    problems = [(t["name"], p) for t in targets for p in t["problems"]]
    if problems:
        lines.append("## Problems")
        lines.append("")
        for name, p in problems:
            lines.append("- {}: {}".format(name, p))
        lines.append("")

    lines.append("## Snapshots by category and encoding (largest tier, client side)")
    lines.append("")
    lines.append("`normal` is a 1024x768 viewport, `full` the whole scene. Bytes are means; times are p50 "
                 "in microseconds. Round trip includes A's save and compression.")
    lines.append("")
    lines.append("| target | category | encoding | count | XOffset wire bytes | bytes sent | sent / wire | "
                 "decompress | verify | round trip |")
    lines.append("| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    for t in targets:
        big = t["largest"]
        if not big:
            continue
        for key, c in big["cells"].items():
            cat, e = key.split("/")
            wire, net = c["wire_bytes"], c["net_bytes"]
            ratio = net / wire if wire and not math.isnan(wire) and not math.isnan(net) else float("nan")
            lines.append("| {} | {} | {} | {} | {} | {} | {} | {} | {} | {} |".format(
                t["name"], cat, e, c["count"], fmt(wire), fmt(net), fmt(ratio, 3),
                fmt(c["decompress_p50"]) if e == "zstd" else "-", fmt(c["load_p50"]), fmt(c["rtt_p50"])))
    lines.append("")
    lines.append("Per-target detail: `<target>/RESULT_TABLES.md`; offline runs: `<target>/selfcheck/`, "
                 "`<target>/replay/client_replay.csv`.")

    with open(os.path.join(args.root, "E2E_RESULT.md"), "w") as f:
        f.write("\n".join(lines) + "\n")
    js = {"label": args.label, "passed": passed, "same_replay_digests": same_everywhere,
          "corpus": {"dir": args.corpus, "rows": len(corpus_rows), "bytes": corpus_bytes, "meta": corpus_meta},
          "targets": [{k: v for k, v in t.items() if k != "replay_digests"} for t in targets]}
    with open(os.path.join(args.root, "e2e_summary.json"), "w") as f:
        json.dump(js, f, indent=1, sort_keys=True, default=str)
    for name, p in problems:
        print("FAIL {}: {}".format(name, p), file=sys.stderr)
    if not same_everywhere:
        print("FAIL: replay digests differ between targets", file=sys.stderr)
    print("e2e_report: {} ({} targets)".format("PASS" if passed else "FAIL", len(targets)), file=sys.stderr)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
