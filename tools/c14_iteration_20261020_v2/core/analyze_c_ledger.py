"""Read fixed C receipts/raw records, write finite offline work accounting only."""
import copy
import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
DOC = ROOT / 'docs/coordination/c14-iteration-20261020-v2'
UNC = Path('//wsl.localhost/Ubuntu-22.04')
STAGES = ['load', 'query_before', 'mutate', 'emit_after', 'retire_modified_state', 'reload', 'query_after', 'cleanup']
EMIT = ['normalization_root_allocator', 'header_preparation', 'string_and_checks',
        'resize_to_callback_string_preparation', 'header_copy', 'payload_copy',
        'callback_return_and_resize_return', 'save_return_tail']


def local(path):
    if path.startswith('/root/'):
        return Path(path) if os.name != 'nt' else UNC / path.lstrip('/')
    if path.startswith('/mnt/c/'):
        if os.name != 'nt':
            return Path(path)
        return Path('C:/' + path[len('/mnt/c/'):])
    return Path(path)


def fid(path):
    b = path.read_bytes()
    return {'bytes': len(b), 'sha256': hashlib.sha256(b).hexdigest()}


def ref(path):
    return {'path': str(path), 'identity': fid(path)}


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def pinned(reference):
    p = local(reference['path'])
    assert fid(p) == reference['identity'], str(p)
    return read(p)


def write_new(path, value):
    with path.open('x', encoding='utf-8') as f:
        json.dump(value, f, indent=2, ensure_ascii=False)
        f.write('\n')


def main():
    outputs = [DOC/'core/C_WORK_LEDGER.json', DOC/'core/M2_MECHANISM_EXACT_BRIDGE.json']
    assert not any(p.exists() for p in outputs), 'Do not overwrite prepared accounting'
    cpath = DOC/'validation/diag/C_RESULT.json'
    c = read(cpath)
    assert c['status'] == 'passed' and c['actual_compiles'] == c['actual_diagnostic_native'] == 2
    freeze = pinned(c['freeze'])
    ledger = {'schema': 'core-C-diagnostic-work-ledger-v1', 'status': 'offline_accounting_of_passed_registered_diagnostics',
              'tool': ref(Path(__file__).resolve()), 'C_result': ref(cpath),
              'R1_dynamic_result': ref(DOC/'validation/diag/R1_DYNAMIC_RESULT.json'),
              'roles': {}, 'interpretation': 'Two single diagnostic observations, no candidate speed ratio, CPU-share estimate or performance inference.'}
    for role, references in c['roles'].items():
        a, o, comp = pinned(references['audit']), pinned(references['observer']), pinned(references['compile'])
        directory = local(references['observer']['path']).parent
        assert a['status'] == 'passed' and o['status'] == 'complete' and o['cleanup_remaining'] == []
        for name, identity in o['files'].items():
            assert fid(directory/name) == identity, name
        stdout = [json.loads(line) for line in (directory/'stdout.raw').read_bytes().splitlines()]
        assert stdout == [a['emission'], a['mechanism'], a['replay']]
        raw = (directory/'events.raw').read_text(encoding='ascii').splitlines()
        for i, (line, event) in enumerate(zip(raw, o['events'])):
            z = line.split()
            assert z[0] == 'XD1' and int(z[1]) == event['sequence'] == i
            assert z[2] == event['child_monotonic_ns'] and z[3] == event['label']
            assert list(map(int, z[4:])) == event['values']
        assert len(raw) == len(o['events']) == 36
        events = {v['label']: int(v['child_monotonic_ns']) for v in o['events']}
        def wall(first, last):
            return events[last] - events[first]
        start, end = int(o['started_monotonic_ns']), int(o['returned_monotonic_ns'])
        boundaries = [('launch_before_fork_exec', start)] + [(v['label'], int(v['child_monotonic_ns'])) for v in o['events']] + [('wait4_return', end)]
        partition = [{'from': x[0], 'to': y[0], 'wall_ns': y[1]-x[1]} for x, y in zip(boundaries, boundaries[1:])]
        assert all(v['wall_ns'] >= 0 for v in partition) and sum(v['wall_ns'] for v in partition) == end-start
        views = {name: wall(name+'.before', name+'.after') for name in
                 ['input.read', 'input.decode', 'input.hash', 'postaudit.load', 'postaudit.export', 'postaudit.hash', 'truth.replay', 'write.wire', 'write.canonical', 'write.queries', 'write.result']}
        views['primary_t0_t8_event_boundaries'] = wall('t0', 't8')
        views['after_replay_to_first_write_unsegmented'] = wall('truth.replay.after', 'write.wire.before')
        # These selected views are disjoint; uncovered gaps remain explicitly residual.
        views['other_launch_exit_flush_and_inter_boundary_gaps'] = end-start-sum(views.values())
        assert sum(views.values()) == end-start and min(views.values()) >= 0
        n = a['native']; phases = {k: int(n['phase_ns'][k]) for k in STAGES}
        assert sum(phases.values()) == int(n['total_ns'])
        rr = a['replay']['replay_resources_before_final_JSON_flush']
        whole_cpu_us = round(o['cpu_seconds']*1e6)
        primary_cpu_us = int(n['cpu_ns'])//1000
        replay_cpu_us = rr['user_us']+rr['system_us']
        samples = a['emission']['samples']; intervals = []
        for i, name in enumerate(EMIT):
            x, y = samples[i:i+2]
            intervals.append({'name': name, 'from_sample': i, 'to_sample': i+1,
                              'wall_ns': y['ns']-x['ns'], 'user_us': y['utime_us']-x['utime_us'],
                              'system_us': y['stime_us']-x['stime_us'], 'minor_faults': y['minor']-x['minor'],
                              'major_faults': y['major']-x['major']})
        emit_total = {k: sum(v[k] for v in intervals) for k in ('wall_ns', 'user_us', 'system_us', 'minor_faults', 'major_faults')}
        assert emit_total['wall_ns'] == samples[8]['ns']-samples[0]['ns']
        ledger['roles'][role] = {
            'inputs': {k: references[k] for k in ('audit', 'observer', 'compile')},
            'raw_files_verified': o['files'],
            'whole_child': {'wall_ns': end-start, 'user_us': round(o['user_cpu_seconds']*1e6),
                            'system_us': round(o['system_cpu_seconds']*1e6), 'cpu_us': whole_cpu_us,
                            'minor_faults': o['minor_faults'], 'major_faults': o['major_faults'], 'peak_rss_kib': o['max_rss_kib']},
            'nonoverlapping_wall_summary_ns': views, 'complete_boundary_wall_partition_ns': partition,
            'primary': {'phase_ns': phases, 'wall_ns': int(n['total_ns']), 'cpu_us_aggregate_only': primary_cpu_us,
                        'minor_faults_aggregate_only': int(n['minor_faults']), 'major_faults_aggregate_only': int(n['major_faults']),
                        'retire_includes_post_t4_emit_observer_flush': True, 'per_phase_CPU_fault_allocation_available': False},
            'replay': {'wall_ns_before_final_JSON_flush': rr['end_ns']-rr['start_ns'], 'cpu_us': replay_cpu_us,
                       'user_us': rr['user_us'], 'system_us': rr['system_us'], 'minor_faults': rr['minor_faults'],
                       'major_faults': rr['major_faults'], 'work': a['replay']['work']},
            'residual_resource_accounting': {'cpu_us_whole_minus_primary_minus_replay': whole_cpu_us-primary_cpu_us-replay_cpu_us,
                                            'minor_faults_whole_minus_primary_minus_replay': o['minor_faults']-int(n['minor_faults'])-rr['minor_faults'],
                                            'major_faults_whole_minus_primary_minus_replay': o['major_faults']-int(n['major_faults'])-rr['major_faults'],
                                            'scope': 'Unsplit input/startup, original post-audit, hashes/query serialization, output writes, observation/finalization; do not allocate by wall fractions.'},
            'first_owning_emit': {'invocations': a['emission']['invocations'], 'output_bytes': samples[3]['bytes'],
                                  'header_bytes': samples[2]['bytes'], 'payload_bytes': samples[1]['bytes'],
                                  'adjacent_sample_intervals': intervals, 'sample0_to8': emit_total,
                                  'native_emit_minus_sample0_to8_wall_ns': phases['emit_after']-emit_total['wall_ns'],
                                  'containment': 'All sample intervals lie within emit_after and primary; never add totals, phases, copy work or faults twice.'},
            'mechanism': a['count_conservation'],
            'compile_wall_ns': int(comp['returned_monotonic_ns'])-int(comp['started_monotonic_ns']),
            'wrapper_post_wait4_drain_and_seal_wall_ns': int(o['finished_monotonic_ns'])-end}
    ledger['outer_job'] = {'seconds': c['actual_outer_seconds'], 'native_wall_ns_sum': sum(v['whole_child']['wall_ns'] for v in ledger['roles'].values()),
                           'compile_wall_ns_sum': sum(v['compile_wall_ns'] for v in ledger['roles'].values()),
                           'scope': 'Outer includes both compile/native roles, identity/resource/dependency checks, audits and sealing. It is not a native benchmark.'}
    control = freeze['roles']['c14']; material = read(local(freeze['materialization'])) if isinstance(freeze['materialization'], str) else None
    if material is None:
        material = read(local('/root/xoffset-c14-iteration/20261020/v2/validation/common-diag-final-v1/MATERIALIZATION.json'))
    record = material['roles']['m2']; newout = '/root/xoffset-c14-iteration/20261020/v2/validation/m2-common-mechanism-v1/m2'
    oldout = str(Path(control['binary']).parent).replace('\\', '/')
    def retarget(v):
        if isinstance(v, str): return v.replace(control['source'], record['source']).replace(oldout, newout)
        if isinstance(v, dict): return {k: retarget(x) for k, x in v.items()}
        if isinstance(v, list): return [retarget(x) for x in v]
        return v
    candidate = retarget(copy.deepcopy(control)); candidate['source'] = record['source']
    candidate['source_154'] = record['source_154']; candidate['production_14'] = record['production_14']
    assert candidate['native_argv'][1] == 'run' and '-O3' in candidate['compile_argv'] and '-UNDEBUG' not in candidate['compile_argv']
    assert control['source'] not in json.dumps(candidate) and oldout not in json.dumps(candidate)
    cp = pinned(c['roles']['c14']['compile']); binary = local(control['binary']); assert fid(binary) == cp['binary']
    bridge = {'schema': 'core-M2-exact-common-control-request-bridge-v1', 'status': 'prepared_for_validation_freeze_not_execution_admission',
              'original_conditional_request': ref(DOC/'core/REQUESTS_M2_MECHANISM_DRAFT.json'), 'C_result': ref(cpath), 'actual_C_freeze': c['freeze'],
              'M2_control_predeclaration': freeze['M2_control_predeclaration'],
              'existing_control': {'role': 'c14', 'audit': c['roles']['c14']['audit'], 'observer': c['roles']['c14']['observer'],
                                   'compile': c['roles']['c14']['compile'], 'binary': {'path': control['binary'], 'identity': cp['binary']}},
              'actual_materialization_identity': freeze['materialization_identity'],
              'actual_M2_final_identity': {'path': record['source']+'/FINAL_SOURCE_IDENTITY.json', 'identity': fid(local(record['source']+'/FINAL_SOURCE_IDENTITY.json'))},
              'candidate_role_exact_proposal': candidate, 'only_argv_changes': 'c14 final-source prefix -> existing m2 final-source prefix; c14 role output/TMPDIR/dependency/body prefixes -> new M2 prefix; flags, wire/trace and all public calls unchanged',
              'budgets': {'candidate_compiles': 1, 'compile_seconds': 120, 'candidate_native': 1, 'native_seconds': 20, 'outer_seconds': 600, 'role_audit_seconds': 60, 'control_compiles': 0, 'control_native': 0, 'retry': 0, 'warmup': 0},
              'retained_closure': {'fixed_files_reference': c['freeze'], 'same_final_driver_adapter_hook_channel_wrapper_audit': True,
                                   'original_global_supervisor_and_lease': True, 'outer_launcher': {'path': candidate['compile_argv'][0], 'identity': freeze['fixed_files'][candidate['compile_argv'][0]]}},
              'validation_must_close': ['Actual fresh-prefix check, current resources/backing/host/original lease and independent finite dispatch/admission',
                                       'Verify all actual source/tool/runtime/dependency identities inherited from C freeze, plus own candidate compiler dependencies/binary/receipt before and after',
                                       'Use exact same observer and semantic audit, event/order/status/truth/body limits and descendant cleanup; stop/seal on first failure',
                                       'Pin actual successful control and candidate AUDIT identities and run the predeclared offline M2 count predicate, never run another control'],
              'candidate_mechanism_observed': False, 'screen_admitted': False, 'QF_admitted': False, 'core_new_compiles': 0, 'core_new_native': 0}
    write_new(outputs[0], ledger); write_new(outputs[1], bridge)
    print(json.dumps({p.name: fid(p) for p in outputs}, ensure_ascii=False))


if __name__ == '__main__':
    main()
