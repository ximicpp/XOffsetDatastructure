"""Read-only source equivalence and exact failed-output preservation; no child launch."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
D = ROOT / 'docs/coordination/c14-iteration-20261020-v2'
V = D / 'validation/combined_full'
OUT = Path('/root/xoffset-c14-iteration/20261020/v2/validation')


def fid(path):
    data = Path(path).read_bytes()
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def ref(path):
    return {'path': str(path), 'identity': fid(path)}


def main():
    request_path = D / 'core/REQUESTS_COMBINED_EXTERNAL.json'
    assert fid(request_path)['sha256'] == '88e8d3a1ab95c0ac5e6d2b65632a38666447f17d781a1ae028229610d0cf07ff'
    request = read(request_path)
    binding = read(V / 'INITIAL_EXECUTION_BINDING.json')
    candidate = ROOT / 'build/c14-iteration-20261020-v2/candidates/r1-m2-combined-v1/source'
    snapshot = OUT / 'combined-full-v1-normal/source'
    candidate_members = {name: fid(candidate / name) for name in binding['source154']}
    snapshot_members = {name: fid(snapshot / name) for name in binding['source154']}
    assert len(candidate_members) == 154
    assert candidate_members == snapshot_members == binding['source154']
    production = {name: snapshot_members[name] for name in binding['production14']}
    assert len(production) == 14 and production == binding['production14']
    assert production['xoffsetdatastructure.hpp']['sha256'] == '155f0847dff72047ac580ae8d3ead6927e52ff84eb5c9062ea2d4999980cd501'
    assert not (snapshot / 'version').exists() and (snapshot / 'VERSION').is_file()
    assert fid(candidate / 'version') == fid(candidate / 'VERSION') == fid(snapshot / 'VERSION')
    include_paths = []
    for command in request['commands']:
        argv = command['argv']
        paths = []
        for index, item in enumerate(argv):
            if item == '-I':
                paths.append(argv[index + 1])
            elif item.startswith('-I') and len(item) > 2:
                paths.append(item[2:])
        for name in paths:
            path = Path(name)
            include_paths.append({'label': command['label'], 'path': name,
                                  'VERSION_exists': (path / 'VERSION').exists(),
                                  'lowercase_version_exists': (path / 'version').exists(),
                                  'case_insensitive_project_root_requires_replacement':
                                  name.startswith('/mnt/c/') and (path / 'VERSION').exists()})
    lease = OUT / 'combined-external-v1-lease'
    failed_out = OUT / 'combined-external-v1'
    result = read(lease / 'result.json')
    receipt = read(failed_out / 'receipt.json')
    assert result['status'] == 'failed' and result['command_returncode'] == 1
    assert result['cleanup'] == {'live': [], 'process_groups': [], 'zombies': []}
    assert len(receipt['commands']) == 1 and receipt['commands'][0]['returncode'] == 1
    destinations = V / 'external-v1-failure-raw'
    destinations.mkdir(exist_ok=False)
    raw = []
    for parent, names in ((lease, ('request.json', 'result.json', 'manifest.json')),
                          (failed_out, ('receipt.json', 'normal-test_r1_thread_handoff_v2-compile.log'))):
        for name in names:
            src = parent / name
            dst = destinations / (('queue-' if parent == lease else '') + name)
            with dst.open('xb') as stream:
                stream.write(src.read_bytes())
            assert fid(src) == fid(dst)
            raw.append({'original': ref(src), 'preserved_copy': ref(dst)})
    proof = {
        'status': 'passed_source_equivalence_only', 'candidate': request['candidate'],
        'original_failed_request': ref(request_path),
        'candidate_source': str(candidate), 'verified_linux_snapshot': str(snapshot),
        'initial_binding': ref(V / 'INITIAL_EXECUTION_BINDING.json'),
        'normal73_actual_verification': ref(V / 'INITIAL_NORMAL_VERIFIED.json'),
        'sanitized73_actual_verification': ref(V / 'INITIAL_SANITIZED_VERIFIED.json'),
        'candidate_and_snapshot154_exact_equal': True, 'source154': snapshot_members,
        'production14': production,
        'production14_canonical_json_sha256': hashlib.sha256(json.dumps(production, sort_keys=True, separators=(',', ':')).encode()).hexdigest(),
        'all_original_command_include_paths': include_paths,
        'snapshot_VERSION': ref(snapshot / 'VERSION'), 'snapshot_lowercase_version_exists': False,
        'raw_failure_preserved': raw,
        'historical_failed_attempts': {'compile': 1, 'successful_compile': 0, 'native': 0},
        'new_compile': 0, 'new_native': 0,
        'corrective_request_not_yet_executed': True,
        'restriction': 'Every /mnt/c project root containing VERSION must be replaced with byte-equivalent case-sensitive snapshot inputs. No source or fixture edits, normal/SAN repetition, or new tests.'}
    target = V / 'EXTERNAL_CORRECTION_SNAPSHOT_PROOF.json'
    with target.open('x', encoding='utf-8') as stream:
        json.dump(proof, stream, indent=2)
        stream.write('\n')
    print(json.dumps({'proof': ref(target), 'production14_digest': proof['production14_canonical_json_sha256'],
                      'original_project_include_paths': [r for r in include_paths if r['case_insensitive_project_root_requires_replacement']],
                      'new_compile': 0, 'new_native': 0}))


if __name__ == '__main__':
    main()
