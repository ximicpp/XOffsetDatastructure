"""One fixed twelve-process resource observation; original execution/truth/ledger.

No compilation, new input capture, Q/F run, inference, retry or general scheduler.
This source must be independently admitted with its exact request before use.
"""
import argparse
import datetime
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import time

ROOT = Path(__file__).resolve().parents[3]
D = ROOT / 'docs/coordination/c14-iteration-20261020-v2'
OUT = Path('/root/xoffset-c14-iteration/20261020/v2/validation/final-six-role-resource-v1')
RUNNER = Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py')
ROLES = ('old_xoffset', 'new_xoffset', 'cista', 'flatbuffers', 'capnproto', 'cereal')


def need(ok, message):
    if not ok:
        raise ValueError(message)


def read(p):
    return json.loads(Path(p).read_text(encoding='utf-8-sig'))


def fid(p):
    p = Path(p)
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for b in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(b)
    return dict(bytes=p.stat().st_size, sha256=h.hexdigest())


def put(p, value):
    p.parent.mkdir(parents=True, exist_ok=True)
    with p.open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2)
        stream.write('\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--request', required=True, type=Path)
    parser.add_argument('--request-sha256', required=True)
    parser.add_argument('--admission', required=True, type=Path)
    args = parser.parse_args()
    began = time.monotonic()
    need(fid(args.request)['sha256'] == args.request_sha256, 'exact request differs')
    q, admission = read(args.request), read(args.admission)
    need(q['kind'] == 'final-six-role-fixed-resource-request-v1' and q['native_budget'] == 12
         and q['child_seconds'] == 20 and q['outer_seconds'] == 600
         and q['new_capture'] == q['new_compile'] == q['retry'] == 0, 'fixed resource budget differs')
    need(admission['kind'] == 'final-six-role-resource-execution-admission' and admission['status'] == 'passed'
         and admission['request_sha256'] == args.request_sha256, 'independent exact resource admission absent')
    bound_acceptance = admission['bound_source_acceptance']
    need(fid(bound_acceptance['path']) == bound_acceptance['identity'] and read(bound_acceptance['path'])['status'].startswith('passed'), 'independent bound-source/capture review absent')
    need(len(q['jobs']) == 12 and [(j['role'], j['execution_kind']) for j in q['jobs']] ==
         [(role, kind) for role in ROLES for kind in ('conditioning', 'primary')], 'fixed twelve-process order differs')
    for p, expected in q['inputs'].items():
        need(fid(p) == expected, 'bound source/tool/profile/proof changed: ' + p)
    need(read(D / 'reviews/COMBINED_FULL_ACTUAL_ACCEPTANCE.json')['status'] == q['accepted_full_status'], 'combined root acceptance absent')
    hostpath = Path(admission['fresh_host_observation']['path'])
    need(fid(hostpath) == admission['fresh_host_observation']['identity'], 'fresh host observation changed')
    host = read(hostpath)
    age = (datetime.datetime.now(datetime.timezone.utc) - datetime.datetime.fromisoformat(host['observed_utc'].replace('Z', '+00:00'))).total_seconds()
    need(0 <= age <= 900 and host['backing_free_bytes'] >= 8 * 1024**3, 'fresh Windows8GiB observation required')
    need(min(shutil.disk_usage('/root').free, shutil.disk_usage('/mnt/c').free) >= 8 * 1024**3, 'actual guest/backing8GiB required')
    lease = read(OUT / 'queue/request.json')
    command = q['worker_argv'] + ['--request-sha256', args.request_sha256, '--admission', str(args.admission)]
    need(lease['command'] == command and lease['owner'] == 'W3' and lease['kind'] == 'functional'
         and lease['timeout_seconds'] == 600 and lease['out'] == str(OUT / 'queue')
         and lease['cwd'] == str(ROOT) and lease['runner_sha256'] == fid(RUNNER)['sha256']
         and lease['lock_dir'] == str(RUNNER.parents[2] / 'build/vm-coordination'), 'original exact queue request differs')
    holder = read(RUNNER.parents[2] / 'build/vm-coordination/queue.lock')
    need(holder['status'] == 'running' and holder['run_id'] == lease['run_id'] and holder['out'] == lease['out'], 'actual original lease not held')
    need(set(p.name for p in OUT.iterdir()) == {'queue'}, 'fresh one-attempt resource output required')
    entry = Path(q['bound_campaign'])
    spec = importlib.util.spec_from_file_location('fixed_resource_original_six_role', entry)
    X = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(X)
    X.configure('qualification')
    need(tuple(X.P.ROLES) == ROLES, 'original six-role order differs')
    new, old = read(q['candidate_profile']), read(q['baseline_profile'])
    profile = X.profile_from_origins(new, old, proposal_path=str(args.request))
    basis = X.runtime(profile)
    X.captured_candidate_domains(profile, basis)
    X.fixed_binary_domains(basis)
    expected_by_path = {}
    for origin, path in ((new, q['candidate_runtime_record']), (old, q['baseline_runtime_record'])):
        previous = read(path)
        for label, path in origin['closure'].items():
            expected = previous['closure'][label]
            need(path not in expected_by_path or expected_by_path[path] == expected, 'inconsistent old/new closure path')
            expected_by_path[path] = expected
    expected_by_path[str(args.request)] = {'.': X.K.file_id(args.request)}
    need(all(basis['closure'][label] == expected_by_path[path] for label, path in profile['closure'].items()), 'actual current closure differs from admitted original runtime records')
    put(OUT / 'request.json', q)
    put(OUT / 'admission.json', admission)
    put(OUT / 'runtime-before.json', basis)
    put(OUT / 'profile.json', profile)
    X.copy(args.request, OUT / 'resource-request.json')
    for source in X.tool_paths():
        X.copy(source, OUT / 'protocol' / X.TOOL_MEMBERS[source.name])
    fixture = Path(q['fixture'])
    for name in (*X.G.FILES, 'fixture.json', 'queries.bin'):
        need(X.K.file_id(fixture / name) == X.K.file_id(X.BASELINE_FIXTURE / name), 'same fixedM fixture required')
        X.copy(fixture / name, OUT / 'frozen/fixture' / name)
    sizes = X.capture_inputs(OUT / 'frozen', profile)
    for role, item in profile['binaries'].items():
        X.copy(item['path'], OUT / 'frozen/bin' / role)
        (OUT / 'frozen/bin' / role).chmod(0o755)
        need(X.K.file_id(OUT / 'frozen/bin' / role) == basis['binaries'][role], 'staged executable differs')
    (OUT / 'commands').mkdir()
    put(OUT / 'frozen/status.json', {'final_wire_bytes': sizes})
    policy = X.E.resource_policy('qualification')
    ledger = X.L.Ledger(OUT, policy, Path('/mnt/c'))
    need(ledger.snapshot()['allocated_output_bytes'] <= q['preparation_base_reserve_bytes'], 'actual staged base exceeds declared512MiB projection reserve')
    completed, attempted, error = [], [], None
    try:
        for expected in q['jobs']:
            need(time.monotonic() - began <= 600 - 20 - 30, 'remaining child/seal allowance exhausted')
            job = {k: expected[k] for k in ('id', 'role', 'execution_kind')}
            argv, prefix = X.argv_for(OUT, job, profile)
            need(argv == expected['argv'], 'actual original resource argv differs')
            limit = X.limit_for(OUT / 'frozen', job['role'])
            timeout = X.G.launch_preflight(prefix, began + 600 - 30, 20, ledger.generated_wire, sizes[job['role']], limit, X.MAX_WIRE)
            need(timeout == 20, 'retain original full20second child allowance')
            ledger.before_attempt(job, prefix, limit)
            attempted.append(job)
            verified = False
            online_started = time.monotonic()
            try:
                receipt = X.G.execute(argv, prefix, 20, profile['environment'], limit)
                need(prefix.with_suffix('.stdout').stat().st_size == prefix.with_suffix('.stderr').stat().st_size == 0, 'native stdout/stderr nonempty')
                native = X.G.native_result(prefix.with_suffix('.native.json'), profile['binaries'][job['role']], OUT / 'frozen/fixture', OUT / 'frozen/input-wires' / (job['role'] + '.wire'), prefix.with_suffix('.wire'), prefix.with_suffix('.canonical'), prefix.with_suffix('.queries'), receipt, sizes[job['role']])
                need(int(native['peak_rss_after_audit_kib']) * 1024 <= 16 * 1024**3, 'reported resource RSS above16GiB')
                verified = True
            finally:
                ledger.record_outputs(job, prefix, verified=verified)
            completed.append(dict(**job, full_process_seconds=(int(receipt['finished_monotonic_ns']) - int(receipt['started_monotonic_ns'])) / 1e9,
                                  execute_truth_pool_wall_seconds=time.monotonic() - online_started,
                                  receipt=X.K.file_id(prefix.with_suffix('.receipt.json'))))
    except Exception as caught:
        error = type(caught).__name__ + ': ' + str(caught)
    try:
        after = X.runtime(profile)
        need(after == basis, 'runtime/source/tool identity changed')
        for p, expected in q['inputs'].items():
            need(fid(p) == expected, 'admitted input changed after resource observation')
    except Exception as caught:
        after = {'identity_error': str(caught)}
        error = error or str(caught)
    put(OUT / 'runtime-after.json', after)
    storage = ledger.reconcile(allowed_extra=('runtime-after.json',))
    put(OUT / 'resource-ledger.json', storage)
    by_role = {role: {kind: next((r for r in completed if r['role'] == role and r['execution_kind'] == kind), None)
                      for kind in ('conditioning', 'primary')} for role in ROLES}
    process_sum = sum(r['full_process_seconds'] for r in completed) if len(completed) == 12 else None
    online_sum = sum(r['execute_truth_pool_wall_seconds'] for r in completed) if len(completed) == 12 else None
    elapsed = time.monotonic() - began
    if elapsed > 600:
        error = error or 'original600second resource budget exceeded'
    result = dict(status='passed' if error is None and len(completed) == 12 else 'failed', error=error,
        request_sha256=args.request_sha256, attempted=attempted, completed=completed, by_role=by_role,
        elapsed_seconds=elapsed, observed_process_S=process_sum, observed_execute_truth_pool_S=online_sum,
        Q_process_necessary_seconds=None if process_sum is None else 388 * process_sum + 300,
        F_process_necessary_seconds=None if process_sum is None else 772 * process_sum + 300,
        Q_online_conservative_seconds=None if online_sum is None else 388 * online_sum + 300,
        F_online_conservative_seconds=None if online_sum is None else 772 * online_sum + 300,
        current_guest_free_bytes=shutil.disk_usage('/root').free, current_backing_free_bytes=shutil.disk_usage('/mnt/c').free,
        storage=storage, measurements_are_one_observation_each_not_future_tail_guarantees=True,
        capture=0, compile=0, retries=0, inference_samples=0, QF_admitted=False, performance_accepted=False)
    put(OUT / 'result.json', result)
    X.K.seal(OUT, dict(status=result['status'], kind='final-six-role-fixed-resource-observation-v1', error=error,
                      request_sha256=args.request_sha256, elapsed_before_seal_seconds=elapsed,
                      original_outer_seconds=600, reserved_seal_seconds=30, native=len(attempted), capture=0, compile=0,
                      retries=0, performance_accepted=False, QF_admitted=False))
    need(time.monotonic() - began <= 600, 'complete resource observation including seal exceeds600')
    need(result['status'] == 'passed', 'finite resource observation failed; preserve prefix, no retry: ' + str(error))
    print('Fixed12 resource processes and original truth/pool complete; no Q/F admission')


if __name__ == '__main__':
    main()
