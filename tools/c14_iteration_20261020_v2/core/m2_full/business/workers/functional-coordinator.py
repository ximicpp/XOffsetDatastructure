"""Root outside a lease; new original-tool functional80/audit/compat dispatch only."""
from business_common import *

identity = require_final_identity()
tool = B / 'functional-tools-v2/campaign.py'
derivation = read(B / 'functional-tool-derivation-v2.json')
names = ('campaign.py', 'core.py', 'protocol.py')
require(derivation['source_identity'] == identity and
        derivation['copied_tools'] == derivation['original_tools'] ==
        {name: fid(tool.parent / name) for name in names} ==
        {name: fid(BENCH_META / 'tools/serialization_comparison' / name) for name in names},
        'Three original tool files must be byte-identical to actual sealed source')
outcome = B / 'final-business-functional-v2'
outcome.mkdir(exist_ok=False)
bench_profiles = B / 'bench-profiles-v2/result.json'
require(read(bench_profiles)['status'] == 'passed', 'Reuse actual passed bench/profile-v2 only')
write(outcome / 'registration.json', {'source_identity': fid(MATERIALIZATION_FILE),
    'tool': fid(tool), 'tool_copy': fid(B / 'functional-tool-derivation-v2.json'),
    'reused_bench_profiles': fid(bench_profiles),
    'normal_children': 80, 'sanitized_children': 80,
    'cells_per_profile': 15, 'main_V1_cases': 84, 'extra_cases': 8,
    'AB_native_actions': 4, 'C_native_actions': 4, 'performance_samples': 0,
    'fresh_functional_encoding': True, 'fresh_functional80_uses_fixed_old_wire': False,
    'formal_measurement_executed_here': False,
    'business_inputs': {'path':str(INPUTS_PATH),'identity':INPUTS_ID}})
functionals = {}
for kind in ('normal', 'sanitized'):
    full_record(kind)
    bench = read(B / ('bench-' + kind + '-logs-v2/gate.json'))
    require(bench['status'] == 'passed' and bench['new_contract_cases'] == 2,
            'Two related XOffset contracts missing: ' + kind)
    profile_path = B / ('profile-' + kind + '-v2.json')
    require(read(bench_profiles)['profiles'][kind] == fid(profile_path), 'Reused profile differs from passed bench worker')
    binding = read(B / ('compiled-source-binding-' + kind + '-v2.json'))
    require(binding['profile'] == fid(profile_path) and
            binding['production_files'] == identity['production_files'] and
            binding['source'] == identity['production_files']['xoffsetdatastructure.hpp'] and
            binding['OS'] == identity['production_files']['xoffset_virtual_memory.hpp'],
            'Profile/source binding differs')
    profile = read(profile_path)
    require(profile['binaries']['xoffset']['library'] == CANDIDATE_ID and
            profile['binaries']['xoffset']['source_content_sha256'] == identity['source_content_sha256'],
            'Source/profile must execute all actual receive-completion production files')
    out = B / ('functional-' + kind + '-v2')
    require(not out.exists(), 'Fresh v2 functional output required: ' + str(out))
    argv = ['/usr/bin/python3', '-B', str(RUNNER),
        'run', '--owner', 'W3', '--kind', 'functional', '--out', str(out / 'queue'),
        '--timeout-seconds', '1800', '--wait-seconds', '3600', '--',
        '/usr/bin/python3', '-B', str(tool), 'functional',
        '--prepared', '/root/xoffset-serialization-comparison/20261017/implementation/prepared-v1',
        '--profile', str(profile_path), '--out', str(out), '--queue', str(out / 'queue')]
    write(C / ('functional-' + kind + '-v2-request.json'), {'argv': argv, 'tool': fid(tool)})
    result = subprocess.run(argv, capture_output=True, text=True, env=env(kind == 'sanitized'))
    write(C / ('functional-' + kind + '-v2-receipt.json'),
          {'returncode': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
    require(result.returncode == 0, 'Preserved receive-completion original-tool functional failure: ' + kind)
    main_manifest, queue_manifest = fid(out / 'manifest.json'), fid(out / 'queue/manifest.json')
    pin = hashlib.sha256(b'SC-POST-RUN-PIN-v1\0' + bytes.fromhex(main_manifest['sha256']) +
                         bytes.fromhex(queue_manifest['sha256'])).hexdigest()
    external = C / ('functional-' + kind + '-v2-external-pin.json')
    write(external, {'manifest': main_manifest, 'queue_manifest': queue_manifest, 'composite_pin': pin})
    queue('functional-' + kind + '-audit-v2', tool,
          ['audit', '--bundle', out, '--postrun-pin', pin], timeout=600)
    status = read(out / 'status.json')
    require(status['status'] == 'complete' and status['error'] is None and
            len(status['attempted']) == 80 and len(status['completed']) == 15 and
            status['unexecuted'] == [], 'Functional80/15 incomplete')
    before, after = read(out / 'runtime-before.json'), read(out / 'runtime-after.json')
    require(before == after and read(out / 'profile.json') == profile, 'Runtime/profile changed')
    for library, expected in binding['reused_four_expected_binary_identities'].items():
        require(before['binaries'][library] == expected, 'Reused binary differs: ' + library)
    functionals[kind] = {'bundle': str(out), 'manifest': main_manifest,
        'queue_manifest': queue_manifest, 'external_pin': fid(external), 'status': fid(out / 'status.json')}
queue('business-compatibility-v2', HERE / 'compatibility-worker.py', timeout=1500)
compat_path = B / 'business-compatibility-driver-v2/result.json'
compat = read(compat_path)
require(compat['status'] == 'passed' and compat['main_V1_cases'] == 84 and compat['extra_cases'] == 8 and
        compat['AB_bidirectional_cases'] == 2 and compat['AB_native_actions'] == 4 and
        compat['C_bidirectional_cases'] == 2 and compat['C_native_actions'] == 4,
        'V1 72+12,8 extras, independent AB4 and C4 incomplete')
require(require_final_identity() == identity, 'Final receive-completion source changed')
write(outcome / 'result.json', {'status': 'passed', 'native_business_children_each': 80,
    'business_cells_each': 15, 'contracts_each': 2, 'main_V1_cases': 84, 'extra_cases': 8,
    'AB_bidirectional_cases': 2, 'AB_native_actions': 4,
    'C_bidirectional_cases': 2, 'C_native_actions': 4,
    'business_inputs': INPUTS_ID,
    'production_files': identity['production_files'], 'functionals': functionals,
    'tool_copy': fid(B / 'functional-tool-derivation-v2.json'),
    'reused_bench_profiles': fid(bench_profiles), 'compatibility': fid(compat_path),
    'unchanged_competitor_binaries_rebuilt': False, 'performance_samples': 0,
    'fresh_functional_encoding': True, 'fresh_functional80_uses_fixed_old_wire': False,
    'compatibility_uses_sealed_old_control_wires': True, 'formal_measurement_executed_here': False})
