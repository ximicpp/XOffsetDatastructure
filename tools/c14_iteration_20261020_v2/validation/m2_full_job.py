"""M2-only complete normal73/SAN73 gates through the original shared lease.

check is read-only. run invokes the already reviewed full entry's execute()
inside a command-bound worker, adding complete before/after tool/provider pins.
No diagnostic C, business, screen or extra native/codegen is launched here.
"""
import argparse
import datetime
import fcntl
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[3]
CORE=ROOT/'docs/coordination/c14-iteration-20261020-v2/core'
V=ROOT/'docs/coordination/c14-iteration-20261020-v2/validation'
D=V/'m2_full';REVIEW=V/'review'
H=ROOT/'tools/c14_iteration_20261020_v2/core'
INNER=CORE/'REQUESTS_M2_FULL.json';OUTER=INNER
IDENT=ROOT/'build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1/IDENTITY.json'
OUTPUT=Path('/root/xoffset-c14-iteration/20261020/v2/validation')
OLD=Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发')
LOCK=OLD/'build/vm-coordination';RUNNER=OLD/'experiments/vm_coordination/run_job.py'
VENDOR=Path('/root/xoffset-receive-completion/20261019/v1/final-v4/source')
INVENTORY=Path('/root/xoffset-receive-completion/20261019/v1/full-final-v4-normal-build/test-inventory.json')
INNER_SHA='9fe1735d1e1bb4e0f3a7afde43f1ae9257176a61a485e0879374f85a9dc3d1c8'
OUTER_SHA=INNER_SHA
MAIN='3e347612c1a62299bb910c68dd6b1411663f3165e18e298a51e7ba2e062c201f'
WRAPPER=Path('/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/bin/clang++-libcxx')
WRAPPER_ID=dict(bytes=61,sha256='c8ea06a7c58557e3b89d09873dc88457d7b0c5d9af3f08cf0449093f2fbd1b87')
RUNNER_SHA='6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909'

def need(ok,message):
    if not ok:raise ValueError(message)
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def fid(p):
    p=Path(p);h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return dict(bytes=p.stat().st_size,sha256=h.hexdigest())
def write(p,value):
    with Path(p).open('x',encoding='utf-8') as f:json.dump(value,f,indent=2,ensure_ascii=False);f.write('\n')
def ref(p):return dict(path=str(p),identity=fid(p))
def matches(a,b):return a['sha256']==b['sha256'] and ('bytes' not in b or a['bytes']==b['bytes'])
def load_full():
    s=importlib.util.spec_from_file_location('m2_retained_full_jobs',H/'m2_full_jobs.py')
    module=importlib.util.module_from_spec(s);s.loader.exec_module(module);return module
def locations(mode):
    label='m2-full-v1-'+mode
    return OUTPUT/label,OUTPUT/(label+'-lease'),REVIEW/('M2_FULL_ENTRY_'+mode.upper()+'_ADMISSION.json'),D/('M2_FULL_'+mode.upper()+'_REQUEST_v1.json')
def source_ids(source,members):
    actual={}
    for n,v in members.items():
        p=Path(n);need(not p.is_absolute() and '..' not in p.parts and '\\' not in n,'unsafe member')
        actual[n]=fid(source/n)
        need(actual[n]==dict(bytes=v['bytes'],sha256=v['candidate_sha256']),'M2 member changed: '+n)
    return actual

def inspect(mode):
    need(fid(INNER)['sha256']==INNER_SHA and fid(OUTER)['sha256']==OUTER_SHA,'exact approved request changed')
    q=read(INNER);outer=read(OUTER);ident=read(IDENT);material=read(ROOT/'handoff/github-20261019/C14_MATERIALIZATION.json')
    need(outer==q and outer['compiler_launcher']==ref(WRAPPER),'single approved M2 request and explicit launcher binding')
    need(q['candidate']==outer['candidate']=='m2-exact-owns-v1' and q['status']==outer['status']=='ready_for_validation_review','M2 finite request required')
    need(q['timing_admitted'] is outer['timing_admitted'] is False and q['screen_admitted'] is q['QF_admitted'] is False,'functional only')
    need(q['source_members']==outer['source_members']==ident['members'] and len(ident['members'])==154,'complete M2 source154 identity')
    need(fid(IDENT)==q['candidate_identity']==outer['candidate_identity'],'candidate identity file changed')
    need({n:v['baseline_sha256'] for n,v in ident['members'].items()}=={n:v['sha256'] for n,v in material['non_external_files'].items()},'independent C14 baseline154')
    actual=source_ids(IDENT.parent/'source',ident['members'])
    prod={n:actual[n] for n in material['production_files']}
    need(len(prod)==14 and prod==q['production14']==outer['production14'] and prod['xoffsetdatastructure.hpp']['sha256']==q['candidate_main_sha256']==MAIN,'actual M2 production14')
    bpath=REVIEW/'M2_NATIVE_RESULT.json';b=read(bpath)
    bwpath=REVIEW/'M2_NATIVE_WORKER_RECEIPT.json';bw=read(bwpath)
    cgpath=REVIEW/'M2_CODEGEN_RESULT.json';cg=read(cgpath)
    cgapath=REVIEW/'M2_CODEGEN_ADMISSION.json';cga=read(cgapath)
    staticpath=REVIEW/'M2_CODEGEN_MECHANISM_REVIEW.json';static=read(staticpath)
    need(b['status']=='passed' and b['actual_native_executions']==8 and b['production_files']==prod,'actual same-source M2 targeted8')
    need(bw['status']=='passed' and bw['source154_before']==bw['source154_after']==actual,'actual M2 targeted source154')
    need(cg['status']=='passed' and cga['candidate_members']==ident['members'] and static['status']=='passed' and static['production_files']==prod,'same-source M2 static codegen4to1')
    need(read(V/'A_RESULT.json')['status']==read(V/'S0_RESULT.json')['status']=='passed','current A/S0')
    job=next(j for j in q['jobs'] if j['id']=='M2-FULL-'+mode+'-v1')
    need(job['argv']==['/usr/bin/python3','-I','-B',str(H/'m2_full_jobs.py'),mode] and job['timeout_seconds']==2400,'original exact full job recipe')
    inputs={}
    for p,v in outer['tool_files'].items():need(fid(p)==v,'full derived tool changed: '+p);inputs[p]=v
    nativeqpath=CORE/'REQUESTS_M2_NATIVE.json';nativeq=read(nativeqpath)
    for p,v in nativeq['input_files'].items():
        got=fid(p);need(matches(got,v),'fixed compiler/runtime/input changed: '+p);inputs[p]=got
    need(fid(WRAPPER)==WRAPPER_ID,'outer libc++ compiler wrapper changed');inputs[str(WRAPPER)]=fid(WRAPPER)
    need(fid(RUNNER)['sha256']==RUNNER_SHA,'original runner changed');inputs[str(RUNNER)]=fid(RUNNER)
    provider={};lock=read(ROOT/'SOURCE_MANIFEST.json')['dependency_lock']
    for name in list(lock['include_trees'])+list(lock['licenses']):
        p=VENDOR/name
        for item in (sorted(p.rglob('*')) if p.is_dir() else [p]):
            if item.is_file():provider[item.relative_to(VENDOR).as_posix()]=fid(item);inputs[str(item)]=fid(item)
    need(len(provider)>=851,'complete locked dependency provider required')
    inv=read(INVENTORY);tests=inv['runtime_test_programs']+inv['subprocess_scenarios']+inv['tooling_checks']
    need(inv['mode']=='full' and len(tests)==len(set(tests))==73 and inv['configure_check_count']==3,'original full73 inventory')
    J=load_full().J
    for p in J.FIXED_TOOLS:inputs[str(p)]=fid(p)
    for p in (INNER,OUTER,IDENT,bpath,bwpath,cgpath,cgapath,staticpath,nativeqpath,V/'A_RESULT.json',V/'S0_RESULT.json',INVENTORY,ROOT/'SOURCE_MANIFEST.json',ROOT/'handoff/github-20261019/C14_MATERIALIZATION.json',Path(__file__).resolve()):inputs[str(p)]=fid(p)
    out,lease,_,request=locations(mode)
    need(not out.exists() and not lease.exists() and not request.exists(),'fresh one-attempt mode output/queue/request required')
    if mode=='sanitized':need(read(REVIEW/'M2_FULL_ENTRY_NORMAL_RESULT.json')['status']=='passed','actual new M2 normal gate first')
    review=CORE/'REVIEW_M2_FULL.md';text=review.read_text(encoding='utf-8')
    need('No blocking static defect found in the actual M2 complete functional recipe' in text,'full-path source review missing')
    return dict(mode=mode,source_members=ident['members'],source_before=actual,production_files=prod,inputs=inputs,dependency_provider=provider,
      expected_tests=tests,targets=inv['runtime_test_programs'],request_identity=fid(INNER),outer_request_identity=fid(OUTER),
      source_review_observed=dict(reference=ref(review),scope='Exact single M2 complete functional request admitted; dynamic mechanism remains separate',text=text),
      new_timing_samples=0,C_diagnostic_prerequisite=False)

def unchanged(a,source=None):
    for p,v in a['inputs'].items():need(fid(p)==v,'full admitted input changed: '+p)
    need(source_ids(IDENT.parent/'source',a['source_members'])==a['source_before'],'M2 candidate154 changed')
    if source is not None:
        need(source_ids(source,a['source_members'])==a['source_before'],'M2 actual full snapshot154 changed')
        for n,v in a['dependency_provider'].items():need(fid(source/n)==v,'actual full dependency changed: '+n)

def prove_lease(mode,asha):
    _,lease,_,_=locations(mode)
    expected=['/usr/bin/python3','-I','-B',str(Path(__file__).resolve()),'worker',mode,'--admission-sha',asha]
    with (LOCK/'queue.lock').open('r+') as f:
        try:fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:pass
        else:fcntl.flock(f,fcntl.LOCK_UN);raise ValueError('original global lease not held')
        f.seek(0);holder=json.load(f)
    q=read(lease/'request.json')
    need(holder['status']=='running' and Path(holder['out'])==lease and holder['run_id']==q['run_id'],'own lease out/run')
    need(holder['owner']==q['owner']=='W1' and holder['kind']==q['kind']=='validation' and holder['supervisor_pid']==os.getppid(),'own supervisor/role')
    need(q['command']==expected and q['cwd']==str(ROOT) and q['out']==str(lease) and q['lock_dir']==str(LOCK) and q['timeout_seconds']==2400 and q['runner_sha256']==RUNNER_SHA,'own exact command/cwd/cap/namespace')
    need(Path('/proc/self/cmdline').read_bytes().split(b'\0')[:-1]==[s.encode() for s in expected],'actual full wrapper command')
    need((Path('/proc')/str(os.getppid())/'cmdline').read_bytes().split(b'\0')[:-1]==[b'/usr/bin/python3',str(RUNNER).encode(),b'_worker',str(lease).encode()],'actual original supervisor command')
    return dict(holder=holder,request=ref(lease/'request.json'))

def post_full(mode,a):
    out,_,_,request=locations(mode);receipt=read(out/'receipt.json');inv=read(out/'build/test-inventory.json')
    need(receipt['status']=='passed' and receipt['production_files']==a['production_files'] and receipt['request']['sha256']==fid(request)['sha256'],'actual own M2 full receipt')
    expected=inv['runtime_test_programs']+inv['subprocess_scenarios']+inv['tooling_checks']
    need(inv['mode']=='full' and len(expected)==len(set(expected))==73 and sorted(expected)==sorted(a['expected_tests'])==sorted(receipt['junit_names']) and inv['configure_check_count']==3,'actual full73 and separate configure3')
    need(receipt['inventory_identity']==fid(out/'build/test-inventory.json') and receipt['compile_commands_identity']==fid(out/'build/compile_commands.json'),'actual inventory/compile database identity')
    cases=list(ET.parse(out/'ctest.xml').getroot().iter('testcase'))
    need(len(cases)==73 and sorted(c.get('name') for c in cases)==sorted(expected) and all(c.get('status')=='run' and all(c.find(n) is None for n in ('failure','error','skipped')) for c in cases),'actual JUnit73 ran/pass')
    unchanged(a,out/'source');return ref(out/'receipt.json')

def worker(mode,asha):
    out,_,apath,request=locations(mode);need(fid(apath)['sha256']==asha,'full admission changed');a=read(apath)
    proof=prove_lease(mode,asha);unchanged(a);need(fid(request)==a['child_request'],'full child request changed')
    result=dict(status='running',mode=mode,admission=ref(apath),lease_proof=proof,compiler_wrapper_before=fid(WRAPPER),new_timing_samples=0)
    try:
        J=load_full().J;J.execute(request,a['child_request']['sha256'])
        result['full_receipt']=post_full(mode,a);prove_lease(mode,asha);result['compiler_wrapper_after']=fid(WRAPPER);result['status']='passed'
    except BaseException as error:result.update(status='failed',reason=type(error).__name__+': '+str(error));raise
    finally:write(REVIEW/('M2_FULL_ENTRY_'+mode.upper()+'_WORKER.json'),result)

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=('check','run','worker'));p.add_argument('mode',choices=('normal','sanitized'));p.add_argument('--admission-sha');args=p.parse_args()
    if args.action=='worker':need(bool(args.admission_sha),'admission SHA required');worker(args.mode,args.admission_sha);return
    a=inspect(args.mode);out,lease,apath,request=locations(args.mode)
    if args.action=='check':print(json.dumps(dict(status='ready',mode=args.mode,source154=154,production14=14,expected_tests=73,tool_runtime_provider_inputs=len(a['inputs']),wrapper61_explicit=True,new_compiles=0,new_native=0)));return
    hostpath=V/('windows-m2-full-'+args.mode+'.json');host=read(hostpath)
    age=(datetime.datetime.now(datetime.timezone.utc)-datetime.datetime.fromisoformat(host['observed_utc'].replace('Z','+00:00'))).total_seconds()
    need(0<=age<=900 and host['backing_free_bytes']>=8*1024**3 and shutil.disk_usage('/root').free>=8*1024**3,'fresh actual host/guest8GiB reserve')
    a['inputs'][str(hostpath)]=fid(hostpath);a['host_resource_observation']=ref(hostpath)
    F=load_full();identity,prod,material,source=F.prepare();J=F.J
    q=dict(schema_version=1,stage='S2_targeted',candidate='M2',job_id='m2-full-v1-'+args.mode,mode=args.mode,
      baseline_main_sha256=material['production_files']['xoffsetdatastructure.hpp']['sha256'],production_files=prod,
      overlays={n:dict(path=str(source/n),identity=v) for n,v in prod.items() if v!=material['production_files'][n]},
      targets=a['targets'],tests=a['expected_tests'],runner_identity=fid(RUNNER),entry_identity=fid(H/'m2_full/full_entry.py'),
      tool_files={str(p):fid(p) for p in J.FIXED_TOOLS},compiler_launcher=ref(WRAPPER),backing_free_bytes=host['backing_free_bytes'],backing_observed_utc=host['observed_utc'])
    write(request,q);J.verify(request);a['child_request']=fid(request)
    a['inputs'][str(request)]=fid(request);a['inputs'][str(D/'S0_RESULT.json')]=fid(D/'S0_RESULT.json');a['inputs'][str(D/'M2_FULL_SOURCE_BINDING.json')]=fid(D/'M2_FULL_SOURCE_BINDING.json')
    write(apath,a)
    command=['/usr/bin/python3','-I','-B',str(Path(__file__).resolve()),'worker',args.mode,'--admission-sha',fid(apath)['sha256']]
    argv=['/usr/bin/python3','-I','-B',str(RUNNER),'run','--owner','W1','--kind','validation','--out',str(lease),'--timeout-seconds','2400','--wait-seconds','0','--',*command]
    write(REVIEW/('M2_FULL_ENTRY_'+args.mode.upper()+'_DISPATCH.json'),dict(argv=argv,admission=ref(apath)))
    r=subprocess.run(argv,cwd=ROOT,env=dict(os.environ,PATH='/usr/bin:/bin'))
    queue=read(lease/'result.json');wrpath=REVIEW/('M2_FULL_ENTRY_'+args.mode.upper()+'_WORKER.json');wr=read(wrpath) if wrpath.exists() else None
    passed=r.returncode==0 and queue['status']=='complete' and queue['command_returncode']==0 and queue.get('reason')=='command_exit' and queue.get('cleanup') is not None and all(not v for v in queue['cleanup'].values()) and wr and wr['status']=='passed'
    post_error=None
    try:unchanged(a,out/'source' if (out/'source').exists() else None)
    except BaseException as error:passed=False;post_error=str(error)
    result=dict(status='passed' if passed else 'failed',mode=args.mode,request=ref(request),inner_request=ref(INNER),outer_request=ref(OUTER),
      admission=ref(apath),queue_result=queue,worker=ref(wrpath) if wr else None,postcheck_error=post_error,
      production_files=prod,expected_tests=73,actual_tests=73 if passed else None,new_timing_samples=0,performance_accepted=False,screen_admitted=False)
    write(REVIEW/('M2_FULL_ENTRY_'+args.mode.upper()+'_RESULT.json'),result);need(passed,'M2 full gate failed; preserve and diagnose without retry')
    if args.mode=='sanitized':
        refs={m:ref(OUTPUT/('m2-full-v1-'+m)/'receipt.json') for m in ('normal','sanitized')}
        for m in refs:
            own=read(REVIEW/('M2_FULL_ENTRY_'+m.upper()+'_RESULT.json'));need(own['status']=='passed' and own['production_files']==prod,'both actual M2 wrapper results required')
            post_full(m,a)
        write(D/'M2_FULL_RESULT.json',dict(status='passed',candidate=identity['candidate'],request_identity=fid(INNER),normal_count=73,sanitized_count=73,
          production_files=prod,normal_receipt=refs['normal'],sanitized_receipt=refs['sanitized'],compiler_launcher=ref(WRAPPER),performance_samples=0))
    print('M2 '+args.mode+' full73 passed;0 timing; C diagnostic independent')
if __name__=='__main__':main()
