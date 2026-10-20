"""Index completed F64 original audit; fixed metadata only, no body/statistical rerun."""
from pathlib import Path
import datetime
import hashlib
import json

R = Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
D = R / 'docs/coordination/c14-iteration-20261020-v2'
V = D / 'validation'
P = V / 'f64_preparation'
C = V / 'final_formal_v1'
B = Path('/root/xoffset-c14-iteration/20261020/v2/validation/final-formal-v1')
OUT = B / 'formal-v1'
PRE = B / 'leases/formal-host-prelude-v1'
AUD = B / 'leases/formal-offline-audit-v1'


def metadata(path):
    path = Path(path)
    assert path.is_file() and not path.is_symlink()
    assert path.stat().st_size <= 64 * 1024**2
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


def compact(group, label):
    rows = []
    for role, metrics in group.items():
        for metric, item in metrics.items():
            assert item['df'] == 63 and item['inference_units'] == 64
            rows.append({'group': label, 'role': role, 'metric': metric,
                         **{k: v for k, v in item.items() if not isinstance(v, (dict, list))}})
    return rows


def main():
    report_path = V / 'formal_freeze_preparation/F64_OFFLINE_AUDIT_RESULT_v1.json'
    report = read(report_path)
    analysis = report['analysis']
    status = read(OUT / 'status.json')
    assert report['status'] == 'passed' and report['complete'] is True
    assert report['measurement_qualified'] is analysis['measurement_qualified'] is True
    assert report['successful_actual_children'] == report['attempted_actual_children'] == 9264
    assert report['successful_primary_children'] == report['successful_conditioning_children'] == 4632
    assert report['unexecuted_actual_children'] == 0 and status['error'] is None
    assert report['budget_violations'] == []
    assert analysis['measured_children'] == 4608 and analysis['excluded_warmup_children'] == 24
    assert analysis['completed_blocks'] == analysis['required_blocks'] == 384
    assert analysis['inference']['units'] == 64 and analysis['inference']['df'] == 63
    assert analysis['competitor_contrasts_computed'] is True
    assert analysis['all_measured_rows_retained'] is analysis['no_outlier_filter'] is True
    aa = compact(analysis['AA'], 'AA')
    primary = compact(analysis['primary_targets'], 'primary_targets')
    diagnostics = compact(analysis['auxiliary_diagnostics'], 'auxiliary_diagnostics')
    assert (len(aa), len(primary), len(diagnostics)) == (30, 12, 40)
    assert all(row['available'] and row['equivalence_established'] for row in aa)
    assert all(row['family_contrasts'] == 30 for row in aa)
    assert all(row['family_contrasts'] == 12 for row in primary)
    assert all(row['family_contrasts'] == 40 for row in diagnostics)
    assert report['target_achieved'] is analysis['target_achieved']
    assert report['promotion_eligible'] is analysis['promotion_eligible']
    assert report['confirmed_cost_transfer'] is analysis['confirmed_cost_transfer']
    # Preserve every original result, including failures; no threshold or outcome changes.
    assert report['primary_targets'] == analysis['primary_targets']
    assert report['auxiliary_diagnostics'] == analysis['auxiliary_diagnostics']
    aq, tq = read(AUD / 'result.json'), read(OUT / 'queue/result.json')
    for queue in (PRE, OUT / 'queue', AUD):
        request, result = read(queue / 'request.json'), read(queue / 'result.json')
        assert result['status'] == 'complete' and result['command_returncode'] == 0
        assert result['cleanup'] == {'live': [], 'process_groups': [], 'zombies': []}
        assert request['run_id'] == result['run_id'] and request['wait_seconds'] == 0
    assert tq['run_id'] == 'dd86951c94214459ad6af99ac707f2dd'
    assert aq['run_id'] == 'd3a610bd7559478a906f86606c76925b'
    assert aq['owner'] == tq['owner'] == 'W3'
    assert aq['kind'] == 'functional' and tq['kind'] == 'timing'
    assert read(AUD / 'request.json')['timeout_seconds'] == 3600
    assert read(OUT / 'queue/request.json')['timeout_seconds'] == 5400
    dispatch = read(C / 'formal-controller-v1/TIMING_DISPATCH_RESULT.json')
    restored_path = C / 'formal-controller-v1/RESTORED.json'
    coverage_path = C / 'host-formal-v1/TIMING_PAUSE_COVERAGE.json'
    power_path = C / 'power-formal-v1/completed.json'
    pause_path = C / 'host-formal-v1/process-state-v1/completed.json'
    restored, coverage, power, pause = map(read, (restored_path, coverage_path, power_path, pause_path))
    assert dispatch['formal_attempts'] == 1 and dispatch['retries'] == 0 and dispatch['returncode'] == 0
    assert restored['status'] == 'passed' and restored['user_processes_restored'] is True
    assert restored['error'] is None and restored['power_error'] is None
    assert coverage['status'] == 'passed' and coverage['actual_suspended'] == 2
    assert power['status'] == pause['status'] == 'passed'
    assert power['restored'] is pause['restored'] is True
    assert power['original'] == restored['power_before_evidence']['guid'] == restored['power_after_evidence']['guid']
    assert power['original'] == '381b4222-f694-41f0-9685-ff5bb260df2e'
    assert report['power_window']['valid_complete_power_window'] is True
    assert report['power_window']['restored'] is True
    parse = lambda value: datetime.datetime.fromisoformat(value.replace('Z', '+00:00'))
    assert parse(coverage['ready_observed_utc']) <= parse(tq['started_at'])
    assert parse(tq['finished_at']) <= parse(coverage['stop_requested_utc'])
    preaudit = read(P / 'F64_OFFLINE_AUDIT_EXECUTION_REQUEST_v1.json')
    for key, path in (('restored', restored_path), ('actual_power_completed', power_path),
                      ('actual_pause_coverage', coverage_path)):
        assert norm(preaudit[key]['identity']) == fid(path) and preaudit[key]['path'] == str(path)
    assert norm(restored['guard_completed']['identity']) == fid(pause_path)
    assert norm(restored['power_completed']['identity']) == fid(power_path)
    assert read(OUT / 'runtime-before.json') == read(OUT / 'runtime-after.json') == read(OUT / 'frozen/runtime.json')
    source_refs = {}
    for name, entry in read(V / 'final_formal_v1_preparation/FINAL_FORMAL_BOUND_SOURCE_READY.json')['inventory'].items():
        assert fid(entry['bound']['path']) == norm(entry['bound']['identity'])
        source_refs[name] = entry['bound']
    host_sources = {}
    for name, size, sha in (
        ('execute.py', 19409, '5ead5e942d97698196a4e1a2b99799f3f44e19957c51073dc41e4062d0af9a6c'),
        ('process_pause.py', 16289, 'b5c4d5890309bab3bdd68e5e271b0c9978834741760dbce55435ce3dd78549b8'),
        ('pause-control.ps1', 12670, '0898a5f0e1be74368be71161be970db6e85ae63c4782df8638aa705f471e5465')):
        path = R / 'tools/c14_iteration_20261020_v2/validation/formal_host_v1' / name
        assert fid(path) == {'bytes': size, 'sha256': sha}
        host_sources[name] = ref(path)
    q_path = C / 'qualification-acceptance.json'
    assert fid(q_path) == fid(OUT / 'frozen/qualification-acceptance.json')
    assert fid(q_path)['sha256'] == '4aa3c2ce7eb3e1566e5eaee5fdfe5a664a26661f2f7d71a057f20e93e49e5fd1'
    for name, path in (('frozen_manifest', OUT / 'frozen/manifest.json'), ('run_manifest', OUT / 'manifest.json'),
                       ('queue_manifest', OUT / 'queue/manifest.json'), ('queue_result', OUT / 'queue/result.json')):
        assert norm(report[name]) == fid(path)
    ledger_path = OUT / 'resource-ledger.json'
    ledger = read(ledger_path)
    assert norm(report['resource_ledger']) == fid(ledger_path) and ledger['status'] == 'passed'
    assert ledger['recorded_attempts'] == 9264
    assert ledger['deduplicated_body_slots'] == len(ledger['body_proofs']) == 27792
    postpin = hashlib.sha256(b'SC-POST-RUN-PIN-v1\0' + bytes.fromhex(fid(OUT / 'manifest.json')['sha256'])
                            + bytes.fromhex(fid(OUT / 'queue/manifest.json')['sha256'])).hexdigest()
    assert postpin == preaudit['postrun_pin']
    queue_audit_path = P / 'F64_OFFLINE_AUDIT_ORIGINAL_QUEUE_AUDIT_v1.json'
    queue_audit = read(queue_audit_path)
    assert queue_audit['returncode'] == 0 and json.loads(queue_audit['stdout'])['status'] == 'audit_passed'
    index = {
        'status': 'F64_complete_original_full_audit_passed_measurement_qualified_target_not_established_pending_root_review',
        'captured_at': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'audit_report': ref(report_path), 'run_directory': str(OUT),
        'actual_run_manifest': ref(OUT / 'manifest.json'), 'actual_queue_manifest': ref(OUT / 'queue/manifest.json'),
        'postrun_pin': postpin, 'frozen_manifest': ref(OUT / 'frozen/manifest.json'),
        'run_status': ref(OUT / 'status.json'), 'analysis_record': ref(OUT / 'analysis.json'),
        'resource_ledger': ref(ledger_path), 'runtime_before': ref(OUT / 'runtime-before.json'),
        'runtime_after': ref(OUT / 'runtime-after.json'), 'actual_source_runtime_closure_equal': True,
        'accepted_live_bound_source11_unchanged': True, 'accepted_bound_sources': source_refs,
        'accepted_formal_host_source3_unchanged': True, 'formal_host_sources': host_sources,
        'qualified_Q_current_and_frozen_equal': True, 'qualified_Q_acceptance': ref(q_path),
        'actual_native_children': 9264, 'successful_conditioning_children': 4632,
        'successful_primary_children': 4632, 'unexecuted_actual_children': 0,
        'primary_warmup_excluded': 24, 'measured_primary_children': 4608,
        'macrocycles': 64, 'completed_blocks': 384, 'AA_count': 30,
        'all_AA_equivalent': True, 'measurement_qualified': report['measurement_qualified'],
        'competitor_contrasts_computed': analysis['competitor_contrasts_computed'],
        'all_inference_defined': analysis['all_inference_defined'],
        'external_binary_identity_verified_in_math_record': analysis['external_binary_identity_verified'],
        'external_binary_identity_scope': 'The unchanged mathematical record has external_binary_identity_verified=false; actual binary/source/runtime identity closure is separately verified by the completed original outer auditor and pinned runtime records.',
        'target_achieved': report['target_achieved'], 'promotion_eligible': report['promotion_eligible'],
        'confirmed_cost_transfer': report['confirmed_cost_transfer'],
        'confirmed_cost_transfer_contrasts': analysis['confirmed_cost_transfer_contrasts'],
        'primary_targets_all_pass': analysis['primary_targets_all_pass'],
        'primary_target_count': len(primary), 'primary_targets_passed': sum(row['target_pass'] for row in primary),
        'primary_targets_not_established': sum(not row['target_pass'] for row in primary),
        'target_status': analysis['target_status'], 'failure_reasons': analysis['failure_reasons'],
        'budget_violations': report['budget_violations'], 'AA_confidence_rows': aa,
        'primary_target_confidence_rows': primary, 'auxiliary_diagnostic_confidence_rows': diagnostics,
        'confidence_scope': analysis['confidence_scope'], 'inference': analysis['inference'],
        'restored': ref(restored_path), 'actual_pause_coverage': ref(coverage_path),
        'actual_power_completed': ref(power_path), 'actual_pause_completed': ref(pause_path),
        'power_window_audit': report['power_window'], 'restoration_preceded_audit': True,
        'restoration_order_basis': 'Actual closed restore/power/pause-coverage identities were checked and pinned in the actual pre-audit dispatch record before invoking the original auditor. WSL ready/queue/stop coverage is checked within the WSL clock domain. Windows and WSL UTC clocks are not ordered against each other.',
        'preaudit_execution_request': ref(P / 'F64_OFFLINE_AUDIT_EXECUTION_REQUEST_v1.json'),
        'body_proofs': len(ledger['body_proofs']), 'body_anchors': ledger['body_anchors'],
        'allocated_output_bytes': ledger['allocated_output_bytes'], 'logical_retained_bytes': ledger['logical_retained_bytes'],
        'actual_report_retained_output_bytes': report['retained_output_bytes'],
        'generated_wire_bytes': ledger['generated_wire_bytes'], 'generated_primary_wire_bytes': ledger['generated_primary_wire_bytes'],
        'minimum_observed_free_bytes': ledger['minimum_observed_free_bytes'],
        'prelude_queue': {name: ref(PRE / name) for name in ('request.json', 'result.json', 'events.json', 'supervisor.log', 'stdout.log', 'stderr.log', 'manifest.json')},
        'timing_queue': {name: ref(OUT / 'queue' / name) for name in ('request.json', 'result.json', 'events.json', 'supervisor.log', 'stdout.log', 'stderr.log', 'manifest.json')},
        'audit_queue': {name: ref(AUD / name) for name in ('request.json', 'result.json', 'events.json', 'supervisor.log', 'stdout.log', 'stderr.log', 'manifest.json')},
        'original_prelude_timing_queue_audits': ref(P / 'F64_PRELUDE_AND_TIMING_ORIGINAL_QUEUE_AUDITS_v1.json'),
        'original_offline_audit_queue_audit': ref(queue_audit_path),
        'timing_run_id': tq['run_id'], 'timing_elapsed_seconds': tq['command_elapsed_seconds'],
        'audit_run_id': aq['run_id'], 'audit_elapsed_seconds': aq['command_elapsed_seconds'],
        'new_capture': 0, 'compile': 0, 'retry': 0, 'extra_benchmark_children_after_F64': 0,
        'index_activity': 'Fixed named metadata read/hash only; completed original full body/receipt/source/power/statistical audit and completed audit-queue metadata audit reused. No additional full body hash, statistics test, benchmark or capture.',
        'physical_core_exclusive_or_frequency_locked_claimed': False,
        'limitations': ['Fixed actual WSL/toolchain and live host with original temporary existing HighPerformance scope only.',
                        'Only two authorized processes paused; no whole-machine/account quiet, physical-core exclusivity or frequency-lock claim.',
                        'Separate nominal model-conditional confidence families; no combined overall 95% confidence claim.',
                        'Original F64 target not established; no rerun, sample extension, threshold change or integration authorized.'],
    }
    path = P / 'F64_ACTUAL_RESULT_INDEX_v1.json'
    with path.open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(index, stream, indent=2, sort_keys=True)
        stream.write('\n')
    print(json.dumps(ref(path)))


if __name__ == '__main__':
    main()
