"""Small actual registration closure check; no project import or body reread."""
from pathlib import Path
import ast, datetime, hashlib, json, subprocess

R = Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
D = R / 'docs/coordination/c14-iteration-20261020-v2'
V = D / 'validation'
PREP = V / 'q32_freeze_preparation'
COORD = V / 'final_formal_v1'
BOUND = R / 'tools/c14_iteration_20261020_v2/validation/final_formal_bound_v1'
QUEUE = Path('/root/xoffset-c14-iteration/20261020/v2/validation/final-formal-v1/leases/registration-prepare-v1')
RUNNER = Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py')
OLD = Path('/root/xoffset-four-paths/20261018/v1/frozen-qualification-v1')

def read(p):
    p = Path(p); assert p.is_file() and not p.is_symlink() and p.stat().st_size < 2 * 1024**2
    return json.loads(p.read_bytes())

def fid(p):
    p = Path(p); assert p.is_file() and not p.is_symlink() and p.stat().st_size < 2 * 1024**2
    raw = p.read_bytes(); return {'bytes': str(len(raw)), 'sha256': hashlib.sha256(raw).hexdigest()}

def norm(v):
    return {'bytes': str(v['bytes']), 'sha256': v['sha256']}

def ref(p):
    return {'path': str(p), 'identity': fid(p)}

def write(p, v):
    with p.open('x', encoding='utf-8', newline='\n') as s:
        json.dump(v, s, indent=2, sort_keys=True); s.write('\n')

def main():
    actual_path = PREP / 'REGISTRATION_ACTUAL_READY_v1.json'
    actual = read(actual_path)
    assert actual['status'] == 'registered-not-frozen-not-launched'
    assert {p.relative_to(COORD).as_posix() for p in COORD.rglob('*') if p.is_file()} == set(actual['outputs'])
    for name, identity in actual['outputs'].items():
        assert fid(COORD / name) == norm(identity)
    proposal_path = BOUND / 'protocol-proposal-v1.json'
    proposal = read(proposal_path); old_proposal = read(OLD / 'proposal.json')
    assert fid(proposal_path) == norm(actual['proposal']['identity'])
    assert all(proposal[k] == old_proposal[k] for k in ('kind', 'statistics_version', 'campaigns'))
    inventory = read(V / 'final_formal_v1_preparation/FINAL_FORMAL_BOUND_SOURCE_READY.json')['inventory']
    assert len(inventory) == 11
    for entry in inventory.values():
        assert fid(entry['bound']['path']) == norm(entry['bound']['identity'])
    binding = next(ast.literal_eval(n.value) for n in ast.parse((BOUND / 'six-role-campaign.py').read_bytes()).body
                   if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'STAGE_BINDINGS' for t in n.targets))
    assert proposal['actual_stage_bindings'] == binding
    assert set(proposal['coordinator_references']) == {'measurement-decision-v1.json', 'measurement-decision-v2.json',
        'backing-volume-windows-v1.json', 'backing-volume-verification-v1.json', 'power-capability-v2.json',
        'power-capability-v3.json', 'resources-pure-tests-v3.json'}
    registration = {}
    for name, identity in proposal['coordinator_references'].items():
        path = OLD / 'registration' / name if name == 'resources-pure-tests-v3.json' else COORD / name
        assert fid(path) == norm(identity); registration[name] = ref(path)
    d1, d2 = read(COORD / 'measurement-decision-v1.json'), read(COORD / 'measurement-decision-v2.json')
    old_decision = read(OLD / 'registration/measurement-decision-v2.json')
    assert all(d1[k] == d2[k] == old_decision[k] for k in ('schema_version', 'common', 'qualification', 'formal'))
    assert norm(d2['supersedes_v1']) == fid(COORD / 'measurement-decision-v1.json')
    assert d2['policy_and_budgets_unchanged'] is True
    for key, name in (('backing_verification', 'backing-volume-verification-v1.json'),
                      ('power_capability', 'power-capability-v3.json'), ('resources_pure_tests', 'resources-pure-tests-v3.json')):
        assert norm(d2[key]) == registration[name]['identity']
    cap = read(COORD / 'power-capability-v3.json')
    assert norm(cap['previous_capability']) == fid(COORD / 'power-capability-v2.json')
    assert cap['status'] == 'passed' and cap['native_benchmark_executions'] == 0
    assert norm(cap['module']) == fid(BOUND / 'power_environment.py') and norm(cap['helper']) == fid(BOUND / 'power-control.ps1')
    resource = read(OLD / 'registration/resources-pure-tests-v3.json')
    assert resource['status'] == 'passed' and resource['native_children'] == 0 and resource['test_cases_each_mode'] == 26
    assert resource['checks'] == ['normal', 'optimized'] and norm(resource['module']) == fid(BOUND / 'runtime_resources.py')
    windows = read(COORD / 'backing-volume-windows-v1.json'); capacity = read(COORD / 'backing-volume-verification-v1.json')
    assert capacity['status'] == 'passed' and norm(capacity['windows_snapshot']) == fid(COORD / 'backing-volume-windows-v1.json')
    assert min(capacity['guest_free_bytes'], capacity['backing_free_bytes'], windows['volume_free_bytes']) >= 10 * 1024**3
    assert abs(capacity['backing_free_bytes'] - windows['volume_free_bytes']) <= 1024**3
    age = (datetime.datetime.now(datetime.timezone.utc) - datetime.datetime.fromisoformat(capacity['captured_at'])).total_seconds()
    assert 0 <= age <= 600
    gates = read(COORD / 'gate-map-v1.json')
    root_acceptance = read(actual['gate_acceptance']['path'])
    assert fid(actual['gate_acceptance']['path']) == norm(actual['gate_acceptance']['identity'])
    assert set(gates['gates']) == set(root_acceptance['accepted_gate_names']) and len(gates['gates']) == 11
    for name, item in gates['gates'].items():
        assert fid(item['path']) == norm(item['identity'])
        gate = read(item['path'])
        assert gate['gate'] == name and gate['status'] == gate['independent_review']['status'] == 'passed'
        assert gate['candidate_identity'] == root_acceptance['candidate_identity']
        assert gate['independent_review']['record'] == actual['gate_acceptance']
    assert fid(COORD / 'qualification-prerequisites.json') == fid(COORD / 'combined-gates.json')
    assert norm(proposal['gate_map']) == fid(COORD / 'gate-map-v1.json')
    request, result = read(QUEUE / 'request.json'), read(QUEUE / 'result.json')
    admission = read(D / 'reviews/Q32_REGISTRATION_EXECUTION_ADMISSION_v1.json')
    assert request['command'] == admission['argv'][admission['argv'].index('--') + 1:]
    assert request['owner'] == result['owner'] == 'W1' and request['kind'] == result['kind'] == 'validation'
    assert request['timeout_seconds'] == 600 and request['wait_seconds'] == 0
    assert result['status'] == 'complete' and result['command_returncode'] == 0
    assert result['cleanup'] == {'live': [], 'process_groups': [], 'zombies': []}
    audit_argv = ['/usr/bin/python3', '-I', '-B', str(RUNNER), 'audit', '--out', str(QUEUE)]
    audit = subprocess.run(audit_argv, capture_output=True, text=True, timeout=30)
    audit_path = PREP / 'REGISTRATION_ORIGINAL_QUEUE_AUDIT_v1.json'
    write(audit_path, {'argv': audit_argv, 'returncode': audit.returncode, 'stdout': audit.stdout, 'stderr': audit.stderr})
    assert audit.returncode == 0
    index = {'status': 'actual_registration_verified_not_frozen_not_timing', 'actual_ready': ref(actual_path),
        'proposal': ref(proposal_path), 'registration7': registration,
        'coordinator_output_count': len(actual['outputs']), 'all_current_output_ids_match': True,
        'exact_original_policy_common_QF_unchanged': True, 'bound_source11_unchanged': True,
        'actual11_gates_match_independent_scoped_acceptance': True,
        'root_gate_acceptance': actual['gate_acceptance'], 'capacity': ref(COORD / 'backing-volume-verification-v1.json'),
        'capacity_age_seconds_at_check': age, 'guest_free_bytes': capacity['guest_free_bytes'],
        'backing_free_bytes': capacity['backing_free_bytes'], 'windows_free_bytes': windows['volume_free_bytes'],
        'queue': {name: ref(QUEUE / name) for name in ('request.json', 'result.json', 'events.json', 'supervisor.log', 'stdout.log', 'stderr.log', 'manifest.json')},
        'queue_original_audit': ref(audit_path), 'run_id': result['run_id'], 'elapsed_seconds': result['command_elapsed_seconds'],
        'native': 0, 'compile': 0, 'capture': 0, 'body_audit': 0, 'pause': 0, 'power_change': False,
        'Q32_timing_admitted': False, 'F64_admitted': False}
    path = PREP / 'REGISTRATION_ACTUAL_VERIFIED_v1.json'; write(path, index)
    print(json.dumps(ref(path)))

if __name__ == '__main__':
    main()
