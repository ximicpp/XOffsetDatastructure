#!/usr/bin/env python3
"""One finite original-shared-lease Q/F CAS packaging job; never executes evidence."""
import datetime
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import sys
import time
import zipfile

ROOT = Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
BASE = Path('/root/xoffset-c14-iteration/20261020/v2/validation/final-formal-v1')
OUT = ROOT / 'handoff/mac-20261020/evidence'
WORK = Path('/root/xoffset-c14-iteration/20261020/v2/mac-handoff-20261020/bundle-v1')
COORD = ROOT / 'docs/coordination/c14-iteration-20261020-v2'
MAX_UNIQUE = 2 * 1024**3
MAX_FILES = 150000
MAX_BLOB = 64 * 1024**2
VOLUME_BUDGET = 80 * 1024**2
SCOPES = [
    ('Q32', BASE / 'qualification-v1', 'complete_original_run'),
    ('F64', BASE / 'formal-v1', 'complete_original_run'),
    ('Q32_frozen', BASE / 'frozen-qualification-v1', 'complete_original_frozen'),
    ('F64_frozen', BASE / 'frozen-formal-v1', 'complete_original_frozen'),
    ('Q32_audit_queue', BASE / 'leases/qualification-offline-audit-v1', 'complete_original_audit_queue'),
    ('F64_audit_queue', BASE / 'leases/formal-offline-audit-v1', 'complete_original_audit_queue'),
    ('Q32_audit_result', COORD / 'validation/q32_freeze_preparation/Q32_OFFLINE_AUDIT_RESULT_v1.json', 'original_audit_result'),
    ('F64_audit_result', COORD / 'validation/formal_freeze_preparation/F64_OFFLINE_AUDIT_RESULT_v1.json', 'original_audit_result'),
]


def need(ok, message):
    if not ok:
        raise ValueError(message)


def write_json(path, value):
    with path.open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(value, stream, sort_keys=True, indent=2)
        stream.write('\n')


def fid(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024**2), b''):
            h.update(block)
    return {'name': path.name, 'bytes': path.stat().st_size, 'sha256': h.hexdigest()}


def fingerprint(s):
    return (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns, s.st_mode)


def enumerate_files(source):
    s = source.lstat()
    need(not stat.S_ISLNK(s.st_mode), 'source symlink rejected: ' + str(source))
    if stat.S_ISREG(s.st_mode):
        return [(source.name, source, s)]
    need(stat.S_ISDIR(s.st_mode), 'nonregular source root')
    result = []
    for directory, dirs, files in os.walk(source, followlinks=False):
        dirs.sort()
        files.sort()
        for name in dirs:
            p = Path(directory) / name
            need(stat.S_ISDIR(p.lstat().st_mode), 'symlink/non-directory rejected: ' + str(p))
        for name in files:
            p = Path(directory) / name
            s = p.lstat()
            need(stat.S_ISREG(s.st_mode), 'symlink/nonregular file rejected: ' + str(p))
            result.append((p.relative_to(source).as_posix(), p, s))
    return result


def gzip_bytes(path, raw):
    with path.open('xb') as target:
        with gzip.GzipFile(filename='', mode='wb', compresslevel=6, fileobj=target, mtime=0) as stream:
            stream.write(raw)


def main():
    started = time.monotonic()
    need(not OUT.exists() and not WORK.exists(), 'fresh output/work roots required; no retry or overwrite')
    WORK.mkdir(parents=True)
    records, roots, inodes = [], [], {}
    for key, source, kind in SCOPES:
        entries = enumerate_files(source)
        roots.append({'key': key, 'original_root': str(source), 'kind': kind,
                      'root_type': 'file' if source.is_file() else 'directory',
                      'files': len(entries), 'logical_bytes': sum(s.st_size for _, _, s in entries)})
        for relative, path, s in entries:
            records.append((key, relative, path, s))
            inode = (s.st_dev, s.st_ino)
            if inode in inodes:
                need(fingerprint(s) == fingerprint(inodes[inode][1]), 'inode identity differs')
            else:
                inodes[inode] = (path, s)
    unique_bytes = sum(s.st_size for _, s in inodes.values())
    projection = {'status': 'finite_preflight', 'scope_roots': roots,
                  'logical_files': len(records), 'logical_bytes': sum(s.st_size for _, _, _, s in records),
                  'actual_unique_inodes': len(inodes), 'actual_unique_inode_bytes': unique_bytes,
                  'largest_unique_file': max(s.st_size for _, s in inodes.values()),
                  'maximum_unique_inode_bytes': MAX_UNIQUE, 'maximum_files': MAX_FILES,
                  'maximum_blob_bytes': MAX_BLOB, 'zip_volume_raw_plus_overhead_budget': VOLUME_BUDGET,
                  'workspace_free_bytes': shutil.disk_usage(WORK).free,
                  'destination_free_bytes': shutil.disk_usage(OUT.parent.parent.parent).free,
                  'logical_aliases_will_be_expanded': False, 'new_native_or_performance_runs': 0,
                  'original_source_writes': 0}
    write_json(WORK / 'projection.json', projection)
    need(len(records) <= MAX_FILES and unique_bytes <= MAX_UNIQUE
         and projection['largest_unique_file'] <= MAX_BLOB, 'finite projection exceeds declared format/resource budget')
    need(projection['workspace_free_bytes'] >= unique_bytes + 2 * 1024**3
         and projection['destination_free_bytes'] >= unique_bytes + 2 * 1024**3,
         'insufficient actual staging/output space')
    OUT.mkdir(parents=True)
    cas = WORK / 'cas'
    cas.mkdir()
    inode_sha, blobs = {}, {}
    hashed_bytes = 0
    for index, (inode, (path, s)) in enumerate(inodes.items()):
        need(time.monotonic() - started < 480, 'internal finite 480s packaging bound reached')
        h = hashlib.sha256()
        temp = cas / ('pending-%08d' % index)
        with path.open('rb') as source, temp.open('xb') as target:
            need(fingerprint(os.fstat(source.fileno())) == fingerprint(s), 'source changed before read')
            while True:
                block = source.read(1024**2)
                if not block:
                    break
                h.update(block)
                target.write(block)
                hashed_bytes += len(block)
            need(fingerprint(os.fstat(source.fileno())) == fingerprint(s), 'source changed while read')
        sha = h.hexdigest()
        need(temp.stat().st_size == s.st_size, 'short source read')
        if sha in blobs:
            need(blobs[sha]['bytes'] == s.st_size, 'SHA size collision')
            temp.unlink()  # Newly created, unshared temporary staging only.
        else:
            temp.rename(cas / sha)
            blobs[sha] = {'sha256': sha, 'bytes': s.st_size}
        inode_sha[inode] = sha
        if index % 5000 == 0:
            print(json.dumps({'phase': 'hash_unique_inodes', 'done': index, 'total': len(inodes)}), flush=True)
    need(hashed_bytes == unique_bytes, 'unique source-byte accounting mismatch')
    logical_lines = []
    for key, relative, path, s in records:
        need(fingerprint(path.lstat()) == fingerprint(s), 'source changed after hashing')
        logical_lines.append(json.dumps({'scope': key, 'path': relative,
                                        'sha256': inode_sha[(s.st_dev, s.st_ino)],
                                        'bytes': s.st_size, 'mode': stat.S_IMODE(s.st_mode)},
                                       sort_keys=True, separators=(',', ':')))
    logical_raw = ('\n'.join(logical_lines) + '\n').encode()
    need(len(logical_raw) <= 128 * 1024**2, 'logical map exceeds portable verifier bound')
    gzip_bytes(OUT / 'logical-paths.jsonl.gz', logical_raw)
    volumes, current, cost, number = [], None, 0, 0
    try:
        for sha, entry in sorted(blobs.items()):
            member = 'cas/sha256/' + sha
            contribution = entry['bytes'] + entry['bytes'] // 1000 + 1024 + 2 * len(member)
            if current is None or cost + contribution > VOLUME_BUDGET:
                if current is not None:
                    current.close()
                    volumes.append(fid(current_path))
                number += 1
                current_path = OUT / ('cas-%03d.zip' % number)
                current = zipfile.ZipFile(current_path, 'x', compression=zipfile.ZIP_DEFLATED,
                                          compresslevel=6, allowZip64=False)
                cost = 0
            info = zipfile.ZipInfo(member, date_time=(2026, 10, 20, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = (stat.S_IFREG | 0o444) << 16
            with (cas / sha).open('rb') as source, current.open(info, 'w') as target:
                shutil.copyfileobj(source, target, 1024**2)
            entry.update({'volume': current_path.name, 'member': member})
            cost += contribution
        if current is not None:
            current.close()
            current = None
            volumes.append(fid(current_path))
    finally:
        if current is not None:
            current.close()
    need(all(v['bytes'] < 90 * 1024**2 for v in volumes), 'ZIP volume exceeds 90MiB')
    catalog_raw = (json.dumps({'blobs': list(blobs.values())}, sort_keys=True,
                             separators=(',', ':')) + '\n').encode()
    need(len(catalog_raw) <= 64 * 1024**2, 'catalog exceeds verifier metadata bound')
    gzip_bytes(OUT / 'catalog.json.gz', catalog_raw)
    manifest = {'format': 'c14-qf-portable-cas-v1',
                'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                'scopes': roots, 'logical_paths': fid(OUT / 'logical-paths.jsonl.gz'),
                'catalog': fid(OUT / 'catalog.json.gz'), 'volumes': volumes,
                'summary': {'logical_files': len(records),
                            'logical_bytes': sum(s.st_size for _, _, _, s in records),
                            'source_unique_inodes': len(inodes),
                            'source_unique_inode_bytes': unique_bytes,
                            'cas_blobs': len(blobs), 'cas_bytes': sum(x['bytes'] for x in blobs.values()),
                            'zip_bytes': sum(v['bytes'] for v in volumes),
                            'original_content_bytes_read_for_hashing': hashed_bytes},
                'mapping': 'original_root + relative path (or original_root itself for file scope) -> logical_paths(scope,path,sha256) -> catalog(sha256,volume,member)',
                'omissions': ['Compiler/SDK and installed runtime trees outside declared roots',
                              'External historical raw archives outside declared roots',
                              'R1/M2 individual screening and functional raw build trees outside declared roots (repository summaries remain separate)',
                              'No original host authorization is reactivated; archived scripts/binaries are inert evidence'],
                'semantics': {'integrity_only_not_new_platform_test': True,
                              'original_inputs_and_outputs_unmodified': True,
                              'logical_hardlink_aliases_not_expanded': True,
                              'linux_binaries_not_executed_on_mac': True,
                              'old_queue_host_or_power_actions_not_replayed': True},
                'construction': {'script': 'tools/c14_handoff_20261020/build_bundle.py',
                                 'shared_lease_owner': 'W1', 'shared_lease_kind': 'validation',
                                 'shared_lease_timeout_seconds': 600, 'native_runs': 0,
                                 'internal_elapsed_seconds': time.monotonic() - started}}
    write_json(OUT / 'BUNDLE.json', manifest)
    print(json.dumps({'status': 'packaged', 'summary': manifest['summary'],
                      'volumes': len(volumes), 'manifest': fid(OUT / 'BUNDLE.json')}, sort_keys=True), flush=True)


if __name__ == '__main__':
    main()
