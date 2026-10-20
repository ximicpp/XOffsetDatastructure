"""Twelve additional cases: actual sealed direct V8, not a renamed V6 control."""
from business_common import *

out = B / 'v8-direct-compatibility-v2'
out.mkdir(exist_ok=False)
new_identity = require_final_identity()
bundle, manifest, provenance = sealed_v8('normal')
acceptance = read(C8 / 'final-acceptance.json')
old_source_identity = read(C8 / 'candidate-source-identity-v1.json')
require(fid(C8 / 'candidate-source-identity-v1.json') == acceptance['identity'] and
        old_source_identity['candidate_id'] == 'xoffset-performance-v8', 'V8 production identity differs')
for name, expected in old_source_identity['production_files'].items():
    require(fid(B8 / 'final-v1/source' / name) == expected, 'V8 control production changed')
require_baseline_identity() # Immediate source is now four-path B060, not V8.


def match_member(root, members, relative):
    identity = fid(root / relative)
    expected = members[relative]
    require(identity == {'bytes': int(expected['bytes']), 'sha256': expected['sha256']},
            'Sealed member differs: ' + str(root / relative))
    return identity


for name in ('runtime-before.json', 'runtime-after.json', 'profile.json', 'status.json'):
    match_member(bundle, manifest, name)
runtime = read(bundle / 'runtime-before.json')
require(runtime == read(bundle / 'runtime-after.json'), 'V8 runtime changed')
require(read(bundle / 'profile.json')['binaries']['xoffset']['library'] == 'xoffset-performance-v8',
        'Direct old control is not V8')
oldbin = B8 / 'bench-normal-v1/bin/sc_xoffset'
old_identity = fid(oldbin)
require(old_identity == {'bytes': int(runtime['binaries']['xoffset']['bytes']),
                        'sha256': runtime['binaries']['xoffset']['sha256']}, 'Actual V8 binary differs')
newbundle = B / 'functional-normal-v2'
newpin = read(C / 'functional-normal-v2-external-pin.json')
require(fid(newbundle / 'manifest.json') == newpin['manifest'] and
        fid(newbundle / 'queue/manifest.json') == newpin['queue_manifest'], 'New functional pin differs')
newmanifest = read(newbundle / 'manifest.json')['files']
match_member(newbundle, newmanifest, 'runtime-before.json')
newruntime = read(newbundle / 'runtime-before.json')
newbin = B / 'bench-normal-v2/bin/sc_xoffset'
newbin_id = fid(newbin)
require(newbin_id == {'bytes': int(newruntime['binaries']['xoffset']['bytes']),
                     'sha256': newruntime['binaries']['xoffset']['sha256']}, 'Final receive-completion binary differs')
rows = []
for size in ('S', 'M', 'L'):
    for moment, suffix, canonical_suffix in [('initial', '.input.wire', '.initial.canonical'),
                                            ('modified', '.wire', '.reloaded.canonical')]:
        for direction, reader, writer_bundle, members in (
                ('old-writer-new-reader', newbin, bundle, manifest),
                ('new-writer-old-reader', oldbin, newbundle, newmanifest)):
            name = 'direct-v8-' + size + '-' + moment + '-' + direction
            wire_rel = 'commands/xoffset-' + size + suffix
            canonical_rel = 'commands/xoffset-' + size + canonical_suffix
            wire_id = match_member(writer_bundle, members, wire_rel)
            expected = match_member(writer_bundle, members, canonical_rel)
            canonical = out / (name + '.canonical')
            run(out, name, ['/usr/bin/taskset', '-c', '2', reader, 'inspect', '--wire',
                writer_bundle / wire_rel, '--canonical', canonical], 30)
            require(fid(canonical) == expected, 'Direct V8 canonical differs: ' + name)
            rows.append({'case': name, 'wire': wire_id, 'canonical': fid(canonical),
                         'reader': str(reader), 'reader_identity': fid(reader), 'status': 'passed'})
require(len(rows) == 12 and fid(oldbin) == old_identity and fid(newbin) == newbin_id and
        require_final_identity() == new_identity, 'V8/new identity or case count changed')
write(out / 'result.json', {'status': 'passed', 'direct_v8_bidirectional_cases': 12,
    'total_cases': 12, 'cases': rows, 'old_control_provenance': provenance,
    'old_production_files': old_source_identity['production_files'],
    'new_production_files': new_identity['production_files'],
    'old_reader': old_identity, 'new_reader': newbin_id,
    'scope': 'Additional accepted direct V8 control, S/M/L initial/modified both directions. New writer inputs come from the fresh receive-completion functional bundle; each reader reproduces its pinned canonical truth. No cross-encode byte-identity requirement. Historical six-control72 retained separately. No formal window or fixed-old-input admission is started here.',
    'formal_comparison': False})
