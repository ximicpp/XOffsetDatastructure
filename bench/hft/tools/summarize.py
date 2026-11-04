#!/usr/bin/env python3
"""Tables for the HFT demo (bench/hft/run.sh): summarize.py OUT_DIR.

Reads OUT_DIR/time.jsonl (timed runs), OUT_DIR/ir.jsonl (callgrind runs) and
OUT_DIR/handshake.json, checks that every model and format of a case reaches
the same digest (and that a joining client ends with the state service's
digest), and writes OUT_DIR/summary.md. Exits 1 on a digest mismatch.
"""
import json
import statistics
import sys
from pathlib import Path

STATE = ["pool", "std", "xo", "cista"]
HOT = ["raw", "fixed", "sbe", "xo"]
JOIN = ["xo", "xo_verified", "pool", "cista", "cista_checked", "zpp", "fb", "fb_verified", "sbe"]


def lines(path):
    if not path.exists():
        return []
    return [json.loads(l) for l in path.read_text().splitlines() if l.strip()]


def key(run):
    return run.get("model") or run.get("format")


def main():
    out = Path(sys.argv[1])
    timed = lines(out / "time.jsonl")
    ir = lines(out / "ir.jsonl")
    runs = timed + [r["run"] for r in ir]
    md = []
    ok = True

    # Digests: one per case; join ends where state ends.
    digests = {}
    for r in runs:
        digests.setdefault(r["case"], {}).setdefault(r["digest"], set()).add(key(r))
    md.append("## Digests\n")
    for case, d in sorted(digests.items()):
        state = "equal" if len(d) == 1 else "MISMATCH"
        ok &= len(d) == 1
        md.append(f"- {case}: {state} ({', '.join(f'{k} {sorted(v)}' for k, v in d.items())})")
    if "state" in digests and "join" in digests:
        same = set(digests["state"]) == set(digests["join"])
        ok &= same
        md.append(f"- join clients end with the state service's digest: {'yes' if same else 'NO'}")
    md.append("")

    def med(case, name, field):
        v = [r[field] for r in timed if r["case"] == case and key(r) == name and field in r]
        return statistics.median(v) if v else None

    def irv(case, name, phase):
        for r in ir:
            if r["ir_case"] == case and r["name"] == name and r["phase"] == phase:
                return r["ir"], r["run"]
        return None, None

    def f(v, fmt="{:.1f}"):
        return "" if v is None else fmt.format(v)

    reps = max([sum(1 for r in timed if r["case"] == "state" and key(r) == m) for m in STATE] + [0])

    # Claim C: state service.
    md.append("## State service (claim C): apply every update of the window in place\n")
    md.append(f"Ir per update (callgrind); ns per update (median of {reps} timed runs, noisy on a cloud VM).\n")
    md.append("| model | Ir/update | Ir vs pool | ns/update | snapshot bytes | levels | orders | longest queue |")
    md.append("|---|---:|---:|---:|---:|---:|---:|---:|")
    base = None
    for m in STATE:
        v, run = irv("state", m, "apply")
        per = v / run["updates"] if v else None
        if m == "pool":
            base = per
        any_run = run or next((r for r in timed if r["case"] == "state" and key(r) == m), None)
        if not any_run:
            continue
        md.append(f"| {m} | {f(per)} | {f(per / base if per and base else None, '{:.2f}x')} | "
                  f"{f(med('state', m, 'ns_per_update'))} | {any_run['snapshot_bytes'] or ''} | "
                  f"{any_run['levels']} | {any_run['orders']} | {any_run['max_queue']} |")
    md.append("")

    # Claim A: hot path.
    md.append("## Hot path (claim A): publish each packet into a ring slot, 4 readers read every field\n")
    md.append("| format | bytes/packet | encode Ir/packet | read Ir/packet/reader | encode ns/packet | read ns/packet/reader |")
    md.append("|---|---:|---:|---:|---:|---:|")
    for h in HOT:
        e, run = irv("hot", h, "encode")
        rd, _ = irv("hot", h, "read")
        any_run = run or next((r for r in timed if r["case"] == "hot" and key(r) == h), None)
        if not any_run:
            continue
        p = any_run["packets"]
        md.append(f"| {h} | {any_run['bytes_per_packet']:.1f} | {f(e / p if e else None)} | "
                  f"{f(rd / p / 4 if rd else None)} | {f(med('hot', h, 'encode_ns_per_packet'))} | "
                  f"{f(med('hot', h, 'read_ns_per_packet_per_reader'))} |")
    md.append("")

    # Claim B: late join.
    md.append("## Late join (claim B): snapshot at the middle of the window, client applies the rest\n")
    md.append("| format | bytes | snapshot Ir | ready Ir | rest Ir/update | snapshot ms | ready ms | rest ns/update |")
    md.append("|---|---:|---:|---:|---:|---:|---:|---:|")
    for j in JOIN:
        s, run = irv("join", j, "snapshot")
        rdy, _ = irv("join", j, "ready")
        rest, _ = irv("join", j, "rest")
        any_run = run or next((r for r in timed if r["case"] == "join" and key(r) == j), None)
        if not any_run:
            continue
        sm, rm = med("join", j, "snapshot_ns"), med("join", j, "ready_ns")
        md.append(f"| {j} | {any_run['bytes']} | {f(s, '{:,}')} | {f(rdy, '{:,}')} | "
                  f"{f(rest / any_run['rest_updates'] if rest else None)} | {f(sm / 1e6 if sm else None, '{:.2f}')} | "
                  f"{f(rm / 1e6 if rm else None, '{:.2f}')} | {f(med('join', j, 'rest_ns_per_update'))} |")
    md.append("")

    hs = out / "handshake.json"
    if hs.exists():
        h = json.loads(hs.read_text())
        md.append("## Handshake (claim A): a strategy from another build connects\n")
        md.append(f"The feed sends order id {h['sent_order_id']}.\n")
        md.append("| strategy | verdict | same batch tag | view_fixed_batch | a raw cast reads order id | hello bytes |")
        md.append("|---|---|---|---|---:|---:|")
        for r in h["runs"]:
            md.append(f"| {r['strategy']} | {r['verdict']} | {r['same_batch_tag']} | {r['view_fixed_batch']} | "
                      f"{r['raw_cast_order_id']} | {r['hello_bytes']} |")
        md.append("")

    text = "\n".join(md)
    (out / "summary.md").write_text(text + "\n")
    print(text)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
