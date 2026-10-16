#!/usr/bin/env python3
"""Freeze OS-layer sources, then run separate throughput, latency and diagnostic windows.

Prepare never starts timing. Each run requires a new output directory. Audit rejects
incomplete runs and detects changed binaries, sources, runtime inputs and samples.
"""
import argparse
import datetime as dt
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
KIB = 1024


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(path, value):
    Path(path).write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')


def command(args):
    return subprocess.run(list(map(str, args)), check=True, text=True, capture_output=True, timeout=180)


def instrument(header):
    replacements = {
        'std::lock_guard<std::mutex> lock(': ('::pool_diagnostics::Lock lock(', 4),
        'if (state.cached[i].maximum_capacity_ != maximum_capacity_) continue;':
            ('++::pool_diagnostics::local.scans;\n        if (state.cached[i].maximum_capacity_ != maximum_capacity_) continue;', 1),
        '::mmap(': ('::pool_diagnostics::mmap_counted(', 2),
        '::mprotect(': ('::pool_diagnostics::mprotect_counted(', 2),
        '::munmap(': ('::pool_diagnostics::munmap_counted(', 1),
    }
    for old, (new, count) in replacements.items():
        if header.count(old) != count:
            raise RuntimeError(f'instrumentation pattern mismatch: {old}')
        header = header.replace(old, new)
    return header


def identity(paths):
    result = {}
    for name in paths:
        p = Path(name).resolve(strict=True)
        if not p.is_file():
            raise RuntimeError(f'identity input is not a file: {name}')
        result[str(p)] = sha(p)
    return result


def verify_identity(manifest):
    for path, expected in manifest.items():
        if not Path(path).is_file() or sha(path) != expected:
            raise RuntimeError(f'identity mismatch: {path}')


def prepare(args):
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    meta = {'schema': 1, 'status': 'running', 'created_at': dt.datetime.now().astimezone().isoformat()}
    write(out / 'metadata.json', meta)
    try:
        compiler = Path(shutil.which(args.compiler) or args.compiler)
        sources = out / 'source'
        sources.mkdir()
        for name in ('xoffset_virtual_memory.hpp', 'tools/bench_vm_pool_matrix.cpp', 'tools/run_vm_pool_matrix.py'):
            shutil.copy2(ROOT / name, sources / Path(name).name)
        diagnostic = out / 'diagnostic'
        diagnostic.mkdir()
        (diagnostic / 'xoffset_virtual_memory.hpp').write_text(instrument((sources / 'xoffset_virtual_memory.hpp').read_text()))
        shutil.copy2(sources / 'bench_vm_pool_matrix.cpp', diagnostic / 'bench_vm_pool_matrix.cpp')
        flags = ['-O3', '-DNDEBUG', '-std=c++17', '-pthread']
        inputs = [compiler]
        if platform.system() == 'Darwin':
            if not args.sdk:
                raise RuntimeError('Darwin requires an explicit --sdk')
            sdk = Path(args.sdk).resolve(strict=True)
            flags += ['-isysroot', str(sdk), '-mmacosx-version-min=13.0']
            inputs += [sdk / 'SDKSettings.json', '/System/Library/CoreServices/SystemVersion.plist']
        if args.library_dir:
            lib = Path(args.library_dir).resolve(strict=True)
            flags += ['-L' + str(lib), '-Wl,-rpath,' + str(lib)]
            inputs += [lib / ('libc++.1.dylib' if platform.system() == 'Darwin' else 'libc++.so.1'),
                       lib / ('libc++abi.1.dylib' if platform.system() == 'Darwin' else 'libc++abi.so.1')]
        meta['runtime_identity'] = identity(inputs)
        meta['compiler_version'] = command([compiler, '--version']).stdout
        meta['commands'] = {}
        for kind, source in [('timing', sources), ('diagnostic', diagnostic)]:
            cmd = [compiler] + flags + (['-DXOFFSET_POOL_DIAGNOSTICS=1'] if kind == 'diagnostic' else [])
            cmd += ['-I', source, source / 'bench_vm_pool_matrix.cpp', '-o', out / (kind + '-bench')]
            meta['commands'][kind] = list(map(str, cmd))
            proc = command(cmd)
            (out / f'compile_{kind}.log').write_text(proc.stdout + proc.stderr)
        meta['artifacts'] = {str(p.relative_to(out)): sha(p) for p in out.rglob('*') if p.is_file() and p.name != 'metadata.json'}
        verify_identity(meta['runtime_identity'])
        meta['status'] = 'completed'
        meta['platform'] = platform.platform()
        meta['page_size'] = os.sysconf('SC_PAGE_SIZE')
        meta['notes'] = 'Only listed runtime files are hashed; system shared-cache libraries and all SDK headers are not claimed fully frozen.'
        write(out / 'metadata.json', meta)
    except BaseException as error:
        meta.update(status='failed', error=repr(error))
        write(out / 'metadata.json', meta)
        raise
    print(out)


def verify_prepared(out, runtime=True):
    meta = json.loads((out / 'metadata.json').read_text())
    if meta.get('status') != 'completed':
        raise RuntimeError('prepared binaries are incomplete')
    for name, expected in meta['artifacts'].items():
        path = out / name
        if not path.is_file() or sha(path) != expected:
            raise RuntimeError(f'prepared artifact changed: {name}')
    if runtime:
        verify_identity(meta['runtime_identity'])
    return meta


def cases(kind):
    if kind in ('matrix', 'diagnostic', 'latency'):
        counts = (1, 8, 64, 256) if kind != 'latency' else (1, 256)
        threads = (1, 2, 4, 8) if kind != 'latency' else (1, 8)
        return [dict(workload='matrix', distribution=d, cache=c, threads=t, cycles=16, idle_ms=0,
                     measurement='latency' if kind == 'latency' else 'throughput', policies=['none', 'shared', 'local'],
                     id=f'{d}-c{c}-t{t}')
                for d in ('fixed', 'mixed', 'bimodal', 'phase') for c in counts for t in threads]
    if kind == 'lifecycle':
        result = [dict(workload='idle_pool', distribution='fixed', cache=16, threads=1, cycles=3, idle_ms=idle,
                       measurement='throughput', policies=['none', 'retain', 'clear'], id=f'idle-pool-{idle}') for idle in (0, 10, 100)]
        result += [dict(workload='live_reclaim', distribution='fixed', cache=1, threads=1, cycles=3, idle_ms=idle,
                        measurement='throughput', policies=['retain', 'reclaim'], id=f'live-reclaim-{idle}') for idle in (0, 10, 100)]
        result += [dict(workload='concurrent_clear', distribution='mixed', cache=256, threads=8, cycles=32, idle_ms=0,
                        measurement='throughput', policies=['shared'], id='concurrent-clear'),
                   dict(workload='handle_lifetime', distribution='fixed', cache=1, threads=1, cycles=256, idle_ms=0,
                        measurement='throughput', policies=['shared'], id='handle-lifetime')]
        return result
    raise RuntimeError('unknown run kind')


def controls():
    return [dict(workload='matrix', distribution=d, cache=256, threads=t, cycles=16, idle_ms=0,
                 measurement='throughput', policies=['shared', 'shared'], id=f'aa-{d}-t{t}')
            for d in ('fixed', 'mixed') for t in (1, 8)]


def schedule(case, repeat):
    labels = ['a', 'b'] if case['id'].startswith('aa-') else case['policies']
    endpoints = list(zip(labels, case['policies']))
    offset = repeat % len(endpoints)
    return endpoints[offset:] + endpoints[:offset]


def environment():
    return dict(time=dt.datetime.now().astimezone().isoformat(), load=os.getloadavg(),
                processes=command(['ps', '-Ao', 'pid,pcpu,comm']).stdout,
                platform=platform.platform())


def distribution(values):
    values = sorted(values)
    if not values:
        raise RuntimeError('empty sample group')
    if not all(math.isfinite(x) for x in values):
        raise RuntimeError('nonfinite sample')
    return dict(n=len(values), minimum=min(values), median=statistics.median(values), maximum=max(values))


def summarize(case_list, rows, repeats):
    result = []
    for case in case_list:
        selected = [r for r in rows if r['case_id'] == case['id']]
        labels = ['a', 'b'] if case['id'].startswith('aa-') else case['policies']
        if len(selected) != len(labels) * repeats:
            raise RuntimeError(f'incomplete case: {case["id"]}')
        groups = {}
        for label in labels:
            group = [r for r in selected if r['label'] == label]
            if len(group) != repeats or {r['repeat'] for r in group} != set(range(repeats)):
                raise RuntimeError('duplicate or missing paired sample')
            groups[label] = group
        if len({r['checksum'] for r in selected}) != 1:
            raise RuntimeError('paired trace checksum differs')
        for repeat in range(repeats):
            ordered = sorted((r for r in selected if r['repeat'] == repeat), key=lambda r: r['execution_position'])
            if ([r['execution_position'] for r in ordered] != list(range(len(labels))) or
                    [(r['label'], r['policy']) for r in ordered] != schedule(case, repeat)):
                raise RuntimeError('paired execution order differs from declared schedule')
        entry = dict(case=case, policies={})
        for label, group in groups.items():
            entry['policies'][label] = {'metrics': {k: distribution([r['metrics'][k] for r in group]) for k in group[0]['metrics']}}
        if len(labels) > 1 and all(r['metrics']['timed_ms'] > 0 for r in selected):
            baseline = labels[0]
            entry['paired_baseline_over_policy'] = {}
            for label in labels[1:]:
                ratios = []
                for repeat in range(repeats):
                    a = next(r for r in groups[baseline] if r['repeat'] == repeat)
                    b = next(r for r in groups[label] if r['repeat'] == repeat)
                    ratios.append(a['metrics']['timed_ms'] / b['metrics']['timed_ms'])
                entry['paired_baseline_over_policy'][label] = distribution(ratios)
        result.append(entry)
    return result


def validate_row(row, case, policy, diagnostic):
    for field in ('workload', 'distribution', 'cache', 'threads', 'cycles', 'idle_ms', 'measurement'):
        if row[field] != case[field]:
            raise RuntimeError(f'child returned wrong {field}')
    if row['policy'] != policy or (row['diagnostics'] is not None) != diagnostic:
        raise RuntimeError('child policy/diagnostic mode mismatch')
    if case['workload'] in ('matrix', 'concurrent_clear'):
        m = row['metrics']
        for k, bound in [('blocks', case['cache']), ('reserved_bytes', case['cache'] * 256 * KIB), ('accessible_bytes', case['cache'] * 64 * KIB)]:
            if m['idle_' + k] > bound or m['cleared_' + k] != 0:
                raise RuntimeError('idle budget or clearing invariant failed')
        expected = max(case['cache'], case['threads']) * case['cycles']
        if row['operations'] != expected:
            raise RuntimeError('wrong operation count')
        if policy != 'none' and m['idle_hits'] + m['idle_misses'] != expected:
            raise RuntimeError('pool acquisition accounting mismatch')
        if policy != 'none':
            for key, expected_budget in [('count_budgets', case['cache']), ('reserved_budgets', case['cache'] * 256 * KIB),
                                         ('accessible_budgets', case['cache'] * 64 * KIB)]:
                if sum(row['samples'][key]) != expected_budget:
                    raise RuntimeError('partitioned budgets differ from shared total')
            if policy == 'local' and len(row['samples']['count_budgets']) != case['threads']:
                raise RuntimeError('wrong number of thread-local budget shares')
        if case['measurement'] == 'latency' and any(len(row['samples'][key]) != expected for key in ('acquire_ns', 'release_ns')):
            raise RuntimeError('missing independent latency samples')


def run(args):
    prepared = args.prepared.resolve()
    frozen = verify_prepared(prepared)
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=False)
    meta = dict(schema=1, status='running', prepared=str(prepared), prepared_sha256=sha(prepared / 'metadata.json'),
                kind=args.kind, repeats=args.repeats,
                runtime_identity=frozen['runtime_identity'], timing_scope='sum of parallel acquire/release phase spans; setup, barriers, zero/value verification and fill are outside spans; scheduler skew inside spans remains',
                diagnostics_scope='worker/clearer OS calls and pool locks, including budget stats; acquisition scan visits; excludes final main-thread pool.clear and destructor cleanup; instrumentation changes timing',
                latency_scope='separate per-owner acquire and release clock intervals; synthetic fresh-child samples, not production percentile guarantees')
    write(out / 'metadata.json', meta)
    try:
        meta['environment_start'] = environment()
        selected = cases(args.kind) + (controls() if args.kind == 'matrix' else [])
        if args.case_filter:
            selected = [c for c in selected if args.case_filter in c['id']]
        if not selected:
            raise RuntimeError('no selected cases')
        meta['cases'] = selected
        write(out / 'metadata.json', meta)
        rows = []
        binary = prepared / ('diagnostic-bench' if args.kind == 'diagnostic' else 'timing-bench')
        with (out / 'raw.jsonl').open('w') as raw:
            for case in selected:
                labels = ['a', 'b'] if case['id'].startswith('aa-') else case['policies']
                for repeat in range(args.repeats):
                    order = schedule(case, repeat)
                    for position, (label, policy) in enumerate(order):
                        cmd = [binary]
                        for field in ('workload', 'distribution', 'cache', 'threads', 'cycles', 'idle_ms', 'measurement'):
                            cmd += ['--' + field.replace('_', '-'), case[field]]
                        cmd += ['--policy', policy]
                        proc = command(cmd); row = json.loads(proc.stdout)
                        validate_row(row, case, policy, args.kind == 'diagnostic')
                        row.update(case_id=case['id'], label=label, repeat=repeat, sequence=len(rows), execution_position=position)
                        rows.append(row); raw.write(json.dumps(row, sort_keys=True) + '\n'); raw.flush()
                print(case['id'], flush=True)
        summary = summarize(selected, rows, args.repeats)
        verify_prepared(prepared)
        if sha(prepared / 'metadata.json') != meta['prepared_sha256']:
            raise RuntimeError('prepared metadata changed during run')
        write(out / 'summary.json', summary)
        meta.update(status='completed', environment_end=environment(), rows=len(rows),
                    artifacts={name: sha(out / name) for name in ('raw.jsonl', 'summary.json')})
        write(out / 'metadata.json', meta)
    except BaseException as error:
        meta.update(status='failed', error=repr(error))
        write(out / 'metadata.json', meta)
        raise
    print(out)


def audit(out, prepared=None):
    out = Path(out)
    meta = json.loads((out / 'metadata.json').read_text())
    if meta.get('status') != 'completed':
        raise RuntimeError('cannot audit incomplete/failed window')
    prepared = Path(prepared or meta['prepared'])
    verify_prepared(prepared, runtime=False)
    if sha(prepared / 'metadata.json') != meta['prepared_sha256']:
        raise RuntimeError('prepared metadata identity mismatch')
    for name, expected in meta['artifacts'].items():
        if sha(out / name) != expected:
            raise RuntimeError('window artifact changed: ' + name)
    rows = [json.loads(line) for line in (out / 'raw.jsonl').read_text().splitlines()]
    if len(rows) != meta['rows'] or [r['sequence'] for r in rows] != list(range(len(rows))):
        raise RuntimeError('row count/sequence mismatch')
    for row in rows:
        case = next(c for c in meta['cases'] if c['id'] == row['case_id'])
        if row['policy'] not in case['policies']:
            raise RuntimeError('unknown policy')
        validate_row(row, case, row['policy'], meta['kind'] == 'diagnostic')
    rebuilt = summarize(meta['cases'], rows, meta['repeats'])
    if rebuilt != json.loads((out / 'summary.json').read_text()):
        raise RuntimeError('summary does not match samples')
    return {'status': 'passed', 'rows': len(rows), 'cases': len(meta['cases'])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subs = parser.add_subparsers(dest='action', required=True)
    p = subs.add_parser('prepare'); p.add_argument('--out', type=Path, required=True)
    p.add_argument('--compiler', default='/usr/local/bin/clang++'); p.add_argument('--sdk'); p.add_argument('--library-dir')
    p = subs.add_parser('run'); p.add_argument('--prepared', type=Path, required=True); p.add_argument('--out', type=Path, required=True)
    p.add_argument('--kind', choices=('matrix', 'latency', 'diagnostic', 'lifecycle'), required=True)
    p.add_argument('--repeats', type=int, default=3); p.add_argument('--case-filter')
    p = subs.add_parser('audit'); p.add_argument('--out', type=Path, required=True); p.add_argument('--prepared', type=Path)
    args = parser.parse_args()
    if args.action == 'prepare': prepare(args)
    elif args.action == 'run':
        if args.repeats < 1: parser.error('repeats must be positive')
        run(args)
    else: print(json.dumps(audit(args.out, args.prepared)))


if __name__ == '__main__':
    try: main()
    except (Exception, KeyboardInterrupt) as error:
        print(f'Pool experiment failed: {error}', file=sys.stderr); sys.exit(1)
