"""Index maintained once-only freeze result without another large-body audit."""
from pathlib import Path
import datetime, hashlib, json, shutil, stat, subprocess

R = Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
D = R / 'docs/coordination/c14-iteration-20261020-v2'
V = D / 'validation'
P = V / 'q32_freeze_preparation'
B = Path('/root/xoffset-c14-iteration/20261020/v2/validation/final-formal-v1')
F = B / 'frozen-qualification-v1'
Q = B / 'leases/qualification-freeze-v1'
RUNNER = Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py')

def fid(path):
    path = Path(path); info = path.lstat()
    assert stat.S_ISREG(info.st_mode) and info.st_size <= 2 * 1024**2
    raw = path.read_bytes(); return {'bytes': str(len(raw)), 'sha256': hashlib.sha256(raw).hexdigest()}

def norm(v):
    return {'bytes': str(v['bytes']), 'sha256': v['sha256']}

def read(path):
    assert Path(path).stat().st_size <= 2 * 1024**2
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
    admission = read(D / 'reviews/Q32_SOURCE_FREEZE_CONDITIONAL_ADMISSION_v1.json')
    assert request['command'] == admission['argv'][admission['argv'].index('--') + 1:]
    assert stdout['status'] == 'frozen' and norm(stdout['manifest']) == fid(F / 'manifest.json')
    assert status['status'] == 'complete' and status['performance_accepted'] is False
    assert status['campaign'] == 'qualification' and status['actual_children'] == 4656
    assert status['primary_children'] == status['conditioning_children'] == 2328
    assert status['max_seconds'] == 2700 and status['window_out'] == str(B / 'qualification-v1')
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
    argv = ['/usr/bin/python3', '-I', '-B', str(RUNNER), 'audit', '--out', str(Q)]
    audit = subprocess.run(argv, capture_output=True, text=True, timeout=30)
    audit_path = P / 'FREEZE_ORIGINAL_QUEUE_AUDIT_v1.json'
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
        'actual_registration_verified': ref(P / 'REGISTRATION_ACTUAL_VERIFIED_v1.json'),
        'conditional_admission': ref(D / 'reviews/Q32_SOURCE_FREEZE_CONDITIONAL_ADMISSION_v1.json'),
        'prelaunch': ref(P / 'Q32_FREEZE_PRELAUNCH_ACTUAL_v1.json'),
        'queue': {name: ref(Q / name) for name in ('request.json', 'result.json', 'events.json', 'supervisor.log', 'stdout.log', 'stderr.log', 'manifest.json')},
        'queue_original_audit': ref(audit_path), 'run_id': result['run_id'], 'elapsed_seconds': result['command_elapsed_seconds'],
        'native': 0, 'compile': 0, 'new_capture': 0, 'process_pause': 0, 'power_change': False,
        'performance_accepted': False, 'Q32_timing_admitted': False, 'F64_timing_admitted': False}
    path = P / 'Q32_FREEZE_ACTUAL_READY_v1.json'; write(path, index)
    print(json.dumps(ref(path)))

if __name__ == '__main__':
    main()
