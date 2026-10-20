"""M2-only finite resource-capture metadata. Never dispatch, compile or run native.

ready: publish exact prerequisite fields and source readiness, no final config.
prepare: only after exact independent full/mechanism reviews, bind current
M2-only power/profile/source/input and emit one finite two-child request.
All output files are new; R1/shared/v3 files are read-only.
"""
from pathlib import Path
import argparse,ast,datetime,hashlib,json,os,re,sys
HERE=Path(__file__).resolve().parent
ROOT=Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
D=ROOT/'docs/coordination/c14-iteration-20261020-v2';OUT=D/'validation/screen/M2'
B=Path('/root/xoffset-c14-iteration/20261020/v2/validation/m2-business-v1')
CAPTURE=B.parent/'m2-screen-resource-capture-v1'
FULL=D/'validation/m2_full/M2_FUNCTIONAL_GATES.json'
MECH=D/'validation/diag/M2_MECHANISM_RESULT.json'
DYNAMIC=D/'validation/diag/M2_DYNAMIC_RESULT.json'
FULL_SHA='6af220ba48246bacf65e58b5c776aaa59be691a4fa37bda7cf55261d7b77cceb'
RUNTIME=('screen.py','campaign.py','core.py','protocol.py','execute.py','audit_job.py','finalize.py','host/process_pause.py','host/pause-control.ps1')
COUNTS=dict(full_normal=73,full_SAN=73,business_normal=80,business_SAN=80,V1_main=84,V1_extras=8,AB_native_actions=4,C_native_actions=4,lifecycle_children=52)
def need(v,m):
    if not v:raise ValueError(m)
def fid(p):
    b=Path(p).read_bytes();return dict(bytes=len(b),sha256=hashlib.sha256(b).hexdigest())
def ref(p):return dict(path=str(p),identity=fid(p))
def read(p):return json.loads(Path(p).read_text())
def resolve(p):
    text=str(p).replace('\\','/')
    if text.lower().startswith('c:/'):
        text='/mnt/c/'+re.sub('/+','/',text[3:]).lstrip('/')
    elif 'wsl.localhost/' in text.lower():
        text='/'+text.split('Ubuntu-22.04/',1)[1].lstrip('/');text=re.sub('/+','/',text)
    elif not text.startswith('/'):text=str(D/text)
    return Path(text)
def checked(r):
    p=resolve(r['path']);v=fid(p);need(v['sha256']==r['identity']['sha256'] and v['bytes']==int(r['identity']['bytes']),'changed ref: '+str(p));return p
def put(name,v):
    p=OUT/name;need(p.resolve().parent==OUT.resolve(),'M2-only write domain')
    with p.open('x') as stream:json.dump(v,stream,ensure_ascii=False,indent=2);stream.write('\n')
    return ref(p)
def actual_facts():
    need(fid(FULL)['sha256']==FULL_SHA,'final M2_FUNCTIONAL_GATES identity changed; no R1-style BINDING substitution')
    gates=read(FULL);design=read(D/'core/SCREEN_DESIGN_M2.json');need(gates['status']=='passed' and gates['candidate']=='m2-exact-owns-v1' and gates['candidate_identity']==design['candidate_identity'] and gates['production_files']==design['source']['candidate_production14'] and gates['source154_each_full_rechecked'] is True,'actual final M2 source/full schema')
    checked(gates['request']);checked(gates['candidate_identity']);need(len(design['source']['candidate_members154'])==154 and len(gates['production_files'])==14,'exact154/14 required')
    full=read(checked(gates['full']));need(full['status']=='passed' and full['normal_count']==full['sanitized_count']==73,'own full73+73 required')
    business=read(checked(gates['business']));compat=read(checked(gates['V1_bidirectional']));life=read(checked(gates['lifecycle52']))
    need(business['status']=='passed' and business['native_business_children_each']==80 and compat['status']=='passed' and compat['main_V1_cases']==84 and compat['extra_cases']==8 and compat['AB_native_actions']==compat['C_native_actions']==4,'actual80+80/V1/AB/C inventory')
    need(read(checked(gates['consumers']))['status']==read(checked(gates['shared_runtimes']))['status']=='passed' and life['status']=='passed' and life['actual_native_children']==52 and life['normal_model_cases']==life['sanitized_model_cases']==25 and life['pool_cases']==2,'actual consumers/runtime/lifecycle52')
    mech=read(MECH);dyn=read(DYNAMIC)
    need(mech['status']=='passed' and mech['actual_compiles']==mech['actual_diagnostic_native']==1 and mech['new_control_compiles']==mech['new_control_runs']==0 and mech['source_runtime_before_after_unchanged'] is True and all(not v for v in mech['cleanup'].values()),'actual finite one-candidate mechanism/cleanup required')
    need(checked(mech['mechanism'])==DYNAMIC and dyn.get('returncode',0)==0 and dyn['result']['status']=='mechanism_hit','sealed actual M2 dynamic mechanism required')
    mutate=[v for v in dyn['result']['scopes'] if v['scope']=='mutate'];need(len(mutate)==1 and all(mutate[0][k]>0 for k in ('nonempty_misses','data_reads_removed','canonical_calls_removed')),'positive actual mutate predicate required')
    for name in ('queue_request','queue_result','audit','observer','compile'):checked(mech[name])
    queue=read(checked(mech['queue_result']));need(queue['run_id']==mech['run_id']=='bf6d7ffe921f4646a9efe6785d0fd89c' and queue['status']=='complete' and queue['command_returncode']==0 and all(not v for v in queue['cleanup'].values()),'actual mechanism original lease/cleanup')
    return gates,design,mech,mutate[0]
def requirements(gates,design,mech,mutate):
    return dict(kind='M2-finite-capture-required-independent-reviews',status='pending_actual_independent_records',candidate='m2-exact-owns-v1',full_review=dict(suggested_path=str(D/'reviews/M2_FULL_ACCEPTANCE.json'),required=dict(status='passed or pass',candidate='m2-exact-owns-v1',candidate_identity=gates['candidate_identity'],functional_gates=ref(FULL),source154_each_full_bound=True,counts=COUNTS,mismatches=[]),scope='Independent acceptance of actual entire own M2 source154/14/full73+73/business80+80/V1/consumers/shared-runtime/lifecycle52. A prior partial precheck/business-only/targeted acceptance cannot substitute.'),mechanism_review=dict(suggested_path=str(D/'reviews/M2_MECHANISM_EXECUTION_ACCEPTANCE.json'),required=dict(status='passed or pass',candidate='m2-exact-owns-v1',mechanism_result=ref(MECH),dynamic_result=ref(DYNAMIC),actual_candidate_diagnostic_native=1,new_control_compiles=0,new_control_runs=0,mechanism_hit=True,logical_work_and_call_conservation_passed=True,mismatches=[]),scope='Independent acceptance of exact admitted candidate1/common-C14-control0 execution, source/runtime/input before-after, full truth/queue cleanup and positive mutate conserved data/canonical reduction. Diagnostic time is not AB speed.'),existing_actual_refs=dict(functional_gates=ref(FULL),mechanism_result=ref(MECH),dynamic_result=ref(DYNAMIC),source_gate_binding=ref(D/'core/M2_SCREEN_GATE_BINDING.json'),bare_profile_observation_v3=ref(OUT/'M2_BARE_PROFILE_OBSERVATION_v3.json'),input_policy=ref(OUT/'INPUT_POLICY.json')),positive_mutate_facts=mutate,immutable_rule='Reviews must be new independent files. Do not overwrite R1/shared/v3/any closed runtime input. These fields do not themselves pass a gate or admit capture/screen.',counts=dict(new_compile=0,new_native=0,new_queue=0))
def review_file(p,sha):
    need(p.resolve().parent==(D/'reviews').resolve(),'independent review must be a new review-domain file')
    need(re.fullmatch('[0-9a-f]{64}',sha or '') and fid(p)['sha256']==sha,'exact independent review SHA required');v=read(p)
    need(v.get('status') in ('passed','pass') and v.get('candidate')=='m2-exact-owns-v1' and v.get('mismatches')==[],'actual same-candidate independent passed review required')
    return v
def validate_reviews(a,gates):
    full=review_file(a.full_review,a.full_review_sha256);mechanism=review_file(a.mechanism_review,a.mechanism_review_sha256)
    need(checked(full['functional_gates'])==FULL and checked(full['candidate_identity'])==checked(gates['candidate_identity']) and full['source154_each_full_bound'] is True and all(full['counts'].get(k)==v for k,v in COUNTS.items()),'full review lacks exact entire own M2 source/count acceptance')
    need(checked(mechanism['mechanism_result'])==MECH and checked(mechanism['dynamic_result'])==DYNAMIC and mechanism['actual_candidate_diagnostic_native']==1 and mechanism['new_control_compiles']==mechanism['new_control_runs']==0 and mechanism['mechanism_hit'] is True and mechanism['logical_work_and_call_conservation_passed'] is True,'actual independent execution/positive conserved M2 mechanism review required')
def power(p,fresh):
    need(p.resolve().parent==OUT.resolve(),'fresh power must be M2-only; no R1/shared power record')
    v=read(p);need(v['power_changed'] is False and v['processes_paused']==0 and re.fullmatch('[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}',v['active_scheme_guid']),'actual unchanged M2 power observation')
    age=(datetime.datetime.now(datetime.timezone.utc)-datetime.datetime.fromisoformat(v['observed_utc'].replace('Z','+00:00'))).total_seconds()
    if fresh:need(0<=age<=900,'M2 power observation older than15min; root must supply a new M2-only file, never overwrite an existing runtime input')
    return v
def ready(a):
    gates,design,mech,mutate=actual_facts();req=requirements(gates,design,mech,mutate);r=put('M2_CAPTURE_REVIEW_REQUIREMENTS_v5.json',req)
    p=power(a.power_observation,False);manifest=read(OUT/'M2_TOOL_MANIFEST_v3.json')
    for item in manifest['runtime_files'].values():checked(item)
    files={n:ref(HERE/n) for n in RUNTIME}
    for n in RUNTIME:
        if n.endswith('.py'):ast.parse((HERE/n).read_text())
    out=put('M2_CAPTURE_PREPARATION_READY_v5.json',dict(status='source_ready_waiting_independent_full_mechanism_reviews_and_tool_review',candidate='m2-exact-owns-v1',tool=ref(Path(__file__)),runtime_files=files,execution_worker_modified=False,requirements=r,power_observation=ref(a.power_observation),current_power_guid=p['active_scheme_guid'],schema_note='M2_FUNCTIONAL_GATES is the final functional summary; no R1-style FUNCTIONAL_GATES_BINDING is required or generated.',independent_review_files_present={str(p):p.exists() for p in (D/'reviews/M2_FULL_ACCEPTANCE.json',D/'reviews/M2_MECHANISM_EXECUTION_ACCEPTANCE.json')},budgets=dict(resource_children=2,child_seconds=20,outer_seconds=120,new_compile=0,warmup=0,retry=0,timing_inference_samples=0,screen_children=144,screen_outer_seconds=600,nonchild_seconds=60,seal_seconds=30,tail_seconds_per_child=0.25,RSS_bytes=16*1024**3,allocated_bytes=4*1024**3,free_reserve_bytes=2*1024**3),input_policy=ref(OUT/'INPUT_POLICY.json'),staging_engineering_cost=dict(seconds=1.275457023,retained_allocated_bytes=62218240),outputs_not_generated=['PRECONDITIONS.json','config.json','RESOURCE_CAPTURE_REQUEST.json','RESOURCE_CAPTURE_DISPATCH.json'],actions=dict(compile=0,native=0,queue=0,capture=0,host_pause=0,freeze=0),screen_not_admitted=True))
    print(json.dumps(dict(source_ready=out,requirements=r,tool=ref(Path(__file__)),compile=0,native=0,queue=0)))
def prepare(a):
    need(all((a.full_review,a.full_review_sha256,a.mechanism_review,a.mechanism_review_sha256)),'prepare requires both independently passed exact reviews')
    gates,design,mech,mutate=actual_facts();validate_reviews(a,gates);pw=power(a.power_observation,True)
    need(not CAPTURE.exists() and not any((OUT/n).exists() for n in ('PRECONDITIONS.json','config.json','RESOURCE_CAPTURE_REQUEST.json','RESOURCE_CAPTURE_DISPATCH.json')),'only one fresh M2 final metadata prefix')
    eq_path=D/'reviews/CONTROL_METADATA_EQUIVALENCE.json';eq=read(eq_path)
    need(fid(eq_path)['sha256']=='812a151321ba3cb317747f91395ed6919bd4dd91ffc2e8b6c996a24ebc5ad0cf' and eq['kind']=='control-source-manifest-metadata-equivalence' and eq['status']=='passed' and eq['baseline153_and_production14_identical'] is True and eq['root_actual_member_identities_checked']==308 and eq['mismatches']==[] and eq['compiled_adapter_production_digest_exactly_matches'] is True and eq['scope']=='Only exact historical control packaging manifest exception; no candidate/code/header/dependency/workload exception; preserve old files and declare actual manifest hashes in new freeze. Historical adapter commit string is inherited benchmark metadata, not candidate git identity.','exact parent control metadata-only equivalence and no-candidate exception scope required')
    checked(eq['provenance'])
    eq_fact_path=D/'validation/review/CONTROL_METADATA_EQUIVALENCE_REVIEW.json';eq_fact=read(eq_fact_path)
    need(fid(eq_fact_path)['sha256']=='1ac89b1d75c524b3165396f3fb6346cb2e9f2b9db48850acbcd670cf3ed0bdc8' and eq_fact['status']=='passed' and eq_fact['kind']==eq['kind'] and eq_fact['candidate_exemption'] is False and eq_fact['baseline153_and_production14_identical'] is True and eq_fact['actual_manifest']==eq['actual_manifest'] and eq_fact['expected_manifest']==eq['expected_manifest'],'exact executor no-candidate-exemption fact with same actual/expected manifest required')
    checked(eq['actual_manifest']);checked(eq['expected_manifest'])
    # Import only the already-reviewed frozen worker, after all independent
    # reviews passed. config/request-capture only hash/read/write metadata.
    sys.path.insert(0,str(HERE));import screen as S
    need(2 in os.sched_getaffinity(0),'actual registered CPU2 unavailable')
    affinity=put('M2_CAPTURE_AFFINITY_v5.json',dict(observed_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),allowed_cpus=sorted(os.sched_getaffinity(0)),selected_cpu=2,basis='Existing M2 own bare profile taskset2; not chosen by timing.',native=0,queue=0))
    costs=put('M2_CAPTURE_RESOURCE_COST_POLICY_v5.json',dict(kind='M2-predeclared-finite-capture-resource-policy',status='pending_independent_exact_capture_and_screen_admission',candidate='m2-exact-owns-v1',capture_children=2,child_seconds=20,capture_outer_seconds=120,screen_children=144,screen_outer_seconds=600,nonchild_reserve_seconds=60,seal_reserve_seconds=30,projection_tail_margin_seconds=0.25,tail_basis='Fixed additional0.25s above complete process+full native_result/byte oracle audit per child for dedup/ledger; predeclared before this finite resource capture, no data-based retuning/retry.',staging_seconds=1.275457023,staging_retained_allocated_bytes=62218240,staging_time_and_allocated_charged=True,RSS_bytes=16*1024**3,allocated_bytes=4*1024**3,free_reserve_bytes=2*1024**3,independent_full_acceptance=ref(a.full_review),independent_mechanism_acceptance=ref(a.mechanism_review),future_auxiliary_veto='Original9 auxiliaries, any lower>1 rejects; any zero/undefined blocks promotion. Keep retire/reload/cleanup/total; diagnostics and resource runs never waive future actual gates.',engineering_gate_not_passed_here=True))
    pre=OUT/'PRECONDITIONS.json';put(pre.name,dict(kind='v2-screen-preconditions',status='passed',scope='Actual technical source/profile/input/prerequisite binding only; independent capture/screen execution still required.',candidate='m2-exact-owns-v1',cpu=2,affinity_observation=affinity,cost_policy=costs,functional_summary=ref(FULL),full_request=gates['request'],control_manifest_exception=dict(kind='control-source-manifest-metadata-equivalence',status='passed',baseline153_and_production14_identical=True,actual_manifest=eq['actual_manifest'],expected_manifest=eq['expected_manifest'],provenance=ref(eq_path),executor_fact=ref(eq_fact_path),historical_control_profile=eq['provenance'],candidate_exemption=False),input_policy=ref(OUT/'INPUT_POLICY.json'),projection_tail_margin_seconds=0.25,host_policy=dict(power_guid=pw['active_scheme_guid'],change_power=False,power_observation=ref(a.power_observation)),independent_full_acceptance=ref(a.full_review),independent_mechanism_acceptance=ref(a.mechanism_review),mechanism_result=ref(MECH),dynamic_result=ref(DYNAMIC),metadata_producer=ref(Path(__file__))))
    config=OUT/'config.json';S.config(ROOT,config,B.parent/'M2-screen-20261020-v2-v1',Path('/root/xoffset-receive-completion/20261019/v1/profile-normal-v2.json'),B/'profile-normal-v2.json',pre)
    request=OUT/'RESOURCE_CAPTURE_REQUEST.json';result=S.request_capture(config,CAPTURE,request)
    command=['/usr/bin/python3','-B',str(S.RUNNER),'run','--owner','W3','--kind','functional','--out',str(CAPTURE/'queue'),'--timeout-seconds','120','--wait-seconds','0','--','/usr/bin/python3','-B',str(HERE/'screen.py'),'capture','--config',str(config),'--out',str(CAPTURE),'--queue',str(CAPTURE/'queue'),'--request',str(request),'--request-sha256',result['request_sha256'],'--review',str(OUT/'RESOURCE_CAPTURE_ADMISSION.json')]
    dispatch=put('RESOURCE_CAPTURE_DISPATCH.json',dict(status='pending_independent_exact_capture_admission',candidate='m2-exact-owns-v1',argv=command,cwd=str(ROOT),request=ref(request),independent_full_acceptance=ref(a.full_review),independent_mechanism_acceptance=ref(a.mechanism_review),required_review=dict(kind='independent-resource-capture-admission',status='pending',candidate='m2-exact-owns-v1',request_sha256=result['request_sha256']),actual_actions=dict(compile=0,native=0,queue=0),future_native_budget=2,timing_inference_samples=0,screen_not_admitted=True))
    print(json.dumps(dict(status='exact_request_source_bound_not_dispatched',request=ref(request),dispatch=dispatch,tool=ref(Path(__file__)),compile=0,native=0,queue=0)))
def main():
    p=argparse.ArgumentParser();p.add_argument('mode',choices=('ready','prepare'))
    for name in ('full-review','mechanism-review'):p.add_argument('--'+name,type=Path);p.add_argument('--'+name+'-sha256')
    p.add_argument('--power-observation',type=Path,default=OUT/'WINDOWS_POWER_OBSERVATION.json');a=p.parse_args()
    if a.mode=='ready':ready(a)
    else:prepare(a)
if __name__=='__main__':main()
