"""Correct prior static finding using exact pinned verifier and actual CTest data."""
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
D = ROOT / 'docs/coordination/c14-iteration-20261020-v2'


def fid(p):
    b = Path(p).read_bytes()
    return dict(bytes=len(b), sha256=hashlib.sha256(b).hexdigest())


def ref(p):
    return dict(path=str(p), identity=fid(p))


def main():
    old = D / 'validation/review/COMBINED_PREPARE_CTEST_FINDING.json'
    original = json.loads(old.read_text(encoding='utf-8-sig'))
    verifier = ROOT / 'tools/c14_iteration_20261020_v2/core/verify_combined_initial.py'
    assert fid(verifier) == original['verifier']['identity']
    text = verifier.read_text(encoding='utf-8')
    assert "str(build/'bin/Release'/name)" in text
    assert "re.match(r'add_test\\('+token,x)" in text
    assert "re.match(r'set_tests_properties\\('+token,x)" in text
    names = json.loads((D / 'core/REQUESTS_COMBINED_INITIAL.json').read_text(encoding='utf-8'))['six_repository_targeted']
    rows = []
    for mode in ('normal', 'sanitized'):
        build = Path('/root/xoffset-c14-iteration/20261020/v2/validation') / ('combined-full-v1-' + mode) / 'build'
        ctestpath = build / 'tests/CTestTestfile.cmake'
        ctest = ctestpath.read_text(encoding='utf-8')
        for name in names:
            token = r'(?:\[=\[' + re.escape(name) + r'\]=\]|' + re.escape(name) + r')(?=\s)'
            lines = [x for x in ctest.splitlines() if re.match(r'add_test\(' + token, x)]
            assert len(lines) == 1 and re.findall(r'"([^"]*)"', lines[0]) == [str(build / 'bin/Release' / name)]
            props = [x for x in ctest.splitlines() if re.match(r'set_tests_properties\(' + token, x)]
            assert len(props) == 1
            assert not any(re.search(r'\b(?:DISABLED|WILL_FAIL)\s+(?:TRUE|ON|1)\b', x) for x in props)
            rows.append(dict(mode=mode, name=name, actual_CTest=ref(ctestpath), add_test=lines[0], properties=props,
                             actual_binary=ref(build / 'bin/Release' / name)))
    out = D / 'validation/review/COMBINED_PREPARE_CTEST_FINDING_CORRECTION.json'
    with out.open('x', encoding='utf-8') as stream:
        json.dump(dict(status='prior_static_finding_retracted_exact_pinned_source_already_correct',
            original_finding=ref(old), unchanged_original_verifier=ref(verifier),
            explanation='The previous finding incorrectly described its own pinned verifier. Exact d5f9116d source already accepts bracketed or unbracketed names, requires build/bin/Release/name, and selects properties using the same token. All12 actual normal/SAN registrations and properties satisfy that unchanged consumer. No CTest parsing fix or full rerun is required. External-v1 failed receipt remains a real independent blocker requiring the corrective external addendum.',
            actual12_registration_checks=rows, compiler=0, native=0, queue=0,
            full_targeted_gate_passed_by_this_correction=False), stream, indent=2)
        stream.write('\n')
    print(json.dumps(ref(out)))


if __name__ == '__main__':
    main()
