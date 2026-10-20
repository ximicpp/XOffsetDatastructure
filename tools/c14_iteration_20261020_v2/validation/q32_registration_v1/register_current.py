"""One fixed metadata registration for the accepted C14 R1+M2 executor.

No project imports, native execution, compilation, body audit, process pause or
power switch. Original frozen policy objects and helper receipts are reused.
The root must independently accept the explicit eleven-gate composition first.
"""
from pathlib import Path
import argparse, ast, datetime, hashlib, json, shutil, stat

R = Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
D = R / 'docs/coordination/c14-iteration-20261020-v2'
V = D / 'validation'
COORD = V / 'final_formal_v1'
HERE = Path(__file__).resolve().parent
BOUND = HERE.parent / 'final_formal_bound_v1'
OLD = Path('/root/xoffset-four-paths/20261018/v1/frozen-qualification-v1')
PREP = V / 'q32_freeze_preparation'
NAMES = ('codegen', 'mechanism', 'targeted', 'full_normal', 'full_san',
         'business_SML', 'v1_bidirectional', 'consumers', 'lifecycle', 'safety', 'engineering')
REG = ('measurement-decision-v1.json', 'measurement-decision-v2.json',
       'backing-volume-windows-v1.json', 'backing-volume-verification-v1.json',
       'power-capability-v2.json', 'power-capability-v3.json', 'resources-pure-tests-v3.json')

def need(ok, message):
    if not ok:
        raise ValueError(message)

def data(path):
    path = Path(path)
    info = path.lstat()
    need(stat.S_ISREG(info.st_mode) and info.st_size <= 2 * 1024**2,
         'only small regular metadata/source is permitted: ' + str(path))
    value = path.read_bytes()
    after = path.lstat()
    fields = ('st_dev', 'st_ino', 'st_mode', 'st_size', 'st_mtime_ns', 'st_ctime_ns')
    need(all(getattr(info, key) == getattr(after, key) for key in fields), 'metadata/source changed while reading')
    return value

def fid(path):
    value = data(path)
    return {'bytes': str(len(value)), 'sha256': hashlib.sha256(value).hexdigest()}

def norm(value):
    return {'bytes': str(value['bytes']), 'sha256': value['sha256']}

def read(path):
    return json.loads(data(path))

def ref(path):
    return {'path': str(path), 'identity': fid(path)}

def check(item):
    need(fid(item['path']) == norm(item['identity']), 'pinned reference changed: ' + item['path'])
    return read(item['path'])

def write(path, value):
    with Path(path).open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write('\n')

def fresh(record, key='captured_at'):
    moment = datetime.datetime.fromisoformat(record[key].replace('Z', '+00:00'))
    age = (datetime.datetime.now(datetime.timezone.utc) - moment).total_seconds()
    need(0 <= age <= 600, 'capacity observation must be current within 600 seconds')

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capacity', required=True, type=Path)
    p.add_argument('--gate-acceptance', required=True, type=Path)
    p.add_argument('--gate-acceptance-sha256', required=True)
    a = p.parse_args()
    need(a.capacity.is_absolute() and a.gate_acceptance.is_absolute(), 'absolute POSIX inputs required')
    plan = read(PREP / 'REGISTRATION_ADAPTATION_PLAN_v1.json')
    inputs = plan['actual_inputs']
    for item in inputs.values():
        check(item)
    full = check(inputs['own_full_closure'])
    own_acceptance = check(inputs['own_full_acceptance'])
    need(full['status'] == 'passed' and own_acceptance['combined_full_functional_gates_passed'] is True,
         'actual own full closure and independent acceptance required')
    source_acceptance = check(inputs['source_acceptance'])
    need(source_acceptance['status'].startswith('passed'), 'accepted original authoring required')
    need(fid(BOUND / 'six-role-campaign.py')['sha256'] ==
         '6c3319cb3502ce380d3ddd022be33de9da79483ea7ecbd911695fc215b4964c8', 'accepted bound source changed')
    literals = {}
    for node in ast.parse(data(BOUND / 'six-role-campaign.py')).body:
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'STAGE_BINDINGS' for t in node.targets):
            literals['binding'] = ast.literal_eval(node.value)
    binding = literals['binding']
    need(binding['fixed_input_captures'] == check(inputs['capture_index'])['captures'], 'accepted capture binding differs')
    bridge_path = PREP / 'COMBINED_TYPED_GATE_COMPOSITION_REQUEST_v1.json'
    bridge = read(bridge_path)
    need(set(bridge['gates']) == set(NAMES), 'exact eleven proposed gates required')
    need(fid(a.gate_acceptance)['sha256'] == a.gate_acceptance_sha256, 'root acceptance pin differs')
    accepted = read(a.gate_acceptance)
    need(accepted['status'].startswith('passed') and accepted['composition_accepted'] is True
         and accepted['candidate_identity'] == inputs['candidate_identity']
         and accepted['composition_request'] == ref(bridge_path)
         and set(accepted['accepted_gate_names']) == set(NAMES), 'independent current gate composition not accepted')
    for record in bridge['gates'].values():
        for item in record['evidence']:
            need(fid(item['path']) == norm(item['identity']), 'gate evidence reference changed')
    pins = read(PREP / 'ORIGINAL_POLICY_AND_CAPABILITY_PINS_v1.json')
    for item in pins['files'].values():
        need(fid(item['path']) == norm(item['identity']), 'original registration source changed')
    old_proposal = read(OLD / 'proposal.json')
    old_decision = read(OLD / 'registration/measurement-decision-v2.json')
    cap2 = OLD / 'registration/power-capability-v2.json'
    cap3 = OLD / 'registration/power-capability-v3.json'
    resource = OLD / 'registration/resources-pure-tests-v3.json'
    c, rr = read(cap3), read(resource)
    need(c['status'] == rr['status'] == 'passed' and c['native_benchmark_executions'] == rr['native_children'] == 0,
         'original capability receipts failed')
    need(norm(c['previous_capability']) == fid(cap2), 'original power capability predecessor changed')
    power_probe = Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/docs/coordination/performance-v5-20261018/power-probe-v3')
    for name in ('ready', 'completed'):
        need(norm(c[name]) == fid(power_probe / (name + '.json')), 'historical power probe receipt changed')
    need(read(power_probe / 'completed.json')['restored'] is True, 'historical power capability restore not verified')
    for key, name in (('module', 'power_environment.py'), ('helper', 'power-control.ps1')):
        need(norm(c[key]) == fid(BOUND / name), 'power helper identity differs')
    need(norm(rr['module']) == fid(BOUND / 'runtime_resources.py') and rr['checks'] == ['normal', 'optimized']
         and rr['test_cases_each_mode'] == 26, 'pure resource receipt differs')
    windows = a.capacity / 'windows.json'
    verification = a.capacity / 'verification.json'
    host, observation = read(windows), read(verification)
    request, receipt = read(a.capacity / 'request.json'), read(a.capacity / 'receipt.json')
    for record in (host, observation):
        fresh(record)
    fresh(request, 'created_at'); fresh(receipt, 'finished_at')
    helper, observer = HERE / 'observe-backing.ps1', HERE / 'observe-capacity.py'
    need(observation['status'] == 'passed' and receipt['returncode'] == 0
         and norm(observation['observer']) == fid(observer)
         and norm(observation['windows_helper']) == norm(request['helper']) == fid(helper)
         and host['helper_sha256'] == fid(helper)['sha256']
         and host['distribution'] == 'Ubuntu-22.04' and host['windows_backing_root'] == 'C:\\'
         and host['linux_backing_probe'] == '/mnt/c' and host['read_only_inspection'] is True
         and host['no_reparse_points_to_backing_root'] is True
         and norm(observation['windows_snapshot']) == fid(windows), 'fresh capacity provenance failed')
    capacities = (observation['guest_free_bytes'], observation['backing_free_bytes'], host['volume_free_bytes'])
    need(all(type(n) is int and n >= 10 * 1024**3 for n in capacities)
         and abs(capacities[1] - capacities[2]) <= 1024**3, 'dual-volume capacity insufficient')
    proposal_path = BOUND / 'protocol-proposal-v1.json'
    need(not COORD.exists() and not proposal_path.exists(), 'registration requires fresh outputs; no overwrite/retry')
    COORD.mkdir()
    for source, name in ((windows, REG[2]), (verification, REG[3]), (cap2, REG[4]), (cap3, REG[5])):
        shutil.copyfile(source, COORD / name)
    # The seventh source remains the exact accepted immutable resource receipt.
    decision = {k: old_decision[k] for k in ('schema_version', 'common', 'qualification', 'formal')}
    decision.update(backing_verification=fid(COORD / REG[3]), power_capability=fid(COORD / REG[5]), resources_pure_tests=fid(resource))
    write(COORD / REG[0], decision)
    write(COORD / REG[1], {**decision, 'supersedes_v1': fid(COORD / REG[0]),
         'policy_and_budgets_unchanged': True,
         'revision_reason': 'Same policy; explicit reused helper-capability chain with fresh capacity observation'})
    gate_refs = {}
    (COORD / 'gates').mkdir()
    for name in NAMES:
        receipt = {**bridge['gates'][name], 'gate': name, 'status': 'passed',
                   'candidate_identity': inputs['candidate_identity'],
                   'independent_review': {'status': 'passed', 'record': ref(a.gate_acceptance)}}
        path = COORD / 'gates' / (name + '.json')
        write(path, receipt); gate_refs[name] = ref(path)
    gate_map = COORD / 'gate-map-v1.json'
    write(gate_map, {'candidate_identity': inputs['candidate_identity'], 'gates': gate_refs,
                    'composition_request': ref(bridge_path), 'independent_acceptance': ref(a.gate_acceptance)})
    proposal = {k: old_proposal[k] for k in ('kind', 'statistics_version', 'campaigns')}
    proposal.update(coordinator_references={name: fid(resource if name == REG[6] else COORD / name) for name in REG},
        actual_stage_bindings=binding, gate_map=fid(gate_map), root_attempt_ledger_required=True,
        new_formal_authorization_created_here=False)
    write(proposal_path, proposal)
    prerequisite = {'status': 'passed', 'scope': bridge['scope'], 'candidate_identity': inputs['candidate_identity'],
        'evidence': gate_refs, 'own_complete_functional_acceptance': inputs['own_full_acceptance'],
        'component_mechanisms_are_not_combination_measurements': True,
        'capture_reuse_acceptance': inputs['bound_acceptance'], 'resource_actual_acceptance': bridge['resource_actual_acceptance'],
        'capacity': ref(verification), 'proposal': ref(proposal_path),
        'root_prelaunch_and_external_attempt_ledger_required': True, 'formal_qualified_here': False,
        'QF_timing_admitted': False}
    write(COORD / 'qualification-prerequisites.json', prerequisite)
    write(COORD / 'combined-gates.json', prerequisite)
    write(COORD / 'formal-binding-v1.json', {'status': 'bound-not-launched', 'binding': binding,
        'bound_executor': ref(BOUND / 'six-role-campaign.py'), 'proposal': ref(proposal_path),
        'gate_map': ref(gate_map), 'generator': ref(Path(__file__)), 'native_children': 0, 'queues': 0,
        'source_delta': 'No accepted source edited; only new adjacent proposal and new coordinator metadata.',
        'original_policy_source': pins['files']['proposal'], 'policy_and_budgets_unchanged': True})
    outputs = {path.relative_to(COORD).as_posix(): fid(path) for path in sorted(COORD.rglob('*')) if path.is_file()}
    write(PREP / 'REGISTRATION_ACTUAL_READY_v1.json', {'status': 'registered-not-frozen-not-launched',
        'outputs': outputs, 'proposal': ref(proposal_path), 'gate_acceptance': ref(a.gate_acceptance),
        'native_children': 0, 'compile': 0, 'capture': 0, 'body_audit': 0, 'power_changed': False})
    print(json.dumps({'status': 'registered-not-frozen-not-launched', 'outputs': len(outputs), 'proposal': ref(proposal_path)}))

if __name__ == '__main__':
    main()
