"""Metadata-only request authoring; no project imports or process launch."""
import ast
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
D = ROOT / 'docs/coordination/c14-iteration-20261020-v2'
V = D / 'validation/final_formal_v1_preparation'
TOOLS = Path(__file__).parent
BOUND = TOOLS / 'final_formal_bound_v1'
BASE = Path('/root/xoffset-receive-completion/20261019/v1')
COMBO = Path('/root/xoffset-c14-iteration/20261020/v2/validation/combined-business-v1')
OUT = Path('/root/xoffset-c14-iteration/20261020/v2/validation/final-six-role-resource-v1')
RUNNER = Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py')
ROLES = ('old_xoffset', 'new_xoffset', 'cista', 'flatbuffers', 'capnproto', 'cereal')


def read(p):
    return json.loads(Path(p).read_text(encoding='utf-8-sig'))


def fid(p):
    data = Path(p).read_bytes()
    return dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def ref(p):
    return dict(path=str(p), identity=fid(p))


def put(p, value):
    with p.open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2)
        stream.write('\n')


def main():
    assert not OUT.exists()
    mainpath = BOUND / 'six-role-campaign.py'
    assert fid(mainpath)['sha256'] == '6c3319cb3502ce380d3ddd022be33de9da79483ea7ecbd911695fc215b4964c8'
    tree = ast.parse(mainpath.read_text(encoding='utf-8'))
    stage = ast.literal_eval(next(n.value for n in tree.body if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'STAGE_BINDINGS' for t in n.targets)))
    captures = read(V / 'CAPTURE_REUSE_INDEX_DRAFT.json')['captures']
    assert stage['fixed_input_captures'] == captures
    oldpath, newpath = BASE / 'profile-normal-v2.json', COMBO / 'profile-normal-v2.json'
    old, new = read(oldpath), read(newpath)
    assert old['environment'] == new['environment'] and old['platform_class'] == new['platform_class'] == 'wsl2-vm-x86_64'
    acceptance = D / 'reviews/COMBINED_FULL_ACTUAL_ACCEPTANCE.json'
    assert fid(acceptance)['sha256'] == '6b3a0227d1e258181f4824c0862fbb609cc8ea32a2a549c17c40e367f4c4d27c'
    assert fid(RUNNER)['sha256'] == '6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909'
    inputs = {}
    paths = [*sorted(p for p in BOUND.rglob('*') if p.is_file()), Path(__file__).resolve(),
             TOOLS / 'final_six_role_resource_worker_v1.py', RUNNER, acceptance,
             D / 'reviews/FINAL_FORMAL_SOURCE_ACCEPTANCE.json',
             V / 'FINAL_FORMAL_BOUND_SOURCE_READY.json', V / 'FINAL_FORMAL_CAPTURE_BINDING.patch',
             V / 'CAPTURE_REUSE_INDEX_DRAFT.json', V / 'RECORD_REUSE_AUDIT.json',
             V / 'FINITE_RESOURCE_REQUEST_DRAFT.json', oldpath, newpath,
             D / 'validation/combined_full/COMBINED_FUNCTIONAL_GATES_v2.json',
             D / 'validation/combined_full/business_guard_v2/FINAL_AFTER.json']
    runtime_records = {}
    for label, root in (('baseline', BASE), ('candidate', COMBO)):
        before = root / 'functional-normal-v2/runtime-before.json'
        after = root / 'functional-normal-v2/runtime-after.json'
        assert read(before) == read(after), label
        paths += [before, after]
        runtime_records[label] = str(before)
    for p in paths:
        if p.suffix == '.py':
            ast.parse(p.read_text(encoding='utf-8'))
        inputs[str(p)] = fid(p)
    assert len(list(BOUND.rglob('*.py'))) == 10
    jobs = []
    sizes = {}
    for role in ROLES:
        entry = (old if role == 'old_xoffset' else new)['binaries']['xoffset' if role in ('old_xoffset', 'new_xoffset') else role]
        binary = captures['normal']['roles'][role]['binary']
        assert entry['path'] == binary['path'] and fid(entry['path']) == {'bytes': int(binary['identity']['bytes']), 'sha256': binary['identity']['sha256']}
        inputs[entry['path']] = fid(entry['path'])
        sizes[role] = int(captures['normal']['roles'][role]['files']['.wire']['bytes'])
        for kind in ('conditioning', 'primary'):
            label = 'resource-%02d-%s-%s' % (len(jobs), role, kind)
            prefix = OUT / 'commands' / label
            argv = [*entry['prefix'], str(OUT / 'frozen/bin' / role), 'run',
                    '--wire', str(OUT / 'frozen/input-wires' / (role + '.wire')),
                    '--trace', str(OUT / 'frozen/fixture/trace.bin'), '--wire-out', str(prefix.with_suffix('.wire')),
                    '--canonical-out', str(prefix.with_suffix('.canonical')), '--queries-out', str(prefix.with_suffix('.queries')),
                    '--result', str(prefix.with_suffix('.native.json'))]
            jobs.append(dict(id=label, role=role, execution_kind=kind, timeout_seconds=20, argv=argv))
    request_path = V / 'RESOURCE_EXECUTION_REQUEST_v1.json'
    admission_path = D / 'reviews/FINAL_SIX_ROLE_RESOURCE_EXECUTION_ADMISSION.json'
    worker_argv = ['/usr/bin/python3', '-I', '-B', str(TOOLS / 'final_six_role_resource_worker_v1.py'), '--request', str(request_path)]
    fixture = Path(captures['normal']['roles']['new_xoffset']['prefix']).parent.parent / 'prepared/M'
    draft = read(V / 'FINITE_RESOURCE_REQUEST_DRAFT.json')
    request = dict(kind='final-six-role-fixed-resource-request-v1', status='pending_independent_exact_resource_admission',
        candidate='r1-m2-combined-v1', bound_campaign=str(mainpath), accepted_full_status=read(acceptance)['status'],
        baseline_profile=str(oldpath), candidate_profile=str(newpath), baseline_runtime_record=runtime_records['baseline'],
        candidate_runtime_record=runtime_records['candidate'], fixture=str(fixture), output=str(OUT), queue=str(OUT / 'queue'),
        inputs=inputs, jobs=jobs, worker_argv=worker_argv,
        native_budget=12, child_seconds=20, outer_seconds=600, native_cap_sum_seconds=240,
        conservative_nonchild_allowance_seconds=300, reserved_seal_seconds=30,
        new_capture=0, new_compile=0, retry=0, inference_samples=0, warmup=0,
        original_lease_owner='W3', original_lease_kind='functional', original_lease_file=str(RUNNER.parents[2] / 'build/vm-coordination/queue.lock'),
        preparation_base_reserve_bytes=512 * 1024**2, minimum_guest_and_backing_free_bytes=8 * 1024**3,
        post_verified_body_pooling='Original Ledger, all three body types, after actual complete native truth check and exit. Generated/logical totals remain fully charged. Fresh independent outputs required.',
        envelope='Same normal six-role binaries/profile/environment/affinity, original G.command/G.execute and native_result, original file ceilings and qualification Ledger policy. One immediate C/P pair per role in original role order; no formal balanced schedule or inference. No host pause or power policy change.',
        admission_contract={'kind': 'final-six-role-resource-execution-admission', 'status': 'passed',
            'request_sha256': 'EXACT_REQUEST_SHA_REQUIRED', 'bound_source_acceptance': {'path': 'ROOT_ACTUAL_BOUND_CAPTURE_ACCEPTANCE', 'identity': 'EXACT_ID_REQUIRED'},
            'fresh_host_observation': {'path': 'FRESH_WINDOWS_OBSERVATION', 'identity': 'EXACT_ID_REQUIRED'},
            'host_observation_fields': ['observed_utc', 'backing_free_bytes']},
        projection=draft['projection_fields'],
        actual_maxima_pending=True, QF_admitted=False, performance_accepted=False,
        output_contract=['profile.json', 'request.json', 'resource-request.json', 'admission.json', 'runtime-before.json', 'runtime-after.json',
            'protocol/exact-original11', 'frozen/fixture', 'frozen/input-captures.json', 'frozen/input-capture-metadata', 'frozen/input-wires', 'frozen/bin',
            'commands/12freshoriginalruns×7slots', 'verified-body-pool', 'resource-ledger.json', 'result.json', 'status.json', 'manifest.json', 'queue/original-full-receipts'],
        projection_limits='Observed maxC/maxP complete process intervals and execute+truth+pool wall both reported. 388*S+300/772*S+300 are necessary estimates, not future tail guarantees. Resource run does not approve Q/F. Source/capture staging, actual retained output, reserve/7slots and fresh free-space checks charged; prior archives already consume observed free space and are never deleted.')
    put(request_path, request)
    command = worker_argv + ['--request-sha256', fid(request_path)['sha256'], '--admission', str(admission_path)]
    argv = ['/usr/bin/python3', '-I', '-B', str(RUNNER), 'run', '--owner', 'W3', '--kind', 'functional', '--out', str(OUT / 'queue'),
            '--timeout-seconds', '600', '--wait-seconds', '0', '--', *command]
    ready = V / 'RESOURCE_REQUEST_SOURCE_READY_v1.json'
    put(ready, dict(status='concrete_fixed12_source_request_ready_not_executed', request=ref(request_path), worker=ref(TOOLS / 'final_six_role_resource_worker_v1.py'),
        argv=argv, exact_admission_path=str(admission_path), compile=0, native=0, queue=0, capture=0, QF_admitted=False,
        budget={'children': 12, 'child_seconds': 20, 'outer_seconds': 600, 'nonchild_projection_seconds': 300, 'seal_seconds': 30},
        note='All action argv concrete; no invented original CLI flags. Run only after independent source/budget review and exact root admission including fresh host and bound-source acceptance refs.'))
    print(json.dumps(dict(ready=ref(ready), request=ref(request_path), worker=ref(TOOLS / 'final_six_role_resource_worker_v1.py'))))


if __name__ == '__main__':
    main()
