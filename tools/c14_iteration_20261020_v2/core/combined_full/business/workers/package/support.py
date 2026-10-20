"""Small adapters to the existing root command/lease helpers; no runner."""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,stat,sys,xml.etree.ElementTree as ET
sys.path.insert(1,str(Path(__file__).resolve().parents[1]))
from common import B,H,P,P1,env,fid,read,write
from patch import headers
_INPUT_SPEC=None

def need(ok,message):
    if not ok:raise ValueError(message)

def get_spec(stage):
    global _INPUT_SPEC
    ap=argparse.ArgumentParser();ap.add_argument('--spec',type=Path,required=True);args=ap.parse_args()
    spec=read(args.spec);_INPUT_SPEC=spec
    need(spec['kind']=='c14-combined-package-input-v1','exact COMBINED package input')
    source=Path(spec['source']);need(source==Path('/root/xoffset-c14-iteration/20261020/v2/validation/combined-full-v1-normal/source'),'explicit COMBINED full source')
    own=headers(spec['own_headers']);check_production(source,spec['production_files'])
    for mode,ref in spec['full_receipts'].items():
        need(fid(ref['path'])==ref['identity'],'full receipt identity changed')
        r=read(ref['path']);need(r['status']=='passed' and r['production_files']==spec['production_files'] and len(r['junit_names'])==73,'candidate full73 required')
    for p,v in spec['bindings'].items():need(fid(p)==v,'tool/input binding changed: '+p)
    label={'package':'package-standard-v2','runtime':'package-runtime-v2'}[stage]
    out=B/label;out.mkdir(parents=True,exist_ok=False)
    write(out/'authority.json',P.queue_authority(B/'queues'/label))
    write(out/'input.json',{'spec':fid(args.spec),'production_files':spec['production_files']})
    return spec,source,out,own

def check_production(source,production):
    for name,value in production.items():need(fid(source/name)==value,'production changed: '+name)
    if _INPUT_SPEC:
        for p,v in _INPUT_SPEC['bindings'].items():need(fid(p)==v,'bound input changed: '+p)


def command(out,name,args,timeout=600,environ=None):
    environ=dict(environ if environ is not None else env())
    for name_ in ('CPATH','CPLUS_INCLUDE_PATH','C_INCLUDE_PATH'):environ.pop(name_,None)
    return P.command(out,name,list(map(str,args)),timeout,environ)

def flags(sanitized=False):
    value='-stdlib=libc++ -pthread'
    if sanitized:value+=' -resource-dir='+str(P1/'tools/p2996-profile/sanitizer-resource')+' -fsanitize=address,undefined -fno-omit-frame-pointer -g'
    return ['-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
      '-DCMAKE_CXX_COMPILER='+str(P1/'tools/p2996-profile/bin/clang++-libcxx'),
      '-DCMAKE_CXX_FLAGS='+value,'-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON']

def junit(path,expected):
    cases=list(ET.parse(path).getroot().iter('testcase'));actual=[c.get('name') for c in cases]
    need(len(actual)==len(set(actual)) and sorted(actual)==sorted(expected),'exact JUnit inventory')
    need(all(c.get('status')=='run' and not any(c.find(t) is not None for t in ('failure','error','skipped')) for c in cases),'all cases must execute/pass')
    return {'cases':len(cases),'junit':fid(path),'names':sorted(actual)}

def ctest(out,tag,build,expected):
    path=out/(tag+'.xml');command(out,tag+'-ctest',[P1/'tools/cmake/bin/ctest','--test-dir',build,'--output-on-failure','--timeout','120','--output-junit',path])
    return junit(path,expected)

def actual_deps(out,tag,build,allowed_include,forbidden,required):
    command(out,tag+'-deps',['/usr/bin/ninja','-C',build,'-t','deps'],60)
    path=out/(tag+'-deps.stdout');text=path.read_text();lines={x.strip() for x in text.splitlines()}
    for name in required:need(str(allowed_include/name) in lines,'missing actual include '+name)
    for value in forbidden:need(str(value) not in text,'original source/prefix used: '+str(value))
    return fid(path)

def copy_consumer(destination):
    shutil.copytree(Path(__file__).parent/'consumer',destination)

def build_c_consumer(out,tag,consumer,source=None,prefix=None,own=(),forbidden=()):
    build=out/(tag+'-build');args=[P1/'tools/cmake/bin/cmake','-S',consumer,'-B',build,*flags()]
    args+=['-DXC_SOURCE='+str(source)] if source else ['-DCMAKE_PREFIX_PATH='+str(prefix),'-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF','-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF']
    command(out,tag+'-configure',args,180);command(out,tag+'-build',[P1/'tools/cmake/bin/cmake','--build',build,'--parallel','2'])
    result=ctest(out,tag,build,['consumer_receive_paths'])
    result['actual_dependencies']=actual_deps(out,tag,build,(source if source else prefix/'include'),forbidden,own)
    return result
