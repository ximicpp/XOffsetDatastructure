"""Source-only independent review and before/after binding of finite correction."""
import argparse
import datetime
import hashlib
import importlib.util
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
D = ROOT / 'docs/coordination/c14-iteration-20261020-v2'
V = D / 'validation/combined_full'
Q = D / 'core/REQUESTS_COMBINED_EXTERNAL_v2.json'
SHA = '70f4e4a8a12b246fe5ae03294328e5aa1c86271ffe157f00102a519c80a9734f'
OUT = Path('/root/xoffset-c14-iteration/20261020/v2/validation')
LEASE = OUT / 'combined-external-corrective-v2-lease'


def need(ok, message):
    if not ok:
        raise ValueError(message)


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def fid(path):
    path = Path(path)
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for b in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(b)
    return dict(bytes=path.stat().st_size, sha256=h.hexdigest())


def ref(path):
    return dict(path=str(path), identity=fid(path))


def put(path, value):
    with path.open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2)
        stream.write('\n')


def check():
    need(fid(Q)['sha256'] == SHA, 'exact corrective request changed')
    oldpath = D / 'core/REQUESTS_COMBINED_EXTERNAL.json'
    old, q = read(oldpath), read(Q)
    need(q['source_members'] == old['source_members'] and q['candidate'] == old['candidate'], 'candidate identity changed')
    need(q['source'] == str(OUT / 'combined-full-v1-normal/source') and q['output'] == str(OUT / 'combined-external-corrective-v2'), 'new scope differs')
    need(q['task_timeout_seconds'] == 900 and q['compile_budget'] == q['native_budget'] == 4 and not q['timing_admitted'] and not q['screening_admitted'], 'finite scope changed')
    need(q['corrective_of'] == ref(oldpath) and q['failure_history']['failed_compile_attempts'] == 1 and q['failure_history']['successful_compiles'] == q['failure_history']['native_attempts'] == 0, 'failure history lost')
    need(len(q['commands']) == len(old['commands']) == 8, 'command count differs')
    def convert(value):
        return value.replace(old['source'], q['source']).replace(old['output'], q['output'])
    for previous, current in zip(old['commands'], q['commands']):
        expected = {**previous, 'argv': [convert(a) for a in previous['argv']]}
        need(current == expected, 'changes beyond exact source/output replacement: ' + current['label'])
        need(current['timeout_seconds'] == (120 if current['kind'] == 'compile' else 20), 'child cap changed')
        for a in current['argv']:
            if a.startswith('-I'):
                p = Path(a[2:])
                need(not (str(p).startswith('/mnt/c/') and (p / 'VERSION').exists()), 'mounted project VERSION still shadows libc++')
    for p, value in old['input_files'].items():
        mapped = convert(p)
        need(mapped in q['input_files'] and q['input_files'][mapped] == value, 'old input omitted or changed: ' + p)
    inputs = {str(Q): fid(Q), str(Path(__file__).resolve()): fid(__file__)}
    for p, expected in q['input_files'].items():
        got = fid(p)
        need(got['sha256'] == expected['sha256'] and ('bytes' not in expected or got['bytes'] == expected['bytes']), 'changed exact input: ' + p)
        inputs[p] = got
    initialguard = Path(__file__).with_name('combined_initial_guard.py')
    need(fid(initialguard)['sha256'] == 'f298c76860682e3a66dbbae72824ed450950efdaa2384e2824173b366dcee84c', 'immutable initial guard changed')
    spec = importlib.util.spec_from_file_location('initial_readonly', initialguard)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    actual = module.check()
    before = read(V / 'INITIAL_EXECUTION_BINDING.json')
    need(all(actual[k] == before[k] for k in ('source154', 'production14', 'inputs', 'provider')), 'initial closure changed')
    for name, expected in actual['source154'].items():
        need(fid(Path(q['source']) / name) == expected, 'Linux snapshot differs: ' + name)
    need(not (Path(q['source']) / 'version').exists(), 'Linux snapshot is case insensitive')
    for p in (initialguard, V / 'INITIAL_EXECUTION_BINDING.json', V / 'INITIAL_NORMAL_VERIFIED.json', V / 'INITIAL_SANITIZED_VERIFIED.json', V / 'EXTERNAL_CORRECTION_SNAPSHOT_PROOF.json'):
        inputs[str(p)] = fid(p)
    inputs.update(actual['inputs'])
    for mode in ('normal', 'sanitized'):
        need(read(V / ('INITIAL_' + mode.upper() + '_VERIFIED.json'))['status'] == 'passed', 'own full not passed')
    command = ['/usr/bin/python3', '-I', '-B', str(ROOT / 'tools/c14_iteration_20261020_v2/core/run_native_targets.py'), '--request', str(Q)]
    need(command == q['worker_argv'], 'worker argv differs')
    return dict(inputs=inputs, source154=actual['source154'], production14=actual['production14'], provider=actual['provider'],
                request=ref(Q), command=command, original_failed_history=q['failure_history'])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('stage', choices=('before', 'after'))
    parser.add_argument('--windows-observation', type=Path)
    args = parser.parse_args()
    current = check()
    if args.stage == 'before':
        need(not Path(read(Q)['output']).exists() and not LEASE.exists(), 'preserve prior output; no repeat')
        need(args.windows_observation is not None, 'fresh Windows observation required')
        host = read(args.windows_observation)
        age = (datetime.datetime.now(datetime.timezone.utc) - datetime.datetime.fromisoformat(host['observed_utc'].replace('Z', '+00:00'))).total_seconds()
        need(0 <= age <= 900 and host['backing_free_bytes'] >= 8 * 1024**3 and min(shutil.disk_usage('/root').free, shutil.disk_usage('/mnt/c').free) >= 8 * 1024**3, 'fresh guest/backing8GiB required')
        current['inputs'][str(args.windows_observation)] = fid(args.windows_observation)
        put(V / 'EXTERNAL_V2_BEFORE.json', dict(status='passed_independent_static_review_inputs_bound', **current, windows_observation=ref(args.windows_observation), compile=0, native=0, queue=0))
        print('Corrective external v2 exact static review passed; inputs bound;0 execution')
        return
    previous = read(V / 'EXTERNAL_V2_BEFORE.json')
    need(all(fid(p) == expected for p, expected in previous['inputs'].items()), 'bound input changed across correction')
    q = read(Q)
    receiptpath = Path(q['output']) / 'receipt.json'
    receipt = read(receiptpath)
    need(receipt['status'] == 'passed' and receipt['request']['sha256'] == SHA and len(receipt['commands']) == 8, 'corrective actual evidence incomplete')
    for expected, recorded in zip(q['commands'], receipt['commands']):
        need(all(recorded[k] == expected[k] for k in ('label', 'kind', 'argv')) and recorded['returncode'] == 0 and recorded['elapsed_seconds'] <= expected['timeout_seconds'], 'actual command failed/differs')
    proof = module_queue(current['command'])
    put(V / 'EXTERNAL_V2_VERIFIED.json', dict(status='passed', stage='external_corrective_v2', request=ref(Q), before=ref(V / 'EXTERNAL_V2_BEFORE.json'), actual_receipt=ref(receiptpath), queue=proof,
        source154_before_after_unchanged=True, compiler_runtime_provider_before_after_unchanged=True, production14=current['production14'],
        counts=dict(corrective_compiles=4, corrective_native=4, previous_failed_compiles=1, previous_native=0),
        initial_normal73_SAN73_preserved=True, screen_admitted=False, QF_admitted=False))
    print('Corrective external v2 actual4compile4native passed; original failed1compile/0native preserved')


def module_queue(command):
    spec = importlib.util.spec_from_file_location('queue_readonly', Path(__file__).with_name('combined_initial_guard.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.queue(LEASE, command, 900)


if __name__ == '__main__':
    main()
