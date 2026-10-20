"""Parent W1 queue: old72 + directV8 twelve +8 extras +separate AB4 CLI."""
from business_common import *

out = B / 'business-compatibility-driver-v2'
out.mkdir(exist_ok=False)
write(out / 'authority.json', P.queue_authority(B / 'queues/business-compatibility-v2'))
identity = require_final_identity()
entries = [('old-six', 'v1-compatibility.py', 'v1-compatibility-v2', 'total_cases', 72),
           ('direct-v8', 'v8-direct-compatibility.py', 'v8-direct-compatibility-v2', 'total_cases', 12),
           ('alpha-S', 'alpha-S-compatibility.py', 'alpha-S-compatibility-v2', 'extra_cases', 2),
           ('resize', 'resize-compatibility.py', 'resize-schema-compatibility-v2', 'bidirectional_cases', 2),
           ('string', 'string-compatibility.py', 'string-schema-compatibility-v2', 'bidirectional_cases', 2),
           ('new-API', 'new-API-compatibility.py', 'new-API-schema-compatibility-v2', 'bidirectional_cases', 2),
           ('AB', 'AB-compatibility.py', 'AB-schema-compatibility-v2', 'bidirectional_cases', 2),
           ('C', 'C-compatibility.py', 'C-schema-compatibility-v2', 'bidirectional_cases', 2)]
functionals = {}
for kind in ('normal', 'sanitized'):
    bundle = B / ('functional-' + kind + '-v2')
    external_path = C / ('functional-' + kind + '-v2-external-pin.json')
    external = read(external_path)
    require(fid(bundle / 'manifest.json') == external['manifest'] and
            fid(bundle / 'queue/manifest.json') == external['queue_manifest'] and
            hashlib.sha256(b'SC-POST-RUN-PIN-v1\0' + bytes.fromhex(external['manifest']['sha256']) +
                bytes.fromhex(external['queue_manifest']['sha256'])).hexdigest() == external['composite_pin'],
            'Current v2 functional manifest/composite pin differs')
    status = read(bundle / 'status.json')
    require(status['status'] == 'complete' and status['error'] is None and
            len(status['attempted']) == 80 and len(status['completed']) == 15 and not status['unexecuted'],
            'Current v2 functional80/15 incomplete')
    functionals[kind] = {'bundle': str(bundle), 'external_pin': fid(external_path),
        'manifest': fid(bundle / 'manifest.json'), 'queue_manifest': fid(bundle / 'queue/manifest.json')}
write(out / 'registration.json', {'tools': {name: fid(HERE / name) for _, name, _, _, _ in entries},
    'source_identity': fid(MATERIALIZATION_FILE), 'main_V1_cases': 84,
    'retained_old_control_cases': 72, 'direct_v8_cases': 12, 'extra_cases': 8,
    'AB_bidirectional_cases': 2, 'AB_native_actions': 4,
    'C_bidirectional_cases': 2, 'C_native_actions': 4, 'performance_samples': 0,
    'current_functionals': functionals, 'fresh_functional_encoding': True,
    'old_control_wires_retained': True, 'formal_measurement_executed_here': False})
before = P.runtime()
results = {}
for label, name, folder, key, count in entries:
    P.command(out, label, ['/usr/bin/python3', '-B', HERE / name], 600, env())
    result = read(B / folder / 'result.json')
    require(result['status'] == 'passed' and result[key] == count, 'Compatibility count differs: ' + label)
    if label in ('AB','C'):
        require(result['native_actions'] == 4 and len(result['cases']) == 2,
                'Independent '+label+' two-direction/four-CLI inventory differs')
    results[label] = fid(B / folder / 'result.json')
require(P.runtime() == before and require_final_identity() == identity, 'Runtime/source changed')
write(out / 'result.json', {'status': 'passed', 'main_V1_cases': 84,
    'retained_old_control_cases': 72, 'direct_v8_cases': 12, 'extra_cases': 8,
    'AB_bidirectional_cases': 2, 'AB_native_actions': 4,
    'C_bidirectional_cases': 2, 'C_native_actions': 4,
    'business_inputs': INPUTS_ID,
    'results': results, 'current_functionals': functionals, 'performance_samples': 0,
    'fresh_functional_encoding': True, 'old_control_wires_retained': True,
    'formal_measurement_executed_here': False})
