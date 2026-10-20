"""Separate A/B model V1 compatibility: two directions, exactly four CLI actions."""
from business_common import *
import shlex, shutil

identity = require_final_identity()
out = B / 'AB-schema-compatibility-v2'
out.mkdir(exist_ok=False)
baseline = BASELINE_SOURCE
fixture = SOURCE / 'tests/test_verified_paths.cpp'
shutil.copyfile(fixture, out / 'shared-fixture.cpp')
entry = next(row for row in read(full_build('normal') / 'compile_commands.json')
             if row['file'] == str(fixture))
args = entry.get('arguments') or shlex.split(entry['command'])
argv = []
for arg in args:
    if arg == '-c':
        continue
    if arg == str(fixture):
        argv.append(str(out / 'shared-fixture.cpp'))
    else:
        argv.append(arg.replace(str(SOURCE), str(baseline)))
old = out / 'baseline-reader-writer'
argv[argv.index('-o') + 1] = str(old)
argv += ['-MD', '-MF', str(out / 'baseline.d')]
run(out, 'compile-baseline-fixture', argv, 180)
dependencies = set(shlex.split((out / 'baseline.d').read_text().replace('\\\n', ' ')))
require(str(baseline / 'xoffsetdatastructure.hpp') in dependencies and
        str(SOURCE / 'xoffsetdatastructure.hpp') not in dependencies,
        'A/B baseline fixture used the wrong main header')
old_identity = require_baseline_identity()
require(old_identity['candidate_id'] == BASELINE_ID, 'Require actual four-path baseline')
for name, expected in old_identity['production_files'].items():
    require(fid(baseline / name) == expected, 'A/B baseline production differs')
new = test_binary('test_verified_paths')
binaries = {'baseline': fid(old), 'candidate': fid(new)}
rows = []
for label, writer, reader in [('baseline-writer-final-reader', old, new),
                               ('final-writer-baseline-reader', new, old)]:
    wire = out / (label + '.wire')
    run(out, label + '-write', [writer, '--write-wire', wire], 30)
    run(out, label + '-read', [reader, '--read-wire', wire], 30)
    rows.append({'case': label, 'writer': fid(writer), 'reader': fid(reader),
                 'wire': fid(wire), 'status': 'passed'})
require(fid(old) == binaries['baseline'] and fid(new) == binaries['candidate'] and
        require_final_identity() == identity, 'A/B fixture source/binary changed')
write(out / 'result.json', {'status': 'passed', 'bidirectional_cases': 2, 'native_actions': 4,
    'cases': rows, 'fixture': fid(fixture), 'compiled_fixture': fid(out / 'shared-fixture.cpp'),
    'baseline_deps': fid(out / 'baseline.d'), 'binaries': binaries,
    'baseline_production': old_identity['production_files'],
    'candidate_production': identity['production_files'],
    'scope': 'Independent A/B model only, immediate four-path baseline and new receive-completion source. Candidate writes joined immutable snapshot segments; both readers check the same PathsRoot model. C is separately tested, not inferred from this gate.',
    'performance_samples': 0})
