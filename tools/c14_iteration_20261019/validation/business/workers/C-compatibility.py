"""Four CLI actions: old normal typed load versus new direct verified receive.

The SAME final frozen fixture has an explicit old-header CLI branch. No new
business schema, adapter, phase or modified fixture is manufactured here.
"""
from business_common import *
import shutil

identity=require_final_identity()
baseline_identity=require_baseline_identity()
out=B/'C-schema-compatibility-v2';out.mkdir(exist_ok=False)
fixture=SOURCE/'tests/test_verified_receive.cpp'
copied=out/'shared-fixture.cpp';shutil.copyfile(fixture,copied)
require(fid(copied)==fid(fixture),'C fixture bytes changed')
text=fixture.read_text()
require('#if defined(XOFFSET_VERIFIED_RECEIVE_API)' in text and
        'xo::finish_verified(token)' in text and 'xo::TypedXBuffer<ReceiveRoot>::load_verified(wire)' in text and
        '--write-wire' in text and '--read-wire' in text,'Explicit old-load/new-direct-receive fixture CLI is required')
build=full_build()
entries=[row for row in read(build/'compile_commands.json') if row['file']==str(fixture)]
require(len(entries)==1,'Actual C full-test compile row missing/ambiguous')
entry=entries[0];args=entry.get('arguments') or shlex.split(entry['command'])
require(not any(x.startswith('-DXOFFSET_VERIFIED_RECEIVE_API') for x in args),
        'Receive API selection must come from actual headers, not forced old compile flags')
old=out/'baseline-reader-writer';argv=[]
for arg in args:
    if arg=='-c':continue
    if arg==str(fixture):argv.append(str(copied))
    else:argv.append(arg.replace(str(SOURCE),str(BASELINE_SOURCE)))
argv[argv.index('-o')+1]=str(old)
argv+=['-MD','-MF',str(out/'baseline.d')]
run(out,'compile-baseline-fixture',argv,180)
old_deps=set(shlex.split((out/'baseline.d').read_text().replace('\\\n',' ')))
require(str(BASELINE_SOURCE/'xoffsetdatastructure.hpp') in old_deps and
        str(SOURCE/'xoffsetdatastructure.hpp') not in old_deps and
        str(SOURCE/'xoffset_verified_receive.hpp') not in old_deps,
        'Old C fixture accidentally compiled the new header/API')
new=test_binary('test_verified_receive')
object_file=Path(entry.get('output') or args[args.index('-o')+1])
if object_file.is_absolute():
    require(object_file.is_relative_to(build),'C object output is outside actual full build')
    object_file=object_file.relative_to(build)
run(out,'actual-new-test-deps',['/usr/bin/ninja','-C',build,'-t','deps',object_file],120)
new_deps={line.strip() for line in (out/'actual-new-test-deps.log').read_text().splitlines() if line.startswith('    ')}
for name in ('xoffsetdatastructure.hpp','xoffset_virtual_memory.hpp','xoffset_verified_receive.hpp','receive_raw.hpp'):
    require({p for p in new_deps if Path(p).name==name}=={str(SOURCE/name)},
            'Actual direct-receive test dependency differs: '+name)
require(str(fixture) in new_deps,'Actual direct-receive test source absent from Ninja dependencies')
binaries={'baseline':fid(old),'candidate':fid(new)};rows=[]
for label,writer,reader,reader_mode in (
        ('baseline-writer-direct-receive-reader',old,new,'direct_verified_receive'),
        ('candidate-writer-baseline-load-reader',new,old,'old_TypedXBuffer_load_verified')):
    wire=out/(label+'.wire')
    run(out,label+'-write',[writer,'--write-wire',wire],30)
    run(out,label+'-read',[reader,'--read-wire',wire],30)
    rows.append({'case':label,'status':'passed','wire':fid(wire),'writer':fid(writer),
                 'reader':fid(reader),'reader_mode':reader_mode})
require(fid(old)==binaries['baseline'] and fid(new)==binaries['candidate'] and
        fid(fixture)==fid(copied) and require_final_identity()==identity and
        require_baseline_identity()==baseline_identity,'C compatibility source/binary changed')
write(out/'result.json',{'status':'passed','bidirectional_cases':2,'native_actions':4,
    'cases':rows,'fixture':fid(fixture),'compiled_fixture':fid(copied),
    'baseline_dependencies':fid(out/'baseline.d'),'actual_new_dependencies':fid(out/'actual-new-test-deps.log'),
    'full_compile_commands':fid(build/'compile_commands.json'),
    'full_ctest_metadata':fid(build/'tests/CTestTestfile.cmake'),
    'baseline_production':baseline_identity['production_files'],'candidate_production':identity['production_files'],
    'binaries':binaries,'performance_samples':0,'formal_measurement_executed_here':False,
    'scope':'The unchanged frozen ReceiveRoot CLI fixture checks its defined scalar/string/nested payload values through old normal typed loading and new direct receive. This is additional V1 interoperability, not a replacement for full C lifecycle/OOM/canonical/malicious-input targets or the separate 80-child nine-stage business truth.'})
