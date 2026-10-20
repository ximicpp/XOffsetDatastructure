"""Parent runs once in existing W1 queue B/queues/bench-profiles-v2."""
from business_common import *

out = B / 'bench-profiles-v2'
out.mkdir(exist_ok=False)
write(out / 'authority.json', P.queue_authority(B / 'queues/bench-profiles-v2'))
identity = require_final_identity()
for kind in ('normal', 'sanitized'):
    full_record(kind) # Actual input-pinned inventory and dynamic expected count.
tools = [HERE / 'bench-gates.py', HERE / 'profiles-reuse-v1.py']
write(out / 'registration.json', {'source_identity': fid(MATERIALIZATION_FILE),
    'business_inputs': {'path': str(INPUTS_PATH), 'identity': INPUTS_ID},
    'tools': {str(path): fid(path) for path in tools}, 'new_contract_cases_each': 2,
    'unchanged_four_binaries_rebuilt': False, 'performance_samples': 0})
before = P.runtime()
for kind in ('normal', 'sanitized'):
    argv = ['/usr/bin/python3', '-B', tools[0]] + (['--sanitized'] if kind == 'sanitized' else [])
    P.command(out, 'bench-' + kind, argv, 900, env(kind == 'sanitized'))
P.command(out, 'profiles', ['/usr/bin/python3', '-B', tools[1]], 300, env())
require(P.runtime() == before and require_final_identity() == identity,
        'Tool runtime or final production changed')
write(out / 'result.json', {'status': 'passed', 'new_contract_cases_each': 2,
    'profiles': {kind: fid(B / ('profile-' + kind + '-v2.json')) for kind in ('normal', 'sanitized')},
    'functional_tool_copy': 'Not executed by bench worker; dispatch owns its fresh W1 copy stage',
    'business_inputs': INPUTS_ID, 'production_files': identity['production_files'],
    'performance_samples': 0})
