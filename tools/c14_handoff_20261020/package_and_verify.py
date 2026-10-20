#!/usr/bin/env python3
"""Finite packaging plus one integrity check, inside the original W1 lease."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time


ROOT = Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
OUT = ROOT / 'handoff/mac-20261020/evidence'
HERE = Path(__file__).resolve().parent


def main():
    start = time.monotonic()
    builder = HERE / 'build_bundle.py'
    checker = ROOT / 'tools/verify_c14_evidence_bundle_20261020.py'
    tools = {}
    for path in (builder, checker, Path(__file__).resolve()):
        raw = path.read_bytes()
        tools[str(path.relative_to(ROOT))] = {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}
    subprocess.run([sys.executable, '-I', '-B', str(builder)], check=True, timeout=500)
    result = subprocess.run([sys.executable, '-I', '-B', str(checker), '--manifest', str(OUT / 'BUNDLE.json')],
                            check=True, capture_output=True, text=True, timeout=80)
    checked = json.loads(result.stdout)
    if checked['status'] != 'passed':
        raise ValueError('portable integrity check did not pass')
    receipt = {'status': 'passed', 'operation': 'one finite packaging and one read-only CAS verification',
               'verification': checked, 'tools': tools,
               'elapsed_seconds': time.monotonic() - start,
               'new_benchmark_or_native_runs': 0, 'original_audit_replays': 0,
               'original_source_writes': 0, 'platform_test_claim': False}
    with (OUT / 'VERIFICATION_RESULT.json').open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(receipt, stream, indent=2, sort_keys=True)
        stream.write('\n')
    print(json.dumps(receipt, sort_keys=True), flush=True)


if __name__ == '__main__':
    main()
