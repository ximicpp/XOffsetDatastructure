#!/usr/bin/env python3
"""Freeze two source variants and measure reuse policies in paired trials.

--before DIR --after DIR --prepare-only prepares both binaries; --resume DIR
runs them once the host is quiet. Source snapshots include the unchanged old
helper. Default behavior and opt-in policies are separate comparisons.
"""
import argparse
import datetime as dt
import gzip
import importlib.util
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import time

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('historical_helpers', ROOT / 'tools/run_vm_perf_compare.py')
H = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(H)
MIB = 1024 * 1024


def matrix():
    cases = []
    def add(workload, size, comparison, cycles=8):
        endpoints = {'before': {'binary': 'before', 'enabled': False},
                     'after': {'binary': 'after', 'enabled': False}}
        if comparison == 'opt_in':
            endpoints['before']['binary'] = 'after'
            endpoints['after']['enabled'] = True
        cases.append(dict(id=f'{workload}_{size}_{comparison}', workload=workload,
                          size=size, cycles=cycles, comparison=comparison, endpoints=endpoints))
    for w in ('core_split', 'core_coalesce', 'cache_split'):
        add(w, 1024, 'default')
    for w, n in (('pool_create', 256), ('pool_load', 128), ('automatic', 1_000_000),
                 ('automatic_interleaved', 1_000_000), ('reclaim', 64 * MIB)):
        add(w, n, 'default', 4 if w == 'reclaim' else 8)
        add(w, n, 'opt_in', 4 if w == 'reclaim' else 8)
    return cases


def freeze(args, out):
    common = [args.compiler, '-O3', '-DNDEBUG', '-std=c++26', '-freflection',
              '-fexpansion-statements', '-stdlib=libc++', '-pthread']
    if platform.system() == 'Darwin':
        common += ['-arch', platform.machine(), '-isysroot', args.sdk, '-mmacosx-version-min=10.15']
    deps = {}
    for name in ('boost', 'typelayout'):
        path = ROOT / 'external' / name
        deps[name] = H.output(['git', '-C', path, 'rev-parse', 'HEAD'])
        if H.output(['git', '-C', path, 'status', '--porcelain']):
            raise RuntimeError(f'dirty dependency: {name}')
    for lib in H.BOOST_LIBS:
        common += ['-isystem', ROOT / 'external/boost/libs' / lib / 'include']
    common += ['-isystem', ROOT / 'external/typelayout/include']
    manifests, commands = {}, {}
    for variant in ('before', 'after'):
        snap = out / 'snapshots' / variant
        snap.mkdir(parents=True)
        for name in ('xoffsetdatastructure.hpp', 'xoffset_virtual_memory.hpp'):
            shutil.copy2(getattr(args, variant) / name, snap / name)
        for name in ('bench_vm_followup.cpp', 'bench_vm_compare.cpp'):
            shutil.copy2(ROOT / 'tools' / name, snap / name)
        cmd = common + [f'-DXOFFSET_BENCH_FOLLOWUP={int(variant == "after")}', '-I', snap,
                        snap / 'bench_vm_followup.cpp', '-L/usr/local/lib', '-Wl,-rpath,/usr/local/lib', '-o', out / variant]
        commands[variant] = list(map(str, cmd))
        print(f'Compiling {variant}', flush=True)
        try:
            result = H.command(cmd, text=True)
        except subprocess.CalledProcessError as e:
            (out / f'compile_{variant}.log').write_text(e.stdout + e.stderr)
            raise
        (out / f'compile_{variant}.log').write_text(result.stdout + result.stderr)
        manifests[variant] = {p.name: H.sha(p) for p in snap.iterdir()}
        manifests[variant]['binary_sha256'] = H.sha(out / variant)
    for name in ('run_vm_followup.py', 'run_vm_perf_compare.py'):
        shutil.copy2(ROOT / 'tools' / name, out / 'snapshots' / name)
    return deps, manifests, commands


def verify(out, meta):
    for variant, manifest in meta['manifests'].items():
        for name, expected in manifest.items():
            path = out / variant if name == 'binary_sha256' else out / 'snapshots' / variant / name
            if H.sha(path) != expected:
                raise RuntimeError(f'prepared evidence changed: {path}')
    for name, expected in meta['runners'].items():
        if H.sha(ROOT / 'tools' / name) != expected or H.sha(out / 'snapshots' / name) != expected:
            raise RuntimeError(f'runner changed: {name}')


def summarize(cases, rows):
    summary = []
    for case in cases:
        group = [r for r in rows if r['case_id'] == case['id'] and r['phase'] == 'measured']
        item = dict(case, variants={})
        for variant in ('before', 'after'):
            data = [r for r in group if r['variant'] == variant]
            metrics = sorted(set().union(*(r['metrics'] for r in data)))
            memory = sorted(set().union(*(r['memory'] for r in data)))
            item['variants'][variant] = dict(
                semantic_checksum=data[0]['semantic_checksum'], timing_scope=data[0]['timing_scope'],
                metrics={k: H.distribution([r['metrics'][k] for r in data if k in r['metrics']]) for k in metrics},
                memory={k: {f: H.distribution([r['memory'][k][f] for r in data if k in r['memory']])
                            for f in ('rss_bytes', 'virtual_bytes', 'peak_rss_bytes')} for k in memory})
        pairs = {p: {r['variant']: r for r in group if r['pair'] == p} for p in {r['pair'] for r in group}}
        item['paired_before_over_after'] = H.distribution([
            p['before']['metrics']['elapsed_ms'] / p['after']['metrics']['elapsed_ms'] for p in pairs.values()])
        for metric in ('replacement_ms', 'warm_mean_ms', 'regrow_ms'):
            if all(metric in r['metrics'] and r['metrics'][metric] > 0 for r in group):
                item['paired_' + metric + '_before_over_after'] = H.distribution([
                    p['before']['metrics'][metric] / p['after']['metrics'][metric] for p in pairs.values()])
        summary.append(item)
    return summary


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before', type=Path); p.add_argument('--after', type=Path)
    p.add_argument('--output-dir', type=Path); p.add_argument('--resume', type=Path)
    p.add_argument('--prepare-only', action='store_true'); p.add_argument('--case-filter')
    p.add_argument('--runs', type=int, default=7); p.add_argument('--warmups', type=int, default=1)
    p.add_argument('--compiler', default='/usr/local/bin/clang++')
    p.add_argument('--sdk', default='/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk')
    args = p.parse_args()
    if args.resume:
        out = args.resume.resolve(); meta = json.loads((out / 'metadata.json').read_text()); verify(out, meta)
        if (out / 'raw.jsonl').exists(): raise RuntimeError('output already contains trials')
    else:
        if not args.before or not args.after or not args.output_dir: p.error('before, after, output-dir required')
        if args.runs < 1 or args.warmups < 0: p.error('invalid run counts')
        out = args.output_dir.resolve(); out.mkdir(parents=True, exist_ok=False)
        cases = [c for c in matrix() if not args.case_filter or args.case_filter in c['id']]
        if not cases: p.error('no selected cases')
        deps, manifests, commands = freeze(args, out)
        meta = dict(prepared_at=dt.datetime.now().astimezone().isoformat(),
                    before_source=str(args.before.resolve()), after_source=str(args.after.resolve()),
                    branch=H.output(['git', 'branch', '--show-current']), head=H.output(['git', 'rev-parse', 'HEAD']),
                    compiler_version=H.output([args.compiler, '--version']), platform=platform.platform(),
                    machine=platform.machine(), page_size=os.sysconf('SC_PAGE_SIZE'), cpu_count=os.cpu_count(),
                    runs=args.runs, warmups=args.warmups, cases=cases, manifests=manifests,
                    compile_commands=commands, submodules=deps,
                    runners={n: H.sha(ROOT / 'tools' / n) for n in ('run_vm_followup.py', 'run_vm_perf_compare.py')},
                    schedule='fresh child per sample, sequential cases, paired alternating before/after order',
                    validation='complete value verification outside timers; exact common-work semantic checksum across pair and all repeats; pressure successes may differ and every successful pressure byte is verified separately; schema and graph checks enabled for typed wire roundtrips',
                    metrics='missing metrics are unavailable, not zero; RSS/VA include process and verification allocations; operation samples are diagnostic batches, not production p99',
                    variant_sources={v: {n: manifests[v][n] for n in
                        ('xoffsetdatastructure.hpp', 'xoffset_virtual_memory.hpp')}
                        for v in ('before', 'after')},
                    isolation='default compares before vs after source fingerprints with all opt-ins off; opt_in compares the identical after binary with a single policy enabled',
                    pool='explicit bounded pool, exact limit buckets; limits are count*capacity for reserved/access bytes, count cached blocks; first cold batch and later warm batches; clear measured separately',
                    reclaim='logical shrink first; optional reclaim; timed regrow includes zeroing; full-byte zero checks and dirtying unused tail excluded from timer')
        (out / 'metadata.json').write_text(json.dumps(meta, indent=2) + '\n')
    if args.prepare_only:
        print(f'Prepared; resume with --resume {out}', flush=True); return
    meta['started_at'] = dt.datetime.now().astimezone().isoformat(); meta['load_average_start'] = os.getloadavg()
    if platform.system() == 'Darwin':
        meta['host'] = {k: H.output(['sysctl', '-n', k]) for k in ('hw.memsize', 'hw.physicalcpu', 'hw.logicalcpu', 'machdep.cpu.brand_string')}
        meta['power_source'] = H.output(['pmset', '-g', 'batt'])
    rows = []
    with (out / 'raw.jsonl').open('w') as raw:
        for case in meta['cases']:
            print(case['id'], flush=True); expected = None
            for phase, count in (('warmup', meta['warmups']), ('measured', meta['runs'])):
                for pair in range(count):
                    for position, variant in enumerate(('before', 'after') if pair % 2 == 0 else ('after', 'before')):
                        endpoint = case['endpoints'][variant]
                        cmd = [out / endpoint['binary'], '--workload', case['workload'], '--size', str(case['size']),
                               '--cycles', str(case['cycles']), '--enabled', str(int(endpoint['enabled']))]
                        start = time.monotonic()
                        try: proc = H.command(cmd, text=True, timeout=180)
                        except subprocess.CalledProcessError as e: raise RuntimeError(f'trial failed: {cmd}\n{e.stderr}') from e
                        row = json.loads(proc.stdout)
                        for key in ('workload', 'size', 'cycles'):
                            if row[key] != case[key]: raise RuntimeError(f'wrong returned {key}')
                        if row['enabled'] != endpoint['enabled']: raise RuntimeError('wrong policy')
                        if expected is None: expected = row['semantic_checksum']
                        if row['semantic_checksum'] != expected: raise RuntimeError('semantic mismatch')
                        if any(not m['available'] for m in row['memory'].values()): raise RuntimeError('memory unavailable')
                        row.update(case_id=case['id'], variant=variant, binary=endpoint['binary'], phase=phase,
                                   pair=pair, execution_position=position, sequence=len(rows),
                                   process_wall_ms=(time.monotonic() - start) * 1000)
                        rows.append(row); raw.write(json.dumps(row) + '\n'); raw.flush()
            summary = summarize([case], rows)[0]
            print('  paired ratio {:.4f}'.format(summary['paired_before_over_after']['median']), flush=True)
    verify(out, meta)
    (out / 'summary.json').write_text(json.dumps(summarize(meta['cases'], rows), indent=2) + '\n')
    meta.update(finished_at=dt.datetime.now().astimezone().isoformat(), load_average_end=os.getloadavg(),
                measured_trials=sum(r['phase'] == 'measured' for r in rows), warmup_trials=sum(r['phase'] == 'warmup' for r in rows),
                raw_sha256=H.sha(out / 'raw.jsonl'))
    with (out / 'raw.jsonl').open('rb') as src, gzip.open(out / 'raw.jsonl.gz', 'wb') as dst: shutil.copyfileobj(src, dst)
    with tarfile.open(out / 'source_snapshots.tar.gz', 'w:gz') as archive: archive.add(out / 'snapshots', arcname='snapshots')
    meta['archive_sha256'] = H.sha(out / 'source_snapshots.tar.gz')
    (out / 'metadata.json').write_text(json.dumps(meta, indent=2) + '\n')
    print(f'Results: {out}', flush=True)


if __name__ == '__main__':
    try: main()
    except (RuntimeError, subprocess.SubprocessError) as error:
        print(f'Comparison failed: {error}', file=sys.stderr); sys.exit(1)
