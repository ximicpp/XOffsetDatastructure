"""Restore/verify the GitHub handoff with Python 3.9+ stdlib only.

No network, C++ build, benchmark, process control, or resume of old work.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import sys
import zipfile

EXPECTED = '313e41f3890042c44cf93a14ddfb6b833d7112d35b23b07c8c5030e5ae149deb'

def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(chunk)
    return {'bytes': path.stat().st_size, 'sha256': value.hexdigest()}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, help='Fresh directory, or an already complete restored directory')
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    archive = root / 'handoff/XOffset-continuation-20261019-v1.zip'
    if digest(archive)['sha256'] != EXPECTED:
        raise ValueError('Frozen handoff ZIP checksum differs')
    output = (options.out or root / 'handoff/continuation-20261019-v1').resolve()
    if output == root or output in root.parents:
        raise ValueError('Extraction cannot replace the repository or its parents')
    if output.exists():
        if not (output / 'TRANSFER_MANIFEST.json').is_file():
            raise ValueError('Partial/existing output preserved; choose a fresh --out')
    else:
        with zipfile.ZipFile(archive) as source:
            entries = source.infolist()
            names = [entry.filename for entry in entries]
            if len(names) != len(set(names)):
                raise ValueError('Duplicate ZIP members')
            for entry in entries:
                name = entry.filename
                key = PurePosixPath(name)
                if key.is_absolute() or '..' in key.parts or '\\' in name or ':' in name:
                    raise ValueError('Unsafe ZIP member: ' + name)
                if stat.S_ISLNK(entry.external_attr >> 16):
                    raise ValueError('Symlink ZIP member: ' + name)
                try:
                    output.joinpath(*key.parts).resolve().relative_to(output)
                except ValueError:
                    raise ValueError('ZIP member escapes output: ' + name)
            output.mkdir(parents=True)
            for entry in entries:
                destination = output.joinpath(*PurePosixPath(entry.filename).parts)
                if entry.is_dir():
                    destination.mkdir(parents=True, exist_ok=True)
                else:
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    with source.open(entry) as reader, destination.open('xb') as writer:
                        shutil.copyfileobj(reader, writer)
    # Verify the checker and its manifest against the pinned archive before
    # executing anything from an already existing extraction directory.
    with zipfile.ZipFile(archive) as source:
        for name in ('verify_handoff.py', 'TRANSFER_MANIFEST.json'):
            if (output / name).read_bytes() != source.read(name):
                raise ValueError('Existing extraction checker/manifest differs: ' + name)
    subprocess.run([sys.executable, '-I', '-B', str(output / 'verify_handoff.py'), str(output)],
                   check=True, env=dict(os.environ, PYTHONDONTWRITEBYTECODE='1'))
    record = json.loads((root / 'handoff/github-20261019/C14_MATERIALIZATION.json').read_text(encoding='utf-8'))
    for name, expected in record['non_external_files'].items():
        if digest(root / name) != expected:
            raise ValueError('Repository C14 member differs: ' + name)
    result = subprocess.run(['git', '-C', str(root), 'ls-files', '--stage', '--', *record['gitlinks']],
                            capture_output=True, text=True, check=True, timeout=30)
    for name, sha in record['gitlinks'].items():
        if not any(line.startswith('160000 ' + sha + ' ') and line.endswith('\t' + name)
                   for line in result.stdout.splitlines()):
            raise ValueError('Dependency gitlink differs: ' + name)
    print(json.dumps({'status': 'github_handoff_integrity_passed', 'restored': str(output),
                      'root_C14_non_external_members': len(record['non_external_files']),
                      'root_C14_production_files': len(record['production_files']),
                      'new_platform_tested': False, 'new_performance_samples': 0,
                      'old_machine_remains_paused': True}, ensure_ascii=True, indent=2))

if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print('HANDOFF RESTORE FAILED: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
