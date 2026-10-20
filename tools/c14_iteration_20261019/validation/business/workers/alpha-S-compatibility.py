from pathlib import Path
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from business_common import *

O=B/'alpha-S-compatibility-v2';O.mkdir(exist_ok=False)
oldbuild=Path('/root/xoffset-serialization-comparison/20261017/implementation/build-v1')
reader=oldbuild/'bin/sc_xoffset'
oldfunctional=oldbuild.parent/'functional-v2'
oldruntime=read(oldfunctional/'runtime-before.json')
actual=fid(reader);assert {**actual,'bytes':str(actual['bytes'])}==oldruntime['binaries']['xoffset']
alpha=P1/'sources/xoffset-source/xoffsetdatastructure-0.1.0-alpha.1'
header=alpha/'xoffsetdatastructure.hpp'
assert fid(header)['sha256']=='9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2'
deps=subprocess.run(['/usr/bin/ninja','-C',str(oldbuild),'-t','deps'],check=True,capture_output=True,text=True).stdout
(O/'actual-ninja-dependencies.txt').open('x').write(deps)
section=next(x for x in deps.split('\n\n') if x.startswith('CMakeFiles/sc_xoffset.dir/driver.cpp.o:'))
assert str(header) in section and str(SOURCE) not in section
run(O,'describe',['/usr/bin/taskset','-c','2',reader,'describe'],20)
description=read(O/'describe.log');assert description['library']=='xoffset-alpha1'
assert description['commit']=='7539433ab96d69c4a07b1db5c0a13786da85cdcd'
rows=[];newdir=B/'functional-normal-v2/commands'
for moment,suffix,canonical_suffix in [('initial','.input.wire','.initial.canonical'),('modified','.wire','.reloaded.canonical')]:
    name='S-'+moment+'-new-writer-alpha-reader';wire=newdir/('xoffset-S'+suffix);out=O/(name+'.canonical')
    run(O,name,['/usr/bin/taskset','-c','2',reader,'inspect','--wire',wire,'--canonical',out],30)
    assert fid(out)==fid(newdir/('xoffset-S'+canonical_suffix))
    rows.append(dict(case=name,wire=fid(wire),canonical=fid(out),status='passed'))
assert fid(reader)==actual
write(O/'result.json',dict(status='passed',extra_cases=2,cases=rows,
    scope='Supplement only: new writer -> existing alpha reader S initial/modified; no alpha M/L or bidirectional claim.',
    reader=str(reader),reader_identity=actual,alpha_header=fid(header),old_runtime_identity=fid(oldfunctional/'runtime-before.json'),
    dependency_section=section,compile_commands=fid(oldbuild/'compile_commands.json'),description=description,
    required_prior_compatibility_record=fid(B/'v1-compatibility-v2/result.json'),performance_samples=False))
print(json.dumps(dict(status='passed',extra_cases=2)))
