"""C14 targeted CMake normal/SAN jobs, serialized by the existing real run_job.

Run under WSL: python3 -I -B candidate_job.py check|run --request PATH.
The request contains only source overlays and explicit CMake targets/CTest names.
This entry deliberately has no timing, generic-command or process-pause action.
"""
import argparse
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

ROOT = Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
OLD = Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发')
RUNNER = OLD/'experiments/vm_coordination/run_job.py'
LOCK = OLD/'build/vm-coordination'
P1 = Path('/root/xoffset-serialization-comparison/20261017/p1-v1')
TOOLS = P1/'tools'
VENDOR = Path('/root/xoffset-receive-completion/20261019/v1/final-v4/source')
OUTPUT = Path('/root/xoffset-c14-iteration/20261020/v2/validation')
DOC = ROOT/'docs/coordination/c14-iteration-20261020-v2/validation/m2_full'
FIXED_TOOLS = [RUNNER, TOOLS/'cmake/bin/cmake', TOOLS/'cmake/bin/ctest',
               TOOLS/'p2996-profile/bin/clang++-libcxx', Path('/usr/bin/ninja')]

def read(p):
    return json.loads(Path(p).read_text(encoding='utf-8-sig'))

def fid(p):
    p=Path(p); h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''): h.update(b)
    return {'bytes':p.stat().st_size,'sha256':h.hexdigest()}

def write(p,d):
    with Path(p).open('x',encoding='utf-8') as f:
        json.dump(d,f,indent=2,ensure_ascii=False); f.write('\n')

def require(condition,message):
    if not condition: raise ValueError(message)

def relative(name):
    p=PurePosixPath(name)
    require(not p.is_absolute() and p.parts and '..' not in p.parts and '\\' not in name and ':' not in name,'unsafe source member: '+name)
    require(p.parts[0] not in ('.git','external','build','handoff'),'overlay outside candidate code closure: '+name)
    return name

def verify(request_path):
    q=read(request_path)
    require(q['schema_version']==1 and q['stage']=='S2_targeted','unsupported job stage')
    require(q['candidate'] in ('M1','L1','M2','M2'),'candidate outside registered set')
    require(re.fullmatch(r'[A-Za-z0-9_-]{1,80}',q['job_id']) is not None,'invalid job id')
    require(q['mode'] in ('normal','sanitized'),'unsupported correctness mode')
    for key in ('targets','tests'):
        require(q[key] and len(q[key])==len(set(q[key])) and all(re.fullmatch(r'test_[A-Za-z0-9_-]+',n) for n in q[key]),'explicit unique '+key+' required')
    s0=read(DOC/'S0_RESULT.json'); require(s0['status']=='passed','S0 not passed')
    material=read(ROOT/'handoff/github-20261019/C14_MATERIALIZATION.json')
    files={n:{'path':str(ROOT/n),'identity':v} for n,v in material['non_external_files'].items()}
    require(q['baseline_main_sha256']==material['production_files']['xoffsetdatastructure.hpp']['sha256'],'wrong baseline')
    for n,v in q['overlays'].items():
        relative(n); p=Path(v['path']).resolve(strict=True)
        require(p.is_relative_to(ROOT) or p.is_relative_to(Path('/root/xoffset-c14-iteration/20261019/v1/core')),'overlay source outside current iteration')
        files[n]={'path':str(p),'identity':v['identity']}
    for n,v in files.items(): require(fid(v['path'])==v['identity'],'changed source input: '+n)
    prod={n:files[n]['identity'] for n in material['production_files']}
    require(q['production_files']==prod,'candidate 14-file identity mismatch')
    for p,record in s0['toolchain']['files'].items(): require(fid(p)['sha256']==record['actual'],'toolchain changed: '+p)
    require(fid(RUNNER)==q['runner_identity'],'run_job identity mismatch')
    require(fid(__file__)==q['entry_identity'],'candidate entry identity mismatch')
    require(set(q['tool_files'])==set(map(str,FIXED_TOOLS)),'fixed tool closure required')
    for p,value in q['tool_files'].items(): require(fid(p)==value,'tool changed: '+p)
    for n,value in s0['toolchain']['sanitizer_files'].items():
        require(fid(TOOLS/'p2996-profile/sanitizer-resource'/n)==value,'sanitizer resource changed: '+n)
    out=OUTPUT/q['job_id']
    require(not out.exists(),'fresh output required: '+str(out))
    require(shutil.disk_usage('/root').free>=8*1024**3,'WSL volume below 8GiB reserve')
    require(q['backing_free_bytes']>=8*1024**3,'host backing below 8GiB reserve')
    stamp=datetime.datetime.fromisoformat(q['backing_observed_utc'].replace('Z','+00:00'))
    age=(datetime.datetime.now(datetime.timezone.utc)-stamp).total_seconds()
    require(0<=age<=900,'refresh actual Windows backing free-space observation within 15 minutes')
    return q,files,out

def execute(request_path,expected_request_sha):
    require(fid(request_path)['sha256']==expected_request_sha,'request changed before worker startup')
    q,files,out=verify(request_path)
    # The supervisor must actually hold the historical shared inode now.
    with (LOCK/'queue.lock').open('r+') as f:
        try:
            fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError: pass
        else:
            fcntl.flock(f,fcntl.LOCK_UN); raise ValueError('worker invoked outside real global lease')
        holder=json.load(f)
    require(holder['status']=='running' and Path(holder['out'])==OUTPUT/(q['job_id']+'-lease'),'wrong active lease identity')
    out.mkdir(parents=True,exist_ok=False)
    receipt={'schema_version':1,'stage':'S2_targeted','candidate':q['candidate'],'mode':q['mode'],
             'status':'running','started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
             'request':{'path':str(request_path),**fid(request_path)},'production_files':q['production_files'],
             'input_files':files,'entry_identity':fid(__file__),'runner_identity':fid(RUNNER),
             'lease_holder':holder,'performance_samples':0,'commands':[]}
    env=dict(LANG='C',LC_ALL='C',TZ='UTC',PATH='/usr/bin:/bin',
             ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:allocator_may_return_null=1',
             UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    receipt['environment']=env
    receipt['tool_files']=q['tool_files']
    def command(label,argv):
        argv=list(map(str,argv)); started=time.monotonic()
        with (out/(label+'.stdout.log')).open('xb') as stdout,(out/(label+'.stderr.log')).open('xb') as stderr:
            p=subprocess.run(argv,cwd=out,env=env,stdout=stdout,stderr=stderr)
        receipt['commands'].append({'label':label,'argv':argv,'returncode':p.returncode,'elapsed_seconds':time.monotonic()-started})
        require(p.returncode==0,label+' failed; preserve output and diagnose')
    try:
        source=out/'source'; source.mkdir()
        for n,v in files.items():
            p=source/n; p.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(v['path'],p)
            require(fid(p)==v['identity'],'snapshot copy mismatch: '+n)
        lock=read(ROOT/'SOURCE_MANIFEST.json')['dependency_lock']
        for name in list(lock['include_trees'])+list(lock['licenses']):
            src=VENDOR/name; dst=source/name; dst.parent.mkdir(parents=True,exist_ok=True)
            if src.is_dir(): shutil.copytree(src,dst)
            else: shutil.copyfile(src,dst)
        # Configure enforces the original dependency content lock again.
        flags='-stdlib=libc++ -pthread'
        if q['mode']=='sanitized': flags+=' -resource-dir='+str(TOOLS/'p2996-profile/sanitizer-resource')+' -fsanitize=address,undefined -fno-omit-frame-pointer -g'
        cmake=TOOLS/'cmake/bin/cmake'; build=out/'build'
        command('configure',[cmake,'-G','Ninja','-S',source,'-B',build,'-DCMAKE_BUILD_TYPE=Release',
            '-DCMAKE_CXX_COMPILER='+str(TOOLS/'p2996-profile/bin/clang++-libcxx'),'-DCMAKE_CXX_FLAGS='+flags,
            '-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
            '-DXOFFSET_BUILD_TESTS=ON','-DXOFFSET_BUILD_EXAMPLES=OFF','-DXOFFSET_BUILD_TOOLS=OFF','-DXOFFSET_INSTALL=OFF'])
        inv=read(build/'test-inventory.json'); require(inv['mode']=='full','storage-only cannot satisfy candidate job')
        names=inv['runtime_test_programs']+inv['subprocess_scenarios']+inv['tooling_checks']
        require(set(q['tests']).issubset(names),'requested tests missing from actual inventory')
        command('build',[cmake,'--build',build,'--parallel','2','--target',*q['targets']])
        receipt['compile_commands_identity']=fid(build/'compile_commands.json')
        receipt['assertion_policy']='Correctness tests retain project -UNDEBUG; this is not bare performance codegen.'
        junit=out/'ctest.xml'; regex='^('+'|'.join(re.escape(n) for n in q['tests'])+')$'
        command('ctest',[TOOLS/'cmake/bin/ctest','--test-dir',build,'--output-on-failure','--timeout','120','--output-junit',junit,'-R',regex])
        cases=list(ET.parse(junit).getroot().iter('testcase'))
        require(sorted(c.get('name') for c in cases)==sorted(q['tests']),'JUnit names/count mismatch')
        require(all(c.get('status')=='run' and all(c.find(t) is None for t in ('failure','error','skipped')) for c in cases),'JUnit contains failure or skip')
        for n,v in files.items():
            require(fid(v['path'])==v['identity'] and fid(source/n)==v['identity'],'input/source changed during validation: '+n)
        receipt.update(status='passed',junit_names=[c.get('name') for c in cases],junit_identity=fid(junit),inventory_identity=fid(build/'test-inventory.json'))
    except BaseException as e:
        receipt.update(status='rejected',reason=type(e).__name__+': '+str(e)); raise
    finally:
        receipt['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
        write(out/'receipt.json',receipt)

def main():
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('action',choices=('check','run','worker'))
    p.add_argument('--request',required=True,type=Path); p.add_argument('--request-sha256'); a=p.parse_args()
    request=a.request.resolve(strict=True)
    if a.action=='worker': execute(request,a.request_sha256); return
    q,files,out=verify(request)
    argv=['/usr/bin/python3','-I','-B',str(RUNNER),'run','--owner','W1','--kind','validation',
          '--out',str(OUTPUT/(q['job_id']+'-lease')),'--timeout-seconds','2400','--wait-seconds','0','--',
          '/usr/bin/python3','-I','-B',str(Path(__file__).resolve()),'worker','--request',str(request),'--request-sha256',fid(request)['sha256']]
    if a.action=='check': print(json.dumps({'status':'ready','argv':argv,'source_member_count':len(files),'new_cpp_executions':0})); return
    OUTPUT.mkdir(parents=True,exist_ok=True)
    result=subprocess.run(argv,cwd=ROOT)
    write(DOC/(q['job_id']+'-dispatch.json'),{'request':fid(request),'argv':argv,'returncode':result.returncode,
           'receipt':str(out/'receipt.json'),'lease_result':str(OUTPUT/(q['job_id']+'-lease')/'result.json')})
    raise SystemExit(result.returncode)

if __name__=='__main__':
    main()
