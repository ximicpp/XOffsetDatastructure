#!/usr/bin/env python3
"""Exploratory (NON-FORMAL) phase timing for round-3 candidates on macOS.
usage: r3_explore.py <size> <rounds> <out.jsonl> name=bin:wire [...]
Each candidate runs with its OWN input wire. Every run's canonical and query
outputs are compared byte-for-byte with the prepared reference."""
import json, random, subprocess, sys, tempfile
P = '/Volumes/XOffsetCS/mac-validation/prepared-v1/'
size, rounds, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
libs = {}
for e in sys.argv[4:]:
    k, rest = e.split('=', 1)
    b, w = rest.split(':', 1)
    libs[k] = (b, w)
ref_final = open(P + size + '/final.canonical.bin', 'rb').read()
rng = random.Random(20261021)
with open(out, 'w') as f:
    for r in range(rounds):
        order = list(libs); rng.shuffle(order)
        for k in order:
            b, w = libs[k]
            with tempfile.TemporaryDirectory(dir='/Volumes/XOffsetCS/r3-diag') as d:
                p = subprocess.run([b, 'run', '--wire', w, '--trace', P + size + '/trace.bin',
                                    '--wire-out', d + '/w', '--canonical-out', d + '/c',
                                    '--queries-out', d + '/q', '--result', d + '/r.json'],
                                   capture_output=True, text=True)
                if p.returncode:
                    print(k, p.stderr, file=sys.stderr); sys.exit(1)
                res = json.load(open(d + '/r.json'))
                if open(d + '/c', 'rb').read() != ref_final:
                    print(k, 'canonical mismatch', file=sys.stderr); sys.exit(1)
                import os
                wire_bytes = os.path.getsize(d + '/w')
                q = open(d + '/q', 'rb').read()
            f.write(json.dumps({'round': r, 'lib': k, 'wire_out_bytes': wire_bytes,
                                'queries_len': len(q),
                                'phase_ns': {a: int(v) for a, v in res['phase_ns'].items()},
                                'minor_faults': int(res.get('minor_faults', 0))}) + '\n')
            f.flush()
