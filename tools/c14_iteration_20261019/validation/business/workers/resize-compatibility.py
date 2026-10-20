from pathlib import Path
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from business_common import *
import shlex, shutil

O=B/'resize-schema-compatibility-v2'; O.mkdir(exist_ok=False)
S=SOURCE; baseline=BASELINE_SOURCE
require_baseline_identity()
fixture=S/'tests/test_resize_growth.cpp'
shutil.copy2(fixture,O/'shared-fixture.cpp')
entry=next(x for x in read(full_build()/'compile_commands.json') if x['file']==str(fixture))
args=entry.get('arguments') or shlex.split(entry['command'])
newargs=[]
for arg in args:
    if arg=='-c': continue
    if arg==str(fixture): newargs.append(str(O/'shared-fixture.cpp'))
    else: newargs.append(arg.replace(str(S),str(baseline)))
index=newargs.index('-o'); newargs[index+1]=str(O/'baseline-reader-writer')
newargs+=['-MD','-MF',str(O/'baseline.d')]
run(O,'compile-baseline-fixture',newargs,180)
deps=(O/'baseline.d').read_text()
assert str(baseline/'xoffsetdatastructure.hpp') in deps and str(S/'xoffsetdatastructure.hpp') not in deps
old=O/'baseline-reader-writer'; new=test_binary('test_resize_growth')
rows=[]
for label,writer,reader in [('baseline-writer-final-reader',old,new),('final-writer-baseline-reader',new,old)]:
    wire=O/(label+'.wire')
    run(O,label+'-write',[writer,'--write-wire',wire],30)
    run(O,label+'-read',[reader,'--read-wire',wire],30)
    rows.append({'case':label,'writer':fid(writer),'reader':fid(reader),'wire':fid(wire),'status':'passed'})
write(O/'result.json',{'status':'passed','bidirectional_cases':2,'native_actions':4,
    'scope':'Same full P2 nested/default/fill-alias schema fixture against byte-pinned baseline and final header; no timing.',
    'baseline_main':fid(baseline/'xoffsetdatastructure.hpp'),'final_main':fid(S/'xoffsetdatastructure.hpp'),
    'fixture':fid(fixture),'compiled_fixture':fid(O/'shared-fixture.cpp'),'actual_baseline_dependencies':fid(O/'baseline.d'),
    'cases':rows,'performance_samples':False})
print('P2-specific nested/fill-alias V1 bidirectional readers passed; no timing',flush=True)
