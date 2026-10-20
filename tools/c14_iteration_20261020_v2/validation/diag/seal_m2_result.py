"""Bind the completed M2 attempt to its original queue; no compiler/native/checker run."""
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import m2_mechanism_job as M

C, OUT, DOC = M.C, M.OUTPUT, M.DOC
PIN = '001ddddf7bf6e83ca6e68dc0de095c517b127d094d2168157c68c7a1084a018b'


def ref(path):
    return dict(path=str(path), identity=C.fid(path))


def main():
    freeze = DOC / 'M2_MECHANISM_FREEZE_DRAFT.json'
    admission = DOC / 'M2_MECHANISM_ADMISSION.json'
    C.need(C.fid(freeze)['sha256'] == PIN, 'admitted freeze changed')
    f = C.read(freeze)
    C.need(C.fid(OUT / 'freeze.json') == C.fid(freeze), 'actual freeze copy differs')
    C.need(C.fid(OUT / 'admission.json') == C.fid(admission), 'actual admission copy differs')
    C.verify_files(f)
    for name in ('identity-before.json', 'identity-after.json'):
        C.need(C.read(OUT / name) == f['fixed_files'], 'actual identity map differs: ' + name)
    request = C.read(DOC / 'M2_MECHANISM_REQUEST_DRAFT.json')
    q = C.read(OUT / 'queue/request.json')
    r = C.read(OUT / 'queue/result.json')
    expected = ['/usr/bin/python3', '-B', str(HERE / 'm2_mechanism_job.py'), 'run', '--frozen', str(freeze),
                '--freeze-sha256', PIN, '--admission', str(admission), '--queue', str(OUT / 'queue')]
    C.need(q['command'] == expected and q['owner'] == 'W3' and q['kind'] == 'diagnostic'
           and q['timeout_seconds'] == 300 and q['wait_seconds'] == 0 and q['cwd'] == str(C.ROOT)
           and q['lock_dir'] == str(C.LOCK) and q['out'] == str(OUT / 'queue'), 'exact admitted M2 request differs')
    for key in ('command', 'cwd', 'owner', 'kind', 'timeout_seconds', 'wait_seconds', 'out', 'lock_dir'):
        C.need(q[key] == request[key], 'original queue request differs: ' + key)
    C.need(q['runner_sha256'] == C.fid(C.RUNNER)['sha256'], 'original runner changed')
    lease = C.read(OUT / 'lease.json')
    C.need(lease['request'] == q and lease['request_identity'] == C.fid(OUT / 'queue/request.json'), 'actual lease request differs')
    C.need(lease['holder']['run_id'] == q['run_id'] and lease['holder']['out'] == q['out']
           and lease['holder']['owner'] == 'W3' and lease['holder']['kind'] == 'diagnostic'
           and lease['holder']['status'] == 'running' and lease['coordinator_pid'] == lease['session'], 'actual lease provenance differs')
    C.need(q['run_id'] == r['run_id'] and r['status'] == 'complete' and r['command_returncode'] == 0
           and r['reason'] == 'command_exit' and r['command_elapsed_seconds'] <= 300,
           'outer queue did not complete within its admitted cap')
    C.need(r['cleanup'] == dict(live=[], process_groups=[], zombies=[]), 'outer cleanup incomplete')
    manifest = C.read(OUT / 'queue/manifest.json')['files']
    names = {entry['path'] for entry in manifest}
    C.need(len(manifest) == 6 and names == {'request.json', 'result.json', 'events.json', 'stdout.log', 'stderr.log', 'supervisor.log'},
           'original queue manifest inventory differs')
    C.need({p.name for p in (OUT / 'queue').iterdir()} == names | {'manifest.json'}, 'queue inventory changed')
    for entry in manifest:
        C.need(C.fid(OUT / 'queue' / entry['path']) == {k: entry[k] for k in ('bytes', 'sha256')}, 'queue manifest identity differs')
    result = C.read(OUT / 'RESULT.json')
    C.need(result['status'] == 'passed' and result['error'] is None and result['freeze_sha256'] == PIN
           and result['elapsed_seconds'] <= 300, 'internal result incomplete')
    for key in ('compile_attempts', 'compiles_passed', 'native_attempts', 'audits_passed'):
        C.need(result[key] == 1, 'one-candidate inventory differs: ' + key)
    C.need(result['new_control_compiles'] == result['new_control_runs'] == result['retry'] == 0, 'unexpected control or retry')
    role = f['role']
    p = OUT / 'm2'
    audit = C.read(p / 'AUDIT.json')
    observer = C.read(p / 'observe/receipt.json')
    compile_receipt = C.read(p / 'compile-receipt.json')
    C.need(observer['queue_request'] == q, 'native observer belongs to a different queue request')
    C.need(audit['status'] == 'passed' and audit['observer_receipt'] == C.fid(p / 'observe/receipt.json'), 'audited observer identity differs')
    C.need(observer['status'] == 'complete' and observer['returncode'] == 0 and observer['successful_wait4_calls'] == 1
           and observer['cleanup_remaining'] == [] and observer['command'] == role['native_argv'], 'native/reaper proof failed')
    process_seconds = (int(observer['returned_monotonic_ns']) - int(observer['started_monotonic_ns'])) / 1e9
    C.need(process_seconds <= 20, 'native exceeded admitted20 seconds')
    C.need(compile_receipt['returncode'] == 0 and compile_receipt['successful_wait4_calls'] == 1
           and compile_receipt['cleanup_remaining'] == [] and compile_receipt['argv'] == role['compile_argv'], 'compile/reaper proof failed')
    C.need(int(compile_receipt['returned_monotonic_ns']) - int(compile_receipt['started_monotonic_ns']) <= 120 * 10**9, 'compile exceeded120 seconds')
    C.need(compile_receipt['binary'] == C.fid(role['binary']), 'compiled binary changed')
    C.need(set(audit['body_identities']) == set(role['bodies']) == {'canonical', 'native', 'queries', 'wire'}, 'four-body inventory differs')
    for key, identity in audit['body_identities'].items():
        C.need(C.fid(role['bodies'][key]) == identity, 'audited body changed: ' + key)
    expected_bindings = dict(source_identity=Path(role['source']) / 'FINAL_SOURCE_IDENTITY.json',
        driver=Path(role['source']) / 'experiment/driver.cpp', binary=Path(role['binary']),
        compile_receipt=p / 'compile-receipt.json', hook=Path(role['source']) / 'source/c14_common_diag.hpp',
        event_channel=Path(role['source']) / 'experiment/event_channel.hpp')
    C.need(set(observer['binding']['files']) == set(expected_bindings), 'six-binding inventory differs')
    C.need(C.read(p / 'native-attempt.json')['binding'] == observer['binding'], 'native attempt/observer binding differs')
    for key, binding in observer['binding']['files'].items():
        C.need(binding['path'] == str(expected_bindings[key]), 'native binding path differs: ' + key)
        C.need(C.fid(binding['path']) == binding['identity'], 'native binding changed')
    mechanism = C.read(OUT / 'MECHANISM.json')
    C.need(mechanism['status'] in ('mechanism_hit', 'closed_no_dynamic_hit')
           and mechanism['status'] == result['mechanism_outcome'] and mechanism['candidate'] == 'M2'
           and mechanism['diagnostic_only'] is True and mechanism['speed_inferred'] is False,
           'actual preregistered checker result differs')
    C.need(all(mechanism[key] is False for key in ('execution_admission_checked', 'bare_codegen_checked', 'screen_admitted', 'QF_admitted')),
           'checker scope flags differ')
    C.need(mechanism['external_audit_sha256'] == dict(control=M.CONTROL_SHA, candidate=C.fid(p / 'AUDIT.json')['sha256']),
           'checker audit pins differ')
    # The checker already ran once in the admitted attempt; preserve its exact output.
    C.write(DOC / 'M2_DYNAMIC_RESULT.json', dict(result=mechanism, actual_checker=ref(OUT / 'MECHANISM.json'),
        checker=ref(Path(f['checker'])), new_checker_runs=0, new_diagnostic_native=0))
    C.write(DOC / 'M2_MECHANISM_RESULT.json', dict(status='passed', freeze=ref(freeze), admission=ref(admission),
        queue_request=ref(OUT / 'queue/request.json'), queue_result=ref(OUT / 'queue/result.json'),
        queue_manifest=ref(OUT / 'queue/manifest.json'), internal_result=ref(OUT / 'RESULT.json'),
        run_id=q['run_id'], actual_outer_seconds=r['command_elapsed_seconds'], cleanup=r['cleanup'],
        actual_compiles=1, actual_diagnostic_native=1, new_control_compiles=0, new_control_runs=0, retries=0,
        source_runtime_input_files=len(f['fixed_files']), source_runtime_before_after_unchanged=True,
        actual_allocated_bytes=C.allocated(OUT), process_seconds=process_seconds,
        audit=ref(p / 'AUDIT.json'), compile=ref(p / 'compile-receipt.json'), observer=ref(p / 'observe/receipt.json'),
        control=f['control'], mechanism=ref(DOC / 'M2_DYNAMIC_RESULT.json'), tool=ref(Path(__file__)),
        performance_admitted=False, screen_admitted=False, QF_admitted=False, limitations=result['limitations']))
    print(dict(status='passed', mechanism=mechanism['status'], new_native_in_seal=0, new_checker_runs=0))


if __name__ == '__main__':
    main()
