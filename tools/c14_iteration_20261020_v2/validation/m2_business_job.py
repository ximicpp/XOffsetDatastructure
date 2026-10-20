"""Bound M2-only continuation of the reviewed original functional workers.

The normal/SAN full tool is independent and remains frozen. This entry adds
current tool/provider/resource identity checks to each existing later stage.
Original functional coordinators stay outside a lease, avoiding nested locks.
"""
import argparse
import ast
import datetime
import fcntl
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

HERE=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('frozen_m2_full_validation',HERE/'m2_full_job.py')
F=importlib.util.module_from_spec(s);s.loader.exec_module(F)
ROOT,H,CORE,V,D,REVIEW=F.ROOT,F.H,F.CORE,F.V,F.D,F.REVIEW
read,fid,write,need,ref=F.read,F.fid,F.write,F.need,F.ref
BH=H/'m2_full/business';MH=H/'m2_full/business_matrix'
B=F.OUTPUT/'m2-business-v1';M=F.OUTPUT/'m2-business-matrix-v1';BD=D/'business'
STAGES=('prepare','bench','functional','package','runtime','lifecycle','final')
CAP=dict(prepare=300,bench=2400,package=2400,runtime=2400,lifecycle=3600,final=120)

def job_id(stage):
    return 'M2-LIFECYCLE52-v1' if stage=='lifecycle' else 'M2-FINAL-FUNCTIONAL-v1' if stage=='final' else 'M2-BUSINESS-'+stage+'-v1'
def lease(stage):
    return (F.OUTPUT/'leases/m2-business-prepare-v1') if stage=='prepare' else M/'queues/production-C-matrix-v2' if stage=='lifecycle' else B/'queues'/dict(bench='bench-profiles-v2',package='package-standard-v2',runtime='package-runtime-v2')[stage]
def worker_command(stage):
    if stage=='lifecycle':return ['/usr/bin/python3','-B',str(MH/'dispatch.py'),'worker']
    scripts=dict(prepare=BH/'prepare.py',bench=BH/'workers/bench-profiles-worker.py',package=BH/'workers/package/package-worker.py',runtime=BH/'workers/package/runtime-worker.py')
    args=[] if stage=='prepare' else ['--inputs',str(BD/'inputs.json')] if stage=='bench' else ['--spec',str(BD/'package-input.json')]
    return ['/usr/bin/python3','-B',str(scripts[stage]),*args]
def apath(stage):return REVIEW/('M2_FULL_ENTRY_BUSINESS_'+stage.upper()+'_ADMISSION.json')
def rpath(stage):return REVIEW/('M2_FULL_ENTRY_BUSINESS_'+stage.upper()+'_RESULT.json')
def keep(inputs,path,identity=None):
    path=Path(path);actual=fid(path);need(identity is None or F.matches(actual,identity),'changed bound input: '+str(path));inputs[str(path)]=actual

def inspect(stage,full_required):
    need(fid(F.INNER)['sha256']==F.INNER_SHA and fid(F.OUTER)['sha256']==F.OUTER_SHA,'frozen full requests changed')
    q=read(F.OUTER);inner=read(F.INNER);identity=read(F.IDENT)
    need(q==inner and q['compiler_launcher']==ref(F.WRAPPER) and q['source_members']==identity['members'],'single approved M2 source/launcher request')
    actual=F.source_ids(F.IDENT.parent/'source',identity['members']);prod={n:actual[n] for n in q['production14']}
    need(len(actual)==154 and len(prod)==14 and prod==q['production14'] and prod['xoffsetdatastructure.hpp']['sha256']==F.MAIN,'M2 actual154/14')
    inputs={}
    for p,v in q['tool_files'].items():keep(inputs,p,v)
    for p,v in read(CORE/'REQUESTS_M2_NATIVE.json')['input_files'].items():keep(inputs,p,v)
    keep(inputs,F.WRAPPER,F.WRAPPER_ID);keep(inputs,F.RUNNER)
    need(inputs[str(F.RUNNER)]['sha256']==F.RUNNER_SHA,'original runner changed')
    for p in (F.INNER,F.OUTER,F.IDENT,CORE/'REQUESTS_M2_NATIVE.json',V/'A_RESULT.json',V/'S0_RESULT.json',HERE/'m2_full_job.py',Path(__file__).resolve()):keep(inputs,p)
    need(read(V/'A_RESULT.json')['status']==read(V/'S0_RESULT.json')['status']=='passed','current A/S0')
    provider={};lock=read(ROOT/'SOURCE_MANIFEST.json')['dependency_lock']
    for n in list(lock['include_trees'])+list(lock['licenses']):
        p=F.VENDOR/n
        for item in (sorted(p.rglob('*')) if p.is_dir() else [p]):
            if item.is_file():keep(inputs,item);provider[item.relative_to(F.VENDOR).as_posix()]=fid(item)
    job=next(j for j in q['jobs'] if j['id']==job_id(stage))
    expected=['/usr/bin/python3','-B',str(MH/'dispatch.py'),'run'] if stage=='lifecycle' else ['/usr/bin/python3','-I','-B',str(H/'finalize_m2_full.py')] if stage=='final' else ['/usr/bin/python3','-B',str(BH/'dispatch.py'),stage]
    need(job['argv']==expected and job['timeout_seconds']==(None if stage=='functional' else CAP[stage]),'exact registered stage command/budget changed')
    for base in (BH,MH):
        for n,v in read(base/'TOOL_MANIFEST.json')['files'].items():need(fid(base/n)==v,'derived manifest member changed')
        for p in base.rglob('*.py'):ast.parse(p.read_text(encoding='utf-8-sig'))
    a=dict(stage=stage,source_before=actual,source_members=identity['members'],production_files=prod,inputs=inputs,dependency_provider=provider,
      outer_request=ref(F.OUTER),inner_request=ref(F.INNER),job=job,wrapper61_before=fid(F.WRAPPER),C_diagnostic_prerequisite=False,timing_samples=0,screen_admitted=False)
    if not (D/'M2_FULL_RESULT.json').exists():
        need(not full_required,'actual M2 own full normal73/SAN73 prerequisite missing')
        a['readiness']='source_ready_waiting_actual_full73x2';return a
    full=read(D/'M2_FULL_RESULT.json');need(full['status']=='passed' and full['request_identity']==fid(F.INNER) and full['production_files']==prod and full['normal_count']==full['sanitized_count']==73,'actual M2 full summary')
    keep(inputs,D/'M2_FULL_RESULT.json');keep(inputs,D/'M2_FULL_SOURCE_BINDING.json')
    for mode,key in (('normal','normal_receipt'),('sanitized','sanitized_receipt')):
        p=Path(full[key]['path']);keep(inputs,p,full[key]['identity']);receipt=read(p)
        need(receipt['status']=='passed' and receipt['production_files']==prod and len(receipt['junit_names'])==len(set(receipt['junit_names']))==73,'actual own full73 receipt')
        F.source_ids(p.parent/'source',identity['members'])
        for n,v in actual.items():keep(inputs,p.parent/'source'/n,v)
        for n,v in provider.items():keep(inputs,p.parent/'source'/n,v)
        for path,want in ((p.parent/'ctest.xml',receipt['junit_identity']),(p.parent/'build/test-inventory.json',receipt['inventory_identity']),(p.parent/'build/compile_commands.json',receipt['compile_commands_identity'])):keep(inputs,path,want)
    index=STAGES.index(stage)
    if index>0:
        previous=read(rpath(STAGES[index-1]));need(previous['status']=='passed','actual previous M2 functional stage required')
        keep(inputs,rpath(STAGES[index-1]))
        prepared=read(BD/'PREPARED.json');need(prepared['status']=='prepared' and prepared['production_files']==prod and prepared['requests']==fid(F.INNER),'current M2 prepared metadata')
        keep(inputs,BD/'PREPARED.json')
        for p,v in prepared['bindings'].items():keep(inputs,p,v)
    if stage in ('functional','package','runtime','lifecycle','final'):
        for kind in ('normal','sanitized'):
            profile=B/('profile-'+kind+'-v2.json');binding=B/('compiled-source-binding-'+kind+'-v2.json');value=read(profile)
            need(value['binaries']['xoffset']['library']=='xoffset-c14-m2-exact-owns-v1' and value['binaries']['xoffset']['source_content_sha256']==read(BD/'candidate-source-identity-v2.json')['source_content_sha256'],'actual M2 driver metadata label/content')
            keep(inputs,profile);keep(inputs,binding);keep(inputs,value['binaries']['xoffset']['path'])
            for lib in ('cista','flatbuffers','capnproto','cereal'):keep(inputs,value['binaries'][lib]['path'])
    if full_required:
        need(not rpath(stage).exists() and not apath(stage).exists(),'fresh one-attempt stage result/admission required')
        if stage not in ('functional','final'):need(not lease(stage).exists(),'fresh original lease output required')
    a['readiness']='ready';return a

def unchanged(a):
    for p,v in a['inputs'].items():need(fid(p)==v,'M2 functional input/tool changed: '+p)
    need(F.source_ids(F.IDENT.parent/'source',a['source_members'])==a['source_before'],'M2 candidate154 changed')
def prove(stage,sha):
    l=lease(stage);command=['/usr/bin/python3','-I','-B',str(Path(__file__).resolve()),'worker',stage,'--admission-sha',sha]
    with (F.LOCK/'queue.lock').open('r+') as f:
        try:fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:pass
        else:fcntl.flock(f,fcntl.LOCK_UN);raise ValueError('original global lease not held')
        f.seek(0);holder=json.load(f)
    q=read(l/'request.json')
    need(holder['status']=='running' and holder['run_id']==q['run_id'] and holder['out']==str(l) and holder['supervisor_pid']==os.getppid(),'own current out/run/supervisor')
    need(holder['owner']==q['owner']=='W1' and holder['kind']==q['kind']=='validation' and q['command']==command and q['out']==str(l) and q['lock_dir']==str(F.LOCK) and q['cwd']==str(ROOT) and q['runner_sha256']==F.RUNNER_SHA and q['timeout_seconds']==CAP[stage],'own exact command/cwd/runner/cap')
    need(Path('/proc/self/cmdline').read_bytes().split(b'\0')[:-1]==[x.encode() for x in command],'own actual cmdline')
    need((Path('/proc')/str(os.getppid())/'cmdline').read_bytes().split(b'\0')[:-1]==[b'/usr/bin/python3',str(F.RUNNER).encode(),b'_worker',str(l).encode()],'own actual supervisor argv')
    return dict(holder=holder,request=ref(l/'request.json'))
def queue_result(l,command,cap,owner='W1',kind='validation'):
    q=read(l/'request.json');r=read(l/'result.json')
    need(q['out']==str(l) and q['lock_dir']==str(F.LOCK) and q['cwd']==str(ROOT) and q['runner_sha256']==F.RUNNER_SHA and q['command']==command and q['timeout_seconds']==cap and q['owner']==owner and q['kind']==kind,'original queued command/identity mismatch: '+str(l))
    need(r['run_id']==q['run_id'] and r['status']=='complete' and r['command_returncode']==0 and r['reason']=='command_exit' and r['cleanup'] is not None and all(not value for value in r['cleanup'].values()),'original queue failed/unclean: '+str(l))
    return dict(request=ref(l/'request.json'),result=ref(l/'result.json'),run_id=q['run_id'])
def functional_queues():
    inputs=str(BD/'inputs.json');tool=B/'functional-tools-v2/campaign.py';rows=[]
    rows.append(queue_result(B/'queues/functional-tools-v2',['/usr/bin/python3','-B',str(BH/'workers/prepare-functional-tools.py'),'--inputs',inputs],300))
    for kind in ('normal','sanitized'):
        out=B/('functional-'+kind+'-v2')
        command=['/usr/bin/python3','-B',str(tool),'functional','--prepared','/root/xoffset-serialization-comparison/20261017/implementation/prepared-v1','--profile',str(B/('profile-'+kind+'-v2.json')),'--out',str(out),'--queue',str(out/'queue')]
        rows.append(queue_result(out/'queue',command,1800,owner='W3',kind='functional'))
        pin=read(BD/('functional-'+kind+'-v2-external-pin.json'))['composite_pin']
        rows.append(queue_result(B/('queues/functional-'+kind+'-audit-v2'),['/usr/bin/python3','-B',str(tool),'audit','--bundle',str(out),'--postrun-pin',pin],600))
    rows.append(queue_result(B/'queues/business-compatibility-v2',['/usr/bin/python3','-B',str(BH/'workers/compatibility-worker.py'),'--inputs',inputs],1500))
    return rows
def evidence(stage,a):
    p=BD/'PREPARED.json' if stage=='prepare' else B/'bench-profiles-v2/result.json' if stage=='bench' else B/'final-business-functional-v2/result.json' if stage=='functional' else B/'package-standard-v2/result.json' if stage=='package' else B/'package-runtime-v2/result.json' if stage=='runtime' else M/'production-C-matrix-v2/result.json' if stage=='lifecycle' else D/'M2_FUNCTIONAL_GATES.json'
    value=read(p);need(value['status']==('prepared' if stage=='prepare' else 'passed'),'actual M2 stage result required')
    if stage=='prepare':need(value['production_files']==a['production_files'] and value['requests']==fid(F.INNER),'M2 metadata source/request')
    if stage=='functional':need(value['native_business_children_each']==80 and value['main_V1_cases']==84 and value['extra_cases']==8 and value['AB_native_actions']==value['C_native_actions']==4,'actual functional80/V1/AB/C inventory')
    if stage=='lifecycle':need(value['actual_native_children']==52 and value['normal_model_cases']==value['sanitized_model_cases']==25 and value['pool_cases']==2,'actual lifecycle52')
    return ref(p)
def worker(stage,sha):
    need(fid(apath(stage))['sha256']==sha,'functional admission changed');a=read(apath(stage));proof=prove(stage,sha);unchanged(a)
    result=dict(status='running',stage=stage,admission=ref(apath(stage)),lease_proof=proof,wrapper61_before=fid(F.WRAPPER),timing_samples=0)
    try:
        command=worker_command(stage)
        with (lease(stage)/'retained-worker.log').open('xb') as log:r=subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        need(r.returncode==0,'original functional worker failed; preserve prefix')
        unchanged(a);prove(stage,sha);result.update(status='passed',actual_result=evidence(stage,a),wrapper61_after=fid(F.WRAPPER))
    except BaseException as error:result.update(status='failed',reason=type(error).__name__+': '+str(error));raise
    finally:write(REVIEW/('M2_FULL_ENTRY_BUSINESS_'+stage.upper()+'_WORKER.json'),result)
def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=('check','run','worker'));p.add_argument('stage',choices=STAGES);p.add_argument('--admission-sha');args=p.parse_args()
    if args.action=='worker':need(bool(args.admission_sha),'admission SHA');worker(args.stage,args.admission_sha);return
    a=inspect(args.stage,args.action=='run')
    if args.action=='check':print(json.dumps(dict(status=a['readiness'],stage=args.stage,source154=154,production14=14,tools_runtime_inputs=len(a['inputs']),wrapper61_explicit=True,new_compiles=0,new_native=0,new_queues=0)));return
    hostpath=V/('windows-m2-business-'+args.stage+'.json');host=read(hostpath)
    age=(datetime.datetime.now(datetime.timezone.utc)-datetime.datetime.fromisoformat(host['observed_utc'].replace('Z','+00:00'))).total_seconds()
    need(0<=age<=900 and host['backing_free_bytes']>=8*1024**3 and shutil.disk_usage('/root').free>=8*1024**3,'fresh per-stage actual host/backing and guest8GiB')
    keep(a['inputs'],hostpath);a['resource_observation']=ref(hostpath);write(apath(args.stage),a)
    direct=args.stage in ('functional','final')
    command=a['job']['argv'] if direct else ['/usr/bin/python3','-I','-B',str(Path(__file__).resolve()),'worker',args.stage,'--admission-sha',fid(apath(args.stage))['sha256']]
    argv=command if direct else ['/usr/bin/python3','-I','-B',str(F.RUNNER),'run','--owner','W1','--kind','validation','--out',str(lease(args.stage)),'--timeout-seconds',str(CAP[args.stage]),'--wait-seconds','0','--',*command]
    write(REVIEW/('M2_FULL_ENTRY_BUSINESS_'+args.stage.upper()+'_DISPATCH.json'),dict(argv=argv,admission=ref(apath(args.stage)),nested_lease=False))
    result=dict(status='running',stage=args.stage,admission=ref(apath(args.stage)),wrapper61_before=fid(F.WRAPPER),new_timing_samples=0,screen_admitted=False)
    try:
        r=subprocess.run(argv,cwd=ROOT,timeout=120 if args.stage=='final' else None);need(r.returncode==0,'M2 stage failed; preserve')
        unchanged(a)
        if args.stage=='functional':result['queues']=functional_queues()
        elif not direct:result['queue']=queue_result(lease(args.stage),command,CAP[args.stage])
        result.update(status='passed',actual_result=evidence(args.stage,a),wrapper61_after=fid(F.WRAPPER))
    except BaseException as error:result.update(status='failed',reason=type(error).__name__+': '+str(error));raise
    finally:write(rpath(args.stage),result)
    print('M2 '+args.stage+' own functional gate passed;0 timing')
if __name__=='__main__':main()
