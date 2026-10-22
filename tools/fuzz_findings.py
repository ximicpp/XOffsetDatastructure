#!/usr/bin/env python3
"""Turn one libFuzzer job's output into findings.json and a corpus summary.

Used by .github/workflows/fuzz.yml on the runner host. Commands that need the
fuzzer binary run through a wrapper (build/xo-docker) that starts them in the
P2996 image with the workspace at /workspace, so every path passed here is
relative to the workspace root.

  findings  replay every crash/leak/timeout/oom artifact, compute a signature,
            minimize one crash or leak input per signature, write
            <out>/findings.json and <out>/<target>-<signature>
  summary   collect corpus and coverage numbers into <out>/stats.json, the job
            summary and one ::notice line

Signature: sha1 of target, error type and the first three stack frames that
belong to this repository, first 10 hex digits.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

KINDS = ('crash', 'leak', 'timeout', 'oom')
REPO_ROOT = '/workspace/'
NOT_REPO = ('/workspace/build/', '/workspace/external/')
LOG_LINES = 60

FRAME = re.compile(r'^\s*#(\d+)\s+0x[0-9a-fA-F]+\s+in\s+(.*)\s+(\S+)$')
BUILD_ID = re.compile(r'\s+\(BuildId: [0-9a-fA-F]+\)\s*$')


def _mask(text, quotes=False):
    text = re.sub(r'0x[0-9a-fA-F]+', 'X', text)
    text = re.sub(r'\d+', 'N', text)
    if quotes:
        text = re.sub(r"'[^']*'", "'…'", text)
        text = re.sub(r'\([^()]*\)', '(…)', text)
    text = ' '.join(text.split())
    return text[:80]


def error_type(lines):
    """(type, index of the line that names it), or (None, None)."""
    rules = (
        (re.compile(r'v2 fuzz FAILED: (.*)'), lambda m: 'diff: ' + _mask(m.group(1), quotes=True)),
        (re.compile(r'ERROR: AddressSanitizer: (\S+)'), lambda m: 'asan: ' + m.group(1)),
        (re.compile(r'ERROR: LeakSanitizer: detected memory leaks'), lambda m: 'leak'),
        (re.compile(r'runtime error: (.*)'), lambda m: 'ubsan: ' + _mask(m.group(1))),
        (re.compile(r'ERROR: libFuzzer: ([a-z -]+)'), lambda m: m.group(1).strip()),
    )
    for pattern, name in rules:
        for i, line in enumerate(lines):
            m = pattern.search(line)
            if m:
                return name(m), i
    return None, None


def function_name(raw):
    name = raw.replace('(anonymous namespace)', '{anon}').replace('operator()', 'operator<call>')
    depth = 0
    for i, ch in enumerate(name):
        if ch == '<':
            depth += 1
        elif ch == '>':
            depth = max(0, depth - 1)
        elif ch == '(' and depth == 0:
            return name[:i]
    return name.strip()


def repo_frames(lines, start, count=3):
    """First `count` in-repository function names of the first stack at or after `start`."""
    frames, seen_stack = [], False
    for line in lines[start:]:
        m = FRAME.match(BUILD_ID.sub('', line))
        if not m:
            if seen_stack:
                break
            continue
        seen_stack = True
        location = m.group(3)
        if location.startswith(REPO_ROOT) and not location.startswith(NOT_REPO):
            frames.append(function_name(m.group(2)))
            if len(frames) == count:
                break
    return frames


def signature(target, etype, frames):
    key = '|'.join([target, etype or ''] + frames)
    return hashlib.sha1(key.encode()).hexdigest()[:10]


def classify(target, kind, log_text):
    lines = log_text.splitlines()
    etype, at = error_type(lines)
    frames = repo_frames(lines, at) if at is not None else []
    if etype is None:
        etype = kind + ' (not reproduced)'
    head_from = max(0, at - 2) if at is not None else 0
    return {
        'error_type': etype,
        'top_frames': frames,
        'top_function': frames[0] if frames else '',
        'signature': signature(target, etype, frames),
        'reproduced': at is not None,
        'log_head': '\n'.join(lines[head_from:head_from + LOG_LINES]),
    }


def artifact_kind(target, name):
    rest = name[len(target) + 1:] if name.startswith(target + '-') else name
    kind = rest.split('-', 1)[0]
    return kind if kind in KINDS else None


def run(wrapper, argv, log_path, limit):
    cmd = [wrapper, 'timeout', '-s', 'KILL', str(limit)] + argv
    with open(log_path, 'w') as log:
        proc = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT)
    return proc.returncode, Path(log_path).read_text(errors='replace')


def cmd_findings(args):
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    work = out / 'replay'
    work.mkdir(exist_ok=True)
    flags = args.flags.split()
    artifacts = sorted(Path(args.artifacts).glob(args.target + '-*')) if Path(args.artifacts).is_dir() else []
    ignored = [a.name for a in artifacts if artifact_kind(args.target, a.name) is None]
    candidates = [a for a in artifacts if artifact_kind(args.target, a.name) is not None]

    groups = {}
    for n, art in enumerate(candidates):
        kind = artifact_kind(args.target, art.name)
        if n < args.max_replays:
            _, text = run(args.wrapper, [args.bin] + flags + [str(art)], work / (art.name + '.log'), 300)
        else:
            text = ''
        info = classify(args.target, kind, text)
        group = groups.setdefault(info['signature'], dict(info, kind=kind, artifacts=[], source=str(art)))
        group['artifacts'].append(art.name)

    minimized = 0
    for sig, group in groups.items():
        group['minimized'] = False
        source = Path(group['source'])
        if group['kind'] in ('crash', 'leak') and group['reproduced'] and minimized < args.max_minimize:
            minimized += 1
            small = work / (source.name + '.min')
            run(args.wrapper, [args.bin, '-minimize_crash=1', '-max_total_time=%d' % args.minimize_seconds,
                               '-exact_artifact_path=' + str(small)] + flags + [str(source)],
                work / (source.name + '.minimize.log'), args.minimize_seconds + 120)
            if small.is_file() and small.stat().st_size > 0:
                _, text = run(args.wrapper, [args.bin] + flags + [str(small)], work / (small.name + '.log'), 300)
                again = classify(args.target, group['kind'], text)
                if again['signature'] == sig:
                    group.update(again)
                    group['minimized'] = True
                    source = small
        name = '%s-%s' % (args.target, sig)
        shutil.copyfile(source, out / name)
        group['input'] = name
        group['replay_cmd'] = ' '.join([args.bin] + flags + [name])
        del group['source']

    findings = sorted(groups.values(), key=lambda g: (g['kind'], g['signature']))
    doc = {
        'schema': 1,
        'target': args.target,
        'mode': os.environ.get('XOFFSET_FUZZ_MODE', ''),
        'sha': os.environ.get('GITHUB_SHA', ''),
        'ref': os.environ.get('GITHUB_REF_NAME', ''),
        'run_id': os.environ.get('GITHUB_RUN_ID', ''),
        'run_url': '%s/%s/actions/runs/%s' % (os.environ.get('GITHUB_SERVER_URL', ''),
                                              os.environ.get('GITHUB_REPOSITORY', ''),
                                              os.environ.get('GITHUB_RUN_ID', '')),
        'artifact': 'fuzz-' + args.target,
        'findings': findings,
        'ignored_artifacts': ignored,
    }
    (out / 'findings.json').write_text(json.dumps(doc, indent=2, ensure_ascii=False) + '\n')
    for f in findings:
        print('::error title=fuzz %s %s::%s in %s (%s), %d input(s)%s' % (
            args.target, f['kind'], f['error_type'], f['top_function'] or '?', f['signature'],
            len(f['artifacts']), ', minimized' if f['minimized'] else ''))
    print('%s: %d finding(s), %d artifact(s) ignored' % (args.target, len(findings), len(ignored)))
    return 0


def dir_stats(path):
    files = [p for p in Path(path).rglob('*') if p.is_file()] if Path(path).is_dir() else []
    return len(files), sum(p.stat().st_size for p in files)


def last_status(text):
    """execs, cov, ft, exec/s from the last libFuzzer status line."""
    result = {}
    for line in text.splitlines():
        m = re.match(r'^#(\d+):?\s.*\bcov: (\d+) ft: (\d+)', line)
        if m:
            result = {'execs': int(m.group(1)), 'cov': int(m.group(2)), 'ft': int(m.group(3))}
            e = re.search(r'exec/s:? (\d+)', line)
            if e:
                result['exec_per_sec'] = int(e.group(1))
    m = re.findall(r'stat::number_of_executed_units: (\d+)', text)
    if m:
        result['execs'] = int(m[-1])
    m = re.findall(r'stat::average_exec_per_sec:\s+(\d+)', text)
    if m:
        result['exec_per_sec'] = int(m[-1])
    return result


def read(path):
    p = Path(path)
    return p.read_text(errors='replace') if p.is_file() else ''


def cmd_summary(args):
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    before = read(args.before).split()
    files, size = dir_stats(args.corpus)
    fuzz = last_status(read(args.log))
    inited = re.search(r'INITED cov: (\d+) ft: (\d+)', read(args.corpus_log))
    merge = re.search(r'MERGE-OUTER: (\d+) new files', read(args.merge_log))
    findings = json.loads(read(args.findings) or '{"findings": []}')['findings']
    stats = {
        'target': args.target,
        'mode': args.mode,
        'seconds': args.seconds,
        'corpus_source': ' '.join(before[2:]) if len(before) > 2 else 'none',
        'corpus_before_files': int(before[0]) if before else 0,
        'corpus_before_bytes': int(before[1]) if len(before) > 1 else 0,
        'corpus_files': files,
        'corpus_bytes': size,
        'corpus_cov': int(inited.group(1)) if inited else None,
        'corpus_ft': int(inited.group(2)) if inited else None,
        'merge_new_files': int(merge.group(1)) if merge else None,
        'fuzzer_exit': read(args.exit_file).strip() or None,
        'findings': len(findings),
    }
    stats.update({'fuzz_' + k: v for k, v in fuzz.items()})
    (out / 'stats.json').write_text(json.dumps(stats, indent=2) + '\n')

    def show(v):
        return '–' if v is None else str(v)
    rows = [
        ('mode / seconds', '%s / %s' % (args.mode, args.seconds)),
        ('corpus restored from', stats['corpus_source']),
        ('corpus before → after (files)', '%d → %d' % (stats['corpus_before_files'], files)),
        ('corpus before → after (bytes)', '%d → %d' % (stats['corpus_before_bytes'], size)),
        ('corpus cov / ft (after merge)', '%s / %s' % (show(stats['corpus_cov']), show(stats['corpus_ft']))),
        ('executions', show(stats.get('fuzz_execs'))),
        ('exec/s (last status)', show(stats.get('fuzz_exec_per_sec'))),
        ('fuzzer exit code', show(stats['fuzzer_exit'])),
        ('findings', str(len(findings))),
    ]
    md = ['### libFuzzer `%s`' % args.target, '', '| | |', '| --- | --- |']
    md += ['| %s | %s |' % row for row in rows]
    if findings:
        md += ['', '| kind | type | top function | signature | inputs | minimized |', '| --- | --- | --- | --- | --- | --- |']
        md += ['| %s | %s | `%s` | `%s` | %d | %s |' % (f['kind'], f['error_type'].replace('|', '\\|'),
                                                     f['top_function'] or '?', f['signature'],
                                                     len(f['artifacts']), 'yes' if f['minimized'] else 'no')
               for f in findings]
    summary = os.environ.get('GITHUB_STEP_SUMMARY')
    if summary:
        with open(summary, 'a') as fh:
            fh.write('\n'.join(md) + '\n')
    print('\n'.join(md))
    print('::notice title=fuzz %s %s::source=%s corpus %d->%d files %d->%d bytes cov=%s ft=%s execs=%s exec/s=%s exit=%s findings=%d' % (
        args.target, args.mode, stats['corpus_source'], stats['corpus_before_files'], files,
        stats['corpus_before_bytes'], size, show(stats['corpus_cov']), show(stats['corpus_ft']),
        show(stats.get('fuzz_execs')), show(stats.get('fuzz_exec_per_sec')), show(stats['fuzzer_exit']),
        len(findings)))
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    f = sub.add_parser('findings')
    f.add_argument('--target', required=True)
    f.add_argument('--bin', required=True, help='fuzzer binary, workspace-relative')
    f.add_argument('--flags', default='', help='libFuzzer flags for replay and minimization')
    f.add_argument('--artifacts', required=True)
    f.add_argument('--out', required=True)
    f.add_argument('--wrapper', default='build/xo-docker')
    f.add_argument('--max-replays', type=int, default=20)
    f.add_argument('--max-minimize', type=int, default=3)
    f.add_argument('--minimize-seconds', type=int, default=120)
    s = sub.add_parser('summary')
    s.add_argument('--target', required=True)
    s.add_argument('--mode', required=True)
    s.add_argument('--seconds', required=True)
    s.add_argument('--log', required=True)
    s.add_argument('--before', required=True, help='file: "<files> <bytes> <source...>"')
    s.add_argument('--corpus', required=True)
    s.add_argument('--corpus-log', required=True)
    s.add_argument('--merge-log', required=True)
    s.add_argument('--exit-file', required=True)
    s.add_argument('--findings', required=True)
    s.add_argument('--out', required=True)
    args = parser.parse_args(argv)
    return cmd_findings(args) if args.command == 'findings' else cmd_summary(args)


if __name__ == '__main__':
    sys.exit(main())
