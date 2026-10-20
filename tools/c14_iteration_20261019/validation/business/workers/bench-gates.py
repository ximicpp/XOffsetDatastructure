"""Build only the changed XOffset driver and two related contracts; no timing."""
from pathlib import Path
import sys, json, shlex, xml.etree.ElementTree as ET
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from business_common import *

source = SOURCE
identity_before = require_final_identity()
san = '--sanitized' in sys.argv
kind = 'sanitized' if san else 'normal'
build = B / ('bench-' + kind + '-v2')
out = B / ('bench-' + kind + '-logs-v2')
if build.exists() or out.exists():
    raise RuntimeError('Require fresh build and logs')
out.mkdir()
implementation = Path('/root/xoffset-serialization-comparison/20261017/implementation')
args = read(implementation / 'integration-v1/configure.request.json')['argv']
for i, item in enumerate(args):
    if i and args[i-1] == '-S':
        args[i] = str(BENCH_META / 'experiments/serialization_comparison')
    elif i and args[i-1] == '-B':
        args[i] = str(build)
    elif item.startswith('-DSC_XOFFSET_SOURCE='):
        args[i] = '-DSC_XOFFSET_SOURCE=' + str(source)
    elif item.startswith('-DCMAKE_CXX_FLAGS=') and san:
        args[i] = item + ' -resource-dir=' + str(P1 / 'tools/p2996-profile/sanitizer-resource')
    elif item.startswith('-DSC_CAPNPROTO_PREFIX=') and san:
        args[i] = '-DSC_CAPNPROTO_PREFIX=' + str(implementation / 'install-capnp-asan-v2')
args += ['-DSC_CAPNP=' + str(P1 / 'install/capnproto/bin/capnp'),
         '-DSC_CAPNPC_CPP=' + str(P1 / 'install/capnproto/bin/capnpc-c++')]
if san:
    args += ['-DSC_ENABLE_SANITIZERS=ON']
run(out, 'configure', args, 180, env(san))
targets = ('sc_xoffset', 'sc_xoffset_contract', 'sc_xoffset_resource')
run(out, 'build', [P1 / 'tools/cmake/bin/cmake', '--build', build, '--parallel', '2',
                   '--target', *targets], 600, env(san))
binaries = {n: fid(build / 'bin' / n) for n in targets}
run(out, 'ninja-deps', ['/usr/bin/ninja', '-C', build, '-t', 'deps'], 120, env(san))
deps = (out / 'ninja-deps.log').read_text()
for header in identity_before['production_files']:
    found = {line.strip() for line in deps.splitlines() if Path(line.strip()).name == header}
    if found or header in ('xoffsetdatastructure.hpp', 'xoffset_virtual_memory.hpp'):
        assert found == {str(source / header)}, found
compiled = [r for r in read(build / 'compile_commands.json')
            if any('/' + n + '.dir/' in (r.get('output', '') + r.get('command', '')) for n in targets)]
assert len(compiled) == 3
for row in compiled:
    opts = row.get('arguments') or shlex.split(row['command'])
    assert '-O3' in opts and '-DNDEBUG' in opts
    assert '-freflection' in opts and '-fexpansion-statements' in opts
    assert [x for x in opts if x.startswith('-std=')][-1] == '-std=gnu++26'
    assert ('-fsanitize=address,undefined' in opts) == san
    if san:
        assert '-resource-dir=' + str(P1 / 'tools/p2996-profile/sanitizer-resource') in opts
write(out / 'compiled-binding-before.json', {'source': fid(source / 'xoffsetdatastructure.hpp'),
    'business_inputs': INPUTS_ID,
    'production_files': identity_before['production_files'],
    'OS': fid(source / 'xoffset_virtual_memory.hpp'), 'compile_commands': fid(build / 'compile_commands.json'),
    'ninja_deps': fid(out / 'ninja-deps.log'), 'binaries': binaries, 'targets': targets})
run(out, 'ctest', [P1 / 'tools/cmake/bin/ctest', '--test-dir', build, '-R', '^sc_xoffset_(contract|resource)$',
    '--output-on-failure', '--timeout', '60', '--output-junit', out / 'ctest.xml'], 180, env(san))
cases = list(ET.parse(out / 'ctest.xml').getroot().iter('testcase'))
assert {c.get('name') for c in cases} == {'sc_xoffset_contract', 'sc_xoffset_resource'} and len(cases) == 2
assert all(c.get('status') == 'run' and not any(c.find(t) is not None for t in ('failure','error','skipped')) for c in cases)
assert {n: fid(build / 'bin' / n) for n in targets} == binaries
assert require_final_identity() == identity_before
assert not any((build / 'bin' / ('sc_' + n)).exists() for n in ('cista', 'flatbuffers', 'capnproto', 'cereal'))
write(out / 'gate.json', {'status': 'passed', 'new_contract_cases': 2, 'binaries': binaries,
    'production_files': identity_before['production_files'],
    'unchanged_competitor_binaries_rebuilt': False, 'performance_measurement': False})
print(kind + ' changed XOffset adapter/resource contracts passed; no performance', flush=True)
