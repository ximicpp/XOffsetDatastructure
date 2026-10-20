"""Only dispatch entry; root invokes outside every lease. No bench recompilation."""
from business_common import *

identity = require_final_identity()
require(read(B / 'bench-profiles-v2/result.json')['status'] == 'passed',
        'Actual bench/profile-v2 prerequisite not passed')
for path in ('functional-tools-v2', 'functional-tool-derivation-v2.json',
             'final-business-functional-v2', 'functional-normal-v2', 'functional-sanitized-v2',
             'business-compatibility-driver-v2'):
    require(not (B / path).exists(), 'Fresh dispatch target required: ' + path)
queue('functional-tools-v2', HERE / 'prepare-functional-tools.py', timeout=300)
argv = ['/usr/bin/python3', '-B', str(HERE / 'functional-coordinator.py')]
write(C / 'final-business-functional-v2-dispatch-request.json', {'argv': argv,
    'coordinator': fid(HERE / 'functional-coordinator.py'), 'source_identity': fid(MATERIALIZATION_FILE),
    'reused_bench_profiles': fid(B / 'bench-profiles-v2/result.json')})
result = subprocess.run(argv, capture_output=True, text=True, env=env())
write(C / 'final-business-functional-v2-dispatch-receipt.json',
      {'returncode': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
require(result.returncode == 0, 'Preserved receive-completion functional dispatch failure')
require(read(B / 'final-business-functional-v2/result.json')['status'] == 'passed' and
        require_final_identity() == identity, 'Receive-completion functional/source final state differs')
print('receive-completion original-tools normal80/SAN80 and compatibility passed; no formal window', flush=True)
