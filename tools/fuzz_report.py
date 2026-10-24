#!/usr/bin/env python3
"""Nightly fuzz reporting (H4).

`report` runs after a nightly or manual fuzz run, on GitHub Actions
(.github/workflows/fuzz.yml) or on any Docker host (tools/fuzz_nightly.sh).
It reads every findings.json under --findings-dir, files one issue per crash
signature (comment on an open one, reopen a closed one as a regression,
otherwise create one), pushes the reproducer to a `fuzz-crash/<signature>`
branch under tests/fuzz/regressions/, and records job failures without
findings on the single rolling `fuzz-infra` issue. A nightly with every job
green closes that issue. Crash issues are never closed here; the fix PR
closes them.

findings.json is what tools/fuzz_findings.py (libFuzzer jobs) and
tools/fuzz_gcc16.py (GCC 16 jobs) write. The fields read here:

    {"target": "v2_wire", "artifact": "fuzz-v2_wire",
     "job": "libfuzzer-v2_wire",   # optional; default libfuzzer-<target>
     "needs": "libfuzzer",         # optional; the job's key in `needs`
     "findings": [{"signature": "0123456789",    # 10 lowercase hex digits
                   "kind": "crash", "error_type": "asan: heap-buffer-overflow",
                   "top_function": "...", "log_head": "...",
                   "input": "v2_wire-0123456789",  # relative to findings.json
                   "replay_cmd": "build/fuzz/bin/Debug/fuzz_v2_wire ... v2_wire-0123456789",
                   "reproduce": ["optional shell lines, used instead of replay_cmd"]}]}

A stats.json next to it with a non-zero `fuzzer_exit` and no findings counts
as an infrastructure failure of that job.

All GitHub calls go through `gh api`, so GH_TOKEN must be set.
"""
import argparse
import base64
import datetime as dt
import json
import os
from pathlib import Path
import re
import subprocess
import sys

CRASH_LABEL = 'fuzz-crash'
INFRA_LABEL = 'fuzz-infra'
LABELS = {
    CRASH_LABEL: ('b60205', 'Crash, leak, timeout or OOM found by nightly fuzzing'),
    INFRA_LABEL: ('fbca04', 'Nightly fuzz did not run or failed without a finding'),
}
INFRA_TITLE = '[fuzz-infra] nightly fuzz infrastructure'
REGRESSION_DIR = 'tests/fuzz/regressions'
BRANCH_PREFIX = 'fuzz-crash/'
MAX_LOG_CHARS = 6000
MAX_INPUT_BYTES = 1 << 20

SIGNATURE_RE = re.compile(r'^[0-9a-f]{10}$')
TARGET_RE = re.compile(r'^[A-Za-z0-9_]{1,64}$')


class GhError(RuntimeError):
    def __init__(self, status, message):
        super().__init__(message)
        self.status = status


class Gh:
    """The few REST calls the report needs, through `gh api`."""

    def __init__(self, repo):
        self.repo = repo

    def api(self, method, path, body=None, paginate=False):
        command = ['gh', 'api', '--method', method, '-H', 'Accept: application/vnd.github+json']
        if paginate:
            command += ['--paginate', '--slurp']
        command.append(path if path.startswith('/') else f'repos/{self.repo}/{path}')
        if body is not None:
            command += ['--input', '-']
        result = subprocess.run(command, input=json.dumps(body) if body is not None else None,
                                capture_output=True, text=True)
        if result.returncode != 0:
            match = re.search(r'HTTP (\d{3})', result.stderr)
            raise GhError(int(match.group(1)) if match else None,
                          f'gh api {method} {path}: {result.stderr.strip()}')
        if not result.stdout.strip():
            return None
        data = json.loads(result.stdout)
        if paginate:
            return [item for page in data for item in page]
        return data

    def ensure_label(self, name, color, description):
        try:
            self.api('POST', 'labels', {'name': name, 'color': color, 'description': description})
        except GhError as error:
            if error.status != 422:  # 422: the label already exists
                raise

    def issues(self, label, state):
        items = self.api('GET', f'issues?labels={label}&state={state}&per_page=100', paginate=True)
        return [item for item in items if 'pull_request' not in item]

    def create_issue(self, title, body, labels):
        return self.api('POST', 'issues', {'title': title, 'body': body, 'labels': labels})

    def comment(self, number, body):
        self.api('POST', f'issues/{number}/comments', {'body': body})

    def set_state(self, number, state, reason=None):
        body = {'state': state}
        if reason:
            body['state_reason'] = reason
        self.api('PATCH', f'issues/{number}', body)

    def branch_exists(self, branch):
        try:
            self.api('GET', f'git/ref/heads/{branch}')
            return True
        except GhError as error:
            if error.status == 404:
                return False
            raise

    def create_branch_with_file(self, branch, base_sha, path, content, message):
        """One commit on base_sha that adds path, then refs/heads/<branch>."""
        blob = self.api('POST', 'git/blobs', {
            'content': base64.b64encode(content).decode('ascii'), 'encoding': 'base64'})
        base_tree = self.api('GET', f'git/commits/{base_sha}')['tree']['sha']
        tree = self.api('POST', 'git/trees', {'base_tree': base_tree, 'tree': [
            {'path': path, 'mode': '100644', 'type': 'blob', 'sha': blob['sha']}]})
        commit = self.api('POST', 'git/commits', {
            'message': message, 'tree': tree['sha'], 'parents': [base_sha]})
        self.api('POST', 'git/refs', {'ref': f'refs/heads/{branch}', 'sha': commit['sha']})


def fence(text):
    longest = max((len(run) for run in re.findall(r'`+', text)), default=0)
    ticks = '`' * max(3, longest + 1)
    return f'{ticks}\n{text}\n{ticks}'


def one_line(text, limit=80):
    text = ' '.join(str(text).split())
    return text if len(text) <= limit else text[:limit - 3] + '...'


def load_findings(root):
    """Returns (reports by job name, findings, problems)."""
    reports, findings, problems = {}, [], []
    for path in sorted(Path(root).rglob('findings.json')):
        try:
            report = json.loads(path.read_text())
            items = report.get('findings', [])
            assert isinstance(items, list)
            job = str(report.get('job') or f'libfuzzer-{report["target"]}')
        except (ValueError, KeyError, AssertionError, AttributeError) as error:
            problems.append(f'{path}: unreadable findings.json ({error!r})')
            continue
        reports[job] = report
        needs = str(report.get('needs') or 'libfuzzer')
        if not items:
            try:
                code = str(json.loads((path.parent / 'stats.json').read_text()).get('fuzzer_exit'))
            except (OSError, ValueError, AttributeError):
                code = None
            if code not in (None, 'None', '0'):
                problems.append(f'job `{job}`: fuzzer exited {code} and left no crash, leak, timeout or oom input')
        for item in items:
            signature = str(item.get('signature', ''))
            target = str(item.get('target') or report.get('target', ''))
            if not SIGNATURE_RE.match(signature) or not TARGET_RE.match(target):
                problems.append(f'{path}: finding with bad signature {signature!r} or target {target!r}')
                continue
            entry = {
                'job': job, 'needs': needs, 'signature': signature, 'target': target,
                'artifact': str(report.get('artifact') or f'fuzz-{target}'),
                'kind': one_line(item.get('error_type') or item.get('kind') or 'crash', 60),
                'top_frame': one_line(item.get('top_function') or item.get('top_frame') or 'unknown frame', 60),
                'log': str(item.get('log_head') or item.get('log') or ''),
                'replay_cmd': str(item.get('replay_cmd') or ''),
                'reproduce': item.get('reproduce'),
                'input_path': None,
            }
            if item.get('input'):
                candidate = (path.parent / str(item['input'])).resolve()
                if path.parent.resolve() in candidate.parents and candidate.is_file():
                    entry['input_path'] = candidate
                else:
                    problems.append(f'{path}: input {item["input"]!r} missing or outside the artifact')
            findings.append(entry)
    return reports, findings, problems


def infra_failures(needs, expect, reports, findings):
    """Jobs that failed or are missing without a finding to explain it."""
    failures = []
    with_findings = {item['needs'] for item in findings}
    for key, value in sorted(needs.items()):
        result = value.get('result')
        if result in ('failure', 'cancelled') and key not in with_findings:
            failures.append(f'job `{key}`: {result} with no finding')
    for job in expect:
        if job not in reports:
            failures.append(f'job `{job}`: no findings.json (did not finish)')
    return failures


class Context:
    def __init__(self, args):
        self.repo = args.repo
        self.sha = args.sha
        self.run_id = args.run_id
        self.mode = args.mode
        self.ref = getattr(args, 'ref', None) or 'next_cpp26'
        server = args.server_url.rstrip('/')
        self.run_url = getattr(args, 'run_url', None) or f'{server}/{self.repo}/actions/runs/{self.run_id}'
        self.push_branches = not getattr(args, 'no_branches', False)
        self.commit_url = f'{server}/{self.repo}/commit/{self.sha}'
        self.today = dt.datetime.now(dt.timezone.utc).strftime('%Y-%m-%d')

    def stamp(self):
        return f'{self.mode} run [{self.run_id}]({self.run_url}) on {self.today}, {self.ref} [{self.sha[:10]}]({self.commit_url})'


def issue_title(finding):
    return f'[fuzz] {finding["target"]}: {finding["kind"]} in {finding["top_frame"]} ({finding["signature"]})'


def reproduce_lines(ctx, finding):
    if finding['reproduce']:
        return [str(line) for line in finding['reproduce']]
    name = finding['input_path'].name if finding['input_path'] else '<input>'
    command = finding['replay_cmd'] or f'build/fuzz/bin/Debug/fuzz_{finding["target"]} {name}'
    command = command.replace(name, f'fuzz-repro/{name}') if name in command else f'{command} fuzz-repro/{name}'
    return [
        f'gh run download {ctx.run_id} --repo {ctx.repo} -n {finding["artifact"]} -D fuzz-repro-artifact',
        f'mkdir -p fuzz-repro && cp "$(find fuzz-repro-artifact -name {name} | head -n 1)" fuzz-repro/',
        f'git checkout {ctx.sha}',
        '# build build/fuzz, build/fuzz-resource and build/xo-docker as in .github/workflows/fuzz.yml, then:',
        f'build/xo-docker {command}',
        f'# without libFuzzer, on any compiler: fuzz_{finding["target"]}_replay fuzz-repro/{name}',
    ]


def finding_details(ctx, finding, count, branch_note):
    log = finding['log'][:MAX_LOG_CHARS]
    lines = [
        f'- Seen in: {ctx.stamp()}',
        f'- Job: `{finding["job"]}`, target `{finding["target"]}`, type `{finding["kind"]}`',
        f'- Inputs with this signature in this run: {count}',
        f'- Artifact: `{finding["artifact"]}`',
        f'- {branch_note}',
        '',
        'Report (head):',
        fence(log or '(no log captured)'),
        '',
        'Reproduce:',
        fence('\n'.join(reproduce_lines(ctx, finding))),
    ]
    return '\n'.join(lines)


def regression_branch(gh, ctx, finding, actions):
    branch = BRANCH_PREFIX + finding['signature']
    path = f'{REGRESSION_DIR}/{finding["target"]}/{finding["signature"]}'
    if not ctx.push_branches:
        return 'Regression branch: not pushed (drill run)'
    if finding['input_path'] is None:
        return 'Regression branch: not pushed, the finding has no input file'
    if finding['input_path'].stat().st_size > MAX_INPUT_BYTES:
        return f'Regression branch: not pushed, the input is over {MAX_INPUT_BYTES} bytes'
    if gh.branch_exists(branch):
        return f'Regression branch: `{branch}` already exists (not changed)'
    message = (f'fuzz: regression input {finding["target"]}/{finding["signature"]}\n\n'
               f'Found by the {ctx.mode} fuzz run {ctx.run_url}\n'
               f'Fix on this branch and keep {path} so CTest replays it.\n')
    gh.create_branch_with_file(branch, ctx.sha, path, finding['input_path'].read_bytes(), message)
    actions.append(f'pushed `{branch}` with `{path}`')
    return f'Regression branch: `{branch}` adds `{path}` on top of {ctx.sha[:10]}'


def report_findings(gh, ctx, findings, actions):
    groups = {}
    for finding in findings:
        groups.setdefault(finding['signature'], []).append(finding)
    if not groups:
        return
    crash_issues = gh.issues(CRASH_LABEL, 'all')
    for signature, group in groups.items():
        first = next((item for item in group if item['input_path'] is not None), group[0])
        branch_note = regression_branch(gh, ctx, first, actions)
        details = finding_details(ctx, first, len(group), branch_note)
        marker = f'({signature})'
        matches = [issue for issue in crash_issues if issue['title'].endswith(marker)]
        open_issue = next((issue for issue in matches if issue['state'] == 'open'), None)
        if open_issue:
            gh.comment(open_issue['number'], f'Seen again.\n\n{details}')
            actions.append(f'commented on #{open_issue["number"]} ({signature})')
        elif matches:
            closed = max(matches, key=lambda issue: issue['number'])
            gh.set_state(closed['number'], 'open')
            gh.comment(closed['number'], f'**Regression**: this signature is back after the issue was closed.\n\n{details}')
            actions.append(f'reopened #{closed["number"]} as a regression ({signature})')
        else:
            body = (f'Nightly fuzzing found a new crash signature `{signature}`.\n\n{details}\n\n'
                    f'Fix on `{BRANCH_PREFIX}{signature}` (or copy its input into '
                    f'`{REGRESSION_DIR}/{first["target"]}/`), open the PR against next_cpp26 with '
                    f'`Fixes #<this issue>`, and let CTest replay the input. This issue is not closed automatically.')
            issue = gh.create_issue(issue_title(first), body, [CRASH_LABEL])
            actions.append(f'opened #{issue["number"]} ({signature})')


def open_infra_issue(gh):
    issues = gh.issues(INFRA_LABEL, 'open')
    return min(issues, key=lambda issue: issue['number']) if issues else None


def infra_comment(gh, body, actions):
    issue = open_infra_issue(gh)
    if issue:
        gh.comment(issue['number'], body)
        actions.append(f'commented on #{issue["number"]} ({INFRA_LABEL})')
        return issue['number']
    issue = gh.create_issue(INFRA_TITLE, 'Rolling issue for nightly fuzz runs that failed without a '
                            'finding, or did not run. The first all-green nightly closes it.\n\n' + body,
                            [INFRA_LABEL])
    actions.append(f'opened #{issue["number"]} ({INFRA_LABEL})')
    return issue['number']


def run_report(gh, args):
    ctx = Context(args)
    needs = json.loads(Path(args.needs_json).read_text()) if args.needs_json else {}
    expect = [job for job in (args.expect or '').split(',') if job]
    reports, findings, problems = load_findings(args.findings_dir)
    for name, (color, description) in LABELS.items():
        gh.ensure_label(name, color, description)
    actions = []
    report_findings(gh, ctx, findings, actions)
    failures = infra_failures(needs, expect, reports, findings) + problems
    if failures:
        infra_comment(gh, f'Failures without a finding in {ctx.stamp()}:\n\n'
                      + '\n'.join(f'- {line}' for line in failures), actions)
    elif not findings and ctx.mode == 'nightly':
        issue = open_infra_issue(gh)
        if issue:
            gh.comment(issue['number'], f'Recovered: every job passed in {ctx.stamp()}.')
            gh.set_state(issue['number'], 'closed', 'completed')
            actions.append(f'closed #{issue["number"]} ({INFRA_LABEL}): recovered')
    return {'findings': len(findings), 'signatures': len({item['signature'] for item in findings}),
            'infra_failures': failures, 'actions': actions}


def write_summary(title, result):
    path = os.environ.get('GITHUB_STEP_SUMMARY')
    lines = [f'### {title}', '', '```json', json.dumps(result, indent=2, default=str), '```', '']
    if path:
        with open(path, 'a') as summary:
            summary.write('\n'.join(lines))


def main(argv=None):
    env = os.environ
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--repo', default=env.get('GITHUB_REPOSITORY'))
    sub = parser.add_subparsers(dest='command', required=True)
    report = sub.add_parser('report', help='file issues and regression branches for a run')
    report.add_argument('--findings-dir', required=True)
    report.add_argument('--needs-json', help='file with toJSON(needs)')
    report.add_argument('--expect', help='comma-separated job names that must upload findings.json')
    report.add_argument('--mode', required=True, choices=['nightly', 'manual'])
    report.add_argument('--sha', default=env.get('GITHUB_SHA'))
    report.add_argument('--ref', default=env.get('GITHUB_REF_NAME'))
    report.add_argument('--run-id', default=env.get('GITHUB_RUN_ID'))
    report.add_argument('--server-url', default=env.get('GITHUB_SERVER_URL', 'https://github.com'))
    report.add_argument('--run-url', help='link for the run (default: the Actions run of --run-id)')
    report.add_argument('--no-branches', action='store_true', help='drill: do not push fuzz-crash branches')
    args = parser.parse_args(argv)
    if not args.repo:
        parser.error('--repo or GITHUB_REPOSITORY is required')
    if not args.sha or not args.run_id:
        parser.error('--sha and --run-id are required outside Actions')
    result = run_report(Gh(args.repo), args)
    write_summary('Fuzz report', result)
    json.dump(result, sys.stdout, indent=2, default=str)
    print()
    return 0


if __name__ == '__main__':
    sys.exit(main())
