#!/usr/bin/env python3
"""Freeze selected delivery inputs; all invocations use the accepted queue.

External dependencies, runtimes, binaries and large evidence remain explicit
content-addressed references. This is not a self-contained build environment.
"""
import argparse
import hashlib
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SKIP = {'__pycache__', '.git', 'evidence', 'artifacts', 'snapshots',
        'g4-preparation', 'docker-adapter-design'}
EXTENSIONS = {'.py', '.cpp', '.hpp', '.h', '.c', '.cmake', '.sh', '.ps1',
              '.json', '.md', '.txt', '.in', '.yml', '.yaml'}
DEFER = 'experiments/vm_owner_measurement/'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def record(path):
    data = path.read_bytes()
    return {'sha256': digest(data), 'bytes': len(data)}


def write(path, value):
    with path.open('x') as stream:
        json.dump(value, stream, indent=2, ensure_ascii=False)
        stream.write('\n')


def selected():
    result = set()
    for name in ('CMakeLists.txt', 'README.md', 'CLAUDE.md', 'LICENSE', 'Dockerfile',
                 'docker-compose.yml', 'build.sh', 'xoffsetdatastructure.hpp',
                 'xoffset_virtual_memory.hpp', '.gitmodules'):
        if (ROOT / name).is_file():
            result.add(name)
    roots = ['cmake', 'tests', 'tools', 'examples', 'include', '.github/workflows',
             'toolchains']
    roots += ['experiments/' + name for name in (
        'vm_network', 'vm_observability', 'vm_network_measurement',
        'vm_owner_scaling', 'vm_io_candidates', 'vm_coordination',
        'vm_platform_validation')]
    for name in roots:
        directory = ROOT / name
        if not directory.exists():
            continue
        for path in directory.rglob('*'):
            relative = path.relative_to(ROOT)
            if any(part in SKIP for part in relative.parts):
                continue
            if path.is_symlink():
                raise RuntimeError('source symlink: ' + str(relative))
            if not path.is_file():
                continue
            if path.name.lower().startswith('handoff') or path.name.startswith('.'):
                continue
            if path.suffix in EXTENSIONS or path.name in {'CMakeLists.txt', 'COMPLETE'}:
                result.add(relative.as_posix())
    return sorted(result)


def stage(out):
    out.mkdir(exist_ok=False, parents=True)
    source = out / 'source'
    source.mkdir()
    paths = selected()
    total = 0
    manifest = {}
    for name in paths:
        path = ROOT / name
        before = record(path)
        total += before['bytes']
        if before['bytes'] > 4 * 1024 * 1024 or total > 32 * 1024 * 1024:
            raise RuntimeError('selected source size budget exceeded: ' + name)
        target = source / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
        if before != record(target) or before != record(path):
            raise RuntimeError('source changed while copying: ' + name)
        manifest[name] = before
    if not manifest or len(manifest) > 1500:
        raise RuntimeError('invalid selected source count')
    write(out / 'stable-source-manifest.json', manifest)
    mapping = json.loads((ROOT / 'experiments/vm_platform_validation/g4-preparation/identity-mapping.json').read_text())
    checked = {}
    for name, old in mapping['candidate_source_identity'].items():
        expected = old if isinstance(old, str) else old['sha256']
        current = record(ROOT / name)
        checked[name] = {'expected_sha256': expected, **current,
                         'same': current['sha256'] == expected}
    historical = json.loads((ROOT / 'docs/validation/2026-10-15-vm-stage3/macos-native-full/source-manifest.json').read_text())
    differences = []
    for name, old in historical.items():
        current = record(ROOT / name) if (ROOT / name).is_file() else None
        expected = old if isinstance(old, str) else old['sha256']
        if current is None or current['sha256'] != expected:
            if name == 'README.md':
                disposition = 'D024 accepted documentary change; outside C++ build/runtime closure'
            elif name in {'tools/audit_vm_allocator.py', 'tools/tests/test_audit_vm_allocator.py'}:
                disposition = 'D013/D015 mapped offline auditor changes outside full CMake/CTest closure; retain separate tool evidence'
            else:
                disposition = 'REVIEW_REQUIRED'
            differences.append({'path': name, 'historical': old,
                                'current': current, 'disposition': disposition})
    source_checks = {}
    for role in ('vm_network', 'vm_observability', 'vm_network_measurement'):
        handoff = json.loads((ROOT / 'experiments' / role / 'handoff.json').read_text())
        key = 'tool_source_sha256' if role == 'vm_network_measurement' else 'source_identity'
        checks = []
        for name, expected in handoff.get(key, {}).items():
            if not name.startswith(('experiments/', 'xoffset')):
                name = 'experiments/' + role + '/' + name
            if Path(name).suffix == '.md':
                continue
            current = record(ROOT / name)
            checks.append({'path': name, 'expected_sha256': expected,
                           **current, 'same': current['sha256'] == expected})
        source_checks[role] = checks
    review = {'schema_version': 1, 'status': 'stable_source_staged_owner_measurement_deferred',
              'cut_at': datetime.now(timezone.utc).isoformat(), 'files': len(manifest),
              'source_bytes': total, 'final_G4_frozen': False,
              'deferred': [DEFER, 'final role handoffs/reports/owner78 acceptance'],
              'base_inheritance_candidate_checks': checked,
              'stageIII_129_record_comparison': {'records': len(historical), 'differences': differences},
              'experimental_handoff_source_checks': source_checks,
              'new_CPP_jobs': 0,
              'dependency_boundary': 'External dependencies/compiler/SDK/runtime and executed binaries referenced by accepted manifests; bytes not bundled; no offline self-contained build claim'}
    write(out / 'source-differences.json', review)
    print(json.dumps({'files': len(manifest), 'bytes': total,
                      'base_mismatches': [k for k, v in checked.items() if not v['same']],
                      'stageIII_differences': differences,
                      'experimental_mismatches': [v for values in source_checks.values() for v in values if not v['same']],
                      'owner_measurement_deferred': True}, ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    stage(args.out.resolve())
