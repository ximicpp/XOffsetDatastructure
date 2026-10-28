#!/usr/bin/env python3
"""Class-0 evidence: are two builds of the benchmark programs the same machine code?

MERGE_RULES_V2 class 0 (2026-10-26): under cloud GCC 16 and P2996 d77eff1, every function of
the formal harness (sc_xoffset), bench/slg (slg_bench) and bench/reloc (reloc_bench) must have
the same instructions before and after the change, ignoring addresses and symbol names. If the
order and sizes of the functions in .text are also the same, the binaries are laid out the same;
if only the placement changed it is still class 0, but the PR says so and the change becomes a
separate bisect point at the next checkpoint (INT1: map.insert had identical instructions, only
its alignment moved, and it was 6-8% slower on the Mac).

  asm_equal.py compare --base DIR --head DIR [--summary F.md] [--json F.json]
      DIR holds the three binaries (perf_gate.py build output: <work>/bin/<compiler>/<sha>).
  asm_equal.py gate --work DIR --compiler gcc16|p2996 [--summary ...]
      base and head of a perf_gate.py work directory that is already built (the Ir steps of
      tools/local_validate.sh leave one per compiler; its "asm" step runs this).
  asm_equal.py run --repo . --base REV --head REV --compiler gcc16|p2996 --work DIR
                   --cereal INC [--cxx CXX] [--extra-flags F] [--jobs N] [--summary ...]
      perf_gate.py prepare + build of base and head, then compare.

How functions are matched. Each function's instructions come from objdump, cut to the symbol's
size (alignment padding after it is placement, not code). Addresses are removed: a jump inside
the function becomes an offset from its start, a reference to another function becomes that
function's identity, a rip-relative or absolute reference to data becomes "data" (an absolute
value in a fixed-address executable is read as an address only when it is a function's start or
lies in a data section; anything else stays a number). A function's
identity is a hash of its normalized instructions in which the functions it references are
replaced by their own identities, refined over several rounds, so a renamed function still
matches and a call to a different function does not. The builds have the same machine code when
the two multisets of identities are equal; the layout is the same when, in addition, the
sequence (identity, offset from the start of .text, size) is equal.

Exit status: 0 when every program has the same machine code (layout may differ: see the
summary), 1 when some function differs, 2 on usage or tool errors.
"""
import argparse
import bisect
import collections
import difflib
import hashlib
import json
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

PROGRAMS = ("sc_xoffset", "slg_bench", "reloc_bench")
LABELS = {"sc_xoffset": "formal harness", "slg_bench": "bench/slg", "reloc_bench": "bench/reloc"}
ROUNDS = 4

SYM_LINE = re.compile(r"^\s*\d+:\s+([0-9a-f]+)\s+(\d+|0x[0-9a-f]+)\s+(\w+)\s+(\w+)\s+\w+\s+(\S+)\s+(.+)$")
INSN_LINE = re.compile(r"^\s*([0-9a-f]+):\s*(.*)$")
TARGET = re.compile(r"\b([0-9a-f]+) <([^>]+)>")
RIP = re.compile(r"-?0x[0-9a-f]+\(%rip\)")
HEX = re.compile(r"(?<![\w.])\$?(0x[0-9a-f]+)\b")


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw).stdout


class Binary:
    """Functions of one executable: address -> (names, size, section, normalized instructions)."""

    def __init__(self, path: Path, objdump="objdump", readelf="readelf"):
        self.path = path
        self.sections = self._sections(readelf)
        # Position-independent code reaches addresses only rip-relative; a fixed-address
        # executable (GCC -static) also has them as immediates and displacements.
        self.fixed_address = "EXEC" in run([readelf, "-hW", str(self.path)])
        self.funcs = {}       # start address -> {"names": [...], "size": n, "insns": [...]}
        self._symbols(readelf)
        self._disassemble(objdump)
        self.starts = sorted(self.funcs)

    def _sections(self, readelf):
        out = {}
        for line in run([readelf, "-SW", str(self.path)]).splitlines():
            m = re.match(r"^\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+\S+\s+(\S*)", line)
            if m and int(m.group(2), 16):
                out[m.group(1)] = {"addr": int(m.group(2), 16), "size": int(m.group(4), 16), "flags": m.group(5)}
        return out

    def _symbols(self, readelf):
        names = collections.defaultdict(set)
        sizes = {}
        for line in run([readelf, "-sW", str(self.path)]).splitlines():
            m = SYM_LINE.match(line)
            if not m or m.group(5) == "UND":
                continue
            addr = int(m.group(1), 16)
            size = int(m.group(2), 0)
            kind, name = m.group(3), m.group(6).split("@")[0]
            if kind in ("FUNC", "IFUNC") and addr:
                names[addr].add(name)
                sizes[addr] = max(sizes.get(addr, 0), size)
        for addr, ns in names.items():
            self.funcs[addr] = {"names": sorted(ns), "size": sizes[addr], "insns": []}

    def _disassemble(self, objdump):
        text = run([objdump, "-d", "-w", "--no-show-raw-insn", str(self.path)])
        starts = sorted(self.funcs)
        cur = None
        for line in text.splitlines():
            if line.endswith(">:"):
                addr = int(line.split()[0], 16)
                cur = addr if addr in self.funcs else None
                continue
            if cur is None:
                continue
            m = INSN_LINE.match(line)
            if not m:
                continue
            addr = int(m.group(1), 16)
            f = self.funcs[cur]
            if f["size"] and addr >= cur + f["size"]:
                continue
            f["insns"].append((addr, m.group(2).strip()))
        # Symbols without a size (hand-written assembly): drop the trailing padding.
        for addr in starts:
            f = self.funcs[addr]
            if not f["size"] and f["insns"]:
                while f["insns"] and re.match(r"^(nop|int3|cs nop|xchg\s+%ax,%ax|data16)", f["insns"][-1][1]):
                    f["insns"].pop()
                last = f["insns"][-1][0] if f["insns"] else addr
                f["size"] = last - addr + 1

    def section_of(self, addr):
        for name, s in self.sections.items():
            if s["addr"] <= addr < s["addr"] + s["size"]:
                return name
        return None

    def function_at(self, addr):
        """(start, offset) of the function containing `addr`, or None."""
        i = bisect.bisect_right(self.starts, addr) - 1
        if i >= 0:
            start = self.starts[i]
            if addr < start + max(self.funcs[start]["size"], 1):
                return start, addr - start
        return None

    def normalize(self):
        """Per function: instructions with addresses replaced by symbolic references."""
        for start, f in self.funcs.items():
            end = start + f["size"]
            out = []
            for addr, text in f["insns"]:
                body, _, comment = text.partition("#")
                body = body.strip()
                refs = []

                def ref(target, label=""):
                    if start <= target < end:
                        return f".+{target - start:#x}"
                    hit = self.function_at(target)
                    if hit:
                        refs.append(hit[0])
                        return f"F{hit[0]:x}" + (f"+{hit[1]:#x}" if hit[1] else "")
                    if label.endswith("@plt"):
                        return "<" + label + ">"
                    sec = self.section_of(target)
                    if sec is None:
                        return None
                    return f"data:{sec}" if "x" not in self.sections[sec]["flags"] else f"code:{sec}"

                # Direct branches and calls: "jmp 4011a0 <foo+0x10>".
                m = TARGET.search(body)
                if m:
                    r = ref(int(m.group(1), 16), m.group(2))
                    body = body[:m.start()] + (r or m.group(0)) + body[m.end():]
                # rip-relative operands: the comment names the target.
                if RIP.search(body):
                    m = TARGET.search(comment)
                    r = ref(int(m.group(1), 16), m.group(2)) if m else None
                    body = RIP.sub("[rip:" + (r or "?") + "]", body)
                elif self.fixed_address:
                    # Absolute addresses in non-PIE code (immediates and displacements).
                    # A constant can fall into the address range too (open flags, bit masks), so a
                    # value counts as an address only where code takes addresses: the start of a
                    # function, or anywhere in a data section. A value inside a function's body or
                    # below the first section stays a number.
                    def absolute(mm):
                        v = int(mm.group(1), 16)
                        sec = self.section_of(v)
                        if sec is None:
                            return mm.group(0)
                        hit = self.function_at(v)
                        if ("x" in self.sections[sec]["flags"] or hit) and not (hit and hit[1] == 0):
                            return mm.group(0)
                        r = ref(v)
                        return ("$" if mm.group(0).startswith("$") else "") + f"[abs:{r}]" if r else mm.group(0)
                    body = HEX.sub(absolute, body)
                body = re.sub(r"\s+", " ", body)
                out.append((body, refs))
            f["norm"] = out

    def identities(self, rounds=ROUNDS):
        def digest(parts):
            return hashlib.sha256("\n".join(parts).encode()).hexdigest()[:16]

        ident = {a: digest(re.sub(r"F[0-9a-f]+", "F", b) for b, _ in f["norm"]) for a, f in self.funcs.items()}
        self.own = ident
        for _ in range(rounds):
            nxt = {}
            for a, f in self.funcs.items():
                nxt[a] = digest(re.sub(r"F([0-9a-f]+)", lambda m: "F" + ident.get(int(m.group(1), 16), "?"), b)
                                for b, _ in f["norm"])
            ident = nxt
        self.ident = ident
        return ident

    def text_layout(self):
        text = self.sections.get(".text")
        if not text:
            return []
        lo, hi = text["addr"], text["addr"] + text["size"]
        return [(self.ident[a], a - lo, self.funcs[a]["size"]) for a in self.starts if lo <= a < hi]


def demangle(names):
    if not names:
        return []
    try:
        return subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True,
                              check=True).stdout.splitlines()
    except (OSError, subprocess.CalledProcessError):
        return list(names)


def readable(binary: Binary, addr):
    return binary.funcs[addr]["names"][0]


def body_text(binary: Binary, addr, names):
    lines = []
    for b, _ in binary.funcs[addr]["norm"]:
        lines.append(re.sub(r"F([0-9a-f]+)", lambda m: "<" + names.get(int(m.group(1), 16), "?") + ">", b))
    return lines


def compare_program(base_path: Path, head_path: Path, objdump, readelf, max_details=8):
    bb, hb = Binary(base_path, objdump, readelf), Binary(head_path, objdump, readelf)
    for b in (bb, hb):
        b.normalize()
        b.identities()
    bc = collections.Counter(bb.ident.values())
    hc = collections.Counter(hb.ident.values())
    only_base = bc - hc
    only_head = hc - bc
    same_code = not only_base and not only_head
    lb, lh = bb.text_layout(), hb.text_layout()
    same_layout = same_code and lb == lh
    moved = 0
    if same_code and not same_layout:
        moved = sum(1 for x, y in zip(lb, lh) if x != y) + abs(len(lb) - len(lh))
    res = {
        "functions": len(bb.funcs), "functions_head": len(hb.funcs),
        "instructions": sum(len(f["norm"]) for f in bb.funcs.values()),
        "instructions_head": sum(len(f["norm"]) for f in hb.funcs.values()),
        "same_code": same_code, "same_layout": same_layout,
        "text_size": bb.sections.get(".text", {}).get("size"), "text_size_head": hb.sections.get(".text", {}).get("size"),
        "layout_positions_differ": moved,
        "differ_base": [], "differ_head": [], "details": [],
    }
    if not same_code:
        # A function whose own instructions (references not followed) occur on the other side
        # differs only through what it references; list the ones that changed themselves first.
        def listing(b, counter, other):
            own_other = collections.Counter(other.own.values())
            addrs = [a for a in b.starts if counter.get(b.ident[a])]
            dem = demangle([readable(b, a) for a in addrs])
            rows = [{"name": d, "symbol": readable(b, a), "instructions": len(b.funcs[a]["norm"]),
                     "own_change": not own_other.get(b.own[a])} for a, d in zip(addrs, dem)]
            return sorted(rows, key=lambda r: not r["own_change"])
        res["differ_base"] = listing(bb, only_base, hb)
        res["differ_head"] = listing(hb, only_head, bb)
        # Unified diffs for functions that differ but keep their demangled name.
        bnames = dict(zip(bb.starts, demangle([readable(bb, a) for a in bb.starts])))
        hnames = dict(zip(hb.starts, demangle([readable(hb, a) for a in hb.starts])))
        hby = {hnames[a]: a for a in hb.starts if only_head.get(hb.ident[a])}
        own_head = collections.Counter(hb.own.values())
        for a in bb.starts:
            if not only_base.get(bb.ident[a]) or own_head.get(bb.own[a]) or len(res["details"]) >= max_details:
                continue
            name = bnames[a]
            if name in hby:
                diff = list(difflib.unified_diff(body_text(bb, a, bnames), body_text(hb, hby[name], hnames),
                                                 "base", "head", n=2, lineterm=""))
                if diff:
                    res["details"].append({"name": name, "diff": diff[:80]})
    return res


def cmd_compare(a):
    base, head = Path(a.base), Path(a.head)
    results = {}
    rc = 0
    for prog in a.programs:
        pb, ph = base / prog, head / prog
        if not pb.is_file() or not ph.is_file():
            results[prog] = {"missing": [str(p) for p in (pb, ph) if not p.is_file()]}
            rc = max(rc, 2 if not a.allow_missing else rc)
            continue
        results[prog] = compare_program(pb, ph, a.objdump, a.readelf)
        if not results[prog]["same_code"]:
            rc = max(rc, 1)
    out = {"base": str(base), "head": str(head), "compiler": a.label or "", "programs": results,
           "verdict": "differ" if rc == 1 else ("error" if rc == 2 else
                      ("identical" if all(r.get("same_layout") for r in results.values() if "missing" not in r)
                       else "identical code, layout differs"))}
    summary = render(out)
    try:
        print(summary)
    except BrokenPipeError:
        pass
    if a.summary:
        Path(a.summary).write_text(summary + "\n", encoding="utf-8")
    if a.json:
        Path(a.json).write_text(json.dumps(out, indent=1) + "\n", encoding="utf-8")
    return rc


def render(out):
    L = [f"#### Machine code {out['compiler']}".rstrip() + f": {out['verdict']}", "",
         "| Program | Functions | Instructions | Same instructions | Same .text layout |",
         "| --- | --- | --- | --- | --- |"]
    for prog, r in out["programs"].items():
        name = f"{prog} ({LABELS.get(prog, prog)})"
        if "missing" in r:
            L.append(f"| {name} | missing: {', '.join(r['missing'])} | | | |")
            continue
        fn = str(r["functions"]) if r["functions"] == r["functions_head"] else f"{r['functions']} -> {r['functions_head']}"
        ins = str(r["instructions"]) if r["instructions"] == r["instructions_head"] else \
            f"{r['instructions']} -> {r['instructions_head']}"
        lay = "yes" if r["same_layout"] else ("no" if not r["same_code"] else
                                              f"no ({r['layout_positions_differ']} positions, .text {r['text_size']} -> {r['text_size_head']} B)")
        L.append(f"| {name} | {fn} | {ins} | {'yes' if r['same_code'] else '**no**'} | {lay} |")
    for prog, r in out["programs"].items():
        if "missing" in r or r["same_code"]:
            continue
        own = {side: sum(f["own_change"] for f in r[side]) for side in ("differ_base", "differ_head")}
        L += ["", f"{prog}: {len(r['differ_base'])} base-only and {len(r['differ_head'])} head-only function bodies; "
                  f"{own['differ_base']} and {own['differ_head']} of them differ in their own instructions, "
                  "the rest only in what they reference"]
        for side in ("differ_base", "differ_head"):
            for f in r[side][:20]:
                tag = "" if f["own_change"] else ", references only"
                L.append(f"- {side[7:]}: `{f['name'][:160]}` ({f['instructions']} insns{tag})")
        for d in r["details"]:
            L += ["", "```diff"] + d["diff"] + ["```"]
    return "\n".join(L)


def compare_gate_work(a, work: Path):
    """Compare base and head of a perf_gate work directory that has been built for a.compiler."""
    plan = json.loads((work / "plan.json").read_text())
    builds = json.loads((work / f"build-{a.compiler}.json").read_text())
    a.base = builds["trees"][plan["roles"]["base"]]["dir"]
    a.head = builds["trees"][plan["roles"]["head"]]["dir"]
    a.label = a.label or a.compiler
    return cmd_compare(a)


def cmd_run(a):
    here = Path(__file__).resolve().parent
    pg = [sys.executable, str(here / "perf_gate.py")]
    work = Path(a.work).resolve()
    repo = Path(a.repo).resolve()
    subprocess.run(pg + ["prepare", "--work", str(work), "--repo", str(repo), "--base", a.base, "--head", a.head,
                         "--anchor", a.base], check=True)
    build = pg + ["build", "--work", str(work), "--compiler", a.compiler, "--cereal", a.cereal,
                  "--jobs", str(a.jobs), "--extra-flags", a.extra_flags]
    if a.cxx:
        build += ["--cxx", a.cxx]
    r = subprocess.run(build, cwd=work)
    if r.returncode:
        return 2
    return compare_gate_work(a, work)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    for name in ("compare", "run", "gate"):
        sp = sub.add_parser(name)
        if name != "gate":
            sp.add_argument("--base", required=True)
            sp.add_argument("--head", required=True)
        sp.add_argument("--programs", nargs="+", default=list(PROGRAMS))
        sp.add_argument("--allow-missing", action="store_true", help="a missing program is not an error")
        sp.add_argument("--objdump", default="objdump")
        sp.add_argument("--readelf", default="readelf")
        sp.add_argument("--label", default="", help="compiler name for the summary")
        sp.add_argument("--summary", default="")
        sp.add_argument("--json", default="")
        if name == "gate":
            sp.add_argument("--work", required=True, help="perf_gate.py work directory (plan.json, build-*.json)")
            sp.add_argument("--compiler", required=True, choices=("gcc16", "p2996"))
        if name == "run":
            sp.add_argument("--repo", default=".")
            sp.add_argument("--work", required=True)
            sp.add_argument("--compiler", required=True, choices=("gcc16", "p2996"))
            sp.add_argument("--cxx", default="")
            sp.add_argument("--cereal", required=True)
            sp.add_argument("--extra-flags", default="")
            sp.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    a = p.parse_args()
    try:
        if a.cmd == "gate":
            return compare_gate_work(a, Path(a.work).resolve())
        return cmd_compare(a) if a.cmd == "compare" else cmd_run(a)
    except subprocess.CalledProcessError as e:
        print(f"asm_equal: {shlex.join(e.cmd)} failed: {e.stderr}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
