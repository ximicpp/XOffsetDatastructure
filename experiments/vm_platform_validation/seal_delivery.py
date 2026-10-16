#!/usr/bin/env python3
"""One final source/metadata seal after owner parent acceptance; queue-only."""
import argparse
import gzip
import hashlib
import io
import json
import shutil
import stat
import sys
import tarfile
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def identity(path):
    data = path.read_bytes()
    return {'sha256': sha(data), 'bytes': len(data)}


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('x') as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write('\n')


def regular_files(directory):
    for path in sorted(directory.rglob('*')):
        if path.is_symlink():
            raise RuntimeError('symlink forbidden: ' + str(path))
        if path.is_file():
            yield path
        elif not path.is_dir():
            raise RuntimeError('special file forbidden: ' + str(path))


def copy_file(original, target, boundary):
    if original.is_symlink() or not original.is_file():
        raise RuntimeError('not a regular input: ' + str(original))
    if not original.resolve().is_relative_to(ROOT) or not target.resolve().is_relative_to(boundary.resolve()):
        raise RuntimeError('input/output containment violation')
    if original.stat().st_size > 2 * 1024 * 1024:
        raise RuntimeError('metadata/source file over budget: ' + str(original))
    before = identity(original)
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        raise RuntimeError('refusing existing destination: ' + str(target))
    shutil.copyfile(original, target)
    target.chmod(0o755 if original.stat().st_mode & stat.S_IXUSR else 0o644)
    if identity(target) != before or identity(original) != before:
        raise RuntimeError('input changed during cut: ' + str(original))
    return before


def safe_relative(name):
    path = Path(name)
    if path.is_absolute() or '..' in path.parts or not name or '\\' in name:
        raise RuntimeError('unsafe capsule path: ' + name)
    return path


def metadata_paths(spec):
    paths = set(spec['metadata_files'])
    build_index = json.loads((ROOT / 'docs/validation/vm-final/build-identities.json').read_text())
    paths.update(build_index.get('suggested_capsule_metadata_files', []))
    for pattern in spec.get('metadata_globs', []):
        safe_relative(pattern)
        paths.update(str(path.relative_to(ROOT)) for path in ROOT.glob(pattern) if path.is_file())
    for name in spec['metadata_trees']:
        safe_relative(name)
        for path in regular_files(ROOT / name):
            if path.suffix in {'.json', '.md', '.py', '.log'}:
                paths.add(str(path.relative_to(ROOT)))
    for role in spec['handoff_directories']:
        safe_relative(role)
        for path in (ROOT / role).iterdir():
            if path.is_file() and path.name.lower().startswith('handoff'):
                paths.add(str(path.relative_to(ROOT)))
    canonical, aliases = {}, {}
    total = 0
    for name in sorted(paths):
        original = ROOT / safe_relative(name)
        if original.is_symlink() or not original.is_file() or not original.resolve().is_relative_to(ROOT):
            raise RuntimeError('invalid metadata input: ' + name)
        folded = name.casefold()
        if folded in canonical:
            previous = canonical[folded]
            if not original.samefile(ROOT / previous):
                raise RuntimeError('case-colliding metadata inputs: ' + name)
            aliases[name] = previous
            continue
        canonical[folded] = name
        size = original.stat().st_size
        total += size
        if size > 2 * 1024 * 1024 or total > 32 * 1024 * 1024 or len(canonical) > 1500:
            raise RuntimeError('metadata preflight budget exceeded')
    return sorted(canonical.values()), aliases


def main(args):
    args.owner_acceptance = args.owner_acceptance.resolve()
    args.spec = args.spec.resolve()
    out = args.out.resolve()
    if not out.is_relative_to(ROOT / 'docs/validation/vm-final'):
        raise RuntimeError('capsule output outside owned delivery path')
    source = out / 'source'
    initial_names = {'source', 'CAPSULE.md', 'stable-source-manifest.json',
                     'source-differences.json', 'preserved-inputs.json'}
    if {p.name for p in out.iterdir()} != initial_names:
        raise RuntimeError('unexpected/preexisting final output; refusing to bless it')
    spec = json.loads(args.spec.read_text())
    if spec.get('state') != 'ready_to_seal' or not spec.get('expected_final_source') or not spec.get('D031_tool_acceptance'):
        raise RuntimeError('final owner/D031 inputs still awaiting acceptance')
    if set(spec.get('initial_control_identities', {})) != initial_names - {'source'}:
        raise RuntimeError('all staged controls must have explicit identities')
    expected_authority = ROOT / safe_relative(spec['owner_acceptance_path'])
    if args.owner_acceptance != expected_authority.resolve() or identity(args.owner_acceptance)['sha256'] != spec['owner_acceptance_sha256']:
        raise RuntimeError('wrong final owner authority')
    authority = json.loads(args.owner_acceptance.read_text())
    if not str(authority.get('status', '')).startswith('accepted'):
        raise RuntimeError('final owner acceptance missing')
    tool_authority = ROOT / safe_relative(spec['D031_tool_acceptance'])
    if identity(tool_authority)['sha256'] != spec['D031_tool_acceptance_sha256']:
        raise RuntimeError('D031 tool acceptance identity mismatch')
    for name, expected in spec['initial_control_identities'].items():
        control = out / safe_relative(name)
        if control.is_symlink() or identity(control) != expected:
            raise RuntimeError('staged control identity mismatch: ' + name)
    stable = json.loads((out / 'stable-source-manifest.json').read_text())
    preserved = json.loads((out / 'preserved-inputs.json').read_text())
    expected_source = dict(stable)
    for entry in preserved:
        name = str(safe_relative(entry['source_path']))
        if name in expected_source:
            raise RuntimeError('preserved source collision: ' + name)
        expected_source[name] = {k: entry[k] for k in ('sha256', 'bytes')}
    actual_staged = {str(p.relative_to(source)): identity(p) for p in regular_files(source)}
    if actual_staged != expected_source:
        raise RuntimeError('staged source closed set/accepted identity mismatch')
    paths, aliases = metadata_paths(spec)
    for name, expected in spec.get('expected_final_source', {}).items():
        if identity(ROOT / safe_relative(name))['sha256'] != expected:
            raise RuntimeError('accepted final tool identity mismatch: ' + name)
    for name, expected in stable.items():
        if identity(ROOT / name) != expected or identity(source / name) != expected:
            raise RuntimeError('stable related source delta requires review: ' + name)
    for directory in [ROOT / 'experiments/vm_owner_measurement']:
        selected_owner = []
        for original in regular_files(directory):
            if '__pycache__' in original.parts or original.name.lower().startswith('handoff'):
                continue
            if original.suffix not in {'.py', '.json', '.md', '.hpp', '.cpp'}:
                continue
            selected_owner.append(original)
        if {str(p.relative_to(ROOT)) for p in selected_owner} != set(spec['expected_final_source']):
            raise RuntimeError('final owner source closed set differs from accepted spec')
        for original in selected_owner:
            relative = str(original.relative_to(ROOT))
            copied = copy_file(original, source / relative, out)
            if copied['sha256'] != spec['expected_final_source'][relative]:
                raise RuntimeError('copied owner source identity mismatch')
            expected_source[relative] = copied
    for name in ('preserve_final_inputs.py', 'seal_delivery.py'):
        relative = Path('experiments/vm_platform_validation') / name
        expected_source[str(relative)] = copy_file(ROOT / relative, source / relative, out)
    for entry in spec.get('extra_source_files', []):
        original = ROOT / safe_relative(entry['original'])
        if identity(original)['sha256'] != entry['sha256']:
            raise RuntimeError('accepted supplemental source mismatch: ' + entry['original'])
        destination = str(safe_relative(entry['source_path']))
        copied = copy_file(original, source / destination, out)
        if copied['sha256'] != entry['sha256']:
            raise RuntimeError('copied supplemental source identity mismatch')
        expected_source[destination] = copied
    owner_tool_differences = []
    for old in json.loads((out / 'preserved-inputs.json').read_text()):
        if old['source_path'].startswith('accepted-executed/D030/') and Path(old['source_path']).name != 'queue-runner.py':
            current_path = ROOT / 'experiments/vm_owner_measurement' / Path(old['source_path']).name
            current = identity(current_path)
            owner_tool_differences.append({'path': str(current_path.relative_to(ROOT)),
                'original_D030_sha256': old['sha256'], 'final_live': current,
                'same': old['sha256'] == current['sha256'],
                'verification': spec.get('D031_tool_acceptance'),
                'scope': 'D031 Python tool correction/regression; original execution bytes preserved, no C++ or native retry'})
    save(out / 'final-input-differences.json', {'base_stable_files_rechecked': len(stable),
        'base_stable_deltas': [], 'owner_tool_lineage': owner_tool_differences,
        'new_CPP_jobs': 0, 'packaging_tool_additions': ['preserve_final_inputs.py', 'seal_delivery.py']})
    source_manifest = {str(p.relative_to(source)): identity(p) for p in regular_files(source)}
    if source_manifest != expected_source:
        raise RuntimeError('final source closed set/accepted identity mismatch')
    if sum(x['bytes'] for x in source_manifest.values()) > 32 * 1024 * 1024:
        raise RuntimeError('source byte budget exceeded')
    save(out / 'source-manifest.json', source_manifest)
    with (out / 'source.tar.gz').open('xb') as raw:
        with gzip.GzipFile(fileobj=raw, mode='wb', mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode='w') as archive:
                for name in source_manifest:
                    path = source / name
                    data = path.read_bytes()
                    member = tarfile.TarInfo('source/' + name)
                    member.size = len(data)
                    original = ROOT / name
                    executable = original.is_file() and original.stat().st_mode & stat.S_IXUSR
                    member.mode = 0o755 if executable else 0o644
                    member.mtime = member.uid = member.gid = 0
                    archive.addfile(member, io.BytesIO(data))
    with tarfile.open(out / 'source.tar.gz') as archive:
        members = archive.getmembers()
        if len(members) != len(source_manifest) or {p.name for p in members} != {'source/' + n for n in source_manifest}:
            raise RuntimeError('archive member-set mismatch')
        for member in members:
            name = member.name.removeprefix('source/')
            if not member.isfile() or name not in source_manifest:
                raise RuntimeError('invalid source archive member')
            data = archive.extractfile(member).read()
            if {'sha256': sha(data), 'bytes': len(data)} != source_manifest[name]:
                raise RuntimeError('archive content mismatch: ' + name)
    metadata = {}
    metadata_bytes = 0
    for name in paths:
        expected_metadata = spec.get('expected_metadata', {}).get(name)
        if expected_metadata and identity(ROOT / name)['sha256'] != expected_metadata:
            raise RuntimeError('accepted metadata/license mismatch: ' + name)
        metadata_bytes += (ROOT / name).stat().st_size
        if metadata_bytes > 32 * 1024 * 1024 or len(metadata) >= 1500:
            raise RuntimeError('metadata budget exceeded before copy')
        copied = copy_file(ROOT / name, out / 'metadata' / name, out)
        if expected_metadata and copied['sha256'] != expected_metadata:
            raise RuntimeError('copied metadata/license identity mismatch: ' + name)
        metadata[name] = copied
    if len(metadata) > 1500 or sum(x['bytes'] for x in metadata.values()) > 32 * 1024 * 1024:
        raise RuntimeError('metadata budget exceeded')
    save(out / 'metadata-manifest.json', metadata)
    save(out / 'metadata-path-aliases.json', aliases)
    save(out / 'cut.json', {
        'schema_version': 1, 'cut_at': datetime.now(timezone.utc).isoformat(),
        'status': 'frozen_for_independent_G4_review', 'parent_G4_accepted': False,
        'owner_acceptance': str(args.owner_acceptance.relative_to(ROOT)),
        'owner_acceptance_sha256': identity(args.owner_acceptance)['sha256'],
        'scope': 'Bounded local research delivery; external roadmap prerequisites remain open',
        'dependency_boundary': 'Source and metadata reference capsule; external dependencies, toolchains, SDK/runtime, binaries and large evidence are not bundled. Accepted content identities and missing historic seals remain explicit.',
        'new_CPP_jobs': 0, 'new_container_jobs': 0,
        'source_files': len(source_manifest), 'metadata_files': len(metadata),
        'source_tar_verified': True, 'final_review': 'Outside capsule, bound to manifest to avoid a hash cycle',
        'phase_scope': 'stable-source-manifest/source-differences preserve the earlier staged cut; source-manifest and current metadata govern the completed cut',
        'sealing_python': {'executable': sys.executable, 'version': sys.version,
                           'identity': identity(Path(sys.executable).resolve())},
    })
    # The verified archive retains all staged bytes; avoid delivering duplicate trees.
    shutil.rmtree(source)
    files = {str(p.relative_to(out)): identity(p) for p in regular_files(out)}
    expected_files = (initial_names - {'source'}) | {
        'source-manifest.json', 'source.tar.gz', 'final-input-differences.json',
        'metadata-manifest.json', 'metadata-path-aliases.json', 'cut.json'}
    expected_files |= {'metadata/' + name for name in metadata}
    if set(files) != expected_files:
        raise RuntimeError('unexpected capsule output before final manifest')
    save(out / 'manifest.json', files)
    print(json.dumps({'status': 'frozen_for_independent_G4_review',
                      'source_files': len(source_manifest), 'metadata_files': len(metadata),
                      'capsule_files': len(files), 'capsule_bytes': sum(v['bytes'] for v in files.values()),
                      'manifest_sha256': identity(out / 'manifest.json')['sha256'],
                      'source_archive_sha256': identity(out / 'source.tar.gz')['sha256'],
                      'new_CPP_jobs': 0}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--owner-acceptance', type=Path, required=True)
    parser.add_argument('--spec', type=Path, required=True)
    main(parser.parse_args())
