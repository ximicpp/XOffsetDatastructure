"""Read-only cross-platform transfer verification. Python 3.9+, stdlib only.

This is an integrity check, not a build, platform test, source proof, or benchmark.
It imports no project modules, executes no subprocess, and uses no network.
"""
from pathlib import Path, PurePosixPath
import hashlib
import json
import sys
import zipfile

def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            value.update(chunk)
    return {'bytes': path.stat().st_size, 'sha256': value.hexdigest()}

def verify(root):
    root = root.resolve()
    manifest = json.loads((root / 'TRANSFER_MANIFEST.json').read_text(encoding='utf-8'))
    if manifest['schema_version'] != 1:
        raise ValueError('Unsupported transfer manifest')
    for relative, identity in manifest['files'].items():
        key = PurePosixPath(relative)
        if key.is_absolute() or '..' in key.parts or '\\' in relative or ':' in relative:
            raise ValueError('Unsafe relative member: ' + relative)
        path = root.joinpath(*key.parts)
        try:
            path.resolve().relative_to(root)
        except ValueError:
            raise ValueError('Member escapes transfer root: ' + relative)
        if path.is_symlink() or not path.is_file() or digest(path) != identity:
            raise ValueError('Missing or changed member: ' + relative)
    actual = {path.relative_to(root).as_posix() for path in root.rglob('*') if path.is_file()}
    if actual != set(manifest['files']) | {'TRANSFER_MANIFEST.json'}:
        raise ValueError('Unexpected or missing transfer members')
    source_zip = root / manifest['source_zip']
    if digest(source_zip)['sha256'] != manifest['source_zip_sha256']:
        raise ValueError('C14 source ZIP identity mismatch')
    delivery = json.loads((root / manifest['C14_delivery_result']).read_text(encoding='utf-8'))
    prefix = 'xoffsetdatastructure-0.1.0-alpha.1/'
    with zipfile.ZipFile(source_zip) as archive:
        members = [name for name in archive.namelist() if not name.endswith('/')]
        if len(set(members)) != len(members) or len(members) != delivery['decoded_source']['files']:
            raise ValueError('C14 source member count/duplicates mismatch')
        for name, identity in delivery['production_files'].items():
            data = archive.read(prefix + name)
            if {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()} != identity:
                raise ValueError('C14 production file differs: ' + name)
    print(json.dumps({
        'status': 'passed_transfer_integrity_only',
        'transfer_files': len(manifest['files']),
        'C14_source_members': len(members),
        'C14_production_files': len(delivery['production_files']),
        'new_platform_tested': False,
        'new_performance_samples': 0,
        'development_status': manifest['development_status'],
    }, ensure_ascii=False, indent=2))

if __name__ == '__main__':
    try:
        verify(Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent)
    except Exception as error:
        print('TRANSFER CHECK FAILED: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
