#!/usr/bin/env python3
"""Map state demo: check and tabulate the output of run_all_tiers.sh.

OWNER: integration thread. Reads OUT_ROOT/{selftest,tier_<N>,evil}/ (CSV
formats in demo/mapstate/INTERFACES.md section 6) and writes one Markdown
report plus a JSON summary. Exit 0 only when every verdict passes.

Verdicts per tier:
  * a, b and client exited 0;
  * every metric name of INTERFACES.md section 6 recorded (headless: no draw);
  * a_steps.csv has at least --min-steps rows, ticks consecutive, check_ok all 1;
  * a_checks.csv and client_checks.csv exist, non-empty, every row "pass";
    a_checks.csv has the six fixed row names;
  * every snapshot request id logged by A was logged by the client and vice
    versa, with equal tick, category, visible, wire_bytes and digest;
  * normal, full and empty categories each sampled on both sides; every
    empty snapshot has visible == 0; every full-scene snapshot has
    visible == units; check_ok all 1 on both sides.
Self-test: exit 0 and every selftest_checks.csv row "pass".
Evil: a, b, client exited 0, A logged at least 3 "rejected:" lines and every
row of the evil client's client_checks.csv passes.

Per-category numbers are computed here from the per-request rows
(nearest-rank percentiles); whole-run numbers come from the processes'
own *_metrics.csv. Python 3.9 compatible (macOS CLT python3).
"""
import argparse
import csv
import json
import math
import os
import re
import sys

CATEGORIES = ("normal", "full", "empty")

BATCH_METRICS = (
    ("a.step_req_bytes", "A->B request wire"),
    ("a.step_res_bytes", "B->A result wire"),
    ("a.ab_rtt_us", "A<->B round trip (send start -> StepRes complete)"),
    ("a.save_us", "A save_verified (batch)"),
    ("a.load_us", "A load_verified (B's result)"),
    ("a.destroy_us", "A destroy old batch"),
    ("b.load_us", "B load_verified"),
    ("b.modify_us", "B step in place"),
    ("b.save_us", "B save_verified"),
    ("b.send_us", "B send"),
    ("b.destroy_us", "B destroy received buffer"),
    ("b.in_bytes", "B bytes in"),
    ("b.out_bytes", "B bytes out"),
    ("a.check_us", "A checks (reference model; not part of the path)"),
)
VIEW_METRICS = (
    ("a.req_load_us", "A load_verified (ViewRequest)"),
    ("a.filter_us", "A filter"),
    ("a.snap_build_us", "A snapshot build"),
    ("a.snap_save_us", "A snapshot save_verified"),
    ("a.snap_destroy_us", "A snapshot destroy"),
    ("a.snap_bytes", "snapshot wire (A)"),
    ("a.visible_count", "visible units (A)"),
    ("c.view_rtt_us", "client view round trip (ViewReq sent -> snapshot complete)"),
    ("c.load_us", "client load_verified (snapshot)"),
    ("c.read_present_us", "client field read + presentation data (MERGED)"),
    ("c.draw_us", "client draw"),
    ("c.present_us", "client present"),
    ("c.destroy_us", "client snapshot destroy"),
    ("c.snap_bytes", "snapshot wire (client)"),
    ("c.visible_count", "visible units (client)"),
    ("c.catalog_load_us", "client catalog load_verified (once)"),
)

# Row names server A writes to a_checks.csv (INTERFACES.md, 85ac689).
A_CHECK_NAMES = ("batch_roundtrip", "reference_model", "modification_coverage",
                 "view_snapshot", "snapshot_paths", "rejected_requests")

# INTERFACES.md section 6; the headless client has no draw / present.
HEADLESS_OPTIONAL = ("c.draw_us", "c.present_us")
REQUIRED_METRICS = tuple(k for k, _ in BATCH_METRICS + VIEW_METRICS
                         if k not in HEADLESS_OPTIONAL) + ("a.units_count", "a.ticks_per_s")


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
                line = line.rstrip("\n")
                if "=" in line:
                    k, v = line.split("=", 1)
                    out[k] = v
    return out


def num(text):
    try:
        return float(text)
    except (TypeError, ValueError):
        return float("nan")


def pct(sorted_values, p):
    if not sorted_values:
        return float("nan")
    rank = max(1, int(math.ceil(p * len(sorted_values))))
    return sorted_values[rank - 1]


def stats(values):
    v = sorted(x for x in values if not math.isnan(x))
    if not v:
        return {"count": 0, "min": float("nan"), "mean": float("nan"),
                "p50": float("nan"), "p95": float("nan"), "max": float("nan")}
    return {"count": len(v), "min": v[0], "mean": sum(v) / len(v),
            "p50": pct(v, 0.50), "p95": pct(v, 0.95), "max": v[-1]}


def fmt(x, metric=""):
    if x is None or (isinstance(x, float) and math.isnan(x)):
        return "-"
    if metric.endswith("_bytes") or metric.endswith("_count") or metric == "bytes":
        return "{:,.0f}".format(x)
    if abs(x) >= 100:
        return "{:,.0f}".format(x)
    return "{:.2f}".format(x)


class Verdicts:
    def __init__(self):
        self.items = []   # (scope, name, ok, detail)

    def add(self, scope, name, ok, detail=""):
        self.items.append((scope, name, bool(ok), detail))

    def ok(self):
        return all(i[2] for i in self.items)


def metrics_map(rows):
    out = {}
    for r in rows or []:
        out[r.get("metric", "")] = r
    return out


def check_rows(v, scope, label, rows):
    if rows is None:
        v.add(scope, label + " present", False, "file missing")
        return
    if not rows:
        v.add(scope, label + " present", False, "no rows")
        return
    failed = [r for r in rows if r.get("result") != "pass"]
    v.add(scope, label + " all pass", not failed,
          "{} rows, {} failed{}".format(
              len(rows), len(failed),
              (": " + "; ".join("{} ({})".format(r.get("check"), r.get("detail", ""))
                                for r in failed[:5])) if failed else ""))


def exit_code_verdicts(v, scope, codes, names):
    if not codes:
        v.add(scope, "exit codes recorded", False, "exit_codes.txt missing")
        return
    for n in names:
        v.add(scope, n + " exit 0", codes.get(n) == "0", "exit " + codes.get(n, "missing"))


def summarize_tier(root, name, min_steps):
    d = os.path.join(root, name)
    scope = name
    v = Verdicts()
    meta = read_kv(os.path.join(d, "meta.txt"))
    codes = read_kv(os.path.join(d, "exit_codes.txt"))
    units = int(meta.get("units", re.sub(r"\D", "", name) or 0))
    exit_code_verdicts(v, scope, codes, ("a", "b", "client"))

    steps = read_csv(os.path.join(d, "a_steps.csv"))
    tick_info = {}
    if steps is None:
        v.add(scope, "a_steps.csv present", False, "file missing")
        steps = []
    else:
        ticks = [int(num(r.get("tick")) if not math.isnan(num(r.get("tick"))) else -1) for r in steps]
        gaps = sum(1 for a, b in zip(ticks, ticks[1:]) if b != a + 1)
        bad = sum(1 for r in steps if r.get("check_ok") != "1")
        v.add(scope, "steps >= {}".format(min_steps), len(steps) >= min_steps,
              "{} steps".format(len(steps)))
        v.add(scope, "ticks consecutive", gaps == 0 and len(ticks) > 0,
              "first {} last {} gaps {}".format(ticks[0] if ticks else "-",
                                                 ticks[-1] if ticks else "-", gaps))
        v.add(scope, "step check_ok all 1", bad == 0 and len(steps) > 0,
              "{} rows with check_ok != 1".format(bad))
        tick_info = {"steps": len(steps), "first_tick": ticks[0] if ticks else None,
                     "last_tick": ticks[-1] if ticks else None}

    a_checks = read_csv(os.path.join(d, "a_checks.csv"))
    c_checks = read_csv(os.path.join(d, "client_checks.csv"))
    check_rows(v, scope, "a_checks", a_checks)
    if a_checks is not None:
        names = set(r.get("check") for r in a_checks)
        absent = [n for n in A_CHECK_NAMES if n not in names]
        v.add(scope, "a_checks has every fixed row", not absent,
              "missing: " + ", ".join(absent) if absent else ", ".join(A_CHECK_NAMES))
    check_rows(v, scope, "client_checks", c_checks)

    a_snap = read_csv(os.path.join(d, "a_snapshots.csv")) or []
    c_snap = read_csv(os.path.join(d, "client_snapshots.csv")) or []
    a_by = {r["request_id"]: r for r in a_snap}
    c_by = {r["request_id"]: r for r in c_snap}
    dup_a = len(a_snap) - len(a_by)
    dup_c = len(c_snap) - len(c_by)
    matched = mismatched = 0
    mismatch_examples = []
    for rid, ar in a_by.items():
        cr = c_by.get(rid)
        if cr is None:
            continue
        diffs = [k for k in ("tick", "category", "visible", "wire_bytes", "digest")
                 if ar.get(k) != cr.get(k)]
        if diffs:
            mismatched += 1
            if len(mismatch_examples) < 5:
                mismatch_examples.append("{}: {}".format(
                    rid, ", ".join("{} A={} C={}".format(k, ar.get(k), cr.get(k)) for k in diffs)))
        else:
            matched += 1
    a_only = len(set(a_by) - set(c_by))
    c_only = len(set(c_by) - set(a_by))
    digest = {"a_rows": len(a_snap), "client_rows": len(c_snap), "matched": matched,
              "mismatched": mismatched, "a_only": a_only, "client_only": c_only,
              "dup_a": dup_a, "dup_client": dup_c}
    v.add(scope, "A and client snapshot logs agree (digest per request id)",
          matched > 0 and mismatched == 0 and a_only == 0 and c_only == 0
          and dup_a == 0 and dup_c == 0,
          "matched {} mismatched {} A-only {} client-only {} dup A/C {}/{}{}".format(
              matched, mismatched, a_only, c_only, dup_a, dup_c,
              ("; " + "; ".join(mismatch_examples)) if mismatch_examples else ""))
    for side, rows in (("A", a_snap), ("client", c_snap)):
        bad = sum(1 for r in rows if r.get("check_ok") != "1")
        v.add(scope, side + " snapshot check_ok all 1", rows and bad == 0,
              "{} rows, {} with check_ok != 1".format(len(rows), bad))

    per_cat = {}
    for cat in CATEGORIES:
        ar = [r for r in a_snap if r.get("category") == cat]
        cr = [r for r in c_snap if r.get("category") == cat]
        vis = [num(r.get("visible")) for r in cr] or [num(r.get("visible")) for r in ar]
        per_cat[cat] = {
            "a_count": len(ar), "client_count": len(cr),
            "visible": stats(vis),
            "visible_zero": sum(1 for x in vis if x == 0),
            "wire_bytes": stats([num(r.get("wire_bytes")) for r in cr] or
                                [num(r.get("wire_bytes")) for r in ar]),
            "a.req_load_us": stats([num(r.get("req_load_us")) for r in ar]),
            "a.filter_us": stats([num(r.get("filter_us")) for r in ar]),
            "a.build_us": stats([num(r.get("build_us")) for r in ar]),
            "a.save_us": stats([num(r.get("save_us")) for r in ar]),
            "a.destroy_us": stats([num(r.get("destroy_us")) for r in ar]),
            "c.rtt_us": stats([num(r.get("rtt_us")) for r in cr]),
            "c.load_us": stats([num(r.get("load_us")) for r in cr]),
            "c.read_present_us": stats([num(r.get("read_present_us")) for r in cr]),
            "c.destroy_us": stats([num(r.get("destroy_us")) for r in cr]),
        }
        v.add(scope, "category {} sampled".format(cat), len(ar) > 0 and len(cr) > 0,
              "A {} client {}".format(len(ar), len(cr)))
    empty_nonzero = [r for r in a_snap + c_snap
                     if r.get("category") == "empty" and r.get("visible") != "0"]
    v.add(scope, "empty snapshots have 0 units", not empty_nonzero,
          "{} empty rows with visible != 0".format(len(empty_nonzero)))
    full_wrong = [r for r in a_snap + c_snap
                  if r.get("category") == "full" and num(r.get("visible")) != units]
    v.add(scope, "full-scene snapshots have every unit", not full_wrong,
          "{} full rows with visible != {}".format(len(full_wrong), units))

    metrics = {**metrics_map(read_csv(os.path.join(d, "a_metrics.csv"))),
               **metrics_map(read_csv(os.path.join(d, "b_metrics.csv"))),
               **metrics_map(read_csv(os.path.join(d, "client_metrics.csv")))}
    missing = [k for k in REQUIRED_METRICS if k not in metrics]
    v.add(scope, "every metric of INTERFACES.md section 6 recorded", not missing,
          "missing: " + ", ".join(missing) if missing else "{} metrics".format(len(REQUIRED_METRICS)))
    ucount = metrics.get("a.units_count")
    if ucount is not None:
        v.add(scope, "a.units_count == units", num(ucount.get("max")) == units,
              "max {}".format(ucount.get("max")))

    return {
        "name": name, "units": units, "meta": meta, "exit_codes": codes,
        "ticks": tick_info,
        "steps_stats": {k: stats([num(r.get(k)) for r in steps]) for k in
                        ("req_bytes", "res_bytes", "ab_rtt_us", "save_us", "load_us",
                         "destroy_us", "free_blocks", "free_bytes")},
        "metrics": metrics,
        "checks": {"a": a_checks or [], "client": c_checks or []},
        "digest": digest, "per_category": per_cat, "verdicts": v,
    }


def metric_table(lines, metrics, wanted):
    lines.append("| metric | meaning | count | mean | p50 | p95 | max |")
    lines.append("| --- | --- | ---: | ---: | ---: | ---: | ---: |")
    for key, meaning in wanted:
        r = metrics.get(key)
        if r is None:
            lines.append("| `{}` | {} | missing | | | | |".format(key, meaning))
            continue
        lines.append("| `{}` | {} | {} | {} | {} | {} | {} |".format(
            key, meaning, r.get("count", "-"),
            *(fmt(num(r.get(c)), key) for c in ("mean", "p50", "p95", "max"))))


def mean_p95(s, metric=""):
    if not s or s["count"] == 0:
        return "-"
    return "{} / {}".format(fmt(s["mean"], metric), fmt(s["p95"], metric))


def render_tier(lines, t):
    lines.append("## Tier: {} unit(s)".format(t["units"]))
    lines.append("")
    m = t["metrics"]
    tps = m.get("a.ticks_per_s")
    ti = t["ticks"]
    lines.append("- steps completed: {} (ticks {} .. {}); actual ticks/s: {} (target 20)".format(
        ti.get("steps", "-"), ti.get("first_tick", "-"), ti.get("last_tick", "-"),
        fmt(num(tps.get("mean"))) if tps else "missing"))
    lines.append("- exit codes: " + ", ".join("{}={}".format(k, v) for k, v in t["exit_codes"].items()))
    lines.append("- load average before / after: {} / {}".format(
        t["meta"].get("load_before", "-"), t["meta"].get("load_after", "-")))
    lines.append("")
    lines.append("### A <-> B full batch round trip (per step)")
    lines.append("")
    metric_table(lines, m, BATCH_METRICS)
    ss = t["steps_stats"]
    lines.append("")
    lines.append("Free blocks carried in the batch (a_steps.csv): blocks mean/max {} / {}, bytes mean/max {} / {}.".format(
        fmt(ss["free_blocks"]["mean"]), fmt(ss["free_blocks"]["max"], "_count"),
        fmt(ss["free_bytes"]["mean"], "_bytes"), fmt(ss["free_bytes"]["max"], "_bytes")))
    lines.append("")
    lines.append("### View snapshots by category (per-request rows; mean / p95)")
    lines.append("")
    pc = t["per_category"]
    lines.append("| | " + " | ".join(CATEGORIES) + " |")
    lines.append("| --- | " + " | ".join("---:" for _ in CATEGORIES) + " |")

    def row(label, f):
        lines.append("| {} | {} |".format(label, " | ".join(f(pc[c]) for c in CATEGORIES)))

    row("requests (A / client)", lambda c: "{} / {}".format(c["a_count"], c["client_count"]))
    row("visible units min / mean / max", lambda c: "{} / {} / {}".format(
        fmt(c["visible"]["min"], "_count"), fmt(c["visible"]["mean"]), fmt(c["visible"]["max"], "_count")))
    row("snapshots with 0 visible", lambda c: str(c["visible_zero"]))
    row("wire bytes mean / p95 / max", lambda c: "{} / {} / {}".format(
        fmt(c["wire_bytes"]["mean"], "_bytes"), fmt(c["wire_bytes"]["p95"], "_bytes"),
        fmt(c["wire_bytes"]["max"], "_bytes")))
    row("A load_verified request (us)", lambda c: mean_p95(c["a.req_load_us"]))
    row("A filter (us)", lambda c: mean_p95(c["a.filter_us"]))
    row("A build (us)", lambda c: mean_p95(c["a.build_us"]))
    row("A save_verified (us)", lambda c: mean_p95(c["a.save_us"]))
    row("A destroy (us)", lambda c: mean_p95(c["a.destroy_us"]))
    row("client round trip (us)", lambda c: mean_p95(c["c.rtt_us"]))
    row("client load_verified (us)", lambda c: mean_p95(c["c.load_us"]))
    row("client read + present data, MERGED (us)", lambda c: mean_p95(c["c.read_present_us"]))
    row("client destroy (us)", lambda c: mean_p95(c["c.destroy_us"]))
    lines.append("")
    lines.append("### View path, whole run (process metrics)")
    lines.append("")
    metric_table(lines, m, VIEW_METRICS)
    lines.append("")
    dg = t["digest"]
    lines.append("### A / client snapshot digest comparison")
    lines.append("")
    lines.append("A rows {a_rows}, client rows {client_rows}: matched {matched}, mismatched {mismatched}, "
                 "A-only {a_only}, client-only {client_only}, duplicate ids A/client {dup_a}/{dup_client}.".format(**dg))
    lines.append("")
    lines.append("### Checks reported by the processes")
    lines.append("")
    lines.append("| process | check | result | detail |")
    lines.append("| --- | --- | --- | --- |")
    for side in ("a", "client"):
        for r in t["checks"][side]:
            lines.append("| {} | {} | {} | {} |".format(side, r.get("check", ""), r.get("result", ""),
                                                      (r.get("detail") or "").replace("|", "/")))
    lines.append("")


def render_overview(lines, tiers):
    lines.append("## Three tiers side by side (mean / p95)")
    lines.append("")
    lines.append("| | " + " | ".join("{} unit(s)".format(t["units"]) for t in tiers) + " |")
    lines.append("| --- | " + " | ".join("---:" for _ in tiers) + " |")

    def mrow(label, key):
        cells = []
        for t in tiers:
            r = t["metrics"].get(key)
            cells.append("{} / {}".format(fmt(num(r.get("mean")), key), fmt(num(r.get("p95")), key)) if r else "-")
        lines.append("| {} | {} |".format(label, " | ".join(cells)))

    def crow(label, cat, key, metric=""):
        lines.append("| {} | {} |".format(label, " | ".join(
            mean_p95(t["per_category"][cat][key], metric) for t in tiers)))

    lines.append("| steps / actual ticks per s | {} |".format(" | ".join(
        "{} / {}".format(t["ticks"].get("steps", "-"),
                         fmt(num(t["metrics"].get("a.ticks_per_s", {}).get("mean")))) for t in tiers)))
    mrow("batch wire A->B (bytes)", "a.step_req_bytes")
    mrow("batch wire B->A (bytes)", "a.step_res_bytes")
    mrow("A<->B round trip (us)", "a.ab_rtt_us")
    mrow("A save_verified (us)", "a.save_us")
    mrow("A load_verified (us)", "a.load_us")
    mrow("A destroy (us)", "a.destroy_us")
    mrow("B load_verified (us)", "b.load_us")
    mrow("B step in place (us)", "b.modify_us")
    mrow("B save_verified (us)", "b.save_us")
    mrow("B destroy (us)", "b.destroy_us")
    for cat in CATEGORIES:
        crow("{} snapshot wire (bytes)".format(cat), cat, "wire_bytes", "_bytes")
        crow("{} snapshot visible units".format(cat), cat, "visible", "_count")
        crow("{} view round trip (us)".format(cat), cat, "c.rtt_us")
        crow("{} A snapshot build (us)".format(cat), cat, "a.build_us")
        crow("{} client load_verified (us)".format(cat), cat, "c.load_us")
        crow("{} client read + present, MERGED (us)".format(cat), cat, "c.read_present_us")
    lines.append("")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("root", help="OUT_ROOT of run_all_tiers.sh")
    ap.add_argument("--min-steps", type=int, default=500)
    ap.add_argument("--no-evil", action="store_true", help="evil run was skipped")
    ap.add_argument("--label", default="", help="free text, e.g. machine and compiler")
    ap.add_argument("--out", help="Markdown report path (default: stdout)")
    ap.add_argument("--json", help="JSON summary path")
    args = ap.parse_args()

    root = args.root
    tier_dirs = sorted((n for n in os.listdir(root)
                        if n.startswith("tier_") and os.path.isdir(os.path.join(root, n))),
                       key=lambda n: int(re.sub(r"\D", "", n) or 0))
    overall = Verdicts()
    if not tier_dirs:
        overall.add("tiers", "at least one tier", False, "no tier_* directories")
    tiers = [summarize_tier(root, n, args.min_steps) for n in tier_dirs]

    st_codes = read_kv(os.path.join(root, "selftest", "exit_codes.txt"))
    exit_code_verdicts(overall, "selftest", st_codes, ("selftest",))
    st_rows = read_csv(os.path.join(root, "selftest", "selftest_checks.csv"))
    check_rows(overall, "selftest", "selftest_checks", st_rows)

    if not args.no_evil:
        ev = read_kv(os.path.join(root, "evil", "exit_codes.txt"))
        exit_code_verdicts(overall, "evil", ev, ("a", "b", "client"))
        rej = int(num(ev.get("a_rejected_lines")) if not math.isnan(num(ev.get("a_rejected_lines"))) else 0)
        overall.add("evil", "A logged >= 3 rejected requests", rej >= 3, "{} line(s)".format(rej))
        check_rows(overall, "evil", "evil client_checks",
                   read_csv(os.path.join(root, "evil", "client_checks.csv")))

    all_items = list(overall.items)
    for t in tiers:
        all_items.extend(t["verdicts"].items)
    passed = all(i[2] for i in all_items)

    lines = ["# Map state demo: tier results", ""]
    meta0 = tiers[0]["meta"] if tiers else {}
    lines.append("- label: {}".format(args.label or "-"))
    lines.append("- host: {}; commit: {}; seed: {}; steps per tier: {}".format(
        meta0.get("host", "-"), meta0.get("commit", "-"), meta0.get("seed", "-"), meta0.get("steps", "-")))
    lines.append("- overall: **{}** ({} verdicts, {} failed)".format(
        "PASS" if passed else "FAIL", len(all_items), sum(1 for i in all_items if not i[2])))
    lines.append("- percentiles: process metrics as the processes computed them; per-category rows here, nearest rank")
    lines.append("")
    lines.append("## Verdicts")
    lines.append("")
    lines.append("| scope | verdict | result | detail |")
    lines.append("| --- | --- | --- | --- |")
    for scope, name, ok, detail in all_items:
        lines.append("| {} | {} | {} | {} |".format(scope, name, "pass" if ok else "**FAIL**",
                                                   detail.replace("|", "/")))
    lines.append("")
    if st_rows:
        lines.append("## Self-test checks")
        lines.append("")
        lines.append("| check | result | detail |")
        lines.append("| --- | --- | --- |")
        for r in st_rows:
            lines.append("| {} | {} | {} |".format(r.get("check", ""), r.get("result", ""),
                                                 (r.get("detail") or "").replace("|", "/")))
        lines.append("")
    if tiers:
        render_overview(lines, tiers)
    for t in tiers:
        render_tier(lines, t)

    text = "\n".join(lines) + "\n"
    if args.out:
        with open(args.out, "w") as f:
            f.write(text)
    else:
        sys.stdout.write(text)
    if args.json:
        js = {"label": args.label, "passed": passed,
              "verdicts": [{"scope": s, "name": n, "ok": o, "detail": d} for s, n, o, d in all_items],
              "tiers": [{k: v for k, v in t.items() if k != "verdicts"} for t in tiers]}
        with open(args.json, "w") as f:
            json.dump(js, f, indent=1, sort_keys=True, default=str)
    for scope, name, ok, detail in all_items:
        if not ok:
            print("FAIL {}: {} ({})".format(scope, name, detail), file=sys.stderr)
    print("summarize: {} ({} verdicts)".format("PASS" if passed else "FAIL", len(all_items)), file=sys.stderr)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
