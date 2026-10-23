#!/usr/bin/env python3
"""L2 Mac perf gate batch (H3 design section 7).

One gate batch compares a head library commit with the anchor commit
(bench/perf/ANCHOR) on the Mac M4.  It has two timed parts:

  formal  the ROUND3 formal acceptance harness (formal_r3.py protocol): 8 binaries
          head, head_aa (a copy of head), anchor, r3 (R3 frozen candidate), cista,
          flatbuffers, capnproto, cereal; 65 macro cycles x 3 rounds, every round runs
          each binary once in a seeded shuffled order, cycle 0 discarded, inference
          on per-cycle mean log ratios.  Two batches, pooled for the gate.
  slg     the SLG prototype at M scale: head and anchor slg_bench processes
          alternate ABBA ABBA ABBA (6 each), SLG_REUSE=1, four formats, 41 cycles.
          Orchestration and statistics come from tools/perf/xc_ab.py.

Both the old protocol (12 items against the four rivals, G1-G3) and the new one
(head/anchor upper bounds) are reported side by side.  The rules are written into
FREEZE.json before the first timed run and never change afterwards.

Subcommands (run in this order on the Mac; analyze/trend/selftest run anywhere):
  freeze   DIR  check inputs, copy binaries, write FREEZE.json
  formal   DIR --batch N   one formal batch (waits for the quiet gate first)
  slg      DIR  the SLG alternating processes (waits for the quiet gate first)
  analyze  DIR  RESULT.json + RESULT.md
  trend    DIR --trend PERF_TREND.md   append one row
  selftest      synthetic data through the whole analysis
  diag-freeze / diag-formal / diag-analyze   a diagnostic batch with any set of equal-length labels and
                A/A copies (same harness, copies and start conditions; report only, no verdict)
"""
import argparse, datetime, hashlib, json, math, os, platform, random, shutil, statistics as st
import subprocess, sys, tempfile, time
from statistics import NormalDist

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'perf'))
import xc_ab  # noqa: E402  (tools/perf/xc_ab.py, the shared paired timing tool)

SCHEMA = 'xoffset-perf-gate-l2-v1'
PHASES8 = ['load', 'query_before', 'mutate', 'emit_after', 'retire_modified_state', 'reload', 'query_after', 'cleanup']
RIVALS = ['cista', 'flatbuffers', 'capnproto', 'cereal']
GOALS = [('read_sum', 1.05), ('mutate', 1.05), ('emit_after', 0.20)]
GATE_PHASES = ['read_sum', 'mutate', 'emit_after']
NEW_METRICS = ['load', 'read_sum', 'mutate', 'emit_after', 'reload', 'total']
LIBS = ['head', 'head_aa', 'anchor', 'r3'] + RIVALS
SLG_FORMATS = 'xoffset,cereal,protobuf,flatbuffers'
SLG_BASELINES = ['cereal', 'protobuf', 'flatbuffers']
SCEN = ['A', 'B', 'C', 'D']

# On-disk names: all the same length, so argv and path sizes do not differ between the
# binaries.  Labels in raw.jsonl stay long.  Each binary and wire is stored as COPIES
# separate files (head0 head1 head2 ...) and round r runs copy r % COPIES: the A/A
# (head_aa/head) mutate offset of ~1% survived equal-length names in W0b and varies
# between freezes (-0.7% .. +1.2% over 6 freezes), so it looks like a per-file effect
# (e.g. page-cache placement of the text pages); rotating copies averages it out.
FN = {'head': 'head', 'head_aa': 'hdaa', 'anchor': 'anch', 'r3': 'r3fz',
      'cista': 'cist', 'flatbuffers': 'flat', 'capnproto': 'capn', 'cereal': 'cere'}
COPIES = 3  # = rounds per cycle, so every cycle runs every copy once

# Screen and remote-desktop load: W0b ran with WindowServer >=30% in 429 of 483 samples and
# UURemoteServer at 15-18% (a remote viewer was attached); A/A per-round sd on mutate was
# 0.10-0.12 against 0.06-0.08 in quiet historical batches.
SCREEN_MAX = {'WindowServer': 20.0}
REMOTE_PROCS = ('UURemoteServer', 'screensharingd', 'ScreensharingAgent', 'AnyDesk', 'TeamViewer', 'ToDesk',
                'SunloginClient', 'RustDesk', 'parsecd')
REMOTE_MAX = 5.0
SCREEN_HOLD_S = 60.0

# Pre-registered rules (copied into FREEZE.json; analysis reads them back from there).
RULES = {
    'formal': {
        'cycles': 65, 'rounds_per_cycle': 3, 'warmup_cycles': 1, 'batches': 2,
        'seed': 'random.Random(20261022 + 1000 * (batch - 1) + round).shuffle(order of the 8 binaries)',
        'child_timeout_s': 30, 'file_names': FN, 'copies': COPIES,
        'copy_rule': 'every binary and wire is stored as `copies` separate files <name><i>; round r runs copy r % copies',
        'child_env': 'every child gets the same environment and the same working directory (the batch '
                     'directory); its temporary directory names have a fixed length',
        'gate': {
            'statistic': 'head/anchor per-cycle mean log ratio (formal_r3.py), cycles of all included batches '
                         'pooled; one-sided 95% upper bound, Bonferroni k=3, t(1-0.05/3, df)',
            'phases': GATE_PHASES, 'pass_le': 1.02, 'fail_gt': 1.03,
            'rule': 'after 2 batches: max upper bound <=1.02 pass, (1.02,1.03] pending, >1.03 fail; pending -> '
                    'diagnose, run one extra batch, pool all 3: <=1.02 pass, otherwise fail',
            'consistency': 'before pooling: for every gate phase and every pair of batches, the per-batch head/anchor '
                           'geometric means differ by <=2% and the per-batch two-sided k=12 intervals overlap; '
                           'otherwise do not pool: pending, diagnose, run a third batch',
            'max_batch_gap': 0.02,
            'quantiles': 'per gate phase, median and 10%/90% quantiles of the per-cycle ratios, per batch and pooled; report only',
        },
        'validity': {
            'pair': 'head_aa/head', 'phases': GATE_PHASES,
            'rule': 'same pooled statistic; for every gate phase the two-sided 95% interval (k=3) contains 1 '
                    'and |geo-1| <= 0.01; otherwise the gate is invalid: diagnose (start conditions, CPU '
                    'samples, per-batch A/A) before any rerun, and write the replacement into the report',
            'max_offset': 0.01,
        },
        'report': {
            'ratios': ['head/anchor', 'head/r3'], 'metrics': NEW_METRICS,
            'interval': 'two-sided 95%, Bonferroni k=12 (6 metrics x 2 references), per batch and pooled; '
                        'report only (head/r3 is G4 data, judged by 性能优化)',
        },
        'old_protocol': {
            'family': '12 = {cista,flatbuffers,capnproto,cereal} x {read_sum<=1.05, mutate<=1.05, emit_after<=0.20}; '
                      'ratio head/rival; joint one-sided 95% upper bound t(1-0.05/12, df), per batch',
            'aa': 'head_aa/head, 8 phases, two-sided t(1-0.05/16, df) inside [1/1.05, 1.05]; reported, does not '
                  'invalidate a gate batch',
            'auxiliary': 'reload head/rival two-sided t(1-0.05/8, df), report only',
        },
        'legacy_norm': {
            'item': 'cereal:mutate norm head/anchor = (head/cereal)/(anchor/cereal) in the same round '
                    '(identical to head/anchor because cereal cancels); criterion on the pooled geometric '
                    'mean (the 1.014-1.017 noise floor was a point estimate); one-sided 95% upper bound '
                    'k=2 reported, not a criterion',
            'threshold': 1.016,
        },
    },
    'slg': {
        'order': 'ABBA ABBA ABBA, A=head B=anchor, 6 processes each',
        'argv': f'--mode bench --scale M --cycles 41 --formats {SLG_FORMATS}', 'env': {'SLG_REUSE': '1'},
        'process_value': 'per scenario: mean over cycles 1-40 of log(xoffset total / geometric mean of '
                         'cereal, protobuf, flatbuffers totals in the same cycle)',
        'ratio': 'head/anchor = exp(mean head values - mean anchor values), Welch t interval, two-sided 95%, '
                 'Bonferroni k=4 (A-D)',
        'gate': 'A, B, C upper bound <=1.02; D report only', 'pass_le': 1.02,
        'old_protocol': 'G1-G3: head processes pooled; per process geo log ratio xoffset/rival on the scenario '
                        'total, run-level t (df = processes-1), one-sided 95%, k=12; plus analyze.py-style '
                        'per-cycle bound of the first head process',
    },
    'quiet': 'before each batch and before the SLG part: no compiler process for 3 minutes and 1-minute '
             'load < 3; then for 60 s (ps pcpu, every 5 s) WindowServer < 20% and every remote-desktop '
             'process < 5%; on timeout (30 min) nothing is timed',
    'screen': {'max': SCREEN_MAX, 'remote_procs': REMOTE_PROCS, 'remote_max': REMOTE_MAX, 'hold_s': SCREEN_HOLD_S},
}

h = lambda p: hashlib.sha256(open(p, 'rb').read()).hexdigest()


def tq(p, df):
    """formal_r3.py's t quantile (Cornish-Fisher); kept so old-protocol numbers match formal_r3.py."""
    z = NormalDist().inv_cdf(p)
    return z + (z**3 + z) / (4 * df) + (5 * z**5 + 16 * z**3 + 3 * z) / (96 * df**2) + (3 * z**7 + 19 * z**5 + 17 * z**3 - 15 * z) / (384 * df**3)


# ---------------------------------------------------------------- freeze

def git_tree_note(src):
    if not src or not os.path.isdir(os.path.join(src, '.git')): return None
    r = lambda *a: subprocess.run(['git', '-C', src, *a], capture_output=True, text=True).stdout.strip()
    return {'path': src, 'head': r('rev-parse', 'HEAD'), 'status': r('status', '--porcelain')[:2000]}


def cmd_freeze(a):
    D = a.dir
    if os.path.exists(D): sys.exit(f'{D} exists')
    r3 = json.load(open(os.path.join(a.r3_dir, 'FREEZE.json')))
    src = {
        'head': (a.head_bin, a.head_wire), 'head_aa': (a.head_bin, a.head_wire),
        'anchor': (a.anchor_bin, a.anchor_wire),
        'r3': (os.path.join(a.r3_dir, 'bin', 'xoffset_cand'), os.path.join(a.r3_dir, 'wire', 'xoffset_cand.input.wire')),
    }
    for k in RIVALS: src[k] = (os.path.join(a.r3_dir, 'bin', k), os.path.join(a.r3_dir, 'wire', k + '.input.wire'))
    problems = []
    for k in ['r3'] + RIVALS:  # references must be exactly the R3 formal ones
        rk = 'xoffset_cand' if k == 'r3' else k
        if h(src[k][0]) != r3['binaries'][rk] or h(src[k][1]) != r3['input_wires'][rk]:
            problems.append(f'{k}: binary or wire differs from {a.r3_dir}/FREEZE.json')
    hw, aw = h(a.head_wire), h(a.anchor_wire)
    if a.expect_wire and (hw != a.expect_wire or aw != a.expect_wire):
        problems.append(f'M input wire: head {hw}, anchor {aw}, expected {a.expect_wire}')
    prep = {}
    for item in a.prep or []:  # NAME=LOGFILE:EXITCODE written by the MAC_STEPS prep commands
        name, rest = item.split('=', 1); path, code = rest.rsplit(':', 1)
        prep[name] = {'log': os.path.basename(path), 'log_sha256': h(path), 'exit': int(code)}
        if int(code) != 0: problems.append(f'prep {name} exit {code} (see {path})')
    if problems:
        sys.exit('freeze refused, nothing timed:\n  ' + '\n  '.join(problems))
    os.makedirs(D + '/bin'); os.makedirs(D + '/wire'); os.makedirs(D + '/prep')
    for k, (b, w) in src.items():
        for i in range(COPIES):  # separate files, not links: each copy gets its own page-cache pages
            shutil.copy2(b, f'{D}/bin/{FN[k]}{i}'); shutil.copy2(w, f'{D}/wire/{FN[k]}{i}.input.wire')
    shutil.copy2(a.slg_head, D + '/bin/slg_head'); shutil.copy2(a.slg_anchor, D + '/bin/slg_anch')
    for item in a.prep or []:
        path = item.split('=', 1)[1].rsplit(':', 1)[0]; shutil.copy2(path, D + '/prep/' + os.path.basename(path))
    shutil.copy2(os.path.abspath(__file__), D + '/mac_gate.py')
    shutil.copy2(xc_ab.__file__, D + '/xc_ab.py')
    try: therm = subprocess.run(['pmset', '-g', 'therm'], capture_output=True, text=True).stdout
    except OSError: therm = 'pmset unavailable'
    fr = {
        'schema': SCHEMA, 'name': os.path.basename(os.path.normpath(D)),
        'head': {'commit': a.head_commit, 'desc': a.head_desc}, 'anchor': {'commit': a.anchor_commit, 'desc': a.anchor_desc},
        'lib_identity_note': a.lib_note,
        'binaries': {k: h(f'{D}/bin/{FN[k]}0') for k in src}, 'input_wires': {k: h(f'{D}/wire/{FN[k]}0.input.wire') for k in src},
        'slg': {'head': h(D + '/bin/slg_head'), 'anchor': h(D + '/bin/slg_anch'), 'tree': a.slg_tree, 'tree_sha256': a.slg_tree_sha},
        'r3_dir': a.r3_dir, 'trace': a.trace, 'trace_sha256': h(a.trace),
        'mac_gate_py_sha256': h(D + '/mac_gate.py'), 'xc_ab_py_sha256': h(D + '/xc_ab.py'),
        'sources': {'head': git_tree_note(a.head_src), 'anchor': git_tree_note(a.anchor_src)},
        'prep': prep, 'rules': RULES,
        'host': platform.platform(), 'frozen_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds'),
        'pmset_therm': therm,
    }
    for k in src:
        for i in range(1, COPIES):
            if h(f'{D}/bin/{FN[k]}{i}') != fr['binaries'][k] or h(f'{D}/wire/{FN[k]}{i}.input.wire') != fr['input_wires'][k]:
                sys.exit(f'freeze: copy {i} of {k} differs from copy 0')
    json.dump(fr, open(D + '/FREEZE.json', 'w'), indent=1)
    print(f'FREEZE.json written: {D}')
    for k, v in fr['binaries'].items(): print(f'  {k:12s} {v}')


# ---------------------------------------------------------------- quiet gate and sampling

def start_sampler(D, label):
    os.makedirs(D + '/cpu', exist_ok=True)
    sm = xc_ab.CpuSampler(f'{D}/cpu/{label}.jsonl', interval=2.0, threshold=30.0); sm.start()
    return sm


def screen_load():
    """pcpu (ps) of WindowServer and of remote-desktop processes; {} where ps is unavailable."""
    try: txt = subprocess.run(['ps', '-A', '-o', 'pcpu=,comm='], capture_output=True, text=True, timeout=10).stdout
    except (OSError, subprocess.SubprocessError): return {}
    out = {}
    for line in txt.splitlines():
        parts = line.split(None, 1)
        if len(parts) != 2: continue
        name = os.path.basename(parts[1].strip())
        if name in SCREEN_MAX or name.startswith(REMOTE_PROCS):
            try: out[name] = round(out.get(name, 0.0) + float(parts[0]), 1)
            except ValueError: pass
    return out


def screen_problems(load):
    return [f'{n} {v:.0f}%' for n, v in sorted(load.items()) if v >= SCREEN_MAX.get(n, REMOTE_MAX)]


def quiet_or_exit(D, label, sampler, timeout_s, quiet_s=180.0):
    """Block until the start conditions hold (xc_ab.quiet_gate: the sampler has seen no compiler for
    180 s and the 1-minute load is < 3; then WindowServer and remote-desktop processes below their
    limits for 60 s); on timeout record it and exit without timing anything."""
    utc = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds')
    t0 = time.time(); hold = min(SCREEN_HOLD_S, quiet_s); ok_since = None; bad = []
    try:
        while True:
            info = xc_ab.quiet_gate(sampler, load_max=3.0, quiet_s=quiet_s, timeout_s=max(0.0, timeout_s - (time.time() - t0)))()
            screen = screen_load(); bad = screen_problems(screen); now = time.time()
            ok_since = None if bad else (ok_since or now)
            if ok_since is not None and now - ok_since >= hold: break
            if now - t0 > timeout_s:
                raise xc_ab.GateTimeout(f'screen or remote desktop busy after {timeout_s:.0f} s: {", ".join(bad) or "not held " + str(hold) + " s"}')
            time.sleep(5)
        info = {**info, 'waited_s': round(time.time() - t0, 1), 'screen': screen}
        ok = True
    except xc_ab.GateTimeout as e:
        info, ok = {'reason': str(e)}, False
    rec = {'label': label, 'utc': utc, 'ok': ok, 'quiet_s_required': quiet_s, **info}
    with open(D + '/quiet.jsonl', 'a') as f: f.write(json.dumps(rec) + '\n')
    if not ok:
        sampler.stop()
        sys.exit(f'quiet gate timed out before {label}: {info["reason"]}. Nothing was timed; do the non-timing steps and report.')
    print(f'== {label} start {utc} {info}')
    return rec


def cpu_summary(path, t0=None, t1=None, own=()):
    """Summary of a CpuSampler log over [t0, t1]: samples, Chrome CPU, other busy processes, compilers."""
    recs = [json.loads(l) for l in open(path)] if os.path.exists(path) else []
    recs = [r for r in recs if (t0 is None or r['t'] >= t0) and (t1 is None or r['t'] <= t1)]
    chrome, other, comp, names = [], 0, 0, {}
    for r in recs:
        chrome.append(sum(b['pcpu'] for b in r['busy'] if 'chrome' in b['comm'].lower()))
        ob = [b for b in r['busy'] if 'chrome' not in b['comm'].lower() and 'python' not in os.path.basename(b['comm']).lower()
              and not os.path.basename(b['comm']).strip('()').startswith(tuple(FN.values()) + tuple(own) + ('slg_',))]  # ps shows exiting children as (name)
        other += bool(ob); comp += bool(r['compilers'])
        for b in ob: names[os.path.basename(b['comm'])] = names.get(os.path.basename(b['comm']), 0) + 1
    out = {'samples': len(recs), 'other_busy_samples': other, 'compile_samples': comp,
           'other_busy_top': dict(sorted(names.items(), key=lambda x: -x[1])[:5])}
    if chrome: out.update(chrome_busy_mean=round(st.mean(chrome), 1), chrome_busy_max=round(max(chrome), 1))
    if recs: out.update(load1_max=max(r['load1'] for r in recs))
    return out


# ---------------------------------------------------------------- formal part

def cmd_formal(a):
    D = a.dir; fr = json.load(open(D + '/FREEZE.json')); R = fr['rules']['formal']
    B = f'{D}/formal-{a.batch}'
    if os.path.exists(B): sys.exit(f'{B} exists (no reruns into the same directory)')
    C = R['copies']
    for k in LIBS:
        if any(h(f'{D}/bin/{FN[k]}{i}') != fr['binaries'][k] for i in range(C)): sys.exit(f'{k}: binary changed since FREEZE')
    cycles = a.cycles or R['cycles']; smoke = cycles != R['cycles'] or a.quiet_s != 180.0
    sampler = start_sampler(D, f'formal-{a.batch}')
    quiet_or_exit(D, f'formal-{a.batch}', sampler, a.quiet_timeout, a.quiet_s)
    os.makedirs(B)
    t0 = time.time()
    meta = {'batch': a.batch, 'cycles': cycles, 'smoke': smoke, 'start_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds'),
            'load_start': os.getloadavg()}
    try:
        with open(B + '/raw.jsonl', 'w') as f:
            for rnd in range(cycles * R['rounds_per_cycle']):
                order = list(LIBS); random.Random(20261022 + 1000 * (a.batch - 1) + rnd).shuffle(order)
                for k in order:
                    f.write(json.dumps({'round': rnd, 'cycle': rnd // R['rounds_per_cycle'], 'lib': k, 'copy': rnd % C,
                                        'phase_ns': run_formal_child(D, B, FN[k], rnd % C, fr['trace'], R['child_timeout_s'])}) + '\n')
    finally:
        sampler.stop()
    meta['end_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds'); meta['load_end'] = os.getloadavg()
    meta['cpu'] = cpu_summary(f'{D}/cpu/formal-{a.batch}.jsonl', t0, time.time())
    json.dump(meta, open(B + '/meta.json', 'w'), indent=1)
    b = analyze_formal_batch(B, R)
    print(f'formal-{a.batch}: this batch alone: max UB head/anchor {b["gate"]["max_upper"]:.4f}, A/A ok {b["aa"]["ok"]}, '
          f'old {b["old"]["passed"]}/12, cpu {meta["cpu"]}  (the gate verdict needs the pooled batches: run analyze)')


def run_formal_child(D, B, stem, copy, trace, timeout):
    f = f'{stem}{copy}'
    with tempfile.TemporaryDirectory(dir=B) as d:
        p = subprocess.run([f'{D}/bin/{f}', 'run', '--wire', f'{D}/wire/{f}.input.wire', '--trace', trace, '--wire-out', d + '/w',
                            '--canonical-out', d + '/c', '--queries-out', d + '/q', '--result', d + '/r.json'],
                           capture_output=True, timeout=timeout, cwd=B, env=CHILD_ENV)
        if p.returncode: raise SystemExit(f'child failed {f}: {p.stderr[-300:]}')
        return {kk: int(v) for kk, v in json.load(open(d + '/r.json'))['phase_ns'].items()}


CHILD_ENV = dict(os.environ)


def load_formal(B, R, nlibs=len(LIBS)):
    by = {}
    for line in open(B + '/raw.jsonl'):
        r = json.loads(line); p = r['phase_ns']
        p['read_sum'] = p['query_before'] + p['query_after']
        p['total'] = sum(v for kk, v in r['phase_ns'].items() if kk != 'read_sum')
        by.setdefault(r['round'], {})[r['lib']] = p
    rpc = R['rounds_per_cycle']; ncyc = len(by) // rpc
    if ncyc * rpc != len(by) or any(len(v) != nlibs for v in by.values()):
        raise SystemExit(f'{B}/raw.jsonl is incomplete ({len(by)} rounds)')
    return by, ncyc


def cycle_logs(data, R, num, den, m):
    """Per-cycle mean log ratios num/den (formal_r3.py); data = [(by, ncyc), ...] is pooled."""
    rpc = R['rounds_per_cycle']
    return [st.mean(math.log(by[rr][num][m] / by[rr][den][m]) for rr in range(c * rpc, (c + 1) * rpc))
            for by, ncyc in data for c in range(R['warmup_cycles'], ncyc)]


def interval(cm, q_lo, q_hi):
    mu, se = st.mean(cm), st.stdev(cm) / math.sqrt(len(cm))
    return {'geo': math.exp(mu), 'low': math.exp(mu - q_lo * se), 'high': math.exp(mu + q_hi * se), 'n': len(cm)}


def gate_block(data, R):
    """The gate statistic and the A/A validity on the given (pooled) batches."""
    G, V = R['gate'], R['validity']
    df = len(cycle_logs(data, R, 'head', 'anchor', 'mutate')) - 1
    q1, q2 = tq(1 - 0.05 / 3, df), tq(1 - 0.05 / 6, df)
    ph = {m: interval(cycle_logs(data, R, 'head', 'anchor', m), q2, q1) for m in G['phases']}  # low two-sided, high one-sided
    mx = max(v['high'] for v in ph.values())
    verdict = 'pass' if mx <= G['pass_le'] else ('pending' if mx <= G['fail_gt'] else 'fail')
    aa = {m: interval(cycle_logs(data, R, 'head_aa', 'head', m), q2, q2) for m in V['phases']}
    for v in aa.values(): v['ok'] = v['low'] <= 1 <= v['high'] and abs(v['geo'] - 1) <= V['max_offset']
    for m in G['phases']:
        cm = sorted(math.exp(x) for x in cycle_logs(data, R, 'head', 'anchor', m)); qs = st.quantiles(cm, n=10)
        ph[m]['cycle_quantiles'] = {'p10': qs[0], 'median': st.median(cm), 'p90': qs[-1]}
    return {'df': df, 'phases': ph, 'max_upper': mx, 'verdict': verdict, 'aa': {'phases': aa, 'ok': all(v['ok'] for v in aa.values())}}


def consistency(batches, R):
    """Pairwise check of the per-batch head/anchor estimates before they are pooled."""
    gap = R['gate']['max_batch_gap']; problems = []
    for i in range(len(batches)):
        for j in range(i + 1, len(batches)):
            for m in R['gate']['phases']:
                x = batches[i]['report']['ratios'][f'head/anchor:{m}']; y = batches[j]['report']['ratios'][f'head/anchor:{m}']
                d = abs(math.log(x['geo'] / y['geo']))
                if d > math.log(1 + gap) or x['low'] > y['high'] or y['low'] > x['high']:
                    problems.append(f'{batches[i]["batch_dir"]} vs {batches[j]["batch_dir"]} {m}: {x["geo"]:.4f} vs {y["geo"]:.4f}')
    return problems


def report_block(data, R):
    df = len(cycle_logs(data, R, 'head', 'anchor', 'mutate')) - 1
    q12 = tq(1 - 0.05 / 24, df)
    out = {f'head/{ref}:{m}': interval(cycle_logs(data, R, 'head', ref, m), q12, q12) for ref in ['anchor', 'r3'] for m in NEW_METRICS}
    ln = interval(cycle_logs(data, R, 'head', 'anchor', 'mutate'), 0, tq(1 - 0.05 / 2, df))
    ln['ok'] = ln['geo'] <= R['legacy_norm']['threshold']
    return {'df': df, 'ratios': out, 'legacy_norm': ln}


def analyze_formal_batch(B, R):
    data = [load_formal(B, R)]; ncyc = data[0][1]
    df = ncyc - R['warmup_cycles'] - 1
    res = {'batch_dir': os.path.basename(B), 'df': df, 'cycles': ncyc}
    g = gate_block(data, R); res['gate'] = {k: g[k] for k in ('phases', 'max_upper', 'verdict')}; res['aa'] = g['aa']
    res['report'] = report_block(data, R)
    res['aa_detail'] = aa_detail(data[0][0], ncyc, R)
    q1 = tq(1 - 0.05 / 12, df); passed = 0; items = {}
    for comp in RIVALS:
        for m, goal in GOALS:
            iv = interval(cycle_logs(data, R, 'head', comp, m), 0, q1); ok = iv['high'] <= goal; passed += ok
            items[f'{comp}:{m}'] = {'geo': iv['geo'], 'upper': iv['high'], 'threshold': goal, 'pass': ok}
    qa = tq(1 - 0.05 / 16, df); aa8 = {}
    for m in PHASES8:
        iv = interval(cycle_logs(data, R, 'head_aa', 'head', m), qa, qa)
        aa8[m] = {**iv, 'ok': iv['low'] >= 1 / 1.05 and iv['high'] <= 1.05}
    qx = tq(1 - 0.05 / 8, df)
    aux = {f'{comp}:reload': interval(cycle_logs(data, R, 'head', comp, 'reload'), qx, qx) for comp in RIVALS}
    ref_passed = {ref: sum(interval(cycle_logs(data, R, ref, comp, m), 0, q1)['high'] <= goal for comp in RIVALS for m, goal in GOALS)
                  for ref in ['anchor', 'r3']}
    res['old'] = {'items': items, 'passed': passed, 'aa8': aa8, 'aa8_valid': all(v['ok'] for v in aa8.values()),
                  'auxiliary': aux, 'passed_anchor': ref_passed['anchor'], 'passed_r3': ref_passed['r3']}
    by = data[0][0]
    keep = [rr for c in range(R['warmup_cycles'], ncyc) for rr in range(c * R['rounds_per_cycle'], (c + 1) * R['rounds_per_cycle'])]
    res['medians_ms'] = {lib: {m: st.median(by[rr][lib][m] for rr in keep) / 1e6 for m in PHASES8 + ['read_sum', 'total']} for lib in LIBS}
    json.dump(res, open(B + '/RESULT.json', 'w'), indent=1)
    return res


def aa_detail(by, ncyc, R):
    """Report only: per gate phase, the sd of the per-round log(head_aa/head) (noise level; quiet
    historical batches had 0.06-0.08 on mutate) and the A/A geometric mean per copy index."""
    rpc, C = R['rounds_per_cycle'], R.get('copies', 1)
    rr = range(R['warmup_cycles'] * rpc, ncyc * rpc); out = {}
    for m in GATE_PHASES:
        x = {r: math.log(by[r]['head_aa'][m] / by[r]['head'][m]) for r in rr}
        out[m] = {'round_sd': st.stdev(x.values()),
                  'by_copy': {str(c): math.exp(st.mean(v for r, v in x.items() if r % C == c)) for c in range(C)}}
    return out


def formal_overall(bdirs, batches, R):
    """Gate verdict on all included batches pooled (section 'gate' of the rules)."""
    n = len(bdirs)
    if n < R['batches']: return {'verdict': 'incomplete', 'batches': n}
    data = [load_formal(B, R) for B in bdirs]
    g = gate_block(data, R); rep = report_block(data, R); inc = consistency(batches, R)
    if inc: v = 'pending'          # not pooled: diagnose, then a third batch (or exclude a diagnosed one)
    elif not g['aa']['ok']: v = 'invalid'
    elif g['verdict'] == 'pending' and n > R['batches']: v = 'fail'
    else: v = g['verdict']
    return {'verdict': v, 'batches': n, 'inconsistent': inc, 'gate': g, 'report': rep}


# ---------------------------------------------------------------- SLG part

SLG_LABELS = {'head': 'head', 'anch': 'anchor'}  # equal-length labels: raw paths in argv have the same length


def cmd_slg(a):
    D = a.dir; fr = json.load(open(D + '/FREEZE.json'))
    S = D + '/slg'
    if os.path.exists(S): sys.exit(f'{S} exists (no reruns into the same directory)')
    for k, f in [('head', 'slg_head'), ('anchor', 'slg_anch')]:
        if h(f'{D}/bin/{f}') != fr['slg'][k]: sys.exit(f'{f}: binary changed since FREEZE')
    sampler = start_sampler(D, 'slg')
    quiet_or_exit(D, 'slg', sampler, a.quiet_timeout, a.quiet_s)
    cycles = a.cycles or 41
    sched = xc_ab.abba(blocks=3, a='head', b='anch')
    argv = {lab: [f'{D}/bin/slg_{lab}', '--mode', 'bench', '--scale', 'M', '--cycles', str(cycles), '--formats', SLG_FORMATS]
            for lab in SLG_LABELS}
    gate = xc_ab.quiet_gate(sampler, load_max=3.0, quiet_s=a.quiet_s, timeout_s=a.quiet_timeout)
    try:
        xc_ab.run_schedule(sched, argv, {'SLG_REUSE': '1'}, S, gate=gate,
                           binaries={lab: f'{D}/bin/slg_{lab}' for lab in SLG_LABELS})
    finally:
        sampler.stop()
    doc = json.load(open(S + '/runs.json')); doc['smoke'] = cycles != 41 or a.quiet_s != 180.0
    doc['cpu'] = cpu_summary(f'{D}/cpu/slg.jsonl', doc['started'], doc.get('finished', time.time()))
    json.dump(doc, open(S + '/runs.json', 'w'), indent=1)
    res = analyze_slg(S)
    if res.get('aborted'): sys.exit(f'slg aborted: {res["aborted"]}')
    print('slg: ' + '  '.join(f'{s} {res["new"][s]["geo"]:.4f} [{res["new"][s]["low"]:.4f}, {res["new"][s]["high"]:.4f}]' for s in SCEN)
          + f'  verdict {res["verdict"]}')


def welch(xs, ys, k):
    r = xc_ab.welch_log_ratio(xs, ys, alpha=0.05, k=k, sided='two', logs=True)
    return {'geo': r['ratio'], 'low': r['lo'], 'high': r['hi'], 'df': r.get('df'), 'n': r['n']}


def analyze_slg(S):
    doc = json.load(open(S + '/runs.json'))
    vals = {'head': [], 'anchor': []}; rival = []; first_head = None; med = {'head': [], 'anchor': []}
    for r in doc['runs']:
        lab = SLG_LABELS.get(r['label'], r['label'])
        ns = xc_ab.read_slg_raw(os.path.join(S, r['raw']))
        vals[lab].append({s: xc_ab.norm_log_mean(ns, 'xoffset', SLG_BASELINES, s) for s in SCEN})
        med[lab].append({s: st.median(ns['xoffset'][s + '.total'].values()) / 1e6 for s in SCEN})
        if lab == 'head':
            rival.append({f'{s}/{b}': xc_ab.paired_log_ratio([(ns['xoffset'][s + '.total'][c], ns[b][s + '.total'][c])
                                                              for c in sorted(set(ns['xoffset'][s + '.total']) & set(ns[b][s + '.total']))])['ratio']
                          for s in SCEN for b in SLG_BASELINES})
            if first_head is None: first_head = ns
    res = {'aborted': doc.get('aborted'), 'smoke': doc.get('smoke', False), 'cpu': doc.get('cpu'),
           'n': {k: len(v) for k, v in vals.items()}, 'process_values': vals, 'xoffset_median_ms': med}
    if min(res['n'].values()) < 2:
        res.update(verdict='incomplete'); json.dump(res, open(S + '/RESULT.json', 'w'), indent=1); return res
    new = {s: welch([v[s] for v in vals['head']], [v[s] for v in vals['anchor']], k=4) for s in SCEN}
    gate = {s: new[s]['high'] <= RULES['slg']['pass_le'] for s in 'ABC'}
    old = {}
    for s in SCEN:  # run-level: one geometric-mean ratio per head process, t across processes
        for b in SLG_BASELINES:
            r = xc_ab.paired_log_ratio([(v[f'{s}/{b}'], 1.0) for v in rival], alpha=0.05, k=12, sided='upper')
            old[f'{s}/{b}'] = {'geo': r['ratio'], 'upper': r['hi']}
    per_cycle = {}
    for s in SCEN:  # tools/analyze.py's per-cycle bound for the first head process (continuity)
        for b in SLG_BASELINES:
            r = xc_ab.per_cycle_ratio(first_head, 'xoffset', b, s + '.total', alpha=0.05, k=12, sided='upper')
            per_cycle[f'{s}/{b}'] = {'geo': r['ratio'], 'upper': r['hi']}
    drift = {lab: {s: math.exp(v[-1][s] - v[0][s]) for s in SCEN} for lab, v in vals.items()}
    res.update(new=new, gate=gate, verdict='pass' if all(gate.values()) else 'fail', old_pooled=old,
               old_first_head_per_cycle=per_cycle, drift_last_over_first=drift)
    if res['aborted']: res['verdict'] = 'incomplete'
    json.dump(res, open(S + '/RESULT.json', 'w'), indent=1)
    return res


# ---------------------------------------------------------------- analyze / report

def cmd_analyze(a):
    D = a.dir; fr = json.load(open(D + '/FREEZE.json')); R = fr['rules']['formal']
    excl = {int(x.split(':', 1)[0]): x.split(':', 1)[1] if ':' in x else '' for x in (a.exclude or [])}
    bdirs = sorted((os.path.join(D, x) for x in os.listdir(D) if x.startswith('formal-')), key=lambda p: int(p.rsplit('-', 1)[1]))
    batches = []
    for B in bdirs:
        b = analyze_formal_batch(B, R)
        b['meta'] = json.load(open(B + '/meta.json')) if os.path.exists(B + '/meta.json') else {}
        n = int(B.rsplit('-', 1)[1]); b['excluded'] = excl.get(n) if n in excl else None
        batches.append(b)
    used = [(B, b) for B, b in zip(bdirs, batches) if b['excluded'] is None]
    overall = formal_overall([B for B, _ in used], [b for _, b in used], R)
    slg = analyze_slg(D + '/slg') if os.path.exists(D + '/slg/runs.json') else None
    fv = overall['verdict']
    if fv in ('fail',) or (slg and slg['verdict'] == 'fail'): verdict = 'fail'
    elif fv in ('invalid', 'pending', 'incomplete'): verdict = fv
    elif slg is None: verdict = 'incomplete'
    else: verdict = 'pass'
    if any(b['meta'].get('smoke') for b in batches) or (slg and slg.get('smoke')): verdict = 'smoke (not a gate result)'
    res = {'schema': SCHEMA, 'name': fr['name'], 'head': fr['head'], 'anchor': fr['anchor'], 'verdict': verdict,
           'formal_overall': overall, 'formal_batches': batches, 'slg': slg, 'freeze_sha256': h(D + '/FREEZE.json'),
           'quiet': [json.loads(l) for l in open(D + '/quiet.jsonl')] if os.path.exists(D + '/quiet.jsonl') else []}
    json.dump(res, open(D + '/RESULT.json', 'w'), indent=1)
    open(D + '/RESULT.md', 'w').write(render_md(fr, res))
    print(f'RESULT.json / RESULT.md written: verdict {verdict}')


def f3(x): return f'{x:.3f}'
def f4(x): return f'{x:.4f}'
def iv4(v): return f'{f4(v["geo"])} [{f4(v["low"])}, {f4(v["high"])}]'


def render_md(fr, res):
    R = fr['rules']['formal']; out = []; p = out.append
    hd, an, ov, bs = fr['head'], fr['anchor'], res['formal_overall'], res['formal_batches']
    p(f'# L2 Mac 门禁批次 {fr["name"]}\n')
    p(f'- head `{hd["commit"]}`（{hd["desc"]}）；anchor `{an["commit"]}`（{an["desc"]}）')
    if fr.get('lib_identity_note'): p(f'- {fr["lib_identity_note"]}')
    p(f'- **结论：{res["verdict"]}**。正式部分 {ov["verdict"]}（合并 {ov["batches"]} 批），SLG {res["slg"]["verdict"] if res["slg"] else "未跑"}。')
    for b in bs:
        if b['excluded'] is not None: p(f'- {b["batch_dir"]} 未计入合并：{b["excluded"]}')
    C = R.get('copies', 1)
    p(f'- 8 个二进制在磁盘上用等长文件名（{", ".join(f"{k}→{v}" for k, v in R["file_names"].items())}）；raw 里的标签不变。'
      + (f'每个二进制和 wire 各存 {C} 份独立文件，第 r 轮用第 r % {C} 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，'
         '历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。' if C > 1 else
         '以前的 formal_r3.py 批次用 xoffset_cand / xoffset_cand_aa，同一二进制的 A/A 在 mutate 上有约 1% 的偏差，疑似路径长度效应。'))
    p(f'- FREEZE.json sha256 `{res["freeze_sha256"]}`；mac_gate.py `{fr["mac_gate_py_sha256"]}`；xc_ab.py `{fr["xc_ab_py_sha256"]}`\n')
    p('## 1. 新口径：head÷anchor（门禁）\n')
    p('逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。'
      'A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。\n')
    p('| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |')
    p('| --- | --- | --- | --- | --- | --- | --- | --- |')
    def row(name, g, aa, verdict=None):  # the pooled row shows the overall verdict (batch count, consistency, A/A)
        return (f'| {name} | ' + ' | '.join(iv4(g['phases'][m]) for m in GATE_PHASES) + f' | {f4(g["max_upper"])} | {verdict or g["verdict"]} | '
                + ' / '.join(f4(aa['phases'][m]['geo']) for m in GATE_PHASES) + f' | {"有效" if aa["ok"] else "**无效**"} |')
    if 'gate' in ov: p(row(f'**合并 {ov["batches"]} 批（判定）**', ov['gate'], ov['gate']['aa'], ov['verdict']))
    for b in bs: p(row(b['batch_dir'] + '（单批，只报）', b['gate'], b['aa']))
    p('\n区间列为 几何均值 [双侧下界, 单侧上界]。\n')
    if ov.get('inconsistent'): p('**两批不一致，未合并判定（待定，先诊断再加跑一批）：** ' + '；'.join(ov['inconsistent']) + '\n')
    p('逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：\n')
    p('| 范围 | ' + ' | '.join(f'{m} p10 / 中位数 / p90' for m in GATE_PHASES) + ' |')
    p('| --- |' + ' --- |' * len(GATE_PHASES))
    qrow = lambda name, g: p(f'| {name} | ' + ' | '.join(' / '.join(f4(g['phases'][m]['cycle_quantiles'][q]) for q in ('p10', 'median', 'p90')) for m in GATE_PHASES) + ' |')
    if 'gate' in ov: qrow('合并', ov['gate'])
    for b in bs: qrow(b['batch_dir'], b['gate'])
    p('')
    p('### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）\n')
    p('| 范围 | ' + ' | '.join(NEW_METRICS) + ' |')
    p('| --- |' + ' --- |' * len(NEW_METRICS))
    for ref in ['anchor', 'r3']:
        if 'report' in ov: p(f'| 合并 head÷{ref} | ' + ' | '.join(iv4(ov['report']['ratios'][f'head/{ref}:{m}']) for m in NEW_METRICS) + ' |')
        for b in bs: p(f'| {b["batch_dir"]} head÷{ref} | ' + ' | '.join(iv4(b['report']['ratios'][f'head/{ref}:{m}']) for m in NEW_METRICS) + ' |')
    p('\nhead÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。\n')
    p('### 旧 norm 项（#30 等合并判据）\n')
    p(f'cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤{R["legacy_norm"]["threshold"]}；单侧 95% 上界（k=2）只报。\n')
    p('| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |')
    p('| --- | --- | --- | --- |')
    if 'report' in ov:
        v = ov['report']['legacy_norm']; p(f'| **合并（判据）** | {f4(v["geo"])} | {f4(v["high"])} | {"是" if v["ok"] else "**否**"} |')
    for b in bs:
        v = b['report']['legacy_norm']; p(f'| {b["batch_dir"]} | {f4(v["geo"])} | {f4(v["high"])} | {"是" if v["ok"] else "否"} |')
    p('\n## 2. 旧口径：ROUND3 12 项（照原样，逐批）\n')
    p('head÷对手，联合单侧 95% 上界 k=12。\n')
    p('| 项 | 门槛 | ' + ' | '.join(b['batch_dir'] for b in bs) + ' |')
    p('| --- | --- |' + ' --- |' * len(bs))
    for comp in RIVALS:
        for m, goal in GOALS:
            k = f'{comp}:{m}'
            p(f'| {k} | {goal} | ' + ' | '.join(f'{f3(b["old"]["items"][k]["geo"])} / {f3(b["old"]["items"][k]["upper"])} {"通过" if b["old"]["items"][k]["pass"] else "**不通过**"}'
                                               for b in bs) + ' |')
    p('| 通过数 head | | ' + ' | '.join(f'{b["old"]["passed"]}/12' for b in bs) + ' |')
    p('| 通过数 anchor（同批） | | ' + ' | '.join(f'{b["old"]["passed_anchor"]}/12' for b in bs) + ' |')
    p('| 通过数 r3（同批） | | ' + ' | '.join(f'{b["old"]["passed_r3"]}/12' for b in bs) + ' |')
    p('| 8 阶段 A/A（旧规则，只报） | | ' + ' | '.join('有效' if b['old']['aa8_valid'] else '无效（' + ', '.join(m for m, v in b['old']['aa8'].items() if not v['ok']) + '）'
                                                     for b in bs) + ' |')
    p('\nreload 辅助比值（head÷对手，双侧 k=8）：\n')
    p('| 批 | ' + ' | '.join(RIVALS) + ' |')
    p('| --- |' + ' --- |' * len(RIVALS))
    for b in bs: p(f'| {b["batch_dir"]} | ' + ' | '.join(iv4(b['old']['auxiliary'][f'{c}:reload']) for c in RIVALS) + ' |')
    if res['slg']:
        s = res['slg']
        p('\n## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家）\n')
        p('### 新口径 head÷anchor\n')
        p('每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4。A、B、C 上界 ≤1.02 通过，D 只报。\n')
        p('| 场景 | head÷anchor | 判定 |')
        p('| --- | --- | --- |')
        for sc in SCEN:
            p(f'| {sc} | {iv4(s["new"][sc])} | {("通过" if s["gate"][sc] else "**不通过**") if sc in s["gate"] else "只报"} |')
        p(f'\n进程数 head {s["n"]["head"]}、anchor {s["n"]["anchor"]}。漂移（最后一个进程÷第一个）：'
          + '；'.join(f'{lab} ' + ' '.join(f'{sc} {f3(v)}' for sc, v in d.items()) for lab, d in s['drift_last_over_first'].items()) + '\n')
        p('### 旧口径 G1–G3（head 进程合并）\n')
        p('每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。\n')
        p('| 场景 | ' + ' | '.join(SLG_BASELINES) + ' |')
        p('| --- |' + ' --- |' * len(SLG_BASELINES))
        for sc in SCEN:
            p(f'| {sc} | ' + ' | '.join(f'{f3(s["old_pooled"][f"{sc}/{b}"]["geo"])} / {f3(s["old_pooled"][f"{sc}/{b}"]["upper"])}'
                                        + (f' ({f3(s["old_first_head_per_cycle"][f"{sc}/{b}"]["upper"])})' if s['old_first_head_per_cycle'] else '')
                                        for b in SLG_BASELINES) + ' |')
    p('\n## 4. 环境\n')
    for q in res['quiet']: p(f'- 开跑条件 {q["label"]}：{q["utc"]} ' + ', '.join(f'{k}={v}' for k, v in q.items() if k not in ('label', 'utc')))
    for b in bs:
        m = b.get('meta', {})
        if m: p(f'- {b["batch_dir"]}：{m.get("start_utc")} → {m.get("end_utc")}，load {m.get("load_start")} → {m.get("load_end")}，CPU 采样 {m.get("cpu")}')
    for b in bs:
        d = b.get('aa_detail')
        if d: p(f'- {b["batch_dir"]} A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：'
                + '，'.join(f'{m} {f3(v["round_sd"])}' for m, v in d.items())
                + (f'；mutate 按副本 ' + ' / '.join(f4(x) for x in d['mutate']['by_copy'].values()) if len(d['mutate']['by_copy']) > 1 else ''))
    p(f'- host {fr["host"]}；冻结 {fr["frozen_utc"]}')
    p('- 二进制 sha256：' + '；'.join(f'{k} `{v[:16]}…`' for k, v in fr['binaries'].items()))
    p(f'- SLG 树 {fr["slg"]["tree"]}（sha256 `{fr["slg"]["tree_sha256"]}`），slg_head `{fr["slg"]["head"][:16]}…`，slg_anch `{fr["slg"]["anchor"][:16]}…`')
    p('- 准备检查：' + ('；'.join(f'{k} exit {v["exit"]}' for k, v in fr['prep'].items()) or '无'))
    return '\n'.join(out) + '\n'


# ---------------------------------------------------------------- diagnostic batches (no gate verdict)

DIAG_RULES = {
    'cycles': 65, 'rounds_per_cycle': 3, 'warmup_cycles': 1, 'copies': COPIES, 'child_timeout_s': 30,
    'seed': 'random.Random(20261023 + 1000 * (batch - 1) + round).shuffle(labels)',
    'phases': GATE_PHASES + ['load', 'reload', 'total'],
    'interval': 'per-cycle mean log ratio (formal_r3.py), batches pooled and per batch; two-sided 95%, Bonferroni '
                'k = (compared labels) x 3 gate phases, the same q for the other phases and for every A/A pair; '
                'plus the median of the per-round ratios (robust to outlier rounds); report only, no verdict',
    'copy_rule': 'every binary and wire is stored as `copies` separate files <label><i>; round r runs copy r % copies',
    'quiet': 'same start conditions as the gate batches',
}


def cmd_diag_freeze(a):
    D = a.dir
    if os.path.exists(D): sys.exit(f'{D} exists')
    bins = {}
    for item in a.bin:  # LABEL=BIN:WIRE
        lab, rest = item.split('=', 1); b, w = rest.rsplit(':', 1); bins[lab] = (b, w)
    aa = dict(x.split(':', 1) for x in a.aa or [])  # AA_LABEL:LABEL, a second copy of LABEL
    for x, y in aa.items():
        if y not in bins or x in bins: sys.exit(f'--aa {x}:{y}: {y} must be a --bin label and {x} a new one')
        bins[x] = bins[y]
    if len({len(l) for l in bins}) != 1: sys.exit('all labels must have the same length (equal path sizes)')
    if a.reference not in bins: sys.exit(f'reference {a.reference} is not a --bin label')
    if a.expect_wire:
        bad = [l for l, (b, w) in bins.items() if h(w) != a.expect_wire]
        if bad: sys.exit(f'input wire differs from {a.expect_wire}: {bad}')
    os.makedirs(D + '/bin'); os.makedirs(D + '/wire')
    for lab, (b, w) in bins.items():
        for i in range(COPIES): shutil.copy2(b, f'{D}/bin/{lab}{i}'); shutil.copy2(w, f'{D}/wire/{lab}{i}.input.wire')
    shutil.copy2(os.path.abspath(__file__), D + '/mac_gate.py'); shutil.copy2(xc_ab.__file__, D + '/xc_ab.py')
    fr = {'schema': SCHEMA + '-diag', 'name': os.path.basename(os.path.normpath(D)), 'note': a.note,
          'labels': sorted(bins), 'reference': a.reference, 'aa': aa,
          'desc': dict(x.split('=', 1) for x in a.desc or []),
          'binaries': {l: h(f'{D}/bin/{l}0') for l in bins}, 'input_wires': {l: h(f'{D}/wire/{l}0.input.wire') for l in bins},
          'trace': a.trace, 'trace_sha256': h(a.trace), 'rules': DIAG_RULES,
          'mac_gate_py_sha256': h(D + '/mac_gate.py'), 'xc_ab_py_sha256': h(D + '/xc_ab.py'),
          'host': platform.platform(), 'frozen_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds')}
    for l in bins:
        if any(h(f'{D}/bin/{l}{i}') != fr['binaries'][l] for i in range(COPIES)): sys.exit(f'freeze: copies of {l} differ')
    json.dump(fr, open(D + '/FREEZE.json', 'w'), indent=1)
    print(f'FREEZE.json written: {D}')
    for k, v in fr['binaries'].items(): print(f'  {k:6s} {v}')


def cmd_diag_formal(a):
    D = a.dir; fr = json.load(open(D + '/FREEZE.json')); R = fr['rules']; labels = fr['labels']; C = R['copies']
    B = f'{D}/formal-{a.batch}'
    if os.path.exists(B): sys.exit(f'{B} exists (no reruns into the same directory)')
    for l in labels:
        if any(h(f'{D}/bin/{l}{i}') != fr['binaries'][l] for i in range(C)): sys.exit(f'{l}: binary changed since FREEZE')
    cycles = a.cycles or R['cycles']
    sampler = start_sampler(D, f'formal-{a.batch}')
    quiet_or_exit(D, f'formal-{a.batch}', sampler, a.quiet_timeout, a.quiet_s)
    os.makedirs(B); t0 = time.time()
    meta = {'batch': a.batch, 'cycles': cycles, 'smoke': cycles != R['cycles'] or a.quiet_s != 180.0,
            'start_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds'), 'load_start': os.getloadavg()}
    try:
        with open(B + '/raw.jsonl', 'w') as f:
            for rnd in range(cycles * R['rounds_per_cycle']):
                order = list(labels); random.Random(20261023 + 1000 * (a.batch - 1) + rnd).shuffle(order)
                for l in order:
                    f.write(json.dumps({'round': rnd, 'cycle': rnd // R['rounds_per_cycle'], 'lib': l, 'copy': rnd % C,
                                        'phase_ns': run_formal_child(D, B, l, rnd % C, fr['trace'], R['child_timeout_s'])}) + '\n')
    finally:
        sampler.stop()
    meta['end_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds'); meta['load_end'] = os.getloadavg()
    meta['cpu'] = cpu_summary(f'{D}/cpu/formal-{a.batch}.jsonl', t0, time.time(), own=labels)
    json.dump(meta, open(B + '/meta.json', 'w'), indent=1)
    print(f'formal-{a.batch} done, cpu {meta["cpu"]}  (run diag-analyze)')


def diag_block(data, R, fr):
    ref, aa = fr['reference'], fr['aa']; rpc = R['rounds_per_cycle']
    comp = [l for l in fr['labels'] if l != ref and l not in aa]
    df = len(cycle_logs(data, R, comp[0], ref, 'mutate')) - 1
    q = tq(1 - 0.05 / (2 * len(comp) * len(GATE_PHASES)), df)
    def one(num, den):
        out = {}
        for m in R['phases']:
            v = interval(cycle_logs(data, R, num, den, m), q, q)
            v['round_median'] = math.exp(st.median(math.log(by[r][num][m] / by[r][den][m])
                                                   for by, ncyc in data for r in range(R['warmup_cycles'] * rpc, ncyc * rpc)))
            out[m] = v
        return out
    return {'df': df, 'k': len(comp) * len(GATE_PHASES), 'compare': {f'{l}/{ref}': one(l, ref) for l in comp},
            'aa': {f'{x}/{y}': one(x, y) for x, y in aa.items()}}


def cmd_diag_analyze(a):
    D = a.dir; fr = json.load(open(D + '/FREEZE.json')); R = fr['rules']; n = len(fr['labels'])
    bdirs = sorted((os.path.join(D, x) for x in os.listdir(D) if x.startswith('formal-')), key=lambda p: int(p.rsplit('-', 1)[1]))
    data = [load_formal(B, R, n) for B in bdirs]
    metas = [json.load(open(B + '/meta.json')) if os.path.exists(B + '/meta.json') else {} for B in bdirs]
    res = {'schema': fr['schema'], 'name': fr['name'], 'pooled': diag_block(data, R, fr),
           'batches': {os.path.basename(B): diag_block([d], R, fr) for B, d in zip(bdirs, data)}, 'meta': metas,
           'smoke': any(m.get('smoke') for m in metas), 'freeze_sha256': h(D + '/FREEZE.json'),
           'quiet': [json.loads(l) for l in open(D + '/quiet.jsonl')] if os.path.exists(D + '/quiet.jsonl') else []}
    json.dump(res, open(D + '/DIAG_RESULT.json', 'w'), indent=1)
    out = [f'# 诊断批次 {fr["name"]}' + ('（冒烟，不是结果）' if res['smoke'] else '') + '\n',
           f'- {fr["note"]}', '- ' + '；'.join(f'{l} = {fr["desc"].get(l, "")} `{fr["binaries"][l][:16]}…`' for l in fr['labels'] if l not in fr['aa']),
           f'- A/A：' + '，'.join(f'{x} 是 {y} 的另一份' for x, y in fr['aa'].items()) + f'；每个二进制 {R["copies"]} 份文件轮换',
           f'- 只报，不判定。区间：逐周期 log 比值，双侧 95%，Bonferroni k={res["pooled"]["k"]}；"轮中位数"是逐轮比值的中位数（不受离群轮影响）',
           f'- FREEZE.json sha256 `{res["freeze_sha256"]}`；mac_gate.py `{fr["mac_gate_py_sha256"]}`\n']
    for scope, blk in [('合并 ' + str(len(bdirs)) + ' 批', res['pooled'])] + list(res['batches'].items()):
        out += [f'## {scope}\n', '| 比值 | ' + ' | '.join(R['phases']) + ' |', '| --- |' + ' --- |' * len(R['phases'])]
        for name, v in list(blk['compare'].items()) + [(k + '（A/A）', v) for k, v in blk['aa'].items()]:
            out.append(f'| {name} | ' + ' | '.join(f'{iv4(v[m])} 轮中位数 {f4(v[m]["round_median"])}' for m in R['phases']) + ' |')
        out.append('')
    out.append('## 环境\n')
    for q in res['quiet']: out.append(f'- 开跑条件 {q["label"]}：' + ', '.join(f'{k}={v}' for k, v in q.items() if k != 'label'))
    for B, m in zip(bdirs, metas): out.append(f'- {os.path.basename(B)}：{m.get("start_utc")} → {m.get("end_utc")}，CPU 采样 {m.get("cpu")}')
    open(D + '/DIAG_RESULT.md', 'w').write('\n'.join(out) + '\n')
    print(f'DIAG_RESULT.json / DIAG_RESULT.md written: {D}')


# ---------------------------------------------------------------- trend

TREND_HEADER = ('| 批次 | head | anchor | 结论 | 合并批数 | 旧口径通过数（逐批） | head÷anchor 合并上界 read_sum / mutate / emit_after | A/A '
                '| SLG head÷anchor A / B / C / D（几何均值≤上界） | G1–G3 最大上界 A / B / C | 报告 |\n'
                '| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n')


def cmd_trend(a):
    res = json.load(open(a.dir + '/RESULT.json'))
    ov, bs, slg = res['formal_overall'], res['formal_batches'], res['slg']
    ub = ' / '.join(f3(ov['gate']['phases'][m]['high']) for m in GATE_PHASES) if 'gate' in ov else ''
    aa = ('有效' if ov['gate']['aa']['ok'] else '无效') if 'gate' in ov else ''
    sl = ' / '.join(f'{f3(slg["new"][s]["geo"])}≤{f3(slg["new"][s]["high"])}' for s in SCEN) if slg else ''
    g = ' / '.join(f3(max(slg['old_pooled'][f'{s}/{b}']['upper'] for b in SLG_BASELINES)) for s in 'ABC') if slg else ''
    row = (f'| {res["name"]} | {res["head"]["commit"][:7]} | {res["anchor"]["commit"][:7]} | {res["verdict"]} | {ov["batches"]} '
           f'| {" ".join(str(b["old"]["passed"]) for b in bs)} | {ub} | {aa} | {sl} | {g} | {a.link or ""} |\n')
    new = not os.path.exists(a.trend)
    with open(a.trend, 'a') as f:
        if new: f.write('# PERF_TREND：L2 Mac 门禁批次\n\n每行一个批次，由 `tools/perf_gate/mac_gate.py trend` 追加。规则见 H3 设计第 7 节和各批 FREEZE.json。\n\n' + TREND_HEADER)
        f.write(row)
    print(row, end='')


# ---------------------------------------------------------------- selftest

def synth_formal(B, R, rng, effect, aa_effect=None, cycles=None):
    base = {'load': 2.2e6, 'query_before': 3.0e6, 'mutate': 3.4e6, 'emit_after': 0.76e6, 'retire_modified_state': 1.0e5,
            'reload': 3.3e6, 'query_after': 3.1e6, 'cleanup': 9e4}
    scale = {'head': 1.0, 'head_aa': 1.0, 'anchor': 1.0, 'r3': 0.98, 'cista': 1.3, 'flatbuffers': 1.5, 'capnproto': 4.0, 'cereal': 0.95}
    os.makedirs(B)
    with open(B + '/raw.jsonl', 'w') as f:
        for rnd in range((cycles or R['cycles']) * R['rounds_per_cycle']):
            drift = 1 + 0.02 * math.sin(rnd / 20)
            for k in LIBS:
                eff = effect if k in ('head', 'head_aa') else {}
                if k == 'head_aa' and aa_effect: eff = {m: eff.get(m, 1.0) * aa_effect.get(m, 1.0) for m in base}
                ph = {m: int(v * scale[k] * drift * eff.get(m, 1.0) * math.exp(rng.gauss(0, 0.05))) for m, v in base.items()}
                f.write(json.dumps({'round': rnd, 'cycle': rnd // 3, 'lib': k, 'copy': rnd % COPIES, 'phase_ns': ph}) + '\n')


def cmd_selftest(a):
    """Synthetic raw data with known effects through analyze, so the statistics can be checked in the cloud."""
    R = RULES['formal']
    checks = []
    def chk(name, cond): checks.append(cond); print(('ok   ' if cond else 'FAIL ') + name)

    def setup(effect, aa_effect=None, nb=2, slg_effect=1.0, seed=1):
        rng = random.Random(seed)  # fixed per case, so the selftest is deterministic
        D = tempfile.mkdtemp(prefix='mac_gate_selftest_')
        fr = {'schema': SCHEMA, 'name': '2026-10-22-selftest', 'head': {'commit': 'h' * 40, 'desc': 'synthetic'},
              'anchor': {'commit': 'a' * 40, 'desc': 'synthetic'}, 'lib_identity_note': '', 'binaries': {k: '0' * 64 for k in LIBS},
              'slg': {'head': '0' * 64, 'anchor': '0' * 64, 'tree': 'synthetic', 'tree_sha256': '0' * 64},
              'mac_gate_py_sha256': h(os.path.abspath(__file__)), 'xc_ab_py_sha256': h(xc_ab.__file__), 'prep': {}, 'rules': RULES,
              'host': platform.platform(), 'frozen_utc': 'n/a'}
        json.dump(fr, open(D + '/FREEZE.json', 'w'))
        for b in range(1, nb + 1): synth_formal(f'{D}/formal-{b}', R, rng, effect, aa_effect)
        S = D + '/slg'; os.makedirs(S); runs = []
        sched = xc_ab.abba(blocks=3, a='head', b='anch')
        slg_base = {'xoffset': {'A': 2.0e6, 'B': 3.4e6, 'C': 0.33e6, 'D': 0.27e6}, 'cereal': {'A': 5e6, 'B': 9e6, 'C': 1e6, 'D': 0.5e6},
                    'protobuf': {'A': 4.5e6, 'B': 9.5e6, 'C': 1.2e6, 'D': 0.55e6}, 'flatbuffers': {'A': 3.8e6, 'B': 8e6, 'C': 0.9e6, 'D': 0.4e6}}
        for i, lab in enumerate(sched):
            os.makedirs(S + '/raw', exist_ok=True); path = f'{S}/raw/{i:03d}_{lab}.jsonl'; proc = math.exp(rng.gauss(0, 0.004))
            with open(path, 'w') as f:
                f.write(json.dumps({'meta': {'scale': 'M', 'seed': 20261021, 'cycles': 41}}) + '\n')
                for c in range(41):
                    for fmt, sc in slg_base.items():
                        for s, v in sc.items():
                            e = slg_effect if (fmt == 'xoffset' and lab == 'head' and s == 'B') else 1.0
                            f.write(json.dumps({'cycle': c, 'format': fmt, 'metric': f'{s}.x', 'ns': int(v * e * proc * math.exp(rng.gauss(0, 0.03)))}) + '\n')
            runs.append({'label': lab, 'index': i, 'raw': os.path.relpath(path, S)})
        json.dump({'schedule': sched, 'runs': runs, 'smoke': False, 'aborted': None}, open(S + '/runs.json', 'w'))
        cmd_analyze(argparse.Namespace(dir=D, exclude=None))
        return D, json.load(open(D + '/RESULT.json'))

    D, res = setup({'mutate': 1.03}, slg_effect=1.03)
    ov = res['formal_overall']
    chk('pooled mutate head/anchor near 1.03', abs(ov['gate']['phases']['mutate']['geo'] - 1.03) < 0.01)
    chk('pooled read_sum head/anchor near 1.00', abs(ov['gate']['phases']['read_sum']['geo'] - 1.0) < 0.01)
    chk('+3% mutate -> fail', ov['verdict'] == 'fail')
    chk('A/A valid', ov['gate']['aa']['ok'])
    chk('head/r3 load near 1/0.98', abs(ov['report']['ratios']['head/r3:load']['geo'] - 1 / 0.98) < 0.01)
    b1 = res['formal_batches'][0]
    chk('old protocol counts 12 items', len(b1['old']['items']) == 12)
    chk('cereal:mutate head/rival near 1.03/0.95', abs(b1['old']['items']['cereal:mutate']['geo'] - 1.03 / 0.95) < 0.02)
    chk('legacy norm equals head/anchor mutate geo', abs(ov['report']['legacy_norm']['geo'] - ov['gate']['phases']['mutate']['geo']) < 1e-12)
    chk('legacy norm fails at +3%', ov['report']['legacy_norm']['ok'] is False)
    chk('SLG B head/anchor near 1.03', abs(res['slg']['new']['B']['geo'] - 1.03) < 0.01)
    chk('SLG A head/anchor near 1.00', abs(res['slg']['new']['A']['geo'] - 1.0) < 0.01)
    chk('SLG B fails the gate', res['slg']['gate']['B'] is False)
    chk('overall verdict fail', res['verdict'] == 'fail')
    t = D + '/PERF_TREND.md'
    cmd_trend(argparse.Namespace(dir=D, trend=t, link='selftest'))
    chk('trend row written', open(t).read().count('\n| 2026-10-22-selftest') == 1)
    _, res = setup({})
    chk('null -> pass', res['verdict'] == 'pass' and res['formal_overall']['report']['legacy_norm']['ok'])
    _, res = setup({}, aa_effect={'mutate': 1.02})
    chk('A/A offset 2% -> invalid', res['verdict'] == 'invalid')
    _, res = setup({'emit_after': 1.018})
    g = res['formal_overall']['gate']['phases']['emit_after']
    chk(f'+1.8% emit_after -> pending (geo {g["geo"]:.4f} ub {g["high"]:.4f})', res['formal_overall']['verdict'] == 'pending')
    _, res = setup({'mutate': 1.022}, nb=3, seed=3)
    md = open(_ + '/RESULT.md').read()
    chk(f'3 batches, +2.2% mutate -> fail, and the pooled row says fail (verdict {res["formal_overall"]["verdict"]})',
        res['formal_overall']['verdict'] == 'fail' and res['formal_overall']['gate']['verdict'] == 'pending' and '**合并 3 批（判定）**' in md and md.split('**合并 3 批（判定）**')[1].split('\n')[0].count('| fail |') == 1)
    _, res = setup({}, nb=1)
    chk('one batch -> incomplete', res['verdict'] == 'incomplete')
    D2 = tempfile.mkdtemp(prefix='mac_gate_selftest_')  # two batches that disagree by 3% are not pooled
    json.dump(json.load(open(D + '/FREEZE.json')), open(D2 + '/FREEZE.json', 'w'))
    synth_formal(D2 + '/formal-1', R, random.Random(5), {}); synth_formal(D2 + '/formal-2', R, random.Random(6), {'mutate': 1.03})
    cmd_analyze(argparse.Namespace(dir=D2, exclude=None)); res = json.load(open(D2 + '/RESULT.json'))
    chk('3% gap between batches -> pending, not pooled', res['formal_overall']['verdict'] == 'pending' and res['formal_overall']['inconsistent'])
    cmd_analyze(argparse.Namespace(dir=D2, exclude=['2:diagnosed'])); res = json.load(open(D2 + '/RESULT.json'))
    chk('excluding a batch leaves one -> incomplete', res['formal_overall']['verdict'] == 'incomplete')
    d = res['formal_batches'][0]['aa_detail']['mutate']
    chk(f'A/A detail: round sd near 0.07 ({d["round_sd"]:.3f}), {COPIES} copies', abs(d['round_sd'] - 0.0707) < 0.01 and len(d['by_copy']) == COPIES)
    chk('screen limits', screen_problems({'WindowServer': 36.3, 'UURemoteServer': 16.0, 'screensharingd': 0.4}) == ['UURemoteServer 16%', 'WindowServer 36%']
        and screen_problems({'WindowServer': 8.0}) == [])
    chk('screen_load parses ps', isinstance(screen_load(), dict))
    print(f'selftest dir {D}')
    sys.exit(0 if all(checks) else 1)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest='cmd', required=True)
    f = sp.add_parser('freeze')
    f.add_argument('dir')
    for k in ['head', 'anchor']:
        f.add_argument(f'--{k}-bin', required=True); f.add_argument(f'--{k}-wire', required=True)
        f.add_argument(f'--{k}-commit', required=True); f.add_argument(f'--{k}-desc', default='')
        f.add_argument(f'--{k}-src', help='library clone, recorded (HEAD, status)')
    f.add_argument('--r3-dir', required=True, help='R3-formal-mac-v1 directory (r3 and the four rivals)')
    f.add_argument('--trace', required=True)
    f.add_argument('--slg-head', required=True); f.add_argument('--slg-anchor', required=True)
    f.add_argument('--slg-tree', required=True); f.add_argument('--slg-tree-sha', required=True)
    f.add_argument('--expect-wire', help='expected sha256 of both M input wires')
    f.add_argument('--lib-note', default='', help='one line on how head and anchor library code relate')
    f.add_argument('--prep', action='append', help='NAME=LOGFILE:EXITCODE of a prep check (repeatable)')
    for name in ['formal', 'slg']:
        s = sp.add_parser(name); s.add_argument('dir')
        if name == 'formal': s.add_argument('--batch', type=int, required=True)
        s.add_argument('--quiet-timeout', type=int, default=1800)
        s.add_argument('--quiet-s', type=float, default=180.0, help='no-compile window; only lowered for a smoke test')
        s.add_argument('--cycles', type=int, help='smoke test only; marks the result as not a gate result')
    s = sp.add_parser('analyze'); s.add_argument('dir')
    s.add_argument('--exclude', action='append', help='N:reason, leave formal-N out of the pooled gate (diagnosed invalid batch)')
    s = sp.add_parser('trend'); s.add_argument('dir'); s.add_argument('--trend', required=True); s.add_argument('--link')
    s = sp.add_parser('diag-freeze', help='diagnostic batch: any equal-length labels, A/A copies, no verdict'); s.add_argument('dir')
    s.add_argument('--bin', action='append', required=True, help='LABEL=BIN:WIRE (repeatable)')
    s.add_argument('--aa', action='append', help='AA_LABEL:LABEL, a second copy of LABEL (repeatable)')
    s.add_argument('--desc', action='append', help='LABEL=description (repeatable)')
    s.add_argument('--reference', required=True); s.add_argument('--trace', required=True)
    s.add_argument('--expect-wire'); s.add_argument('--note', default='')
    s = sp.add_parser('diag-formal'); s.add_argument('dir'); s.add_argument('--batch', type=int, required=True)
    s.add_argument('--quiet-timeout', type=int, default=1800); s.add_argument('--quiet-s', type=float, default=180.0); s.add_argument('--cycles', type=int)
    s = sp.add_parser('diag-analyze'); s.add_argument('dir')
    sp.add_parser('selftest')
    a = ap.parse_args()
    {'freeze': cmd_freeze, 'formal': cmd_formal, 'slg': cmd_slg, 'analyze': cmd_analyze, 'trend': cmd_trend, 'selftest': cmd_selftest,
     'diag-freeze': cmd_diag_freeze, 'diag-formal': cmd_diag_formal, 'diag-analyze': cmd_diag_analyze}[a.cmd](a)


if __name__ == '__main__':
    main()
