#!/usr/bin/env python3
"""Freeze F, generate auditable diagnostics, and run identical Automatic traces.

prepare compiles pristine and instrumented binaries; run is diagnostic by default.
Timing runs require --timing and never use the instrumented binary. Output paths
must be fresh, and audit refuses incomplete, modified or failed evidence.
"""
import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BOOST_LIBS = ('container', 'core', 'config', 'throw_exception', 'static_assert',
              'container_hash', 'type_traits', 'winapi', 'intrusive', 'predef')
SOURCES = ('xoffsetdatastructure.hpp', 'xoffset_virtual_memory.hpp')
HELPERS = ('bench_vm_automatic_trace.cpp', 'bench_vm_compare.cpp', 'run_vm_automatic_trace.py',
           'test_vm_automatic_tail_candidate.cpp')
MODES = ('preallocated', 'explicit', 'automatic')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def now():
    return dt.datetime.now().astimezone().isoformat()


def write(path, obj):
    path.write_text(json.dumps(obj, indent=2) + '\n')


def command(argv):
    return subprocess.run(list(map(str, argv)), capture_output=True, text=True, check=True, timeout=120)


def instrument(source):
    """Exact source anchors fail closed when allocator implementation changes."""
    patches = [
        ('if (required <= buffer_.size()) return true;',
         'if (required <= buffer_.size()) return true;\n            ::vm_trace::emit("extent_request", required, buffer_.size(), buffer_.maximum_capacity(), allow_growth);'),
        ('if (!buffer_.grow_to(std::max(required, geometric))) throw XBadAlloc();',
         'if (!buffer_.grow_to(std::max(required, geometric))) throw XBadAlloc();\n            ::vm_trace::emit("auto_growth", current, buffer_.size(), required, 0);'),
        ('free_blocks_.erase(free_blocks_.begin() + static_cast<std::ptrdiff_t>(i));',
         '::vm_trace::emit("core_reuse", block.user_offset, block.user_bytes, bytes, i);\n                    free_blocks_.erase(free_blocks_.begin() + static_cast<std::ptrdiff_t>(i));'),
        ('sm->used_end = std::max<std::uint64_t>(sm->used_end, block_end);',
         '::vm_trace::emit("core_new", user_offset, bytes, current_end, block_end);\n            sm->used_end = std::max<std::uint64_t>(sm->used_end, block_end);'),
        ('if (header->user_offset != offset || header->user_bytes == 0 ||',
         '::vm_trace::emit("expand_check", offset, header->user_bytes, bytes, arena()->used_end);\n            if (header->user_offset != offset || header->user_bytes == 0 ||'),
        ('if (bytes <= header->user_bytes) return true;',
         'if (bytes <= header->user_bytes) { ::vm_trace::emit("expand_existing", offset, header->user_bytes, bytes, arena()->used_end); return true; }'),
        ('if (bytes > MAX_PAYLOAD - offset || !ensure_allocation_extent(offset + bytes)) return false;',
         'if (bytes > MAX_PAYLOAD - offset || !ensure_allocation_extent(offset + bytes)) { ::vm_trace::emit("expand_capacity_failure", offset, header->user_bytes, bytes, arena()->used_end); return false; }\n            ::vm_trace::emit("expand_success", offset, header->user_bytes, bytes, offset + bytes);'),
        ('free_blocks_.push_back(\n                    {header->user_offset, header->total_bytes, header->user_bytes, header->alignment});',
         'free_blocks_.push_back(\n                    {header->user_offset, header->total_bytes, header->user_bytes, header->alignment});\n                ::vm_trace::emit("core_return", header->user_offset, header->user_bytes, header->total_bytes, free_blocks_.size());'),
        ('if (!state || !arena) return;\n            for (std::uint32_t i = 0; i < freelist_class_count; ++i)',
         'if (!state || !arena) return;\n            ::vm_trace::emit("cache_flush", state->free_bytes, state->epoch, arena->used_end, 0);\n            for (std::uint32_t i = 0; i < freelist_class_count; ++i)'),
        ('state->free_bytes -= block->block_bytes;',
         '::vm_trace::emit("wire_reuse", reinterpret_cast<char*>(block) - reinterpret_cast<char*>(arena), block->block_bytes, aligned_bytes, i);\n                                state->free_bytes -= block->block_bytes;'),
        ('state->free_bytes += aligned_bytes;',
         'state->free_bytes += aligned_bytes;\n            ::vm_trace::emit("wire_return", reinterpret_cast<char*>(ptr) - reinterpret_cast<char*>(arena), aligned_bytes, state->free_bytes, class_index);'),
        ('auto old_cap = capacity_;\n                for (size_type i = 0; i < size_; ++i)',
         'auto old_cap = capacity_;\n                ::vm_trace::emit("vector_relocation", old ? reinterpret_cast<char*>(old) - reinterpret_cast<char*>(alloc.segment) : 0, reinterpret_cast<char*>(fresh) - reinterpret_cast<char*>(alloc.segment), std::uint64_t(size_) * sizeof(T), std::uint64_t(new_cap) * sizeof(T));\n                for (size_type i = 0; i < size_; ++i)'),
    ]
    for old, new in patches:
        if source.count(old) != 1:
            raise RuntimeError(f'instrumentation anchor count is not one: {old[:90]}')
        source = source.replace(old, new)
    return source


def tail_flush_candidate(source):
    """Experimental source transformation; never edits the working tree header."""
    patches = [
        ('bool try_expand(void* ptr, std::uint32_t bytes, std::size_t alignment) {\n                return owner().try_expand_impl(ptr, bytes, alignment);',
         'bool try_expand(void* ptr, std::uint32_t bytes, std::size_t alignment,\n                            void (*before_growth)(XAllocatorStateV1*) noexcept = nullptr,\n                            XAllocatorStateV1* context = nullptr) {\n                return owner().try_expand_impl(ptr, bytes, alignment, before_growth, context);'),
        ('bool try_expand_impl(void* ptr, std::uint32_t bytes, std::size_t alignment) {',
         'bool try_expand_impl(void* ptr, std::uint32_t bytes, std::size_t alignment,\n                             void (*before_growth)(XAllocatorStateV1*) noexcept, XAllocatorStateV1* context) {'),
        ('if (bytes > MAX_PAYLOAD - offset || !ensure_allocation_extent(offset + bytes)) return false;',
         'if (before_growth && growth_policy_ == XGrowPolicy::Automatic &&\n                bytes <= MAX_PAYLOAD - offset && offset + bytes > buffer_.size() &&\n                offset + bytes <= buffer_.maximum_capacity()) before_growth(context);\n            if (bytes > MAX_PAYLOAD - offset || !ensure_allocation_extent(offset + bytes)) return false;'),
        ('if (!alloc_ref.segment->try_expand(ptr, aligned_bytes, alignment)) return false;',
         'if (!alloc_ref.segment->try_expand(ptr, aligned_bytes, alignment,\n                    alloc_ref.state && alloc_ref.state->free_bytes ? release_cached_blocks : nullptr,\n                    alloc_ref.state)) return false;'),
    ]
    for old, new in patches:
        if source.count(old) != 1: raise RuntimeError('candidate anchor count is not one')
        source = source.replace(old, new)
    return source


def cases():
    # Independent axes, including the exact historical initial=4096/cadence=256.
    return [dict(id=f'i{initial}_c{cadence}_b{burst}_l{lifetime}', count=1000000,
                 initial=initial, maximum=32 * 1024 * 1024, cadence=cadence,
                 burst=burst, lifetime=lifetime)
            for initial in (4096, 65536) for cadence in (256, 1024)
            for burst in (1, 8) for lifetime in (0, 16)]


def verify_prepared(directory):
    meta = json.loads((directory / 'prepared.json').read_text())
    if meta['status'] != 'prepared':
        raise RuntimeError('preparation is incomplete')
    for name, expected in meta['files'].items():
        if sha(directory / name) != expected:
            raise RuntimeError(f'prepared input changed: {name}')
    if sha(directory / 'source' / 'run_vm_automatic_trace.py') != sha(Path(__file__)):
        raise RuntimeError('runner differs from prepared runner')
    for name, expected in meta.get('runtime_files', {}).items():
        if sha(Path(name)) != expected: raise RuntimeError(f'runtime input changed: {name}')
    for name, expected in meta.get('dependencies', {}).items():
        path = Path(meta['dependency_root']) / name
        if command(['git', '-C', path, 'rev-parse', 'HEAD']).stdout.strip() != expected or command(['git', '-C', path, 'status', '--porcelain']).stdout.strip():
            raise RuntimeError(f'dependency changed: {name}')
    return meta


def prepare(args):
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=False)
    status = dict(status='running', started_at=now())
    write(out / 'prepared.json', status)
    try:
        source = out / 'source'; source.mkdir()
        for name in SOURCES: shutil.copy2(ROOT / name, source / name)
        for name in HELPERS: shutil.copy2(ROOT / 'tools' / name, source / name)
        (source / 'tests').mkdir()
        shutil.copy2(ROOT / 'tests/test_storage_policies.cpp', source / 'tests/test_storage_policies.cpp')
        if args.candidate_tail_flush:
            original = out / 'baseline'; original.mkdir()
            for name in SOURCES: shutil.copy2(source / name, original / name)
            (source / SOURCES[0]).write_text(tail_flush_candidate((source / SOURCES[0]).read_text()))
        diag = out / 'diagnostic'; diag.mkdir()
        for path in source.iterdir():
            if path.is_file(): shutil.copy2(path, diag / path.name)
        (diag / SOURCES[0]).write_text(instrument((source / SOURCES[0]).read_text()))
        deps = {}
        for name in ('boost', 'typelayout'):
            path = args.deps.resolve() / name
            if command(['git', '-C', path, 'status', '--porcelain']).stdout.strip():
                raise RuntimeError(f'dirty dependency {name}')
            deps[name] = command(['git', '-C', path, 'rev-parse', 'HEAD']).stdout.strip()
        common = [args.compiler, '-O3', '-DNDEBUG', '-std=c++26', '-freflection',
                  '-fexpansion-statements', '-stdlib=libc++', '-pthread']
        if platform.system() == 'Darwin':
            common += ['-arch', platform.machine(), '-isysroot', args.sdk, '-mmacosx-version-min=10.15']
        for lib in BOOST_LIBS: common += ['-isystem', str(args.deps.resolve() / 'boost/libs' / lib / 'include')]
        common += ['-isystem', str(args.deps.resolve() / 'typelayout/include'), '-L' + args.library_dir,
                   '-Wl,-rpath,' + args.library_dir]
        commands = {}
        for name, folder, enabled in [('timed', source, 0), ('instrumented', diag, 1)]:
            cmd = common + [f'-DXOFFSET_TRACE_DIAGNOSTIC={enabled}', '-I', str(folder),
                            str(folder / 'bench_vm_automatic_trace.cpp'), '-o', str(out / name)]
            commands[name] = cmd
            result = subprocess.run(cmd, capture_output=True, text=True)
            (out / (name + '.compile.log')).write_text(result.stdout + result.stderr)
            if result.returncode: raise RuntimeError(f'{name} compilation failed')
        for name, path in [('storage_policies', source / 'tests/test_storage_policies.cpp'),
                           ('candidate_boundaries', source / 'test_vm_automatic_tail_candidate.cpp')]:
            cmd = common + ['-UNDEBUG', f'-DXOFFSET_CANDIDATE_TAIL_FLUSH={int(args.candidate_tail_flush)}',
                            '-I', str(source), str(path), '-o', str(out / name)]
            commands[name] = cmd
            result = subprocess.run(cmd, capture_output=True, text=True)
            (out / (name + '.compile.log')).write_text(result.stdout + result.stderr)
            if result.returncode: raise RuntimeError(f'{name} compilation failed')
            result = subprocess.run([str(out / name)], capture_output=True, text=True)
            (out / (name + '.run.log')).write_text(result.stdout + result.stderr)
            if result.returncode: raise RuntimeError(f'{name} validation failed')
        files = {str(p.relative_to(out)): sha(p) for p in out.rglob('*') if p.is_file() and p.name != 'prepared.json'}
        runtime_files = [Path(shutil.which(args.compiler) or args.compiler)]
        for name in ('libc++.1.0.dylib', 'libc++abi.1.0.dylib', 'libunwind.1.0.dylib'):
            path = Path(args.library_dir) / name
            if path.exists(): runtime_files.append(path)
        for name in ('SDKSettings.json', 'SDKSettings.plist'):
            path = Path(args.sdk) / name
            if path.exists(): runtime_files.append(path)
        status.update(status='prepared', completed_at=now(), files=files, compile_commands=commands,
                      variant='tail_flush_candidate' if args.candidate_tail_flush else 'F',
                      dependencies=deps, compiler_version=command([args.compiler, '--version']).stdout,
                      dependency_root=str(args.deps.resolve()), runtime_files={str(p): sha(p) for p in runtime_files},
                      compiler_sha256=sha(shutil.which(args.compiler) or args.compiler),
                      platform=platform.platform(), page_size=os.sysconf('SC_PAGE_SIZE'), cases=cases(),
                      event_schema=['name', 'operation_index', 'lane_0_primary_1_secondary_2_release', 'a', 'b', 'c', 'd'])
        write(out / 'prepared.json', status)
    except BaseException as error:
        status.update(status='failed', error=str(error)); write(out / 'prepared.json', status); raise


def audit(directory):
    result = json.loads((directory / 'result.json').read_text())
    if result['status'] != 'complete' or not (directory / 'COMPLETE').is_file():
        raise RuntimeError('incomplete experiment')
    if (directory / 'COMPLETE').read_text().strip() != sha(directory / 'result.json'):
        raise RuntimeError('result marker mismatch')
    for name, expected in result['files'].items():
        if sha(directory / name) != expected: raise RuntimeError(f'evidence changed: {name}')
    rows = [json.loads(line) for line in (directory / 'raw.jsonl').read_text().splitlines()]
    if len(rows) != result['expected_samples']: raise RuntimeError('missing samples')
    expected_keys = {(case['id'], repetition, mode, variant) for case in result['cases']
                     for repetition in range(result['runs']) for mode in MODES for variant in result.get('variants', ['F'])}
    keys = [(r['case_id'], r['repetition'], r['mode'], r.get('variant', 'F')) for r in rows]
    if len(set(keys)) != len(keys) or set(keys) != expected_keys:
        raise RuntimeError('missing, duplicate or unexpected policy sample')
    for case in result['cases']:
        group = [r for r in rows if r['case_id'] == case['id']]
        for row in group:
            if row['diagnostic'] == result['timing']:
                raise RuntimeError('diagnostic/timed mode mismatch')
            for key, value in case.items():
                if key != 'id' and row[key] != value:
                    raise RuntimeError('trace parameters changed')
        if len({r['trace_checksum'] for r in group}) != 1 or len({r['semantic_checksum'] for r in group}) != 1:
            raise RuntimeError('same-trace or semantic comparison mismatch')
    return result, rows


def run(args):
    prepared = args.prepared.resolve(); meta = verify_prepared(prepared)
    inputs = {meta.get('variant', 'F'): (prepared, meta)}
    if args.compare_prepared:
        other = args.compare_prepared.resolve(); other_meta = verify_prepared(other)
        if other_meta['variant'] in inputs: raise RuntimeError('comparison variants must differ')
        # Different generated headers are intended; operation harness and runtime must match.
        for name in HELPERS:
            if sha(prepared / 'source' / name) != sha(other / 'source' / name):
                raise RuntimeError('comparison harness differs')
        if meta['runtime_files'] != other_meta['runtime_files'] or meta['dependencies'] != other_meta['dependencies']:
            raise RuntimeError('comparison runtime differs')
        inputs[other_meta['variant']] = (other, other_meta)
    prepared_identities = {variant: sha(directory / 'prepared.json')
                           for variant, (directory, _) in inputs.items()}
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=False)
    selected = [c for c in meta['cases'] if not args.case or c['id'] in args.case]
    if not selected: raise RuntimeError('no cases selected')
    status = dict(status='running', started_at=now(), timing=bool(args.timing), cases=selected,
                  prepared_sha256=sha(prepared / 'prepared.json'), runs=args.runs,
                  prepared_identities=prepared_identities,
                  variants=list(inputs), expected_samples=len(selected) * len(MODES) * args.runs * len(inputs),
                  timing_scope='operation loop only; includes Explicit failure/retry, growth and common retiring-value checks for finite lifetimes; excludes trace generation, arena setup, diagnostics output, final live-value/wire checks; preallocated setup separately measured',
                  load_average_start=os.getloadavg())
    write(out / 'result.json', status)
    try:
        for variant, (directory, _) in inputs.items():
            shutil.copy2(directory / 'prepared.json', out / (variant + '.prepared.json'))
        rows = []
        with (out / 'raw.jsonl').open('w') as raw:
            for case in selected:
                for repetition in range(args.runs):
                    order = MODES[repetition % 3:] + MODES[:repetition % 3]
                    for mode in order:
                        variants = list(inputs)
                        if repetition % 2: variants.reverse()
                        for variant in variants:
                            directory, _ = inputs[variant]
                            cmd = [directory / ('timed' if args.timing else 'instrumented'), '--mode', mode]
                            for key, value in case.items():
                                if key != 'id': cmd += ['--' + key, str(value)]
                            row = json.loads(command(cmd).stdout)
                            if row['mode'] != mode or row['diagnostic'] == bool(args.timing): raise RuntimeError('wrong execution mode')
                            for key, value in case.items():
                                if key != 'id' and row[key] != value: raise RuntimeError('wrong trace parameters')
                            row.update(case_id=case['id'], repetition=repetition, variant=variant)
                            raw.write(json.dumps(row) + '\n'); raw.flush(); rows.append(row)
        for variant, (directory, _) in inputs.items():
            verify_prepared(directory)
            if sha(directory / 'prepared.json') != prepared_identities[variant]:
                raise RuntimeError('prepared manifest changed during execution')
        for case in selected:
            group = [r for r in rows if r['case_id'] == case['id']]
            if len({r['trace_checksum'] for r in group}) != 1 or len({r['semantic_checksum'] for r in group}) != 1:
                raise RuntimeError('same-trace or semantic comparison mismatch')
        status.update(status='complete', completed_at=now(), load_average_end=os.getloadavg(),
                      files={p.name: sha(p) for p in out.iterdir() if p.name != 'result.json'})
        write(out / 'result.json', status)
        (out / 'COMPLETE').write_text(sha(out / 'result.json') + '\n')
        audit(out)
    except BaseException as error:
        (out / 'COMPLETE').unlink(missing_ok=True)
        status.update(status='failed', error=str(error)); write(out / 'result.json', status); raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subs = parser.add_subparsers(dest='action', required=True)
    p = subs.add_parser('prepare'); p.add_argument('--out', type=Path, required=True)
    p.add_argument('--compiler', required=True); p.add_argument('--sdk', required=True)
    p.add_argument('--candidate-tail-flush', action='store_true')
    p.add_argument('--library-dir', required=True); p.add_argument('--deps', type=Path, required=True)
    p = subs.add_parser('run'); p.add_argument('--prepared', type=Path, required=True); p.add_argument('--out', type=Path, required=True)
    p.add_argument('--compare-prepared', type=Path)
    p.add_argument('--timing', action='store_true'); p.add_argument('--runs', type=int, default=1); p.add_argument('--case', action='append')
    p = subs.add_parser('audit'); p.add_argument('directory', type=Path)
    args = parser.parse_args()
    if args.action == 'prepare': prepare(args)
    elif args.action == 'run':
        if args.runs < 1: parser.error('runs must be positive')
        run(args)
    else:
        result, rows = audit(args.directory); print(f"{len(rows)} samples audited ({'timed' if result['timing'] else 'diagnostic'})")


if __name__ == '__main__': main()
