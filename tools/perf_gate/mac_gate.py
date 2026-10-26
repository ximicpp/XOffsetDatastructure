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
  slg      DIR  the SLG alternating processes (waits for the quiet gate first); --group 2 only after pending
  amend-slg DIR  write the SLG pending-band rule into a run frozen before it existed
  formal DIR --batch K --replaces N / slg DIR --group G --replace
           the one replacement of a batch or group the environment rule invalidated
  analyze  DIR  RESULT.json + RESULT.md
  trend    DIR --trend PERF_TREND.md   append one row
  bisect   DIR  checkpoint run that failed (frozen with --bisect): next bisection step, or the culprit
  auto     DIR  unattended: formal 1-2, SLG, the pre-registered follow-ups (one replacement per invalid
                batch/group, third batch or SLG group 2 in the pending band), analyze, and the bisection of
                a failing checkpoint; exit 0 pass / 10 fail / 11 pending / 12 invalid / 13 incomplete / 14 n/a
  selftest      synthetic data through the whole analysis
  diag-freeze / diag-formal / diag-analyze   a diagnostic batch with any set of equal-length labels and
                A/A copies (same harness, copies and start conditions; report only, no verdict)
"""
import argparse, datetime, hashlib, json, math, os, platform, random, shutil, statistics as st
import subprocess, sys, tempfile, threading, time
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
# susu 2026-10-26 04:23Z: a connected remote desktop counts as not affecting the timing. Remote-desktop
# processes are still sampled and reported (so a run can be checked afterwards) but no longer gate a
# start or invalidate a batch; WindowServer still does (W0b, T4 formal-1). Runs frozen earlier keep gating.
REMOTE_GATED = False
SCREEN_HOLD_S = 60.0
# T4 formal-1: WindowServer rose to >=30% in 114 of 241 samples after a clean start check (1.2%),
# and only that batch moved. The start check cannot see this, so the screen is sampled during timing.
ENV_INTERVAL_S = 4.0
ENV_MAX_FRAC = 0.05

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
        'fail_gt': 1.03, 'max_groups': 2,
        'pending': 'frozen by 性能优化 2026-10-23 11:53Z: max upper bound of A, B, C in (1.02, 1.03] -> pending: run '
                   'one extra ABBA x3 group (slg-2), pool both groups (Welch on all processes): <=1.02 pass, '
                   'otherwise fail; >1.03 fail. No further groups (no rerun until pass)',
        'trend_adjusted': 'per group, OLS of the process value on process order plus a head indicator; two-sided '
                          '95% k=4; report only (diagnosis of in-session drift), never part of the verdict',
        'old_protocol': 'G1-G3: head processes pooled; per process geo log ratio xoffset/rival on the scenario '
                        'total, run-level t (df = processes-1), one-sided 95%, k=12; plus analyze.py-style '
                        'per-cycle bound of the first head process',
    },
    'quiet': 'before each batch and before the SLG part: no compiler process for 3 minutes and 1-minute '
             'load < 3; then for 60 s (ps pcpu, every 5 s) WindowServer < 20%; remote-desktop processes are '
             'recorded only (susu 2026-10-26 04:23Z: a connected remote does not affect the timing); on timeout '
             '(30 min) nothing is timed',
    'screen': {'max': SCREEN_MAX, 'remote_procs': REMOTE_PROCS, 'remote_max': REMOTE_MAX, 'hold_s': SCREEN_HOLD_S,
               'remote_gated': REMOTE_GATED},
    'identical_binaries': 'frozen by 性能优化 2026-10-25 10:35Z, not retroactive: at freeze the head and anchor binaries '
                          'are compared, the formal pair (sc_xoffset) and the SLG pair (slg_bench) separately. A part whose '
                          'two binaries are byte-identical is not applicable: it is not timed and takes no part in the '
                          'verdict, and the report states the sha256 (T4-r2: slg_head = slg_anch = 3b37bd3f..., so its SLG '
                          'C bound 1.0201 was an A/A of one binary)',
    'checkpoint': {
        'rule': 'merge rules v2 (性能优化 2026-10-26 07:30Z, susu 07:26Z): a checkpoint is a cumulative gate run, head = '
                'next_cpp26, anchor = the previous checkpoint; it is judged by the ordinary formal and SLG rules. At freeze '
                'the binaries of every first-parent commit between them are built and stored (--bisect), so a failing '
                'checkpoint is bisected in the same session',
        'points': 'anchor, the listed commits oldest first, head; consecutive points whose binaries of the failed part(s) are '
                  'identical collapse into the earliest one (a point identical to the anchor or the head takes its value '
                  'without timing)',
        'step': 'the middle point between lo and hi (culprit in (lo, hi]) is frozen as a full gate directory bisect-<k> '
                '(head = that point, anchor = the checkpoint anchor); only the failed part(s) are timed: one formal batch '
                'and/or one SLG group, under the same start, environment and replacement rules',
        'decision': 'failed metrics = gate phases (formal) or scenarios A-C (SLG) whose checkpoint pooled upper bound is above '
                    'pass_le and whose geometric mean is above 1; r = log geo of the checkpoint, m = log geo of the step; '
                    'f = sum(m*r) / sum(r*r); f >= 0.5 -> the culprit is in (lo, mid], otherwise in (mid, hi]',
        'spread': 'a failed metric with its own m/r in (0.25, 0.75) is reported as possibly spread over several merges',
        'max_steps': 3,
        'result': 'hi = lo + 1 -> the culprit is the earliest commit of point hi; after max_steps the remaining range is reported',
    },
    'env_invalid': {
        'rule': 'frozen by 性能优化 2026-10-23 14:51Z, not retroactive: during timing WindowServer and remote-desktop '
                'pcpu are sampled every 4 s; a sample is busy when one of them is at or above its start limit. A formal '
                'batch or SLG group with more than 5% busy samples (or none at all) is invalid by environment. The '
                'script decides it from the samples alone, before computing any result; the batch stays in RESULT, '
                'marked invalid with its sample counts. Each one may be replaced once in the same directory (formal '
                '--replaces N, slg --group G --replace); an invalid replacement stops the run for a report. Nothing '
                'is ever closed; load is only recorded',
        'interval_s': ENV_INTERVAL_S, 'max_busy_frac': ENV_MAX_FRAC, 'screen_max': SCREEN_MAX, 'remote_procs': REMOTE_PROCS,
        'remote_max': REMOTE_MAX, 'max_replacements': 1,
    },
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
    ref = getattr(a, 'ref_bin', None)
    if ref: src['r3'] = (a.ref_bin, a.ref_wire)  # the r3 slot holds another reference build (report only, e.g. a checkpoint)
    problems = []
    for k in ([] if ref else ['r3']) + RIVALS:  # references must be exactly the R3 formal ones
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
    bis = []
    for item in a.bisect or []:  # COMMIT|LABEL|FORMAL_BIN|WIRE|SLG_BIN, oldest first
        f = item.split('|')
        if len(f) != 5: problems.append(f'--bisect {item!r}: want COMMIT|LABEL|FORMAL_BIN|WIRE|SLG_BIN'); continue
        if any(not os.path.isfile(x) for x in f[2:]): problems.append(f'--bisect {f[0]}: missing file'); continue
        bis.append(f)
    if len({f[0] for f in bis}) != len(bis): problems.append('--bisect: a commit is listed twice')
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
    if bis:
        fr['bisect'] = []
        for i, (c, lab, b, w, sl) in enumerate(bis):
            P = f'bisect/{i:02d}-{c[:7]}'; os.makedirs(f'{D}/{P}')
            for x, y in ((b, 'sc'), (w, 'sc.input.wire'), (sl, 'slg')): shutil.copy2(x, f'{D}/{P}/{y}')
            fr['bisect'].append({'commit': c, 'label': lab, 'dir': P, 'formal': h(f'{D}/{P}/sc'), 'wire': h(f'{D}/{P}/sc.input.wire'),
                                 'slg': h(f'{D}/{P}/slg')})
    if ref: fr['reference'] = {'commit': a.ref_commit, 'desc': a.ref_desc or '', 'binary': fr['binaries']['r3'], 'wire': fr['input_wires']['r3']}
    if bis and getattr(a, 'bisect_max_steps', None):
        fr['rules'] = {**fr['rules'], 'checkpoint': {**fr['rules']['checkpoint'], 'max_steps': a.bisect_max_steps}}
    fr.update(getattr(a, 'extra', None) or {})
    for k in src:
        for i in range(1, COPIES):
            if h(f'{D}/bin/{FN[k]}{i}') != fr['binaries'][k] or h(f'{D}/wire/{FN[k]}{i}.input.wire') != fr['input_wires'][k]:
                sys.exit(f'freeze: copy {i} of {k} differs from copy 0')
    json.dump(fr, open(D + '/FREEZE.json', 'w'), indent=1)
    print(f'FREEZE.json written: {D}')
    for k, v in fr['binaries'].items(): print(f'  {k:12s} {v}')
    for x in fr.get('bisect', []):
        print(f'  bisect {x["commit"][:12]} {x["label"]}: formal {x["formal"][:12]} slg {x["slg"][:12]} wire {x["wire"][:12]}')
    for part in ('formal', 'slg'):
        if not_applicable(fr, part): print(f'  {part}: NOT APPLICABLE, head and anchor binaries are identical ({not_applicable(fr, part)}); skip its timing')


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


def screen_problems(load, remote_gated=REMOTE_GATED):
    """Processes over their start limit; remote-desktop processes only where the frozen rule gates them."""
    return [f'{n} {v:.0f}%' for n, v in sorted(load.items())
            if (n in SCREEN_MAX or remote_gated) and v >= SCREEN_MAX.get(n, REMOTE_MAX)]


def remote_gated(D):
    """Runs frozen before 2026-10-26 (no screen.remote_gated in FREEZE.json) keep gating on remote desktops."""
    p = D + '/FREEZE.json'
    return json.load(open(p))['rules'].get('screen', {}).get('remote_gated', True) if os.path.exists(p) else REMOTE_GATED


def remote_active(load):
    return sorted(n for n, v in load.items() if n not in SCREEN_MAX and v >= REMOTE_MAX)


class ScreenSampler(threading.Thread):
    """During timing: screen_load() every interval_s into cpu/<label>.screen.jsonl (environment rule)."""
    def __init__(self, path, interval_s, gated=REMOTE_GATED):
        super().__init__(daemon=True); self.path, self.interval_s, self.halt = path, interval_s, threading.Event()
        self.gated = gated

    def run(self):
        with open(self.path, 'a') as f:
            while True:  # a last sample at stop, so even a short run has two
                load = screen_load()
                f.write(json.dumps({'t': time.time(), 'load': load, 'busy': screen_problems(load, self.gated),
                                    'remote': remote_active(load)}) + '\n'); f.flush()
                if self.halt.is_set(): break
                self.halt.wait(self.interval_s)

    def stop(self): self.halt.set(); self.join(timeout=30)


def env_summary(path, t0, t1, E):
    """The environment rule over [t0, t1] of a ScreenSampler log, decided from the samples alone."""
    recs = [json.loads(l) for l in open(path)] if os.path.exists(path) else []
    recs = [r for r in recs if t0 <= r['t'] <= t1]
    busy = [r for r in recs if r['busy']]; by, mx = {}, {}
    for r in busy:
        for b in r['busy']: by[b.rsplit(' ', 1)[0]] = by.get(b.rsplit(' ', 1)[0], 0) + 1
    for r in recs:
        for n, v in r['load'].items(): mx[n] = max(mx.get(n, 0.0), v)
    frac = len(busy) / len(recs) if recs else 1.0
    rem = sum(1 for r in recs if r.get('remote', remote_active(r['load'])))  # recorded only (susu 2026-10-26)
    return {'samples': len(recs), 'busy_samples': len(busy), 'busy_frac': round(frac, 4), 'busy_by_process': by, 'max_pcpu': mx,
            'remote_active_samples': rem,
            'max_busy_frac': E['max_busy_frac'], 'invalid': frac > E['max_busy_frac'],
            'decided_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds')}


def env_text(e):
    return (f'{e["busy_samples"]}/{e["samples"]} 次采样超限（{e["busy_frac"]:.1%}，上限 {e["max_busy_frac"]:.0%}），'
            f'按进程 {e["busy_by_process"] or "无"}，最高 pcpu {e["max_pcpu"] or "无"}'
            + (f'；远程桌面活动（≥{REMOTE_MAX:.0f}%）{e["remote_active_samples"]}/{e["samples"]} 次采样，只记录' if 'remote_active_samples' in e else ''))


def quiet_or_exit(D, label, sampler, timeout_s, quiet_s=180.0):
    """Block until the start conditions hold (xc_ab.quiet_gate: the sampler has seen no compiler for
    180 s and the 1-minute load is < 3; then WindowServer (and, in runs frozen before 2026-10-26,
    remote-desktop processes) below the limits for 60 s); on timeout record it and exit without timing anything."""
    utc = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds')
    t0 = time.time(); hold = min(SCREEN_HOLD_S, quiet_s); ok_since = None; bad = []; rg = remote_gated(D)
    try:
        while True:
            info = xc_ab.quiet_gate(sampler, load_max=3.0, quiet_s=quiet_s, timeout_s=max(0.0, timeout_s - (time.time() - t0)))()
            screen = screen_load(); bad = screen_problems(screen, rg); now = time.time()
            ok_since = None if bad else (ok_since or now)
            if ok_since is not None and now - ok_since >= hold: break
            if now - t0 > timeout_s:
                raise xc_ab.GateTimeout(f'screen or remote desktop busy after {timeout_s:.0f} s: {", ".join(bad) or "not held " + str(hold) + " s"}')
            time.sleep(5)
        info = {**info, 'waited_s': round(time.time() - t0, 1), 'screen': screen, 'remote_active': remote_active(screen)}
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
    if not_applicable(fr, 'formal'): sys.exit(f'formal not applicable: head and anchor are the same binary ({not_applicable(fr, "formal")}); nothing to time')
    if os.path.exists(B): sys.exit(f'{B} exists (no reruns into the same directory)')
    C = R['copies']
    for k in LIBS:
        if any(h(f'{D}/bin/{FN[k]}{i}') != fr['binaries'][k] for i in range(C)): sys.exit(f'{k}: binary changed since FREEZE')
    E = fr['rules'].get('env_invalid')
    if a.replaces is not None: check_formal_replacement(D, a.replaces, E)
    cycles = a.cycles or R['cycles']; smoke = cycles != R['cycles'] or a.quiet_s != 180.0
    sampler = start_sampler(D, f'formal-{a.batch}')
    quiet_or_exit(D, f'formal-{a.batch}', sampler, a.quiet_timeout, a.quiet_s)
    os.makedirs(B)
    t0 = time.time(); spath = f'{D}/cpu/formal-{a.batch}.screen.jsonl'
    scr = ScreenSampler(spath, E['interval_s'], remote_gated(D)) if E else None
    if scr: scr.start()
    meta = {'batch': a.batch, 'cycles': cycles, 'smoke': smoke, 'start_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds'),
            'load_start': os.getloadavg(), 'replaces': a.replaces}
    try:
        with open(B + '/raw.jsonl', 'w') as f:
            for rnd in range(cycles * R['rounds_per_cycle']):
                order = list(LIBS); random.Random(20261022 + 1000 * (a.batch - 1) + rnd).shuffle(order)
                for k in order:
                    f.write(json.dumps({'round': rnd, 'cycle': rnd // R['rounds_per_cycle'], 'lib': k, 'copy': rnd % C,
                                        'phase_ns': run_formal_child(D, B, FN[k], rnd % C, fr['trace'], R['child_timeout_s'])}) + '\n')
    finally:
        sampler.stop()
        if scr: scr.stop()
    t1 = time.time()
    meta['end_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds'); meta['load_end'] = os.getloadavg()
    meta['cpu'] = cpu_summary(f'{D}/cpu/formal-{a.batch}.jsonl', t0, t1)
    if E: meta['env'] = env_summary(spath, t0, t1, E)  # decided before any timing result is looked at
    json.dump(meta, open(B + '/meta.json', 'w'), indent=1)
    if E and meta['env']['invalid']:
        print(f'formal-{a.batch}: INVALID by environment: {env_text(meta["env"])}; '
              + ('it was the replacement of formal-' + str(a.replaces) + ': stop and report' if a.replaces is not None
                 else f'replace it once: formal {D} --batch <next free> --replaces {a.batch}'))
    b = analyze_formal_batch(B, R)
    print(f'formal-{a.batch}: this batch alone: max UB head/anchor {b["gate"]["max_upper"]:.4f}, A/A ok {b["aa"]["ok"]}, '
          f'old {b["old"]["passed"]}/12, cpu {meta["cpu"]}  (the gate verdict needs the pooled batches: run analyze)')


def formal_metas(D):
    return {int(x.rsplit('-', 1)[1]): json.load(open(f'{D}/{x}/meta.json')) if os.path.exists(f'{D}/{x}/meta.json') else {}
            for x in os.listdir(D) if x.startswith('formal-')}


def formal_aborted(B, R):
    """A batch whose process died before meta.json (e.g. the session dropped): a short or torn raw.jsonl.
    It has no result, so it is reported and skipped; the next batch number takes its place."""
    if os.path.exists(B + '/meta.json'): return None
    good, torn = 0, 0
    for l in open(B + '/raw.jsonl') if os.path.exists(B + '/raw.jsonl') else []:
        try: json.loads(l); good += 1
        except ValueError: torn += 1
    want = R['cycles'] * R['rounds_per_cycle'] * len(LIBS)
    return None if good == want and not torn else {'records': good, 'torn_lines': torn, 'expected': want}


def check_formal_replacement(D, n, E):
    """formal-n may be replaced once, only when the environment rule invalidated it and it is no replacement itself."""
    if not E: sys.exit('this run was frozen without the environment rule: no --replaces')
    ms = formal_metas(D)
    if n not in ms: sys.exit(f'formal-{n} does not exist')
    if not (ms[n].get('env') or {}).get('invalid'): sys.exit(f'formal-{n} is not invalid by environment: no replacement')
    if ms[n].get('replaces') is not None: sys.exit(f'formal-{n} is itself a replacement: stop and report')
    if any(m.get('replaces') == n for m in ms.values()): sys.exit(f'formal-{n} was already replaced once')


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


def slg_rules(D, fr):
    """The SLG rules in force: FREEZE.json's, or RULES_AMEND.json's when the rule was amended before an extra group."""
    p = D + '/RULES_AMEND.json'
    return json.load(open(p))['slg'] if os.path.exists(p) else fr['rules']['slg']


def not_applicable(fr, part):
    """identical_binaries rule: the sha256 when the head and anchor binaries of this part are the same file, else None."""
    if 'identical_binaries' not in fr['rules']: return None
    a, b = (fr['binaries']['head'], fr['binaries']['anchor']) if part == 'formal' else (fr['slg']['head'], fr['slg']['anchor'])
    return a if a == b else None


def slg_groups(D):
    return [S for S in [D + '/slg'] + [f'{D}/slg-{g}' for g in range(2, 10)] if os.path.exists(S + '/runs.json')]


def slg_env(S): return json.load(open(S + '/runs.json')).get('env') or {}


def slg_used(D):
    """The groups that count: each group (slg, slg-2), or its one replacement <group>-r when the environment rule
    invalidated it; plus the environment problems that make the SLG part invalid."""
    used, prob = [], []
    for S in slg_groups(D):
        g, r = os.path.basename(S), S + '-r'
        if not slg_env(S).get('invalid'): used.append(S)
        elif not os.path.exists(r + '/runs.json'):
            prob.append(f'{g} 环境失效，尚未补跑：slg --group {1 if g == "slg" else g.split("-")[1]} --replace')
        elif slg_env(r).get('invalid'): prob.append(f'{g}-r（替换 {g}）环境仍失效：停下报告')
        else: used.append(r)
    return used, prob


def cmd_amend_slg(a):
    """Write the current SLG rule (with the pending band) into a directory frozen before it existed."""
    D = a.dir; fr = json.load(open(D + '/FREEZE.json'))
    if 'fail_gt' in fr['rules']['slg']: sys.exit('FREEZE.json already has the SLG pending band; nothing to amend')
    if os.path.exists(D + '/RULES_AMEND.json'): sys.exit('RULES_AMEND.json exists')
    if len(slg_groups(D)) != 1: sys.exit('amend only between the first SLG group and the extra one')
    am = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds'), 'reason': a.reason,
          'slg': RULES['slg'], 'mac_gate_py_sha256': h(os.path.abspath(__file__))}
    json.dump(am, open(D + '/RULES_AMEND.json', 'w'), indent=1)
    shutil.copy2(os.path.abspath(__file__), D + '/mac_gate_amend.py')
    print(f'RULES_AMEND.json written: {D}')


def cmd_slg(a):
    D = a.dir; fr = json.load(open(D + '/FREEZE.json')); E = fr['rules'].get('env_invalid')
    if not_applicable(fr, 'slg'): sys.exit(f'SLG not applicable: slg_head and slg_anch are the same binary ({not_applicable(fr, "slg")}); nothing to time')
    S = D + '/slg' if a.group == 1 else f'{D}/slg-{a.group}'
    if a.replace:  # the one replacement of a group the environment rule invalidated
        if not E: sys.exit('this run was frozen without the environment rule: no --replace')
        if not os.path.exists(S + '/runs.json') or not slg_env(S).get('invalid'): sys.exit(f'{S} is not invalid by environment: no replacement')
        S += '-r'
    if os.path.exists(S): sys.exit(f'{S} exists (no reruns into the same directory)')
    if a.group > 1 and not a.replace:  # only the pre-registered extra group after a pending first group
        Rs = slg_rules(D, fr)
        if a.group > Rs.get('max_groups', 1): sys.exit(f'group {a.group}: the rule allows {Rs.get("max_groups", 1)} group(s)')
        if len(slg_groups(D)) != a.group - 1: sys.exit(f'group {a.group} needs groups 1..{a.group - 1} first')
        v = analyze_slg(D, Rs)['verdict']
        if v != 'pending': sys.exit(f'SLG so far is {v}, not pending: no extra group')
    for k, f in [('head', 'slg_head'), ('anchor', 'slg_anch')]:
        if h(f'{D}/bin/{f}') != fr['slg'][k]: sys.exit(f'{f}: binary changed since FREEZE')
    lab = os.path.basename(S)
    sampler = start_sampler(D, lab)
    quiet_or_exit(D, lab, sampler, a.quiet_timeout, a.quiet_s)
    cycles = a.cycles or 41
    sched = xc_ab.abba(blocks=3, a='head', b='anch')
    argv = {lab: [f'{D}/bin/slg_{lab}', '--mode', 'bench', '--scale', 'M', '--cycles', str(cycles), '--formats', SLG_FORMATS]
            for lab in SLG_LABELS}
    gate = xc_ab.quiet_gate(sampler, load_max=3.0, quiet_s=a.quiet_s, timeout_s=a.quiet_timeout)
    spath = f'{D}/cpu/{lab}.screen.jsonl'; scr = ScreenSampler(spath, E['interval_s'], remote_gated(D)) if E else None
    t0 = time.time()
    if scr: scr.start()
    try:
        xc_ab.run_schedule(sched, argv, {'SLG_REUSE': '1'}, S, gate=gate,
                           binaries={lab: f'{D}/bin/slg_{lab}' for lab in SLG_LABELS})
    finally:
        sampler.stop()
        if scr: scr.stop()
    doc = json.load(open(S + '/runs.json')); doc['smoke'] = cycles != 41 or a.quiet_s != 180.0
    doc['cpu'] = cpu_summary(f'{D}/cpu/{lab}.jsonl', doc['started'], doc.get('finished', time.time()))
    if E: doc['env'] = env_summary(spath, t0, time.time(), E)  # decided before any timing result is looked at
    json.dump(doc, open(S + '/runs.json', 'w'), indent=1)
    if E and doc['env']['invalid']:
        print(f'{lab}: INVALID by environment: {env_text(doc["env"])}; '
              + ('it was the replacement: stop and report' if a.replace else f'replace it once: slg {D} --group {a.group} --replace'))
    res = analyze_slg(D, slg_rules(D, fr))
    if res.get('aborted'): sys.exit(f'slg aborted: {res["aborted"]}')
    if 'new' not in res: sys.exit(f'slg: verdict {res["verdict"]}: ' + '; '.join(res.get('env_invalid') or []))
    print('slg: ' + '  '.join(f'{s} {res["new"][s]["geo"]:.4f} [{res["new"][s]["low"]:.4f}, {res["new"][s]["high"]:.4f}]' for s in SCEN)
          + f'  verdict {res["verdict"]} ({res["groups"]} group(s) pooled)')


def welch(xs, ys, k):
    r = xc_ab.welch_log_ratio(xs, ys, alpha=0.05, k=k, sided='two', logs=True)
    return {'geo': r['ratio'], 'low': r['lo'], 'high': r['hi'], 'df': r.get('df'), 'n': r['n']}


def trend_adjusted(order, is_head, y, k=4):
    """OLS y = a + b*order + d*is_head; returns exp(d) with a two-sided t interval (report only)."""
    X = [(1.0, float(t), float(hh)) for t, hh in zip(order, is_head)]; n = len(X)
    if n < 5: return None
    xtx = [[sum(r[i] * r[j] for r in X) for j in range(3)] for i in range(3)]
    xty = [sum(r[i] * v for r, v in zip(X, y)) for i in range(3)]
    det = lambda m: (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                     + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]))
    dt = det(xtx)
    if abs(dt) < 1e-12: return None
    inv = [[0.0] * 3 for _ in range(3)]
    for i in range(3):
        for j in range(3):
            minor = [[xtx[r][c] for c in range(3) if c != i] for r in range(3) if r != j]
            inv[i][j] = (-1) ** (i + j) * (minor[0][0] * minor[1][1] - minor[0][1] * minor[1][0]) / dt
    beta = [sum(inv[i][j] * xty[j] for j in range(3)) for i in range(3)]
    res = [v - sum(bb * xx for bb, xx in zip(beta, r)) for r, v in zip(X, y)]
    s2 = sum(e * e for e in res) / (n - 3); se = math.sqrt(s2 * inv[2][2]); q = xc_ab.t_ppf(1 - 0.05 / (2 * k), n - 3)
    return {'geo': math.exp(beta[2]), 'low': math.exp(beta[2] - q * se), 'high': math.exp(beta[2] + q * se),
            'drift_per_process': math.exp(beta[1]), 'df': n - 3}


def slg_group(S):
    """Per-process values of one SLG group (one ABBA x3 run directory)."""
    doc = json.load(open(S + '/runs.json'))
    g = {'dir': os.path.basename(S), 'aborted': doc.get('aborted'), 'smoke': doc.get('smoke', False), 'cpu': doc.get('cpu'), 'env': doc.get('env'),
         'vals': {'head': [], 'anchor': []}, 'order': {'head': [], 'anchor': []}, 'med': {'head': [], 'anchor': []}, 'rival': [], 'first_head': None}
    for i, r in enumerate(doc['runs']):
        lab = SLG_LABELS.get(r['label'], r['label'])
        ns = xc_ab.read_slg_raw(os.path.join(S, r['raw']))
        g['vals'][lab].append({sc: xc_ab.norm_log_mean(ns, 'xoffset', SLG_BASELINES, sc) for sc in SCEN}); g['order'][lab].append(i)
        g['med'][lab].append({sc: st.median(ns['xoffset'][sc + '.total'].values()) / 1e6 for sc in SCEN})
        if lab == 'head':
            g['rival'].append({f'{sc}/{b}': xc_ab.paired_log_ratio([(ns['xoffset'][sc + '.total'][c], ns[b][sc + '.total'][c])
                                                                    for c in sorted(set(ns['xoffset'][sc + '.total']) & set(ns[b][sc + '.total']))])['ratio']
                               for sc in SCEN for b in SLG_BASELINES})
            if g['first_head'] is None: g['first_head'] = ns
    return g


def abba_blocks(g, sc):
    """Per ABBA block (4 consecutive processes): mean head value - mean anchor value; linear drift cancels."""
    lab = {i: ('head', v[sc]) for i, v in zip(g['order']['head'], g['vals']['head'])}
    lab.update({i: ('anchor', v[sc]) for i, v in zip(g['order']['anchor'], g['vals']['anchor'])})
    ds = []
    for b in range(max(lab) // 4 + 1 if lab else 0):
        blk = [lab[i] for i in range(4 * b, 4 * b + 4) if i in lab]
        hs = [v for l, v in blk if l == 'head']; an = [v for l, v in blk if l == 'anchor']
        if len(blk) == 4 and hs and an: ds.append(st.mean(hs) - st.mean(an))
    return ds


def abba_paired(ds, k=4):
    """Report only (性能优化 2026-10-23 14:51Z): t across blocks, two-sided 95% k=4, never part of the verdict."""
    if len(ds) < 2: return None
    m = st.mean(ds); se = st.stdev(ds) / math.sqrt(len(ds)); q = xc_ab.t_ppf(1 - 0.05 / (2 * k), len(ds) - 1)
    return {'geo': math.exp(m), 'low': math.exp(m - q * se), 'high': math.exp(m + q * se), 'blocks': len(ds)}


def analyze_slg(D, Rs=None):
    """SLG part: every group under D (slg, slg-2) pooled for the verdict; per-group numbers and the
    trend-adjusted fit are reported only."""
    Rs = Rs or RULES['slg']
    used, prob = slg_used(D)
    gs = [slg_group(S) for S in used]
    vals = {lab: [v for g in gs for v in g['vals'][lab]] for lab in ('head', 'anchor')}
    envs = {os.path.basename(S): slg_env(S) for G in slg_groups(D) for S in (G, G + '-r') if os.path.exists(S + '/runs.json') and slg_env(S)}
    res = {'groups': len(gs), 'group_dirs': [g['dir'] for g in gs], 'rules': {k: Rs.get(k) for k in ('pass_le', 'fail_gt', 'max_groups')},
           'env_invalid': prob, 'env_by_group': envs,
           'aborted': next((g['aborted'] for g in gs if g['aborted']), None), 'smoke': any(g['smoke'] for g in gs),
           'cpu': [g['cpu'] for g in gs], 'n': {k: len(v) for k, v in vals.items()}, 'process_values': vals,
           'xoffset_median_ms': {lab: [m for g in gs for m in g['med'][lab]] for lab in ('head', 'anchor')}}
    if min(res['n'].values()) < 2:
        res.update(verdict='invalid' if prob else 'incomplete'); return res
    new = {sc: welch([v[sc] for v in vals['head']], [v[sc] for v in vals['anchor']], k=4) for sc in SCEN}
    gate = {sc: new[sc]['high'] <= Rs['pass_le'] for sc in 'ABC'}
    mx = max(new[sc]['high'] for sc in 'ABC')
    if mx <= Rs['pass_le']: verdict = 'pass'
    elif 'fail_gt' in Rs and mx <= Rs['fail_gt'] and len(gs) < Rs.get('max_groups', 1): verdict = 'pending'
    else: verdict = 'fail'
    per_group = []
    for g in gs:
        pg = {'dir': g['dir'], 'n': {k: len(v) for k, v in g['vals'].items()}}
        if min(pg['n'].values()) >= 2:
            pg['new'] = {sc: welch([v[sc] for v in g['vals']['head']], [v[sc] for v in g['vals']['anchor']], k=4) for sc in SCEN}
            order = g['order']['head'] + g['order']['anchor']; ish = [1] * len(g['order']['head']) + [0] * len(g['order']['anchor'])
            pg['trend_adjusted'] = {sc: trend_adjusted(order, ish, [v[sc] for v in g['vals']['head'] + g['vals']['anchor']]) for sc in SCEN}
            pg['drift_last_over_first'] = {lab: {sc: math.exp(v[-1][sc] - v[0][sc]) for sc in SCEN} for lab, v in g['vals'].items()}
            pg['abba'] = {sc: abba_paired(abba_blocks(g, sc)) for sc in SCEN}
        per_group.append(pg)
    rival = [x for g in gs for x in g['rival']]
    old = {}
    for sc in SCEN:  # run-level: one geometric-mean ratio per head process, t across processes
        for b in SLG_BASELINES:
            r = xc_ab.paired_log_ratio([(v[f'{sc}/{b}'], 1.0) for v in rival], alpha=0.05, k=12, sided='upper')
            old[f'{sc}/{b}'] = {'geo': r['ratio'], 'upper': r['hi']}
    per_cycle = {}
    for sc in SCEN:  # tools/analyze.py's per-cycle bound for the first head process (continuity)
        for b in SLG_BASELINES:
            r = xc_ab.per_cycle_ratio(gs[0]['first_head'], 'xoffset', b, sc + '.total', alpha=0.05, k=12, sided='upper')
            per_cycle[f'{sc}/{b}'] = {'geo': r['ratio'], 'upper': r['hi']}
    res.update(new=new, gate=gate, max_upper=mx, verdict=verdict, per_group=per_group, old_pooled=old,
               old_first_head_per_cycle=per_cycle, drift_last_over_first=per_group[0].get('drift_last_over_first', {}),
               abba={sc: abba_paired([d for g in gs for d in abba_blocks(g, sc)]) for sc in SCEN})
    if prob: res['verdict'] = 'invalid'
    if res['aborted']: res['verdict'] = 'incomplete'
    return res


# ---------------------------------------------------------------- analyze / report

def cmd_analyze(a):
    D = a.dir; fr = json.load(open(D + '/FREEZE.json')); R = fr['rules']['formal']
    excl = {int(x.split(':', 1)[0]): x.split(':', 1)[1] if ':' in x else '' for x in (a.exclude or [])}
    bdirs = sorted((os.path.join(D, x) for x in os.listdir(D) if x.startswith('formal-')), key=lambda p: int(p.rsplit('-', 1)[1]))
    batches, aborted = [], []
    for B in bdirs:
        ab = formal_aborted(B, R)
        if ab:
            aborted.append({'batch_dir': os.path.basename(B), **ab}); continue
        b = analyze_formal_batch(B, R)
        b['meta'] = json.load(open(B + '/meta.json')) if os.path.exists(B + '/meta.json') else {}
        n = int(B.rsplit('-', 1)[1]); b['excluded'] = excl.get(n) if n in excl else None
        b['n'] = n; b['env'] = b['meta'].get('env'); b['replaces'] = b['meta'].get('replaces')
        b['env_invalid'] = bool(b['env'] and b['env']['invalid'])
        batches.append(b)
    bdirs = [B for B in bdirs if os.path.basename(B) not in {x['batch_dir'] for x in aborted}]
    used = [(B, b) for B, b in zip(bdirs, batches) if b['excluded'] is None and not b['env_invalid']]
    overall = formal_overall([B for B, _ in used], [b for _, b in used], R)
    env_prob = []  # environment rule: an invalid batch needs its one replacement; an invalid replacement stops the run
    for b in batches:
        if not b['env_invalid']: continue
        if b['replaces'] is not None: env_prob.append(f'formal-{b["n"]}（替换 formal-{b["replaces"]}）环境仍失效：停下报告')
        elif not any(x['replaces'] == b['n'] for x in batches): env_prob.append(f'formal-{b["n"]} 环境失效，尚未补跑：formal --batch <下一个> --replaces {b["n"]}')
    if len(aborted) > 1: env_prob.append(f'{len(aborted)} 批中断（{", ".join(x["batch_dir"] for x in aborted)}）：只允许一批中断后换批号补跑，停下报告')
    overall['env_invalid'] = env_prob; overall['aborted'] = aborted
    if env_prob: overall['verdict'] = 'invalid'
    na_f, na_s = not_applicable(fr, 'formal'), not_applicable(fr, 'slg')
    if na_f: overall, batches = {'verdict': 'n/a', 'batches': 0, 'identical_sha256': na_f}, []
    slg = ({'verdict': 'n/a', 'identical_sha256': na_s} if na_s else
           analyze_slg(D, slg_rules(D, fr)) if slg_groups(D) else None)
    fv = overall['verdict']
    amend = json.load(open(D + '/RULES_AMEND.json')) if os.path.exists(D + '/RULES_AMEND.json') else None
    if fv in ('fail',) or (slg and slg['verdict'] == 'fail'): verdict = 'fail'
    elif fv in ('invalid', 'pending', 'incomplete'): verdict = fv
    elif slg and slg['verdict'] in ('invalid', 'pending', 'incomplete'): verdict = slg['verdict']
    elif slg is None: verdict = 'incomplete'
    elif fv == 'n/a' and slg['verdict'] == 'n/a': verdict = 'n/a (identical binaries)'
    else: verdict = 'pass'  # a part that is n/a (identical binaries) neither passes nor fails the gate
    if any(b['meta'].get('smoke') for b in batches) or (slg and slg.get('smoke')): verdict = 'smoke (not a gate result)'
    res = {'schema': SCHEMA, 'name': fr['name'], 'head': fr['head'], 'anchor': fr['anchor'], 'verdict': verdict,
           'formal_overall': overall, 'formal_batches': batches, 'slg': slg, 'freeze_sha256': h(D + '/FREEZE.json'),
           'rules_amend': amend, 'rules_amend_sha256': h(D + '/RULES_AMEND.json') if amend else None,
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
        if b['excluded'] is not None: p(f'- {b["batch_dir"]} 未计入合并（逐例诊断后排除）：{b["excluded"]}')
        if b.get('env_invalid'): p(f'- {b["batch_dir"]} **环境失效**，未计入合并：' + env_text(b['env'])
                                   + (f'；它是 formal-{b["replaces"]} 的补跑' if b.get('replaces') is not None else ''))
        elif b.get('replaces') is not None: p(f'- {b["batch_dir"]} 是 formal-{b["replaces"]} 的补跑（环境失效后补跑一次）')
    for x in ov.get('aborted', []): p(f'- {x["batch_dir"]} **中断**（进程在写 meta.json 之前退出，raw 只有 {x["records"]}/{x["expected"]} 条记录、{x["torn_lines"]} 行残缺），没有结果，不计入；下一个批号补上')
    for x in ov.get('env_invalid', []): p(f'- **正式部分 invalid：{x}**')
    for x in (res['slg'] or {}).get('env_invalid', []): p(f'- **SLG invalid：{x}**')
    if 'env_invalid' in fr['rules']:
        p(f'- 环境失效规则（{fr["rules"]["env_invalid"]["rule"][:40]}…）：计时期间每 {fr["rules"]["env_invalid"]["interval_s"]:.0f} s 采一次屏幕负载，'
          f'超限采样占比 >{fr["rules"]["env_invalid"]["max_busy_frac"]:.0%} 即失效，同目录最多补跑一次。')
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
    for b in bs: p(row(b['batch_dir'] + ('（环境失效，未计入）' if b.get('env_invalid') else '（单批，只报）'), b['gate'], b['aa']))
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
    if 'reference' in fr:
        rf = fr['reference']
        p(f'\n**本批的 r3 槽不是 R3**：放的是参照 {rf["commit"][:12]}（{rf["desc"]}，sc_xoffset {rf["binary"][:12]}）。表里的 head÷r3 就是 head÷该参照，只报不判。\n')
    else: p('\nhead÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。\n')
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
    if ov.get('identical_sha256'):
        p(f'\n**正式部分不适用**：head 与 anchor 的 sc_xoffset 是同一个二进制（sha256 `{ov["identical_sha256"]}`），不计时，不参与判定。\n')
    if res['slg'] and res['slg'].get('identical_sha256'):
        p(f'\n## 3. SLG M\n\n**SLG 不适用**：slg_head 与 slg_anch 是同一个二进制（sha256 `{res["slg"]["identical_sha256"]}`），不计时，不参与判定。\n')
    elif res['slg'] and 'new' not in res['slg']:
        p(f'\n## 3. SLG M\n\n**SLG 判定：{res["slg"]["verdict"]}**，没有可计入的组（' + '；'.join(res['slg'].get('env_invalid') or ['进程不足']) + '）。\n')
    if res['slg'] and 'new' in res['slg']:
        s = res['slg']
        p(f'\n## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；{s.get("groups", 1)} 组合并）\n')
        if res.get('rules_amend'):
            am = res['rules_amend']
            p(f'- SLG 规则在 {am["utc"]} 补写（RULES_AMEND.json sha256 `{res["rules_amend_sha256"]}`），先于第 2 组计时：{am["reason"]}\n')
        p('### 新口径 head÷anchor\n')
        p('每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。'
          'A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。\n')
        p(f'**SLG 判定：{s["verdict"]}**（A、B、C 最大上界 {f4(s["max_upper"])}）\n' if 'max_upper' in s else '')
        p('| 场景 | head÷anchor（合并） | ≤1.02 |')
        p('| --- | --- | --- |')
        for sc in SCEN:
            p(f'| {sc} | {iv4(s["new"][sc])} | {("是" if s["gate"][sc] else "**否**") if sc in s["gate"] else "只报"} |')
        p('\n按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：\n')
        p('| 组 | ' + ' | '.join(f'{sc} Welch / 趋势调整' for sc in SCEN) + ' | 漂移（最后÷第一个进程）|')
        p('| --- |' + ' --- |' * (len(SCEN) + 1))
        for g in s.get('per_group', []):
            if 'new' not in g: continue
            ta = g['trend_adjusted']
            p(f'| {g["dir"]} | ' + ' | '.join(f'{iv4(g["new"][sc])} / ' + (iv4(ta[sc]) if ta[sc] else '—') for sc in SCEN) + ' | '
              + '；'.join(f'{lab} ' + ' '.join(f'{sc} {f3(v)}' for sc, v in d.items()) for lab, d in g['drift_last_over_first'].items()) + ' |')
        if s.get('abba'):
            p('\nABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：\n')
            p('| 组 | ' + ' | '.join(SCEN) + ' |')
            p('| --- |' + ' --- |' * len(SCEN))
            for g in s.get('per_group', []):
                if g.get('abba'): p(f'| {g["dir"]} | ' + ' | '.join(iv4(g['abba'][sc]) if g['abba'][sc] else '—' for sc in SCEN) + ' |')
            p('| 合并 | ' + ' | '.join(iv4(s['abba'][sc]) if s['abba'][sc] else '—' for sc in SCEN) + ' |')
        p(f'\n进程数 head {s["n"]["head"]}、anchor {s["n"]["anchor"]}。\n')
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
        if b.get('env'): p(f'  - 计时期间屏幕采样：{env_text(b["env"])}，{"**失效**" if b["env"]["invalid"] else "有效"}')
    for g, e in ((res['slg'] or {}).get('env_by_group') or {}).items():
        p(f'- {g} 计时期间屏幕采样：{env_text(e)}，{"**失效**" if e["invalid"] else "有效"}')
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
    sl = (' / '.join(f'{f3(slg["new"][s]["geo"])}≤{f3(slg["new"][s]["high"])}' for s in SCEN) if slg and 'new' in slg
          else '不适用（二进制相同）' if slg and slg.get('identical_sha256') else '')
    g = ' / '.join(f3(max(slg['old_pooled'][f'{s}/{b}']['upper'] for b in SLG_BASELINES)) for s in 'ABC') if slg and 'new' in slg else ''
    if 'identical_sha256' in ov: ub = '不适用（二进制相同）'
    row = (f'| {res["name"]} | {res["head"]["commit"][:7]} | {res["anchor"]["commit"][:7]} | {res["verdict"]} | {ov["batches"]} '
           f'| {" ".join(str(b["old"]["passed"]) for b in bs)} | {ub} | {aa} | {sl} | {g} | {a.link or ""} |\n')
    new = not os.path.exists(a.trend)
    with open(a.trend, 'a') as f:
        if new: f.write('# PERF_TREND：L2 Mac 门禁批次\n\n每行一个批次，由 `tools/perf_gate/mac_gate.py trend` 追加。规则见 H3 设计第 7 节和各批 FREEZE.json。\n\n' + TREND_HEADER)
        f.write(row)
    print(row, end='')


# ---------------------------------------------------------------- checkpoint bisection

def failed_metrics(res):
    """The checkpoint's failed metrics and their log effect r: {('formal', phase) | ('slg', scenario): r}."""
    out, ov, slg = {}, res['formal_overall'], res['slg']
    if ov.get('verdict') == 'fail':
        pl = res.get('rules_formal_pass_le', 1.02)
        for m, v in ov['gate']['phases'].items():
            if v['high'] > pl and v['geo'] > 1: out[('formal', m)] = math.log(v['geo'])
    if slg and slg.get('verdict') == 'fail':
        for sc in 'ABC':
            v = slg['new'][sc]
            if v['high'] > slg['rules']['pass_le'] and v['geo'] > 1: out[('slg', sc)] = math.log(v['geo'])
    return out


def bisect_points(fr, parts):
    """anchor, the --bisect commits, head; consecutive identical binaries (for the failed parts) collapse to the earliest."""
    key = lambda f, s: tuple(x for p, x in (('formal', f), ('slg', s)) if p in parts)
    raw = ([{'commit': fr['anchor']['commit'], 'label': 'anchor', 'formal': fr['binaries']['anchor'], 'slg': fr['slg']['anchor'], 'role': 'anchor'}]
           + [{**x, 'role': 'commit'} for x in fr['bisect']]
           + [{'commit': fr['head']['commit'], 'label': 'head', 'formal': fr['binaries']['head'], 'slg': fr['slg']['head'], 'role': 'head'}])
    pts = []
    for x in raw:
        if pts and key(pts[-1]['formal'], pts[-1]['slg']) == key(x['formal'], x['slg']):
            pts[-1].setdefault('same', []).append(x['commit']); continue
        pts.append(dict(x))
    return pts


def step_effects(S, fr_step, metrics):
    """log geo of the step's head over anchor for each failed metric, or (None, reason) while it is not measured."""
    out, need = {}, []
    parts = {p for p, _ in metrics}
    if 'formal' in parts:
        R = fr_step['rules']['formal']; bs = []
        for n in sorted(int(x.rsplit('-', 1)[1]) for x in os.listdir(S) if x.startswith('formal-')):
            B = f'{S}/formal-{n}'
            if formal_aborted(B, R) or not os.path.exists(B + '/meta.json'): continue
            m = json.load(open(B + '/meta.json'))
            if m.get('smoke') or (m.get('env') or {}).get('invalid'): continue
            bs.append(B)
        if bs:
            b = analyze_formal_batch(bs[0], R)
            out.update({('formal', m): math.log(b['gate']['phases'][m]['geo']) for p, m in metrics if p == 'formal'})
            out['_formal'] = {'batch': os.path.basename(bs[0]), 'phases': b['gate']['phases'], 'aa_ok': b['aa']['ok']}
        else: need.append(f'formal {S} --batch <下一个>（环境失效就 --replaces N）')
    if 'slg' in parts:
        used, prob = slg_used(S)
        r = analyze_slg(S, slg_rules(S, fr_step)) if used else {}
        if prob or 'new' not in r or r.get('smoke'): need.append(f'slg {S}' + ('（' + '；'.join(prob) + '）' if prob else ''))
        else:
            out.update({('slg', sc): math.log(r['new'][sc]['geo']) for p, sc in metrics if p == 'slg'})
            out['_slg'] = {'groups': r['group_dirs'], 'new': r['new']}
    return out, need


def cmd_bisect(a):
    D = os.path.normpath(a.dir); fr = json.load(open(D + '/FREEZE.json')); C = fr['rules'].get('checkpoint') or RULES['checkpoint']
    if 'bisect' not in fr: sys.exit('not a checkpoint run: frozen without --bisect')
    if not os.path.exists(D + '/RESULT.json'): sys.exit('run analyze first')
    res = json.load(open(D + '/RESULT.json')); res['rules_formal_pass_le'] = fr['rules']['formal']['gate']['pass_le']
    if res['verdict'] != 'fail': sys.exit(f'checkpoint verdict is {res["verdict"]}, not fail: nothing to bisect')
    metrics = failed_metrics(res)
    if not metrics: sys.exit('the checkpoint failed but no failed metric has a geometric mean above 1: no direction to bisect, report')
    parts = {p for p, _ in metrics}
    pts = bisect_points(fr, parts)
    lo, hi, steps, log = 0, len(pts) - 1, [], []
    rr = sum(r * r for r in metrics.values())
    def value(i, S=None, frs=None):
        """Per-metric m for point i: known for anchor/head binaries, else measured in step S."""
        known = {}
        for (p, m), r in metrics.items():
            if pts[i][p] == pts[0][p]: known[(p, m)] = 0.0
            elif pts[i][p] == pts[-1][p]: known[(p, m)] = r
        if len(known) == len(metrics): return known, [], {}
        if S is None: return None, ['freeze'], {}
        eff, need = step_effects(S, frs, [k for k in metrics if k not in known])
        if need: return None, need, {}
        isd = lambda x: isinstance(x, str)  # '_formal' / '_slg' details next to the (part, metric) effects
        return {**known, **{k: v for k, v in eff.items() if not isd(k)}}, [], {k: v for k, v in eff.items() if isd(k)}
    k = 0
    while hi - lo > 1:
        mid = (lo + hi) // 2
        vals, need, detail = value(mid)
        S = None
        if vals is None:
            k += 1
            S = f'{D}/bisect-{k}'
            if not os.path.exists(S):
                if k > C['max_steps']: break
                p = pts[mid]
                cmd_freeze(argparse.Namespace(
                    dir=S, head_bin=f'{D}/{p["dir"]}/sc', head_wire=f'{D}/{p["dir"]}/sc.input.wire', head_commit=p['commit'],
                    head_desc=f'checkpoint {fr["name"]} bisection step {k}: {p["label"]}', head_src=None,
                    anchor_bin=f'{D}/bin/anch0', anchor_wire=f'{D}/wire/anch0.input.wire', anchor_commit=fr['anchor']['commit'],
                    anchor_desc=fr['anchor']['desc'], anchor_src=None, r3_dir=fr['r3_dir'], trace=fr['trace'],
                    slg_head=f'{D}/{p["dir"]}/slg', slg_anchor=f'{D}/bin/slg_anch', slg_tree=fr['slg']['tree'],
                    slg_tree_sha=fr['slg']['tree_sha256'], expect_wire=None, prep=None, bisect=None,
                    **({'ref_bin': f'{D}/bin/r3fz0', 'ref_wire': f'{D}/wire/r3fz0.input.wire', 'ref_commit': fr['reference']['commit'],
                        'ref_desc': fr['reference']['desc']} if 'reference' in fr else {}),
                    lib_note=f'bisection step {k} of checkpoint {fr["name"]} (FREEZE sha256 {h(D + "/FREEZE.json")}): {p["commit"]} {p["label"]}',
                    extra={'bisect_step': {'k': k, 'parent': fr['name'], 'parent_freeze_sha256': h(D + '/FREEZE.json'), 'point': p['commit'],
                                           'lo': pts[lo]['commit'], 'hi': pts[hi]['commit'], 'parts': sorted(parts)}}))
            frs = json.load(open(S + '/FREEZE.json'))
            if (frs.get('bisect_step') or {}).get('point') != pts[mid]['commit']: sys.exit(f'{S} times {frs.get("bisect_step")}, expected {pts[mid]["commit"]}: stop and report')
            vals, need, detail = value(mid, S, frs)
            if vals is None:
                log.append({'step': k, 'dir': os.path.basename(S), 'point': pts[mid]['commit'], 'label': pts[mid]['label'], 'waiting': need})
                break
        f = sum(vals[x] * r for x, r in metrics.items()) / rr
        own = {f'{p}:{m}': (vals[(p, m)] / r) for (p, m), r in metrics.items()}
        log.append({'step': k if S else None, 'dir': os.path.basename(S) if S else None, 'point': pts[mid]['commit'], 'label': pts[mid]['label'],
                    'lo': pts[lo]['commit'], 'hi': pts[hi]['commit'], 'f': f, 'own': own,
                    'spread': [x for x, v in own.items() if 0.25 < v < 0.75], 'side': 'lo-mid' if f >= 0.5 else 'mid-hi',
                    'timed': S is not None, **detail})
        if f >= 0.5: hi = mid
        else: lo = mid
    done = hi - lo == 1
    waiting = bool(log and 'waiting' in log[-1])
    out = {'checkpoint': fr['name'], 'failed_metrics': {f'{p}:{m}': math.exp(r) for (p, m), r in metrics.items()},
           'points': [{k2: v for k2, v in x.items() if k2 in ('commit', 'label', 'role', 'same')} for x in pts], 'steps': log,
           'range': {'lo': pts[lo]['commit'], 'hi': pts[hi]['commit'], 'lo_label': pts[lo]['label'], 'hi_label': pts[hi]['label']},
           'status': 'culprit' if done else 'waiting' if waiting else 'max_steps', 'rules': C}
    if done: out['culprit'] = {'commit': pts[hi]['commit'], 'label': pts[hi]['label'], 'same_binaries': pts[hi].get('same', [])}
    json.dump(out, open(D + '/BISECT.json', 'w'), indent=1)
    open(D + '/BISECT.md', 'w').write(render_bisect(out))
    if done: print(f'DONE culprit {pts[hi]["commit"]} {pts[hi]["label"]}')
    elif waiting:
        print(f'NEXT bisection step {log[-1]["step"]} ({log[-1]["label"]} {log[-1]["point"][:12]}): ' + '; '.join(log[-1]['waiting'])
              + f'; then: bisect {D}')
    else: print(f'STOP after {C["max_steps"]} steps: culprit in ({pts[lo]["commit"][:12]} {pts[lo]["label"]}, {pts[hi]["commit"][:12]} {pts[hi]["label"]}]')


def render_bisect(o):
    L = [f'# 阶段复核 {o["checkpoint"]} 二分', '',
         '失败指标（复核点 head÷anchor 合并几何均值）：' + '，'.join(f'{k} {f4(v)}' for k, v in o['failed_metrics'].items()), '',
         '判定：每步的 head÷anchor 按失败指标投影到复核点的回退上，f = Σ(m·r)/Σ(r²)；f ≥ 0.5 说明回退在这一点之前（含这一点），否则在这一点之后。'
         f'最多 {o["rules"]["max_steps"]} 步计时。', '', '| 点 | 提交 | 说明 | 同一二进制的后续提交 |', '| --- | --- | --- | --- |']
    L += [f'| {i} | {x["commit"][:12]} | {x["label"]} | {" ".join(c[:7] for c in x.get("same", []))} |' for i, x in enumerate(o['points'])]
    L += ['', '| 步 | 点 | 范围 (lo, hi] | f | 各指标 m/r | 方向 | 计时 |', '| --- | --- | --- | --- | --- | --- | --- |']
    for s in o['steps']:
        if 'waiting' in s: L.append(f'| {s["step"]} | {s["point"][:7]} {s["label"]} | | | | 等待计时：{"；".join(s["waiting"])} | |'); continue
        own = '，'.join(f'{k} {v:.2f}' for k, v in s['own'].items())
        tm = (s['dir'] + ('（' + s['_formal']['batch'] + '）' if '_formal' in s else '')) if s['timed'] else '不计时（二进制与 anchor 或 head 相同）'
        L.append(f'| {s["step"] or "—"} | {s["point"][:7]} {s["label"]} | ({s["lo"][:7]}, {s["hi"][:7]}] | {s["f"]:.2f} | {own} | '
                 f'{"回退在此点及之前" if s["side"] == "lo-mid" else "回退在此点之后"} | {tm} |')
        if s['spread']: L.append(f'|  |  |  |  | 可能分散在多个合并里：{"，".join(s["spread"])} |  |  |')
    L.append('')
    r = o['range']
    if o['status'] == 'culprit':
        c = o['culprit']
        L.append(f'**结论：回退来自 {c["commit"][:12]}（{c["label"]}）**' + (f'，与它二进制相同的后续提交 {" ".join(x[:7] for x in c["same_binaries"])} 一并列出' if c['same_binaries'] else '') + '。')
    elif o['status'] == 'waiting': L.append(f'**进行中**：回退在 ({r["lo"][:12]} {r["lo_label"]}, {r["hi"][:12]} {r["hi_label"]}] 内，等下一步计时。')
    else: L.append(f'**已用完 {o["rules"]["max_steps"]} 步**：回退在 ({r["lo"][:12]} {r["lo_label"]}, {r["hi"][:12]} {r["hi_label"]}] 内，交「性能优化」决定。')
    return '\n'.join(L) + '\n'


# ---------------------------------------------------------------- unattended run

AUTO_RC = {'pass': 0, 'fail': 10, 'pending': 11, 'invalid': 12, 'incomplete': 13}


def auto_formal(D, n, a, replaces=None):
    """One formal batch; when the environment rule invalidates it, its one replacement (next number). Returns the next free number."""
    cmd_formal(argparse.Namespace(dir=D, batch=n, replaces=replaces, quiet_timeout=a.quiet_timeout, quiet_s=a.quiet_s, cycles=a.cycles))
    m = json.load(open(f'{D}/formal-{n}/meta.json'))
    if (m.get('env') or {}).get('invalid'):
        if replaces is not None: return None  # an invalid replacement: stop (analyze reports invalid)
        return auto_formal(D, n + 1, a, replaces=n)
    return n + 1


def auto_slg(D, g, a):
    cmd_slg(argparse.Namespace(dir=D, group=g, replace=False, quiet_timeout=a.quiet_timeout, quiet_s=a.quiet_s, cycles=a.cycles))
    S = D + '/slg' if g == 1 else f'{D}/slg-{g}'
    if slg_env(S).get('invalid'):
        cmd_slg(argparse.Namespace(dir=D, group=g, replace=True, quiet_timeout=a.quiet_timeout, quiet_s=a.quiet_s, cycles=a.cycles))


def auto_gate(D, a):
    """The ordinary gate of one directory, follow-ups as the frozen rules pre-register them. Returns RESULT.json."""
    fr = json.load(open(D + '/FREEZE.json')); R = fr['rules']['formal']
    an = lambda: (cmd_analyze(argparse.Namespace(dir=D, exclude=None)), json.load(open(D + '/RESULT.json')))[1]
    if not not_applicable(fr, 'formal'):
        nxt = 1
        for _ in range(R['batches']):
            nxt = auto_formal(D, nxt, a)
            if nxt is None: return an()
        res = an(); ov = res['formal_overall']
        if ov['verdict'] == 'pending' and not ov.get('inconsistent'):  # pending band: one more batch, pool all
            if auto_formal(D, nxt, a) is None: return an()
    if not not_applicable(fr, 'slg'):
        auto_slg(D, 1, a)
        res = an()
        if (res['slg'] or {}).get('verdict') == 'pending': auto_slg(D, 2, a)
    return an()


def cmd_auto(a):
    D = os.path.normpath(a.dir); res = auto_gate(D, a)
    v = res['verdict']; print(f'AUTO {D}: verdict {v}')
    fr = json.load(open(D + '/FREEZE.json'))
    if v == 'fail' and 'bisect' in fr:
        for _ in range(10):
            cmd_bisect(argparse.Namespace(dir=D)); o = json.load(open(D + '/BISECT.json'))
            if o['status'] != 'waiting': break
            S = f'{D}/{o["steps"][-1]["dir"]}'; frs = json.load(open(S + '/FREEZE.json')); parts = frs['bisect_step']['parts']
            if 'formal' in parts and not not_applicable(frs, 'formal'):
                done = [x for x in os.listdir(S) if x.startswith('formal-')]
                if done: sys.exit(f'AUTO {S}: formal batch already there but not usable (invalid replacement or aborted): stop and report')
                if auto_formal(S, 1, a) is None: sys.exit(f'AUTO {S}: replacement invalid by environment: stop and report')
            if 'slg' in parts and not not_applicable(frs, 'slg'):
                if slg_groups(S): sys.exit(f'AUTO {S}: SLG group already there but not usable: stop and report')
                auto_slg(S, 1, a)
                if slg_used(S)[1]: sys.exit(f'AUTO {S}: SLG replacement invalid by environment: stop and report')
        print(f'AUTO bisect: {o["status"]}' + (f' -> {o["culprit"]["commit"]} {o["culprit"]["label"]}' if o['status'] == 'culprit' else ''))
    sys.exit(AUTO_RC.get(v, 14 if v.startswith('n/a') else 13))


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
              'anchor': {'commit': 'a' * 40, 'desc': 'synthetic'}, 'lib_identity_note': '', 'binaries': {k: ('1' if k.startswith('head') else '0') * 64 for k in LIBS},
              'slg': {'head': '1' * 64, 'anchor': '0' * 64, 'tree': 'synthetic', 'tree_sha256': '0' * 64},
              'mac_gate_py_sha256': h(os.path.abspath(__file__)), 'xc_ab_py_sha256': h(xc_ab.__file__), 'prep': {}, 'rules': RULES,
              'host': platform.platform(), 'frozen_utc': 'n/a'}
        json.dump(fr, open(D + '/FREEZE.json', 'w'))
        for b in range(1, nb + 1): synth_formal(f'{D}/formal-{b}', R, rng, effect, aa_effect)
        synth_slg(D + '/slg', rng, slg_effect)
        cmd_analyze(argparse.Namespace(dir=D, exclude=None))
        return D, json.load(open(D + '/RESULT.json'))

    def synth_slg(S, rng, slg_effect, scen='B'):
        os.makedirs(S); runs = []
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
                            e = slg_effect if (fmt == 'xoffset' and lab == 'head' and s == scen) else 1.0
                            f.write(json.dumps({'cycle': c, 'format': fmt, 'metric': f'{s}.x', 'ns': int(v * e * proc * math.exp(rng.gauss(0, 0.03)))}) + '\n')
            runs.append({'label': lab, 'index': i, 'raw': os.path.relpath(path, S)})
        json.dump({'schedule': sched, 'runs': runs, 'smoke': False, 'aborted': None}, open(S + '/runs.json', 'w'))

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
    # SLG pending band: find a seed whose first group lands in (1.02, 1.03], then the extra group decides
    for sd in range(40):
        Dp, res = setup({}, slg_effect=1.016, seed=100 + sd)
        if res['slg']['verdict'] == 'pending': break
    chk(f'SLG +1.6% -> pending after one group (max UB {res["slg"]["max_upper"]:.4f}), overall pending', res['slg']['verdict'] == 'pending' and res['verdict'] == 'pending')
    synth_slg(Dp + '/slg-2', random.Random(7), 1.016)
    cmd_analyze(argparse.Namespace(dir=Dp, exclude=None)); res = json.load(open(Dp + '/RESULT.json'))
    chk(f'two groups pooled, +1.6% -> fail (max UB {res["slg"]["max_upper"]:.4f})', res['slg']['groups'] == 2 and res['slg']['verdict'] == 'fail')
    r3 = analyze_slg(Dp, RULES['slg'])
    chk('no third group: the rule allows 2', r3['verdict'] in ('pass', 'fail') and RULES['slg']['max_groups'] == 2)
    for sd in range(40):
        Dq, res = setup({}, slg_effect=1.0, seed=300 + sd)
        if res['slg']['verdict'] == 'pass': break
    try:
        cmd_slg(argparse.Namespace(dir=Dq, group=2, replace=False, quiet_timeout=1, quiet_s=0.0, cycles=4)); refused = False
    except SystemExit as e:
        refused = 'not pending' in str(e)
    chk('extra group refused when the first group passed', refused)
    ta = res['slg']['per_group'][0]['trend_adjusted']['A']
    chk(f'trend-adjusted fit reported (A {ta["geo"]:.4f})', abs(ta['geo'] - 1) < 0.02 and ta['low'] < ta['high'])
    d = res['formal_batches'][0]['aa_detail']['mutate']
    chk(f'A/A detail: round sd near 0.07 ({d["round_sd"]:.3f}), {COPIES} copies', abs(d['round_sd'] - 0.0707) < 0.01 and len(d['by_copy']) == COPIES)
    chk('screen limits: remote recorded only, WindowServer gates', screen_problems({'WindowServer': 36.3, 'UURemoteServer': 16.0, 'screensharingd': 0.4}) == ['WindowServer 36%']
        and screen_problems({'WindowServer': 8.0, 'UURemoteServer': 16.0}) == [] and remote_active({'WindowServer': 36.3, 'UURemoteServer': 16.0}) == ['UURemoteServer'])
    chk('runs frozen before 2026-10-26 keep the remote gate', screen_problems({'WindowServer': 8.0, 'UURemoteServer': 16.0}, True) == ['UURemoteServer 16%']
        and RULES['screen']['remote_gated'] is False)
    chk('screen_load parses ps', isinstance(screen_load(), dict))
    # environment rule (frozen 2026-10-23 14:51Z): busy fraction > 5% invalidates; one replacement; never retroactive
    E = RULES['env_invalid']; ep = tempfile.mkdtemp(prefix='mac_gate_selftest_') + '/s.jsonl'
    with open(ep, 'w') as f:
        for i in range(41): f.write(json.dumps({'t': 100.0 + i, 'load': {'WindowServer': 31.0 if i < 3 else 2.0},
                                                 'busy': ['WindowServer 31%'] if i < 3 else []}) + '\n')
    e1 = env_summary(ep, 100.0, 139.0, E); e2 = env_summary(ep, 101.0, 140.0, E); e0 = env_summary(ep, 500.0, 600.0, E)
    chk(f'env: 3/40 busy -> invalid, 2/40 (exactly 5%) -> valid, no samples -> invalid',
        e1['invalid'] and e1['busy_by_process'] == {'WindowServer': 3} and not e2['invalid'] and e0['invalid'] and e0['samples'] == 0)
    ss = ScreenSampler(ep + '.live', 0.05); ss.start(); time.sleep(0.3); ss.stop()
    chk('screen sampler writes samples', sum(1 for _ in open(ep + '.live')) >= 2)
    Da, res = setup({}, nb=3)  # formal-2 torn by a dropped session: skipped, formal-1 + formal-3 pooled
    L = open(Da + '/formal-2/raw.jsonl').read().splitlines()
    open(Da + '/formal-2/raw.jsonl', 'w').write('\n'.join(L[:len(L) // 2]) + '\n{"round": 9')
    cmd_analyze(argparse.Namespace(dir=Da, exclude=None)); res = json.load(open(Da + '/RESULT.json'))
    chk(f'aborted batch skipped, two pooled ({res["verdict"]})', res['verdict'] == 'pass' and res['formal_overall']['batches'] == 2
        and [x['batch_dir'] for x in res['formal_overall']['aborted']] == ['formal-2'] and '中断' in open(Da + '/RESULT.md').read())
    open(Da + '/formal-3/raw.jsonl', 'w').write('\n'.join(L[:10]) + '\n')
    cmd_analyze(argparse.Namespace(dir=Da, exclude=None)); res = json.load(open(Da + '/RESULT.json'))
    chk('a second aborted batch -> invalid, stop', res['verdict'] == 'invalid' and len(res['formal_overall']['aborted']) == 2)
    De, res = setup({})
    inv = {'samples': 100, 'busy_samples': 40, 'busy_frac': 0.4, 'busy_by_process': {'WindowServer': 40}, 'max_pcpu': {'WindowServer': 45.0},
           'max_busy_frac': 0.05, 'invalid': True, 'decided_utc': 'n/a'}
    ok_env = {**inv, 'busy_samples': 0, 'busy_frac': 0.0, 'busy_by_process': {}, 'invalid': False}
    json.dump({'batch': 1, 'env': inv, 'replaces': None}, open(De + '/formal-1/meta.json', 'w'))
    cmd_analyze(argparse.Namespace(dir=De, exclude=None)); res = json.load(open(De + '/RESULT.json'))
    chk('env-invalid batch, not yet replaced -> invalid, still reported', res['verdict'] == 'invalid' and res['formal_overall']['env_invalid']
        and res['formal_batches'][0]['env_invalid'] and '环境失效' in open(De + '/RESULT.md').read())
    def refused(fn):
        try: fn(); return None
        except SystemExit as e: return str(e)
    chk('replacement allowed for an env-invalid batch', refused(lambda: check_formal_replacement(De, 1, E)) is None)
    chk('no replacement when frozen without the rule', 'without the environment rule' in (refused(lambda: check_formal_replacement(De, 1, None)) or ''))
    chk('no replacement for a valid batch', 'not invalid' in (refused(lambda: check_formal_replacement(De, 2, E)) or ''))
    synth_formal(De + '/formal-3', R, random.Random(11), {})
    json.dump({'batch': 3, 'env': ok_env, 'replaces': 1}, open(De + '/formal-3/meta.json', 'w'))
    cmd_analyze(argparse.Namespace(dir=De, exclude=None)); res = json.load(open(De + '/RESULT.json'))
    chk(f'replaced -> batches 2+3 pooled as two ({res["verdict"]})', res['verdict'] == 'pass' and res['formal_overall']['batches'] == 2)
    chk('second replacement refused', 'already replaced' in (refused(lambda: check_formal_replacement(De, 1, E)) or ''))
    json.dump({'batch': 3, 'env': inv, 'replaces': 1}, open(De + '/formal-3/meta.json', 'w'))
    cmd_analyze(argparse.Namespace(dir=De, exclude=None)); res = json.load(open(De + '/RESULT.json'))
    chk('invalid replacement -> invalid, stop', res['verdict'] == 'invalid' and any('停下' in x for x in res['formal_overall']['env_invalid'])
        and 'itself a replacement' in (refused(lambda: check_formal_replacement(De, 3, E)) or ''))
    Ds, res = setup({})
    doc = json.load(open(Ds + '/slg/runs.json')); doc['env'] = inv; json.dump(doc, open(Ds + '/slg/runs.json', 'w'))
    cmd_analyze(argparse.Namespace(dir=Ds, exclude=None)); res = json.load(open(Ds + '/RESULT.json'))
    chk('env-invalid SLG group -> invalid', res['slg']['verdict'] == 'invalid' and res['verdict'] == 'invalid')
    synth_slg(Ds + '/slg-r', random.Random(12), 1.0)
    cmd_analyze(argparse.Namespace(dir=Ds, exclude=None)); res = json.load(open(Ds + '/RESULT.json'))
    chk(f'SLG replacement counts as group 1 ({res["slg"]["verdict"]}, groups {res["slg"]["group_dirs"]})',
        res['slg']['group_dirs'] == ['slg-r'] and res['slg']['verdict'] in ('pass', 'pending') and 'slg' in res['slg']['env_by_group'])
    chk('second SLG replacement refused', 'exists' in (refused(lambda: cmd_slg(argparse.Namespace(dir=Ds, group=1, replace=True, quiet_timeout=1, quiet_s=0.0, cycles=4))) or ''))
    doc = json.load(open(Ds + '/slg-r/runs.json')); doc['env'] = inv; json.dump(doc, open(Ds + '/slg-r/runs.json', 'w'))
    res = analyze_slg(Ds, RULES['slg'])
    chk('invalid SLG replacement -> invalid, stop', res['verdict'] == 'invalid' and any('停下' in x for x in res['env_invalid']))
    old = {**RULES}; old.pop('env_invalid')
    fr = json.load(open(Dq + '/FREEZE.json')); fr['rules'] = old; json.dump(fr, open(Dq + '/FREEZE.json', 'w'))
    chk('not retroactive: --replace refused on a run frozen before the rule',
        'without the environment rule' in (refused(lambda: cmd_slg(argparse.Namespace(dir=Dq, group=1, replace=True, quiet_timeout=1, quiet_s=0.0, cycles=4))) or ''))
    ab = res_q = json.load(open(Dq + '/RESULT.json'))['slg']['abba']
    chk(f'ABBA paired reported, 3 blocks per group (A {ab["A"]["geo"]:.4f})', ab['A']['blocks'] == 3 and ab['A']['low'] < ab['A']['geo'] < ab['A']['high'])
    Di, res = setup({})  # identical_binaries rule: SLG pair identical -> SLG n/a, verdict from the formal part alone
    fr = json.load(open(Di + '/FREEZE.json')); fr['slg']['anchor'] = fr['slg']['head']; json.dump(fr, open(Di + '/FREEZE.json', 'w'))
    cmd_analyze(argparse.Namespace(dir=Di, exclude=None)); res = json.load(open(Di + '/RESULT.json'))
    chk(f'identical SLG binaries -> SLG n/a, formal decides ({res["verdict"]})', res['slg']['verdict'] == 'n/a' and res['verdict'] == 'pass'
        and 'SLG 不适用' in open(Di + '/RESULT.md').read())
    chk('slg refused when the SLG binaries are identical',
        'not applicable' in (refused(lambda: cmd_slg(argparse.Namespace(dir=Di, group=2, replace=False, quiet_timeout=1, quiet_s=0.0, cycles=4))) or ''))
    cmd_trend(argparse.Namespace(dir=Di, trend=Di + '/T.md', link=''))
    chk('trend row for an n/a SLG part', '不适用' in open(Di + '/T.md').read())
    fr['binaries']['anchor'] = fr['binaries']['head']; json.dump(fr, open(Di + '/FREEZE.json', 'w'))
    cmd_analyze(argparse.Namespace(dir=Di, exclude=None)); res = json.load(open(Di + '/RESULT.json'))
    chk('both pairs identical -> n/a overall', res['verdict'].startswith('n/a') and res['formal_overall']['verdict'] == 'n/a')
    chk('formal refused when the formal binaries are identical',
        'not applicable' in (refused(lambda: cmd_formal(argparse.Namespace(dir=Di, batch=3, replaces=None, quiet_timeout=1, quiet_s=0.0, cycles=4))) or ''))
    fr['rules'] = {k: v for k, v in fr['rules'].items() if k != 'identical_binaries'}; json.dump(fr, open(Di + '/FREEZE.json', 'w'))
    cmd_analyze(argparse.Namespace(dir=Di, exclude=None)); res = json.load(open(Di + '/RESULT.json'))
    chk('identical-binary rule not retroactive', res['slg']['verdict'] != 'n/a' and res['formal_overall']['verdict'] != 'n/a')
    # checkpoint bisection (merge rules v2): 5 commits between anchor and head, commit c3 regresses mutate by 4%;
    # c1 and c2 share a binary (collapse), the SLG binaries never change (SLG not part of the bisection)
    def ckpt(culprit, eff, effect_of=None):
        T = tempfile.mkdtemp(prefix='mac_gate_selftest_')
        def put(path, data):
            os.makedirs(os.path.dirname(path), exist_ok=True); open(path, 'wb').write(data); return path
        r3 = {'binaries': {}, 'input_wires': {}}
        for k in ['xoffset_cand'] + RIVALS:
            r3['binaries'][k] = h(put(f'{T}/r3/bin/{k}', k.encode())); r3['input_wires'][k] = h(put(f'{T}/r3/wire/{k}.input.wire', b'w'))
        json.dump(r3, open(T + '/r3/FREEZE.json', 'w'))
        trace = put(T + '/trace.bin', b't'); wire = put(T + '/w.wire', b'w'); slg = put(T + '/slg', b'slg')
        names = ['c1', 'c2', 'c3', 'c4', 'c5']; binof = {'c1': 'B1', 'c2': 'B1', 'c3': 'B3', 'c4': 'B4', 'c5': 'B5'}
        bins = {n: put(f'{T}/sc-{n}', binof[n].encode()) for n in names}
        Dc = T + '/ckpt'
        cmd_freeze(argparse.Namespace(dir=Dc, head_bin=put(T + '/sc-head', b'HEAD'), head_wire=wire, head_commit='h' * 40, head_desc='',
                   head_src=None, anchor_bin=put(T + '/sc-anch', b'ANCH'), anchor_wire=wire, anchor_commit='a' * 40, anchor_desc='',
                   anchor_src=None, r3_dir=T + '/r3', trace=trace, slg_head=slg, slg_anchor=put(T + '/slg-a', b'slga'), slg_tree='t',
                   slg_tree_sha='0', expect_wire=None, lib_note='', prep=None,
                   bisect=[f'{n * 20}|#{80 + i} {n}|{bins[n]}|{wire}|{slg}' for i, n in enumerate(names)]))
        rng = random.Random(41)
        for b in (1, 2): synth_formal(f'{Dc}/formal-{b}', R, rng, eff)
        synth_slg(Dc + '/slg', rng, 1.0)
        cmd_analyze(argparse.Namespace(dir=Dc, exclude=None))
        order = names + ['head']
        for _ in range(6):
            cmd_bisect(argparse.Namespace(dir=Dc)); o = json.load(open(Dc + '/BISECT.json'))
            if o['status'] != 'waiting': return Dc, o
            st_ = o['steps'][-1]; S = f'{Dc}/{st_["dir"]}'
            pt = st_['label'].split()[-1]
            e = (effect_of or (lambda q: eff if order.index(q) >= order.index(culprit) else {}))(pt)
            synth_formal(S + '/formal-1', R, random.Random(50 + _), e)
            json.dump({'batch': 1, 'env': None, 'smoke': False, 'replaces': None}, open(S + '/formal-1/meta.json', 'w'))
        return Dc, o
    Dc, o = ckpt('c3', {'mutate': 1.04})
    chk(f'checkpoint bisection finds c3 in {sum(1 for s in o["steps"] if s["timed"])} timed steps ({o["status"]})',
        o['status'] == 'culprit' and o['culprit']['label'].endswith('c3') and sum(1 for s in o['steps'] if s['timed']) <= 3
        and list(o['failed_metrics']) == ['formal:mutate'])
    chk('commits with identical binaries collapse to the earliest', [x['label'] for x in o['points']] == ['anchor', '#80 c1', '#82 c3', '#83 c4', '#84 c5', 'head']
        and o['points'][1]['same'] == ['c2' * 20])
    chk('SLG part never timed in a step (identical SLG binaries)', all(not os.path.exists(f'{Dc}/{s["dir"]}/slg') for s in o['steps'] if s['dir']))
    chk('BISECT.md names the culprit', '回退来自' in open(Dc + '/BISECT.md').read())
    Dc, o = ckpt('head', {'mutate': 1.04})
    chk(f'regression in the head merge itself -> culprit head ({o.get("culprit", {}).get("label")})', o['status'] == 'culprit' and o['culprit']['label'] == 'head')
    Dc, o = ckpt('c3', {'mutate': 1.04}, effect_of=lambda q: {'mutate': 1.02} if q in ('c3', 'c4') else {'mutate': 1.04} if q in ('c5', 'head') else {})
    chk('a half-size step effect is reported as possibly spread', any(s['spread'] for s in o['steps']))
    T2 = tempfile.mkdtemp(prefix='mac_gate_selftest_'); rb = T2 + '/ref'; open(rb, 'wb').write(b'REF414'); rw = T2 + '/rw'; open(rw, 'wb').write(b'w')
    fr0 = json.load(open(Dc + '/FREEZE.json')); src0 = os.path.dirname(fr0['r3_dir'])
    cmd_freeze(argparse.Namespace(dir=T2 + '/int', head_bin=src0 + '/sc-head', head_wire=src0 + '/w.wire', head_commit='h' * 40, head_desc='',
               head_src=None, anchor_bin=src0 + '/sc-anch', anchor_wire=src0 + '/w.wire', anchor_commit='a' * 40, anchor_desc='', anchor_src=None,
               r3_dir=fr0['r3_dir'], trace=fr0['trace'], slg_head=src0 + '/slg', slg_anchor=src0 + '/slg-a', slg_tree='t', slg_tree_sha='0',
               expect_wire=None, lib_note='', prep=None, ref_bin=rb, ref_wire=rw, ref_commit='4' * 40, ref_desc='414dea4', bisect_max_steps=2,
               bisect=[f'{"c1" * 20}|+G9|{src0}/sc-c1|{src0}/w.wire|{src0}/slg']))
    fi = json.load(open(T2 + '/int/FREEZE.json'))
    chk('reference build in the r3 slot, max steps 2', fi['binaries']['r3'] == h(rb) and fi['reference']['commit'] == '4' * 40
        and fi['rules']['checkpoint']['max_steps'] == 2 and RULES['checkpoint']['max_steps'] == 3)
    chk('bisect refused on a checkpoint that passed', 'not fail' in (refused(lambda: cmd_bisect(argparse.Namespace(dir=ckpt('c3', {})[0]))) or ''))
    # unattended run: fake timing parts write synthetic data; the first formal batch is invalid by environment
    g = globals(); real = (g['cmd_formal'], g['cmd_slg']); calls = []
    def fake_formal(x):
        fr_ = json.load(open(x.dir + '/FREEZE.json')); pt = (fr_.get('bisect_step') or {}).get('point', 'head')
        eff = {'mutate': 1.04} if pt in ('head', 'c3' * 20, 'c4' * 20, 'c5' * 20) else {}
        B = f'{x.dir}/formal-{x.batch}'; synth_formal(B, R, random.Random(len(calls) + 70), eff); calls.append(('formal', x.dir, x.batch, x.replaces))
        bad = len(calls) == 1
        json.dump({'batch': x.batch, 'replaces': x.replaces, 'smoke': False, 'env': {**(inv if bad else ok_env)}}, open(B + '/meta.json', 'w'))
    def fake_slg(x):
        S = (x.dir + '/slg' if x.group == 1 else f'{x.dir}/slg-{x.group}') + ('-r' if x.replace else '')
        synth_slg(S, random.Random(len(calls) + 90), 1.0); calls.append(('slg', x.dir, x.group, x.replace))
    g['cmd_formal'], g['cmd_slg'] = fake_formal, fake_slg
    try:
        Dc, _ = ckpt('c3', {'mutate': 1.04})
        for x in os.listdir(Dc):
            if x.startswith(('formal-', 'slg', 'bisect-', 'RESULT', 'BISECT')): shutil.rmtree(f'{Dc}/{x}') if os.path.isdir(f'{Dc}/{x}') else os.remove(f'{Dc}/{x}')
        try: cmd_auto(argparse.Namespace(dir=Dc, quiet_timeout=1, quiet_s=180.0, cycles=None)); rc = None
        except SystemExit as e: rc = e.code
        o = json.load(open(Dc + '/BISECT.json')); res = json.load(open(Dc + '/RESULT.json'))
        chk(f'auto: env-invalid formal-1 replaced by formal-2, formal-3 pooled, fail -> bisected to c3 (rc {rc})',
            rc == 10 and calls[1][2:] == (2, 1) and res['formal_overall']['batches'] == 2 and o['status'] == 'culprit' and o['culprit']['label'].endswith('c3'))
        chk('auto: steps time formal only (SLG identical)', all(c[0] == 'formal' for c in calls if '/bisect-' in c[1]))
    finally:
        g['cmd_formal'], g['cmd_slg'] = real
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
    f.add_argument('--ref-bin', help='put this sc_xoffset in the r3 slot instead of the R3 one (report only)')
    f.add_argument('--ref-wire'); f.add_argument('--ref-commit'); f.add_argument('--ref-desc')
    f.add_argument('--bisect-max-steps', type=int, help='checkpoint: timed bisection steps allowed (default 3)')
    f.add_argument('--bisect', action='append', help='checkpoint: COMMIT|LABEL|FORMAL_BIN|WIRE|SLG_BIN of each first-parent '
                   'commit between anchor and head, oldest first (repeatable)')
    for name in ['formal', 'slg']:
        s = sp.add_parser(name); s.add_argument('dir')
        if name == 'formal':
            s.add_argument('--batch', type=int, required=True)
            s.add_argument('--replaces', type=int, help='N: the one replacement of formal-N, invalid by environment')
        if name == 'slg':
            s.add_argument('--group', type=int, default=1, help='2 = the extra group after a pending SLG verdict')
            s.add_argument('--replace', action='store_true', help='the one replacement of this group, invalid by environment')
        s.add_argument('--quiet-timeout', type=int, default=1800)
        s.add_argument('--quiet-s', type=float, default=180.0, help='no-compile window; only lowered for a smoke test')
        s.add_argument('--cycles', type=int, help='smoke test only; marks the result as not a gate result')
    s = sp.add_parser('amend-slg', help='write the SLG pending-band rule into a run frozen before it (before group 2)'); s.add_argument('dir')
    s.add_argument('--reason', required=True)
    s = sp.add_parser('analyze'); s.add_argument('dir')
    s.add_argument('--exclude', action='append', help='N:reason, leave formal-N out of the pooled gate (diagnosed invalid batch)')
    s = sp.add_parser('bisect', help='checkpoint that failed: freeze the next step or name the culprit'); s.add_argument('dir')
    s = sp.add_parser('auto', help='unattended: the whole gate with its pre-registered follow-ups (and the bisection)'); s.add_argument('dir')
    s.add_argument('--quiet-timeout', type=int, default=43200, help='per timed part; on timeout nothing is timed and auto stops')
    s.add_argument('--quiet-s', type=float, default=180.0, help='smoke test only'); s.add_argument('--cycles', type=int, help='smoke test only')
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
    {'freeze': cmd_freeze, 'formal': cmd_formal, 'slg': cmd_slg, 'analyze': cmd_analyze, 'trend': cmd_trend, 'bisect': cmd_bisect, 'auto': cmd_auto, 'selftest': cmd_selftest, 'amend-slg': cmd_amend_slg,
     'diag-freeze': cmd_diag_freeze, 'diag-formal': cmd_diag_formal, 'diag-analyze': cmd_diag_analyze}[a.cmd](a)


if __name__ == '__main__':
    main()
