"""Once-only Q32 host prelude and direct original timing dispatch.

Derived from validation/screen/m2/execute.py. The original shared W1 prelude
ends before the original shared W3 timing lease starts. No audit is launched
here; owned process resume and original PowerWindow restoration precede audit.
"""
from pathlib import Path
import argparse, base64, datetime, fcntl, hashlib, json, os, re, subprocess, sys, time, types

H = Path(__file__).resolve().parent
ROOT = Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
COORD = ROOT / 'docs/coordination/c14-iteration-20261020-v2/validation/final_formal_v1'
D = COORD / 'qualification-controller-v1'
HD = COORD / 'host-qualification-v1'
B = Path('/root/xoffset-c14-iteration/20261020/v2/validation/final-formal-v1')
OUT = B / 'qualification-v1'
FROZEN = B / 'frozen-qualification-v1'
PRELUDE = B / 'leases/qualification-host-prelude-v1'
RUNNER = Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py')
LOCK = RUNNER.parents[2] / 'build/vm-coordination'
ENTRY = H.parent / 'final_formal_bound_v1/six-role-campaign.py'
POWER_STATE = COORD / 'power-qualification-v1'
CANDIDATE = 'r1-m2-combined-v1'
PIN = 'b8e2eaa5f6743a88eb603637640e80a6cfcb11c0edb4183eedc7a67191aa3f23'
TARGET_POWER = '8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c'

def need(value, message):
    if not value:
        raise ValueError(message)

def fid(path):
    path = Path(path); need(path.is_file() and not path.is_symlink(), 'regular file required')
    need(path.stat().st_size < 2 * 1024**2, 'small source/metadata only')
    raw = path.read_bytes(); return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}

def ref(path):
    return {'path': str(path), 'identity': fid(path)}

def norm(identity):
    return {'bytes': int(identity['bytes']), 'sha256': identity['sha256']}

def read(path):
    fid(path); return json.loads(Path(path).read_text(encoding='utf-8-sig'))

def checked(item):
    need(fid(item['path']) == norm(item['identity']), 'pinned reference changed: ' + item['path'])
    return Path(item['path'])

def write(path, value):
    with Path(path).open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(value, stream, indent=2, sort_keys=True); stream.write('\n')

def pause_module():
    path = H / 'process_pause.py'
    need(fid(path)['sha256'] == '440c6da8dc0d179794ec98da102490aeef941797c78fc34bac9ba69852ed1fb6', 'accepted pause source changed')
    need(fid(H / 'pause-control.ps1')['sha256'] == '1015ace2e6abe0aa1407edcf29d264bf9ba8f3b7391279a9d0d25a72071e0989', 'accepted pause helper changed')
    module = types.ModuleType('_q32_pause'); module.__file__ = str(path)
    exec(compile(path.read_bytes(), str(path), 'exec'), module.__dict__)
    return module

def power(expected=None):
    argv = ['/mnt/c/Windows/System32/powercfg.exe', '/getactivescheme']
    result = subprocess.run(argv, capture_output=True, timeout=15)
    match = re.search(rb'[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}', result.stdout)
    need(result.returncode == 0 and match is not None, 'read-only actual power query failed')
    guid = match.group().decode().lower()
    need(expected is None or guid == expected, 'actual original power scheme differs')
    return {'guid': guid, 'argv': argv, 'stdout_base64': base64.b64encode(result.stdout).decode()}

def prerequisites(a, P):
    quiet, review, host = read(a.quiet), read(a.review), read(a.host_review)
    for record, kind in ((quiet, 'c14-q32-global-quiet'), (review, 'c14-q32-independent-admission'), (host, 'c14-q32-host-admission')):
        need(record['status'] == 'passed' and record['kind'] == kind and record['freeze_sha256'] == PIN
             and record['candidate'] == CANDIDATE, 'exact current Q32 quiet/review/host records required')
    need(quiet['all_other_chats_and_agents_quiet'] is True and quiet['no_other_heavy_jobs'] is True
         and quiet['C_compilation_active'] is False and quiet['evidence'], 'actual controlled global quiet facts required')
    age = (datetime.datetime.now(datetime.timezone.utc) - datetime.datetime.fromisoformat(quiet['observed_utc'].replace('Z', '+00:00'))).total_seconds()
    need(0 <= age <= 900, 'fresh quiet observation required')
    need(fid(a.request)['sha256'] == a.request_sha256 and checked(review['timing_request']) == a.request,
         'root review must pin exact timing request')
    request = read(a.request)
    expected = ['/usr/bin/python3', '-B', str(ENTRY), 'run', '--campaign', 'qualification', '--out', str(OUT),
                '--frozen', str(FROZEN), '--frozen-sha256', PIN, '--queue', str(OUT / 'queue')]
    runner_argv = ['/usr/bin/python3', '-B', str(RUNNER), 'run', '--owner', 'W3', '--kind', 'timing',
                   '--out', str(OUT / 'queue'), '--timeout-seconds', '2700', '--wait-seconds', '0', '--', *expected]
    need(request['status'] == 'ready_for_independent_admission' and request['campaign'] == 'qualification'
         and request['command'] == expected and request['argv'] == runner_argv and request['cwd'] == str(ROOT), 'fixed direct original2700 timing request differs')
    need(fid(FROZEN / 'manifest.json')['sha256'] == PIN and a.frozen == FROZEN and a.freeze_sha256 == PIN,
         'exact frozen manifest domain differs')
    need(read(FROZEN / 'status.json')['window_out'] == str(OUT), 'frozen actual output differs')
    need(fid(ENTRY)['sha256'] == '6c3319cb3502ce380d3ddd022be33de9da79483ea7ecbd911695fc215b4964c8'
         and fid(RUNNER)['sha256'] == '6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909', 'accepted entry/runner changed')
    need(checked(review['launcher_source']) == H / 'execute.py' and checked(host['launcher_source']) == H / 'execute.py', 'launcher review pin differs')
    ledger = read(checked(review['attempt_ledger']))
    need(ledger['status'] == 'registered_once_no_retry' and ledger['candidate'] == CANDIDATE
         and ledger['freeze_sha256'] == PIN and ledger['maximum_windows'] == 1 and ledger['retries'] == 0, 'external once-only attempt ledger required')
    need(host['allowed_names'] == list(P.ALLOWED) and host['fresh_plan_required'] is True and host['no_old_pid_or_state'] is True
         and host['watchdog_seconds'] == 2820 and host['restoration_before_audit'] is True
         and host['prelaunch_remaining_seconds_minimum'] == 2760, 'bounded actual two-name recovery registration differs')
    grant = read(checked(host['standing_human_authorization'])); checked(host['original_human_authorization'])
    need(grant['answer'] == '允许临时暂停并恢复' and grant['authorized_targets'] == list(P.ALLOWED)
         and grant['original_root_record'] == host['original_human_authorization'], 'standing two-name human authority differs')
    policy = read(checked(host['power_policy']))
    need(policy['QF_power']['existing_target_GUID'] == TARGET_POWER and host['power_target_guid'] == TARGET_POWER
         and host['power_temporary_change'] is True and host['source_power_unchanged'] is True
         and host['restore_original_power'] is True, 'original temporary power/restore policy differs')
    checked(host['watchdog_source_acceptance']); checked(review['actual_freeze_acceptance'])
    return quiet, host, request

def own_prelude():
    with (LOCK / 'queue.lock').open('r+') as stream:
        try:
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            pass
        else:
            fcntl.flock(stream, fcntl.LOCK_UN); raise ValueError('original lease not held')
        stream.seek(0); holder = json.load(stream)
    request = read(PRELUDE / 'request.json')
    expected = ['/usr/bin/python3', '-B', str(H / 'execute.py'), *sys.argv[1:]]
    need(holder['status'] == 'running' and holder['out'] == str(PRELUDE) and holder['run_id'] == request['run_id']
         and holder['supervisor_pid'] == os.getppid() and holder['owner'] == request['owner'] == 'W1'
         and holder['kind'] == request['kind'] == 'validation', 'own original W1 prelude identity required')
    need(request['command'] == expected and request['timeout_seconds'] == 60 and request['lock_dir'] == str(LOCK)
         and request['out'] == str(PRELUDE) and request['cwd'] == str(ROOT)
         and request['runner_sha256'] == fid(RUNNER)['sha256'], 'exact original prelude argv/cwd/cap required')
    need(Path('/proc/self/cmdline').read_bytes().split(b'\0')[:-1] == [v.encode() for v in expected]
         and (Path('/proc') / str(os.getppid()) / 'cmdline').read_bytes().split(b'\0')[:-1]
         == [b'/usr/bin/python3', str(RUNNER).encode(), b'_worker', str(PRELUDE).encode()], 'actual worker/supervisor argv required')
    return {'queue': str(PRELUDE), 'run_id': request['run_id'], 'lock': str(LOCK / 'queue.lock'),
            'holder': holder, 'request': fid(PRELUDE / 'request.json')}

def prelude(a, P):
    authority = own_prelude(); _, host, _ = prerequisites(a, P)
    before = power(host['preparation_original_power_guid'])
    need(not P.STATE.exists() and not POWER_STATE.exists(), 'fresh independent pause/power state required')
    HD.mkdir(parents=True, exist_ok=True); write(HD / 'prelude-authority.json', authority)
    pre = HD / 'prelude-prerequisites.json'
    write(pre, {'schema_version': 1, 'kind': 'cursor-process-pause-prerequisites-v1', 'C_compilation_active': False,
                'all_agents_quiet': True, 'controlled_queues_drained': True, 'prelude_lease_owner': 'W1',
                'prelude_lease_kind': 'validation', 'prelude_queue': str(PRELUDE),
                'authority': ref(HD / 'prelude-authority.json'), 'quiet_evidence': ref(a.quiet)})
    plan = P.prepare_plan(HD / 'process-plan-v1.json')
    guard = P.ProcessPause(plan, ref(pre), maximum_seconds=2820)
    before_guard = time.monotonic_ns(); guard.start_and_leave_running()
    write(HD / 'PRELUDE_READY.json', {'status': 'passed', 'freeze_sha256': PIN,
        'guard_controller_start_monotonic_ns': str(before_guard), 'ready_monotonic_ns': str(time.monotonic_ns()),
        'boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip(), 'power_before': before,
        'plan': plan, 'guard_ready': ref(P.STATE / 'ready.json'), 'controller': ref(H / 'execute.py'),
        'standalone_bounded_watchdog_seconds': 2820})

def main():
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument('action', choices=('run', 'prelude'))
    for name in ('frozen', 'review', 'quiet', 'host-review', 'request'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--freeze-sha256', required=True); parser.add_argument('--request-sha256', required=True)
    a = parser.parse_args(); P = pause_module(); _, host, request = prerequisites(a, P)
    if a.action == 'prelude':
        prelude(a, P); return
    need(not OUT.exists() and not PRELUDE.exists() and not P.STATE.exists() and not POWER_STATE.exists() and not D.exists(), 'one fresh qualification attempt only')
    D.mkdir(); HD.mkdir(parents=True, exist_ok=True)
    write(D / 'ATTEMPT_CLAIMED.json', {'status': 'claimed_before_prelude_no_retry', 'request': ref(a.request),
        'review': ref(a.review), 'quiet': ref(a.quiet), 'host_review': ref(a.host_review), 'freeze_sha256': PIN})
    tail = ['--frozen', str(a.frozen), '--review', str(a.review), '--quiet', str(a.quiet), '--host-review', str(a.host_review),
            '--request', str(a.request), '--freeze-sha256', PIN, '--request-sha256', a.request_sha256]
    cmd = ['/usr/bin/python3', '-B', str(RUNNER), 'run', '--owner', 'W1', '--kind', 'validation', '--out', str(PRELUDE),
           '--timeout-seconds', '60', '--wait-seconds', '0', '--', '/usr/bin/python3', '-B', str(H / 'execute.py'), 'prelude', *tail]
    write(D / 'PRELUDE_DISPATCH_REQUEST.json', {'argv': cmd, 'quiet': ref(a.quiet), 'host_review': ref(a.host_review),
        'watchdog_budget': '2820 total; actual conservative remaining>=2760 immediately before2700 timing; no audit inside pause.'})
    rc = None; timing = None; restored = False; error = None; power_error = None; after = {}; ready = {}; power_done = None
    try:
        result = subprocess.run(cmd, cwd=ROOT); need(result.returncode == 0, 'prelude failed; restoration first')
        ready = read(HD / 'PRELUDE_READY.json')
        need(ready['boot_id'] == Path('/proc/sys/kernel/random/boot_id').read_text().strip(), 'monotonic clock boot changed')
        need(time.monotonic_ns() - int(ready['ready_monotonic_ns']) <= 30_000_000_000, 'handoff exceeded30 seconds')
        prerequisites(a, P); power(ready['power_before']['guid'])
        need(not (P.STATE / 'completed.json').exists(), 'guard already restored')
        remaining = 2820 * 10**9 - (time.monotonic_ns() - int(ready['guard_controller_start_monotonic_ns']))
        need(remaining >= 2760 * 10**9, 'insufficient conservative remaining guard coverage')
        write(D / 'HOST.json', {'status': 'passed', 'freeze_sha256': PIN, 'ready': ref(HD / 'PRELUDE_READY.json'),
            'actual_plan': ready['plan'], 'remaining_guard_nanoseconds_before_dispatch': str(remaining),
            'coverage_bound': 'Controller start precedes independent Windows guard clock, so this remaining bound is conservative.',
            'power_before': ready['power_before'], 'power_target_guid': TARGET_POWER, 'power_changed_by_launcher': False})
        write(D / 'TIMING_DISPATCH_REQUEST.json', {'argv': request['argv'], 'request': ref(a.request), 'review': ref(a.review), 'host': ref(D / 'HOST.json')})
        # Atomic runner admission immediately follows the check; any contention fails wait0, without a new attempt.
        remaining = 2820 * 10**9 - (time.monotonic_ns() - int(ready['guard_controller_start_monotonic_ns']))
        need(remaining >= 2760 * 10**9, 'remaining guard fell below2760 at dispatch')
        write(D / 'IMMEDIATE_DISPATCH_GUARD_BOUND.json', {'remaining_nanoseconds': str(remaining), 'minimum_seconds': 2760,
             'observed_monotonic_ns': str(time.monotonic_ns())})
        rc = subprocess.run(request['argv'], cwd=ROOT).returncode
        if (OUT / 'queue/result.json').exists():
            timing = read(OUT / 'queue/result.json')
    except BaseException as exception:
        error = type(exception).__name__ + ': ' + str(exception)
    finally:
        if (P.STATE / 'request.json').exists():
            try:
                restored = P.stop().get('status') == 'passed'
            except BaseException as exception:
                error = (error or '') + '; owned process restoration ' + type(exception).__name__ + ': ' + str(exception)
        if (HD / 'PRELUDE_READY.json').exists():
            ready = read(HD / 'PRELUDE_READY.json')
        try:
            if (POWER_STATE / 'request.json').exists():
                # Original run/finally or independent original watchdog owns power restore; never rewrite its stop/state.
                deadline = time.monotonic() + 90
                while not (POWER_STATE / 'completed.json').exists() and time.monotonic() < deadline:
                    time.sleep(.1)
                power_done = read(POWER_STATE / 'completed.json')
                need(power_done['restored'] is True, 'original PowerWindow did not restore')
            after = power(ready.get('power_before', {}).get('guid', host['preparation_original_power_guid']))
        except BaseException as exception:
            power_error = type(exception).__name__ + ': ' + str(exception)
        write(D / 'RESTORED.json', {'status': 'passed' if restored and power_error is None else 'rejected',
            'user_processes_restored': restored, 'resumed_monotonic_ns': str(time.monotonic_ns()),
            'power_before_evidence': ready.get('power_before'), 'power_after_evidence': after,
            'power_completed': ref(POWER_STATE / 'completed.json') if (POWER_STATE / 'completed.json').exists() else None,
            'guard_completed': ref(P.STATE / 'completed.json') if (P.STATE / 'completed.json').exists() else None,
            'power_changed_by_launcher': False, 'original_powerwindow_temporary_change_disclosed': True,
            'error': error, 'power_error': power_error, 'offline_audit_started': False})
    if restored and timing and timing['status'] == 'complete' and timing['command_returncode'] == 0:
        coverage = P.verify_coverage(P.STATE, ref(OUT / 'queue/request.json'), ref(OUT / 'queue/result.json'))
        write(HD / 'TIMING_PAUSE_COVERAGE.json', coverage)
        need(power_done is not None and power_done['status'] == 'passed' and power_error is None,
             'original actual power condition or restoration failed')
        power_ready = read(POWER_STATE / 'ready.json')
        need(power_ready['status'] == 'ready' and power_ready['active'] == TARGET_POWER
             and power_ready['original'] == ready['power_before']['guid'] and power_ready['maximum_seconds'] == 2760,
             'original actual power ready/coverage identity differs; offline original audit still mandatory')
    write(D / 'TIMING_DISPATCH_RESULT.json', {'returncode': rc, 'restored': restored, 'error': error,
        'power_error': power_error, 'timing_result': timing, 'qualification_attempts': 1, 'retries': 0,
        'offline_audit_started': False})
    need(restored and power_error is None, 'restoration failed; preserve all records and do not audit')
    raise SystemExit(rc if rc is not None else 1)

if __name__ == '__main__':
    main()
