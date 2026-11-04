#!/usr/bin/env python3
"""Read-only source/sample integrity and preregistered-statistics audit.

Does not compile, execute benchmarks, rewrite reports, or validate a platform.
Original native binaries are optional local evidence, not distributed inputs.
"""
import hashlib
import json
import math
from pathlib import Path
import statistics
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
T95 = 1.795884819
T975 = 2.200985160


def sha(data):
    return hashlib.sha256(data).hexdigest()


def interval(values, t):
    mean = statistics.mean(values)
    half = t * statistics.stdev(values) / math.sqrt(len(values))
    return [mean, mean-half, mean+half]


def ratio(values, t):
    return [math.exp(x) for x in interval([math.log(x) for x in values], t)]


def same(actual, expected):
    assert len(actual) == len(expected)
    assert all(math.isclose(a, b, rel_tol=1e-11, abs_tol=1e-11)
               for a, b in zip(actual, expected)), (actual, expected)


def audit_window(directory, summary_name, modes, archive=None):
    window = HERE / directory
    registered = json.loads((window/'preregister.json').read_text())
    summary = json.loads((window/summary_name).read_text())
    assert registered['processes_per_phase'] == 12
    assert registered['blocks_per_process'] == 12
    missing_native = []
    for name, expected in registered['identity'].items():
        if '/bin/' in name:
            # A rebuild can legitimately differ from the original native binary.
            # Report provenance as recorded; do not infer a new platform pass.
            p = ROOT/name
            if not p.exists():
                missing_native.append(name)
            elif sha(p.read_bytes()) != expected:
                missing_native.append(name + ' (local binary differs)')
            continue
        data = (archive.read('evidence/measurement-v1/candidate/'+name)
                if archive is not None else (ROOT/name).read_bytes())
        assert sha(data) == expected, ('source identity', directory, name)
    for name, expected in summary['raw_files'].items():
        assert sha((window/name).read_bytes()) == expected, ('sample identity', directory, name)
    phases = {}
    for phase in ['aa', 'comparison']:
        names = [f'{phase}-{i:02}.json' for i in range(12)]
        assert sorted(p.name for p in window.glob(phase+'-*.json')) == names
        for name in names:
            err = window/(name[:-5]+'.stderr')
            assert err.exists() and err.read_bytes() == b'', err
        phases[phase] = [json.loads((window/n).read_text()) for n in names]
        assert all(len(p['cases']) == 8 for p in phases[phase])
    passed = dict.fromkeys(modes, 0)
    qualified_cases = 0
    for index, result in enumerate(summary['cases']):
        aa = [p['cases'][index] for p in phases['aa']]
        comparison = [p['cases'][index] for p in phases['comparison']]
        for case in aa+comparison:
            assert all(case[k] == result[k] for k in ['bytes', 'nodes', 'holes', 'iterations'])
            assert all(len(v) == 12 for v in case['samples'].values())
        aa_ratios = [statistics.median(b/a for a, b in zip(c['samples']['raw_a'], c['samples']['raw_b'])) for c in aa]
        aa_deltas = [statistics.median(b-a for a, b in zip(c['samples']['raw_a'], c['samples']['raw_b'])) for c in aa]
        ci, delta = ratio(aa_ratios, T975), interval(aa_deltas, T975)
        same(ci, result['aa_ratio_ci'])
        same(delta, result['aa_delta_ci_ns'])
        qualified = ci[1] >= .97 and ci[2] <= 1.03 and (result['bytes'] > 4096 or delta[1] >= -15 and delta[2] <= 15)
        assert qualified == result['qualified']
        qualified_cases += qualified
        for mode in modes:
            baseline = {'image': 'raw_slot', 'borrowed': 'raw_borrowed'}.get(mode, 'raw_a')
            assert all(x > 0 for c in comparison for m in [mode, baseline] for x in c['samples'][m])
            pairs = [statistics.median(b/a for a, b in zip(c['samples'][baseline], c['samples'][mode])) for c in comparison]
            diffs = [statistics.median(b-a for a, b in zip(c['samples'][baseline], c['samples'][mode])) for c in comparison]
            rci, dci = ratio(pairs, T95), interval(diffs, T95)
            saved = result['modes'][mode]
            same(rci, saved['ratio_ci'])
            same(dci, saved['delta_ci_ns'])
            gate = qualified and (dci[2] <= 50 if result['bytes'] <= 4096 else rci[2] <= 1.10)
            assert gate == saved['strict_gate']
            passed[mode] += gate
    assert qualified_cases == summary['qualified_cases']
    return {'window': directory, 'AA_cases': qualified_cases, 'passed_cases': passed,
            'original_binary_not_verified_here': missing_native}


def main():
    manifest_path = HERE/'review-manifest.json'
    if manifest_path.exists():
        manifest = json.loads(manifest_path.read_text())
        for name, expected in manifest['files'].items():
            assert sha((ROOT/name).read_bytes()) == expected, ('review file identity', name)
    with zipfile.ZipFile(HERE/'review-evidence.zip') as archive:
        first = audit_window('measurement-v1', 'summary-recovered.json', ['packed', 'image'], archive)
    second = audit_window('measurement-v2-borrowed', 'summary.json', ['borrowed'])
    assert first['passed_cases'] == {'packed': 8, 'image': 7}
    assert second['passed_cases'] == {'borrowed': 8}
    print(json.dumps({'integrity_and_offline_statistics': 'PASS', 'platform_test': 'NOT_RUN',
                      'windows': [first, second]}, indent=2))


if __name__ == '__main__':
    main()
