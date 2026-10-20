"""Small Q32 actual result index; reuse completed original full body audit."""
from pathlib import Path
import datetime, hashlib, json, subprocess

R = Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
D = R / 'docs/coordination/c14-iteration-20261020-v2'
V = D / 'validation'
P = V / 'q32_freeze_preparation'
C = V / 'final_formal_v1'
B = Path('/root/xoffset-c14-iteration/20261020/v2/validation/final-formal-v1')
OUT = B / 'qualification-v1'
PRE = B / 'leases/qualification-host-prelude-v1'
AUD = B / 'leases/qualification-offline-audit-v1'
RUNNER = Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py')

def metadata(path):
    path = Path(path)
    assert path.is_file() and not path.is_symlink() and path.stat().st_size <= 16 * 1024**2
    return path.read_bytes()

def read(path):
    return json.loads(metadata(path))

def fid(path):
    value = metadata(path)
    return {'bytes': len(value), 'sha256': hashlib.sha256(value).hexdigest()}

def norm(value):
    return {'bytes': int(value['bytes']), 'sha256': value['sha256']}

def ref(path):
    return {'path': str(path), 'identity': fid(path)}

def write(path, value):
    with path.open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(value, stream, indent=2, sort_keys=True); stream.write('\n')

def main():
    report_path = P / 'Q32_OFFLINE_AUDIT_RESULT_v1.json'
    report = read(report_path); analysis = report['analysis']; status = read(OUT / 'status.json')
    assert report['status'] == 'passed' and report['complete'] is True and report['measurement_qualified'] is True
    assert analysis['qualification_passed'] is True and analysis['measurement_qualified'] is True
    assert report['budget_violations'] == [] and analysis['failure_reasons'] == []
    assert report['successful_actual_children'] == report['attempted_actual_children'] == 4656
    assert report['successful_primary_children'] == report['successful_conditioning_children'] == 2328
    assert report['unexecuted_actual_children'] == 0 and status['error'] is None
    assert analysis['measured_children'] == 2304 and analysis['excluded_warmup_children'] == 24
    assert analysis['completed_blocks'] == analysis['required_blocks'] == 192
    assert analysis['competitor_contrasts_computed'] is False
    assert analysis['all_measured_rows_retained'] is True and analysis['no_outlier_filter'] is True
    rows = []
    for role, group in analysis['AA'].items():
        for metric, item in group.items():
            assert item['available'] is True and item['equivalence_established'] is True
            assert item['df'] == 31 and item['inference_units'] == 32 and item['family_contrasts'] == 30
            assert item['ratio_lower'] >= 1 / 1.05 and item['ratio_upper'] <= 1.05
            rows.append({'role': role, 'metric': metric, **{k: item[k] for k in (
                'ratio_point_estimate', 'ratio_lower', 'ratio_upper', 'equivalence_established', 'df',
                'inference_units', 'critical_t', 'simultaneous_family_confidence', 'per_contrast_confidence')}})
    assert len(rows) == 30
    aq, tq = read(AUD / 'result.json'), read(OUT / 'queue/result.json')
    for queue in (PRE, OUT / 'queue', AUD):
        request, result = read(queue / 'request.json'), read(queue / 'result.json')
        assert result['status'] == 'complete' and result['command_returncode'] == 0
        assert result['cleanup'] == {'live': [], 'process_groups': [], 'zombies': []}
        assert request['run_id'] == result['run_id'] and request['wait_seconds'] == 0
    assert aq['owner'] == tq['owner'] == 'W3' and aq['kind'] == 'functional' and tq['kind'] == 'timing'
    assert read(AUD / 'request.json')['timeout_seconds'] == 3600
    assert read(OUT / 'queue/request.json')['timeout_seconds'] == 2700
    dispatch = read(C / 'qualification-controller-v1/TIMING_DISPATCH_RESULT.json')
    restored_path = C / 'qualification-controller-v1/RESTORED.json'; restored = read(restored_path)
    coverage_path = C / 'host-qualification-v1/TIMING_PAUSE_COVERAGE.json'; coverage = read(coverage_path)
    power_path = C / 'power-qualification-v1/completed.json'; power = read(power_path)
    pause_path = C / 'host-qualification-v1/process-state-v1/completed.json'; pause = read(pause_path)
    assert dispatch['qualification_attempts'] == 1 and dispatch['retries'] == 0 and dispatch['returncode'] == 0
    assert restored['status'] == 'passed' and restored['user_processes_restored'] is True
    assert restored['error'] is None and restored['power_error'] is None
    assert coverage['status'] == 'passed' and coverage['actual_suspended'] == 2
    assert power['status'] == pause['status'] == 'passed' and power['restored'] is pause['restored'] is True
    assert power['original'] == restored['power_before_evidence']['guid'] == restored['power_after_evidence']['guid']
    assert report['power_window']['valid_complete_power_window'] is True and report['power_window']['restored'] is True
    parse = lambda value: datetime.datetime.fromisoformat(value.replace('Z', '+00:00'))
    assert parse(coverage['timing_finished_at']) <= parse(coverage['stop_requested_utc'])
    assert parse(power['finished_utc']) < parse(read(AUD / 'request.json')['created_at'])
    assert parse(pause['resume_started_utc']) < parse(read(AUD / 'request.json')['created_at'])
    assert read(OUT / 'runtime-before.json') == read(OUT / 'runtime-after.json') == read(OUT / 'frozen/runtime.json')
    for entry in read(V / 'final_formal_v1_preparation/FINAL_FORMAL_BOUND_SOURCE_READY.json')['inventory'].values():
        assert fid(entry['bound']['path']) == norm(entry['bound']['identity'])
    for name, path in (('frozen_manifest', OUT / 'frozen/manifest.json'), ('run_manifest', OUT / 'manifest.json'),
                       ('queue_manifest', OUT / 'queue/manifest.json'), ('queue_result', OUT / 'queue/result.json')):
        assert norm(report[name]) == fid(path)
    ledger_path = OUT / 'resource-ledger.json'; ledger = read(ledger_path)
    assert norm(report['resource_ledger']) == fid(ledger_path) and ledger['status'] == 'passed'
    assert ledger['recorded_attempts'] == 4656 and ledger['deduplicated_body_slots'] == len(ledger['body_proofs']) == 13968
    actual_request = read(P / 'Q32_OFFLINE_AUDIT_EXECUTION_REQUEST_v1.json')
    postpin = hashlib.sha256(b'SC-POST-RUN-PIN-v1\0' + bytes.fromhex(fid(OUT / 'manifest.json')['sha256'])
                            + bytes.fromhex(fid(OUT / 'queue/manifest.json')['sha256'])).hexdigest()
    assert postpin == actual_request['postrun_pin']
    argv = ['/usr/bin/python3', '-B', str(RUNNER), 'audit', '--out', str(AUD)]
    audit = subprocess.run(argv, capture_output=True, text=True, timeout=30)
    check_path = P / 'Q32_OFFLINE_AUDIT_ORIGINAL_QUEUE_AUDIT_v1.json'
    write(check_path, {'argv': argv, 'returncode': audit.returncode, 'stdout': audit.stdout, 'stderr': audit.stderr})
    assert audit.returncode == 0
    index = {'status': 'Q32_complete_original_full_audit_passed_all30AA_qualified_pending_root_acceptance',
        'captured_at': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'audit_report': ref(report_path),
        'run_directory': str(OUT), 'actual_run_manifest': ref(OUT / 'manifest.json'),
        'actual_queue_manifest': ref(OUT / 'queue/manifest.json'), 'postrun_pin': postpin,
        'frozen_manifest': ref(OUT / 'frozen/manifest.json'), 'run_status': ref(OUT / 'status.json'),
        'analysis_record': ref(OUT / 'analysis.json'), 'resource_ledger': ref(ledger_path),
        'runtime_before': ref(OUT / 'runtime-before.json'), 'runtime_after': ref(OUT / 'runtime-after.json'),
        'actual_source_runtime_closure_equal': True, 'accepted_live_bound_source11_unchanged': True,
        'actual_native_children': 4656, 'successful_conditioning_children': 2328, 'successful_primary_children': 2328,
        'unexecuted_actual_children': 0, 'primary_warmup_excluded': 24, 'measured_primary_children': 2304,
        'macrocycles': 32, 'completed_blocks': 192, 'AA_count': 30, 'all_AA_equivalent': True,
        'measurement_qualified': True, 'qualification_passed': True, 'budget_violations': [],
        'AA_confidence_rows': rows, 'confidence_scope': analysis['confidence_scope'],
        'competitor_contrasts_computed': False, 'target_status': analysis['target_status'],
        'performance_target_claimed': False, 'F64_admitted': False,
        'restored': ref(restored_path), 'actual_pause_coverage': ref(coverage_path),
        'actual_power_completed': ref(power_path), 'actual_pause_completed': ref(pause_path),
        'power_window_audit': report['power_window'], 'restoration_preceded_audit': True,
        'body_proofs': len(ledger['body_proofs']), 'body_anchors': ledger['body_anchors'],
        'allocated_output_bytes': ledger['allocated_output_bytes'], 'logical_retained_bytes': ledger['logical_retained_bytes'],
        'actual_report_retained_output_bytes': report['retained_output_bytes'],
        'generated_wire_bytes': ledger['generated_wire_bytes'], 'generated_primary_wire_bytes': ledger['generated_primary_wire_bytes'],
        'minimum_observed_free_bytes': ledger['minimum_observed_free_bytes'],
        'prelude_queue': {name: ref(PRE / name) for name in ('request.json', 'result.json', 'events.json', 'supervisor.log', 'stdout.log', 'stderr.log', 'manifest.json')},
        'timing_queue': {name: ref(OUT / 'queue' / name) for name in ('request.json', 'result.json', 'events.json', 'supervisor.log', 'stdout.log', 'stderr.log', 'manifest.json')},
        'audit_queue': {name: ref(AUD / name) for name in ('request.json', 'result.json', 'events.json', 'supervisor.log', 'stdout.log', 'stderr.log', 'manifest.json')},
        'original_prelude_timing_queue_audits': ref(P / 'Q32_PRELUDE_AND_TIMING_ORIGINAL_QUEUE_AUDITS_v1.json'),
        'original_offline_audit_queue_audit': ref(check_path),
        'timing_run_id': tq['run_id'], 'timing_elapsed_seconds': tq['command_elapsed_seconds'],
        'audit_run_id': aq['run_id'], 'audit_elapsed_seconds': aq['command_elapsed_seconds'],
        'new_capture': 0, 'compile': 0, 'retry': 0, 'extra_benchmark_children_after_Q32': 0,
        'index_activity': 'Small fixed metadata read/hash and original audit-queue metadata audit only. Completed full original body/source/receipt/statistical audit reused; no new wire/canonical/query rehash or statistics test.',
        'limitations': ['Fixed actual WSL/toolchain/live host and original temporary existing HighPerformance scope only.',
            'Two authorized processes paused; no physical-core exclusivity, frequency lock or whole-machine quiet claim.',
            'Nominal model-conditional AA family; Q32 does not compute cross-library targets or establish F64 success.']}
    path = P / 'Q32_ACTUAL_RESULT_INDEX_v1.json'; write(path, index)
    print(json.dumps(ref(path)))

if __name__ == '__main__':
    main()
