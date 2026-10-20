from pathlib import Path
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from business_common import *

O=B/'v1-compatibility-v2'; O.mkdir(exist_ok=False)
newbin=B/'bench-normal-v2/bin/sc_xoffset'
controls=[('previous-candidate',PRIOR/'bench-normal-v1/bin/sc_xoffset',PRIOR/'functional-normal-v1/commands',
 R/'docs/coordination/load-mutate-optimization-20261017/functional-normal-external-pin.json',
 '5fca3798b79235429aa1a436ad39efb774592df7aac04f4cabc55718883914ef',
 '9831ad07a874ba57b0a35bf0addc5574ed91873064fb6ccc086e450e0df3b9cc'),
 ('range-index',Path('/root/xoffset-verified-ranges/20261017/candidate-v1/repaired-campaign-v2/bench-normal-v1/bin/sc_xoffset'),
 Path('/root/xoffset-verified-ranges/20261017/candidate-v1/repaired-campaign-v2/functional-normal-v1/commands'),
 R/'docs/coordination/verified-ranges-repaired-campaign-20261017/functional-normal-external-pin-v1.json',
 'fe9f8bdefcbf6ad25984458a46cd4ce4fefa3658d94be3ad163d6d57cb94c94b',
 '09e3fb77a07d9fefbebcb6c0ecac2254d2f933fe89a203b7731c1d42765ed0db')]

prior_coord=R/'docs/coordination/performance-roadmap-v2-20261017'
acceptance=prior_coord/'final-acceptance.json'
assert fid(acceptance)['sha256']=='ff4896dc1c7f99170477c17dfe02cda72012354842eae5a1a28c8fdddc0ea587'
sealed=read(acceptance)
external=prior_coord/'functional-normal-external-pin.json'
assert fid(external)==sealed['coordination_members']['functional-normal-external-pin.json']
prior_pin=read(external)
controls.insert(0,('previous-v2',Path('/root/xoffset-performance-roadmap/20261017/v2')/'bench-normal-v1/bin/sc_xoffset',Path('/root/xoffset-performance-roadmap/20261017/v2')/'functional-normal-v1/commands',
                  external,fid(external)['sha256'],prior_pin['manifest']['sha256']))

_protected=RETAINED_SEALS
v3base=Path('/root/xoffset-performance-v3/20261017/v1')
v3coord=R/'docs/coordination/performance-v3-20261017'
v3accept=v3coord/'final-acceptance.json'
assert fid(v3accept)==_protected[str(v3accept)]
v3gates=v3coord/'combined-gates.json'
assert fid(v3gates)==read(v3accept)['gates']
v3external=v3coord/'functional-normal-external-pin.json'
v3pin=read(v3external)
assert v3pin==read(v3gates)['functionals']['normal']['external_pin']
controls.insert(0,('previous-v3',v3base/'bench-normal-v1/bin/sc_xoffset',v3base/'functional-normal-v1/commands',
                  v3external,fid(v3external)['sha256'],v3pin['manifest']['sha256']))

v4base=Path('/root/xoffset-performance-v4/20261018/v1')
v4coord=R/'docs/coordination/performance-v4-20261018'
v4accept=v4coord/'final-acceptance.json'
assert fid(v4accept)==_protected[str(v4accept)]
v4gates=v4coord/'combined-gates.json'
assert fid(v4gates)==read(v4accept)['gates']
v4external=v4coord/'functional-normal-external-pin.json'
v4pin=read(v4external)
assert v4pin==read(v4gates)['functionals']['normal']['external_pin']
controls.insert(0,('previous-v4',v4base/'bench-normal-v1/bin/sc_xoffset',v4base/'functional-normal-v1/commands',
                  v4external,fid(v4external)['sha256'],v4pin['manifest']['sha256']))

# Immediate V5 predecessor is an additional control, not a renamed V4.
v5base=Path('/root/xoffset-performance-v5/20261018/v1')
v5coord=R/'docs/coordination/performance-v5-20261018'
v5accept=v5coord/'final-acceptance.json'
assert fid(v5accept)==_protected[str(v5accept)]
v5sealed=read(v5accept)
assert v5sealed['execution_status']=='completed' and v5sealed['full_validation_and_V1_preserved'] is True
v5gates=v5coord/'combined-gates.json'
assert fid(v5gates)==v5sealed['evidence']['combined_gates']
v5external=v5coord/'functional-normal-external-pin.json'
v5pin=read(v5external)
assert v5pin==read(v5gates)['functionals']['normal']['external_pin']
controls.insert(0,('previous-v5',v5base/'bench-normal-v1/bin/sc_xoffset',v5base/'functional-normal-v1/commands',
                  v5external,fid(v5external)['sha256'],v5pin['manifest']['sha256']))
assert len(controls)==6


def match_member(bundle,manifest,relative):
    got=fid(bundle/relative);want=manifest['files'][relative]
    assert {'bytes':str(got['bytes']),'sha256':got['sha256']}==want,(bundle,relative)
    return got
rows=[];chains=[]
for control,oldbin,olddir,external_pin,pin_sha,manifest_sha in controls:
    assert oldbin.is_file() and olddir.is_dir()
    assert fid(external_pin)['sha256']==pin_sha
    pin=read(external_pin); bundle=olddir.parent
    assert fid(bundle/'manifest.json')['sha256']==manifest_sha==pin['manifest']['sha256']
    manifest=read(bundle/'manifest.json')
    for member in ('runtime-before.json','runtime-after.json'):
        match_member(bundle,manifest,member)
    runtime=read(bundle/'runtime-before.json')
    assert runtime==read(bundle/'runtime-after.json')
    old_id=fid(oldbin)
    assert {'bytes':str(old_id['bytes']),'sha256':old_id['sha256']}==runtime['binaries']['xoffset']
    chains.append({'control':control,'external_pin':fid(external_pin),'external_pin_path':str(external_pin),
       'manifest':fid(bundle/'manifest.json'),'runtime':fid(bundle/'runtime-before.json'),'reader':old_id})
    for size in ('S','M','L'):
        newdir=B/'functional-normal-v2/commands'
        for moment,ws,cs in [('initial','.input.wire','.initial.canonical'),('modified','.wire','.reloaded.canonical')]:
            for direction,reader,writerdir in [('old-writer-new-reader',newbin,olddir),('new-writer-old-reader',oldbin,newdir)]:
                name=control+'-'+size+'-'+moment+'-'+direction
                wire=writerdir/('xoffset-'+size+ws);want=writerdir/('xoffset-'+size+cs);out=O/(name+'.canonical')
                if writerdir==olddir:
                    match_member(bundle,manifest,wire.relative_to(bundle).as_posix())
                    match_member(bundle,manifest,want.relative_to(bundle).as_posix())
                run(O,name,['/usr/bin/taskset','-c','2',reader,'inspect','--wire',wire,'--canonical',out],30)
                assert fid(out)==fid(want),name
                rows.append({'case':name,'wire':fid(wire),'reader':str(reader),'reader_identity':fid(reader),'canonical':fid(out),'status':'passed'})
    assert fid(oldbin)==old_id
write(O/'result.json',{'status':'passed','required_old_range_reader_writer_cases':12,
    'previous_candidate_bidirectional_cases':12,'previous_v2_bidirectional_cases':12,'previous_v3_bidirectional_cases':12,'previous_v4_bidirectional_cases':12,'previous_v5_bidirectional_cases':12,'total_cases':72,'cases':rows,'old_control_provenance':chains,'formal_comparison':False})
print('72 V1 S/M/L initial/modified bidirectional checks passed',flush=True)
