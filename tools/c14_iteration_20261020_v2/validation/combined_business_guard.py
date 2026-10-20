"""Read/hash-only before/after proof for existing combined business dispatchers.

This file never imports project workers, launches a command, touches a lock or
starts a queue. Root invokes the registered core argv separately, outside leases.
Before requires a new stage-specific Windows observation (<=15min,8GiB); after
preserves that admission and checks actual current guest/backing free space.
"""
import argparse,datetime,hashlib,json,re,shutil,stat
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
D=ROOT/'docs/coordination/c14-iteration-20261020-v2'
DOC=D/'validation/combined_full';OUT=DOC/'business_guard'
H=ROOT/'tools/c14_iteration_20261020_v2/core';BH=H/'combined_full/business';MH=H/'combined_full/business_matrix';BD=DOC/'business'
V=Path('/root/xoffset-c14-iteration/20261020/v2/validation');B=V/'combined-business-v1';M=V/'combined-business-matrix-v1'
Q=D/'core/REQUESTS_COMBINED_FULL.json';INITIAL=D/'core/REQUESTS_COMBINED_INITIAL.json';EXTERNAL=D/'core/REQUESTS_COMBINED_EXTERNAL.json'
IDENT=ROOT/'build/c14-iteration-20261020-v2/candidates/r1-m2-combined-v1/IDENTITY.json'
RUNNER=Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py')
RUNNER_SHA='6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909'
LOCK=RUNNER.parents[2]/'build/vm-coordination'
WRAPPER=Path('/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/bin/clang++-libcxx')
WID=dict(bytes=61,sha256='c8ea06a7c58557e3b89d09873dc88457d7b0c5d9af3f08cf0449093f2fbd1b87')
STAGES=('prepare','bench','functional','package','runtime','lifecycle','final')
CAP=dict(prepare=300,bench=2400,functional=None,package=2400,runtime=2400,lifecycle=3600,final=120)
INITIAL_GUARD=Path(__file__).with_name('combined_initial_guard.py')
INITIAL_GUARD_SHA='f298c76860682e3a66dbbae72824ed450950efdaa2384e2824173b366dcee84c'

def need(ok,why):
    if not ok:raise ValueError(why)

def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))

def fid(p):
    p=Path(p);need(stat.S_ISREG(p.stat().st_mode),'regular input required: '+str(p));digest=hashlib.sha256()
    with p.open('rb') as stream:
        for b in iter(lambda:stream.read(1024*1024),b''):digest.update(b)
    return dict(bytes=p.stat().st_size,sha256=digest.hexdigest())

def ref(p):return dict(path=str(p),identity=fid(p))

def keep(inputs,p,expected=None):
    p=Path(p);actual=fid(p)
    need(expected is None or (actual['sha256']==expected['sha256'] and ('bytes' not in expected or actual['bytes']==int(expected['bytes']))),'changed input: '+str(p))
    inputs[str(p)]=actual;return actual

def exact(inputs,r):keep(inputs,r['path'],r['identity']);return Path(r['path'])

def put(p,v):
    with p.open('x',encoding='utf-8') as stream:json.dump(v,stream,ensure_ascii=False,indent=2);stream.write('\n')

def path(stage,side):return OUT/(stage.upper()+'_'+side.upper()+'.json')

def result_path(stage):
    return {'prepare':BD/'PREPARED.json','bench':B/'bench-profiles-v2/result.json','functional':B/'final-business-functional-v2/result.json','package':B/'package-standard-v2/result.json','runtime':B/'package-runtime-v2/result.json','lifecycle':M/'production-C-matrix-v2/result.json','final':DOC/'COMBINED_FUNCTIONAL_GATES.json'}[stage]

def registered(stage):
    return ['/usr/bin/python3','-I','-B',str(H/'prepare_combined_business.py')] if stage=='prepare' else ['/usr/bin/python3','-B',str(MH/'dispatch.py'),'run'] if stage=='lifecycle' else ['/usr/bin/python3','-I','-B',str(H/'finalize_combined_full.py')] if stage=='final' else ['/usr/bin/python3','-B',str(BH/'dispatch.py'),stage]

def resources(host=None):
    actual=dict(observed_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),guest_free_bytes=shutil.disk_usage('/root').free,backing_current_free_bytes=shutil.disk_usage('/mnt/c').free)
    need(min(actual['guest_free_bytes'],actual['backing_current_free_bytes'])>=8*1024**3,'actual guest/backing below8GiB')
    if host:
        v=read(host);age=(datetime.datetime.now(datetime.timezone.utc)-datetime.datetime.fromisoformat(v['observed_utc'].replace('Z','+00:00'))).total_seconds()
        need(0<=age<=900 and v['backing_free_bytes']>=8*1024**3,'fresh Windows8GiB observation required per stage')
        actual['windows_observation']=ref(host)
    return actual

def queue(p,command,cap,owner='W1',kind='validation'):
    q=read(p/'request.json');r=read(p/'result.json')
    need(q['out']==str(p) and q['lock_dir']==str(LOCK) and q['cwd']==str(ROOT) and q['runner_sha256']==RUNNER_SHA and q['command']==command and q['timeout_seconds']==cap and q['owner']==owner and q['kind']==kind,'original exact lease request differs: '+str(p))
    need(r['run_id']==q['run_id'] and r['status']=='complete' and r['command_returncode']==0 and r['reason']=='command_exit' and r['cleanup']==dict(live=[],process_groups=[],zombies=[]),'original lease failed/unclean: '+str(p))
    return dict(request=ref(p/'request.json'),result=ref(p/'result.json'),run_id=q['run_id'])

def actual_queues(stage):
    inputs=str(BD/'inputs.json')
    if stage=='final':return []
    if stage=='functional':
        tool=B/'functional-tools-v2/campaign.py';rows=[queue(B/'queues/functional-tools-v2',['/usr/bin/python3','-B',str(BH/'workers/prepare-functional-tools.py'),'--inputs',inputs],300)]
        for mode in ('normal','sanitized'):
            out=B/('functional-'+mode+'-v2');rows.append(queue(out/'queue',['/usr/bin/python3','-B',str(tool),'functional','--prepared','/root/xoffset-serialization-comparison/20261017/implementation/prepared-v1','--profile',str(B/('profile-'+mode+'-v2.json')),'--out',str(out),'--queue',str(out/'queue')],1800,'W3','functional'))
            pin=read(BD/('functional-'+mode+'-v2-external-pin.json'))['composite_pin'];rows.append(queue(B/('queues/functional-'+mode+'-audit-v2'),['/usr/bin/python3','-B',str(tool),'audit','--bundle',str(out),'--postrun-pin',pin],600))
        rows.append(queue(B/'queues/business-compatibility-v2',['/usr/bin/python3','-B',str(BH/'workers/compatibility-worker.py'),'--inputs',inputs],1500));return rows
    if stage=='lifecycle':return [queue(M/'queues/production-C-matrix-v2',['/usr/bin/python3','-B',str(MH/'dispatch.py'),'worker'],3600)]
    label={'prepare':'combined-business-prepare-v1','bench':'bench-profiles-v2','package':'package-standard-v2','runtime':'package-runtime-v2'}[stage]
    p=V/'leases'/label if stage=='prepare' else B/'queues'/label
    script={'prepare':BH/'prepare.py','bench':BH/'workers/bench-profiles-worker.py','package':BH/'workers/package/package-worker.py','runtime':BH/'workers/package/runtime-worker.py'}[stage]
    args=[] if stage=='prepare' else ['--inputs',inputs] if stage=='bench' else ['--spec',str(BD/'package-input.json')]
    return [queue(p,['/usr/bin/python3','-B',str(script),*args],CAP[stage])]

def check(stage,sha):
    need(re.fullmatch('[0-9a-f]{64}',sha or '') and fid(Q)['sha256']==sha,'exact admitted current combined full request SHA required')
    inputs={};q=read(Q);iq=read(INITIAL);eq=read(EXTERNAL);identity=read(IDENT)
    need(q['candidate']==iq['candidate']==eq['candidate']==identity['candidate']=='r1-m2-combined-v1' and q['source_members']==iq['source_members']==eq['source_members']==identity['members'] and len(q['source_members'])==154 and len(q['production14'])==14 and q['production14']==iq['production14'],'own combined154/14 request identity required')
    need(fid(INITIAL)['sha256']=='3006c3057c6f05300d391e71cb5a70ccdfd0417abc1819bdc9df1c836bde0e2c' and fid(EXTERNAL)['sha256']=='88e8d3a1ab95c0ac5e6d2b65632a38666447f17d781a1ae028229610d0cf07ff','immutable initial/external changed')
    keep(inputs,INITIAL_GUARD,dict(sha256=INITIAL_GUARD_SHA));keep(inputs,Q);keep(inputs,INITIAL,q['initial_request']['identity']);keep(inputs,EXTERNAL);keep(inputs,IDENT,q['candidate_identity']);keep(inputs,Path(__file__).resolve())
    for mapping in (q['tool_files'],iq['tool_files'],eq['input_files']):
        for p,want in mapping.items():keep(inputs,p,want)
    keep(inputs,RUNNER,dict(sha256=RUNNER_SHA));keep(inputs,WRAPPER,WID)
    prior=read(DOC/'INITIAL_EXECUTION_BINDING.json');need(prior['status']=='inputs_bound_not_executed','actual immutable initial before binding required');keep(inputs,DOC/'INITIAL_EXECUTION_BINDING.json')
    for p,want in prior['inputs'].items():keep(inputs,p,want)
    source={n:dict(bytes=x['bytes'],sha256=x['candidate_sha256']) for n,x in q['source_members'].items()};need(source==prior['source154'] and q['production14']==prior['production14'],'initial source map differs')
    for n,want in source.items():keep(inputs,IDENT.parent/'source'/n,want)
    need(source['xoffsetdatastructure.hpp']['sha256']=='155f0847dff72047ac580ae8d3ead6927e52ff84eb5c9062ea2d4999980cd501','selected actual combined main required')
    for mode in ('normal','sanitized','external'):
        p=DOC/('INITIAL_'+mode.upper()+'_VERIFIED.json');verified=read(p)
        need(verified['status']=='passed' and verified['stage']==mode and verified['source154_before_after_unchanged'] is True,'actual own initial verified stage missing')
        keep(inputs,p);exact(inputs,verified['initial_binding']);actual=read(exact(inputs,verified['actual_receipt']));need(actual['status']=='passed','actual initial receipt not passed')
        for k in ('request','result'):exact(inputs,verified['queue'][k])
        qr=read(verified['queue']['request']['path']);rr=read(verified['queue']['result']['path'])
        need(qr['run_id']==rr['run_id']==verified['queue']['run_id'] and rr['status']=='complete' and rr['command_returncode']==0 and rr['cleanup']==dict(live=[],process_groups=[],zombies=[]),'actual initial lease result failed/unclean')
    provider=prior['provider'];need(len(provider)>=851,'locked provider incomplete')
    vendor=Path('/root/xoffset-receive-completion/20261019/v1/final-v4/source')
    for n,want in provider.items():keep(inputs,vendor/n,want)
    full=read(DOC/'COMBINED_FULL_RESULT.json');need(full['status']=='passed' and full['request_identity']==fid(INITIAL) and full['production_files']==q['production14'] and full['normal_count']==full['sanitized_count']==73,'own combined full73+73 required');keep(inputs,DOC/'COMBINED_FULL_RESULT.json')
    for mode,key in (('normal','normal_receipt'),('sanitized','sanitized_receipt')):
        p=exact(inputs,full[key]);receipt=read(p);need(receipt['status']=='passed' and receipt['production_files']==q['production14'] and len(receipt['junit_names'])==len(set(receipt['junit_names']))==73,'own full receipt differs')
        for n,want in {**source,**provider}.items():keep(inputs,p.parent/'source'/n,want)
        for n,k in (('ctest.xml','junit_identity'),('build/test-inventory.json','inventory_identity'),('build/compile_commands.json','compile_commands_identity')):keep(inputs,p.parent/n,receipt[k])
    jobid='COMBINED-LIFECYCLE52-v1' if stage=='lifecycle' else 'COMBINED-FINAL-FUNCTIONAL-v1' if stage=='final' else 'COMBINED-BUSINESS-'+stage+'-v1'
    jobs=[j for j in q['jobs'] if j['id']==jobid];need(len(jobs)==1 and jobs[0]['argv']==registered(stage) and jobs[0]['timeout_seconds']==CAP[stage],'exact core dispatcher/cap differs')
    if (BD/'PREPARED.json').exists():
        prepared=read(BD/'PREPARED.json');need(prepared['status']=='prepared' and prepared['requests']==fid(Q) and prepared['production_files']==q['production14'],'own prepared identity differs');keep(inputs,BD/'PREPARED.json')
        for p,want in prepared['bindings'].items():keep(inputs,p,want)
    elif stage!='prepare':raise ValueError('actual prepare required before later stage')
    if stage in ('functional','package','runtime','lifecycle','final'):
        bench=read(B/'bench-profiles-v2/result.json');need(bench['status']=='passed' and bench['production_files']==q['production14'],'own actual bench profiles required')
        keep(inputs,B/'bench-profiles-v2/result.json');source_identity=read(BD/'candidate-source-identity-v2.json');keep(inputs,BD/'candidate-source-identity-v2.json')
        for mode in ('normal','sanitized'):
            p=B/('profile-'+mode+'-v2.json');profile=read(p);keep(inputs,p,bench['profiles'][mode])
            binding=B/('compiled-source-binding-'+mode+'-v2.json');value=read(binding);keep(inputs,binding)
            need(value['production_files']==q['production14'] and value['profile']==fid(p) and profile['binaries']['xoffset']['library']=='xoffset-c14-r1-m2-combined-v1' and profile['binaries']['xoffset']['source_content_sha256']==source_identity['source_content_sha256'],'own combined compiled source/profile label/content differs')
            for library in ('xoffset','cista','flatbuffers','capnproto','cereal'):keep(inputs,profile['binaries'][library]['path'])
    if stage!='prepare':
        previous=path(STAGES[STAGES.index(stage)-1],'after');need(read(previous)['status']=='passed','actual previous guarded stage required');keep(inputs,previous)
    return dict(inputs=inputs,source154=source,production14=q['production14'],dependency_provider=provider,request=ref(Q),registered_job=jobs[0],wrapper61=fid(WRAPPER))

def evidence(stage,a):
    p=result_path(stage);v=read(p);need(v['status']==('prepared' if stage=='prepare' else 'passed'),'actual stage result missing/failed')
    if stage=='prepare':need(v['requests']==a['request']['identity'] and v['production_files']==a['production14'],'actual prepare request/14 differs')
    if stage=='bench':need(v['production_files']==a['production14'] and v['new_contract_cases_each']==2,'actual own bench source/contracts')
    if stage=='functional':need(v['production_files']==a['production14'] and v['native_business_children_each']==80 and v['main_V1_cases']==84 and v['extra_cases']==8 and v['AB_native_actions']==v['C_native_actions']==4,'actual own80+80/V1/AB/C counts')
    if stage=='lifecycle':need(v['actual_native_children']==52 and v['normal_model_cases']==v['sanitized_model_cases']==25 and v['pool_cases']==2,'actual lifecycle52 incomplete')
    if stage=='final':need(v['candidate']=='r1-m2-combined-v1' and v['production_files']==a['production14'] and v['request']['identity']==a['request']['identity'] and v['performance_accepted'] is False,'actual own final request/14 scope')
    return ref(p)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('before','after'));p.add_argument('stage',choices=STAGES);p.add_argument('--request-sha256',required=True);p.add_argument('--windows-observation',type=Path);args=p.parse_args()
    target=path(args.stage,args.action);need(not target.exists(),'exclusive stage proof already exists')
    if args.action=='before':
        need(args.windows_observation is not None and args.windows_observation.is_absolute(),'fresh root-created per-stage Windows observation required');need(not result_path(args.stage).exists(),'preserve prior stage outputs; no retry')
        a=check(args.stage,args.request_sha256);res=resources(args.windows_observation);keep(a['inputs'],args.windows_observation);OUT.mkdir(exist_ok=True)
        put(target,dict(status='inputs_bound_not_executed',stage=args.stage,**a,resources=res,actions=dict(compiler=0,native=0,queue=0),dispatcher_executed_by_guard=False));print(json.dumps(ref(target)));return
    before=path(args.stage,'before');a=read(before);need(a['status']=='inputs_bound_not_executed' and a['stage']==args.stage and a['request']['identity']['sha256']==args.request_sha256,'same-stage exact before required')
    for name,want in a['inputs'].items():need(fid(name)==want,'bound input changed across stage: '+name)
    current=check(args.stage,args.request_sha256)
    need(current['source154']==a['source154'] and current['production14']==a['production14'] and current['dependency_provider']==a['dependency_provider'] and fid(WRAPPER)==a['wrapper61']==WID,'source/provider/61B changed across stage')
    actual=evidence(args.stage,a);queues=actual_queues(args.stage);res=resources()
    put(target,dict(status='passed',scope='Actual read/hash before-after source/runtime/provider/lease-result binding only; independent combined gate acceptance remains required.',stage=args.stage,before=ref(before),request=a['request'],source154_before_after_unchanged=True,provider_before_after_unchanged=True,actual_all_runtime_inputs_rehashed=True,wrapper61_after=fid(WRAPPER),actual_result=actual,queues=queues,after_inputs=current['inputs'],resources=res,actions=dict(compiler=0,native=0,queue=0),combined_gate_accepted=False,screen_admitted=False,QF_admitted=False));print(json.dumps(ref(target)))

if __name__=='__main__':main()
