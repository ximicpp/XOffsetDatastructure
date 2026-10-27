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



def report_args(root, mode='nightly', needs=None, expect='libfuzzer-v2_wire,libfuzzer-wire_schemas', **extra):
    needs_path = Path(root) / 'needs.json'
    needs_path.write_text(json.dumps(needs or {'libfuzzer': {'result': 'success'}}))
    return types.SimpleNamespace(repo='o/r', sha=SHA, run_id='42', mode=mode,
                                 server_url='https://github.com', findings_dir=str(Path(root) / 'art'),
                                 needs_json=str(needs_path), expect=expect, **extra)


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

    def test_refused_branch_push_still_files_the_issue_with_the_input(self):
        def refuse(*args):
            raise fuzz_report.GhError(403, 'gh api POST git/blobs: not permitted (HTTP 403)')
        self.gh.create_branch_with_file = refuse
        write_job(self.root, 'v2_wire', [SIG], input_bytes=b'\x00\x01wire')
        write_job(self.root, 'wire_schemas')
        result = self.run_report(needs={'libfuzzer': {'result': 'failure'}})
        [issue] = self.gh.issues_list
        self.assertIn('not pushed (GitHub refused the write, HTTP 403)', issue['body'])
        self.assertIn(f'add `tests/fuzz/regressions/v2_wire/{SIG}` by hand', issue['body'])
        self.assertIn('AAF3aXJl', issue['body'])  # base64 of the input
        self.assertIn(f'could not push `fuzz-crash/{SIG}`', ' '.join(result['actions']))

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

    def test_gcc16_job_from_a_docker_host_drill(self):
        """tools/fuzz_nightly.sh: GCC 16 findings.json at the job root, own run link, no branch in a drill."""
        job = Path(self.root) / 'art' / 'gcc16-replay'
        job.mkdir(parents=True)
        (job / f'v2_wire-{SIG}').write_bytes(b'XOFFSET-INJECT')
        (job / 'findings.json').write_text(json.dumps({'job': 'gcc16-replay', 'needs': 'gcc16-replay', 'findings': [
            {'target': 'v2_wire', 'signature': SIG, 'kind': 'crash', 'error_type': 'inject: injected crash',
             'top_function': '', 'log_head': 'XOFFSET_FUZZ_INJECT_CRASH', 'input': f'v2_wire-{SIG}',
             'reproduce': ['docker run ... fuzz_v2_wire_replay tests/fuzz/regressions/v2_wire/' + SIG]}]}))
        diff = Path(self.root) / 'art' / 'gcc16-diff'
        diff.mkdir()
        (diff / 'findings.json').write_text(json.dumps({'job': 'gcc16-diff', 'needs': 'gcc16-diff', 'findings': []}))
        result = fuzz_report.run_report(self.gh, report_args(
            self.root, mode='manual', expect='gcc16-diff,gcc16-replay',
            needs={'gcc16-diff': {'result': 'success'}, 'gcc16-replay': {'result': 'failure'}},
            run_url='https://example.invalid/fuzz-nightly/drill', no_branches=True, ref='next_cpp26'))
        self.assertEqual(result['infra_failures'], [])
        [issue] = self.gh.issues_list
        self.assertEqual(issue['title'], f'[fuzz] v2_wire: inject: injected crash in unknown frame ({SIG})')
        self.assertIn('[42](https://example.invalid/fuzz-nightly/drill)', issue['body'])
        self.assertIn('not pushed (drill run)', issue['body'])
        self.assertIn('fuzz_v2_wire_replay tests/fuzz/regressions/v2_wire/', issue['body'])
        self.assertEqual(self.gh.branches, {})


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
