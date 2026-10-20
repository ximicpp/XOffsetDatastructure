from pathlib import Path
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from business_common import *
import shlex,shutil

O=B/'new-API-schema-compatibility-v2';O.mkdir(exist_ok=False)
S=SOURCE;baseline=BASELINE_SOURCE;fixture=S/'tests/compatibility_mutation.cpp'
require_baseline_identity()
shutil.copyfile(fixture,O/'shared-fixture.cpp')
entry=next(x for x in read(full_build()/'compile_commands.json') if x['file']==str(S/'tests/test_assign_range.cpp'))
args=entry.get('arguments') or shlex.split(entry['command'])
rows=[];binaries={}
for name,headers in [('baseline',baseline),('candidate',S)]:
    argv=[]
    for arg in args:
        if arg=='-c':continue
        if arg==str(S/'tests/test_assign_range.cpp'):argv.append(str(O/'shared-fixture.cpp'))
        else:argv.append(arg.replace(str(S),str(headers)))
    binary=O/(name+'-reader-writer');argv[argv.index('-o')+1]=str(binary)
    argv+=['-MD','-MF',str(O/(name+'.d'))]
    run(O,'compile-'+name,argv,180)
    deps=(O/(name+'.d')).read_text();assert str(headers/'xoffsetdatastructure.hpp') in deps
    if name=='baseline':assert str(S/'xoffsetdatastructure.hpp') not in deps
    else:assert str(baseline/'xoffsetdatastructure.hpp') not in deps
    binaries[name]=binary
for name,writer,reader in [('baseline-writer-candidate-reader',binaries['baseline'],binaries['candidate']),
                         ('candidate-writer-baseline-reader',binaries['candidate'],binaries['baseline'])]:
    wire=O/(name+'.wire');run(O,name+'-write',[writer,'--write-wire',wire],30)
    run(O,name+'-read',[reader,'--read-wire',wire],30)
    rows.append({'case':name,'writer':fid(writer),'reader':fid(reader),'wire':fid(wire),'status':'passed'})
write(O/'result.json',{'status':'passed','bidirectional_cases':2,'native_actions':4,
    'baseline_main':fid(baseline/'xoffsetdatastructure.hpp'),'candidate_main':fid(S/'xoffsetdatastructure.hpp'),
    'fixture':fid(fixture),'baseline_deps':fid(O/'baseline.d'),'candidate_deps':fid(O/'candidate.d'),
    'scope':'Existing fixed V1 schema; current uses public span API, baseline uses old resize+copy fallback; both readers verify and mutate loaded state.',
    'cases':rows,'formal_performance_sample':False})
print('Additive API nested V1 schema bidirectional baseline/current readers passed',flush=True)
