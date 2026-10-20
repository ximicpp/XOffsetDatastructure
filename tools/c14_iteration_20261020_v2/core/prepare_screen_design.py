"""Write source-bound, deterministic schedule designs; never execute children."""
import hashlib
import json
from pathlib import Path
import platform

ROOT = Path(__file__).resolve().parents[3]
DOC = ROOT / 'docs/coordination/c14-iteration-20261020-v2/core'
WSL = '/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发'
STAGES = ['load', 'query_before', 'mutate', 'emit_after',
          'retire_modified_state', 'reload', 'query_after', 'cleanup']
METRICS = STAGES + ['read_sum', 'total']
LAUNCHER = '/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/bin/clang++-libcxx'
LAUNCHER_ID = {'bytes': 61, 'sha256': 'c8ea06a7c58557e3b89d09873dc88457d7b0c5d9af3f08cf0449093f2fbd1b87'}


def fid(path):
    b = path.read_bytes()
    return {'bytes': len(b), 'sha256': hashlib.sha256(b).hexdigest()}


def ref(path):
    return {'path': WSL + '/' + path.relative_to(ROOT).as_posix(), 'identity': fid(path)}


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False).encode('utf-8')


def balanced_orders(seed):
    orders = ['AB'] * 36 + ['BA'] * 36
    counter = 0
    # Unbiased Fisher-Yates with a fully specified SHA256 counter stream.
    for i in range(71, 0, -1):
        modulus = i + 1
        limit = (2**256 // modulus) * modulus
        while True:
            raw = seed.encode('utf-8') + b'\0' + counter.to_bytes(8, 'big')
            counter += 1
            x = int.from_bytes(hashlib.sha256(raw).digest(), 'big')
            if x < limit:
                break
        j = x % modulus
        orders[i], orders[j] = orders[j], orders[i]
    return orders, counter


def make_design(label, primary, relative_identity, material):
    identity_path = ROOT / relative_identity
    identity = json.loads(identity_path.read_text(encoding='utf-8'))
    source = identity_path.parent / 'source'
    baseline = material['non_external_files']
    candidate = {n: {'bytes': v['bytes'], 'sha256': v['candidate_sha256']}
                 for n, v in identity['members'].items()}
    assert len(baseline) == len(candidate) == 154
    for name, expected in baseline.items():
        assert fid(ROOT / name) == expected, ('baseline changed', name)
    for name, expected in candidate.items():
        assert fid(source / name) == expected, ('candidate changed', name)
    seed = f'xoffset/c14-iteration-20261020-v2/screen/{label}/{primary}/72pairs/v1'
    orders, counter = balanced_orders(seed)
    pairs = []
    children = []
    for i, order in enumerate(orders):
        pairs.append({'pair_index': i, 'pair_id': f'p{i:03}', 'order': order,
                      'child_ordinals': [2*i, 2*i+1]})
        for within, symbol in enumerate(order):
            children.append({'ordinal': 2*i + within, 'pair_index': i,
                             'pair_id': f'p{i:03}', 'within_pair': within,
                             'symbol': symbol,
                             'role': 'control' if symbol == 'A' else 'candidate'})
    assert orders.count('AB') == orders.count('BA') == 36
    assert len(children) == 144 and len(pairs) == 72
    prod_names = material['production_files']
    return {
        'schema_version': 1,
        'status': 'design_only_not_execution_admission',
        'candidate_label': label, 'candidate': identity['candidate'],
        'generator': ref(Path(__file__).resolve()),
        'generator_runtime_only': {'implementation': platform.python_implementation(),
                                   'version': platform.python_version(),
                                   'scope': 'metadata generation, not final worker/runtime binding'},
        'candidate_identity': ref(identity_path),
        'baseline_materialization': ref(ROOT / 'handoff/github-20261019/C14_MATERIALIZATION.json'),
        'statistical_precheck': ref(ROOT / 'docs/coordination/c14-iteration-20261020-v2/reviews/STATISTICAL_PRECHECK.md'),
        'source': {'control': WSL, 'candidate': WSL + '/' + (source.relative_to(ROOT)).as_posix(),
                   'control_members154': baseline, 'candidate_members154': candidate,
                   'control_production14': {n: baseline[n] for n in prod_names},
                   'candidate_production14': {n: candidate[n] for n in prod_names},
                   'candidate_patch_sha256': identity['patch_sha256']},
        'compiler_launcher': {'path': LAUNCHER, 'identity': LAUNCHER_ID},
        'randomization': {
            'algorithm': 'SHA256-counter-unbiased-Fisher-Yates-v1', 'seed_utf8': seed,
            'initial_list': '36 AB followed by 36 BA',
            'steps': 'i=71 down to1; m=i+1; hash UTF8(seed)||00||uint64be(counter), counter starts0 and increments after each digest; x=uint256be(digest); reject x>=floor(2^256/m)*m; j=x%m; swap positions i,j',
            'hash_draws': counter,
            'hash_rejection_scope': 'Only deterministic integer range reduction; never native/sample retry.',
            'roles': {'A': 'independent C14 control', 'B': identity['candidate']},
            'counts': {'pairs': 72, 'children': 144, 'AB': 36, 'BA': 36},
            'schedule_digest_encoding': 'UTF8 JSON, sorted keys, compact separators, ensure_ascii=false; no trailing newline',
            'schedule_sha256': hashlib.sha256(canonical({'pairs': pairs, 'children': children})).hexdigest(),
            'pairs': pairs, 'children': children,
            'interpretation': 'Balanced randomized order does not establish independence of pair log contrasts.'},
        'statistics': {
            'stages': STAGES, 'metrics': METRICS, 'primary': primary,
            'auxiliary': [m for m in METRICS if m != primary],
            'derived_before_ratio': {'read_sum': 'query_before+query_after', 'total': 'sum(eight stages)'},
            'native_total_ns': 'retain separately, never substitute for derived total',
            'family': {'original_primary': 4, 'original_auxiliary': 36,
                       'closed_M1_and_rejected_L1_slots': 'retained; alpha never reclaimed/reset'},
            'unit': '72 role-corrected control/candidate pairs, never144 independent children or phases',
            'n': 72, 'df': 71, 'contrast': 'log(candidate_metric/control_metric)',
            'standard_error': 'Bessel sample_stdev(log contrasts)/sqrt(72)',
            'point': 'exp(mean(log contrasts))',
            'primary_upper': 'exp(mean+t(1-.05/4,71)*se)',
            'auxiliary_interval': 'exp(mean +/- t(1-.05/(2*36),71)*se)',
            'critical_values': 'retain independently audited original numeric evidence; final worker must bind it',
            'zero_policy': 'Preserve canonical nonnegative integer zero; any zero in either role makes that metric undefined_log_contrast_zero_preserved; no epsilon, deletion, replacement or resolution change.',
            'promotion': 'All72 pairs complete AND every metric defined (including all auxiliary) AND primary_upper<1 AND no auxiliary_lower>1 AND every predeclared engineering/correctness/resource/recovery/independent-audit gate passed.',
            'auxiliary_crossing_one': 'not confirmed regression; never equivalence or proof of no regression',
            'invalid_or_failed_window': 'Missing, negative, noncanonical, nonfinite, schema/role/identity/truth/native/deadline failure closes unique attempt; summary=null; no prefix benefit.',
            'warmup': 0, 'retry': 0, 'replacement': 0, 'outlier_removal': 0,
            'forbidden': ['posthoc detrending', 'changing unit or test', 'supplementing prefix',
                          'combining R1/M2 samples or adding their percentage savings']},
        'execution_constraints_for_later_freeze': {
            'child_timeout_s': 20, 'command_timeout_s': 600, 'audit_timeout_s': 3600,
            'compiler_profile': 'bare -O3 -DNDEBUG; no -UNDEBUG, diagnostic hook or capture',
            'supervisor': 'original shared run_job.py and Documents/.../build/vm-coordination/queue.lock; no nested lease or new lock',
            'quiet': 'Other agents stop scanning/compiling/archiving during timing.',
            'failure': 'Stop, clean descendants, restore user processes, seal prefix; no extra attempt.',
            'recovery_order': 'Restore user processes before offline audit.'},
        'not_yet_bound_for_execution': [
            'final validation-owned worker/known-answer audit/runtime identities',
            'actual bare control/candidate binaries, compile profile, compiler/libc++/runtime/driver/adapter/input/wire/trace closure',
            'same-candidate full normal/SAN/SML/V1/install/consumer/OOM/lifecycle results',
            'predeclared common C14 control plus candidate dynamic mechanism hit with same final diagnostic closure',
            'actual complete process/wall/output projection, RSS/allocated peak, both volumes/backing, child and seal margin',
            'explicit acceptable build/maintenance/free/retained/active-memory cost gates before seeing results',
            'actual unique attempt id, argv/output paths, host quiet, original lease and stop/recovery checks',
            'independent root freeze with external pin covering this entire schedule and all preceding bindings'],
        'admission': {'native_executed': 0, 'compiles_executed': 0, 'screen_admitted': False,
                      'QF_admitted': False, 'performance_goal_complete': False},
        'immutability': 'This is a prepared design, not a worker request. Preserve this version; any prelaunch revision needs a new version and independent review. After freeze no schedule/seed or sample substitutions.'}


def main():
    material = json.loads((ROOT / 'handoff/github-20261019/C14_MATERIALIZATION.json').read_text(encoding='utf-8'))
    designs = [make_design('R1', 'read_sum', 'build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1/IDENTITY.json', material),
               make_design('M2', 'mutate', 'build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1/IDENTITY.json', material)]
    paths = [DOC / f"SCREEN_DESIGN_{d['candidate_label']}.json" for d in designs]
    assert not any(p.exists() for p in paths), 'Never overwrite a prepared design'
    for path, design in zip(paths, designs):
        with path.open('x', encoding='utf-8') as f:
            json.dump(design, f, indent=2, ensure_ascii=False)
            f.write('\n')
    print(json.dumps({'designs': {p.name: fid(p) for p in paths}, 'native': 0, 'compiles': 0}, ensure_ascii=False))


if __name__ == '__main__':
    main()
