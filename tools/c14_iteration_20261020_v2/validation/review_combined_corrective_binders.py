"""Read/hash/AST-only review of the three finite corrective metadata binders."""
import ast
import difflib
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
D = ROOT / 'docs/coordination/c14-iteration-20261020-v2'
H = ROOT / 'tools/c14_iteration_20261020_v2/core'


def fid(p):
    b = Path(p).read_bytes()
    return dict(bytes=len(b), sha256=hashlib.sha256(b).hexdigest())


def ref(p):
    return dict(path=str(p), identity=fid(p))


def read(p):
    return json.loads(Path(p).read_text(encoding='utf-8-sig'))


def main():
    path = D / 'core/REQUESTS_COMBINED_FULL_CORRECTIVE_v2.json'
    assert fid(path)['sha256'] == '16cf01135fdf789ed82cbfe005310767e26ffef11f4431c082ad8c69c871045e'
    correction = read(path)
    oldpath = D / 'core/REQUESTS_COMBINED_FULL.json'
    old = read(oldpath)
    checked = {}
    for mapping in (old['tool_files'], correction['tool_files']):
        for name, expected in mapping.items():
            assert fid(name) == expected, name
            checked[name] = expected
    assert correction['original_complete_request'] == ref(oldpath)
    assert correction['corrective_external_request']['identity']['sha256'] == '70f4e4a8a12b246fe5ae03294328e5aa1c86271ffe157f00102a519c80a9734f'
    assert len(old['jobs']) == len(correction['jobs']) == 7
    for index, (before, after) in enumerate(zip(old['jobs'], correction['jobs'])):
        assert before['id'] == after['id'] and before['timeout_seconds'] == after['timeout_seconds']
        expected = list(before['argv'])
        if index in (0, 6):
            expected[-1] = expected[-1].replace('.py', '_v2.py')
        assert expected == after['argv']
    derivation = read(D / 'core/COMBINED_EXTERNAL_V2_BINDER_DERIVATION.json')
    differences = ''
    for name, binding in derivation['three_helpers'].items():
        a, b = Path(binding['before']['path']), Path(binding['after']['path'])
        assert fid(a) == binding['before']['identity'] and fid(b) == binding['after']['identity']
        ast.parse(b.read_text(encoding='utf-8'))
        differences += ''.join(difflib.unified_diff(a.read_text(encoding='utf-8').splitlines(True), b.read_text(encoding='utf-8').splitlines(True), fromfile=a.name, tofile=b.name))
    assert differences == (H / 'COMBINED_EXTERNAL_V2_BINDER.patch').read_text(encoding='utf-8')
    output = D / 'validation/review/COMBINED_CORRECTIVE_V2_BINDER_REVIEW.json'
    with output.open('x', encoding='utf-8') as stream:
        json.dump(dict(status='passed_independent_static_review', request=ref(path),
            source_diff=ref(H / 'COMBINED_EXTERNAL_V2_BINDER.patch'), checked_files=checked,
            findings=[
                'All old d535 tool/input bytes retained unchanged. Seven remaining jobs retain order/caps; only prepare/final argv point to new metadata binders.',
                'New verifier retains full73x2 and all six repository fixture/assertion/SAN/CTest/JUnit checks unchanged, checks exact external-v2 same-source/path-only transformation and both source-equivalence proofs, preserves failed external-v1 actual receipt/counts.',
                'Prepare and final retain original d535 identity for stable business recipe/PREPARED, add exact corrective addendum, consume INITIAL_GATES_v2, write final FUNCTIONAL_GATES_v2. No production, fixture, business worker or native scope edits.',
                'Actual original lease cleanup/result and before-after full runtime/provider proof supplied by validation external-v2 guard and forthcoming business guard; metadata binders alone are not those independent checks.',
                'Prior CTest finding retracted; v2 intentionally retains correct d5f9116d CTest logic.'],
            static_scope_only=True, actual_external_pass_claimed=False, compiler=0, native=0, queue=0,
            full_repeated=False, screen_admitted=False, QF_admitted=False), stream, indent=2)
        stream.write('\n')
    print(json.dumps(ref(output)))


if __name__ == '__main__':
    main()
