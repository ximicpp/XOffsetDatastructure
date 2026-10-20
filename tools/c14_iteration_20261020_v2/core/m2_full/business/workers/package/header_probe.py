"""Eleven compile-only first-include objects, with per-object real Ninja deps."""
import shlex
from support import *
from deps_audit import sections

def header_objects(out,source,own,kind,forbidden):
    sanitized=kind=='sanitized';tag=kind+'-public-header-objects'
    driver=out/(tag+'-source');driver.mkdir(exist_ok=False)
    shutil.copyfile(Path(__file__).parent/'header-probe/CMakeLists.txt',driver/'CMakeLists.txt')
    expected={}
    for i,name in enumerate(own):
        path=driver/('probe_'+str(i)+'.cpp')
        text='#include "'+name+'"\n'
        with path.open('x') as stream:stream.write(text)
        expected[str(path)]={'header':name,'input':fid(path),'exact_source':text}
    build=out/(tag+'-build');e=env(sanitized);cmake=P1/'tools/cmake/bin/cmake'
    command(out,tag+'-configure',[cmake,'-S',driver,'-B',build,*flags(sanitized),
        '-DXC_SOURCE='+str(source),'-DXC_OWN_HEADERS='+';'.join(own)],180,e)
    command(out,tag+'-build',[cmake,'--build',build,'--parallel','2'],600,e)
    rows=read(build/'compile_commands.json')
    need(len(rows)==11 and {r['file'] for r in rows}==set(expected),'exact eleven first-include compile rows')
    command(out,tag+'-deps',['/usr/bin/ninja','-C',build,'-t','deps'],60,e)
    raw=(out/(tag+'-deps.stdout')).read_text();objects=sections(raw)
    need(len(objects)==11,'exact eleven first-include dependency objects')
    for value in forbidden:need(str(value) not in raw,'first-include original source used '+str(value))
    records={};object_keys=set()
    for row in rows:
        item=expected[row['file']];path=Path(row['file'])
        need(path.read_text()==item['exact_source'] and fid(path)==item['input'],'first-include source changed')
        need(row['directory']==str(build),'first-include build directory')
        tokens=shlex.split(row['command']);need(tokens[0]==str(P1/'tools/p2996-profile/bin/clang++-libcxx'),'fixed compiler wrapper')
        need(tokens.count('-c')==1 and tokens[tokens.index('-c')+1]==str(path),'exact first-include compile input')
        need(tokens.count('-o')==1 and tokens[tokens.index('-o')+1]==row['output'],'exact first-include object output')
        need('-I'+str(source) in tokens,'clean decoded source include root')
        standards=[t for t in tokens if t.startswith('-std=')]
        need(standards and all(t in ('-std=gnu++26','-std=c++26') for t in standards),'first-include C++26')
        need('-freflection' in tokens and '-fexpansion-statements' in tokens and '-stdlib=libc++' in tokens,'fixed reflection and standard library flags')
        need(all(t=='-O3' for t in tokens if t.startswith('-O')) and '-O3' in tokens,'only O3 optimization')
        need('-DNDEBUG' in tokens and '-UNDEBUG' in tokens and tokens.index('-UNDEBUG')>tokens.index('-DNDEBUG'),'assertions enabled')
        resources=[t for t in tokens if t.startswith('-resource-dir=')]
        sanitizers=[t for t in tokens if t.startswith('-fsanitize=')]
        if sanitized:
            need(resources==['-resource-dir='+str(P1/'tools/p2996-profile/sanitizer-resource')] and sanitizers==['-fsanitize=address,undefined'] and '-fno-omit-frame-pointer' in tokens,'exact sanitizer configuration')
        else:need(not resources and not sanitizers,'normal object unexpectedly sanitized')
        output=Path(row['output']);absolute=output if output.is_absolute() else build/output
        key=str(absolute.relative_to(build));need(key in objects and key not in object_keys,'unique actual header object dependency section')
        object_keys.add(key)
        dependencies=objects[key];need(str(path) in dependencies and str(source/item['header']) in dependencies,'requested first header absent from this object actual deps')
        need(stat.S_ISREG(absolute.lstat().st_mode) and absolute.stat().st_size>0,'compiled nonempty object')
        records[item['header']]={'source':item['input'],'object':fid(absolute),'compile_row':row,'dependency_count':len(dependencies)}
    need(set(records)==set(own) and object_keys==set(objects),'complete exact first-include objects')
    result={'schema_version':1,'status':'passed','profile':kind,'object_count':11,'native_executions':0,
      'scope':'compile-only first include for each declared public own header; not a claim of default full-test use',
      'clean_source':str(source),'records':records,'compile_commands':fid(build/'compile_commands.json'),
      'actual_dependencies':fid(out/(tag+'-deps.stdout'))}
    write(out/(tag+'-acceptance.json'),result)
    return result
