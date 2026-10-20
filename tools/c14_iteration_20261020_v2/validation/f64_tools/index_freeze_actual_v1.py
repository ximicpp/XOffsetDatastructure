"""Index maintained once-only freeze result without another large-body audit."""
from pathlib import Path
import datetime, hashlib, json, shutil, stat, subprocess

R = Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
D = R / 'docs/coordination/c14-iteration-20261020-v2'
V = D / 'validation'
P = V / 'f64_preparation'
B = Path('/root/xoffset-c14-iteration/20261020/v2/validation/final-formal-v1')
F = B / 'frozen-formal-v1'
Q = B / 'leases/formal-freeze-v1'
RUNNER = Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py')

def fid(path):
    path = Path(path); info = path.lstat()
    assert stat.S_ISREG(info.st_mode) and info.st_size <= 2 * 1024**2
    raw = path.read_bytes(); return {'bytes': str(len(raw)), 'sha256': hashlib.sha256(raw).hexdigest()}

def norm(v):
    return {'bytes': str(v['bytes']), 'sha256': v['sha256']}

def read(path):
    limit = 3 * 1024**2 if Path(path) == F / 'runtime.json' else 2 * 1024**2
    assert Path(path).stat().st_size <= limit
    return json.loads(Path(path).read_bytes())

def ref(path):
    return {'path': str(path), 'identity': fid(path)}

def write(path, value):
    with path.open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(value, stream, indent=2, sort_keys=True); stream.write('\n')

def main():
    status, manifest, result, request = (read(F / 'status.json'), read(F / 'manifest.json'),
                                        read(Q / 'result.json'), read(Q / 'request.json'))
    stdout = json.loads((Q / 'stdout.log').read_text())
    assert result['status'] == 'complete' and result['command_returncode'] == 0
    assert result['cleanup'] == {'live': [], 'process_groups': [], 'zombies': []}
    assert request['owner'] == result['owner'] == 'W1' and request['kind'] == 'validation'
    assert request['timeout_seconds'] == 600 and request['wait_seconds'] == 0
    admission = read(D / 'reviews/F64_SOURCE_FREEZE_CONDITIONAL_ADMISSION_v1.json')
    assert request['command'] == admission['argv'][admission['argv'].index('--') + 1:]
    assert stdout['status'] == 'frozen' and norm(stdout['manifest']) == fid(F / 'manifest.json')
    assert status['status'] == 'complete' and status['performance_accepted'] is False
    assert status['campaign'] == 'formal' and status['actual_children'] == 9264
    assert status['primary_children'] == status['conditioning_children'] == 4632
    assert status['max_seconds'] == 5400 and status['window_out'] == str(B / 'formal-v1')
    inventory = {}
    allocated_inodes = {}; apparent = 0; small_hashed = 0
    for path in sorted(F.rglob('*')):
        info = path.lstat()
        if stat.S_ISDIR(info.st_mode):
            continue
        assert stat.S_ISREG(info.st_mode)
        name = path.relative_to(F).as_posix()
        inventory[name] = info.st_size; apparent += info.st_size
        allocated_inodes[(info.st_dev, info.st_ino)] = info.st_blocks * 512
        if name != 'manifest.json':
            assert info.st_size == int(manifest['files'][name]['bytes'])
            if name.endswith(('.json', '.py', '.ps1')) and info.st_size <= 2 * 1024**2:
                assert fid(path) == norm(manifest['files'][name]); small_hashed += 1
    assert set(inventory) == set(manifest['files']) | {'manifest.json'}
    basis = read(F / 'runtime.json')
    ready = read(V / 'final_formal_v1_preparation/FINAL_FORMAL_BOUND_SOURCE_READY.json')
    for item in ready['inventory'].values():
        assert fid(item['bound']['path']) == norm(item['bound']['identity'])
    assert len(basis['tools']) == 11
    assert read(F / 'proposal.json') == read(R / 'tools/c14_iteration_20261020_v2/validation/final_formal_bound_v1/protocol-proposal-v1.json')
    proposal = read(F / 'proposal.json')
    for name, expected in proposal['coordinator_references'].items():
        assert fid(F / 'registration' / name) == norm(expected)
    policy = status['resource_policy']; wires = status['final_wire_bytes']
    side = (F / 'fixture/final.canonical.bin').stat().st_size + (F / 'fixture/queries.bin').stat().st_size
    peak = 7 * (max(*wires.values(), side, 1024**2) + 4096) + policy['metadata_reserve_bytes']
    base = status['frozen_projection_base_bytes']
    assert status['predicted_wire_bytes'] <= policy['maximum_wire_bytes']
    assert status['predicted_primary_wire_bytes'] <= policy['maximum_primary_wire_bytes']
    assert status['predicted_retained_bytes'] <= policy['maximum_logical_retained_bytes']
    assert base + peak <= policy['maximum_allocated_output_bytes']
    guest, backing = shutil.disk_usage(F).free, shutil.disk_usage('/mnt/c').free
    assert min(guest, backing) >= policy['maximum_allocated_output_bytes'] + policy['disk_margin_bytes']
    assert fid(F / 'qualification-acceptance.json') == norm(admission['qualification_acceptance']['identity'])
    assert fid(admission['qualification_acceptance']['path']) == fid(F / 'qualification-acceptance.json')
    qualified = read(F / 'qualification-acceptance.json')
    assert qualified['status'] == 'passed' and qualified['measurement_qualified'] is True and qualified['complete'] is True
    primary = read(F / 'schedule.json'); native = read(F / 'native-schedule.json')
    assert len(primary) == 4632 and len(native) == 9264
    roles = {'old_xoffset', 'new_xoffset', 'cista', 'flatbuffers', 'capnproto', 'cereal'}
    from collections import Counter
    assert Counter(j['role'] for j in primary) == {role: 772 for role in roles}
    assert Counter(j['execution_kind'] for j in native) == {'conditioning': 4632, 'primary': 4632}
    assert Counter(j['role'] for j in native) == {role: 1544 for role in roles}
    assert sum(j['kind'] == 'warmup' for j in primary) == 24
    measured = [j for j in primary if j['kind'] == 'comparison']
    assert len(measured) == 4608 and set(j['macrocycle'] for j in measured) == set(range(64))
    assert Counter(j['macrocycle'] for j in measured) == {i: 72 for i in range(64)}
    assert Counter(j['block'] for j in measured) == {i: 12 for i in range(384)}
    for i, job in enumerate(primary):
        conditioner, actual = native[2*i:2*i+2]
        assert conditioner == {'execution_kind': 'conditioning', 'id': 'c' + format(i, '05d'), 'primary_id': job['id'], 'primary_index': i, 'role': job['role']}
        assert actual == {'execution_kind': 'primary', 'id': job['id'], 'primary_id': job['id'], 'primary_index': i, 'role': job['role']}
    assert len({j['id'] for j in native}) == 9264
    policy_record = read(F / 'policy.json'); envelope_record = read(F / 'envelope-policy.json')
    assert policy_record == proposal['campaigns']['formal']['statistics'] and envelope_record == proposal['campaigns']['formal']['envelope']
    assert policy_record['macrocycles'] == 64 and policy_record['degrees_of_freedom'] == 63
    assert policy_record['AA_contrasts'] == 30 and policy_record['primary_contrasts'] == 12 and policy_record['diagnostic_contrasts'] == 40
    assert policy_record['primary_thresholds'] == {'read_sum': 1.05, 'mutate': 1.05, 'emit_after': .2}
    argv = ['/usr/bin/python3', '-I', '-B', str(RUNNER), 'audit', '--out', str(Q)]
    audit = subprocess.run(argv, capture_output=True, text=True, timeout=30)
    audit_path = P / 'F64_FREEZE_ORIGINAL_QUEUE_AUDIT_v1.json'
    write(audit_path, {'argv': argv, 'returncode': audit.returncode, 'stdout': audit.stdout, 'stderr': audit.stderr})
    assert audit.returncode == 0
    index = {'status': 'maintained_source_freeze_completed_verified_no_timing', 'captured_at': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'frozen_directory': str(F), 'frozen_manifest': ref(F / 'manifest.json'), 'frozen_status': ref(F / 'status.json'),
        'maintained_verify_frozen_completed': True, 'basis': 'Exact admitted source calls verify_frozen after sealing and only then emits frozen stdout; original queue exit0. Its actual full capture/body/source/runtime checks are retained, not repeated by this small index.',
        'new_large_body_rehash_by_index': False, 'all_regular_member_names_and_sizes_match_manifest': True,
        'manifest_member_count': len(manifest['files']), 'small_metadata_source_members_rehashed': small_hashed,
        'actual_frozen_apparent_bytes_after_seal': apparent, 'actual_frozen_allocated_unique_inode_bytes_after_seal': sum(allocated_inodes.values()),
        'unique_frozen_regular_inodes': len(allocated_inodes), 'frozen_projection_base_bytes_before_final_metadata': base,
        'maintained_worst_child_peak_plus_metadata_reserve_bytes': peak, 'base_plus_peak_bytes': base + peak,
        'future_copy_frozen_apparent_plus_protocol_source_bytes': apparent + sum(int(v['bytes']) for v in basis['tools'].values()),
        'resource_policy': policy, 'predicted_wire_bytes': status['predicted_wire_bytes'],
        'predicted_primary_wire_bytes': status['predicted_primary_wire_bytes'], 'predicted_retained_bytes': status['predicted_retained_bytes'],
        'predicted_pool_anchor_bytes': status['predicted_pool_anchor_bytes'],
        'guest_free_bytes': guest, 'backing_free_bytes': backing,
        'required_free_bytes_cap_plus_margin': policy['maximum_allocated_output_bytes'] + policy['disk_margin_bytes'],
        'capacity_observation_scope': 'Current post-freeze free-space snapshot only; actual run and every child retain original admission checks.',
        'actual_registration_verified': ref(V / 'q32_freeze_preparation/REGISTRATION_ACTUAL_VERIFIED_v1.json'),
        'conditional_admission': ref(D / 'reviews/F64_SOURCE_FREEZE_CONDITIONAL_ADMISSION_v1.json'),
        'prelaunch': ref(P / 'F64_FREEZE_PRELAUNCH_ACTUAL_v1.json'),
        'queue': {name: ref(Q / name) for name in ('request.json', 'result.json', 'events.json', 'supervisor.log', 'stdout.log', 'stderr.log', 'manifest.json')},
        'queue_original_audit': ref(audit_path), 'run_id': result['run_id'], 'elapsed_seconds': result['command_elapsed_seconds'],
        'schedule': {'actual': 9264, 'primary': 4632, 'conditioning': 4632, 'warmup_primary': 24, 'measured_primary': 4608, 'macrocycles': 64, 'blocks': 384, 'each_primary_role': 772, 'each_actual_role': 1544, 'immediate_conditioning_primary_pairs_verified': True},
        'primary_schedule': ref(F / 'schedule.json'), 'native_schedule': ref(F / 'native-schedule.json'),
        'formal_policy': ref(F / 'policy.json'), 'formal_envelope': ref(F / 'envelope-policy.json'),
        'qualification_acceptance': ref(F / 'qualification-acceptance.json'),
        'unchanged_original_F_policies_match_proposal': True,
        'native': 0, 'compile': 0, 'new_capture': 0, 'process_pause': 0, 'power_change': False,
        'performance_accepted': False, 'Q32_qualified_prerequisite_checked_by_original_freeze': True, 'F64_timing_admitted': False}
    path = P / 'F64_FREEZE_ACTUAL_READY_v1.json'; write(path, index)
    print(json.dumps(ref(path)))

if __name__ == '__main__':
    main()
