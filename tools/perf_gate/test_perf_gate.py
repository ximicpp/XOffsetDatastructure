#!/usr/bin/env python3
"""Self-test of perf_gate.py's comparison logic on synthetic counts (no compiler, no valgrind)."""
import argparse
import copy
import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import perf_gate as pg  # noqa: E402

A, B, H = "a" * 40, "b" * 40, "c" * 40

BASE_SLG = {"A.apply": 500_000, "A.simulate": 2_000_000, "A.res_encode": 100_000, "A.res_decode": 2_400_000,
            "B.season_edits": 4_000_000, "B.in_decode": 5_000_000, "C.ticks": 2_000_000, "C.restore": 1_000_000,
            "D.read": 1_000_000, "S.ticks_middle": 12_000_000}
BASE_FORMAL = {"load": 52_000_000, "query_before": 29_000_000, "mutate": 43_000_000, "emit_after": 3_000_000,
               "retire": 4_000, "reload": 73_000_000, "query_after": 31_000_000, "cleanup": 2_000}
CHECKS = {"ok": True, "wire_M_sha256": "2bf0c071" + "0" * 56}


def tree(slg=None, formal=None, checks=None):
    return {"checks": copy.deepcopy(checks or CHECKS), "slg_keys": sorted(slg or BASE_SLG),
            "slg": dict(slg or BASE_SLG), "formal": dict(formal or BASE_FORMAL)}


def run_case(head_tree, anchor_tree=None, ledger_head=(), ledger_base=(), determinism=None, anchor_status="ok"):
    with tempfile.TemporaryDirectory() as d:
        w = Path(d)
        trees = {A: anchor_tree or tree(), B: tree(), H: head_tree}
        pg.dump_json(w / "plan.json", {"roles": {"anchor": A, "base": B, "head": H}, "trees": {},
                                       "ledger_head": {"entries": list(ledger_head)},
                                       "ledger_base": {"entries": list(ledger_base)}})
        pg.dump_json(w / "build-gcc16.json", {"compiler": "gcc16", "compiler_version": "test",
                                              "trees": {A: {"status": anchor_status}, B: {"status": "ok"}, H: {"status": "ok"}}})
        pg.dump_json(w / "ir-gcc16.json", {"compiler": "gcc16", "compiler_version": "test", "valgrind": "test",
                                           "fixture_input_digest": {}, "trees": trees, "errors": [],
                                           "determinism": determinism or {}})
        ns = argparse.Namespace(work=str(w), compiler="gcc16", json=str(w / "out.json"), summary=str(w / "summary.md"))
        rc = pg.cmd_compare(ns)
        out = json.loads((w / "out.json").read_text())
        return rc, out


def row(out, metric):
    return next(r for r in out["rows"] if r["metric"] == metric)


def entry(**kw):
    e = {"id": "L1", "pr": 99, "metric": "formal.mutate", "compiler": "*", "max_ratio": 1.03,
         "reason": "test", "evidence": "test", "approved_by": "性能优化"}
    e.update(kw)
    return e


def main():
    failures = []

    def expect(name, cond):
        print(("ok   " if cond else "FAIL ") + name)
        if not cond:
            failures.append(name)

    rc, out = run_case(tree())
    expect("identical counts pass", rc == 0 and out["verdict"] == "pass")

    f = dict(BASE_FORMAL, mutate=int(43_000_000 * 1.02))
    rc, out = run_case(tree(formal=f))
    expect("formal.mutate +2% fails", rc == 1 and row(out, "formal.mutate")["verdict"] == "fail")

    f = dict(BASE_FORMAL, mutate=int(43_000_000 * 1.005))
    rc, out = run_case(tree(formal=f))
    expect("formal.mutate +0.5% warns", rc == 0 and out["verdict"] == "warn")

    f = dict(BASE_FORMAL, mutate=int(43_000_000 * 1.015))
    rc, out = run_case(tree(formal=f), ledger_head=[entry(max_ratio=1.02)])
    expect("new ledger entry allows +1.5%", rc == 0 and row(out, "formal.mutate")["verdict"] == "allowed")

    rc, out = run_case(tree(formal=f), ledger_head=[entry(max_ratio=1.02)], ledger_base=[entry(max_ratio=1.02)])
    expect("an old ledger entry does not allow a new head/base regression", rc == 1)

    rc, out = run_case(tree(formal=f), ledger_head=[entry(max_ratio=1.012)])
    expect("regression above the ledger cap fails", rc == 1)

    rc, out = run_case(tree(), ledger_head=[entry(max_ratio=1.03)])
    expect("scenario-level entry above 2% without susu fails", rc == 1 and any("susu" in x for x in out["failures"]))

    rc, out = run_case(tree(), ledger_head=[entry(max_ratio=1.03, approved_by="susu (card)")])
    expect("scenario-level entry above 2% approved by susu is accepted", rc == 0)

    s = dict(BASE_SLG, **{"A.res_encode": 180_000})
    rc, out = run_case(tree(slg=s))
    expect("small phase +80% is report-only but the A total fails", row(out, "slg.A.res_encode")["class"] == "report"
           and row(out, "slg.A")["verdict"] == "fail")

    s = dict(BASE_SLG, **{"B.season_edits": int(4_000_000 * 1.03), "B.in_decode": int(5_000_000 * 0.976)})
    rc, out = run_case(tree(slg=s))
    expect("big phase +3% warns with an unchanged total", rc == 0 and row(out, "slg.B.season_edits")["verdict"] == "warn")

    s = dict(BASE_SLG, **{"S.ticks_middle": int(12_000_000 * 1.06)})
    rc, out = run_case(tree(slg=s))
    expect("S.ticks_middle +6% fails as a phase", rc == 1 and row(out, "slg.S.ticks_middle")["verdict"] == "fail")

    anchor = tree(formal=dict(BASE_FORMAL, mutate=int(43_000_000 / 1.025)))
    rc, out = run_case(tree(), anchor_tree=anchor)
    expect("head/anchor +2.5% fails although head/base is flat", rc == 1 and row(out, "formal.mutate")["verdict"] == "fail")

    rc, out = run_case(tree(), anchor_tree=anchor, ledger_head=[entry(max_ratio=1.02)])
    expect("head/anchor net of the ledger passes", rc == 0 and row(out, "formal.mutate")["head_anchor_net"] < 1.01)

    rc, out = run_case(tree(), determinism={"formal.mutate": {"first": 1, "second": 2, "identical": False}})
    expect("non-deterministic counts fail", rc == 1)

    rc, out = run_case(tree(), anchor_status="incomparable")
    expect("incomparable anchor warns", rc == 0 and out["verdict"] == "warn" and out["status"]["anchor"] == "incomparable")

    bad = tree(checks={"ok": False})
    rc, out = run_case(bad)
    expect("failed head checks fail", rc == 1)

    rc, out = run_case(tree(checks={"ok": True, "wire_M_sha256": "ffff" + "0" * 60}))
    expect("changed input wire is flagged", any("wire" in x for x in out["warnings"]))

    f = dict(BASE_FORMAL, load=int(52_000_000 * 0.95))
    rc, out = run_case(tree(formal=f))
    expect("an Ir drop below 0.97 gets the note", rc == 0 and any("指令数下降" in x for x in out["notes"]))

    print(f"{len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
