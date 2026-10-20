"""Read/hash the exact combined initial jobs; existing core dispatchers own leases.

No compiler, native child, scheduler, lock or retry is added by this guard.
"""
import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
D = ROOT / 'docs/coordination/c14-iteration-20261020-v2'
V = D / 'validation/combined_full'
OUT = Path('/root/xoffset-c14-iteration/20261020/v2/validation')
Q = D / 'core/REQUESTS_COMBINED_INITIAL.json'
E = D / 'core/REQUESTS_COMBINED_EXTERNAL.json'
I = ROOT / 'build/c14-iteration-20261020-v2/candidates/r1-m2-combined-v1/IDENTITY.json'
ADMISSION = V / 'INITIAL_EXECUTION_BINDING.json'
MAIN = '155f0847dff72047ac580ae8d3ead6927e52ff84eb5c9062ea2d4999980cd501'
QP = '3006c3057c6f05300d391e71cb5a70ccdfd0417abc1819bdc9df1c836bde0e2c'
EP = '88e8d3a1ab95c0ac5e6d2b65632a38666447f17d781a1ae028229610d0cf07ff'
RUNNER = Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py')
RUNNER_SHA = '6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909'
WRAPPER = Path('/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/bin/clang++-libcxx')
WID = dict(bytes=61, sha256='c8ea06a7c58557e3b89d09873dc88457d7b0c5d9af3f08cf0449093f2fbd1b87')


def need(ok, why):
    if not ok:
        raise ValueError(why)


def read(p):
    return json.loads(Path(p).read_text(encoding='utf-8-sig'))


def fid(p):
    p = Path(p)
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return dict(bytes=p.stat().st_size, sha256=h.hexdigest())


def ref(p):
    return dict(path=str(p), identity=fid(p))


def write(p, value):
    with p.open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2)
        stream.write('\n')


def check():
    need(fid(Q)['sha256'] == QP and fid(E)['sha256'] == EP, 'exact initial request changed')
    q, e, identity = read(Q), read(E), read(I)
    need(q['candidate'] == e['candidate'] == identity['candidate'] == 'r1-m2-combined-v1', 'combined candidate differs')
    need(q['candidate_identity'] == fid(I) and q['source_members'] == e['source_members'] == identity['members'], 'combined154 request differs')
    need(len(identity['members']) == 154 and identity['exact_two_patch_union_both_orders_equal'] is True
         and identity['M1_L1_included'] is False, 'exact two-patch union required')
    source = {n: fid(I.parent / 'source' / n) for n in identity['members']}
    need(all(source[n] == dict(bytes=v['bytes'], sha256=v['candidate_sha256']) for n, v in identity['members'].items()), 'current combined154 changed')
    need(source['xoffsetdatastructure.hpp']['sha256'] == MAIN and len(q['production14']) == 14
         and all(source[n] == v for n, v in q['production14'].items()), 'combined14 changed')
    need(fid(WRAPPER) == q['compiler_launcher']['identity'] == WID and fid(RUNNER)['sha256'] == RUNNER_SHA, 'original launcher/runner changed')
    acceptance = D / 'reviews/COMBINED_SOURCE_ACCEPTANCE.json'
    accepted = read(acceptance)
    need(fid(acceptance)['sha256'] == '6b92309f8e147df886fa2651a00a3cfb3b9d372aceedba073b08531e2e7bd31d'
         and accepted['combined_source_accepted'] is True and accepted['candidate'] == q['candidate'], 'independent source acceptance absent')
    authorization = D / 'reviews/COMBINED_FUNCTIONAL_EXECUTION_AUTHORIZATION.json'
    need(fid(authorization)['sha256'] == 'cd51fa1016c35269f1b6bc0bdf868a4d553ab86c4ae1a98a89b25da869980c16', 'exact finite functional authorization changed')
    inputs = {}
    for mapping in (q['tool_files'], e['input_files']):
        for name, expected in mapping.items():
            actual = fid(name)
            need(actual['sha256'] == expected['sha256'] and ('bytes' not in expected or actual['bytes'] == expected['bytes']), 'admitted input changed: ' + name)
            inputs[name] = actual
    for p in (Q, E, I, acceptance, authorization, RUNNER, V / 'WINDOWS_INITIAL_RESOURCES.json', Path(__file__).resolve()):
        inputs[str(p)] = fid(p)
    provider = {}
    base = Path('/root/xoffset-receive-completion/20261019/v1/final-v4/source')
    lock = read(ROOT / 'SOURCE_MANIFEST.json')['dependency_lock']
    for name in list(lock['include_trees']) + list(lock['licenses']):
        p = base / name
        for item in (sorted(p.rglob('*')) if p.is_dir() else [p]):
            if item.is_file():
                provider[item.relative_to(base).as_posix()] = fid(item)
                inputs[str(item)] = fid(item)
    need(len(provider) >= 851, 'complete locked provider required')
    return dict(source154=source, production14=q['production14'], inputs=inputs, provider=provider)


def queue(p, command, cap):
    q, r = read(p / 'request.json'), read(p / 'result.json')
    need(q['command'] == command and q['owner'] == 'W1' and q['kind'] == 'validation'
         and q['timeout_seconds'] == cap and q['out'] == str(p) and q['cwd'] == str(ROOT)
         and q['lock_dir'] == str(RUNNER.parents[2] / 'build/vm-coordination')
         and q['runner_sha256'] == RUNNER_SHA, 'actual original queue scope differs')
    need(r['run_id'] == q['run_id'] and r['status'] == 'complete' and r['command_returncode'] == 0
         and r['reason'] == 'command_exit' and r['cleanup'] == dict(live=[], process_groups=[], zombies=[]), 'actual queue failed or unclean')
    return dict(request=ref(p / 'request.json'), result=ref(p / 'result.json'), run_id=q['run_id'])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('stage', choices=('before', 'normal', 'sanitized', 'external'))
    a = parser.parse_args()
    actual = check()
    if a.stage == 'before':
        need(not ADMISSION.exists(), 'preserve initial binding')
        write(ADMISSION, dict(status='inputs_bound_not_executed', **actual, initial_request=ref(Q), external_request=ref(E),
              source_acceptance=ref(D / 'reviews/COMBINED_SOURCE_ACCEPTANCE.json'), new_native=0, new_compile=0))
        print('Combined initial exact inputs/source154/provider/61B bound;0 native')
        return
    before = read(ADMISSION)
    need(all(actual[k] == before[k] for k in ('source154', 'production14', 'inputs', 'provider')), 'initial inputs changed across actual execution')
    if a.stage == 'external':
        e = read(E)
        receipt_path = OUT / 'combined-external-v1/receipt.json'
        receipt = read(receipt_path)
        need(receipt['status'] == 'passed' and receipt['candidate'] == 'r1-m2-combined-v1'
             and receipt['request']['sha256'] == EP and len(receipt['commands']) == 8, 'own external4 incomplete')
        for recorded, expected in zip(receipt['commands'], e['commands']):
            need(all(recorded[k] == expected[k] for k in ('label', 'kind', 'argv')) and recorded['returncode'] == 0, 'actual external command differs')
        job = next(j for j in read(Q)['jobs'] if j['id'] == 'COMBINED-EXTERNAL4-v1')
        proof = queue(OUT / 'combined-external-v1-lease', job['argv'][job['argv'].index('--') + 1:], 900)
        count = dict(compiles=4, native=4)
    else:
        mode = a.stage
        p = OUT / ('combined-full-v1-' + mode)
        receipt_path = p / 'receipt.json'
        receipt = read(receipt_path)
        child = V / ('COMBINED_FULL_' + mode.upper() + '_REQUEST_v1.json')
        need(receipt['status'] == 'passed' and receipt['production_files'] == actual['production14']
             and receipt['request']['sha256'] == fid(child)['sha256'], 'actual own full receipt differs')
        need({n: fid(p / 'source' / n) for n in actual['source154']} == actual['source154'], 'actual full snapshot154 differs')
        need(all(fid(p / 'source' / n) == value for n, value in actual['provider'].items()), 'actual copied dependency provider differs')
        inv = read(p / 'build/test-inventory.json')
        expected = inv['runtime_test_programs'] + inv['subprocess_scenarios'] + inv['tooling_checks']
        cases = list(ET.parse(p / 'ctest.xml').getroot().iter('testcase'))
        need(inv['mode'] == 'full' and len(expected) == len(set(expected)) == len(cases) == 73
             and inv['configure_check_count'] == 3 and sorted(receipt['junit_names']) == sorted(expected)
             and sorted(c.get('name') for c in cases) == sorted(expected)
             and all(c.get('status') == 'run' and all(c.find(t) is None for t in ('failure', 'error', 'skipped')) for c in cases), 'actual73 inventory/JUnit differs')
        need(receipt['compile_commands_identity'] == fid(p / 'build/compile_commands.json')
             and receipt['inventory_identity'] == fid(p / 'build/test-inventory.json')
             and receipt['junit_identity'] == fid(p / 'ctest.xml'), 'actual compiled/full evidence identity differs')
        command = ['/usr/bin/python3', '-I', '-B', str(ROOT / 'tools/c14_iteration_20261020_v2/core/combined_full/full_entry.py'), 'worker',
                   '--request', str(child), '--request-sha256', fid(child)['sha256']]
        proof = queue(OUT / ('combined-full-v1-' + mode + '-lease'), command, 2400)
        count = dict(full_tests=73, separate_configure_probes=3)
    write(V / ('INITIAL_' + a.stage.upper() + '_VERIFIED.json'), dict(status='passed', stage=a.stage,
          actual_receipt=ref(receipt_path), queue=proof, source154_before_after_unchanged=True, copied_provider_verified=a.stage != 'external',
          compiler_launcher=ref(WRAPPER), initial_binding=ref(ADMISSION), counts=count, performance_samples=0,
          six_repository_targeted_coverage_separately_required=True, screen_admitted=False, QF_admitted=False))
    print('Combined ' + a.stage + ' actual evidence verified;0 new native in guard')


if __name__ == '__main__':
    main()
