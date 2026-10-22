"""fuzz_report.py: issue dedupe, regression branches, the infra issue and the watch."""
import importlib.util
import json
import os
from pathlib import Path
import stat
import sys
import tempfile
import types
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'fuzz_report.py'
spec = importlib.util.spec_from_file_location('fuzz_report', SCRIPT)
fuzz_report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fuzz_report)

SHA = 'a' * 40
SIG = '0123456789'


class FakeGh:
    def __init__(self):
        self.issues_list = []
        self.issue_comments = {}
        self.labels = set()
        self.branches = {}
        self.created_branches = []
        self.enabled = []
        self.dispatcher = {'state': 'active', 'created_at': '2026-10-22T00:00:00Z'}
        self.run_list = []

    def ensure_label(self, name, color, description):
        self.labels.add(name)

    def issues(self, label, state):
        return [dict(i) for i in self.issues_list
                if label in i['labels'] and (state == 'all' or i['state'] == state)]

    def create_issue(self, title, body, labels):
        number = 100 + len(self.issues_list)
        self.issues_list.append({'number': number, 'title': title, 'body': body,
                                 'labels': list(labels), 'state': 'open'})
        return {'number': number}

    def issue(self, number):
        return next(i for i in self.issues_list if i['number'] == number)

    def comment(self, number, body):
        self.issue_comments.setdefault(number, []).append(
            {'body': body, 'created_at': getattr(self, 'now', '2026-10-22T12:00:00Z')})

    def comments(self, number):
        return list(self.issue_comments.get(number, []))

    def set_state(self, number, state, reason=None):
        self.issue(number)['state'] = state

    def branch_exists(self, branch):
        return branch in self.branches

    def create_branch_with_file(self, branch, base_sha, path, content, message):
        self.branches[branch] = (base_sha, path, content)
        self.created_branches.append(branch)

    def enable_workflow(self, workflow):
        if self.dispatcher is None:
            raise fuzz_report.GhError(404, 'not found')
        self.enabled.append(workflow)

    def workflow(self, workflow):
        return self.dispatcher

    def runs(self, workflow, branch, event):
        return list(self.run_list)


def report_args(root, mode='nightly', needs=None, expect='libfuzzer-v2_wire,libfuzzer-wire_schemas'):
    needs_path = Path(root) / 'needs.json'
    needs_path.write_text(json.dumps(needs or {'libfuzzer': {'result': 'success'}}))
    return types.SimpleNamespace(repo='o/r', sha=SHA, run_id='42', mode=mode,
                                 server_url='https://github.com', findings_dir=str(Path(root) / 'art'),
                                 needs_json=str(needs_path), expect=expect)


def write_job(root, target, findings=(), fuzzer_exit='0', input_bytes=b'XOFFSET-INJECT'):
    report = Path(root) / 'art' / f'fuzz-{target}' / 'fuzz-report' / target
    report.mkdir(parents=True, exist_ok=True)
    items = []
    for sig in findings:
        name = f'{target}-{sig}'
        (report / name).write_bytes(input_bytes)
        items.append({'signature': sig, 'kind': 'crash', 'error_type': 'asan: heap-buffer-overflow',
                      'top_function': 'validate_vector', 'log_head': 'ERROR: AddressSanitizer ```x```',
                      'input': name, 'replay_cmd': f'build/fuzz/bin/Debug/fuzz_{target} -timeout=30 {name}'})
    (report / 'findings.json').write_text(json.dumps(
        {'schema': 1, 'target': target, 'artifact': f'fuzz-{target}', 'findings': items}))
    (report / 'stats.json').write_text(json.dumps({'fuzzer_exit': fuzzer_exit}))


class Report(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.root = self.dir.name
        self.gh = FakeGh()

    def tearDown(self):
        self.dir.cleanup()

    def run_report(self, **kwargs):
        return fuzz_report.run_report(self.gh, report_args(self.root, **kwargs))

    def test_new_signature_opens_issue_and_pushes_branch(self):
        write_job(self.root, 'v2_wire', [SIG])
        write_job(self.root, 'wire_schemas')
        result = self.run_report(needs={'libfuzzer': {'result': 'failure'}})
        self.assertEqual(self.gh.labels, {'fuzz-crash', 'fuzz-infra'})
        [issue] = self.gh.issues_list
        self.assertEqual(issue['title'], f'[fuzz] v2_wire: asan: heap-buffer-overflow in validate_vector ({SIG})')
        self.assertEqual(issue['labels'], ['fuzz-crash'])
        self.assertIn('actions/runs/42', issue['body'])
        self.assertIn(f'build/xo-docker build/fuzz/bin/Debug/fuzz_v2_wire -timeout=30 fuzz-repro/v2_wire-{SIG}',
                      issue['body'])
        self.assertIn('````', issue['body'])  # the log's ``` cannot close the fence
        self.assertEqual(self.gh.branches[f'fuzz-crash/{SIG}'],
                         (SHA, f'tests/fuzz/regressions/v2_wire/{SIG}', b'XOFFSET-INJECT'))
        self.assertEqual(result['infra_failures'], [])  # the failure is explained by the finding

    def test_same_signature_comments_and_keeps_branch(self):
        write_job(self.root, 'v2_wire', [SIG])
        write_job(self.root, 'wire_schemas')
        self.run_report()
        self.run_report()
        self.assertEqual(len(self.gh.issues_list), 1)
        self.assertEqual(self.gh.created_branches, [f'fuzz-crash/{SIG}'])
        [comment] = self.gh.issue_comments[100]
        self.assertTrue(comment['body'].startswith('Seen again.'))
        self.assertIn('already exists', comment['body'])

    def test_closed_signature_is_reopened_as_regression(self):
        self.gh.issues_list.append({'number': 7, 'title': f'[fuzz] v2_wire: x in y ({SIG})',
                                    'labels': ['fuzz-crash'], 'state': 'closed'})
        self.gh.branches[f'fuzz-crash/{SIG}'] = ('old', '', b'')
        write_job(self.root, 'v2_wire', [SIG])
        write_job(self.root, 'wire_schemas')
        self.run_report()
        self.assertEqual(self.gh.issue(7)['state'], 'open')
        self.assertIn('Regression', self.gh.issue_comments[7][0]['body'])
        self.assertEqual(len(self.gh.issues_list), 1)

    def test_other_signature_is_a_new_issue(self):
        self.gh.issues_list.append({'number': 7, 'title': '[fuzz] v2_wire: x in y (9999999999)',
                                    'labels': ['fuzz-crash'], 'state': 'open'})
        write_job(self.root, 'v2_wire', [SIG])
        write_job(self.root, 'wire_schemas')
        self.run_report()
        self.assertEqual(len(self.gh.issues_list), 2)
        self.assertNotIn(7, self.gh.issue_comments)

    def test_failure_without_finding_goes_to_one_rolling_infra_issue(self):
        write_job(self.root, 'v2_wire')  # wire_schemas never uploaded findings.json
        first = self.run_report(needs={'libfuzzer': {'result': 'failure'}})
        self.assertIn('job `libfuzzer`: failure with no finding', first['infra_failures'])
        self.assertIn('job `libfuzzer-wire_schemas`: no findings.json (did not finish)', first['infra_failures'])
        self.run_report(needs={'libfuzzer': {'result': 'cancelled'}})
        [issue] = self.gh.issues_list
        self.assertEqual(issue['labels'], ['fuzz-infra'])
        self.assertEqual(len(self.gh.issue_comments[issue['number']]), 1)

    def test_fuzzer_exit_without_input_is_infra(self):
        write_job(self.root, 'v2_wire', fuzzer_exit='77')
        write_job(self.root, 'wire_schemas')
        result = self.run_report()
        self.assertEqual(len(result['infra_failures']), 1)
        self.assertIn('exited 77', result['infra_failures'][0])

    def test_green_nightly_closes_infra_issue_and_manual_does_not(self):
        self.gh.issues_list.append({'number': 5, 'title': fuzz_report.INFRA_TITLE,
                                    'labels': ['fuzz-infra'], 'state': 'open'})
        write_job(self.root, 'v2_wire')
        write_job(self.root, 'wire_schemas')
        self.run_report(mode='manual')
        self.assertEqual(self.gh.issue(5)['state'], 'open')
        self.run_report(mode='nightly')
        self.assertEqual(self.gh.issue(5)['state'], 'closed')
        self.assertTrue(self.gh.issue_comments[5][0]['body'].startswith('Recovered'))

    def test_untrusted_fields_are_checked(self):
        report = Path(self.root) / 'art' / 'x'
        report.mkdir(parents=True)
        (Path(self.root) / 'outside').write_bytes(b'secret')
        (report / 'findings.json').write_text(json.dumps({'target': 'v2_wire', 'findings': [
            {'signature': '../../etc', 'input': 'a'},
            {'signature': SIG, 'target': 'bad/target'},
            {'signature': 'abcdefabcd', 'input': '../../outside'}]}))
        write_job(self.root, 'wire_schemas')
        result = self.run_report()
        self.assertEqual(len(result['infra_failures']), 3)
        self.assertEqual(self.gh.created_branches, [])
        [crash] = [i for i in self.gh.issues_list if 'fuzz-crash' in i['labels']]
        self.assertIn('not pushed, the finding has no input file', crash['body'])

    def test_reads_what_fuzz_findings_writes(self):
        """End to end with tools/fuzz_findings.py and a wrapper that prints a drill crash."""
        import subprocess
        artifacts = Path(self.root) / 'fuzz-artifacts'
        artifacts.mkdir()
        (artifacts / 'v2_wire-crash-1234').write_bytes(b'XOFFSET-INJECT')
        wrapper = Path(self.root) / 'wrapper'
        wrapper.write_text('#!/bin/sh\n'
                           'echo "XOFFSET_FUZZ_INJECT_CRASH: injected crash (drill input)"\n'
                           'echo "==1==ERROR: AddressSanitizer: ABRT on unknown address 0x1 (pc 0x2 bp 0x3 sp 0x4 T0)"\n'
                           'echo "    #0 0x5 in abort (/lib/x86_64-linux-gnu/libc.so.6+0x1)"\n'
                           'echo "    #1 0x6 in inject_crash_drill(unsigned char const*, unsigned long) '
                           '/workspace/tests/fuzz/v2_fuzz_core.hpp:115:9"\n'
                           'echo "    #2 0x7 in LLVMFuzzerTestOneInput /workspace/tests/fuzz/fuzz_v2_wire.cpp:12:5"\n'
                           'exit 1\n')
        wrapper.chmod(wrapper.stat().st_mode | stat.S_IEXEC)
        out = Path(self.root) / 'art' / 'fuzz-v2_wire' / 'fuzz-report' / 'v2_wire'
        subprocess.run([sys.executable, str(SCRIPT.parent / 'fuzz_findings.py'), 'findings', '--target', 'v2_wire',
                        '--bin', 'build/fuzz/bin/Debug/fuzz_v2_wire', '--flags=-timeout=30',
                        '--artifacts', str(artifacts), '--out', str(out), '--wrapper', str(wrapper),
                        '--minimize-seconds', '1'], check=True, capture_output=True)
        (out / 'stats.json').write_text(json.dumps({'fuzzer_exit': '0'}))
        write_job(self.root, 'wire_schemas')
        result = self.run_report(needs={'libfuzzer': {'result': 'failure'}})
        self.assertEqual(result['infra_failures'], [])
        [issue] = self.gh.issues_list
        self.assertRegex(issue['title'], r'^\[fuzz\] v2_wire: asan: ABRT in inject_crash_drill \([0-9a-f]{10}\)$')
        [(branch, (sha, path, content))] = self.gh.branches.items()
        self.assertEqual((sha, content), (SHA, b'XOFFSET-INJECT'))
        self.assertEqual(path, 'tests/fuzz/regressions/v2_wire/' + branch.split('/')[1])
        self.assertIn('build/xo-docker build/fuzz/bin/Debug/fuzz_v2_wire -timeout=30 fuzz-repro/v2_wire-', issue['body'])


def watch_args(**kwargs):
    base = dict(hours=36, manual=False, workflow='fuzz.yml', branch='next_cpp26',
                dispatcher='nightly-fuzz-dispatch.yml', title_regex=r'\bnightly\b',
                run_url='https://github.com/o/r/actions/runs/9', now='2026-10-24T12:00:00Z')
    base.update(kwargs)
    return types.SimpleNamespace(**base)


def run(created, title='Fuzz nightly · next_cpp26', number=1):
    return {'id': number, 'created_at': created, 'display_title': title,
            'html_url': f'https://github.com/o/r/actions/runs/{number}'}


class Watch(unittest.TestCase):
    def test_recent_nightly_is_quiet_and_dispatcher_is_enabled(self):
        gh = FakeGh()
        gh.run_list = [run('2026-10-23T18:30:00Z')]
        result = fuzz_report.run_watch(gh, watch_args())
        self.assertFalse(result['stale'])
        self.assertEqual(gh.enabled, ['nightly-fuzz-dispatch.yml'])
        self.assertEqual(gh.issues_list, [])

    def test_stale_nightly_comments_once_per_window(self):
        gh = FakeGh()
        gh.run_list = [run('2026-10-22T18:30:00Z'), run('2026-10-24T11:00:00Z', 'Fuzz manual 120s · x', 2)]
        gh.now = '2026-10-24T11:00:00Z'
        result = fuzz_report.run_watch(gh, watch_args())
        self.assertTrue(result['stale'])
        self.assertEqual(result['age_hours'], 41.5)
        [issue] = gh.issues_list
        self.assertEqual(issue['labels'], ['fuzz-infra'])
        self.assertIn(fuzz_report.STALE_MARKER, issue['body'])
        gh.comment(issue['number'], fuzz_report.STALE_MARKER + ' earlier')
        fuzz_report.run_watch(gh, watch_args())
        self.assertEqual(len(gh.issue_comments[issue['number']]), 1)  # suppressed
        fuzz_report.run_watch(gh, watch_args(manual=True, hours=0))
        self.assertEqual(len(gh.issue_comments[issue['number']]), 2)  # drills always comment

    def test_no_dispatcher_and_no_runs(self):
        gh = FakeGh()
        gh.dispatcher = None
        self.assertFalse(fuzz_report.run_watch(gh, watch_args())['stale'])
        self.assertTrue(fuzz_report.run_watch(gh, watch_args(manual=True, hours=0))['stale'])

    def test_no_nightly_yet_counts_from_dispatcher_creation(self):
        gh = FakeGh()
        self.assertTrue(fuzz_report.run_watch(gh, watch_args())['stale'])  # 60 h after creation
        self.assertFalse(fuzz_report.run_watch(gh, watch_args(now='2026-10-22T20:00:00Z'))['stale'])


class GhWrapper(unittest.TestCase):
    def test_paginate_and_status(self):
        with tempfile.TemporaryDirectory() as directory:
            fake = Path(directory) / 'gh'
            fake.write_text('#!/bin/sh\n'
                            'case "$*" in\n'
                            '  *missing*) echo "gh: Not Found (HTTP 404)" >&2; exit 1 ;;\n'
                            '  *--paginate*) echo \'[[{"number":1},{"number":2,"pull_request":{}}],[{"number":3}]]\' ;;\n'
                            'esac\n')
            fake.chmod(fake.stat().st_mode | stat.S_IEXEC)
            old = os.environ['PATH']
            os.environ['PATH'] = f'{directory}{os.pathsep}{old}'
            try:
                gh = fuzz_report.Gh('o/r')
                self.assertEqual([i['number'] for i in gh.issues('fuzz-crash', 'all')], [1, 3])
                self.assertFalse(gh.branch_exists('missing'))
            finally:
                os.environ['PATH'] = old


if __name__ == '__main__':
    unittest.main()
