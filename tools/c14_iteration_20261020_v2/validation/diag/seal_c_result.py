"""Seal the completed fixed C queue; inspect bytes and run only the pinned count checker."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('phase_c', HERE / 'phase_c_job.py')
C = importlib.util.module_from_spec(spec)
spec.loader.exec_module(C)
OUT = C.OUTPUT
DOC = C.DOC / 'validation/diag'
PIN = '250ce7daf7e1fb20513c8977928524c008aa6079211dbb66b9f5ef7efc4c7f0c'


def need(value, reason):
    if not value:
        raise ValueError(reason)


def ref(path):
    path = Path(path)
    return {'path': str(path), 'identity': C.fid(path)}


def main():
    freeze = DOC / 'C_FREEZE_DRAFT.json'
    need(C.fid(freeze)['sha256'] == PIN, 'exact admitted freeze changed')
    f = C.read(freeze)
    need(C.fid(OUT / 'freeze.json') == C.fid(freeze), 'executed freeze copy differs')
    need(C.fid(OUT / 'admission.json') == C.fid(DOC / 'C_ADMISSION.json'), 'admission copy differs')
    C.verify_files(f)
    need(C.read(OUT / 'identity-before.json') == f['fixed_files'], 'actual before identity map differs')
    need(C.read(OUT / 'identity-after.json') == f['fixed_files'], 'actual after identity map differs')
    q = C.read(OUT / 'queue/request.json')
    r = C.read(OUT / 'queue/result.json')
    expected = ['/usr/bin/python3', '-B', str(HERE / 'phase_c_job.py'), 'run', '--frozen', str(freeze), '--freeze-sha256', PIN,
                '--admission', str(DOC / 'C_ADMISSION.json'), '--queue', str(OUT / 'queue')]
    need(q['command'] == expected and q['owner'] == 'W3' and q['kind'] == 'diagnostic' and q['cwd'] == str(C.ROOT), 'actual queue command/cwd/owner differs')
    need(q['timeout_seconds'] == 600 and q['wait_seconds'] == 0 and q['lock_dir'] == str(C.LOCK) and q['runner_sha256'] == C.fid(C.RUNNER)['sha256'], 'original runner/lease/cap differs')
    need(r['run_id'] == q['run_id'] and r['status'] == 'complete' and r['command_returncode'] == 0 and r['reason'] == 'command_exit', 'outer original queue not successful')
    need(r['command_elapsed_seconds'] <= 600 and r['cleanup'] == {'live': [], 'process_groups': [], 'zombies': []}, 'outer deadline/cleanup failed')
    for entry in C.read(OUT / 'queue/manifest.json')['files']:
        name = entry['path']
        need(C.fid(OUT / 'queue' / name) == {k: entry[k] for k in ('bytes', 'sha256')}, 'original queue manifest member changed: ' + name)
    result = C.read(OUT / 'RESULT.json')
    for key in ('attempted_roles', 'completed_roles', 'compile_attempts', 'native_attempts'):
        need(result[key] == ['c14', 'r1'], 'actual two-role inventory differs: ' + key)
    need(result['status'] == 'passed' and result['error'] is None and result['unexecuted_roles'] == [] and result['elapsed_seconds'] <= 600, 'internal result is incomplete')
    rows = {}
    for role in ('c14', 'r1'):
        p = OUT / role
        audit = C.read(p / 'AUDIT.json')
        observer = C.read(p / 'observe/receipt.json')
        compile_receipt = C.read(p / 'compile-receipt.json')
        need(audit['status'] == 'passed' and audit['observer_receipt'] == C.fid(p / 'observe/receipt.json'), 'actual role audit/observer identity differs')
        need(observer['status'] == 'complete' and observer['returncode'] == 0 and observer['successful_wait4_calls'] == 1 and observer['cleanup_remaining'] == [], 'actual child/reaper/cleanup failed')
        need(int(observer['returned_monotonic_ns']) - int(observer['started_monotonic_ns']) <= 20 * 10**9, 'child exceeded20 seconds')
        need(observer['command'] == f['roles'][role]['native_argv'], 'executed native argv differs')
        need(compile_receipt['returncode'] == 0 and compile_receipt['successful_wait4_calls'] == 1 and compile_receipt['cleanup_remaining'] == [], 'actual compile/reaper/cleanup failed')
        need(compile_receipt['argv'] == f['roles'][role]['compile_argv'] and int(compile_receipt['returned_monotonic_ns']) - int(compile_receipt['started_monotonic_ns']) <= 120 * 10**9, 'actual compile argv/deadline differs')
        for key, identity in audit['body_identities'].items():
            need(C.fid(f['roles'][role]['bodies'][key]) == identity, 'audited body changed')
        for binding in observer['binding']['files'].values():
            need(C.fid(binding['path']) == binding['identity'], 'compiled source/binary binding changed')
        rows[role] = dict(audit=ref(p / 'AUDIT.json'), observer=ref(p / 'observe/receipt.json'), compile=ref(p / 'compile-receipt.json'),
                          process_seconds=(int(observer['returned_monotonic_ns']) - int(observer['started_monotonic_ns'])) / 1e9,
                          cpu_seconds=observer['cpu_seconds'], user_cpu_seconds=observer['user_cpu_seconds'], system_cpu_seconds=observer['system_cpu_seconds'],
                          minor_faults=observer['minor_faults'], major_faults=observer['major_faults'], max_rss_kib=observer['max_rss_kib'])
    checker = C.ROOT / 'tools/c14_iteration_20261020_v2/core/check_dynamic_acceptance.py'
    need(C.fid(checker) == f['fixed_files'][str(checker)], 'preregistered checker changed')
    command = ['/usr/bin/python3', '-I', '-B', str(checker), '--candidate', 'r1']
    for name, role in (('control', 'c14'), ('candidate', 'r1')):
        command += ['--' + name + '-audit', rows[role]['audit']['path'], '--' + name + '-sha256', rows[role]['audit']['identity']['sha256']]
    checked = subprocess.run(command, capture_output=True, timeout=30)
    need(not checked.stderr and checked.returncode in (0, 1, 2), 'offline checker process failure')
    dynamic = json.loads(checked.stdout)
    C.write(DOC / 'R1_DYNAMIC_RESULT.json', dict(result=dynamic, returncode=checked.returncode, argv=command, checker=ref(checker), audits={role: value['audit'] for role, value in rows.items()}, new_diagnostic_native=0))
    C.write(DOC / 'C_RESULT.json', dict(status='passed', freeze=ref(freeze), admission=ref(DOC / 'C_ADMISSION.json'), queue_request=ref(OUT / 'queue/request.json'),
        queue_result=ref(OUT / 'queue/result.json'), queue_manifest=ref(OUT / 'queue/manifest.json'), internal_result=ref(OUT / 'RESULT.json'),
        run_id=q['run_id'], actual_outer_seconds=r['command_elapsed_seconds'], cleanup=r['cleanup'], roles=rows,
        actual_compiles=2, actual_diagnostic_native=2, retries=0, actual_allocated_bytes=C.allocated(OUT), source_runtime_input_files=len(f['fixed_files']),
        source_runtime_before_after_unchanged=True, mechanism=ref(DOC / 'R1_DYNAMIC_RESULT.json'), performance_admitted=False, screen_admitted=False,
        tool=ref(__file__), limitations=result['limitations']))
    print(json.dumps(dict(status='passed', C_result=str(DOC / 'C_RESULT.json'), dynamic_status=dynamic['status'], diagnostic_native=2, new_native_in_seal=0)))


if __name__ == '__main__':
    main()
