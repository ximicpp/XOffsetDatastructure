#!/usr/bin/env python3
"""Instruction-count (callgrind Ir) performance regression gate (H3 L0).

Builds three copies of the benchmarks in one job -- anchor (bench/perf/ANCHOR),
base and head -- all from the head's benchmark sources, counts the instructions
of every timed phase of the SLG benchmark (S scale) and the formal acceptance
harness (M fixture) under callgrind, and judges head/base and head/anchor
against the thresholds in docs/PERF_GATE.md.

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
ANCHOR_FILE = "bench/perf/ANCHOR"
LEDGER_FILE = "bench/perf/ledger.json"
ROLES = ("anchor", "base", "head")

SLG_SEED = "20261021"
SLG_SCALE = "S"
SLG_CYCLES = "8"
SLG_SKIP_CYCLES = "4"
FORMAL_PHASES = ("load", "query_before", "mutate", "emit_after", "retire", "reload", "query_after", "cleanup")

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
            "slg_dir": a.slg_dir, "formal_dir": a.formal_dir,
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
        (work / "logs").mkdir(exist_ok=True)
        (work / "logs" / f"build-{a.compiler}-{short(sha)}.log").write_text(text, encoding="utf-8")
        log(f"build {a.compiler} {short(sha)}: {status}")
        return sha, {"status": status, "bench": bench, "dir": str(outdir)}

    with concurrent.futures.ThreadPoolExecutor(max(1, min(3, a.jobs // 2))) as ex:
        for sha, res in ex.map(one, list(plan["trees"])):
            builds["trees"][sha] = res
    dump_json(work / f"build-{a.compiler}.json", builds)
    head = plan["roles"]["head"]
    if builds["trees"][head]["status"] != "ok":
        log("head does not build; see logs/")
        tail = (work / "logs" / f"build-{a.compiler}-{short(head)}.log").read_text(encoding="utf-8")[-4000:]
        print(tail, file=sys.stderr)
        return 1
    return 0


# ---------------------------------------------------------------- measure

COLLECTED = re.compile(r"Collected\s*:\s*(\d+)")


def callgrind(valgrind, cmd, env, tmp: Path):
    """Run `cmd` under callgrind with collection off at start; return the collected Ir."""
    logf = tmp / "vg.log"
    full = shlex.split(valgrind) + ["--tool=callgrind", "--collect-atstart=no",
                                    f"--callgrind-out-file={tmp / 'cg.out'}", f"--log-file={logf}"] + cmd
    r = subprocess.run(full, env=env, text=True, capture_output=True, cwd=tmp)
    text = logf.read_text(encoding="utf-8", errors="replace") if logf.exists() else ""
    m = COLLECTED.search(text)
    if r.returncode != 0 or not m:
        raise RuntimeError(f"callgrind failed ({r.returncode}): {shlex.join(cmd)}\n{r.stderr[-2000:]}\n{text[-2000:]}")
    return int(m.group(1))


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
        result["trees"][sha] = {"checks": chk, "slg_keys": keys, "slg": {}, "formal": {}}
        log(f"checks {short(sha)}: {'ok' if chk['ok'] else 'FAILED'}; {len(keys)} SLG keys")

    tasks = []
    for sha, tr in result["trees"].items():
        if not tr["checks"]["ok"]:
            continue
        bdir = Path(usable[sha]["dir"])
        for k in tr["slg_keys"]:
            tasks.append((sha, "slg", k, 0))
        for ph in FORMAL_PHASES:
            tasks.append((sha, "formal", ph, 0))
    # Determinism: the same binary twice must give bit-identical counts.
    if head in result["trees"] and result["trees"][head]["checks"]["ok"]:
        keys = result["trees"][head]["slg_keys"]
        probe = "A.apply" if "A.apply" in keys else (keys[0] if keys else None)
        if probe:
            tasks.append((head, "slg", probe, 1))
        tasks.append((head, "formal", "mutate", 1))

    def run(task):
        sha, kind, key, rep = task
        bdir = Path(usable[sha]["dir"])
        tmp = Path(tempfile.mkdtemp(prefix=f"{kind}-", dir=scratch))
        try:
            if kind == "slg":
                ir = callgrind(a.valgrind, slg_bench_cmd(bdir / "slg_bench", tmp / "raw.jsonl"),
                               slg_env(SLG_IR_KEYS=key, SLG_IR_SKIP_CYCLES=SLG_SKIP_CYCLES), tmp)
            else:
                env = dict(os.environ, SC_IR_PHASE=key)
                ir = callgrind(a.valgrind, [str(bdir / "sc_xoffset"), "run", "--wire", str(bdir / "M.wire"),
                                            "--trace", str(fx / "M" / "trace.bin"), "--wire-out", str(tmp / "w"),
                                            "--canonical-out", str(tmp / "c"), "--queries-out", str(tmp / "q"),
                                            "--result", str(tmp / "r.json")], env, tmp)
            return task, ir, None
        except Exception as e:  # noqa: BLE001 - reported per task
            return task, None, str(e)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    log(f"{len(tasks)} callgrind runs on {a.jobs} workers")
    errors = []
    repeats = {}
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        for (sha, kind, key, rep), ir, err in ex.map(run, tasks):
            if err:
                errors.append(f"{short(sha)} {kind} {key}: {err}")
                continue
            if rep:
                repeats[f"{kind}.{key}"] = ir
            else:
                result["trees"][sha][kind][key] = ir
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
    return m


def classify(metric, base_m, head_m):
    """total | phase | report, plus the phase's share of its scenario."""
    kind, _, rest = metric.partition(".")
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
                if cap is not None and rb <= cap:
                    row["verdict"] = "allowed"
                    row["ledger"] = [e["id"] for e in allow]
                    notes.append(f"{metric} head÷base {rb:.4f} 由 ledger {', '.join(row['ledger'])} 放行（上限 {cap}）")
                else:
                    row["verdict"] = "fail"
                    fail(f"{metric} head÷base {rb:.4f} > {fail_t}" + (f"（超过 ledger 上限 {cap}）" if cap else ""))
            elif rb > warn_t:
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
            if net > ANCHOR_FAIL:
                row["verdict"] = "fail"
                fail(f"{metric} head÷anchor {ra:.4f}（扣除 ledger 后 {net:.4f}）> {ANCHOR_FAIL}")
            elif net > ANCHOR_WARN:
                if row["verdict"] == "pass":
                    row["verdict"] = "warn"
                warn(f"{metric} head÷anchor {ra:.4f}（扣除 ledger 后 {net:.4f}）> {ANCHOR_WARN}")
        rows.append(row)

    # Largest-changing phase per scenario (head/base), for the summary.
    biggest = {}
    for row in rows:
        if row["class"] == "total" or row["head_base"] is None or row["metric"] == "formal.total":
            continue
        scen = ".".join(row["metric"].split(".")[:2]) if row["metric"].startswith("slg.") else "formal"
        cur = biggest.get(scen)
        if cur is None or abs(row["head_base"] - 1) > abs(cur["head_base"] - 1):
            biggest[scen] = row

    out = {"schema": "xoffset-perf-gate-v1", "compiler": a.compiler, "compiler_version": ir["compiler_version"],
           "valgrind": ir["valgrind"], "verdict": verdict, "roles": roles, "status": status,
           "slg": {"scale": SLG_SCALE, "cycles": int(SLG_CYCLES), "skip_cycles": int(SLG_SKIP_CYCLES),
                   "seed": int(SLG_SEED), "protocol": "reuse"},
           "formal": {"fixture": "M", "input_digest": ir["fixture_input_digest"]},
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
             f"seed {out['slg']['seed']}、复用协议；正式 harness M fixture")
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
