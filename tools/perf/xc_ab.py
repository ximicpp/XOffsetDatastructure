#!/usr/bin/env python3
"""Paired wall-clock runs of two benchmark binaries, and their statistics.

The project's one tool for paired wall-clock timing (H1 GCC 16 vs clang-p2996,
the Mac perf gate's SLG part, any head-vs-anchor run). Wall-clock conclusions
come only from quiet machines (the Mac); cloud CI gating uses instruction
counts (tools/perf_gate/), not this tool.

Two binaries A (reference: clang, anchor) and B (subject: GCC, head) run as
separate processes in an interleaved order. Every process writes its own raw
file (slg_bench --mode bench --out ...). Ratios are always B / A.

Protocols
  xcomp  H1 design section 5.2: A A* | A B B A x ceil(n/4) | A A* | A B B A x floor(n/4) | A A*
         (A* = byte-identical copy of A at another path, the A/A control). Adjacent
         A,B runs form a pair; n = number of A/B pairs.
  abba   A B B A repeated `blocks` times (the Mac gate: blocks=3 -> 6 processes per side),
         optionally with A A* control blocks at the start/middle/end.

Library use (import xc_ab):
  xcomp(pairs, aa_pairs=3), abba(blocks, aa=None, aa_at=()), adjacent_pairs(labels, x, y)
  CpuSampler, quiet_gate(...), run_schedule(schedule, argv_by_label, env, outdir, gate)
  read_slg_raw(path) -> ns[format][metric][cycle]  (meta line skipped, cycle 0 dropped,
                        scenario totals "A.total".."D.total" added as in tools/analyze.py)
  t_ppf, paired_log_ratio, welch_log_ratio, drift, norm_log_mean
  analyze_xcomp(outdir_or_runs, ...) -> dict (Q2, Q1, A/A, drift, wire-bytes check)

CLI
  xc_ab.py schedule --protocol xcomp --pairs 10
  xc_ab.py run --protocol xcomp --a BIN_A --b BIN_B --aa-copy PATH --pairs 10 --outdir D \
              [--pin-cpu 2] [--env SLG_REUSE=1] [--prefix "docker exec ctr"] [--path-map H=C] \
              [-- slg_bench args, default: --mode bench --scale M --cycles 41]
  xc_ab.py analyze --outdir D [--a-name clang --b-name gcc] [--aa-range 0.015 --drift-range 0.02]
  xc_ab.py verify --bin A --bin B [--scales S,M] [--protocols reuse,per_message]
  xc_ab.py aa-budget --outdir D [--budget-min 20]
  xc_ab.py sample --out cpu.jsonl --duration 600     (host-side CPU log, e.g. around a VM run)
  xc_ab.py freeze --out H1_FREEZE.json --lib-commit C --bin gcc=... --pairs 16 ...   (H1 section 5.7)
  xc_ab.py compare-cc --a-build B1 --b-build B2 [--source bench/slg_bench.cpp]       (H1 section 4.3)
"""
import argparse, hashlib, json, math, os, platform, re, shlex, shutil, statistics, subprocess, sys, threading, time
from collections import defaultdict

SCENARIOS = ("A", "B", "C", "D")
SCENARIO_NAMES = {"A": "战斗服往返", "B": "跨服迁移", "C": "分区快照", "D": "战报只读"}
DEFAULT_BASELINES = ("cereal", "flatbuffers", "protobuf")
DEFAULT_BENCH_ARGS = ["--mode", "bench", "--scale", "M", "--cycles", "41"]
COMPILER_NAMES = ("cc1plus", "cc1", "clang", "g++", "gcc", "c++", "ld", "ld64", "lld", "ld.lld", "collect2",
                  "ninja", "make", "gmake", "cmake", "swift-frontend", "swiftc", "rustc", "cargo", "xcodebuild")


# ---------------------------------------------------------------- statistics
# Student t without scipy (same implementation as bench/slg/tools/analyze.py).

def _betacf(a, b, x):
    m, eps, fpmin = 200, 3e-14, 1e-300
    qab, qap, qam = a + b, a + 1.0, a - 1.0
    c, d = 1.0, 1.0 - qab * x / qap
    d = 1.0 / (d if abs(d) > fpmin else fpmin)
    h = d
    for i in range(1, m + 1):
        m2 = 2 * i
        aa = i * (b - i) * x / ((qam + m2) * (a + m2))
        d = 1.0 + aa * d; d = 1.0 / (d if abs(d) > fpmin else fpmin)
        c = 1.0 + aa / c if abs(1.0 + aa / c) > fpmin else fpmin
        h *= d * c
        aa = -(a + i) * (qab + i) * x / ((a + m2) * (qap + m2))
        d = 1.0 + aa * d; d = 1.0 / (d if abs(d) > fpmin else fpmin)
        c = 1.0 + aa / c if abs(1.0 + aa / c) > fpmin else fpmin
        de = d * c
        h *= de
        if abs(de - 1.0) < eps:
            break
    return h


def _betainc(a, b, x):
    if x <= 0: return 0.0
    if x >= 1: return 1.0
    lbt = math.lgamma(a + b) - math.lgamma(a) - math.lgamma(b) + a * math.log(x) + b * math.log(1 - x)
    if x < (a + 1) / (a + b + 2):
        return math.exp(lbt) * _betacf(a, b, x) / a
    return 1.0 - math.exp(lbt) * _betacf(b, a, 1 - x) / b


def t_cdf(t, df):
    x = df / (df + t * t)
    p = 0.5 * _betainc(df / 2.0, 0.5, x)
    return 1 - p if t > 0 else p


def t_ppf(q, df):
    """Quantile of Student t with (possibly fractional) df."""
    lo, hi = -1e3, 1e3
    for _ in range(200):
        mid = (lo + hi) / 2
        if t_cdf(mid, df) < q: lo = mid
        else: hi = mid
    return (lo + hi) / 2


def _interval(mean, se, df, alpha, k, sided):
    """Ratio-space interval for a log-scale mean. sided: 'two', 'upper' (one-sided UB), 'lower'."""
    out = {"ratio": math.exp(mean), "lo": None, "hi": None, "df": df, "alpha": alpha, "k": k, "sided": sided}
    if df is None or df <= 0 or se is None or not math.isfinite(se):
        return out
    q = 1 - alpha / (2 * k) if sided == "two" else 1 - alpha / k
    t = t_ppf(q, df)
    if sided in ("two", "lower"): out["lo"] = math.exp(mean - t * se)
    if sided in ("two", "upper"): out["hi"] = math.exp(mean + t * se)
    return out


def paired_log_ratio(pairs, alpha=0.05, k=1, sided="two"):
    """pairs: [(num, den), ...] positive values. Geometric mean of num/den with a t interval on logs."""
    lr = [math.log(n / d) for n, d in pairs if n > 0 and d > 0]
    n = len(lr)
    if n == 0:
        return {"n": 0, "ratio": None, "lo": None, "hi": None, "sd_log": None}
    mean = statistics.mean(lr)
    sd = statistics.stdev(lr) if n > 1 else None
    r = _interval(mean, sd / math.sqrt(n) if sd is not None else None, n - 1, alpha, k, sided)
    r.update(n=n, sd_log=sd)
    return r


def welch_log_ratio(xs, ys, alpha=0.05, k=1, sided="two", logs=False):
    """exp(mean(log xs) - mean(log ys)) with a Welch t interval (Welch-Satterthwaite df).
    xs: subject group (e.g. head), ys: reference group (anchor). logs=True: inputs are already logs."""
    lx = list(xs) if logs else [math.log(v) for v in xs]
    ly = list(ys) if logs else [math.log(v) for v in ys]
    nx, ny = len(lx), len(ly)
    if nx < 2 or ny < 2:
        return {"n": (nx, ny), "ratio": math.exp(statistics.mean(lx) - statistics.mean(ly)) if nx and ny else None,
                "lo": None, "hi": None, "sd_log": None}
    vx, vy = statistics.variance(lx), statistics.variance(ly)
    se2 = vx / nx + vy / ny
    df = se2 * se2 / ((vx / nx) ** 2 / (nx - 1) + (vy / ny) ** 2 / (ny - 1)) if se2 > 0 else nx + ny - 2
    r = _interval(statistics.mean(lx) - statistics.mean(ly), math.sqrt(se2), df, alpha, k, sided)
    r.update(n=(nx, ny), sd_log=(math.sqrt(vx), math.sqrt(vy)))
    return r


def drift(first, last, logs=False):
    """Ratio of the geometric means last / first (values of the same binary early and late in a batch)."""
    lf = list(first) if logs else [math.log(v) for v in first]
    ll = list(last) if logs else [math.log(v) for v in last]
    return math.exp(statistics.mean(ll) - statistics.mean(lf))


# ---------------------------------------------------------------- raw files

def read_slg_raw(path, with_meta=False, drop_warmup=True, totals=True):
    """slg_bench --mode bench output -> ns[format][metric][cycle] (nanoseconds per cycle, whole batch).
    with_meta=True returns (meta, ns, bytes). Scenario totals follow tools/analyze.py: X.total is the
    sum of the X.* stages, excluding *.read_direct."""
    ns = defaultdict(lambda: defaultdict(dict))
    by = defaultdict(lambda: defaultdict(dict))
    meta = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line: continue
            r = json.loads(line)
            if "meta" in r: meta = r["meta"]; continue
            if drop_warmup and r["cycle"] == 0: continue
            if "ns" in r: ns[r["format"]][r["metric"]][r["cycle"]] = r["ns"]
            else: by[r["format"]][r["metric"]][r["cycle"]] = r["bytes"]
    if totals:
        for fmt in list(ns):
            for s in SCENARIOS:
                stages = [m for m in ns[fmt] if m.startswith(s + ".") and m != s + ".total" and not m.endswith("read_direct")]
                if not stages: continue
                cycles = set().union(*(set(ns[fmt][m]) for m in stages))
                ns[fmt][s + ".total"] = {c: sum(ns[fmt][m].get(c, 0) for m in stages) for c in cycles}
    return (meta, ns, by) if with_meta else ns


def run_medians(ns):
    """Per format and metric, the median over cycles (H1 section 5.4: one value per run)."""
    return {f: {m: statistics.median(v.values()) for m, v in ms.items() if v} for f, ms in ns.items()}


def norm_log_mean(ns, subject="xoffset", baselines=DEFAULT_BASELINES, scenario="A"):
    """Mean over cycles of log(subject scenario total / geometric mean of the baselines' totals in
    the same cycle). The per-process value of the Mac gate's SLG part (H3 design section 7.4)."""
    key = scenario + ".total"
    x = ns[subject][key]
    ys = [ns[b][key] for b in baselines]
    cs = sorted(set(x).intersection(*(set(y) for y in ys)))
    vals = [math.log(x[c]) - statistics.mean(math.log(y[c]) for y in ys) for c in cs]
    return statistics.mean(vals) if vals else None


def per_cycle_ratio(ns, subject, baseline, key, alpha=0.05, k=12, sided="upper"):
    """tools/analyze.py's paired per-cycle ratio subject/baseline (one-sided UB by default)."""
    x, y = ns[subject].get(key, {}), ns[baseline].get(key, {})
    return paired_log_ratio([(x[c], y[c]) for c in sorted(set(x) & set(y))], alpha, k, sided)


# ---------------------------------------------------------------- schedules

def abba(blocks, a="A", b="B", aa=None, aa_at=()):
    """A B B A x blocks. aa: label of the A/A copy; aa_at: any of 'start', 'middle', 'end'
    to insert an [a, aa] control block there."""
    body = [[a, b, b, a] for _ in range(blocks)]
    out = []
    if aa and "start" in aa_at: out += [a, aa]
    for i, blk in enumerate(body):
        if aa and "middle" in aa_at and i == (blocks + 1) // 2: out += [a, aa]
        out += blk
    if aa and "end" in aa_at: out += [a, aa]
    return out


def xcomp(pairs, aa_pairs=3, a="A", b="B", aa="A*"):
    """H1 section 5.2. pairs = number of A/B pairs (each A B B A block gives two); aa_pairs in {0,1,2,3}."""
    nblk = (pairs + 1) // 2
    first, second = (nblk + 1) // 2, nblk // 2
    ctl = [a, aa]
    out = []
    if aa_pairs >= 1: out += ctl
    out += [a, b, b, a] * first
    if aa_pairs >= 3: out += ctl
    out += [a, b, b, a] * second
    if aa_pairs >= 2: out += ctl
    return out


def adjacent_pairs(labels, x, y):
    """Non-overlapping adjacent (index_of_x, index_of_y) pairs, scanning left to right."""
    out, i = [], 0
    while i + 1 < len(labels):
        p, q = labels[i], labels[i + 1]
        if p == x and q == y: out.append((i, i + 1)); i += 2
        elif p == y and q == x: out.append((i + 1, i)); i += 2
        else: i += 1
    return out


# ---------------------------------------------------------------- machine state

def _is_compiler(comm):
    base = os.path.basename(comm.strip())
    return base.startswith("clang") or base in COMPILER_NAMES


class CpuSampler(threading.Thread):
    """Every `interval` s, log 1-minute load, processes at >= threshold % of one CPU, and whether any
    compiler/linker/build tool is running. Linux: deltas of /proc/<pid>/stat; elsewhere: ps."""

    def __init__(self, out_path=None, interval=2.0, threshold=30.0):
        super().__init__(daemon=True)
        self.interval, self.threshold, self.out_path = interval, threshold, out_path
        self.started_t = time.time()
        self.last_compile_t = None
        self.last = None
        self._stop = threading.Event()
        self._prev = {}
        self._linux = os.path.isdir("/proc/self") and platform.system() == "Linux"
        self._hz = os.sysconf("SC_CLK_TCK") if self._linux else 100

    def _procs_linux(self, dt):
        cur, out = {}, []
        for pid in os.listdir("/proc"):
            if not pid.isdigit(): continue
            try:
                with open(f"/proc/{pid}/stat") as f: s = f.read()
            except OSError:
                continue
            r = s.rfind(")")
            comm = s[s.find("(") + 1:r]
            fields = s[r + 2:].split()
            ticks = int(fields[11]) + int(fields[12])
            cur[pid] = ticks
            if pid in self._prev and dt > 0:
                pc = 100.0 * (ticks - self._prev[pid]) / self._hz / dt
                out.append((int(pid), pc, comm))
            else:
                out.append((int(pid), 0.0, comm))
        self._prev = cur
        return out

    def _procs_ps(self):
        try:
            txt = subprocess.run(["ps", "-A", "-o", "pid=,pcpu=,comm="], capture_output=True, text=True, timeout=10).stdout
        except Exception:
            return []
        out = []
        for line in txt.splitlines():
            parts = line.split(None, 2)
            if len(parts) == 3:
                try: out.append((int(parts[0]), float(parts[1]), parts[2]))
                except ValueError: pass
        return out

    def sample(self, dt):
        procs = self._procs_linux(dt) if self._linux else self._procs_ps()
        now = time.time()
        busy = [{"pid": p, "pcpu": round(c, 1), "comm": n} for p, c, n in procs if c >= self.threshold]
        comp = sorted({os.path.basename(n) for p, c, n in procs if _is_compiler(n) and p != os.getpid()})
        if comp: self.last_compile_t = now
        rec = {"t": round(now, 3), "load1": round(os.getloadavg()[0], 2), "busy": busy, "compilers": comp}
        self.last = rec
        return rec

    def run(self):
        f = open(self.out_path, "a") if self.out_path else None
        t_prev = time.time()
        self.sample(0)
        while not self._stop.wait(self.interval):
            t = time.time()
            rec = self.sample(t - t_prev)
            t_prev = t
            if f: f.write(json.dumps(rec) + "\n"); f.flush()
        if f: f.close()

    def stop(self):
        self._stop.set()


class GateTimeout(RuntimeError):
    pass


def quiet_gate(sampler, load_max=3.0, quiet_s=180.0, timeout_s=1800.0, poll_s=5.0, log=None):
    """Returns gate(): blocks until the sampler has seen no compiler for quiet_s seconds (and has been
    running that long) and the 1-minute load is < load_max; raises GateTimeout after timeout_s."""
    def gate():
        t0 = time.time()
        while True:
            now = time.time()
            since = now - max(sampler.started_t, sampler.last_compile_t or 0)
            load1 = os.getloadavg()[0]
            if since >= quiet_s and load1 < load_max:
                return {"waited_s": round(now - t0, 1), "load1": round(load1, 2), "quiet_s": round(since, 1)}
            if now - t0 > timeout_s:
                raise GateTimeout(f"not quiet after {timeout_s:.0f} s: load1={load1:.2f}, "
                                  f"no-compile for {since:.0f} s (need {quiet_s:.0f}); last sample {sampler.last}")
            if log: log(f"gate: waiting (load1={load1:.2f}, quiet {since:.0f}/{quiet_s:.0f} s)")
            time.sleep(poll_s)
    return gate


def command_gate(cmd, inner=None, timeout_s=1800.0, poll_s=10.0, log=None):
    """Gate that first runs `inner` (if any), then the shell command `cmd` until it exits 0
    (e.g. a check of the VM's or the host's load); GateTimeout after timeout_s."""
    def gate():
        info = inner() if inner else {}
        t0 = time.time()
        while True:
            p = subprocess.run(cmd, shell=True, capture_output=True, text=True)
            if p.returncode == 0:
                info = dict(info or {}); info["gate_cmd_waited_s"] = round(time.time() - t0, 1)
                return info
            if time.time() - t0 > timeout_s:
                raise GateTimeout(f"gate command still failing after {timeout_s:.0f} s: {cmd}: {p.stdout.strip()} {p.stderr.strip()}")
            if log: log(f"gate: '{cmd}' exited {p.returncode}, waiting")
            time.sleep(poll_s)
    return gate


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""): h.update(chunk)
    return h.hexdigest()


def make_aa_copy(src, dst):
    """Byte-identical copy of a binary at another path (the A/A control). Returns its sha256."""
    os.makedirs(os.path.dirname(os.path.abspath(dst)), exist_ok=True)
    shutil.copy2(src, dst)
    a, b = sha256_file(src), sha256_file(dst)
    if a != b: raise RuntimeError(f"A/A copy differs: {src} {a} vs {dst} {b}")
    return b


def machine_info():
    info = {"platform": platform.platform(), "machine": platform.machine(), "python": platform.python_version(),
            "cpus": os.cpu_count(), "load": os.getloadavg()}
    for cmd in (["lscpu"], ["sysctl", "-n", "machdep.cpu.brand_string"], ["uname", "-a"]):
        if shutil.which(cmd[0]):
            try: info[cmd[0]] = subprocess.run(cmd, capture_output=True, text=True, timeout=10).stdout.strip()
            except Exception: pass
    return info


# ---------------------------------------------------------------- running

def _safe(label):
    return re.sub(r"[^A-Za-z0-9_.-]", "x", label.replace("*", "aa"))


def run_schedule(schedule, argv_by_label, env=None, outdir=".", gate=None, pin_cpu=None, path_map=None,
                 binaries=None, log=print):
    """Run each label of `schedule` as one process, in order. argv_by_label[label] is the full argv; the
    token '{out}' is replaced by the raw-file path (else '--out <path>' is appended). path_map=(host, inside)
    rewrites that path for wrapped runs (docker exec ...). gate() is called before every run; GateTimeout
    stops the schedule (nothing more is timed). A non-zero exit raises. Returns the list of run records,
    also written to outdir/runs.json after every run."""
    os.makedirs(os.path.join(outdir, "raw"), exist_ok=True)
    os.makedirs(os.path.join(outdir, "log"), exist_ok=True)
    full_env = dict(os.environ); full_env.update(env or {})
    runs_path = os.path.join(outdir, "runs.json")
    doc = {"schedule": list(schedule), "env": env or {}, "pin_cpu": pin_cpu, "argv_by_label": argv_by_label,
           "binaries": {l: {"path": p, "sha256": sha256_file(p)} for l, p in (binaries or {}).items()},
           "machine": machine_info(), "started": time.time(), "aborted": None, "runs": []}
    prefix = []
    if pin_cpu is not None:
        if not shutil.which("taskset"): raise RuntimeError("pin_cpu needs taskset (Linux)")
        prefix = ["taskset", "-c", str(pin_cpu)]

    def save():
        with open(runs_path + ".tmp", "w") as f: json.dump(doc, f, indent=1)
        os.replace(runs_path + ".tmp", runs_path)

    save()
    for idx, label in enumerate(schedule):
        raw = os.path.abspath(os.path.join(outdir, "raw", f"{idx:03d}_{_safe(label)}.jsonl"))
        raw_arg = raw.replace(path_map[0], path_map[1], 1) if path_map else raw
        argv = [s.replace("{out}", raw_arg) for s in argv_by_label[label]]
        if not any("{out}" in s for s in argv_by_label[label]): argv += ["--out", raw_arg]
        g = None
        if gate:
            try:
                g = gate()
            except GateTimeout as e:
                doc["aborted"] = {"index": idx, "reason": str(e)}
                save()
                log(f"ABORTED before run {idx} ({label}): {e}")
                return doc["runs"]
        logf = os.path.join(outdir, "log", f"{idx:03d}_{_safe(label)}.log")
        load_before = os.getloadavg()
        t0 = time.time()
        with open(logf, "w") as lf:
            rc = subprocess.run(prefix + argv, env=full_env, stdout=lf, stderr=subprocess.STDOUT).returncode
        t1 = time.time()
        rec = {"index": idx, "label": label, "raw": os.path.relpath(raw, outdir), "log": os.path.relpath(logf, outdir),
               "argv": prefix + argv, "start": round(t0, 3), "end": round(t1, 3), "wall_s": round(t1 - t0, 3),
               "load_before": load_before, "gate": g, "returncode": rc}
        doc["runs"].append(rec)
        save()
        log(f"run {idx + 1}/{len(schedule)} {label}: {t1 - t0:.1f} s rc={rc}")
        if rc != 0:
            raise RuntimeError(f"run {idx} ({label}) exited {rc}; see {logf}")
    doc["finished"] = time.time()
    save()
    return doc["runs"]


# ---------------------------------------------------------------- analysis

def load_runs(outdir):
    with open(os.path.join(outdir, "runs.json")) as f: doc = json.load(f)
    for r in doc["runs"]:
        meta, ns, by = read_slg_raw(os.path.join(outdir, r["raw"]), with_meta=True)
        r["_meta"], r["_ns"], r["_by"] = meta, ns, by
        r["_med"] = run_medians(ns)
    return doc


def _verdict_q2(r):
    if r["ratio"] is None or r["lo"] is None: return "无数据"
    if r["lo"] >= 0.95 and r["hi"] <= 1.05: return "持平"
    if r["ratio"] > 1.05 and r["lo"] > 1.02: return "B 落后"
    if r["ratio"] < 0.95 and r["hi"] < 0.98: return "B 领先"
    return "不确定"


def analyze_xcomp(doc, subject="xoffset", baselines=DEFAULT_BASELINES, a="A", b="B", aa="A*", alpha=0.05,
                  k_q2=4, k_q1=12, aa_range=0.015, drift_range=0.02, goal=0.50):
    runs = doc["runs"]
    labels = [r["label"] for r in runs]
    med = [r["_med"] for r in runs]
    formats = sorted(set().union(*(set(m) for m in med))) if med else []
    res = {"subject": subject, "baselines": list(baselines), "labels": labels, "formats": formats,
           "n_runs": len(runs), "aborted": doc.get("aborted")}

    # Q2: B / A on per-run medians, adjacent pairs.
    pairs = adjacent_pairs(labels, a, b)
    res["pairs"] = pairs
    q2 = {}
    for fmt in formats:
        keys = sorted(set(med[0].get(fmt, {})) if med else set())
        for key in keys:
            vals = [(med[i][fmt][key], med[j][fmt][key]) for j, i in pairs if key in med[i].get(fmt, {}) and key in med[j].get(fmt, {})]
            main = fmt == subject and key.endswith(".total")
            r = paired_log_ratio(vals, alpha, k_q2 if main else 1, "two")
            if main: r["verdict"] = _verdict_q2(r)
            q2.setdefault(fmt, {})[key] = r
    res["q2"] = q2

    # A/A and drift on the subject's scenario totals.
    aap = adjacent_pairs(labels, a, aa)
    res["aa_pairs"] = aap
    aa_res, valid = {}, True
    for s in SCENARIOS:
        key = s + ".total"
        if not med or key not in med[0].get(subject, {}): continue
        r = paired_log_ratio([(med[j][subject][key], med[i][subject][key]) for i, j in aap], alpha, k_q2, "two")
        ok = r["ratio"] is not None and abs(r["ratio"] - 1) <= aa_range and (r["lo"] is None or (r["lo"] <= 1 <= r["hi"]))
        r["valid"] = ok
        valid &= ok
        if len(aap) >= 2:
            f = [med[x][subject][key] for x in aap[0]]
            l = [med[x][subject][key] for x in aap[-1]]
            r["drift"] = drift(f, l)
            r["drift_valid"] = abs(r["drift"] - 1) <= drift_range
            valid &= r["drift_valid"]
        aa_res[s] = r
    res["aa"] = aa_res
    res["aa_range"], res["drift_range"] = aa_range, drift_range
    res["valid"] = bool(aap) and valid and not doc.get("aborted")

    # Q1: subject / baseline per cycle inside each run, then pooled over runs of the same binary.
    q1 = {}
    groups = {a: [i for i, l in enumerate(labels) if l in (a, aa)], b: [i for i, l in enumerate(labels) if l == b]}
    for g, idxs in groups.items():
        for base in baselines:
            for s in SCENARIOS:
                key = s + ".total"
                per_run = [per_cycle_ratio(runs[i]["_ns"], subject, base, key, alpha, k_q1, "upper") for i in idxs
                           if base in runs[i]["_ns"] and subject in runs[i]["_ns"]]
                per_run = [p for p in per_run if p["ratio"]]
                logs = [math.log(p["ratio"]) for p in per_run]
                if not logs: continue
                n = len(logs)
                pooled = _interval(statistics.mean(logs), statistics.stdev(logs) / math.sqrt(n) if n > 1 else None,
                                   n - 1, alpha, k_q1, "upper")
                pooled.update(n=n, per_run=[(p["ratio"], p["hi"]) for p in per_run])
                if s != "D" and pooled["hi"] is not None: pooled["meets_goal"] = pooled["hi"] <= goal
                q1.setdefault(g, {}).setdefault(base, {})[s] = pooled
    res["q1"] = q1

    # Double ratio per pair: (X_B / base_B) / (X_A / base_A), explanatory only.
    dr = {}
    for base in baselines:
        for s in SCENARIOS:
            key = s + ".total"
            vals = []
            for j, i in pairs:
                rb = per_cycle_ratio(runs[i]["_ns"], subject, base, key, alpha, 1, "upper")["ratio"]
                ra = per_cycle_ratio(runs[j]["_ns"], subject, base, key, alpha, 1, "upper")["ratio"]
                if rb and ra: vals.append((rb, ra))
            if vals: dr.setdefault(base, {})[s] = paired_log_ratio(vals, alpha, 1, "two")
    res["double_ratio"] = dr

    # V5: identical wire bytes (median per cycle) across every run of both binaries.
    bytes_med = [{f: {m: statistics.median(v.values()) for m, v in ms.items() if v} for f, ms in r["_by"].items()} for r in runs]
    mism = []
    if bytes_med:
        ref = bytes_med[0]
        for i, bm in enumerate(bytes_med[1:], 1):
            for f in set(ref) | set(bm):
                for m in set(ref.get(f, {})) | set(bm.get(f, {})):
                    if ref.get(f, {}).get(m) != bm.get(f, {}).get(m):
                        mism.append({"run": i, "label": labels[i], "format": f, "metric": m,
                                     "ref": ref.get(f, {}).get(m), "got": bm.get(f, {}).get(m)})
    res["wire_bytes"] = bytes_med[0] if bytes_med else {}
    res["wire_bytes_mismatch"] = mism
    res["wall_s"] = {l: statistics.mean(r["wall_s"] for r in runs if r["label"] == l) for l in set(labels)}
    return res


def _f(x, d=3):
    return "" if x is None else f"{x:.{d}f}"


def markdown_xcomp(res, names=None):
    names = names or {}
    na, nb = names.get("A", "A"), names.get("B", "B")
    subj = res["subject"]
    out = []
    p = out.append
    p(f"## 批次概况\n")
    p(f"- 运行 {res['n_runs']} 次，配对 {len(res['pairs'])} 对，A/A {len(res['aa_pairs'])} 对；顺序 `{' '.join(res['labels'])}`")
    p(f"- A = {na}，B = {nb}，A* = A 的逐字节副本。比值一律 B ÷ A。")
    p(f"- 每次运行平均耗时：" + "，".join(f"{l} {v:.1f} s" for l, v in sorted(res["wall_s"].items())))
    if res.get("aborted"): p(f"- **中止**：{res['aborted']}")
    if res["aa_pairs"]:
        p(f"- 批次有效性：**{'有效' if res['valid'] else '无效'}**（A/A 几何均值在 ±{res['aa_range'] * 100:.1f}% 内且区间含 1；漂移在 ±{res['drift_range'] * 100:.1f}% 内）\n")
    else:
        p("- 这一批没有 A/A 控制，只作报告，不作判定。\n")
    p(f"## A/A 与漂移（{subj} 场景合计）\n")
    p("| 场景 | A*÷A | 95% 区间（k=4） | sd(log) | 有效 | 漂移（末块÷首块） | 漂移有效 |")
    p("| --- | ---: | --- | ---: | --- | ---: | --- |")
    for s, r in res["aa"].items():
        p(f"| {s} | {_f(r['ratio'])} | [{_f(r['lo'])}, {_f(r['hi'])}] | {_f(r['sd_log'], 4)} | {'是' if r['valid'] else '否'} | {_f(r.get('drift'))} | {'是' if r.get('drift_valid', True) else '否'} |")
    p(f"\n## Q2：{nb} ÷ {na}（{subj} 场景合计，每次运行取第 1–40 周期中位数，配对，双侧 95%，Bonferroni k=4）\n")
    p("| 场景 | 几何均值 | 95% 区间 | 对数 | 判定 |")
    p("| --- | ---: | --- | ---: | --- |")
    for s in SCENARIOS:
        r = res["q2"].get(subj, {}).get(s + ".total")
        if r: p(f"| {s} {SCENARIO_NAMES[s]} | {_f(r['ratio'])} | [{_f(r['lo'])}, {_f(r['hi'])}] | {r['n']} | {r.get('verdict', '')} |")
    p(f"\n## 基线自己的 {nb} ÷ {na}（场景合计，只作解释，双侧 95%，未校正）\n")
    others = [f for f in res["formats"] if f != subj]
    p("| 场景 | " + " | ".join(others) + " |")
    p("| --- |" + " --- |" * len(others))
    for s in SCENARIOS:
        cells = []
        for f in others:
            r = res["q2"].get(f, {}).get(s + ".total")
            cells.append(f"{_f(r['ratio'])} [{_f(r['lo'])}, {_f(r['hi'])}]" if r and r["ratio"] else "")
        p(f"| {s} | " + " | ".join(cells) + " |")
    p(f"\n## Q1：{subj} ÷ 基线（各编译器分别；每次运行逐周期配对，再按运行合并；单侧 95% 上界，k=12）\n")
    for g, gname in (("A", na), ("B", nb)):
        q = res["q1"].get(g)
        if not q: continue
        p(f"**{gname}**（{q[next(iter(q))]['A']['n']} 次运行）\n")
        p("| 场景 | " + " | ".join(q) + " |")
        p("| --- |" + " --- |" * len(q))
        for s in SCENARIOS:
            cells = []
            for base in q:
                r = q[base].get(s)
                if not r: cells.append(""); continue
                mark = "" if "meets_goal" not in r else (" ✓" if r["meets_goal"] else " ✗")
                cells.append(f"{_f(r['ratio'])} / {_f(r['hi'])}{mark}")
            p(f"| {s} | " + " | ".join(cells) + " |")
        p("")
    p("几何均值 / 上界；✓ = 上界 ≤ 0.50（A、B、C 的目标门槛），✗ = 未达标；D 只跟踪。\n")
    p(f"## 双重比值 (X_B/基线_B) ÷ (X_A/基线_A)（按对，只作解释）\n")
    dr = res["double_ratio"]
    p("| 场景 | " + " | ".join(dr) + " |")
    p("| --- |" + " --- |" * len(dr))
    for s in SCENARIOS:
        p(f"| {s} | " + " | ".join(f"{_f(dr[b][s]['ratio'])} [{_f(dr[b][s]['lo'])}, {_f(dr[b][s]['hi'])}]" if s in dr[b] else "" for b in dr) + " |")
    p(f"\n## 阶段级 {nb} ÷ {na}（{subj}，只作解释，未校正）\n")
    p("| 阶段 | 几何均值 | 95% 区间 |")
    p("| --- | ---: | --- |")
    for key, r in sorted(res["q2"].get(subj, {}).items()):
        if not key.endswith(".total"): p(f"| {key} | {_f(r['ratio'])} | [{_f(r['lo'])}, {_f(r['hi'])}] |")
    p(f"\n## 同样的工作（V5）：wire 字节\n")
    if res["wire_bytes_mismatch"]:
        p(f"**不一致 {len(res['wire_bytes_mismatch'])} 项**，前 10 项：\n")
        for m in res["wire_bytes_mismatch"][:10]: p(f"- {m}")
    else:
        p("所有运行（两个二进制）每种消息每周期 wire 字节的中位数逐项相等。")
    return "\n".join(out) + "\n"


def _pooled_sd(groups):
    """Pooled within-group standard deviation of lists of values."""
    ss = sum(sum((v - statistics.mean(g)) ** 2 for v in g) for g in groups if len(g) > 1)
    df = sum(len(g) - 1 for g in groups if len(g) > 1)
    return (math.sqrt(ss / df) if df > 0 else None), df


def aa_budget(doc, subject="xoffset", baselines=DEFAULT_BASELINES, a="A", b="B", budget_min=20.0, alpha=0.05):
    """Process-to-process noise of a batch and the 95% half-width reachable in budget_min minutes.
    Process sd is pooled within each label (A, A*, B), so it holds for a batch of two different binaries
    too; pair sd is the sd of log(B/A) over adjacent pairs. Predictions: paired ABBA with budget/2 pairs,
    and unpaired Welch with budget/2 processes per side (raw scenario total and the per-cycle
    normalisation by the baselines' geometric mean, H3 design section 7.4)."""
    runs = doc["runs"]
    labels = [r["label"] for r in runs]
    wall = statistics.mean(r["wall_s"] for r in runs)
    n_runs = max(4, int(budget_min * 60 / wall))
    out = {"wall_s": wall, "budget_min": budget_min, "runs_in_budget": n_runs, "scenarios": {}}
    pairs = adjacent_pairs(labels, a, b)
    for s in SCENARIOS:
        key = s + ".total"
        raw_g, norm_g = defaultdict(list), defaultdict(list)
        for r in runs:
            if key not in r["_med"].get(subject, {}): continue
            raw_g[r["label"]].append(math.log(r["_med"][subject][key]))
            if all(x in r["_ns"] for x in baselines):
                norm_g[r["label"]].append(norm_log_mean(r["_ns"], subject, baselines, s))
        sd_raw, df_raw = _pooled_sd(raw_g.values())
        sd_norm, _ = _pooled_sd(norm_g.values())
        pr = [math.log(runs[i]["_med"][subject][key] / runs[j]["_med"][subject][key]) for i, j in pairs]
        rec = {"sd_log_process": sd_raw, "sd_log_process_norm": sd_norm, "df_process": df_raw,
               "sd_log_pair": statistics.stdev(pr) if len(pr) > 1 else None, "n_pairs": len(pr)}
        npair = n_runs // 2
        if rec["sd_log_pair"]:
            rec["halfwidth_paired"] = math.exp(t_ppf(1 - alpha / 2, npair - 1) * rec["sd_log_pair"] / math.sqrt(npair)) - 1
        for name, sd in (("halfwidth_welch", sd_raw), ("halfwidth_welch_norm", sd_norm)):
            if sd:
                m = n_runs // 2
                rec[name] = math.exp(t_ppf(1 - alpha / 2, 2 * m - 2) * sd * math.sqrt(2.0 / m)) - 1
        out["scenarios"][s] = rec
    return out


# ---------------------------------------------------------------- verify (gates V3, V4)

_CYCLE = re.compile(r"^cycle (\d+) (\S+)\s+state=([0-9a-f]+) reports=(\d+)(.*)$")


def parse_verify(text):
    states, wire, in_wire = {}, {}, False
    for line in text.splitlines():
        m = _CYCLE.match(line.strip())
        if m:
            states[(int(m.group(1)), m.group(2))] = (m.group(3), int(m.group(4)), "MISMATCH" in m.group(5))
            continue
        if line.startswith("wire bytes per message"): in_wire = True; continue
        if in_wire:
            parts = line.split()
            if not parts or line.startswith("compressed"): in_wire = False; continue
            if parts[0] == "format": continue
            wire[parts[0]] = [float(x) for x in parts[1:]]
    return states, wire


def cmd_verify(a):
    prefix = shlex.split(a.prefix) if a.prefix else []
    results, ok = [], True
    for scale in a.scales.split(","):
        for proto in a.protocols.split(","):
            env = dict(os.environ)
            env.pop("SLG_REUSE", None)
            if proto == "reuse": env["SLG_REUSE"] = "1"
            per_bin = []
            for b in a.bin:
                argv = prefix + [b, "--mode", "verify", "--scale", scale] + (["--formats", a.formats] if a.formats else [])
                if a.cycles: argv += ["--cycles", str(a.cycles)]
                p = subprocess.run(argv, env=env, capture_output=True, text=True)
                states, wire = parse_verify(p.stdout)
                agree = p.returncode == 0 and "verify: all formats agree" in p.stderr
                per_bin.append({"bin": b, "rc": p.returncode, "agree": agree, "states": states, "wire": wire,
                                "stderr_tail": p.stderr.strip().splitlines()[-3:]})
                ok &= agree
            same_state = all(x["states"] == per_bin[0]["states"] for x in per_bin[1:])
            same_wire = all(x["wire"] == per_bin[0]["wire"] for x in per_bin[1:])
            ok &= same_state and same_wire and bool(per_bin[0]["states"])
            first = sorted(per_bin[0]["states"].items())
            c1 = [(k[1], v[0]) for k, v in first if k[0] == 1]
            rec = {"scale": scale, "protocol": proto, "agree": [x["agree"] for x in per_bin],
                   "same_state_across_bins": same_state, "same_wire_across_bins": same_wire,
                   "cycle1_state": dict(c1), "wire": per_bin[0]["wire"],
                   "stderr_tail": [x["stderr_tail"] for x in per_bin]}
            results.append(rec)
            print(f"verify {scale} {proto}: agree={rec['agree']} same_state={same_state} same_wire={same_wire} "
                  f"cycle1 xoffset={rec['cycle1_state'].get('xoffset')}")
    if a.json:
        with open(a.json, "w") as f: json.dump({"ok": ok, "bins": a.bin, "results": results}, f, indent=1)
    print("VERIFY OK" if ok else "VERIFY FAILED")
    return 0 if ok else 1


# ---------------------------------------------------------------- CLI

def _schedule(a):
    if a.protocol == "xcomp": return xcomp(a.pairs, a.aa_pairs)
    aa_at = tuple(x for x in a.aa_at.split(",") if x) if a.aa_copy or a.aa_at else ()
    return abba(a.blocks, aa="A*" if aa_at else None, aa_at=aa_at)


def cmd_schedule(a):
    print(" ".join(_schedule(a)))
    return 0


def cmd_run(a):
    bench = a.bench_args or DEFAULT_BENCH_ARGS
    if bench and bench[0] == "--": bench = bench[1:]
    sched = _schedule(a)
    os.makedirs(a.outdir, exist_ok=True)
    bins = {"A": a.a, "B": a.b}
    if "A*" in sched:
        if not a.aa_copy: raise SystemExit("this schedule has A/A runs: pass --aa-copy PATH")
        make_aa_copy(a.a, a.aa_copy)
        bins["A*"] = a.aa_copy
    prefix = shlex.split(a.prefix) if a.prefix else []
    inside = dict(x.split("=", 1) for x in a.bin_inside) if a.bin_inside else {}
    cmds = {"A": a.a_cmd, "B": a.b_cmd, "A*": a.aa_cmd}
    argv = {}
    for l, p in bins.items():
        # --a-cmd/--b-cmd/--aa-cmd: the whole command for that arm (wrapper and binary), e.g.
        # "docker exec h1rt taskset -c 2 /work/out/clang/slg_bench"; else prefix + binary.
        argv[l] = (shlex.split(cmds[l]) if cmds.get(l) else prefix + [inside.get(l, p)]) + bench
    env = dict(x.split("=", 1) for x in a.env)
    path_map = tuple(a.path_map.split("=", 1)) if a.path_map else None
    sampler = gate = None
    if not a.no_gate or a.sample_threshold:
        sampler = CpuSampler(os.path.join(a.outdir, "cpu.jsonl"), 2.0, a.sample_threshold)
        sampler.start()
    if not a.no_gate:
        gate = quiet_gate(sampler, a.load_max, a.quiet_s, a.gate_timeout, log=print)
    if a.gate_cmd:
        gate = command_gate(a.gate_cmd, gate, a.gate_timeout, log=print)
    with open(os.path.join(a.outdir, "protocol.json"), "w") as f:
        json.dump({"protocol": a.protocol, "pairs": a.pairs, "aa_pairs": a.aa_pairs, "blocks": a.blocks,
                   "bench_args": bench, "env": env, "gate": None if a.no_gate else
                   {"load_max": a.load_max, "quiet_s": a.quiet_s, "timeout_s": a.gate_timeout},
                   "pin_cpu": a.pin_cpu, "prefix": prefix, "schedule": sched}, f, indent=1)
    try:
        run_schedule(sched, argv, env, a.outdir, gate, a.pin_cpu, path_map, bins)
    finally:
        if sampler: sampler.stop()
    doc = json.load(open(os.path.join(a.outdir, "runs.json")))
    return 3 if doc.get("aborted") else 0


def cmd_analyze(a):
    doc = load_runs(a.outdir)
    res = analyze_xcomp(doc, a.subject, tuple(a.baselines.split(",")), aa_range=a.aa_range, drift_range=a.drift_range,
                        k_q2=a.k_q2, k_q1=a.k_q1)
    md = markdown_xcomp(res, {"A": a.a_name, "B": a.b_name})
    if a.json:
        with open(a.json, "w") as f: json.dump(res, f, indent=1, default=str)
    if a.md:
        with open(a.md, "w") as f: f.write(md)
    sys.stdout.write(md)
    return 0


def cmd_aa_budget(a):
    doc = load_runs(a.outdir)
    r = aa_budget(doc, a.subject, tuple(a.baselines.split(",")), budget_min=a.budget_min)
    print(f"# A/A budget: {a.outdir}")
    if a.json:
        with open(a.json, "w") as f: json.dump(r, f, indent=1)
    print(f"mean run {r['wall_s']:.1f} s -> {r['runs_in_budget']} runs in {r['budget_min']:.0f} min")
    print("| 场景 | 进程间 sd(log) | 归一后 sd(log) | 配对 sd(log) | 配对半宽 | Welch 半宽 | 归一 Welch 半宽 |")
    print("| --- | ---: | ---: | ---: | ---: | ---: | ---: |")
    for s, x in r["scenarios"].items():
        pc = lambda v: "" if v is None else f"±{v * 100:.1f}%"
        print(f"| {s} | {_f(x['sd_log_process'], 4)} | {_f(x['sd_log_process_norm'], 4)} | {_f(x['sd_log_pair'], 4)} | "
              f"{pc(x.get('halfwidth_paired'))} | {pc(x.get('halfwidth_welch'))} | {pc(x.get('halfwidth_welch_norm'))} |")
    return 0


def _git(*args):
    try:
        return subprocess.run(["git", "-c", "safe.directory=*", *args], capture_output=True, text=True, check=True,
                              cwd=os.path.dirname(os.path.abspath(__file__))).stdout.strip()
    except Exception:
        return None


def _version(cmd):
    try:
        p = subprocess.run(shlex.split(cmd) + ["--version"], capture_output=True, text=True, timeout=60)
        return (p.stdout or p.stderr).strip().splitlines()[0]
    except Exception as e:
        return f"unavailable: {e}"


def cmd_freeze(a):
    """Pre-registration file (H1 design section 5.7). Refuses to overwrite an existing file."""
    if os.path.exists(a.out) and not a.force:
        raise SystemExit(f"{a.out} exists; a freeze is never edited (use a new file, or --force before any timing)")
    kv = lambda xs: dict(x.split("=", 1) for x in (xs or []))
    lib = a.lib_commit
    head = _git("rev-parse", "HEAD")
    doc = {
        "kind": "H1_FREEZE", "created_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "design": "beta2-20261022/H1_GCC16_PERF_DESIGN_20261022.md sha256 2955d1d7f3a946292cb3df289ece6f0e588c0571224c61df978d6f6e7299d1b9",
        "library_commit": lib, "measurement_branch_head": head,
        "diff_lib_to_head_sha256": None, "library_code_diff_bytes": None,
        "binaries": {l: {"path": p, "sha256": sha256_file(p)} for l, p in kv(a.bin).items()},
        "files": {l: {"path": p, "sha256": sha256_file(p)} for l, p in kv(a.file).items()},
        "images": kv(a.image), "compilers": {l: _version(c) for l, c in kv(a.compiler).items()},
        "extra": kv(a.extra),
        "protocol": {
            "bench_args": a.bench_args, "env_primary": {"SLG_REUSE": "1"}, "env_secondary": {},
            "formats": a.formats, "pairs_primary": a.pairs, "pairs_secondary": a.pairs_secondary,
            "aa_pairs": a.aa_pairs, "order": "xcomp: A A* | A B B A x ceil(n/4) | A A* | A B B A x floor(n/4) | A A*",
            "pin_cpu": a.pin_cpu, "runs_per_platform": a.repeats,
        },
        "statistics": {
            "q2_metric": "xoffset A-D scenario totals, per-run median over cycles 1-40, B/A per adjacent pair",
            "q2_interval": "two-sided 95% t on logs", "q2_k": 4,
            "q1": "tools/analyze.py per-cycle paired, one-sided 95% UB, k=12; pooled over runs (df = runs-1)",
            "q1_baselines": ["cereal", "flatbuffers", "protobuf"], "q1_goal_ub": 0.50,
            "aa_valid": f"|geomean-1| <= {a.aa_range} and the k=4 interval contains 1",
            "drift_valid": f"|last A/A block / first A/A block - 1| <= {a.drift_range}",
        },
        "decision_q2": {
            "parity": "interval within [0.95, 1.05]",
            "gcc_behind": "geomean > 1.05 and lower bound > 1.02 -> open thread 'GCC 16 差距定位' (stages with ratio > 1.10 and >= 2% of the scenario)",
            "gcc_ahead": "geomean < 0.95 and upper bound < 0.98 -> hand stages to 性能优化",
            "uncertain": "otherwise; a signal only if the M4 VM and at least one GitHub platform agree in direction with > 3%",
            "github_alone_never_triggers": True,
            "invalid_batches": "reported and marked invalid; diagnosed; rerun at most once",
        },
    }
    if lib and head:
        diff = subprocess.run(["git", "-c", "safe.directory=*", "-c", "core.abbrev=7", "diff", f"{lib}..{head}"], capture_output=True,
                              cwd=os.path.dirname(os.path.abspath(__file__)))
        doc["diff_lib_to_head_sha256"] = hashlib.sha256(diff.stdout).hexdigest()
        code = subprocess.run(["git", "-c", "safe.directory=*", "diff", lib, head, "--", ".", ":!bench", ":!tools/perf", ":!.github"],
                              capture_output=True, cwd=os.path.dirname(os.path.abspath(__file__)))
        doc["library_code_diff_bytes"] = len(code.stdout)
        if code.stdout:
            raise SystemExit("library code differs between the measured commit and the branch head (H1 section 4.7)")
    with open(a.out, "w") as f: json.dump(doc, f, indent=1)
    print(f"wrote {a.out} sha256 {sha256_file(a.out)}")
    return 0


# Flags allowed to differ between the two toolchains' compile commands (H1 section 4.3).
CC_ALLOWED = {"-fexpansion-statements", "-stdlib=libc++", "-freflection", "-std=c++26", "-std=gnu++26"}


def _cc_tokens(entry, roots):
    toks = entry.get("arguments") or shlex.split(entry["command"])
    out, skip = [], False
    for t in toks[1:]:
        if skip: skip = False; continue
        if t in ("-o", "-MF", "-MT", "-MQ"): skip = True; continue
        if t in ("-MD", "-MMD", "-c") or t.endswith((".o", ".d")): continue
        for name, r in roots:
            if r: t = t.replace(r, f"<{name}>")
        out.append(t)
    return out


def cmd_compare_cc(a):
    """Compare compile_commands.json entries of the same source between two build directories."""
    res, ok = [], True
    allowed = CC_ALLOWED | set(a.allow or [])
    ja, jb = (json.load(open(os.path.join(d, "compile_commands.json"))) for d in (a.a_build, a.b_build))
    for src in a.source:
        ea = [e for e in ja if e["file"].endswith(src)]
        eb = [e for e in jb if e["file"].endswith(src)]
        if not ea or not eb:
            print(f"{src}: not found in {'A' if not ea else 'B'}"); ok = False; continue
        ta = _cc_tokens(ea[0], [("BUILD", os.path.abspath(a.a_build)), ("PREFIX_A", a.a_prefix), ("ROOT", a.root)])
        tb = _cc_tokens(eb[0], [("BUILD", os.path.abspath(a.b_build)), ("PREFIX_B", a.b_prefix), ("ROOT", a.root)])
        ta = [t.replace("<PREFIX_A>", "<PREFIX>") for t in ta]
        tb = [t.replace("<PREFIX_B>", "<PREFIX>") for t in tb]
        only_a = [t for t in ta if t not in tb]
        only_b = [t for t in tb if t not in ta]
        bad = [t for t in only_a + only_b if t not in allowed]
        ok &= not bad
        res.append({"source": src, "only_a": only_a, "only_b": only_b, "not_allowed": bad})
        print(f"{src}: only A {only_a}; only B {only_b}; {'OK' if not bad else 'NOT ALLOWED: ' + str(bad)}")
    if a.json:
        with open(a.json, "w") as f: json.dump({"ok": ok, "allowed": sorted(allowed), "results": res}, f, indent=1)
    return 0 if ok else 1


def cmd_sample(a):
    s = CpuSampler(a.out, a.interval, a.threshold)
    s.start()
    try:
        time.sleep(a.duration)
    except KeyboardInterrupt:
        pass
    s.stop()
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    def sched_args(p):
        p.add_argument("--protocol", choices=("xcomp", "abba"), default="xcomp")
        p.add_argument("--pairs", type=int, default=10, help="xcomp: A/B pairs")
        p.add_argument("--aa-pairs", type=int, default=3, help="xcomp: A/A control pairs (0-3)")
        p.add_argument("--blocks", type=int, default=3, help="abba: A B B A blocks")
        p.add_argument("--aa-at", default="", help="abba: A A* control blocks at start,middle,end")

    p = sub.add_parser("schedule"); sched_args(p); p.set_defaults(fn=cmd_schedule, aa_copy=None)
    p = sub.add_parser("run"); sched_args(p)
    p.add_argument("--a", required=True, help="reference binary (clang / anchor)")
    p.add_argument("--b", required=True, help="subject binary (GCC / head)")
    p.add_argument("--aa-copy", help="path for the byte-identical copy of A")
    p.add_argument("--outdir", required=True)
    p.add_argument("--env", action="append", default=[], help="K=V for every run (e.g. SLG_REUSE=1)")
    p.add_argument("--pin-cpu", type=int, help="taskset -c N (Linux)")
    p.add_argument("--prefix", help="wrapper, e.g. 'docker exec -e SLG_REUSE=1 ctr'")
    p.add_argument("--bin-inside", action="append", help="LABEL=PATH: binary path inside the wrapper")
    p.add_argument("--path-map", help="HOST=INSIDE prefix rewrite for the --out path")
    p.add_argument("--a-cmd", help="whole command for arm A (overrides --prefix/--bin-inside)")
    p.add_argument("--b-cmd", help="whole command for arm B")
    p.add_argument("--aa-cmd", help="whole command for the A/A copy")
    p.add_argument("--gate-cmd", help="shell command run before every run; must exit 0 to start")
    p.add_argument("--no-gate", action="store_true")
    p.add_argument("--load-max", type=float, default=3.0)
    p.add_argument("--quiet-s", type=float, default=180.0)
    p.add_argument("--gate-timeout", type=float, default=1800.0)
    p.add_argument("--sample-threshold", type=float, default=30.0)
    p.add_argument("bench_args", nargs=argparse.REMAINDER)
    p.set_defaults(fn=cmd_run)

    def an_args(p):
        p.add_argument("--outdir", required=True)
        p.add_argument("--subject", default="xoffset")
        p.add_argument("--baselines", default=",".join(DEFAULT_BASELINES))
        p.add_argument("--json")

    p = sub.add_parser("analyze"); an_args(p)
    p.add_argument("--a-name", default="A"); p.add_argument("--b-name", default="B")
    p.add_argument("--aa-range", type=float, default=0.015); p.add_argument("--drift-range", type=float, default=0.02)
    p.add_argument("--k-q2", type=int, default=4); p.add_argument("--k-q1", type=int, default=12)
    p.add_argument("--md")
    p.set_defaults(fn=cmd_analyze)
    p = sub.add_parser("aa-budget"); an_args(p)
    p.add_argument("--budget-min", type=float, default=20.0)
    p.set_defaults(fn=cmd_aa_budget)

    p = sub.add_parser("verify")
    p.add_argument("--bin", action="append", required=True)
    p.add_argument("--scales", default="S,M")
    p.add_argument("--protocols", default="reuse,per_message")
    p.add_argument("--formats")
    p.add_argument("--cycles", type=int)
    p.add_argument("--prefix")
    p.add_argument("--json")
    p.set_defaults(fn=cmd_verify)

    p = sub.add_parser("freeze")
    p.add_argument("--out", required=True)
    p.add_argument("--force", action="store_true")
    p.add_argument("--lib-commit", required=True, help="measured library commit (next_cpp26)")
    p.add_argument("--bin", action="append", help="LABEL=PATH")
    p.add_argument("--file", action="append", help="NAME=PATH (dependencies, scripts, tree manifest)")
    p.add_argument("--image", action="append", help="NAME=DIGEST")
    p.add_argument("--compiler", action="append", help="LABEL=COMMAND (its --version is recorded)")
    p.add_argument("--extra", action="append", help="KEY=VALUE")
    p.add_argument("--bench-args", default=" ".join(DEFAULT_BENCH_ARGS))
    p.add_argument("--formats", default="xoffset,cereal,protobuf,flatbuffers")
    p.add_argument("--pairs", type=int, required=True)
    p.add_argument("--pairs-secondary", type=int, required=True)
    p.add_argument("--aa-pairs", type=int, default=3)
    p.add_argument("--repeats", type=int, default=1)
    p.add_argument("--pin-cpu", type=int)
    p.add_argument("--aa-range", type=float, required=True)
    p.add_argument("--drift-range", type=float, required=True)
    p.set_defaults(fn=cmd_freeze)

    p = sub.add_parser("compare-cc")
    p.add_argument("--a-build", required=True); p.add_argument("--b-build", required=True)
    p.add_argument("--a-prefix", help="A's dependency prefix (normalised)"); p.add_argument("--b-prefix")
    p.add_argument("--root", help="checkout root (normalised)")
    p.add_argument("--source", action="append", default=["bench/slg_bench.cpp"])
    p.add_argument("--allow", action="append", help="extra flag allowed to differ (toolchain flags)")
    p.add_argument("--json")
    p.set_defaults(fn=cmd_compare_cc)

    p = sub.add_parser("sample")
    p.add_argument("--out", required=True)
    p.add_argument("--duration", type=float, default=3600)
    p.add_argument("--interval", type=float, default=2.0)
    p.add_argument("--threshold", type=float, default=30.0)
    p.set_defaults(fn=cmd_sample)

    a = ap.parse_args(argv)
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
