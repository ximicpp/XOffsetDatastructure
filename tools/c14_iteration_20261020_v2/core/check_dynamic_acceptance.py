"""Offline count predicates for already audited, externally pinned diagnostics.

No compile, child execution, scheduling, source mutation or timing inference.
This does not replace the final execution/identity/event/truth/resource audit.
"""
import argparse
import hashlib
import json
from pathlib import Path

FIELDS = ('canonical', 'inactive', 'active', 'owns', 'vectors', 'capacity_hits',
          'nontrivial_misses', 'nonempty_misses', 'vector_data_reads')
STRUCTURE = ('owns', 'vectors', 'capacity_hits', 'nontrivial_misses', 'nonempty_misses')
PHASES = ('load', 'query_before', 'mutate', 'emit_after',
          'retire_modified_state', 'reload', 'query_after', 'cleanup')


def need(value, reason):
    if not value:
        raise ValueError(reason)


def count(v):
    need(set(v) == set(FIELDS), 'counter fields differ')
    need(all(type(v[k]) is int and 0 <= v[k] < 2**64 for k in FIELDS), 'counter is not uint64')
    need(v['canonical'] == v['inactive'] + v['active'], 'canonical partition differs')
    need(v['capacity_hits'] <= v['vectors'] and
         v['nontrivial_misses'] <= v['vectors'] - v['capacity_hits'] and
         v['nonempty_misses'] <= v['nontrivial_misses'], 'vector subsets differ')


def subtract(after, before):
    need(all(after[k] >= before[k] for k in FIELDS), 'cumulative counter reversed')
    v = {k: after[k] - before[k] for k in FIELDS}
    count(v)
    return v


def slices(audit):
    need(audit['status'] == 'passed' and audit['diagnostic_only'] is True,
         'role lacks successful final diagnostic audit')
    need(audit['replay']['status'] == 'passed' and
         audit['replay']['query_before'] is True and audit['replay']['query_after'] is True and
         len(audit['replay']['stages']) == 9 and audit['replay']['owning_emit_calls'] == 0,
         'complete nine-stage truth replay missing')
    v = audit['mechanism']
    need(v['kind'] == 'c14-common-counts-v1' and v['mutation_seen'] == 1022 and
         v['emit_seen'] == 511 and v['emit_invocations'] == 1, 'primary count mask differs')
    need(all(type(v[k]) is int and v[k] == 0 for k in
             ('event_errors', 'last_errno', 'channel_clock_status', 'mask_status', 'restore_status')),
         'producer/channel error')
    p, m = v['phases'], v['mutations']
    need(len(p) == 9 and len(m) == 10, 'snapshot inventory differs')
    for x in [v['counts'], *p, *m]:
        count(x)
    zero = {k: 0 for k in FIELDS}
    need(p[0] == m[0] == zero and p[8] == v['counts'], 'primary bracket differs')
    phase = {name: subtract(p[i+1], p[i]) for i, name in enumerate(PHASES)}
    # mutations[1] already includes load and query_before. Never subtract m[0].
    stages = [subtract(m[i], p[2] if i == 1 else m[i-1]) for i in range(1, 10)]
    need(m[9] == p[3], 'last mutation scope does not end at primary mutate boundary')
    need(all(sum(s[k] for s in stages) == phase['mutate'][k] for k in FIELDS),
         'nine stages do not conserve primary mutate counts')
    read_sum = {k: phase['query_before'][k] + phase['query_after'][k] for k in FIELDS}
    return {'primary': v['counts'], 'phases': phase, 'mutations': stages, 'read_sum': read_sum}


def compare(kind, control, candidate):
    a, b = slices(control), slices(candidate)
    scopes = [('primary', a['primary'], b['primary']), ('read_sum', a['read_sum'], b['read_sum'])]
    scopes += [(name, a['phases'][name], b['phases'][name]) for name in PHASES]
    scopes += [(f'mutation_{i+1}', x, y) for i, (x, y) in enumerate(zip(a['mutations'], b['mutations']))]
    rows = []
    for name, x, y in scopes:
        if kind == 'r1':
            need(x == y, 'R1 logical counts changed in ' + name)
            rows.append({'scope': name, 'canonical': y['canonical'], 'inactive': y['inactive'],
                         'active': y['active'], 'logical_counts_equal': True})
        else:
            need(all(x[k] == y[k] for k in STRUCTURE), 'M2 ownership traversal changed in ' + name)
            saved = x['vector_data_reads'] - y['vector_data_reads']
            calls = x['canonical'] - y['canonical']
            n, nonempty = x['nontrivial_misses'], x['nonempty_misses']
            # C14 miss adds begin(data) plus end(data?data+size:null).
            # M2 keeps the original containment data only. k counts nonnull misses.
            k = saved - 2*n
            need(nonempty <= k <= n and calls == 3*k,
                 'M2 exact closed-driver data/canonical conservation differs in ' + name)
            need(x['inactive'] >= y['inactive'] and x['active'] >= y['active'],
                 'M2 classification increased in ' + name)
            rows.append({'scope': name, 'vectors': x['vectors'], 'nontrivial_misses': n,
                         'nonempty_misses': nonempty, 'nonnull_misses_derived': k,
                         'control_data_reads': x['vector_data_reads'], 'candidate_data_reads': y['vector_data_reads'],
                         'data_reads_removed': saved, 'canonical_calls_removed': calls})
    if kind == 'r1':
        hit = b['read_sum']['inactive'] > 0
        reason = 'positive read_sum inactive return count; identical logical work; bare codegen obligation separate'
    else:
        x, y = a['phases']['mutate'], b['phases']['mutate']
        hit = x['nonempty_misses'] > 0 and x['vector_data_reads'] > y['vector_data_reads'] and x['canonical'] > y['canonical']
        reason = 'positive primary-mutate nonempty eligible nodes with conserved traversal and fewer actual observed data/canonical calls'
    return {'status': 'mechanism_hit' if hit else 'closed_no_dynamic_hit', 'candidate': kind.upper(),
            'predicate': reason, 'scopes': rows, 'diagnostic_only': True,
            'execution_admission_checked': False, 'bare_codegen_checked': False,
            'speed_inferred': False, 'screen_admitted': False, 'QF_admitted': False}


def read_pinned(path, sha):
    raw = path.read_bytes()
    need(hashlib.sha256(raw).hexdigest() == sha, 'external audit SHA differs: ' + str(path))
    def pairs(items):
        value = {}
        for k, v in items:
            need(k not in value, 'duplicate JSON key')
            value[k] = v
        return value
    def bad(v):
        raise ValueError('nonfinite JSON ' + v)
    return json.loads(raw, object_pairs_hook=pairs, parse_constant=bad)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate', choices=['r1', 'm2'], required=True)
    for role in ('control', 'candidate'):
        p.add_argument('--' + role + '-audit', type=Path, required=True)
        p.add_argument('--' + role + '-sha256', required=True)
    args = p.parse_args()
    try:
        a = read_pinned(args.control_audit, args.control_sha256)
        b = read_pinned(args.candidate_audit, args.candidate_sha256)
        result = compare(args.candidate, a, b)
        result['external_audit_sha256'] = {'control': args.control_sha256, 'candidate': args.candidate_sha256}
        print(json.dumps(result, sort_keys=True))
        return 0 if result['status'] == 'mechanism_hit' else 2
    except (ValueError, KeyError, TypeError, OSError) as e:
        print(json.dumps({'status': 'invalid_evidence_close', 'reason': str(e), 'speed_inferred': False,
                          'screen_admitted': False, 'QF_admitted': False}))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
