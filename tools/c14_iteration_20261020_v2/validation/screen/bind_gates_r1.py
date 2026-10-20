"""Bind existing R1 facts into eleven typed gates; no freeze or execution.

Default emits eight passed gates and three explicitly pending independent
parent decisions. --parent-review completes only those three in new files.
Every published file is write-once. Frozen07ebe inputs/workers are untouched.
"""
from pathlib import Path
import argparse,hashlib,json
ROOT=Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
D=ROOT/'docs/coordination/c14-iteration-20261020-v2';S=D/'validation/screen/R1'
def need(v,m):
    if not v:raise ValueError(m)
def fid(p):
    b=Path(p).read_bytes();return dict(bytes=len(b),sha256=hashlib.sha256(b).hexdigest())
def ref(p):return dict(path=str(p),identity=fid(p))
def read(p):return json.loads(Path(p).read_text())
def checked(r):
    actual=fid(r['path']);need(actual['sha256']==r['identity']['sha256'] and actual['bytes']==int(r['identity']['bytes']),'changed ref: '+r['path']);return Path(r['path'])
def write(p,v):
    p.parent.mkdir(parents=True,exist_ok=True)
    if p.exists():need(read(p)==v,'immutable existing output differs: '+str(p));return
    with p.open('x') as stream:json.dump(v,stream,ensure_ascii=False,indent=2);stream.write('\n')
def members(records):return {n:dict(bytes=int(r['bytes']),sha256=r.get('sha256',r.get('candidate_sha256'))) for n,r in records.items()}
def body(text):
    start='const auto address = reinterpret_cast<std::uintptr_t>(anchor);';end='return recover(record);';need(text.count(start)==1,'canonical cold-body anchor ambiguous');a=text.index(start);b=text.index(end,a)+len(end);return text[a:b]
def inputs():
    cfg=read(S/'config.json');request=read(S/'RESOURCE_CAPTURE_REQUEST.json');need(fid(S/'RESOURCE_CAPTURE_REQUEST.json')['sha256']=='07ebe1dcc9933faff07a3fa438a4a484c1026be1db536043acc310781cb2bdfa','frozen capture request changed')
    common=request['runtime']['sources']['candidate'];source=members(common['members']);production=members(common['production']);identity=cfg['candidate_identity'];checked(identity)
    design=read(D/'core/SCREEN_DESIGN_R1.json');need(source==design['source']['candidate_members154'] and production==design['source']['candidate_production14'] and len(source)==154 and len(production)==14,'capture candidate154/14 differs from fixed design')
    bind=read(D/'validation/r1_full/R1_FUNCTIONAL_GATES_BINDING.json');full=read(D/'validation/r1_full/R1_FULL_RESULT.json');functional=read(checked(bind['functional_gates']));bridge=read(checked(bind['source_bridge']))
    need(bind['status']==bridge['status']==full['status']==functional['status']=='passed' and bind['source154_each_full_rechecked'] is True and bind['production14_rechecked'] is True,'actual full binding status')
    need(members(bridge['candidate_members'])==source and bridge['production_files']['candidate']==full['production_files']==functional['production_files']==production and bind['candidate_identity']==identity,'actual full source154/14/identity differs')
    root_full=D/'reviews/R1_FULL_ACCEPTANCE.json';rootfull=read(root_full);need(rootfull['status']=='pass' and not rootfull['mismatches'] and rootfull['source154_each_full_bound'] is True,'real independent full acceptance required')
    native=read(checked(functional['targeted']));need(native['status']=='passed' and members(native['source154_before'])==members(native['source154_after'])==source and native['normal_programs']==native['sanitized_programs']==5,'targeted actual154/5+5')
    old=D.parent/'c14-iteration-20261019-v1';codegen_path=old/'validation/R1_CODEGEN_RESULT.json';codegen=read(codegen_path);need(codegen['status']=='passed' and members(codegen['candidate_members'])==source and codegen['actual_new_compiles']==1,'old bare codegen actual154/one compile')
    capture=Path('/root/xoffset-c14-iteration/20261020/v2/validation/r1-screen-resource-capture-v1');need(read(capture/'runtime-before.json')==read(capture/'runtime-after.json')==request['runtime'],'actual capture runtime changed');cs=read(capture/'status.json');need(cs['status']=='passed' and [x['role'] for x in cs['completed']]==['control','candidate'],'actual complete finite capture required')
    projection=read(S/'RESOURCE_PROJECTION.json');need(fid(S/'RESOURCE_PROJECTION.json')['sha256']=='700186f42179656d60d082871f2e75eb9e2a823ff92ce3ee3f4d21941fa0169d' and projection['status']=='projection_passed_pending_independent_screen_freeze_admission' and projection['projection']['physical_peak_bytes']<=4*1024**3 and projection['projection']['projected_online_seconds']<=570,'actual complete-process projection')
    dynamic_path=D/'validation/diag/R1_DYNAMIC_RESULT.json';dynamic=read(dynamic_path);need(dynamic['returncode']==0 and dynamic['result']['status']=='mechanism_hit' and all(x['logical_counts_equal'] for x in dynamic['result']['scopes']),'actual dynamic logical work')
    hit=[x for x in dynamic['result']['scopes'] if x['scope']=='read_sum'];need(len(hit)==1 and hit[0]['inactive']==2439240 and hit[0]['active']==0,'actual positive read_sum inactive hit')
    return cfg,request,source,production,identity,bind,full,functional,root_full,native,old,codegen_path,codegen,capture,projection,dynamic_path
def prepare():
    cfg,request,source,production,identity,bind,full,functional,root_full,native,old,codegen_path,codegen,capture,projection,dynamic_path=inputs()
    root_b=D/'reviews/B_ACCEPTANCE.json';b=read(root_b);need(b['status']=='passed_targeted_scope_only' and b['normal_programs_passed']==b['SAN_programs_passed']==5 and b['source154_before_and_after_match_exact_request'] is True,'actual B independent acceptance')
    root_c=D/'reviews/C_EXECUTION_ACCEPTANCE.json';c=read(root_c);need(c['status']=='pass' and not c['mismatches'] and c['R1_read_sum_inactive_count']==2439240 and c['R1_logical_work_equal'] is True,'actual C independent logical mechanism acceptance')
    rootfull=read(root_full);need(rootfull['counts']['full_normal']==rootfull['counts']['full_SAN']==73 and rootfull['counts']['business_normal']==rootfull['counts']['business_SAN']==80 and rootfull['counts']['lifecycle_children']==52,'actual independent full counts')
    reports={};common_evidence=[ref(D/'validation/r1_full/R1_FUNCTIONAL_GATES_BINDING.json'),bind['source_bridge'],bind['candidate_identity'],ref(D/'validation/r1_full/R1_FUNCTIONAL_GATES.json')]
    scope_limit='Source154/14 denotes the original uninstrumented R1 candidate basis. Diagnostic mechanism evidence has its independently admitted materialization/hook bridge; it does not pretend its instrumented source bytes equal bare candidate154.'
    def gate(name,assertions,evidence,review=None,scope=None):
        for r in evidence:checked(r)
        record=dict(kind='v2-screen-gate-receipt',gate=name,status='passed' if review else 'pending_parent_independent_acceptance',candidate_identity=identity,source154=source,production14=production,assertions=assertions,evidence=evidence,source_binding_evidence=common_evidence,source_scope=scope_limit,independent_review=dict(status='passed',record=ref(review),scope=scope) if review else None,performance_admitted=False,screen_admitted=False)
        path=S/'gate-receipts-v1'/(name+'.json');write(path,record);reports[name]=ref(path)
    codegen_evidence=[ref(codegen_path),ref(old/'core/REQUESTS_R1_CODEGEN.json'),ref(old/'validation/R1_CODEGEN_JOB_ADMISSION.json'),ref(old/'validation/R1_CODEGEN_DISPATCH_RESULT.json'),ref(old/'core/REVIEW_R1.md')]
    for role in ('control','candidate'):
        a=codegen['evidence'][role]['assembly'];r=dict(path=a['path'],identity={k:a[k] for k in ('bytes','sha256')});checked(r);codegen_evidence.append(r)
    codegen_evidence.extend([cfg['binaries']['control']['metadata'],cfg['binaries']['candidate']['metadata'],cfg['binaries']['control']['compile_commands'],cfg['binaries']['candidate']['compile_commands'],ref(Path(cfg['closure']['dependencies'])),ref(Path(cfg['closure']['control-dependencies']))])
    gate('codegen',dict(candidate_compiles=1),codegen_evidence)
    gate('mechanism',dict(actual_nonempty_path_observed=True,static_actual_codegen_passed=True,unhooked_screen_binary=True,actual_read_sum_inactive=2439240,actual_read_sum_active=0,logical_work_equal=True),[ref(dynamic_path),ref(D/'validation/diag/C_RESULT.json'),ref(D/'reviews/DYNAMIC_ACCEPTANCE_REVIEW.md'),ref(D/'reviews/C_LEDGER_REVIEW.md'),ref(codegen_path),cfg['binaries']['candidate']['metadata'],ref(S/'RESOURCE_CAPTURE_REQUEST.json')],root_c,'Positive actual R1 read_sum inactive logical mechanism and full diagnostic admission only; bare codegen independent acceptance remains its separate pending gate. No timing benefit inferred.')
    gate('targeted',dict(normal=5,sanitized=5),[functional['targeted'],native['receipt'],ref(D/'core/REQUESTS_R1_NATIVE_v2.json')],root_b,'Exact five normal/five SAN; historical outer61B before/after limitation retained. No retrospective transitive-tool claim.')
    for mode,name in (('normal','full_normal'),('sanitized','full_san')):
        evidence=list(bind['actual_full'][mode].values());receipt=read(checked(bind['actual_full'][mode]['receipt']));need(receipt['status']=='passed' and len(receipt['junit_names'])==73 and receipt['production_files']==production,'actual own full73 '+mode)
        gate(name,dict(ctest_passed=73),[ref(D/'validation/r1_full/R1_FULL_RESULT.json'),*evidence],root_full,'Actual own R1 full73 with source154/14 bound; no M1 substitution.')
    business=read(checked(functional['business']));need(business['status']=='passed' and business['native_business_children_each']==80,'own SML80+80')
    gate('business_SML',dict(normal_children=80,sanitized_children=80),[functional['business'],ref(D/'validation/review/R1_FULL_ENTRY_BUSINESS_FUNCTIONAL_RESULT.json'),ref(D/'validation/review/R1_FULL_ENTRY_BUSINESS_BENCH_WORKER.json')],root_full,'Actual own normal80/SAN80 S/M/L full truth.')
    compat=read(checked(functional['V1_bidirectional']));need(compat['status']=='passed' and compat['main_V1_cases']==84 and compat['extra_cases']==8 and compat['AB_native_actions']==compat['C_native_actions']==4,'actual V1 inventory')
    gate('v1_bidirectional',dict(main_cases=84,extra_cases=8,AB_actions=4,C_actions=4),[functional['V1_bidirectional'],functional['business']],root_full,'Actual V1 both directions and existing A/B/C extension cases.')
    need(read(checked(functional['consumers']))['status']==read(checked(functional['shared_runtimes']))['status']=='passed','actual consumers/shared runtimes')
    gate('consumers',dict(install_subproject_passed=True,shared_runtime_passed=True),[functional['consumers'],functional['shared_runtimes'],ref(D/'validation/review/R1_FULL_ENTRY_BUSINESS_PACKAGE_WORKER.json'),ref(D/'validation/review/R1_FULL_ENTRY_BUSINESS_RUNTIME_WORKER.json')],root_full,'Actual installation/subproject/relocated-prefix/archive and shared-runtime R1 consumers.')
    life=read(checked(functional['lifecycle52']));need(life['status']=='passed' and life['actual_native_children']==52 and life['normal_model_cases']==life['sanitized_model_cases']==25 and life['pool_cases']==2,'actual lifecycle52 inventory')
    gate('lifecycle',dict(actual_native_children=52,normal_model_cases=25,sanitized_model_cases=25,pool_cases=2),[functional['lifecycle52'],ref(D/'validation/review/R1_FULL_ENTRY_BUSINESS_LIFECYCLE_WORKER.json')],root_full,'Own R1 ABDE/C lifecycle matrix52, including25 normal/25 SAN model cases and2 pool cases.')
    source_root=ROOT/'build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1/source';a=body((ROOT/'xoffsetdatastructure.hpp').read_text());z=body((source_root/'xoffsetdatastructure.hpp').read_text());need(a==z,'cold body source changed');cold=dict(characters=len(a),utf8_bytes=len(a.encode()),sha256=hashlib.sha256(a.encode()).hexdigest(),actual_equal=True,source_review=ref(old/'core/REVIEW_R1.md'),baseline=ref(ROOT/'xoffsetdatastructure.hpp'),candidate=ref(source_root/'xoffsetdatastructure.hpp'))
    safety_evidence=[ref(old/'core/REVIEW_R1.md'),ref(old/'core/R1_DESIGN.md'),ref(D/'core/REVIEW_R1_NATIVE.md'),functional['targeted'],functional['lifecycle52'],ref(root_full),ref(D/'validation/r1_full/R1_FULL_RESULT.json')]
    gate('safety',dict(representation_and_lifetime_preserved=None),safety_evidence)
    costs=[ref(S/'PREDECLARED_RESOURCE_COST_POLICY.json'),ref(S/'RESOURCE_CAPTURE_REQUEST.json'),ref(S/'RESOURCE_CAPTURE_ADMISSION.json'),ref(S/'RESOURCE_PROJECTION.json'),ref(capture/'status.json'),ref(capture/'manifest.json'),ref(capture/'queue/request.json'),ref(capture/'queue/result.json'),ref(S/'INPUT_POLICY.json'),ref(D/'reviews/SCREEN_INPUT_POLICY_ACCEPTANCE.json'),ref(D/'reviews/C_LEDGER_REVIEW.md'),ref(D/'core/C_WORK_LEDGER.json'),ref(old/'core/R1_DESIGN.md'),ref(codegen_path),ref(D/'core/SCREEN_DESIGN_R1.json'),ref(D/'reviews/STATISTICAL_PRECHECK.md')]
    for role in ('control','candidate'):costs.append(ref(capture/'commands'/(role+'.receipt.json')))
    gate('engineering',dict(predeclared_cost_policy_satisfied=None),costs)
    pending=['codegen','safety','engineering'];write(S/'GATES.json',dict(kind='v2-screen-complete-gates-admission',status='pending_parent_codegen_safety_engineering_acceptance',candidate_identity=identity,gates=reports,passed_gates=[n for n in reports if n not in pending],pending_gates=pending,no_freeze_admission=True))
    parent_request=dict(kind='R1-screen-parent-three-gate-review-request',status='pending_parent_independent_review',candidate='r1-canonical-hotcold-v1',candidate_identity=identity,source154=source,production14=production,typed_gates=reports,pending_gates=pending,already_passed_gates=[n for n in reports if n not in pending],actual_cold_body_equality=cold,requested_new_parent_file=str(D/'reviews/R1_SCREEN_SOURCE_COST_GATES_ACCEPTANCE.json'),immutability='Write a new independent acceptance file only. Do not append to or rewrite R1_FULL_ACCEPTANCE/B_ACCEPTANCE/C_EXECUTION_ACCEPTANCE/config/PRECONDITIONS or any07ebe/capture input; historical before/after bytes remain exact.',required_fields=dict(status='passed',candidate='r1-canonical-hotcold-v1',candidate_identity=identity,source154=source,production14=production,accepted_gates=dict(codegen=dict(candidate_compiles=1),safety=dict(representation_and_lifetime_preserved=True),engineering=dict(predeclared_cost_policy_satisfied=True)),evidence='Nonempty exact path/identity refs to actual reviewed facts, including this request and the independent decisions; no status-only self-reference.',screen_admitted=False,performance_admitted=False),review_questions={'codegen':'Independently accept the actual old bare asm mechanism: control16 outlined canonical calls and5 pushes before ledger; candidate14 inline acquire-load/test zero branches bypass14 active-helper calls, active helper remains outlined. Same original R1 source154/14; current bare profiles/deps and metadata-literal bridge are separate facts. Retain historical transitive outer61B binding limitation; do not recompile control or infer speed from asm text size.','safety':'Explicitly accept source-level representation/lifetime preservation for this exact fixed supported candidate/profile after reading original independent safety proof plus completed current owner/OOM/callback/full/lifecycle evidence. Source cold body exact equality above; no fields/layout/registry/TLS/retirement changes or cross-call pointer/zero cache. Do not claim SAN proves all ISO rules or concurrent unsynchronized release.','engineering':'Explicitly accept predeclared build/maintenance/free/retained/active-memory engineering bounds before seeing screen AB outcomes. Disclose potential extra active call/frame and code footprint; source adds no object/registry/TLS storage, payload work or new lifetime/free operation. Resource capture/projection passes only its registered wall/allocated/RSS/free bounds, not comparative speed. Charge staging1.275457023s/62218240B, actual full-process+full-body audit, reserved0.25s tail eachchild, all144 independent wires. Preserve future original9 auxiliary veto: any lower>1 rejects, any zero/undefined blocks, all retire/reload/cleanup/total retained; current diagnostics/capture cannot waive future auxiliary or actual engineering audits.'},raw_evidence_by_gate={n:read(reports[n]['path'])['evidence'] for n in pending},resource_projection_facts=projection['projection'],historical_binding_limit='Prior targetedB and old bare-codegen evidence retain their actual transitive61B before/after scope; actual assembly/source facts remain reviewable. New full/business/current bareprofile/capture closures explicitly bind actual61B before/after. No historical rewrite, repeated compile or new native.',performance_inference=False,new_compile=0,new_native=0,new_queue=0,freeze_executed=False)
    write(S/'PARENT_GATE_REVIEW_REQUEST.json',parent_request)
    print(json.dumps(dict(status='8_actual_passed_3_pending_parent_review',gates=ref(S/'GATES.json'),parent_request=ref(S/'PARENT_GATE_REVIEW_REQUEST.json'),cold_body=cold,compile=0,native=0,queue=0,freeze=0)))
def complete(parent):
    need(parent==D/'reviews/R1_SCREEN_SOURCE_COST_GATES_ACCEPTANCE.json','new designated independent parent record required')
    old=read(S/'GATES.json');a=read(parent);need(a['status']=='passed' and a['candidate']=='r1-canonical-hotcold-v1' and a['candidate_identity']==old['candidate_identity'],'new exact parent acceptance required')
    request=read(S/'PARENT_GATE_REVIEW_REQUEST.json');need(a['source154']==request['source154'] and a['production14']==request['production14'],'parent exact154/14 required');need(set(a['accepted_gates'])=={'codegen','safety','engineering'},'exact three parent decisions')
    need(type(a.get('evidence'))is list and a['evidence'],'parent must name actual evidence refs')
    for r in a['evidence']:checked(r)
    expected={'codegen':{'candidate_compiles':1},'safety':{'representation_and_lifetime_preserved':True},'engineering':{'predeclared_cost_policy_satisfied':True}};records=dict(old['gates'])
    for name,values in expected.items():
        need(all(a['accepted_gates'][name].get(k)==v for k,v in values.items()),'parent did not pass '+name)
        original=read(checked(records[name]));need(original['status']=='pending_parent_independent_acceptance','original pending gate expected');original.update(status='passed',assertions=values,independent_review=dict(status='passed',record=ref(parent),scope='New independent parent acceptance of this exact source/cost gate; capture/screen execution still require separate admission.'));original['evidence'].extend([ref(S/'PARENT_GATE_REVIEW_REQUEST.json'),ref(parent)]);path=S/'gate-receipts-v2'/(name+'.json');write(path,original);records[name]=ref(path)
    for r in records.values():need(read(checked(r))['status']=='passed','all eleven gates must pass')
    write(S/'GATES_v2.json',dict(kind='v2-screen-complete-gates-admission',status='passed',candidate_identity=old['candidate_identity'],gates=records,parent_three_gate_acceptance=ref(parent),previous_pending=ref(S/'GATES.json'),freeze_not_executed=True,screen_admitted=False))
    print(json.dumps(dict(status='all11_bound_not_frozen',gates=ref(S/'GATES_v2.json'),freeze=0,compile=0,native=0,queue=0)))
def main():
    p=argparse.ArgumentParser();p.add_argument('--parent-review',type=Path);a=p.parse_args()
    if a.parent_review:complete(a.parent_review)
    else:prepare()
if __name__=='__main__':main()
