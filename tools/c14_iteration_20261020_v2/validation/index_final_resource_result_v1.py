"""Small actual-result index; no native, project imports or repeated body audit."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
D = ROOT / 'docs/coordination/c14-iteration-20261020-v2'
V = D / 'validation/final_formal_v1_preparation'
OUT = Path('/root/xoffset-c14-iteration/20261020/v2/validation/final-six-role-resource-v1')


def read(p):
    return json.loads(Path(p).read_text(encoding='utf-8-sig'))


def fid(p):
    b = Path(p).read_bytes()
    return dict(bytes=len(b), sha256=hashlib.sha256(b).hexdigest())


def ref(p):
    return dict(path=str(p), identity=fid(p))


def write(p, value):
    with p.open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2)
        stream.write('\n')


def main():
    request = V / 'RESOURCE_EXECUTION_REQUEST_v1.json'
    q = read(request)
    result = read(OUT / 'result.json')
    queue = OUT / 'queue'
    qr, rr = read(queue / 'request.json'), read(queue / 'result.json')
    admission = D / 'reviews/FINAL_SIX_ROLE_RESOURCE_EXECUTION_ADMISSION.json'
    assert fid(request)['sha256'] == result['request_sha256'] == read(admission)['request_sha256'] == '89de4706638869ac1ff6c4a1f5a527809a023ff1b30e3a01dd76e10b37dcf81e'
    assert result['status'] == 'passed' and len(result['attempted']) == len(result['completed']) == 12
    assert rr['status'] == 'complete' and rr['command_returncode'] == 0 and rr['reason'] == 'command_exit'
    assert rr['cleanup'] == dict(live=[], process_groups=[], zombies=[]) and rr['run_id'] == qr['run_id']
    assert qr['command'] == read(V / 'RESOURCE_REQUEST_SOURCE_READY_v1.json')['argv'][16:]
    assert qr['timeout_seconds'] == 600 and qr['owner'] == 'W3' and qr['kind'] == 'functional'
    qmanifest = read(queue / 'manifest.json')
    assert {r['path'] for r in qmanifest['files']} == {'request.json', 'result.json', 'events.json', 'stdout.log', 'stderr.log', 'supervisor.log'}
    for item in qmanifest['files']:
        assert fid(queue / item['path']) == {k: item[k] for k in ('bytes', 'sha256')}
    assert read(OUT / 'runtime-before.json') == read(OUT / 'runtime-after.json')
    profile = read(OUT / 'profile.json')
    rows = []
    for expected, actual in zip(q['jobs'], result['completed']):
        prefix = OUT / 'commands' / expected['id']
        receiptpath = prefix.with_suffix('.receipt.json')
        receipt = read(receiptpath)
        assert receipt['argv'] == expected['argv'] and receipt['execution_environment'] == profile['environment']
        assert receipt['returncode'] == 0 and receipt['timed_out'] is False and receipt['launch_error'] is None and receipt['timeout_seconds'] == 20
        elapsed = (int(receipt['finished_monotonic_ns']) - int(receipt['started_monotonic_ns'])) / 1e9
        assert 0 < elapsed <= 20 and elapsed == actual['full_process_seconds'] and actual['execute_truth_pool_wall_seconds'] >= elapsed
        assert str(fid(receiptpath)['bytes']) == actual['receipt']['bytes'] and fid(receiptpath)['sha256'] == actual['receipt']['sha256']
        assert prefix.with_suffix('.stdout').stat().st_size == prefix.with_suffix('.stderr').stat().st_size == 0
        rows.append(dict(role=expected['role'], execution_kind=expected['execution_kind'], receipt=ref(receiptpath),
                         full_process_seconds=elapsed, execute_truth_pool_wall_seconds=actual['execute_truth_pool_wall_seconds']))
    storage = read(OUT / 'resource-ledger.json')
    assert storage == result['storage'] and storage['status'] == 'passed'
    assert storage['recorded_attempts'] == 12 and storage['deduplicated_body_slots'] == len(storage['body_proofs']) == 36
    manifest = read(OUT / 'manifest.json')['files']
    side = sum(int(manifest['frozen/fixture/' + n]['bytes']) for n in ('final.canonical.bin', 'queries.bin'))
    captures = read(V / 'CAPTURE_REUSE_INDEX_DRAFT.json')['captures']['normal']['roles']
    wires = {role: int(record['files']['.wire']['bytes']) for role, record in captures.items()}
    tool_bytes = sum(int(v['bytes']) for k, v in manifest.items() if k.startswith('protocol/'))
    representative_frozen_logical = sum(int(v['bytes']) for k, v in manifest.items() if k.startswith('frozen/'))
    anchors = sum(wires.values()) + side
    projected = {}
    for name, children, primary, per_role, seconds, reserve, allocated_cap, logical_cap in (
            ('Q32', 4656, 2328, 388, 2700, 372244480, 4 * 1024**3, 256 * 1024**3),
            ('F64', 9264, 4632, 772, 5400, 674234368, 8 * 1024**3, 512 * 1024**3)):
        generated = 2 * per_role * sum(wires.values())
        initial_base_reserve = q['preparation_base_reserve_bytes']
        retained = generated + children * (side + 65536) + initial_base_reserve + tool_bytes + reserve + anchors
        temporary_peak = 7 * (max(max(wires.values()), side, 1024**2) + 4096) + reserve
        process_time = per_role * result['observed_process_S'] + 300
        online_time = per_role * result['observed_execute_truth_pool_S'] + 300
        projected[name] = dict(actual_children=children, primary_children=primary,
            generated_wire_bytes=generated, primary_generated_wire_bytes=generated // 2,
            logical_retained_upper_with_reserved_base=retained, logical_cap_bytes=logical_cap,
            proposed_frozen_base_reserve_bytes=initial_base_reserve,
            original_next_child_temporary_physical_peak_bytes=temporary_peak,
            startup_physical_upper_with_reserved_base=initial_base_reserve + temporary_peak,
            allocated_cap_bytes=allocated_cap, required_free_margin_each_bytes=2 * 1024**3,
            process_necessary_seconds=process_time, execute_truth_pool_conservative_seconds=online_time,
            original_window_seconds=seconds, arithmetic_necessary_limits_fit=retained <= logical_cap and initial_base_reserve + temporary_peak <= allocated_cap and online_time <= seconds,
            actual_final_formal_frozen_base_still_required=True,
            future_unique_body_count_and_tail_not_guaranteed=True, QF_admitted=False)
    projection = V / 'RESOURCE_ACTUAL_PROJECTION_v1.json'
    write(projection, dict(status='actual_fixed12_observation_projected_not_QF_admission',
        raw_result=ref(OUT / 'result.json'), original_queue_result=ref(queue / 'result.json'),
        by_role=rows, measured_process_S=result['observed_process_S'], measured_execute_truth_pool_S=result['observed_execute_truth_pool_S'],
        projections=projected, representative_resource_frozen_logical_bytes=representative_frozen_logical,
        actual_retained_resource_allocated_before_final_metadata_bytes=storage['allocated_output_bytes'],
        actual_resource_metadata_reserved_bytes=storage['policy']['metadata_reserve_bytes'],
        actual_resource_generated_wire_bytes=storage['generated_wire_bytes'],
        actual_resource_logical_retained_bytes=storage['logical_retained_bytes'],
        observed_guest_free_bytes=result['current_guest_free_bytes'], observed_backing_free_bytes=result['current_backing_free_bytes'],
        source_or_staging_budget_recycled=False, new_capture=0, new_compile=0, native=12, inference_samples=0,
        note='Original outer native/online work and post-exit fulltruth+3body pool retained. Existing archives/failed prefixes consume observed free space and are never removed. Source-only Q/F freeze must still bind actual base/staging/metadata/host/power/prelude/recovery and independent acceptance. Observed times are not future-tail guarantees.'))
    index = V / 'RESOURCE_ACTUAL_READY_v1.json'
    write(index, dict(status='passed_actual_fixed12_pending_independent_actual_acceptance', request=ref(request), admission=ref(admission),
        result=ref(OUT / 'result.json'), status_record=ref(OUT / 'status.json'), sealed_manifest=ref(OUT / 'manifest.json'),
        queue_manifest=ref(queue / 'manifest.json'), queue_six_metadata=[ref(queue / r['path']) for r in qmanifest['files']],
        queue_run_id=rr['run_id'], queue_elapsed_seconds=rr['command_elapsed_seconds'], cleanup=rr['cleanup'],
        runtime_before=ref(OUT / 'runtime-before.json'), runtime_after=ref(OUT / 'runtime-after.json'),
        original_body_ledger=ref(OUT / 'resource-ledger.json'), scalar_projection=ref(projection), actual12_receipts=rows,
        metadata_only_index_checks=True, original_under_lease_full_truth_and_body_audit_not_repeated=True,
        current_host_activity_retained=True, processes_paused=0, power_changed=False,
        new_native_in_this_index=0, compile=0, capture=0, retries=0, inference_samples=0, QF_admitted=False))
    print(json.dumps(dict(index=ref(index), projection=ref(projection), queue_run_id=rr['run_id'])))


if __name__ == '__main__':
    main()
