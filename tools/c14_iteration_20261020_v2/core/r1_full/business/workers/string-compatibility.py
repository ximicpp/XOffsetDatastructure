from pathlib import Path
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from business_common import *
import shlex,shutil

O=B/'string-schema-compatibility-v2';O.mkdir(exist_ok=False)
source=SOURCE;baseline=BASELINE_SOURCE;fixture=source/'tests/test_string_replace.cpp'
require_baseline_identity()
shutil.copy2(fixture,O/'shared-fixture.cpp')
entry=next(e for e in read(full_build()/'compile_commands.json') if e['file']==str(fixture))
args=entry.get('arguments') or shlex.split(entry['command']);modified=[]
for arg in args:
    if arg=='-c':continue
    if arg==str(fixture): modified.append(str(O/'shared-fixture.cpp'))
    else: modified.append(arg.replace(str(source),str(baseline)))
modified[modified.index('-o')+1]=str(O/'baseline-reader-writer')
modified+=['-MD','-MF',str(O/'baseline.d')]
run(O,'compile-baseline-fixture',modified,180)
deps=(O/'baseline.d').read_text()
assert str(baseline/'xoffsetdatastructure.hpp') in deps and str(source/'xoffsetdatastructure.hpp') not in deps
old=O/'baseline-reader-writer';new=test_binary('test_string_replace')
rows=[]
for label,writer,reader in [('baseline-writer-final-reader',old,new),('final-writer-baseline-reader',new,old)]:
    wire=O/(label+'.wire');run(O,label+'-write',[writer,'--write-wire',wire],30)
    run(O,label+'-read',[reader,'--read-wire',wire],30)
    rows.append({'case':label,'writer':fid(writer),'reader':fid(reader),'wire':fid(wire),'status':'passed'})
write(O/'result.json',{'status':'passed','bidirectional_cases':2,'native_actions':4,
    'baseline_main':fid(baseline/'xoffsetdatastructure.hpp'),'final_main':fid(source/'xoffsetdatastructure.hpp'),
    'fixture':fid(fixture),'actual_baseline_dependency_file':fid(O/'baseline.d'),
    'scope':'Nested string/vector/attribute schema with final reload and further mutation; these CLI paths do not invoke the new alias regression against the known old assign implementation.',
    'cases':rows,'performance_samples':False})
print('String-specific nested V1 schema bidirectional read/write passed',flush=True)
