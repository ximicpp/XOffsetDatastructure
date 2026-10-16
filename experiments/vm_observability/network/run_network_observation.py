#!/usr/bin/env python3
"""Bounded W2 network diagnostics; immutable cases, exact identities, no timing claims."""
from __future__ import annotations
import argparse,datetime,hashlib,importlib.util,json,os,platform,shlex,shutil,subprocess,sys
from pathlib import Path,PurePosixPath
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
FOUNDATION=ROOT/'docs/validation/vm-observability/frozen-implementation-v1'
PLAN=ROOT/'docs/coordination/vm-followup/contracts/W2-network-v1'
W1=ROOT/'build/vm-network/prepared-v2'
BUILD_ROOT=ROOT/'build/vm-observability/network'
EVIDENCE_ROOT=ROOT/'docs/validation/vm-observability/network'
LIMIT=536870912
NOMINALS=(4096,65536,1048576,16777216)
POOLS=('none','cold','warm')
BOOST=('container','core','config','throw_exception','static_assert','container_hash','type_traits','winapi','intrusive','predef')
PLAN_HASHES={'PLAN.md':'e859da6e64254f80d8df75e49fb115844abef41639d36f2eb7c2f9abea9e4f9e','plan.json':'b6ab834068d9bf23cb57afd367cb4299d9c3c6c153b7862875c1a52e9cad8ecf','anchor-census.json':'a3dbafac4dff100864f113e77872c958f79ecd7e98739f231724d1fa4f77556b'}
FOUNDATION_HASH='fc1780b5ce1f6cb7cfbbdfb85896d9746740c20b5e3df59bddedee5a42bb3c8d'
REFERENCE_HASH='ae6ce12e8ed06b246e19b73995f774da0d3c74f7a369e9f336b2d7fbc30558c8'

class CheckFailure(ValueError):pass
def require(ok,message):
    if not ok:raise CheckFailure(message)
def sha(p):
    h=hashlib.sha256()
    with Path(p).open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()
def now():return datetime.datetime.now(datetime.timezone.utc).isoformat()
def write(p,d):Path(p).write_text(json.dumps(d,indent=2,sort_keys=True)+'\n')
def read(p):return json.loads(Path(p).read_text())
def fresh(p):p=Path(p).resolve();p.mkdir(parents=True,exist_ok=False);return p
def inventory(p):
    result={}
    for f in sorted(p.rglob('*')):
        require(not f.is_symlink(),'symlink in artifact')
        if f.is_file() and f not in {p/'result.json',p/'COMPLETE'}:result[f.relative_to(p).as_posix()]=sha(f)
    return result
def seal(p,d):
    d.update(status='passed',timing_evidence=False,finished_at=now(),files=inventory(p));write(p/'result.json',d);check_budget(65);(p/'COMPLETE').write_text(sha(p/'result.json')+'\n')
def audit_files(p):
    d=read(p/'result.json');require(d.get('schema')=='n3-network-artifact-v1' and d.get('status')=='passed' and d.get('timing_evidence') is False,'artifact is not a passed diagnostic')
    require((p/'COMPLETE').read_text().strip()==sha(p/'result.json'),'completion identity mismatch')
    for name in d['files']:
        rel=PurePosixPath(name);require(not rel.is_absolute() and '..' not in rel.parts,'unsafe inventory path')
    require(inventory(p)==d['files'],'artifact inventory changed');return d

def module(path,name):
    import types
    m=types.ModuleType(name);m.__file__=str(path);sys.modules[name]=m;exec(compile(Path(path).read_text(),str(path),'exec'),m.__dict__);return m

def active(kinds):
    record=read(ROOT/'build/vm-coordination/queue.lock')
    require(record.get('status')=='running' and record.get('owner')=='W2' and record.get('kind') in kinds and record.get('supervisor_pid')==os.getppid(),'must run as direct W4 default-queue W2 job')
    return {'run_id':record['run_id'],'kind':record['kind'],'out':record['out']}

def budget_bytes():
    total=0
    for root in (BUILD_ROOT,EVIDENCE_ROOT):
        if root.exists():total+=sum(p.stat().st_size for p in root.rglob('*') if p.is_file() and not p.is_symlink())
    return total

def check_budget(extra=0):require(budget_bytes()+extra<=LIMIT,'512 MiB new evidence budget would be exceeded')
def bounded_out(value,root):
    value=value.resolve();require(value.is_relative_to(root.resolve()) and value!=root.resolve(),'output outside the owned budget root');return value
def tree_bytes(root):return sum(p.stat().st_size for p in root.rglob('*') if p.is_file())
def semantics(obj):
    if isinstance(obj,dict):return {k:semantics(v) for k,v in obj.items() if not k.endswith('_ns')}
    if isinstance(obj,list):return [semantics(v) for v in obj]
    return obj
def fixture_observation(obs,stdout,nominal):
    require(set(obs)=={'schema','diagnostic','timing_evidence','joined','command','elements','frame_bytes','page_size'},'fixture observation fields differ')
    for key,value in {'schema':'n3-network-raw-v1','diagnostic':True,'timing_evidence':False,'joined':True,'command':'fixture','elements':nominal//4,'frame_bytes':stdout['frame_bytes']}.items():
        require(type(obs[key]) is type(value) and obs[key]==value,'fixture observation differs: '+key)
    page=obs['page_size'];require(type(page) is int and page>=4096 and page&(page-1)==0,'fixture page size invalid')

def prepare(a):
    a.out=bounded_out(a.out,BUILD_ROOT)
    acceptance=read(PLAN/'acceptance.json')
    require(acceptance['snapshot_sha256']==PLAN_HASHES,'accepted plan identity differs')
    for name,digest in acceptance['snapshot_sha256'].items():require(sha(PLAN/name)==digest,'accepted network plan changed')
    spec=read(PLAN/'plan.json')
    for name,digest in spec['source_bindings'].items():
        if name in spec['source_bindings_policy']['context_only']:continue
        require(sha(ROOT/name)==digest,'strict input changed: '+name)
    require(spec['recorder_budget']['hard_events_per_thread_max']==2048,'wrong accepted hard event budget')
    require(sha(FOUNDATION/'snapshot.json')==FOUNDATION_HASH,'foundation snapshot differs');foundation=read(FOUNDATION/'snapshot.json')['files']
    for name in ('xoffsetdatastructure.hpp','xoffset_virtual_memory.hpp','n3_runtime.hpp'):
        require(sha(FOUNDATION/'build/input/diagnostic'/name)==foundation['build/input/diagnostic/'+name],'accepted diagnostic header changed: '+name)
    meta=read(W1/'result.json');require((W1/'COMPLETE').read_text().strip()==sha(W1/'result.json'),'W1 preparation incomplete')
    for name,digest in meta['files'].items():require(sha(W1/name)==digest,'W1 prepared file changed')
    check_budget(4*1024**2);p=fresh(a.out);(p/'source').mkdir();(p/'tool').mkdir();(p/'foundation').mkdir();(p/'plan').mkdir()
    generator=module(HERE/'network_generate.py','n3_network_generate')
    generated,anchors=generator.transform((W1/'source/net_reference.cpp').read_text())
    (p/'source/net_reference.cpp').write_text(generated)
    shutil.copy2(W1/'source/net_reference.cpp',p/'w1-net-reference.cpp')
    for name in ('frame_model.hpp','frame_model_tests.hpp'):shutil.copy2(W1/'source'/name,p/'source'/name)
    for name in ('xoffsetdatastructure.hpp','xoffset_virtual_memory.hpp','n3_runtime.hpp'):shutil.copy2(FOUNDATION/'build/input/diagnostic'/name,p/'source'/name)
    shutil.copy2(HERE/'network_support.hpp',p/'source/network_support.hpp')
    for name in ('audit_events.py','event_kinds.json'):shutil.copy2(FOUNDATION/'source'/name,p/'foundation'/name)
    shutil.copy2(FOUNDATION/'snapshot.json',p/'foundation/snapshot.json')
    for name in ('run_network_observation.py','network_generate.py','network_audit.py','test_network_audit.py','test_network_runner.py'):shutil.copy2(HERE/name,p/'tool'/name)
    for name in ('PLAN.md','plan.json','anchor-census.json','acceptance.json'):shutil.copy2(PLAN/name,p/'plan'/name)
    shutil.copy2(W1/'result.json',p/'w1-prepared-result.json');write(p/'generation-anchors.json',anchors)
    reference=ROOT/'build/vm-network/diagnostic-v2/net_reference';w1build=read(reference.parent/'result.json')
    require((reference.parent/'COMPLETE').read_text().strip()==sha(reference.parent/'result.json'),'W1 reference build incomplete')
    require(w1build['status']=='passed' and w1build['measurement_profile']=='diagnostic' and w1build['timing_evidence'] is False and w1build['prepared_result_sha256']==sha(W1/'result.json'),'W1 reference build profile/parent differs')
    for name in ('net_reference.cpp','frame_model.hpp','frame_model_tests.hpp','xoffsetdatastructure.hpp','xoffset_virtual_memory.hpp','contract.json'):
        require(w1build['files']['source/'+name]==meta['files']['source/'+name],'W1 reference source identity differs')
    require(sha(reference)==w1build['files']['net_reference']==REFERENCE_HASH,'W1 reference binary changed')
    shutil.copy2(reference.parent/'result.json',p/'w1-build-result.json')
    seal(p,{'schema':'n3-network-artifact-v1','kind':'prepared','created_at':now(),'reference_binary':str(reference),'reference_binary_sha256':sha(reference),'W1_prepared_result_sha256':sha(W1/'result.json'),'foundation_snapshot_sha256':sha(FOUNDATION/'snapshot.json'),'header_hashes':{n:sha(p/'source'/n) for n in ('xoffsetdatastructure.hpp','xoffset_virtual_memory.hpp','n3_runtime.hpp')},'accepted_plan_sha256':sha(PLAN/'plan.json'),'anchor_count':len(anchors)})
    print(json.dumps({'status':'passed','kind':'prepared','path':str(p),'anchors':len(anchors)}))

def build(a):
    a.out=bounded_out(a.out,BUILD_ROOT)
    job=active({'build'});prepared=a.prepared.resolve();d=audit(prepared);require(d['kind']=='prepared','not prepared');require(sha(prepared/'tool/run_network_observation.py')==sha(__file__),'build runner differs from frozen source')
    check_budget(8*1024**2);p=fresh(a.out);shutil.copytree(prepared,p/'input')
    # Keep the C++ driver's argv[0]; resolving clang++ to clang changes linkage.
    compiler=a.compiler.absolute();cmd=[str(compiler),'-O2','-std=c++26','-freflection','-fexpansion-statements','-stdlib=libc++','-pthread','-DN7_NET_DIAGNOSTICS=1']
    if platform.system()=='Darwin':cmd+=['-arch',platform.machine(),'-isysroot',str(a.sdk),'-mmacosx-version-min=10.15']
    for lib in BOOST:cmd+=['-isystem',str(a.deps/'boost/libs'/lib/'include')]
    cmd+=['-isystem',str(a.deps/'typelayout/include'),'-I',str(p/'input/source'),'-L'+str(a.library),'-Wl,-rpath,'+str(a.library),'-MD','-MF',str(p/'dependencies.d'),str(p/'input/source/net_reference.cpp'),'-o',str(p/'n3_network')]
    with (p/'compile.stdout').open('x') as stdout,(p/'compile.stderr').open('x') as stderr:r=subprocess.run(cmd,stdout=stdout,stderr=stderr,timeout=240)
    write(p/'command.json',{'argv':cmd,'returncode':r.returncode});require(r.returncode==0,'C++ build failed; see compile.stderr')
    deps=[Path(n).resolve() for n in shlex.split((p/'dependencies.d').read_text().replace('\\\n',' ').split(':',1)[1])]
    include_resolution={}
    for name,expected in d['header_hashes'].items():
        matches=[f for f in deps if f.name==name];require(matches==[(p/'input/source'/name).resolve()],'frozen diagnostic header was shadowed: '+name);require(sha(matches[0])==expected,'selected diagnostic header changed')
        include_resolution[name]={'path':str(matches[0]),'sha256':sha(matches[0])}
    extern={str(f):sha(f) for f in deps if not f.is_relative_to(p)};extern[str(compiler)]=sha(compiler)
    for name in ('libc++.1.0.dylib','libc++abi.1.0.dylib'):
        f=a.library/name
        if f.exists():extern[str(f.resolve())]=sha(f)
    write(p/'include-resolution.json',include_resolution)
    seal(p,{'schema':'n3-network-artifact-v1','kind':'build','queue_job':job,'command':cmd,'returncode':r.returncode,'prepared_result_sha256':sha(prepared/'result.json'),'binary_sha256':sha(p/'n3_network'),'include_resolution':include_resolution,'external_files':extern,'compiler_version':subprocess.run([compiler,'--version'],capture_output=True,text=True,check=True).stdout})
    print(json.dumps({'status':'passed','kind':'build','path':str(p)}))

def ledger():return BUILD_ROOT/'workload-ledger.json'
def launch(command,p,name,case,profile):
    state=read(ledger()) if ledger().exists() else {'schema':'n3-network-workload-ledger-v1','max_attempts':32,'attempts':[]}
    require(len(state['attempts'])<32,'32 workload process limit exhausted')
    check_budget();attempt={'index':len(state['attempts']),'case':case,'profile':profile,'command':list(map(str,command)),'started_at':now(),'status':'started'};state['attempts'].append(attempt);write(ledger(),state)
    with (p/(name+'.stdout')).open('x') as stdout,(p/(name+'.stderr')).open('x') as stderr:
        try:r=subprocess.run(command,stdout=stdout,stderr=stderr,timeout=170);code=r.returncode
        except subprocess.TimeoutExpired:code=124
    state['attempts'][-1].update(status='complete' if code==0 else 'failed',returncode=code,finished_at=now());write(ledger(),state)
    require(code==0,'workload failed; evidence retained: '+case+'/'+profile)
    check_budget();return {'argv':list(map(str,command)),'returncode':code,'attempt_index':attempt['index']}

def same_file(a,b):
    require(a.stat().st_size==b.stat().st_size and sha(a)==sha(b),'wire byte/hash mismatch')
    with a.open('rb') as x,b.open('rb') as y:
        while True:
            left=x.read(1024*1024);right=y.read(1024*1024);require(left==right,'wire byte mismatch')
            if not left:return

def analyzer(p):return module(p/'input/tool/network_audit.py','n3_network_audit')

def collection_root(a):
    b=a.build.resolve();d=audit(b);require(d['kind']=='build','not build')
    require(sha(b/'input/tool/run_network_observation.py')==sha(__file__),'execution runner differs from frozen source')
    for f,digest in d['external_files'].items():require(sha(f)==digest,'compiler/runtime/dependency changed: '+f)
    ref=Path(read(b/'input/result.json')['reference_binary']);require(sha(ref)==read(b/'input/result.json')['reference_binary_sha256'],'W1 reference changed')
    p=bounded_out(a.out,EVIDENCE_ROOT)
    if not p.exists():
        check_budget(tree_bytes(b/'input')+1024**2)
        p=fresh(p);shutil.copytree(b/'input',p/'input');shutil.copy2(b/'result.json',p/'build-result.json');(p/'cases').mkdir()
        write(p/'identity.json',{'build_result_sha256':sha(b/'result.json'),'binary_sha256':sha(b/'n3_network'),'reference_binary_sha256':sha(ref),'original_collection_path':str(p),'original_binary_path':str(b/'n3_network'),'reference_binary_path':str(ref),'created_at':now()})
    else:
        require((p/'identity.json').is_file() and read(p/'identity.json')['build_result_sha256']==sha(b/'result.json'),'collection resume identity mismatch')
    audit(p/'input');require(sha(p/'input/result.json')==d['prepared_result_sha256'],'collection frozen input changed')
    require(sha(p/'build-result.json')==sha(b/'result.json'),'copied build identity changed')
    collection_identity(p)
    for case in (p/'cases').iterdir():
        require((case/'COMPLETE').exists(),'unfinished case must be reviewed before resume: '+case.name)
        audit_case_dir(case,p)
    return b,p,ref

def collect(a):
    job=active({'diagnostic','functional'});b,p,ref=collection_root(a);checker=analyzer(p);kind_catalog=p/'input/foundation/event_kinds.json'
    stages=('fixtures','network') if a.stage=='all' else (a.stage,)
    for stage in stages:
        for nominal in NOMINALS:
            if stage=='fixtures':
                case='fixture-'+str(nominal);out=p/'cases'/case
                if (out/'COMPLETE').exists():audit_case_dir(out,p);continue
                require(not out.exists(),'unfinished case must be reviewed before resume: '+case);check_budget(2*(nominal+65624)+3*1024**2);out=fresh(out)
                command1=[str(ref),'fixture','--output',str(out/'reference.wire'),'--elements',str(nominal//4)]
                commands=[launch(command1,out,'reference',case,'reference')]
                command2=[str(b/'n3_network'),'fixture','--output',str(out/'observed.wire'),'--elements',str(nominal//4)]
                commands.append(launch(command2,out,'observed',case,'diagnostic'))
                same_file(out/'reference.wire',out/'observed.wire')
                stdout=read(out/'observed.stdout');reference=read(out/'reference.stdout');require(stdout==reference,'fixture stdout differs from original W1')
                trace=read(out/'observed.wire.n3.json');obs=read(out/'observed.wire.obs.json')
                summary=checker.audit_fixture(trace,stdout,kind_catalog_path=kind_catalog,nominal_bytes=nominal)
                fixture_observation(obs,stdout,nominal)
                write(out/'summary.json',summary);seal(out,{'schema':'n3-network-artifact-v1','kind':'fixture','case':case,'nominal_bytes':nominal,'commands':commands,'queue_job':job,'wire_sha256':sha(out/'observed.wire')})
            else:
                fixture=p/'cases'/('fixture-'+str(nominal));require((fixture/'COMPLETE').exists(),'fixture prerequisite missing');audit_case_dir(fixture,p)
                for pool in POOLS:
                    case=str(nominal)+'-'+pool;out=p/'cases'/case
                    if (out/'COMPLETE').exists():audit_case_dir(out,p);continue
                    require(not out.exists(),'unfinished case must be reviewed before resume: '+case);wire=fixture/'reference.wire';w=wire.stat().st_size
                    check_budget((5 if pool=='warm' else 4)*w+4*1024**2);out=fresh(out)
                    argv=['run','--input',str(wire),'--pool',pool,'--iterations','2','--max-wire','33554432','--owner-capacity','33554432','--max-inflight-bytes','134217728','--timeout-ms','30000']
                    commands=[launch([str(ref),*argv,'--out-dir',str(out/'reference')],out,'reference',case,'reference')]
                    commands.append(launch([str(b/'n3_network'),*argv,'--out-dir',str(out/'observed')],out,'observed',case,'diagnostic'))
                    for i in range(2):same_file(out/'reference'/f'output-{i}.wire',out/'observed'/f'output-{i}.wire')
                    same_file(out/'observed/output-0.wire',out/'observed/output-1.wire')
                    if pool=='warm':same_file(out/'observed/n3-warm-output.wire',out/'reference/output-0.wire')
                    stdout=read(out/'observed.stdout');reference=read(out/'reference.stdout')
                    require(semantics(stdout)==semantics(reference),'W1 semantic/pool stdout identity mismatch')
                    trace=read(out/'observed/n3-events.json');obs=read(out/'observed/n3-observation.json')
                    summary=checker.audit_case(trace,obs,stdout,kind_catalog_path=kind_catalog,nominal_bytes=nominal,pool=pool)
                    write(out/'summary.json',summary)
                    sidecar=cost_sidecar(out,p,nominal,pool,summary)
                    write(out/'sidecar.json',sidecar);seal(out,{'schema':'n3-network-artifact-v1','kind':'network_case','case':case,'nominal_bytes':nominal,'pool':pool,'commands':commands,'queue_job':job})
    complete=all((p/'cases'/name/'COMPLETE').exists() for name in ['fixture-'+str(n) for n in NOMINALS]+[str(n)+'-'+pool for n in NOMINALS for pool in POOLS])
    if complete:
        for nominal in NOMINALS:
            for pool in POOLS[1:]:same_file(p/'cases'/f'{nominal}-none/observed/output-0.wire',p/'cases'/f'{nominal}-{pool}/observed/output-0.wire')
        write(p/'workload-ledger.json',read(ledger()))
        audit_launches(p)
        seal(p,{'schema':'n3-network-artifact-v1','kind':'collection','case_ids':['fixture-'+str(n) for n in NOMINALS]+[str(n)+'-'+pool for n in NOMINALS for pool in POOLS],'workload_processes':len(read(ledger())['attempts']),'evidence_bytes_observed':budget_bytes(),'queue_job':job,'build_result_sha256':sha(b/'result.json')})
    print(json.dumps({'status':'passed','stage':a.stage,'complete':complete,'workload_attempts':len(read(ledger())['attempts']),'evidence_bytes':budget_bytes()}))

def cost_sidecar(out,p,nominal,pool,summary):
    side={'schema':'n3-network-cost-sidecar-v1','diagnostic':True,'timing_evidence':False,'case':str(nominal)+'-'+pool,'nominal_bytes':nominal,'elements':nominal//4,'pool':pool,'work_iterations':2,'warm_iterations':int(pool=='warm'),'W1_prepared_result_sha256':read(p/'input/result.json')['W1_prepared_result_sha256'],'W1_contract_revision':2,'W1_contract_sha256':read(p/'input/plan/plan.json')['source_bindings']['build/vm-network/prepared-v2/source/contract.json'],'W2_snapshot_sha256':read(p/'input/result.json')['foundation_snapshot_sha256'],'generated_consumer_sha256':sha(p/'input/source/net_reference.cpp'),'anchor_manifest_sha256':sha(p/'input/generation-anchors.json'),'binary_sha256':read(p/'identity.json')['binary_sha256'],'reference_binary_sha256':read(p/'identity.json')['reference_binary_sha256'],'raw_stdout_sha256':sha(out/'observed.stdout'),'reference_stdout_sha256':sha(out/'reference.stdout'),'input_sha256':sha(p/'cases'/('fixture-'+str(nominal))/'reference.wire'),'output_sha256s':[sha(out/'observed'/f'output-{i}.wire') for i in range(2)],'trace_sha256':sha(out/'observed/n3-events.json'),'raw_observation_sha256':sha(out/'observed/n3-observation.json'),'cost_summary':summary,'warm_oracle':'original W1 work0, not original W1 warm artifact'}
    if pool=='warm':side['warm_output_sha256']=sha(out/'observed/n3-warm-output.wire')
    return side

def collection_identity(p):
    build=read(p/'build-result.json');identity=read(p/'identity.json');prepared=read(p/'input/result.json')
    require(build['schema']=='n3-network-artifact-v1' and build['kind']=='build' and build['status']=='passed' and build['timing_evidence'] is False and build['returncode']==0 and '-DN7_NET_DIAGNOSTICS=1' in build['command'],'copied build profile differs')
    require(identity['build_result_sha256']==sha(p/'build-result.json') and build['prepared_result_sha256']==sha(p/'input/result.json'),'collection parent identity differs')
    require(identity['binary_sha256']==build['binary_sha256']==build['files']['n3_network'],'collection binary identity differs')
    require(identity['reference_binary_sha256']==prepared['reference_binary_sha256']==REFERENCE_HASH,'collection reference identity differs')

def expected_commands(collection,case):
    identity=read(collection/'identity.json');base=Path(identity['original_collection_path']);out=base/'cases'/case
    nominal=int(case.split('-')[1] if case.startswith('fixture-') else case.split('-')[0]);observed=identity['original_binary_path'];reference=identity['reference_binary_path']
    if case.startswith('fixture-'):
        return [[binary,'fixture','--output',str(out/(name+'.wire')),'--elements',str(nominal//4)] for binary,name in ((reference,'reference'),(observed,'observed'))]
    pool=case.split('-')[1];argv=['run','--input',str(base/'cases'/('fixture-'+str(nominal))/'reference.wire'),'--pool',pool,'--iterations','2','--max-wire','33554432','--owner-capacity','33554432','--max-inflight-bytes','134217728','--timeout-ms','30000']
    return [[binary,*argv,'--out-dir',str(out/name)] for binary,name in ((reference,'reference'),(observed,'observed'))]

def audit_launches(collection):
    state=read(collection/'workload-ledger.json');require(state['schema']=='n3-network-workload-ledger-v1' and state['max_attempts']==32,'workload ledger schema differs')
    attempts=state['attempts'];require(len(attempts)==32,'workload attempts differ');used=[]
    for case in ['fixture-'+str(n) for n in NOMINALS]+[str(n)+'-'+pool for n in NOMINALS for pool in POOLS]:
        d=read(collection/'cases'/case/'result.json');commands=d['commands'];require(len(commands)==2,'case process count differs')
        for command,argv,profile in zip(commands,expected_commands(collection,case),('reference','diagnostic')):
            index=command['attempt_index'];require(type(index) is int and 0<=index<32,'invalid workload attempt index');used.append(index);item=attempts[index]
            require(command['argv']==argv and command['returncode']==0,'case command differs')
            require(item['index']==index and item['case']==case and item['profile']==profile and item['command']==argv and item['status']=='complete' and item['returncode']==0,'workload ledger binding differs')
    require(sorted(used)==list(range(32)),'workload attempts missing or duplicated')

def audit_case_dir(p,collection):
    d=audit_files(p);checker=analyzer(collection);cat=collection/'input/foundation/event_kinds.json'
    if d['kind']=='fixture':
        same_file(p/'observed.wire',p/'reference.wire');require(read(p/'observed.stdout')==read(p/'reference.stdout'),'fixture raw differs')
        fixture_observation(read(p/'observed.wire.obs.json'),read(p/'observed.stdout'),d['nominal_bytes'])
        summary=checker.audit_fixture(read(p/'observed.wire.n3.json'),read(p/'observed.stdout'),kind_catalog_path=cat,nominal_bytes=d['nominal_bytes'])
    else:
        require(d['kind']=='network_case','unexpected case kind')
        for i in range(2):same_file(p/'observed'/f'output-{i}.wire',p/'reference'/f'output-{i}.wire')
        same_file(p/'observed/output-0.wire',p/'observed/output-1.wire')
        require(semantics(read(p/'observed.stdout'))==semantics(read(p/'reference.stdout')),'W1 stdout semantics differ')
        if d['pool']=='warm':same_file(p/'observed/n3-warm-output.wire',p/'reference/output-0.wire')
        summary=checker.audit_case(read(p/'observed/n3-events.json'),read(p/'observed/n3-observation.json'),read(p/'observed.stdout'),kind_catalog_path=cat,nominal_bytes=d['nominal_bytes'],pool=d['pool'])
        require(read(p/'sidecar.json')==cost_sidecar(p,collection,d['nominal_bytes'],d['pool'],summary),'sidecar identity/counters differ')
    require(summary==read(p/'summary.json'),'case counters differ');return d

def audit(p):
    d=audit_files(p)
    if d['kind']=='prepared':
        for name,digest in PLAN_HASHES.items():require(sha(p/'plan'/name)==digest,'accepted plan changed')
        require(d['accepted_plan_sha256']==sha(p/'plan/plan.json'),'accepted plan changed')
        require(sha(p/'foundation/snapshot.json')==d['foundation_snapshot_sha256']==FOUNDATION_HASH,'foundation snapshot identity differs')
        foundation=read(p/'foundation/snapshot.json')['files']
        for name in ('audit_events.py','event_kinds.json'):require(sha(p/'foundation'/name)==foundation['source/'+name],'accepted checker/catalog identity differs')
        gen=module(p/'tool/network_generate.py','n3_network_regen')
        require(sha(p/'w1-net-reference.cpp')==read(p/'w1-prepared-result.json')['files']['source/net_reference.cpp'],'original W1 source identity differs')
        require(d['W1_prepared_result_sha256']==sha(p/'w1-prepared-result.json')==read(p/'plan/plan.json')['source_bindings']['build/vm-network/prepared-v2/result.json'],'W1 preparation identity differs')
        w1build=read(p/'w1-build-result.json');require(w1build['prepared_result_sha256']==d['W1_prepared_result_sha256'] and w1build['status']=='passed' and w1build['measurement_profile']=='diagnostic' and w1build['timing_evidence'] is False and w1build['binary_sha256']==w1build['files']['net_reference']==REFERENCE_HASH,'W1 build identity differs')
        result,anchors=gen.transform((p/'w1-net-reference.cpp').read_text());require(result==(p/'source/net_reference.cpp').read_text() and anchors==read(p/'generation-anchors.json'),'generated consumer differs')
        for name,digest in d['header_hashes'].items():require(sha(p/'source'/name)==digest==foundation['build/input/diagnostic/'+name],'accepted diagnostic header changed')
        require(d['reference_binary_sha256']==REFERENCE_HASH,'reference binary identity differs')
    elif d['kind']=='build':
        audit(p/'input');require(d['prepared_result_sha256']==sha(p/'input/result.json') and d['binary_sha256']==sha(p/'n3_network'),'build parent/binary mismatch')
        require(d['returncode']==0 and '-DN7_NET_DIAGNOSTICS=1' in d['command'],'not diagnostic build')
        for name,item in d['include_resolution'].items():require(item['sha256']==d['files']['input/source/'+name],'include identity not bound to source')
    elif d['kind']=='collection':
        collection_identity(p)
        audit(p/'input');require(d['build_result_sha256']==sha(p/'build-result.json'),'collection build parent differs');expected=['fixture-'+str(n) for n in NOMINALS]+[str(n)+'-'+pool for n in NOMINALS for pool in POOLS]
        require(d['case_ids']==expected and set(q.name for q in (p/'cases').iterdir())==set(expected),'case inventory differs')
        require(d['workload_processes']==32 and len(read(p/'workload-ledger.json')['attempts'])==32,'workload inventory differs')
        for name in expected:audit_case_dir(p/'cases'/name,p)
        audit_launches(p)
        for nominal in NOMINALS:
            for pool in POOLS[1:]:same_file(p/'cases'/f'{nominal}-none/observed/output-0.wire',p/'cases'/f'{nominal}-{pool}/observed/output-0.wire')
    else:raise CheckFailure('unknown artifact kind')
    return d

def main():
    ap=argparse.ArgumentParser();sub=ap.add_subparsers(dest='command',required=True)
    x=sub.add_parser('prepare');x.add_argument('--out',type=Path,required=True)
    x=sub.add_parser('build');x.add_argument('--prepared',type=Path,required=True);x.add_argument('--out',type=Path,required=True)
    for name,default in [('compiler','/usr/local/bin/clang++'),('sdk','/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk'),('library','/usr/local/lib'),('deps',str(ROOT/'external'))]:x.add_argument('--'+name,type=Path,default=Path(default))
    x=sub.add_parser('collect');x.add_argument('--build',type=Path,required=True);x.add_argument('--out',type=Path,required=True);x.add_argument('--stage',choices=('fixtures','network','all'),default='all')
    x=sub.add_parser('audit');x.add_argument('--record',type=Path,required=True)
    a=ap.parse_args()
    try:
        if a.command=='prepare':prepare(a)
        elif a.command=='build':build(a)
        elif a.command=='collect':collect(a)
        else:d=audit(a.record.resolve());print(json.dumps({'status':'audited','kind':d['kind']}))
        return 0
    except Exception as e:print(type(e).__name__+': '+str(e),file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
