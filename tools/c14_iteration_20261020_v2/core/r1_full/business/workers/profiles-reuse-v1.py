"""Root-run profile derivation after B-only builds; no build, native run or queue.

Prepared source only; execution remains root-owned. Run once under the lease after the
selected source/identity/protocol and both bench builds are final. The four
competitor entries are copied exactly from the sealed V5 functional profiles.
"""
from pathlib import Path
import ast
import copy
import hashlib
import json
import re
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from business_common import *

V5_ACCEPTANCE_SHA256 = 'd1d66631ec305df1685f345c57e73c4e3d2df72bb2c6acf73dee5de6ac7ac448'
OTHER_FOUR = ('cista', 'flatbuffers', 'capnproto', 'cereal')


def require(value, message):
    if not value:
        raise RuntimeError(message)


def normalized(identity):
    return {'bytes': int(identity['bytes']), 'sha256': identity['sha256']}


def match(path, identity):
    require(fid(path) == normalized(identity), 'Identity differs: ' + str(path))


def sealed_v5(kind):
    acceptance = OLD_C / 'final-acceptance.json'
    require(fid(acceptance)['sha256'] == V5_ACCEPTANCE_SHA256, 'V5 seal changed')
    sealed = read(acceptance)
    require(sealed['execution_status'] == 'completed' and
            sealed['functional_acceptance'] is True, 'V5 functional gate missing')
    gates = OLD_C / 'combined-gates.json'
    match(gates, sealed['evidence']['combined_gates'])
    external = OLD_C / ('functional-' + kind + '-external-pin.json')
    pin = read(external)
    require(pin == read(gates)['functionals'][kind]['external_pin'], 'V5 pin differs')
    bundle = OLD_B / ('functional-' + kind + '-v1')
    match(bundle / 'manifest.json', pin['manifest'])
    match(bundle / 'queue/manifest.json', pin['queue_manifest'])
    composite = hashlib.sha256(b'SC-POST-RUN-PIN-v1\0' +
        bytes.fromhex(pin['manifest']['sha256']) +
        bytes.fromhex(pin['queue_manifest']['sha256'])).hexdigest()
    require(composite == pin['composite_pin'], 'V5 composite pin differs')
    manifest = read(bundle / 'manifest.json')['files']
    for name in ('profile.json', 'runtime-before.json', 'runtime-after.json'):
        match(bundle / name, manifest[name])
    runtime = read(bundle / 'runtime-before.json')
    require(runtime == read(bundle / 'runtime-after.json'), 'V5 runtime changed')
    old_profile = OLD_B / ('profile-' + kind + '-v1.json')
    match(old_profile, manifest['profile.json'])
    return read(old_profile), runtime, {'seal': fid(acceptance), 'gates': fid(gates),
        'external_pin': fid(external), 'profile': fid(old_profile),
        'manifest': fid(bundle / 'manifest.json'), 'runtime': fid(bundle / 'runtime-before.json')}


def protocol_constants():
    names = {'IDENTITIES', 'CANDIDATE_CONTENT_SHA256', 'CANDIDATE_CLOSURE_IDS'}
    values = {}
    for statement in ast.parse((BENCH_META / 'tools/serialization_comparison/protocol.py').read_bytes()).body:
        if isinstance(statement, ast.Assign):
            for target in statement.targets:
                if isinstance(target, ast.Name) and target.id in names:
                    values[target.id] = ast.literal_eval(statement.value)
    require(set(values) == names, 'Candidate protocol constants missing')
    return values


def generate(kind, constants):
    build = B / ('bench-' + kind + '-v2')
    logs = B / ('bench-' + kind + '-logs-v2')
    profile_path = B / ('profile-' + kind + '-v2.json')
    binding_path = B / ('compiled-source-binding-' + kind + '-v2.json')
    require(not profile_path.exists() and not binding_path.exists(), 'Fresh profile outputs required')
    require(read(logs / 'gate.json')['status'] == 'passed' and
            read(logs / 'gate.json')['new_contract_cases'] == 2, 'New XOffset contracts not passed')
    previous, captured, provenance = sealed_v5(kind)
    value = copy.deepcopy(previous)
    closure = value['closure']
    old_source = OLD_B / 'final-v1/source'
    # Retain the old four actual dependency files, generated code, build metadata,
    # SDK/runtime/source locks. Remove stale XOffset singleton files/whole tree.
    drop = {str(old_source / name) for name in
            ('xoffsetdatastructure.hpp', 'xoffset_virtual_memory.hpp',
             'experiments/serialization_comparison/adapters/xoffset/adapter.hpp')}
    closure = {key: path for key, path in closure.items()
               if path not in drop and key not in
               ('candidate-source-tree', 'candidate-source-identity',
                'candidate-main-header', 'candidate-os-header')}
    for key in ('compile-commands', 'cmake-cache', 'generated-code', 'environment-lock'):
        if key in closure:
            closure['reused-four-' + key] = closure.pop(key)
    closure.update({'candidate-adapter': str(BENCH_META / 'experiments/serialization_comparison/adapters/xoffset/adapter.hpp'), 'candidate-protocol': str(BENCH_META / 'tools/serialization_comparison/protocol.py'), 'candidate-source-tree': str(SOURCE),
        'candidate-source-identity': str(IDENTITY_FILE),
        'candidate-main-header': str(SOURCE / 'xoffsetdatastructure.hpp'),
        'candidate-os-header': str(SOURCE / 'xoffset_virtual_memory.hpp'),
        'environment-lock': str(BENCH_META / 'experiments/serialization_comparison/locks/environment-p1-v1.json'),
        'compile-commands': str(build / 'compile_commands.json'),
        'cmake-cache': str(build / 'CMakeCache.txt'),
        'reused-four-sealed-profile': str(OLD_B / ('profile-' + kind + '-v1.json')),
        'reused-four-sealed-runtime': str(OLD_B / ('functional-' + kind + '-v1/runtime-before.json'))})
    candidate_identity = require_final_identity()
    require(constants['CANDIDATE_CONTENT_SHA256'] == candidate_identity['source_content_sha256'] and
            constants['IDENTITIES']['xoffset'][0] == CANDIDATE_ID,
            'Actual protocol candidate content/ID differs from production record')
    for name in sorted(candidate_identity['production_files']):
        label = ('candidate-main-header' if name == 'xoffsetdatastructure.hpp' else
                 'candidate-os-header' if name == 'xoffset_virtual_memory.hpp' else
                 'candidate-' + name.replace('.', '-').replace('_', '-'))
        closure[label] = str(SOURCE / name)
    old_bundle, old_members, old_provenance = sealed_v8(kind)
    closure.update({'fixed-V8-input-manifest': str(old_bundle / 'manifest.json'),
        'fixed-V8-input-external-pin': str(C8 / ('functional-' + kind + '-external-pin.json')),
        'fixed-V8-acceptance': str(C8 / 'final-acceptance.json')})
    raw_path = logs / 'ninja-deps.log'
    raw = raw_path.read_text(encoding='utf-8')
    section = next((part for part in raw.split('\n\n')
                    if part.startswith('CMakeFiles/sc_xoffset.dir/driver.cpp.o:')), None)
    require(section is not None, 'Actual new XOffset driver dependency section missing')
    dependencies = []
    for line in section.splitlines()[1:]:
        if line.startswith('    '):
            path = Path(line.strip())
            path = path if path.is_absolute() else build / path
            require(path.is_file(), 'Actual new driver dependency missing: ' + str(path))
            dependencies.append(str(path.resolve()))
    for name in candidate_identity['production_files']:
        found = {path for path in dependencies if Path(path).name == name}
        if found or name in ('xoffsetdatastructure.hpp','xoffset_virtual_memory.hpp'):
            require(found == {str(SOURCE / name)}, 'Candidate dependency singleton differs: ' + name)
    require(str(BENCH_META / 'experiments/serialization_comparison/driver.cpp') in dependencies,
            'Actual new driver source absent')
    for index, path in enumerate(sorted(set(dependencies))):
        closure['new-xoffset-actual-dependency-%04d' % index] = path
    # Actual production inventory is dynamic; no nine/13/14-name allowlist.
    critical = constants['CANDIDATE_CLOSURE_IDS']
    expected_records = list(candidate_identity['production_files'].values()) + [fid(IDENTITY_FILE)]
    identity_key = lambda v: (int(v['bytes']), v['sha256'])
    require(sorted(map(identity_key, critical.values())) == sorted(map(identity_key, expected_records)),
            'Protocol must bind every actual production file plus identity record')
    for key, identity in critical.items():
        if key not in closure:
            candidates = [str(SOURCE / n) for n,v in candidate_identity['production_files'].items()
                          if normalized(v) == normalized(identity)]
            if normalized(fid(IDENTITY_FILE)) == normalized(identity): candidates.append(str(IDENTITY_FILE))
            require(len(candidates) == 1, 'Critical protocol label cannot map uniquely: ' + key)
            closure[key] = candidates[0]
        match(closure[key], identity)
    for name in ('xoffsetdatastructure.hpp', 'xoffset_virtual_memory.hpp'):
        require({path for path in closure.values() if Path(path).name == name} ==
                {str(SOURCE / name)}, 'Stale candidate singleton in closure: ' + name)
    text = (BENCH_META / 'experiments/serialization_comparison/adapters/xoffset/adapter.hpp').read_text()
    def constant(name):
        found = re.search(r'\b' + name + r'\s*=\s*"([^"\n]+)"', text)
        require(found is not None, 'Actual XOffset adapter constant missing: ' + name)
        return found.group(1)
    entry = copy.deepcopy(previous['binaries']['xoffset'])
    entry.update(path=str(build / 'bin/sc_xoffset'), library=constant('library_id'),
                 commit=constant('commit'), trust_mode=constant('trust_mode'),
                 source_content_sha256=constant('source_content_sha256'))
    require((entry['library'], entry['commit']) == tuple(constants['IDENTITIES']['xoffset']) and
            entry['source_content_sha256'] == constants['CANDIDATE_CONTENT_SHA256'],
            'Adapter/protocol candidate identity differs')
    value['binaries']['xoffset'] = entry
    value['closure'] = closure
    require(value['lock_dir'] == str(R / 'build/vm-coordination'), 'Global lease namespace differs')
    for library in OTHER_FOUR:
        require(value['binaries'][library] == previous['binaries'][library], 'Reused entry changed')
    expected = {library: captured['binaries'][library] for library in OTHER_FOUR}
    # These four fixed binaries are small, real inputs. Check their sealed V5
    # bytes before launching any new functional child, then check captured
    # runtime-before/after against the same identities in the coordinator.
    for library, identity in expected.items():
        match(value['binaries'][library]['path'], identity)
    write(profile_path, value)
    write(binding_path, {'schema_version': 1, 'status': 'passed',
        'candidate_actual_header_binding_verified': True, 'profile': fid(profile_path),
        'source': fid(SOURCE / 'xoffsetdatastructure.hpp'),
        'OS': fid(SOURCE / 'xoffset_virtual_memory.hpp'), 'actual_xoffset_section': section,
        'production_files': candidate_identity['production_files'],
        'actual_xoffset_dependencies': len(set(dependencies)), 'ninja_deps': fid(raw_path),
        'new_binary': fid(build / 'bin/sc_xoffset'), 'inherited_v5_evidence': provenance,
        'reused_four_expected_binary_identities': expected,
        'fixed_V8_inputs': old_provenance,
        'actual_dependency_scope': 'Only headers actually included are Ninja dependencies; optional runtime cpp and other addons are separately fixed production closure files, not claimed linked into this driver',
        'reused_four_entries_exact': True, 'performance_measurement': False})


if __name__ == '__main__':
    identity_before = require_final_identity()
    constants = protocol_constants()
    for kind in ('normal', 'sanitized'):
        generate(kind, constants)
    require(require_final_identity() == identity_before, 'Receive-completion production changed during profile derivation')
