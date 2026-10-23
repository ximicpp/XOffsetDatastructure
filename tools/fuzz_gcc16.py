#!/usr/bin/env python3
"""findings.json for the GCC 16 nightly fuzz jobs (gcc16-diff, gcc16-replay).

Used by .github/workflows/fuzz.yml on the runner host, next to
tools/fuzz_findings.py (the libFuzzer jobs), whose error types and signatures
it reuses so the report job reads both the same way. Binaries run through a
wrapper (build/gcc-docker) that starts them in the GCC 16 image with the
workspace at /workspace; every path passed here is workspace-relative.

  diff    read the logs of the parallel test_v2_fuzz processes; each process
          that failed becomes a finding, with the input wire it dumped
          (XOFFSET_FUZZ_DUMP_DIR) and its `test_v2_fuzz --replay` command
  replay  run <target>_replay over corpus, regression and seed inputs in
          chunks; an input that fails becomes a finding and the replay goes
          on with the inputs after it

Both write <out>/findings.json (also with no findings: a job without one died
before it finished) and copy each finding's input next to it as
<target>-<signature>.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fuzz_findings  # noqa: E402

# Builds the target in the GCC 16 image with the job's sanitizer flags and runs `command`.
GCC_REPRO = ('docker run --rm -v "$PWD:/workspace" -w /workspace '
             '-e ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:handle_abort=1 '
             '-e UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 {image} bash -c '
             '\'cmake -S . -B build/gcc16-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ '
             '"-DCMAKE_CXX_FLAGS={flags}" -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined '
             '&& cmake --build build/gcc16-asan --target {target} && {command}\'')


def reproduce(image, target, command):
    flags = ' '.join(os.environ.get('XOFFSET_GCC_SANITIZE', '-fsanitize=address,undefined').split())
    return [GCC_REPRO.format(image=image, flags=flags, target=target, command=command)]


def document(job, findings, extra):
    doc = {
        'schema': 1,
        'job': job,
        'needs': job,
        'mode': os.environ.get('XOFFSET_FUZZ_MODE', ''),
        'sha': os.environ.get('GITHUB_SHA', ''),
        'ref': os.environ.get('GITHUB_REF_NAME', ''),
        'run_id': os.environ.get('GITHUB_RUN_ID', ''),
        'run_url': '%s/%s/actions/runs/%s' % (os.environ.get('GITHUB_SERVER_URL', ''),
                                              os.environ.get('GITHUB_REPOSITORY', ''),
                                              os.environ.get('GITHUB_RUN_ID', '')),
        'artifact': 'fuzz-' + job,
        'findings': sorted(findings, key=lambda f: (f['target'], f['kind'], f['signature'])),
    }
    doc.update(extra)
    return doc


def add_finding(groups, out, target, kind, code, text, source, repro, note):
    """Classify `text`, group by signature, keep the first input of each group."""
    info = fuzz_findings.classify(target, kind, text)
    if kind == 'timeout':
        info['error_type'] = 'timeout'
    elif not info['reproduced']:
        # No sanitizer or differential report: a plain abort or exit code.
        inject = re.search(r'^XOFFSET_FUZZ_INJECT_CRASH: (.*)$', text, re.M)
        info['error_type'] = 'inject: ' + inject.group(1) if inject else '%s (exit %s)' % (kind, code)
        info['reproduced'] = True
    info['signature'] = fuzz_findings.signature(target, info['error_type'], info['top_frames'])
    sig = info['signature']
    if sig in groups:
        groups[sig]['artifacts'].append(note)
        return groups[sig]
    group = dict(info, target=target, kind=kind, artifacts=[note], minimized=False, reproduce=repro)
    if source and Path(source).is_file():
        name = '%s-%s' % (target, sig)
        shutil.copyfile(source, Path(out) / name)
        group['input'] = name
    groups[sig] = group
    return group


def cmd_diff(args):
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    groups, processes = {}, []
    for i, seed in enumerate(args.seeds.split()):
        log = Path(args.logs) / ('p%d.log' % i)
        text = log.read_text(errors='replace') if log.is_file() else ''
        code = (Path(args.logs) / ('p%d.exit' % i)).read_text().strip() if (Path(args.logs) / ('p%d.exit' % i)).is_file() else 'missing'
        done = re.search(r'^test_v2_fuzz: seed=\d+ .*inconsistencies=0$', text, re.M)
        processes.append({'seed': seed, 'exit': code, 'completed': bool(done),
                          'summary': done.group(0) if done else ''})
        if code == '0' and done:
            continue
        replay = re.search(r'^v2 fuzz replay: (test_v2_fuzz --replay \d+ b?\d+)$', text, re.M)
        dump = re.search(r'^v2 fuzz dump: (\S+\.wire)', text, re.M)
        kind = 'timeout' if code in ('124', '137') else 'crash'
        command = replay.group(1) if replay else 'test_v2_fuzz %s %s' % (args.cases, seed)
        group = add_finding(groups, out, 'v2_wire', kind, code, text, dump.group(1) if dump else None,
                            reproduce(args.image, 'test_v2_fuzz', 'build/gcc16-asan/bin/Debug/' + command),
                            'seed %s (exit %s)' % (seed, code))
        group['replay_cmd'] = command
    doc = document('gcc16-diff', list(groups.values()),
                   {'cases_per_process': int(args.cases), 'processes': processes})
    (out / 'findings.json').write_text(json.dumps(doc, indent=2, ensure_ascii=False) + '\n')
    report(doc, out)
    return 0


def inputs_of(paths):
    found = []
    for p in map(Path, paths):
        if p.is_file():
            found.append(p)
        elif p.is_dir():
            found += sorted(q for q in p.iterdir() if q.is_file() and not q.name.startswith('.'))
    return found


def cmd_replay(args):
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    work = out / 'replay'
    work.mkdir(exist_ok=True)
    groups, counts = {}, {}
    for spec in args.spec:
        target, binary, *dirs = spec.split(':')
        todo = inputs_of(dirs)
        counts[target] = {'inputs': len(todo), 'failed': 0, 'runs': 0}
        while todo and counts[target]['runs'] < args.max_runs:
            chunk = todo[:args.chunk]
            n = counts[target]['runs'] = counts[target]['runs'] + 1
            log = work / ('%s-%d.log' % (target, n))
            code, text = fuzz_findings.run(args.wrapper, [binary] + [str(p) for p in chunk], log, args.timeout)
            if code == 0:
                todo = todo[len(chunk):]
                continue
            # The replay names each input before running it: the last one named failed.
            named = re.findall(r'^replay: (.+) \(\d+ bytes\)$', text, re.M)
            if not named:
                raise SystemExit('%s: replay exited %d before its first input; see %s' % (target, code, log))
            failed = Path(named[-1])
            at = [str(p) for p in chunk].index(str(failed))
            tail = text[text.rfind('replay: ' + named[-1]):]
            kind = 'timeout' if code in (124, 137, -9) else 'crash'
            name = Path(binary).name
            group = add_finding(groups, out, target, kind, code, tail, failed, [], str(failed))
            # The report job commits the input as tests/fuzz/regressions/<target>/<signature>.
            regression = 'tests/fuzz/regressions/%s/%s' % (target, group['signature'])
            group['reproduce'] = reproduce(args.image, name, 'build/gcc16-asan/bin/Debug/%s %s' % (name, regression))
            group['replay_cmd'] = '%s %s' % (name, regression)
            counts[target]['failed'] += 1
            todo = todo[at + 1:]
        if todo:
            counts[target]['not_replayed'] = len(todo)
    doc = document('gcc16-replay', list(groups.values()), {'inputs': counts})
    (out / 'findings.json').write_text(json.dumps(doc, indent=2, ensure_ascii=False) + '\n')
    report(doc, out)
    incomplete = [t for t, c in counts.items() if c.get('not_replayed')]
    if incomplete:
        print('::error::replay stopped after %d failing runs for %s' % (args.max_runs, ', '.join(incomplete)))
        return 1
    return 0


def report(doc, out):
    for f in doc['findings']:
        print('::error title=%s %s %s::%s in %s (%s), %d occurrence(s)' % (
            doc['job'], f['target'], f['kind'], f['error_type'], f['top_function'] or '?', f['signature'],
            len(f['artifacts'])))
    md = ['### %s' % doc['job'], '']
    if doc['job'] == 'gcc16-diff':
        md += ['%d cases per process' % doc['cases_per_process'], '', '| seed | exit | result |', '| --- | --- | --- |']
        md += ['| %s | %s | %s |' % (p['seed'], p['exit'], p['summary'] or 'FAILED') for p in doc['processes']]
    else:
        md += ['| target | inputs | failed |', '| --- | --- | --- |']
        md += ['| %s | %d | %d |' % (t, c['inputs'], c['failed']) for t, c in doc['inputs'].items()]
    md += ['', 'findings: %d' % len(doc['findings'])]
    md += ['- `%s` %s: %s in `%s`' % (f['signature'], f['target'], f['error_type'], f['top_function'] or '?')
           for f in doc['findings']]
    summary = os.environ.get('GITHUB_STEP_SUMMARY')
    if summary:
        with open(summary, 'a') as fh:
            fh.write('\n'.join(md) + '\n')
    print('\n'.join(md))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    d = sub.add_parser('diff')
    d.add_argument('--logs', required=True, help='directory with p<i>.log and p<i>.exit per process')
    d.add_argument('--seeds', required=True, help='space-separated seeds, process i ran seed i')
    d.add_argument('--cases', required=True)
    d.add_argument('--image', required=True)
    d.add_argument('--out', required=True)
    r = sub.add_parser('replay')
    r.add_argument('--spec', action='append', required=True, help='target:binary:input[:input...]')
    r.add_argument('--image', required=True)
    r.add_argument('--out', required=True)
    r.add_argument('--wrapper', default='build/gcc-docker')
    r.add_argument('--chunk', type=int, default=500)
    r.add_argument('--timeout', type=int, default=900, help='seconds per chunk')
    r.add_argument('--max-runs', type=int, default=40)
    args = parser.parse_args(argv)
    return cmd_diff(args) if args.command == 'diff' else cmd_replay(args)


if __name__ == '__main__':
    sys.exit(main())
