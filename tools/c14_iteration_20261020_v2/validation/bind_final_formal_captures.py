"""Copy accepted authoring bytes; change only fixed_input_captures, never execute."""
import ast
import copy
import difflib
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
D = ROOT / 'docs/coordination/c14-iteration-20261020-v2'
V = D / 'validation/final_formal_v1_preparation'
SOURCE = ROOT / 'tools/c14_iteration_20261020_v2/final_formal_v1'
TARGET = Path(__file__).parent / 'final_formal_bound_v1'


def fid(p):
    b = Path(p).read_bytes()
    return dict(bytes=len(b), sha256=hashlib.sha256(b).hexdigest())


def ref(p):
    return dict(path=str(p), identity=fid(p))


def read(p):
    return json.loads(Path(p).read_text(encoding='utf-8-sig'))


def main():
    index = V / 'CAPTURE_REUSE_INDEX_DRAFT.json'
    audit = V / 'RECORD_REUSE_AUDIT.json'
    assert fid(index)['sha256'] == '67aeb7207f4145e3869409e0d9fa344c099101a2b024d67c75080eb35c59b2da'
    assert fid(audit)['sha256'] == '1e069d2cc17db68b743942509684965b0ee9854a453b7e5c2d0c4d6981f212c3'
    acceptance = D / 'reviews/FINAL_FORMAL_SOURCE_ACCEPTANCE.json'
    assert fid(acceptance)['sha256'] == '46692a1bdb54d8a069d59510e042dcc56e562eb5f7841b81ed6210106eea99f0'
    accepted = read(acceptance)
    assert accepted['source_only_accepted'] is True
    pins = accepted['source_pins']
    assert len(pins) == 11
    for item in pins:
        value = item['derived']['path'].replace('\\', '/')
        if value.startswith('C:/'):
            value = '/mnt/c/' + value[3:]
        p = Path(value)
        assert SOURCE in p.parents
        assert fid(p) == item['derived']['identity']
    mainpath = SOURCE / 'six-role-campaign.py'
    assert fid(mainpath)['sha256'] == '92c79e792f666b8700981d05bbc71c52045cebc8090716484e2c2ef57d6f451f'
    original = mainpath.read_bytes()
    text = original.decode('utf-8')
    tree = ast.parse(text)
    binding = next(n for n in tree.body if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'STAGE_BINDINGS' for t in n.targets))
    stage = ast.literal_eval(binding.value)
    captures = read(index)['captures']
    assert stage['fixed_input_captures'] is None and set(captures) == {'normal', 'sanitized'}
    expected_roles = {'old_xoffset', 'new_xoffset', 'cista', 'flatbuffers', 'capnproto', 'cereal'}
    for mode, group in captures.items():
        assert set(group['roles']) == expected_roles
        assert all(len(record['files']) == 20 for record in group['roles'].values())
    stage['fixed_input_captures'] = captures
    replacement = ('STAGE_BINDINGS=' + repr(stage) + '\n').encode()
    lines = original.splitlines(keepends=True)
    updated = b''.join(lines[:binding.lineno - 1]) + replacement + b''.join(lines[binding.end_lineno:])
    newtree = ast.parse(updated.decode())
    newbinding = next(n for n in newtree.body if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'STAGE_BINDINGS' for t in n.targets))
    oldvalue = copy.deepcopy(binding.value)
    newbinding.value = oldvalue
    assert ast.dump(tree, include_attributes=False) == ast.dump(newtree, include_attributes=False)
    TARGET.mkdir(exist_ok=False)
    inventories = {}
    for p in sorted(SOURCE.rglob('*')):
        if not p.is_file() or p.suffix == '.patch':
            continue
        rel = p.relative_to(SOURCE)
        dst = TARGET / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(updated if p == mainpath else p.read_bytes())
        inventories[rel.as_posix()] = dict(authoring=ref(p), bound=ref(dst), identical=p != mainpath)
    assert len(inventories) == 11
    patch = V / 'FINAL_FORMAL_CAPTURE_BINDING.patch'
    with patch.open('x', encoding='utf-8', newline='') as stream:
        stream.write(''.join(difflib.unified_diff(text.splitlines(True), updated.decode().splitlines(True), fromfile='authoring/six-role-campaign.py', tofile='bound/six-role-campaign.py')))
    proof = V / 'FINAL_FORMAL_BOUND_SOURCE_READY.json'
    with proof.open('x', encoding='utf-8') as stream:
        json.dump(dict(status='source_ready_pending_independent_bound_review_not_executed',
            authoring_acceptance=ref(acceptance), capture_index=ref(index), capture_reuse_audit=ref(audit),
            target=str(TARGET), whitelist=['STAGE_BINDINGS.fixed_input_captures'],
            other_AST_and_function_text_unchanged=True, original_authoring_preserved=True,
            inventory=inventories, patch=ref(patch), resource_maxC_maxP_pending=True,
            native=0, compile=0, queue=0, freeze=0, QF_admitted=False), stream, indent=2)
        stream.write('\n')
    print(json.dumps(ref(proof)))


if __name__ == '__main__':
    main()
