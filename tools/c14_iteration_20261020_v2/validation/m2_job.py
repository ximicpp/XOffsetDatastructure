"""Exact M2-only stage, candidate -S, and four normal/four SAN entry.

check is read-only. run actions always dispatch through the original global
runner; worker independently proves this exact lease/run/command ownership.
No control compilation, mechanism native, screen, retry, or generic job API.
"""
import argparse
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT=Path(__file__).resolve().parents[3]
CORE=ROOT/'docs/coordination/c14-iteration-20261020-v2/core'
D=ROOT/'docs/coordination/c14-iteration-20261020-v2/validation'
REVIEW=D/'review'
OLD=Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发')
RUNNER=OLD/'experiments/vm_coordination/run_job.py'
LOCK=OLD/'build/vm-coordination'
OUTPUT=Path('/root/xoffset-c14-iteration/20261020/v2/validation')
SOURCE=OUTPUT/'m2-v1/source'
CANDIDATE=ROOT/'build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1'
EXPERIMENT=ROOT/'handoff/continuation-20261019-v1/experiments/serialization_comparison'
P1=Path('/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile')
VENDOR=Path('/root/xoffset-receive-completion/20261019/v1/final-v4/source')
NATIVE_WORKER=ROOT/'tools/c14_iteration_20261020_v2/core/run_native_targets.py'
MAIN='3e347612c1a62299bb910c68dd6b1411663f3165e18e298a51e7ba2e062c201f'
RUNNER_SHA='6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909'
FROZEN={
 'REQUESTS_M2_CODEGEN.json':(146703,'16cf5688e44d752602509547095ed2401f277b9e84a3baf709aba8cd5590851b'),
 'REQUESTS_M2_NATIVE.json':(163031,'ed3de3d994c346071e8c381b16b469e70df73d9e2b05840553c0c1ac187f6618'),
 'M2_IDENTITY.json':(44233,'4a62016c53b103964d4a0b14ce56e3b717b59b54b011efbd52c021b7114b944c'),
 'REVIEW_M2.md':(16470,'0981720db0c638718361e11d7d9b1ccadc7be79b7a4ee00df41f0d2830ce475f')}
ACCEPT=ROOT/'docs/coordination/c14-iteration-20261020-v2/reviews/M2_SOURCE_ACCEPTANCE.json'
ACCEPT_ID={'bytes':1509,'sha256':'264c04a7d17b8f301d0dc0d49134174eaa7f453d2dedcc4ce713ca15213c6bb3'}
NAMES=('test_same_arena_move','test_locator_soundness','test_owner_identity','test_m2_owns')
ENV=dict(PATH='/usr/bin:/bin',LANG='C',LC_ALL='C',TZ='UTC',
 ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:allocator_may_return_null=1',
 UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')

def need(ok,message):
    if not ok:raise ValueError(message)
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def fid(p):
    p=Path(p);h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):h.update(block)
    return dict(bytes=p.stat().st_size,sha256=h.hexdigest())
def matches(actual,declared):
    return actual['sha256']==declared['sha256'] and ('bytes' not in declared or actual['bytes']==declared['bytes'])
def write(p,value):
    with Path(p).open('x',encoding='utf-8') as f:json.dump(value,f,indent=2,ensure_ascii=False);f.write('\n')
def safe_member(n):
    p=Path(n);need(not p.is_absolute() and '..' not in p.parts and '\\' not in n,'unsafe source member')
    return n
def member_ids(path,members):
    result={n:fid(path/safe_member(n)) for n in members}
    need(result=={n:dict(bytes=v['bytes'],sha256=v['candidate_sha256']) for n,v in members.items()},'complete154 source changed: '+str(path))
    return result
def paths(phase):
    out=OUTPUT/('m2-'+phase+'-v1');lease=OUTPUT/('m2-'+phase+'-v1-lease')
    return out,lease,REVIEW/('M2_'+phase.upper()+'_ADMISSION.json')
def expected_native():
    includes=['external/typelayout/include']+['external/boost/libs/'+n+'/include' for n in
      ('config','container','container_hash','core','intrusive','predef','static_assert','throw_exception','type_traits','winapi')]
    base=[str(P1/'bin/clang++-libcxx'),'-std=gnu++26','-freflection','-fexpansion-statements','-stdlib=libc++','-pthread','-O3','-DNDEBUG']
    base+=['-I'+str(VENDOR/n) for n in includes]+['-UNDEBUG','-I'+str(SOURCE),'-I'+str(SOURCE/'tests')]
    result=[]
    for mode in ('normal','san'):
        flags=[] if mode=='normal' else ['-resource-dir='+str(P1/'sanitizer-resource'),'-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
        for name in NAMES:
            src=ROOT/'tools/c14_iteration_20261020_v2/core/test_m2_owns.cpp' if name=='test_m2_owns' else SOURCE/'tests'/(name+'.cpp')
            binary=OUTPUT/'m2-native-v1'/(mode+'-'+name)
            result += [dict(label=mode+'-'+name+'-compile',kind='compile',timeout_seconds=120,argv=base+flags+[str(src),'-o',str(binary)]),
              dict(label=mode+'-'+name+'-run',kind='native',timeout_seconds=20,argv=[str(binary)])]
    return result

def inspect(phase,fresh=True):
    cg=read(CORE/'REQUESTS_M2_CODEGEN.json');native=read(CORE/'REQUESTS_M2_NATIVE.json');identity=read(CORE/'M2_IDENTITY.json')
    bound={}
    for name,(size,sha) in FROZEN.items():
        actual=fid(CORE/name);need(actual==dict(bytes=size,sha256=sha),'reviewed M2 document changed: '+name);bound[str(CORE/name)]=actual
    need(fid(ACCEPT)==ACCEPT_ID,'independent exact source acceptance changed');bound[str(ACCEPT)]=fid(ACCEPT)
    accept=read(ACCEPT);need(accept['status']=='source_admitted_for_registered_checks_only' and accept['candidate_main_sha256']==MAIN,'source gate')
    need(read(D/'A_RESULT.json')['status']==read(D/'S0_RESULT.json')['status']=='passed','A/S0 prerequisite')
    material=read(ROOT/'handoff/github-20261019/C14_MATERIALIZATION.json')
    need(identity['members']==cg['members']==native['source_members'] and len(identity['members'])==identity['member_count']==154,'complete154 identity mismatch')
    need(identity['baseline_commit']=='1904b9c71230026ac6f2d58f919a9ab76447d264' and identity['changed']==['xoffsetdatastructure.hpp'] and identity['R1_or_M1_combined'] is False,'independent C14-only M2 required')
    need({n:v['baseline_sha256'] for n,v in identity['members'].items()}=={n:v['sha256'] for n,v in material['non_external_files'].items()},'baseline154 identity mismatch')
    need(fid(CANDIDATE/'IDENTITY.json')==fid(CORE/'M2_IDENTITY.json')==cg['candidate_identity'],'candidate identity file differs')
    actual=member_ids(CANDIDATE/'source',identity['members']);prod={n:actual[n] for n in material['production_files']}
    need(len(prod)==14 and prod['xoffsetdatastructure.hpp']['sha256']==native['production_main_sha256']==MAIN,'production14/main differs')
    for q in (cg,native):
        need(q['status']=='ready_for_validation_review' and q['candidate']=='m2-exact-owns-v1' and q['source']==str(SOURCE) and q['timing_admitted'] is False,'wrong M2 registration')
    need(cg['stage']=='D_M2_codegen' and cg['control_compiles']==cg['native_budget']==0 and cg['candidate_compiles']==1 and cg['task_timeout_seconds']==240 and cg['compile_timeout_seconds']==120,'codegen budget mismatch')
    need(cg['stage_argv']==['python3','-I','-B',str(ROOT/'tools/c14_iteration_20261020_v2/core/stage_source.py'),'--candidate',str(CANDIDATE),'--out',str(SOURCE)],'stage argv changed')
    prior_path=ROOT/'docs/coordination/c14-iteration-20261019-v1/validation/R1_CODEGEN_JOB_ADMISSION.json'
    prior=read(prior_path);expected=[x.replace('/root/xoffset-c14-iteration/20261019/v1/validation/r1-v1/source',str(SOURCE)).replace('/root/xoffset-c14-iteration/20261019/v1/validation/r1-v1/codegen/candidate-driver.s',str(OUTPUT/'m2-codegen-v1/candidate-driver.s')) for x in prior['argv']]
    need(cg['compile_argv']==expected and '-UNDEBUG' not in expected and '-S' in expected,'candidate-only original bare driver argv differs')
    ref=cg['reference_control'];need(fid(ref['path'])==dict(bytes=ref['bytes'],sha256=ref['sha256']),'fixed control asm changed')
    need(native['stage']=='D_M2_targeted' and native['output']==str(OUTPUT/'m2-native-v1') and native['global_lease_file']==str(LOCK/'queue.lock'),'native output/lease mismatch')
    need(native['compile_budget']==native['native_budget']==8 and native['task_timeout_seconds']==1500 and native['screening_admitted'] is False and native['commands']==expected_native(),'native exact4x2/120/20 budget/argv differs')
    need(native['worker_argv']==['python3','-I','-B',str(NATIVE_WORKER),'--request',str(CORE/'REQUESTS_M2_NATIVE.json')],'fixed core native worker argv mismatch')
    need(cg['input_files']==native['input_files'] and len(cg['input_files'])==395,'registered input closure changed')
    # These13 historical runtime entries intentionally declare only SHA256.
    # Register actual bytes once, then retain that exact full ID before/after.
    for p,v in cg['input_files'].items():
        got=fid(p);need(matches(got,v),'registered input changed: '+p);bound[p]=got
    # The M2 request omitted real benchmark TU/header inputs: preserve the exact
    # existing accepted v1 closure rather than silently compiling unbound files.
    experiment={p:v for p,v in prior['inputs'].items() if Path(p).is_relative_to(EXPERIMENT)}
    need(str(EXPERIMENT/'driver.cpp') in experiment and str(EXPERIMENT/'adapters/xoffset/adapter.hpp') in experiment,'accepted real-driver closure unavailable')
    for p,v in experiment.items():need(fid(p)==v,'accepted benchmark member changed: '+p);bound[p]=v
    bound[str(prior_path)]=fid(prior_path);bound[ref['path']]=fid(ref['path'])
    need(fid(RUNNER)['sha256']==RUNNER_SHA,'original runner changed');bound[str(RUNNER)]=fid(RUNNER)
    bound[str(Path(__file__).resolve())]=fid(__file__)
    for p in (D/'A_RESULT.json',D/'S0_RESULT.json',ROOT/'handoff/github-20261019/C14_MATERIALIZATION.json'):
        bound[str(p)]=fid(p)
    s0=read(D/'S0_RESULT.json')
    for p,v in s0['toolchain']['files'].items():need(fid(p)['sha256']==v['actual'],'retained ABI/runtime changed: '+p)
    if SOURCE.exists():member_ids(SOURCE,identity['members'])
    if phase=='native':
        compiled=read(REVIEW/'M2_CODEGEN_RESULT.json')
        need(compiled['status']=='passed' and compiled['request_identity']==fid(CORE/'REQUESTS_M2_CODEGEN.json'),'actual candidate codegen compile prerequisite')
        need(fid(OUTPUT/'m2-codegen-v1/candidate-driver.s')==compiled['assembly_identity'],'actual candidate asm identity changed')
        bound[str(REVIEW/'M2_CODEGEN_RESULT.json')]=fid(REVIEW/'M2_CODEGEN_RESULT.json')
    out,lease,_=paths(phase)
    if fresh:need(not out.exists() and not lease.exists(),'fresh one-attempt output/lease required')
    return dict(phase=phase,requests={n:fid(CORE/n) for n in ('REQUESTS_M2_CODEGEN.json','REQUESTS_M2_NATIVE.json')},
      candidate_members=identity['members'],candidate_source=str(CANDIDATE/'source'),source_before=actual,production_files=prod,
      inputs=bound,extra_benchmark_inputs=experiment,source_review=dict(path=str(ACCEPT),identity=fid(ACCEPT)),
      stage_argv=cg['stage_argv'],compile_argv=cg['compile_argv'],native_argv=native['worker_argv'],
      reference_control=ref,timing_samples=0,cap_seconds=dict(stage=60,codegen=240,native=1500)[phase])

def unchanged(admission,staged=False):
    for p,v in admission['inputs'].items():need(fid(p)==v,'admitted input changed: '+p)
    need(member_ids(CANDIDATE/'source',admission['candidate_members'])==admission['source_before'],'candidate154 changed')
    if staged:need(member_ids(SOURCE,admission['candidate_members'])==admission['source_before'],'staged154 changed')

def lease_proof(phase,admission_sha):
    _,lease,_=paths(phase);expected=['/usr/bin/python3','-I','-B',str(Path(__file__).resolve()),'worker',phase,'--admission-sha',admission_sha]
    with (LOCK/'queue.lock').open('r+') as stream:
        try:fcntl.flock(stream,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:pass
        else:
            fcntl.flock(stream,fcntl.LOCK_UN);raise ValueError('global lease not held')
        stream.seek(0);holder=json.load(stream)
    request=read(lease/'request.json')
    need(holder['status']=='running' and Path(holder['out'])==lease and holder['run_id']==request['run_id'],'lease out/run mismatch')
    need(holder['supervisor_pid']==os.getppid() and holder['owner']==request['owner']=='W1','not own supervisor/owner')
    need(holder['kind']==request['kind']=='validation','not own validation kind')
    need(request['command']==expected and request['cwd']==str(ROOT) and request['out']==str(lease) and request['lock_dir']==str(LOCK),'not own exact command/cwd/namespace')
    need(request['runner_sha256']==RUNNER_SHA and request['timeout_seconds']==dict(stage=60,codegen=240,native=1500)[phase],'runner/cap mismatch')
    need(Path('/proc/self/cmdline').read_bytes().split(b'\0')[:-1]==[x.encode() for x in expected],'actual worker cmdline differs')
    parent=Path('/proc')/str(os.getppid())/'cmdline'
    need(parent.read_bytes().split(b'\0')[:-1]==[b'/usr/bin/python3',str(RUNNER).encode(),b'_worker',str(lease).encode()],'supervisor executable/argv mismatch')
    return dict(holder=holder,request_identity=fid(lease/'request.json'),command=expected)

def worker(phase,admission_sha):
    out,lease,apath=paths(phase);need(fid(apath)['sha256']==admission_sha,'admission identity mismatch');a=read(apath)
    proof=lease_proof(phase,admission_sha);unchanged(a,staged=phase=='native')
    receipt=dict(status='running',phase=phase,admission_identity=fid(apath),requests=a['requests'],lease_proof=proof,
      commands=[],source154_before=a['source_before'],production_files=a['production_files'],timing_samples=0,static_mechanism_pass=False,
      actual_compiles=0,actual_native_executions=0)
    def command(label,argv,timeout):
        began=time.monotonic();record=dict(label=label,argv=argv,timeout_seconds=timeout)
        try:
            with (lease/(label+'.log')).open('xb') as log:
                p=subprocess.run(argv,cwd=ROOT,env=ENV,stdout=log,stderr=subprocess.STDOUT,timeout=timeout)
            record['returncode']=p.returncode;need(p.returncode==0,label+' failed; preserve prefix')
        finally:record['elapsed_seconds']=time.monotonic()-began;receipt['commands'].append(record)
    try:
        if phase in ('stage','codegen'):
            if not SOURCE.exists():command('stage',a['stage_argv'],60)
            else:
                need(phase=='codegen','standalone stage already exists')
                prior=read(REVIEW/'M2_STAGE_RESULT.json');need(prior['status']=='passed' and prior['request_identity']==a['requests']['REQUESTS_M2_CODEGEN.json'],'existing source not from this successful exact stage')
            unchanged(a,staged=True);out.mkdir(parents=True,exist_ok=False)
            if phase=='codegen':
                receipt['actual_compiles']=1
                command('candidate-only-S',a['compile_argv'],120)
                need((out/'candidate-driver.s').is_file(),'candidate asm absent')
                receipt['assembly_identity']=fid(out/'candidate-driver.s')
        else:
            command('registered-4normal-4SAN',a['native_argv'],1500)
            raw=read(out/'receipt.json')
            need(raw['status']=='passed' and len(raw['commands'])==16 and all(x.get('returncode')==0 for x in raw['commands']),'native exact16 commands incomplete')
            receipt['native_receipt_identity']=fid(out/'receipt.json')
            receipt['actual_compiles']=sum(x['kind']=='compile' for x in raw['commands'])
            receipt['actual_native_executions']=sum(x['kind']=='native' for x in raw['commands'])
            need(receipt['actual_compiles']==receipt['actual_native_executions']==8,'native budget/count mismatch')
        unchanged(a,staged=True);lease_proof(phase,admission_sha)
        receipt.update(status='passed',source154_after=member_ids(SOURCE,a['candidate_members']))
    except BaseException as error:
        receipt.update(status='failed',error=type(error).__name__+': '+str(error));raise
    finally:
        if phase=='native' and (out/'receipt.json').is_file():
            raw=read(out/'receipt.json');receipt['native_receipt_identity']=fid(out/'receipt.json')
            receipt['actual_compiles']=sum(x['kind']=='compile' for x in raw['commands'])
            receipt['actual_native_executions']=sum(x['kind']=='native' for x in raw['commands'])
        write(REVIEW/('M2_'+phase.upper()+'_WORKER_RECEIPT.json'),receipt)

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=('check','run','worker'));p.add_argument('phase',choices=('stage','codegen','native'));p.add_argument('--admission-sha');args=p.parse_args()
    if args.action=='worker':need(bool(args.admission_sha),'admission SHA required');worker(args.phase,args.admission_sha);return
    a=inspect(args.phase);out,lease,apath=paths(args.phase)
    if args.action=='check':print(json.dumps(dict(status='ready',phase=args.phase,source154=154,production14=14,registered_inputs=395,extra_benchmark_inputs=len(a['extra_benchmark_inputs']),requests=a['requests'],new_compiles=0,new_native=0)));return
    hostpath=D/'windows-native-admission.json';host=read(hostpath)
    age=(datetime.datetime.now(datetime.timezone.utc)-datetime.datetime.fromisoformat(host['observed_utc'].replace('Z','+00:00'))).total_seconds()
    need(0<=age<=900 and host['backing_free_bytes']>=8*1024**3 and shutil.disk_usage('/root').free>=8*1024**3,'fresh host/guest8GiB reserve required')
    a['inputs'][str(hostpath)]=fid(hostpath);a['host']=dict(path=str(hostpath),identity=fid(hostpath));write(apath,a)
    worker_command=['/usr/bin/python3','-I','-B',str(Path(__file__).resolve()),'worker',args.phase,'--admission-sha',fid(apath)['sha256']]
    argv=['/usr/bin/python3','-I','-B',str(RUNNER),'run','--owner','W1','--kind','validation','--out',str(lease),'--timeout-seconds',str(a['cap_seconds']),'--wait-seconds','0','--',*worker_command]
    write(REVIEW/('M2_'+args.phase.upper()+'_DISPATCH.json'),dict(argv=argv,admission=fid(apath)))
    result=subprocess.run(argv,cwd=ROOT,env=dict(os.environ,PATH='/usr/bin:/bin'))
    queue=read(lease/'result.json');wrpath=REVIEW/('M2_'+args.phase.upper()+'_WORKER_RECEIPT.json');wr=read(wrpath) if wrpath.is_file() else None
    passed=result.returncode==0 and queue['status']=='complete' and queue['command_returncode']==0 and queue.get('reason')=='command_exit' and queue.get('cleanup') is not None and all(not value for value in queue['cleanup'].values()) and wr and wr['status']=='passed'
    post_error=None
    try:unchanged(a,staged=SOURCE.exists())
    except BaseException as e:passed=False;post_error=str(e)
    summary=dict(status='passed' if passed else 'failed',phase=args.phase,request_identity=a['requests']['REQUESTS_M2_NATIVE.json' if args.phase=='native' else 'REQUESTS_M2_CODEGEN.json'],
      admission_identity=fid(apath),returncode=result.returncode,queue_result=queue,worker_receipt=dict(path=str(wrpath),identity=fid(wrpath)) if wr else None,
      postcheck_error=post_error,production_files=a['production_files'],timing_samples=0,screen_admitted=False,static_mechanism_pass=False,
      scope=dict(stage='exact154 source materialization only',codegen='one candidate-only bare -S compilation; independent4-to1 static review pending',native='four normal/four SAN targeted correctness only')[args.phase],
      actual_compiles=(wr.get('actual_compiles',1 if args.phase=='codegen' and passed else 0) if wr else 0),actual_native_executions=wr.get('actual_native_executions',0) if wr else 0)
    if wr and 'assembly_identity' in wr:summary['assembly_identity']=wr['assembly_identity']
    write(REVIEW/('M2_'+args.phase.upper()+'_RESULT.json'),summary)
    need(passed,'M2 finite job failed; preserve and diagnose; no retry')
    print('M2 '+args.phase+' passed; static mechanism review remains separate;0 timing')
if __name__=='__main__':main()
