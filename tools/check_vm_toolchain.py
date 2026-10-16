#!/usr/bin/env python3
"""Validate the selected experimental compiler/dependencies against the lock file.

This verifies input identity, not reflection capability or test success. CMake
performs the compile/link capability checks. No dependencies are downloaded.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def capture(command):
    return subprocess.check_output(command, text=True, stderr=subprocess.STDOUT).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lock', type=Path, default=Path(__file__).resolve().parents[1] / 'toolchains/vm-validation.json')
    parser.add_argument('--profile', required=True)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--deps', required=True, type=Path)
    parser.add_argument('--sdk', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    record = {'checked_at': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'profile': args.profile, 'status': 'failed', 'errors': []}
    try:
        lock_bytes = args.lock.read_bytes()
        lock = json.loads(lock_bytes)
        record['lock_sha256'] = hashlib.sha256(lock_bytes).hexdigest()
        profile = lock['profiles'][args.profile]
        compiler = args.compiler.resolve(strict=True)
        version = capture([str(compiler), '--version'])
        record['compiler'] = {'path': str(compiler), 'version': version,
                              'sha256': hashlib.sha256(compiler.read_bytes()).hexdigest()}
        if profile['compiler_commit'] not in version:
            record['errors'].append('compiler revision does not match the selected profile')
        if 'Target: ' + profile['target'] not in version.splitlines():
            record['errors'].append('compiler target does not match the selected profile')
        if 'sdk' in profile:
            if args.sdk is None or not args.sdk.is_dir() or args.sdk.name != profile['sdk']:
                record['errors'].append('supply the explicit SDK named ' + profile['sdk'])
            else:
                settings = args.sdk / 'SDKSettings.json'
                record['sdk'] = {'path': str(args.sdk.resolve()),
                                 'settings_sha256': hashlib.sha256(settings.read_bytes()).hexdigest()}
                expected_version = profile['sdk'].removeprefix('MacOSX').removesuffix('.sdk')
                actual_version = str(json.loads(settings.read_text())['Version'])
                if actual_version != expected_version:
                    record['errors'].append('SDK contents do not match the selected profile')
        record['dependencies'] = {}
        for name, expected in lock['dependencies'].items():
            path = (args.deps / name).resolve(strict=True)
            # Trust only the explicitly selected dependency root, without changing git config.
            git = ['git', '-c', 'safe.directory=' + str(path), '-C', str(path)]
            revision = capture(git + ['rev-parse', 'HEAD'])
            dirty = capture(git + ['status', '--porcelain', '--untracked-files=normal'])
            record['dependencies'][name] = {'commit': revision, 'dirty': bool(dirty)}
            if revision != expected or dirty:
                record['errors'].append(name + ' revision or clean-state check failed')
        if not record['errors']:
            record['status'] = 'passed'
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as exc:
        record['errors'].append(str(exc))
    payload = json.dumps(record, indent=2) + '\n'
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload)
    print(payload, end='')
    return 0 if record['status'] == 'passed' else 1


if __name__ == '__main__':
    sys.exit(main())
