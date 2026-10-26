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


BASE_RELOC = {"vec.push_back": 300_000, "map.insert.fresh": 900_000}


def tree(slg=None, formal=None, checks=None, functions=None, reloc=None, reloc_status="ok"):
    t = {"checks": copy.deepcopy(checks or CHECKS), "slg_keys": sorted(slg or BASE_SLG),
         "slg": dict(slg or BASE_SLG), "formal": dict(formal or BASE_FORMAL),
         "reloc_status": reloc_status, "reloc": dict(BASE_RELOC if reloc is None else reloc)}
    if functions is not None:
        t["functions"] = functions
    return t


def prof(self_ir, edges=()):
    return {"self": dict(self_ir), "edges": [list(e) for e in edges]}


CG_SAMPLE = """# callgrind format
version: 1
positions: line
events: Ir
summary: 1000

ob=(1) /bin/x
fl=(1) a.cpp
fn=(1) outer(int)
10 100
+1 50
cfl=(1)
cfn=(2) helper(int) [clone .constprop.0]
calls=3 20
* 600
fn=(2)
20 400
fi=(2) b.hpp
+2 200
fe=(1)
fn=(3) helper(int)
5 0
fn=(4) leaf()
0x1f 250
"""


def run_case(head_tree, anchor_tree=None, ledger_head=(), ledger_base=(), determinism=None, anchor_status="ok",
             base_tree=None):
    with tempfile.TemporaryDirectory() as d:
        w = Path(d)
        trees = {A: anchor_tree or base_tree or tree(), B: base_tree or tree(), H: head_tree}
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

    rc, out = run_case(tree(formal=f), ledger_head=[entry(max_ratio=1.02, record_only=True)])
    expect("a record-only ledger entry allows nothing", rc == 1 and row(out, "formal.mutate")["verdict"] == "fail")

    rc, out = run_case(tree(), ledger_head=[entry(max_ratio=1.06, record_only=True)])
    expect("a record-only scenario entry above 2% needs no susu", rc == 0)

    expect("callgrind runs with glibc's rep-movsb path off", "x86_rep_movsb_threshold" in pg.IR_ENV["GLIBC_TUNABLES"])
    expect("callgrind runs with glibc's rep-stosb path off", "x86_rep_stosb_threshold" in pg.IR_ENV["GLIBC_TUNABLES"])

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

    with tempfile.TemporaryDirectory() as d:
        cg = Path(d) / "cg.out"
        cg.write_text(CG_SAMPLE)
        p = pg.fn_profile(cg)
    expect("fn_profile: self Ir per function, clones merged, call costs excluded",
           p["self"] == {"outer(int)": 150, "helper(int)": 600, "leaf()": 250}
           and p["edges"] == [["outer(int)", "helper(int)"]])

    # D.read: helper gets inlined into caller in base, called out of line in head.
    keys = {k: prof({"f": 10}) for k in BASE_SLG}
    keys.update({f"formal.{k}": prof({"g": 10}) for k in BASE_FORMAL})
    fb = dict(keys, **{"slg.D.read": prof({"caller": 900_000, "other": 100_000})})
    fh = dict(keys, **{"slg.D.read": prof({"caller": 380_000, "helper": 550_000, "other": 102_000},
                                          [("caller", "helper")])})
    s = dict(BASE_SLG, **{"D.read": 1_032_000})
    rc, out = run_case(tree(slg=s, functions=fh), base_tree=tree(functions=fb))
    d = row(out, "slg.D").get("functions") or {}
    expect("inlining change: function only on one side is flagged",
           rc == 1 and d.get("inline_suspect") and any("helper" in x and "只在 head" in x for x in d["suspects"])
           and d["top"][0]["function"] == "helper" and any("疑似内联" in x for x in out["notes"]))
    expect("inlining change: opposite caller/callee pair is flagged",
           any("caller" in x and "一升一降" in x for x in d["suspects"]))

    fh = dict(keys, **{"slg.D.read": prof({"caller": 932_000, "other": 100_000})})
    rc, out = run_case(tree(slg=s, functions=fh), base_tree=tree(functions=fb))
    d = row(out, "slg.D").get("functions") or {}
    expect("a plain regression in one function is not flagged as inlining",
           rc == 1 and d.get("top") and d["top"][0]["delta"] == 32_000 and not d["inline_suspect"])
    expect("passing metrics get no function table", "functions" not in row(out, "slg.A"))

    fh = dict(keys, **{"slg.D.read": prof({"caller": 900_000, "zero_holes": 32_000, "other": 100_000},
                                          [("caller", "zero_holes")])})
    rc, out = run_case(tree(slg=s, functions=fh), base_tree=tree(functions=fb))
    d = row(out, "slg.D")["functions"]
    expect("new code with no partner moving the other way is not inlining",
           d["top"][0]["only"] == "head" and not d["inline_suspect"])

    fb2 = dict(keys, **{"slg.D.read": prof({"write(int)": 900_000, "other": 100_000})})
    fh = dict(keys, **{"slg.D.read": prof({"write(int, bool)": 932_000, "other": 100_000})})
    rc, out = run_case(tree(slg=s, functions=fh), base_tree=tree(functions=fb2))
    expect("a signature change is not inlining", not row(out, "slg.D")["functions"]["inline_suspect"])

    # B.in_decode: calls to malloc/free replaced by recycling the free table in place (D2, #33).
    rec = "XOffsetDatastructure::XBufferCore::FreeTableRecycler::recycle(XOffsetDatastructure::XBufferCore::FreeTable&)"
    lfb = "XOffsetDatastructure::XBufferCore::load_free_blocks(void const*, unsigned long)"
    fb3 = dict(keys, **{"slg.B.in_decode": prof({"free": 128_000, "malloc": 63_000, "operator new(unsigned long)": 21_000,
                                                 lfb: 188_000, "other": 4_600_000})})
    fh = dict(keys, **{"slg.B.in_decode": prof({rec: 904_000, "malloc": 300, lfb: 107_000, "other": 4_600_000})})
    s3 = dict(BASE_SLG, **{"B.in_decode": 5_000_000 + 531_000})
    rc, out = run_case(tree(slg=s3, functions=fh), base_tree=tree(functions=fb3))
    d = row(out, "slg.B.in_decode")["functions"]
    expect("allocator calls replaced by recycling are flagged, verdict unchanged",
           rc == 1 and row(out, "slg.B.in_decode")["verdict"] == "fail" and d["alloc_recycle"]
           and d["alloc_recycle"]["kind"] == "alloc_calls_removed" and d["alloc_recycle"]["alloc"] == -211_700
           and any("分配器回收型" in x and "D2 型" in x for x in out["notes"]))
    summary = pg.render_summary(out, {}, {})
    expect("the function table header carries the recycle flag", "分配器回收型（只是诊断，不放行）" in summary)

    # The same recycle growth without fewer allocator calls: more bins to recycle (COPS).
    fb3 = dict(keys, **{"slg.B.in_decode": prof({rec: 400_000, "malloc": 63_000, "other": 4_537_000})})
    fh = dict(keys, **{"slg.B.in_decode": prof({rec: 900_000, "malloc": 63_000, "other": 4_568_000})})
    rc, out = run_case(tree(slg=s3, functions=fh), base_tree=tree(functions=fb3))
    d = row(out, "slg.B.in_decode")["functions"]
    expect("more recycle work with the same allocator calls is flagged as the COPS kind",
           d["alloc_recycle"] and d["alloc_recycle"]["kind"] == "more_recycle_work"
           and any("COPS 型" in x for x in out["notes"]))

    # A noise-level allocator drop next to the recycle growth (COPS slg.B: -1,730 against +132,242) is still COPS.
    fh = dict(keys, **{"slg.B.in_decode": prof({rec: 900_000, "malloc": 56_500, "other": 4_574_500})})
    rc, out = run_case(tree(slg=s3, functions=fh), base_tree=tree(functions=fb3))
    d = row(out, "slg.B.in_decode")["functions"]
    expect("an allocator drop below a tenth of the recycle growth stays the COPS kind",
           d["alloc_recycle"] and d["alloc_recycle"]["alloc"] == -6_500
           and d["alloc_recycle"]["kind"] == "more_recycle_work")

    fh = dict(keys, **{"slg.B.in_decode": prof({rec: 500_000, "malloc": 63_000, "other": 5_068_000})})
    rc, out = run_case(tree(slg=s3, functions=fh), base_tree=tree(functions=fb3))
    expect("recycle growth below half of the increase is not flagged",
           row(out, "slg.B.in_decode")["functions"]["alloc_recycle"] is None
           and not any("分配器回收型" in x for x in out["notes"]))

    r = dict(BASE_RELOC, **{"map.insert.fresh": int(900_000 * 1.06)})
    rc, out = run_case(tree(reloc=r))
    expect("reloc case +6% fails as a phase", rc == 1 and row(out, "reloc.map.insert.fresh")["verdict"] == "fail"
           and row(out, "reloc.map.insert.fresh")["class"] == "phase")

    r = dict(BASE_RELOC, **{"vec.push_back": int(300_000 * 1.03)})
    rc, out = run_case(tree(reloc=r))
    expect("reloc case +3% warns", rc == 0 and row(out, "reloc.vec.push_back")["verdict"] == "warn")

    rc, out = run_case(tree(), base_tree=tree(reloc={}, reloc_status="absent"))
    expect("reloc cases new in head are not judged", rc == 0 and row(out, "reloc.vec.push_back")["verdict"] == "new")

    rc, out = run_case(tree(reloc={}, reloc_status="verify_failed"))
    expect("head reloc verify failure fails", rc == 1 and any("reloc_bench" in x for x in out["failures"]))

    rc, out = run_case(tree(), base_tree=tree(reloc={}, reloc_status="failed"))
    expect("base that cannot build reloc_bench warns", rc == 0 and any("reloc_bench" in x for x in out["warnings"]))

    print(f"{len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
