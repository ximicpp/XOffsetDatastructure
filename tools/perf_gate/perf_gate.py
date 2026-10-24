#!/usr/bin/env python3
"""Instruction-count (callgrind Ir) performance regression gate (H3 L0).

Builds three copies of the benchmarks in one job -- anchor (bench/perf/ANCHOR),
base and head -- all from the head's benchmark sources, counts the instructions
of every timed phase of the SLG benchmark (S scale), the formal acceptance
harness (M fixture) and every case of the relocation tax benchmark under
callgrind, and judges head/base and head/anchor against the thresholds in
docs/PERF_GATE.md.

Instruction counts only block regressions. They are never evidence that code
got faster; timing conclusions come from the Mac formal batch (L2).

Subcommands (run in this order, or use `run` for all of them):
  prepare  export the anchor/base/head library trees            (needs git)
  build    compile slg_bench and sc_xoffset for every tree     (needs the compiler)
  measure  correctness checks, then Ir of every phase           (needs valgrind)
  compare  thresholds, ledger, job summary and JSON             (python only)
"""
from __future__ import annotations

import argparse
import collections
import concurrent.futures
import fnmatch
import hashlib
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

SLG_DIR = "bench/slg"
FORMAL_DIR = "bench/formal"
RELOC_DIR = "bench/reloc"
ANCHOR_FILE = "bench/perf/ANCHOR"
LEDGER_FILE = "bench/perf/ledger.json"
ROLES = ("anchor", "base", "head")

SLG_SEED = "20261021"
SLG_SCALE = "S"
SLG_CYCLES = "8"
SLG_SKIP_CYCLES = "4"
FORMAL_PHASES = ("load", "query_before", "mutate", "emit_after", "retire", "reload", "query_after", "cleanup")
RELOC_ROUNDS, RELOC_WARMUP, RELOC_SKIP_ROUNDS = "5", "1", "2"   # collect rounds 2-4 of each case

# Thresholds (docs/PERF_GATE.md, design H3 section 5.3).
TOTAL_WARN, TOTAL_FAIL = 1.003, 1.010          # head/base, scenario totals and formal judged phases
PHASE_WARN, PHASE_FAIL = 1.020, 1.050          # head/base, phases with >= 5% of their scenario
PHASE_MIN_SHARE = 0.05
ANCHOR_WARN, ANCHOR_FAIL = 1.010, 1.020        # head/anchor on the totals, net of the ledger
DROP_NOTE = 0.97                               # a total below this gets the "Ir down" note
SLG_TOTALS = ("A", "B", "C", "D")
FORMAL_TOTALS = ("read_sum", "mutate", "emit_after", "load", "reload")
FORMAL_REPORT_ONLY = ("retire", "cleanup")

FIRST_LINE = "指令数只用于拦截回退，计时结论以 Mac 正式批次为准。"

FN_TOP = 10                                    # functions listed per flagged metric
FN_NAME_MAX = 110                              # display width of a function name
FN_SUSPECT_MIN = 0.10                          # inlining suspects move >= 10% of the metric's change


def log(*a):
    print("[perf_gate]", *a, file=sys.stderr, flush=True)


def sh(cmd, **kw):
    return subprocess.run(cmd, check=True, text=True, capture_output=True, **kw).stdout


def short(sha):
    return sha[:12]


def load_json(p):
    return json.loads(Path(p).read_text(encoding="utf-8"))


def dump_json(p, obj):
    Path(p).write_text(json.dumps(obj, indent=2, sort_keys=True, ensure_ascii=False) + "\n", encoding="utf-8")


def sha256_file(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# ---------------------------------------------------------------- prepare

def export_tree(repo: Path, sha: str, dest: Path):
    """Library tree of `sha` (git archive) plus its pinned TypeLayout headers."""
    dest.mkdir(parents=True)
    archive = subprocess.run(["git", "-C", str(repo), "archive", "--format=tar", sha],
                             check=True, capture_output=True).stdout
    subprocess.run(["tar", "-x", "-C", str(dest)], input=archive, check=True)
    tl = sh(["git", "-C", str(repo), "ls-tree", sha, "external/typelayout"]).split()
    if len(tl) < 3 or tl[1] != "commit":
        raise SystemExit(f"{sha}: external/typelayout is not a submodule")
    tl_sha = tl[2]
    sub = repo / "external/typelayout"
    if not (sub / ".git").exists():
        raise SystemExit("external/typelayout is not checked out; run git submodule update --init external/typelayout")
    have = subprocess.run(["git", "-C", str(sub), "cat-file", "-e", tl_sha + "^{commit}"], capture_output=True).returncode == 0
    if not have:
        subprocess.run(["git", "-C", str(sub), "fetch", "--depth", "1", "origin", tl_sha], check=True, capture_output=True)
    archive = subprocess.run(["git", "-C", str(sub), "archive", "--format=tar", tl_sha],
                             check=True, capture_output=True).stdout
    (dest / "external/typelayout").mkdir(parents=True, exist_ok=True)
    subprocess.run(["tar", "-x", "-C", str(dest / "external/typelayout")], input=archive, check=True)
    return tl_sha


def cmd_prepare(a):
    repo = Path(a.repo).resolve()
    work = Path(a.work).resolve()
    work.mkdir(parents=True, exist_ok=True)
    head = sh(["git", "-C", str(repo), "rev-parse", a.head + "^{commit}"]).strip()
    anchor_ref = a.anchor
    if not anchor_ref and a.bench_root and (Path(a.bench_root) / ANCHOR_FILE).is_file():
        anchor_ref = (Path(a.bench_root) / ANCHOR_FILE).read_text().split()[0]
    if not anchor_ref:
        anchor_ref = sh(["git", "-C", str(repo), "show", f"{head}:{ANCHOR_FILE}"]).split()[0]
    anchor = sh(["git", "-C", str(repo), "rev-parse", anchor_ref + "^{commit}"]).strip()
    base = sh(["git", "-C", str(repo), "rev-parse", a.base + "^{commit}"]).strip()
    roles = {"anchor": anchor, "base": base, "head": head}
    trees = {}
    for sha in dict.fromkeys(roles.values()):
        dest = work / "src" / short(sha)
        if dest.exists():
            shutil.rmtree(dest)
        tl = export_tree(repo, sha, dest)
        trees[sha] = {"path": str(dest), "typelayout": tl,
                      "subject": sh(["git", "-C", str(repo), "log", "-1", "--format=%s", sha]).strip()}
        log(f"exported {short(sha)} ({', '.join(r for r, s in roles.items() if s == sha)})")
    bench_root = Path(a.bench_root).resolve() if a.bench_root else Path(trees[head]["path"])
    plan = {"roles": roles, "trees": trees, "bench_root": str(bench_root),
            "slg_dir": a.slg_dir, "formal_dir": a.formal_dir, "reloc_dir": a.reloc_dir,
            "refs": {"anchor": anchor_ref, "base": a.base, "head": a.head}}
    # Ledgers travel with the trees: head's applies, base's tells which entries are new.
    for role in ("base", "head"):
        try:
            plan[f"ledger_{role}"] = json.loads(sh(["git", "-C", str(repo), "show", f"{roles[role]}:{LEDGER_FILE}"]))
        except subprocess.CalledProcessError:
            plan[f"ledger_{role}"] = {"entries": []}
    if a.ledger:
        plan["ledger_head"] = load_json(a.ledger)
    dump_json(work / "plan.json", plan)


# ---------------------------------------------------------------- build

def compiler_profile(name, cxx):
    if name == "gcc16":
        return {"cxx": cxx or "g++", "std": ["-std=c++26", "-freflection"], "formal_std": ["-std=gnu++26", "-freflection"],
                "link": [], "env_note": "GCC 16, libstdc++"}
    if name == "p2996":
        root = os.environ.get("P2996_ROOT", "/opt/p2996-toolchain")
        libdirs = [f"{root}/lib", f"{root}/lib/x86_64-unknown-linux-gnu"]
        link = ["-stdlib=libc++"] + [f"-L{d}" for d in libdirs] + [f"-Wl,-rpath,{d}" for d in libdirs]
        return {"cxx": cxx or f"{root}/bin/clang++",
                "std": ["-std=c++26", "-freflection", "-fexpansion-statements", "-stdlib=libc++"],
                "formal_std": ["-std=gnu++26", "-freflection", "-fexpansion-statements", "-stdlib=libc++"],
                "link": link, "env_note": "P2996 Clang, libc++"}
    raise SystemExit(f"unknown compiler {name}")


def build_one(prof, lib: Path, bench: Path, slg_dir, formal_dir, cereal, outdir: Path, extra):
    """Build slg_bench and sc_xoffset of `bench` against library tree `lib`. Returns (ok, log)."""
    outdir.mkdir(parents=True, exist_ok=True)
    cxx = shlex.split(prof["cxx"])
    common = ["-O3", "-DNDEBUG", f"-I{lib}", "-isystem", f"{lib}/external/typelayout/include"] + extra
    slg = bench / slg_dir
    formal = bench / formal_dir
    cmds = {
        "slg_bench": cxx + prof["std"] + common + [
            f"-I{slg}/include", f"-I{slg}/adapters", f"-I{slg}/bench", "-isystem", str(cereal),
            "-DSLG_WITH_XOFFSET=1", "-DSLG_WITH_CEREAL=1", "-DSLG_IR_COUNT=1",
            f"{slg}/bench/slg_bench.cpp", "-o", str(outdir / "slg_bench")] + prof["link"],
        "sc_xoffset": cxx + prof["formal_std"] + common + [
            f"-I{formal}/include", f"-I{formal}", "-pthread", "-DSC_IR_COUNT=1",
            '-DSC_ADAPTER_HEADER="adapters/xoffset/adapter.hpp"', "-DSC_ADAPTER_TYPE=sc::XOffsetAdapter",
            f"{formal}/driver.cpp", "-o", str(outdir / "sc_xoffset")] + prof["link"],
    }
    out = []
    ok = True

    def run(item):
        name, cmd = item
        r = subprocess.run(cmd, text=True, capture_output=True)
        return name, cmd, r

    with concurrent.futures.ThreadPoolExecutor(2) as ex:
        for name, cmd, r in ex.map(run, cmds.items()):
            out.append(f"$ {shlex.join(cmd)}\n{r.stdout}{r.stderr}")
            ok = ok and r.returncode == 0
    return ok, "\n".join(out)


def build_reloc(prof, lib: Path, reloc: Path, outdir: Path, extra):
    """Build reloc_bench of `reloc` against library tree `lib`. Returns (status, log)."""
    if not (reloc / "reloc_bench.cpp").is_file():
        return "absent", ""
    cmd = shlex.split(prof["cxx"]) + prof["std"] + ["-O3", "-DNDEBUG", f"-I{lib}", "-isystem",
                                                    f"{lib}/external/typelayout/include"] + extra + [
        "-DRELOC_IR_COUNT=1", f"{reloc}/reloc_bench.cpp", "-o", str(outdir / "reloc_bench")] + prof["link"]
    r = subprocess.run(cmd, text=True, capture_output=True)
    return ("ok" if r.returncode == 0 else "failed"), f"$ {shlex.join(cmd)}\n{r.stdout}{r.stderr}"


def cmd_build(a):
    work = Path(a.work).resolve()
    plan = load_json(work / "plan.json")
    prof = compiler_profile(a.compiler, a.cxx)
    extra = shlex.split(a.extra_flags or "")
    head_bench = Path(plan["bench_root"])
    try:
        version = subprocess.run(shlex.split(prof["cxx"]) + ["--version"], text=True, capture_output=True).stdout.splitlines()[0]
    except (OSError, IndexError):
        version = "?"
    builds = {"compiler": a.compiler, "compiler_version": version, "trees": {}}

    def one(sha):
        tree = Path(plan["trees"][sha]["path"])
        outdir = work / "bin" / a.compiler / short(sha)
        if outdir.exists():
            shutil.rmtree(outdir)
        ok, text = build_one(prof, tree, head_bench, plan["slg_dir"], plan["formal_dir"], a.cereal, outdir, extra)
        status, bench = ("ok", "head") if ok else (None, None)
        if not ok and (tree / plan["slg_dir"]).is_dir() and (tree / plan["formal_dir"]).is_dir() and tree != head_bench:
            ok2, text2 = build_one(prof, tree, tree, plan["slg_dir"], plan["formal_dir"], a.cereal, outdir, extra)
            text += "\n--- retry with the tree's own benchmarks ---\n" + text2
            if ok2:
                status, bench = "bench_differs", "own"
        if status is None:
            status = "incomparable"
        # The relocation benchmark is optional per tree: it is built from the same benchmark
        # sources as the rest, and an older tree that cannot build it only loses reloc.*.
        reloc_dir = plan.get("reloc_dir", RELOC_DIR)
        reloc_status = "absent"
        if status != "incomparable":
            reloc_src = (head_bench if bench == "head" else tree) / reloc_dir
            reloc_status, rtext = build_reloc(prof, tree, reloc_src, outdir, extra)
            text += "\n--- reloc_bench ---\n" + rtext
        (work / "logs").mkdir(exist_ok=True)
        (work / "logs" / f"build-{a.compiler}-{short(sha)}.log").write_text(text, encoding="utf-8")
        log(f"build {a.compiler} {short(sha)}: {status}, reloc {reloc_status}")
        return sha, {"status": status, "bench": bench, "dir": str(outdir), "reloc": reloc_status}

    with concurrent.futures.ThreadPoolExecutor(max(1, min(3, a.jobs // 2))) as ex:
        for sha, res in ex.map(one, list(plan["trees"])):
            builds["trees"][sha] = res
    dump_json(work / f"build-{a.compiler}.json", builds)
    head = plan["roles"]["head"]
    head_reloc = builds["trees"][head].get("reloc")
    if builds["trees"][head]["status"] != "ok" or head_reloc == "failed":
        log("head does not build; see logs/")
        tail = (work / "logs" / f"build-{a.compiler}-{short(head)}.log").read_text(encoding="utf-8")[-4000:]
        print(tail, file=sys.stderr)
        return 1
    return 0


# ---------------------------------------------------------------- measure

COLLECTED = re.compile(r"Collected\s*:\s*(\d+)")


def callgrind(valgrind, cmd, env, tmp: Path):
    """Run `cmd` under callgrind with collection off at start; return the collected Ir and the profile."""
    logf = tmp / "vg.log"
    full = shlex.split(valgrind) + ["--tool=callgrind", "--collect-atstart=no",
                                    f"--callgrind-out-file={tmp / 'cg.out'}", f"--log-file={logf}"] + cmd
    r = subprocess.run(full, env=env, text=True, capture_output=True, cwd=tmp)
    text = logf.read_text(encoding="utf-8", errors="replace") if logf.exists() else ""
    m = COLLECTED.search(text)
    if r.returncode != 0 or not m:
        raise RuntimeError(f"callgrind failed ({r.returncode}): {shlex.join(cmd)}\n{r.stderr[-2000:]}\n{text[-2000:]}")
    return int(m.group(1)), fn_profile(tmp / "cg.out")


CG_COMPRESSED = re.compile(r"^\((\d+)\)(?:\s(.*))?$")
CG_CLONE = re.compile(r"(?: \[clone [^\]]+\])+$")


def fn_profile(path: Path):
    """Self Ir per function, and caller/callee pairs, from one callgrind out file.

    Self Ir is what `callgrind_annotate --inclusive=no` lists. GCC clone suffixes
    (`[clone .constprop.0]`) are dropped so that a clone and its original match.
    """
    names = {}

    def name_of(v):
        m = CG_COMPRESSED.match(v)
        if not m:
            return CG_CLONE.sub("", v)
        i, n = m.groups()
        if n is not None:
            names[i] = CG_CLONE.sub("", n)
        return names.get(i, f"({i})")

    self_ir = collections.Counter()
    edges = set()
    npos, fn, callee, call_cost = 1, None, None, False
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            if line[0].isdigit() or line[0] in "+-*":
                parts = line.split()
                ir = int(parts[npos]) if len(parts) > npos else 0
                if call_cost:  # inclusive cost of the call that the calls= line announced
                    call_cost = False
                    if ir and fn and callee:
                        edges.add((fn, callee))
                elif fn:
                    self_ir[fn] += ir
                continue
            if line.startswith("positions:"):
                npos = len(line.split(":", 1)[1].split())
                continue
            key, _, val = line.partition("=")
            if key == "fn":
                fn = name_of(val)
            elif key == "cfn":
                callee = name_of(val)
            elif key == "calls":
                call_cost = True
    return {"self": {k: v for k, v in self_ir.items() if v}, "edges": sorted([a, b] for a, b in edges)}


def slg_env(**extra):
    env = dict(os.environ)
    env.pop("SLG_IR_KEYS", None)
    env.update({"SLG_REUSE": "1"})
    env.update(extra)
    return env


def slg_bench_cmd(binary, out):
    return [str(binary), "--mode", "bench", "--scale", SLG_SCALE, "--cycles", SLG_CYCLES,
            "--seed", SLG_SEED, "--formats", "xoffset", "--out", str(out)]


def discover_slg_keys(binary, tmp: Path):
    out = tmp / "keys.jsonl"
    subprocess.run(slg_bench_cmd(binary, out), env=slg_env(), check=True, capture_output=True)
    keys = set()
    for line in out.read_text().splitlines():
        row = json.loads(line)
        if "metric" in row and "ns" in row:
            keys.add(row["metric"])
    return sorted(keys)


def checks_for(binary_dir: Path, fx: Path, tmp: Path):
    """Correctness before counting: SLG verify (S, both protocols) and formal validate S/M."""
    res = {}
    slg = binary_dir / "slg_bench"
    sc = binary_dir / "sc_xoffset"
    for name, env in (("slg_verify_S", slg_env()), ("slg_verify_S_per_message", {k: v for k, v in slg_env().items() if k != "SLG_REUSE"})):
        r = subprocess.run([str(slg), "--mode", "verify", "--scale", "S", "--formats", "xoffset,cereal"],
                           env=env, text=True, capture_output=True)
        res[name] = {"ok": r.returncode == 0, "tail": (r.stdout + r.stderr)[-1500:] if r.returncode else ""}
    for scale in ("S", "M"):
        wire = tmp / f"{scale}.wire"
        r = subprocess.run([str(sc), "encode", "--canonical", str(fx / scale / "input.canonical.bin"), "--wire", str(wire)],
                           text=True, capture_output=True)
        res[f"formal_encode_{scale}"] = {"ok": r.returncode == 0, "tail": r.stderr[-1500:]}
        if r.returncode:
            continue
        res[f"wire_{scale}_sha256"] = sha256_file(wire)
        out = tmp / f"validate-{scale}.json"
        r = subprocess.run([str(sc), "validate", "--wire", str(wire), "--trace", str(fx / scale / "trace.bin"),
                            "--result", str(out)], text=True, capture_output=True)
        res[f"formal_validate_{scale}"] = {"ok": r.returncode == 0, "tail": r.stderr[-1500:]}
        shutil.copy(wire, binary_dir / f"{scale}.wire")
    res["ok"] = all(v["ok"] for k, v in res.items() if isinstance(v, dict))
    return res


def reloc_cmd(binary, case):
    return [str(binary), "--rounds", RELOC_ROUNDS, "--warmup", RELOC_WARMUP, "--cases", case]


def reloc_env(case):
    return dict(os.environ, RELOC_IR_KEYS=case, RELOC_IR_SKIP_ROUNDS=RELOC_SKIP_ROUNDS)


def reloc_check(binary: Path, tmp: Path):
    """reloc_bench --mode verify (digests of all sides agree) and the case names it ran."""
    out = tmp / "reloc-verify.jsonl"
    r = subprocess.run([str(binary), "--mode", "verify", "--out", str(out)], text=True, capture_output=True)
    cases = set()
    if out.exists():
        for line in out.read_text().splitlines():
            row = json.loads(line)
            if "case" in row and "round" in row:
                cases.add(row["case"])
    return {"ok": r.returncode == 0 and bool(cases), "tail": (r.stdout + r.stderr)[-1500:] if r.returncode else ""}, sorted(cases)


def cmd_measure(a):
    work = Path(a.work).resolve()
    plan = load_json(work / "plan.json")
    builds = load_json(work / f"build-{a.compiler}.json")
    head = plan["roles"]["head"]
    fx = work / "fx"
    reference = Path(plan["bench_root"]) / plan["formal_dir"] / "model/reference.py"
    for scale in ("S", "M"):
        if not (fx / scale / "trace.bin").exists():
            if (fx / scale).exists():
                shutil.rmtree(fx / scale)
            fx.mkdir(exist_ok=True)
            log(f"generating formal fixture {scale}")
            subprocess.run([a.python, str(reference), "--scale", scale, "--out", str(fx / scale)], check=True, capture_output=True)
    fixture = {s: load_json(fx / s / "fixture.json")["input_digest"] for s in ("S", "M")}

    vg_version = sh(shlex.split(a.valgrind) + ["--version"]).strip()
    result = {"compiler": a.compiler, "compiler_version": builds["compiler_version"], "valgrind": vg_version,
              "fixture_input_digest": fixture, "trees": {}}
    scratch = Path(tempfile.mkdtemp(prefix="pg-", dir=work))

    usable = {sha: b for sha, b in builds["trees"].items() if b["status"] in ("ok", "bench_differs")}
    for sha, b in usable.items():
        t = scratch / f"chk-{short(sha)}"
        t.mkdir()
        chk = checks_for(Path(b["dir"]), fx, t)
        t2 = scratch / f"keys-{short(sha)}"
        t2.mkdir()
        keys = discover_slg_keys(Path(b["dir"]) / "slg_bench", t2) if chk["ok"] else []
        tr = {"checks": chk, "slg_keys": keys, "slg": {}, "formal": {}, "functions": {},
              "reloc_status": b.get("reloc", "absent"), "reloc_keys": [], "reloc": {}}
        if chk["ok"] and tr["reloc_status"] == "ok":
            tr["reloc_check"], tr["reloc_keys"] = reloc_check(Path(b["dir"]) / "reloc_bench", t2)
            if not tr["reloc_check"]["ok"]:
                tr["reloc_status"] = "verify_failed"
                tr["reloc_keys"] = []
        result["trees"][sha] = tr
        log(f"checks {short(sha)}: {'ok' if chk['ok'] else 'FAILED'}; {len(keys)} SLG keys; "
            f"reloc {tr['reloc_status']}, {len(tr['reloc_keys'])} cases")

    tasks = []
    for sha, tr in result["trees"].items():
        if not tr["checks"]["ok"]:
            continue
        bdir = Path(usable[sha]["dir"])
        for k in tr["slg_keys"]:
            tasks.append((sha, "slg", k, 0))
        for ph in FORMAL_PHASES:
            tasks.append((sha, "formal", ph, 0))
        for k in tr["reloc_keys"]:
            tasks.append((sha, "reloc", k, 0))
    # Determinism: the same binary twice must give bit-identical counts.
    if head in result["trees"] and result["trees"][head]["checks"]["ok"]:
        keys = result["trees"][head]["slg_keys"]
        probe = "A.apply" if "A.apply" in keys else (keys[0] if keys else None)
        if probe:
            tasks.append((head, "slg", probe, 1))
        tasks.append((head, "formal", "mutate", 1))
        if result["trees"][head]["reloc_keys"]:
            tasks.append((head, "reloc", result["trees"][head]["reloc_keys"][0], 1))

    def run(task):
        sha, kind, key, rep = task
        bdir = Path(usable[sha]["dir"])
        tmp = Path(tempfile.mkdtemp(prefix=f"{kind}-", dir=scratch))
        try:
            if kind == "slg":
                ir, prof = callgrind(a.valgrind, slg_bench_cmd(bdir / "slg_bench", tmp / "raw.jsonl"),
                               slg_env(SLG_IR_KEYS=key, SLG_IR_SKIP_CYCLES=SLG_SKIP_CYCLES), tmp)
            elif kind == "reloc":
                ir, prof = callgrind(a.valgrind, reloc_cmd(bdir / "reloc_bench", key), reloc_env(key), tmp)
            else:
                env = dict(os.environ, SC_IR_PHASE=key)
                ir, prof = callgrind(a.valgrind, [str(bdir / "sc_xoffset"), "run", "--wire", str(bdir / "M.wire"),
                                            "--trace", str(fx / "M" / "trace.bin"), "--wire-out", str(tmp / "w"),
                                            "--canonical-out", str(tmp / "c"), "--queries-out", str(tmp / "q"),
                                            "--result", str(tmp / "r.json")], env, tmp)
            return task, ir, prof, None
        except Exception as e:  # noqa: BLE001 - reported per task
            return task, None, None, str(e)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    log(f"{len(tasks)} callgrind runs on {a.jobs} workers")
    errors = []
    repeats = {}
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        for (sha, kind, key, rep), ir, prof, err in ex.map(run, tasks):
            if err:
                errors.append(f"{short(sha)} {kind} {key}: {err}")
                continue
            if rep:
                repeats[f"{kind}.{key}"] = ir
            else:
                result["trees"][sha][kind][key] = ir
                result["trees"][sha]["functions"][f"{kind}.{key}"] = prof
    det = {}
    for k, ir in repeats.items():
        kind, key = k.split(".", 1)
        first = result["trees"][head][kind].get(key)
        det[k] = {"first": first, "second": ir, "identical": first == ir}
    result["determinism"] = det
    result["errors"] = errors
    shutil.rmtree(scratch, ignore_errors=True)
    dump_json(work / f"ir-{a.compiler}.json", result)
    for e in errors:
        log(e)
    return 0


# ---------------------------------------------------------------- compare

def metrics_of(tree):
    """Flatten one tree's raw counts into gate metrics."""
    m = {}
    slg = tree["slg"]
    for k, v in slg.items():
        m[f"slg.{k}"] = v
    for s in SLG_TOTALS:
        parts = [v for k, v in slg.items() if k.startswith(s + ".")]
        if parts:
            m[f"slg.{s}"] = sum(parts)
    f = tree["formal"]
    for k, v in f.items():
        m[f"formal.{k}"] = v
    if "query_before" in f and "query_after" in f:
        m["formal.read_sum"] = f["query_before"] + f["query_after"]
    if f:
        m["formal.total"] = sum(f.values())
    for k, v in tree.get("reloc", {}).items():
        m[f"reloc.{k}"] = v
    return m


def classify(metric, base_m, head_m):
    """total | phase | report, plus the phase's share of its scenario."""
    kind, _, rest = metric.partition(".")
    if kind == "reloc":
        return "phase", 1.0  # every case is its own scenario
    if kind == "slg":
        if rest in SLG_TOTALS:
            return "total", 1.0
        scen = rest.split(".")[0]
        share = 0.0
        for mm in (base_m, head_m):
            denom = mm.get(f"slg.{scen}") if scen in SLG_TOTALS else sum(v for k, v in mm.items() if k.startswith(f"slg.{scen}."))
            if denom and metric in mm:
                share = max(share, mm[metric] / denom)
        return ("phase" if share >= PHASE_MIN_SHARE else "report"), share
    if rest in FORMAL_TOTALS:
        return "total", 1.0
    if rest in FORMAL_REPORT_ONLY or rest == "total":
        return "report", 0.0
    share = 0.0
    for mm in (base_m, head_m):
        if mm.get("formal.total") and metric in mm:
            share = max(share, mm[metric] / mm["formal.total"])
    return ("phase" if share >= PHASE_MIN_SHARE else "report"), share


def ledger_matches(entries, metric, compiler):
    out = []
    for e in entries:
        comp = e.get("compiler", "*")
        if comp not in ("*", compiler):
            continue
        if fnmatch.fnmatchcase(metric, e["metric"]):
            out.append(e)
    return out


def validate_ledger(entries):
    problems = []
    need = ("id", "pr", "metric", "compiler", "max_ratio", "reason", "evidence", "approved_by")
    seen = set()
    for e in entries:
        miss = [k for k in need if k not in e or e[k] in ("", None)]
        if miss:
            problems.append(f"ledger entry {e.get('id', '?')}: missing {', '.join(miss)}")
        if e.get("id") in seen:
            problems.append(f"ledger entry {e.get('id')}: duplicate id")
        seen.add(e.get("id"))
        try:
            r = float(e.get("max_ratio", 0))
            if r <= 1.0:
                problems.append(f"ledger entry {e.get('id')}: max_ratio must be > 1")
            # Scenario-level allowances above 2% need susu's confirmation (design 5.4).
            kind = e.get("metric", "")
            scen_level = kind in {f"slg.{s}" for s in SLG_TOTALS} or kind.startswith("formal.")
            if scen_level and r > 1.02 and "susu" not in str(e.get("approved_by", "")):
                problems.append(f"ledger entry {e.get('id')}: scenario-level allowance {r} > 1.02 needs approved_by susu")
        except (TypeError, ValueError):
            problems.append(f"ledger entry {e.get('id')}: max_ratio is not a number")
    return problems


def metric_keys(metric, tree):
    """The callgrind runs (keys of a tree's "functions") that a gate metric adds up."""
    kind, _, rest = metric.partition(".")
    if kind == "reloc":
        return [metric]
    if kind == "slg":
        if rest in SLG_TOTALS:
            return sorted(f"slg.{k}" for k in tree.get("slg", {}) if k.startswith(rest + "."))
        return [metric]
    if rest == "read_sum":
        return ["formal.query_before", "formal.query_after"]
    if rest == "total":
        return [f"formal.{p}" for p in FORMAL_PHASES]
    return [metric]


def merged_profile(tree, keys):
    fns = (tree or {}).get("functions") or {}
    if not keys or not all(k in fns for k in keys):
        return None
    self_ir, edges = collections.Counter(), set()
    for k in keys:
        self_ir.update(fns[k]["self"])
        edges.update(tuple(e) for e in fns[k]["edges"])
    return self_ir, edges


def short_fn(name):
    if len(name) > FN_NAME_MAX and name.endswith(")"):
        depth = 0
        for i in range(len(name) - 1, -1, -1):  # drop the parameter list
            depth += {")": 1, "(": -1}.get(name[i], 0)
            if depth == 0:
                name = name[:i] + "(…)"
                break
    return name if len(name) <= FN_NAME_MAX else name[:FN_NAME_MAX - 1] + "…"


def fn_diff(ref_tree, head_tree, metric, ref_role):
    """Functions whose self Ir changed most between the reference tree and head, and inlining suspects.

    A change is "suspected inlining" when, among the listed functions,
      - one exists on one side only and a caller or callee of it moved the other way by at least
        half as much (it was inlined into, or out of, that function), or
      - a caller/callee pair moves in opposite directions and the change of their sum is less
        than half of their combined movement (|d1 + d2| < (|d1| + |d2|) / 2).
    A function that is new or gone without such a partner is new or removed code, not inlining;
    one whose name survives with another parameter list on the other side changed its signature.
    Either rule only counts when the function (or the larger of the pair) moved at least
    FN_SUSPECT_MIN of the metric's whole change, so that a few stray instructions are not reported.
    """
    keys = metric_keys(metric, head_tree)
    ref, head = merged_profile(ref_tree, keys), merged_profile(head_tree, keys)
    if ref is None or head is None:
        return None
    (r, re_), (h, he) = ref, head
    delta = {f: h.get(f, 0) - r.get(f, 0) for f in set(r) | set(h)}
    floor = max(1, FN_SUSPECT_MIN * abs(sum(delta.values())))
    top = sorted((f for f, d in delta.items() if d), key=lambda f: (-abs(delta[f]), f))[:FN_TOP]
    edges = re_ | he
    partners = collections.defaultdict(set)
    for x, y in edges:
        partners[x].add(y)
        partners[y].add(x)
    bare = lambda f: f.split("(", 1)[0]  # noqa: E731 - name without the parameter list
    rows, suspects = [], []
    for f in top:
        only = "head" if not r.get(f) else (ref_role if not h.get(f) else None)
        rows.append({"function": f, "ref": r.get(f, 0), "head": h.get(f, 0), "delta": delta[f], "only": only})
        if not only or abs(delta[f]) < floor:
            continue
        other = r if only == "head" else h
        if any(bare(g) == bare(f) and g != f for g in other):
            continue  # same name, other parameters: a signature change
        moved = sorted(g for g in partners[f] if g != f and delta.get(g, 0) * delta[f] < 0
                       and abs(delta[g]) >= abs(delta[f]) / 2)
        if moved:
            suspects.append(f"`{short_fn(f)}` 只在 {only} 出现，`{short_fn(moved[0])}` 反向变化 {delta[moved[0]]:+,}")
    listed = set(top)
    for x, y in sorted(edges):
        if x != y and x in listed and y in listed:
            dx, dy = delta[x], delta[y]
            if dx * dy < 0 and abs(dx + dy) < (abs(dx) + abs(dy)) / 2 and max(abs(dx), abs(dy)) >= floor:
                suspects.append(f"`{short_fn(x)}` 调用 `{short_fn(y)}`，自身 Ir 一升一降（{dx:+,} / {dy:+,}）")
    return {"ref": ref_role, "keys": keys, "delta": sum(delta.values()), "top": rows,
            "inline_suspect": bool(suspects), "suspects": suspects}


def fmt_ratio(r):
    return "—" if r is None else f"{r:.4f}"


def cmd_compare(a):
    work = Path(a.work).resolve()
    plan = load_json(work / "plan.json")
    builds = load_json(work / f"build-{a.compiler}.json")
    ir = load_json(work / f"ir-{a.compiler}.json")
    roles = plan["roles"]
    head_sha = roles["head"]
    ledger_head = plan.get("ledger_head", {}).get("entries", [])
    base_ids = {e.get("id") for e in plan.get("ledger_base", {}).get("entries", [])}
    new_entries = [e for e in ledger_head if e.get("id") not in base_ids]

    verdict = "pass"
    notes, failures, warnings = [], [], []

    def fail(msg):
        nonlocal verdict
        verdict = "fail"
        failures.append(msg)

    def warn(msg):
        nonlocal verdict
        if verdict == "pass":
            verdict = "warn"
        warnings.append(msg)

    for p in validate_ledger(ledger_head):
        fail(p)
    head_tree = ir["trees"].get(head_sha)
    if head_tree is None or not head_tree["checks"]["ok"]:
        fail("head 的正确性检查失败，不出数字" if head_tree else "head 构建失败")
    for e in ir.get("errors", []):
        fail(f"callgrind 运行失败：{e.splitlines()[0]}")
    if head_tree and head_tree.get("reloc_status") == "verify_failed":
        fail("head 的 reloc_bench --mode verify 失败（各方结果摘要不一致），不出 reloc.* 的数字")
    for k, d in ir.get("determinism", {}).items():
        if not d["identical"]:
            fail(f"同一二进制两次 Ir 不同（{k}: {d['first']} vs {d['second']}），门禁结果不可信")

    status = {}
    for role in ("anchor", "base"):
        sha = roles[role]
        b = builds["trees"].get(sha, {}).get("status", "incomparable")
        t = ir["trees"].get(sha)
        if b == "incomparable":
            status[role] = "incomparable"
            warn(f"{role} {short(sha)} 用 head 的基准编不过，也没有自己的基准：不可比，合并前需要跑一次 Mac L2")
        elif t is None or not t["checks"]["ok"]:
            status[role] = "incomparable"
            warn(f"{role} {short(sha)} 正确性检查失败：不可比，合并前需要跑一次 Mac L2")
        else:
            status[role] = b
            if b == "bench_differs":
                warn(f"{role} {short(sha)} 用它自己那一版基准构建（基准不同），比值仅供参考")
            rs = t.get("reloc_status", "absent")
            if rs in ("failed", "verify_failed") and (head_tree or {}).get("reloc_status") == "ok":
                warn(f"{role} {short(sha)} 的 reloc_bench {'编不过' if rs == 'failed' else '校验失败'}：reloc.* 不和它比")

    metrics = {}
    for role in ROLES:
        t = ir["trees"].get(roles[role])
        metrics[role] = metrics_of(t) if t and t["checks"]["ok"] else {}
    hm, bm, am = metrics["head"], metrics["base"], metrics["anchor"]

    if head_tree and head_tree["checks"]["ok"]:
        hw = head_tree["checks"].get("wire_M_sha256")
        at = ir["trees"].get(roles["anchor"])
        aw = at["checks"].get("wire_M_sha256") if at else None
        if aw and hw and aw != hw:
            warn(f"M 输入 wire 变了（anchor {aw[:16]}… → head {hw[:16]}…）：格式改动要 susu 定")

    rows = []
    names = sorted(set(hm) | set(bm), key=lambda k: (k.split(".")[0], k.count("."), k))
    for metric in names:
        cls, share = classify(metric, bm, hm)
        h, b, an = hm.get(metric), bm.get(metric), am.get(metric)
        rb = h / b if (h is not None and b and status["base"] != "incomparable") else None
        ra = h / an if (h is not None and an and status["anchor"] != "incomparable") else None
        row = {"metric": metric, "class": cls, "share": round(share, 4), "anchor": an, "base": b, "head": h,
               "head_base": rb, "head_anchor": ra, "verdict": "pass", "ledger": []}
        if h is None:
            row["verdict"] = "missing"
            if cls != "report" and b is not None:
                warn(f"{metric}：head 没有这个阶段")
        elif b is None and status["base"] != "incomparable":
            row["verdict"] = "new"
        if rb is not None and cls != "report":
            warn_t, fail_t = (TOTAL_WARN, TOTAL_FAIL) if cls == "total" else (PHASE_WARN, PHASE_FAIL)
            if rb > fail_t:
                allow = ledger_matches(new_entries, metric, a.compiler)
                cap = max((float(e["max_ratio"]) for e in allow), default=None)
                row["trigger"] = "base"
                if cap is not None and rb <= cap:
                    row["verdict"] = "allowed"
                    row["ledger"] = [e["id"] for e in allow]
                    notes.append(f"{metric} head÷base {rb:.4f} 由 ledger {', '.join(row['ledger'])} 放行（上限 {cap}）")
                else:
                    row["verdict"] = "fail"
                    fail(f"{metric} head÷base {rb:.4f} > {fail_t}" + (f"（超过 ledger 上限 {cap}）" if cap else ""))
            elif rb > warn_t:
                row["trigger"] = "base"
                row["verdict"] = "warn"
                warn(f"{metric} head÷base {rb:.4f} > {warn_t}")
            if cls == "total" and rb < DROP_NOTE:
                notes.append(f"{metric} 指令数下降 {rb:.4f}：要 Mac 数据才能说变快")
        if ra is not None and cls == "total":
            allow = ledger_matches(ledger_head, metric, a.compiler)
            credit = 1.0
            for e in allow:
                credit *= float(e["max_ratio"])
            net = ra / credit
            row["head_anchor_net"] = net
            if net > ANCHOR_WARN:
                row.setdefault("trigger", "anchor")
            if net > ANCHOR_FAIL:
                row["verdict"] = "fail"
                fail(f"{metric} head÷anchor {ra:.4f}（扣除 ledger 后 {net:.4f}）> {ANCHOR_FAIL}")
            elif net > ANCHOR_WARN:
                if row["verdict"] == "pass":
                    row["verdict"] = "warn"
                warn(f"{metric} head÷anchor {ra:.4f}（扣除 ledger 后 {net:.4f}）> {ANCHOR_WARN}")
        rows.append(row)

    # Function-level difference for every metric that warns, fails or needs the ledger (design 5.3).
    for row in rows:
        ref_role = row.get("trigger")
        if not ref_role or row["verdict"] not in ("warn", "fail", "allowed"):
            continue
        d = fn_diff(ir["trees"].get(roles[ref_role]), head_tree, row["metric"], ref_role)
        if d is None:
            continue
        row["functions"] = d
        if d["inline_suspect"]:
            notes.append(f"{row['metric']}：疑似内联变化（只是诊断，不放行；见函数差异表）")

    # Largest-changing phase per scenario (head/base), for the summary.
    biggest = {}
    for row in rows:
        if row["class"] == "total" or row["head_base"] is None or row["metric"] == "formal.total":
            continue
        scen = (".".join(row["metric"].split(".")[:2]) if row["metric"].startswith("slg.")
                else row["metric"].split(".")[0])
        cur = biggest.get(scen)
        if cur is None or abs(row["head_base"] - 1) > abs(cur["head_base"] - 1):
            biggest[scen] = row

    out = {"schema": "xoffset-perf-gate-v1", "compiler": a.compiler, "compiler_version": ir["compiler_version"],
           "valgrind": ir["valgrind"], "verdict": verdict, "roles": roles, "status": status,
           "slg": {"scale": SLG_SCALE, "cycles": int(SLG_CYCLES), "skip_cycles": int(SLG_SKIP_CYCLES),
                   "seed": int(SLG_SEED), "protocol": "reuse"},
           "formal": {"fixture": "M", "input_digest": ir["fixture_input_digest"]},
           "reloc": {"rounds": int(RELOC_ROUNDS), "warmup": int(RELOC_WARMUP), "skip_rounds": int(RELOC_SKIP_ROUNDS),
                     "status": {sha: t.get("reloc_status", "absent") for sha, t in ir["trees"].items()}},
           "checks": {sha: t["checks"] for sha, t in ir["trees"].items()},
           "determinism": ir.get("determinism", {}), "failures": failures, "warnings": warnings, "notes": notes,
           "ledger_applied": sorted({i for r in rows for i in r["ledger"]}), "rows": rows,
           "first_line": FIRST_LINE}
    json_path = Path(a.json) if a.json else work / f"perf-gate-{a.compiler}-{short(head_sha)}.json"
    dump_json(json_path, out)
    md = render_summary(out, plan, biggest)
    if a.summary:
        with open(a.summary, "a", encoding="utf-8") as f:
            f.write(md)
    # Also in the job log, with every row's raw counts, since artifacts may be out of reach.
    print(md)
    for row in rows:
        print(f"IR {a.compiler} {row['metric']} anchor={row['anchor']} base={row['base']} head={row['head']} "
              f"class={row['class']} verdict={row['verdict']}")
    log(f"verdict {verdict}: {len(failures)} failures, {len(warnings)} warnings -> {json_path}")
    return 1 if verdict == "fail" else 0


def render_summary(out, plan, biggest):
    r = out["roles"]
    icon = {"pass": "✅ 通过", "warn": "⚠️ 通过（有警告）", "fail": "❌ 失败"}[out["verdict"]]
    L = [FIRST_LINE, "", f"## 指令数门禁（{out['compiler']}）：{icon}", ""]
    L.append(f"anchor `{short(r['anchor'])}` · base `{short(r['base'])}` · head `{short(r['head'])}` · "
             f"{out['compiler_version']} · {out['valgrind']}")
    L.append(f"SLG {out['slg']['scale']} 规模、{out['slg']['cycles']} 周期（跳过前 {out['slg']['skip_cycles']} 个）、"
             f"seed {out['slg']['seed']}、复用协议；正式 harness M fixture；"
             f"reloc 每项 {out['reloc']['rounds']} 轮（热身 {out['reloc']['warmup']}，收第 {out['reloc']['skip_rounds']} 轮起）")
    L.append("")
    for title, items in (("失败", out["failures"]), ("警告", out["warnings"]), ("说明", out["notes"])):
        if items:
            L.append(f"**{title}**")
            L += [f"- {x}" for x in items]
            L.append("")
    L.append("| 指标 | 类别 | head÷base | head÷anchor | head Ir | 判定 |")
    L.append("| --- | --- | --- | --- | --- | --- |")
    for row in out["rows"]:
        if row["class"] == "report" and row["verdict"] in ("pass", "new"):
            continue
        L.append(f"| `{row['metric']}` | {row['class']} | {fmt_ratio(row['head_base'])} | {fmt_ratio(row['head_anchor'])} | "
                 f"{row['head'] if row['head'] is not None else '—'} | {row['verdict']} |")
    if biggest:
        L += ["", "**各场景变化最大的阶段（head÷base）**", ""]
        for scen, row in sorted(biggest.items()):
            L.append(f"- {scen}: `{row['metric']}` {row['head_base']:.4f}")
    shown = {}
    for row in out["rows"]:
        d = row.get("functions")
        if not d:
            continue
        if not shown:
            L += ["", "**函数差异**（自身 Ir，即 `callgrind_annotate --inclusive=no`，变化最大的 "
                  f"{FN_TOP} 个）", ""]
        ident = (d["ref"], tuple(d["keys"]))
        ratio = row["head_base"] if d["ref"] == "base" else row["head_anchor"]
        head = f"`{row['metric']}`（head÷{d['ref']} {fmt_ratio(ratio)}，{d['delta']:+,}）"
        if ident in shown:
            L += [f"{head}：同 `{shown[ident]}`", ""]
            continue
        shown[ident] = row["metric"]
        L.append(head + ("：⚠️ 疑似内联变化（只是诊断，不放行）" if d["inline_suspect"] else ""))
        L += [f"- {x}" for x in d["suspects"]]
        L += ["", f"| 函数 | {d['ref']} | head | 变化 |", "| --- | --- | --- | --- |"]
        for t in d["top"]:
            L.append(f"| `{short_fn(t['function'])}` | {t['ref']:,} | {t['head']:,} | {t['delta']:+,} |")
        L.append("")
    L += ["", "占比 < 5% 的阶段只在 JSON 里报。门槛和 ledger 见 `docs/PERF_GATE.md`。", ""]
    return "\n".join(L)


# ---------------------------------------------------------------- main

def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    def common(sp, compiler=True):
        sp.add_argument("--work", required=True)
        if compiler:
            sp.add_argument("--compiler", required=True, choices=("gcc16", "p2996"))

    sp = sub.add_parser("prepare")
    common(sp, False)
    sp.add_argument("--repo", default=".")
    sp.add_argument("--anchor", default="", help="default: the head's bench/perf/ANCHOR")
    sp.add_argument("--base", required=True)
    sp.add_argument("--head", default="HEAD")
    sp.add_argument("--bench-root", default="",
                    help="benchmark sources and default ANCHOR (default: the head tree); replays pass the checkout")
    sp.add_argument("--slg-dir", default=SLG_DIR)
    sp.add_argument("--formal-dir", default=FORMAL_DIR)
    sp.add_argument("--reloc-dir", default=RELOC_DIR)
    sp.add_argument("--ledger", default="", help="override the head's ledger file")

    for name in ("build", "measure", "run"):
        sp = sub.add_parser(name)
        common(sp)
        sp.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
        if name in ("build", "run"):
            sp.add_argument("--cxx", default="")
            sp.add_argument("--cereal", required=True, help="cereal include directory")
            sp.add_argument("--extra-flags", default="")
        if name in ("measure", "run"):
            sp.add_argument("--valgrind", default="valgrind")
            sp.add_argument("--python", default=sys.executable)
        if name == "run":
            sp.add_argument("--json", default="")
            sp.add_argument("--summary", default=os.environ.get("GITHUB_STEP_SUMMARY", ""))

    sp = sub.add_parser("compare")
    common(sp)
    sp.add_argument("--json", default="")
    sp.add_argument("--summary", default=os.environ.get("GITHUB_STEP_SUMMARY", ""))

    a = p.parse_args()
    if a.cmd == "prepare":
        return cmd_prepare(a) or 0
    if a.cmd == "build":
        return cmd_build(a)
    if a.cmd == "measure":
        return cmd_measure(a)
    if a.cmd == "compare":
        return cmd_compare(a)
    if a.cmd == "run":
        rc = cmd_build(a)
        if rc:
            return rc
        cmd_measure(a)
        return cmd_compare(a)
    return 2


if __name__ == "__main__":
    sys.exit(main())
