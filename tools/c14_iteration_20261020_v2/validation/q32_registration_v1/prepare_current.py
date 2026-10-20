"""Fixed, once-only read-only capacity observation followed by small registration.

Run only inside the admitted original shared lease. No nested queue, native,
compiler, project import, process suspension or power change is performed.
"""
from pathlib import Path
import argparse, hashlib, json, subprocess, sys

HERE = Path(__file__).resolve().parent

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--request', required=True, type=Path)
    p.add_argument('--request-sha256', required=True)
    a = p.parse_args()
    raw = a.request.read_bytes()
    if hashlib.sha256(raw).hexdigest() != a.request_sha256:
        raise ValueError('exact admitted preparation request changed')
    r = json.loads(raw)
    if r['status'] != 'ready_for_independent_admission' or r['native'] != 0:
        raise ValueError('preparation request not ready')
    for name, identity in r['tools'].items():
        value = (HERE / name).read_bytes()
        if {'bytes': len(value), 'sha256': hashlib.sha256(value).hexdigest()} != identity:
            raise ValueError('preparation tool changed: ' + name)
    capacity = Path(r['capacity_output'])
    acceptance = r['gate_acceptance']
    if hashlib.sha256(Path(acceptance['path']).read_bytes()).hexdigest() != acceptance['identity']['sha256']:
        raise ValueError('gate acceptance changed')
    if capacity.exists():
        raise ValueError('fresh capacity output required; no retry or overwrite')
    commands = [
        [sys.executable, '-I', '-B', str(HERE / 'observe-capacity.py'), '--out', str(capacity)],
        [sys.executable, '-I', '-B', str(HERE / 'register_current.py'), '--capacity', str(capacity),
         '--gate-acceptance', acceptance['path'], '--gate-acceptance-sha256', acceptance['identity']['sha256']],
    ]
    for argv in commands:
        subprocess.run(argv, check=True, timeout=120)

if __name__ == '__main__':
    main()
