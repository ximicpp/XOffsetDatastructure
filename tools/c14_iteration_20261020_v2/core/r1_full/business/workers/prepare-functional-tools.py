"""Root's existing W1 validation queue: copy three original tools BYTE-EXACT.

Fresh functional encodings are judged by original semantic/full-body gates,
not byte identity with a previous independently encoded wire. No sealed source,
old failed bundle, benchmark build/profile or formal fixed-old-wire input changes.
"""
from business_common import *
import shutil

out = B / 'functional-tools-v2'
out.mkdir(exist_ok=False)
write(out / 'authority.json', P.queue_authority(B / 'queues/functional-tools-v2'))
identity = require_final_identity()
source_tools = BENCH_META / 'tools/serialization_comparison'
names = ('campaign.py', 'core.py', 'protocol.py')
original = {}
for name in names:
    source = source_tools / name
    require(source.is_file() and not source.is_symlink(), 'Actual regular sealed source tool required: ' + name)
    original[name] = fid(source)
    shutil.copyfile(source, out / name)
copied = {name: fid(out / name) for name in names}
require(copied == original, 'Original source tool bytes changed during copying')
write(B / 'functional-tool-derivation-v2.json', {
    'schema_version': 1, 'kind': 'receive-completion-functional-original-tool-copy',
    'status': 'byte-identical-source-copy-native-pending',
    'source_identity': identity, 'materialization': fid(MATERIALIZATION_FILE),
    'author': fid(HERE / 'prepare-functional-tools.py'),
    'original_tools': original, 'copied_tools': copied,
    'delta': 'No code change: exact original campaign/core/protocol bytes, no extra post-encode fixed-byte guard or injected constants.',
    'functional_input_policy': 'Fresh original encode/inspect/nine-stage validate/run/truth and maintained full offline audit. Legal nondeterministic encoding is not rejected for differing old bytes.',
    'formal_input_policy': 'No Q/F is opened here. Any separately authorized fixed measurement input remains a distinct prerequisite, never replaced by fresh functional encoding.',
    'old_fixed_input_reference_modified': False, 'original_source_modified': False,
    'old_failed_functional_modified': False, 'native_executed_here': 0,
    'performance_measurement': False})
require(require_final_identity() == identity and
        {name: fid(source_tools / name) for name in names} == original,
        'Sealed source changed while copying original tools')
