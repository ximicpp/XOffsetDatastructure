#!/usr/bin/env python3
"""Fetch a hash-pinned official CMake bundle, separately from offline validation."""
import argparse
import json
from pathlib import Path
import shutil
import sys
import urllib.request

from validate_vm import extract_archive, file_manifest, sha, write_json


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=('linux-x86_64', 'linux-aarch64'), required=True)
    parser.add_argument('--out', type=Path, required=True, help='Fresh directory; binary root will be out/root')
    parser.add_argument('--archive', type=Path, help='Use an already downloaded archive without network')
    parser.add_argument('--manifest', type=Path, default=Path(__file__).resolve().parents[1] / 'toolchains/vm-validation.json')
    args = parser.parse_args(argv)
    out = args.out.resolve()
    try:
        out.mkdir(parents=True, exist_ok=False)
    except OSError as exc:
        print(str(exc), file=sys.stderr)
        return 1
    result = {'status': 'running', 'platform': args.platform}
    write_json(out / 'result.json', result)
    try:
        config = json.loads(args.manifest.read_text())['build_tools']['cmake']
        record = config['archives'][args.platform]
        archive = out / 'cmake.tar.gz'
        if args.archive:
            shutil.copyfile(args.archive, archive)
        else:
            with urllib.request.urlopen(record['url'], timeout=120) as response, archive.open('wb') as target:
                shutil.copyfileobj(response, target)
        if sha(archive) != record['sha256']:
            raise ValueError('CMake archive checksum does not match the pinned manifest')
        extract_archive(archive, out / 'extracted')
        children = list((out / 'extracted').iterdir())
        if len(children) != 1 or not children[0].is_dir():
            raise ValueError('Expected one CMake archive root')
        children[0].rename(out / 'root')
        (out / 'extracted').rmdir()
        for name in ('cmake', 'ctest'):
            if not (out / 'root/bin' / name).is_file():
                raise ValueError('CMake bundle is missing bin/' + name)
        files = file_manifest(out / 'root')
        write_json(out / 'files.json', files)
        result.update(status='passed', version=config['version'], archive=record,
                      manifest_sha256=sha(args.manifest), files_sha256=sha(out / 'files.json'))
    except BaseException as exc:
        result.update(status='interrupted' if isinstance(exc, KeyboardInterrupt) else 'failed', reason=str(exc))
        print(str(exc), file=sys.stderr)
    write_json(out / 'result.json', result)
    print(result['status'] + ': ' + str(out / 'root'))
    return 0 if result['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
