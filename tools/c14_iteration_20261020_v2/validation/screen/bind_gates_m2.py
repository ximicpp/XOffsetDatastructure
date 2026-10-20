"""Source-only M2 gate binding; no child, checker, freeze or queue execution.

prepare --review-map EIGHT.json: eight actual gates plus three pending decisions.
complete --review-map THREE.json: new receipts for the three independent decisions.
Map contract: validation/screen/M2/M2_GATE_REVIEW_MAP_REQUEST.md.
Missing, changed or unaccepted evidence raises; nothing is silently substituted.
"""
from pathlib import Path
import argparse
import hashlib
import json

ROOT=Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
D=ROOT/'docs/coordination/c14-iteration-20261020-v2'
S=D/'validation/screen/M2'
F=D/'validation/m2_full/M2_FUNCTIONAL_GATES.json'
CORE=D/'core/M2_SCREEN_GATE_BINDING.json'
CANDIDATE='m2-exact-owns-v1'
EIGHT=('mechanism','targeted','full_normal','full_san','business_SML','v1_bidirectional','consumers','lifecycle')
THREE=('codegen','safety','engineering')
PARENT_ASSERTIONS={'codegen':{'candidate_compiles':1},'safety':{'representation_and_lifetime_preserved':True},'engineering':{'predeclared_cost_policy_satisfied':True}}

def need(v,m):
    if not v:raise ValueError(m)
def fid(p):
    p=Path(p);h=hashlib.sha256()
    with p.open('rb') as stream:
        for b in iter(lambda:stream.read(1024**2),b''):h.update(b)
    return dict(bytes=p.stat().st_size,sha256=h.hexdigest())
def ref(p):return dict(path=str(p),identity=fid(p))
def read(p):
    def pairs(items):
        d={}
        for k,v in items:need(k not in d,'duplicate JSON key');d[k]=v
        return d
    def bad(v):raise ValueError('nonfinite JSON '+v)
    return json.loads(Path(p).read_text(encoding='utf-8-sig'),object_pairs_hook=pairs,parse_constant=bad)
def checked(r):
    need(set(r)=={'path','identity'} and Path(r['path']).is_absolute(),'invalid evidence ref')
    need(fid(r['path'])==r['identity'],'changed evidence: '+r['path']);return Path(r['path'])
def load(r):return read(checked(r))
def write(p,v):
    p.parent.mkdir(parents=True,exist_ok=True)
    with p.open('x',encoding='utf-8') as stream:json.dump(v,stream,ensure_ascii=False,sort_keys=True,indent=2,allow_nan=False);stream.write('\n')
def members(v):return {n:dict(bytes=int(x['bytes']),sha256=x.get('sha256',x.get('candidate_sha256'))) for n,x in v.items()}

def facts():
    need(fid(F)['sha256']=='6af220ba48246bacf65e58b5c776aaa59be691a4fa37bda7cf55261d7b77cceb','actual M2 functional closure changed')
    need(fid(CORE)['sha256']=='1d24709eaaa8384e6ee672575e1758dbf7f65382824b64d3547302239acc068c','M2 core source proof changed')
    functional=read(F);core=read(CORE);identity=functional['candidate_identity'];ident=load(identity)
    source=core['source154'];production=core['production14']
    need(functional['status']=='passed' and functional['candidate']==core['candidate']==ident['candidate']==CANDIDATE,'wrong M2 actual candidate/status')
    need(identity==core['candidate_identity'] and len(source)==154 and len(production)==14 and members(ident['members'])==source,'M2 exact identity154 differs')
    need(functional['production_files']==production and functional['source154_each_full_rechecked'] is True,'M2 full closure does not bind own source154/14')
    source_root=ROOT/'build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1/source'
    for n,v in source.items():need(fid(source_root/n)==v,'actual M2 source member changed: '+n)
    need({n:source[n] for n in production}==production,'production14 not a subset of candidate154')
    full=load(functional['full']);need(full['status']=='passed' and full['candidate']==CANDIDATE and full['normal_count']==full['sanitized_count']==73 and full['production_files']==production,'M2 own full73+73 required')
    assertions={};evidence={}
    for mode,name in (('normal','full_normal'),('sanitized','full_san')):
        closure=functional['full_closure'][mode];need(set(closure)=={'receipt','request','junit','inventory'},'four full closure refs required')
        for r in closure.values():checked(r)
        receipt=load(closure['receipt']);request=load(closure['request'])
        need(receipt['status']=='passed' and receipt['mode']==mode and receipt['production_files']==request['production_files']==production,'full receipt own mode/production mismatch')
        actual={n:v['identity'] for n,v in receipt['input_files'].items()}
        need(actual==source and len(receipt['junit_names'])==73 and set(receipt['junit_names'])==set(request['tests']) and len(set(receipt['junit_names']))==73,'own full exact154/73 test inventory differs')
        need(receipt['junit_identity']==closure['junit']['identity'] and receipt['inventory_identity']==closure['inventory']['identity'],'full raw junit/inventory identity differs')
        assertions[name]=dict(ctest_passed=73,source154_bound=True);evidence[name]=[functional['full'],*closure.values()]
    targeted=load(functional['targeted']);worker=load(targeted['worker_receipt'])
    need(targeted['status']==worker['status']=='passed' and targeted['actual_compiles']==targeted['actual_native_executions']==worker['actual_compiles']==worker['actual_native_executions']==8,'targeted eight actual actions required')
    need(targeted['production_files']==worker['production_files']==production and members(worker['source154_before'])==members(worker['source154_after'])==source,'targeted own154/14 mismatch')
    native_request=D/'core/REQUESTS_M2_NATIVE.json';nr=read(native_request);native_receipt=Path(nr['output'])/'receipt.json'
    need(fid(native_receipt)==worker['native_receipt_identity'],'targeted native receipt changed');native=read(native_receipt)
    need(native['status']=='passed' and native['candidate']==CANDIDATE,'targeted native candidate/status mismatch')
    rows=native['commands'];commands=nr['commands'];need(len(rows)==len(commands)==16,'targeted exact4+4 command inventory differs')
    for row,command in zip(rows,commands):need(row['label']==command['label'] and row['argv']==command['argv'] and row['kind']==command['kind'] and row['returncode']==0,'targeted original command/result differs')
    runs=[v['label'] for v in commands if v['kind']=='native']
    need(sum(v.startswith('normal-') for v in runs)==sum(v.startswith('san-') for v in runs)==4,'targeted normal4/SAN4 required')
    assertions['targeted']=dict(normal=4,sanitized=4);evidence['targeted']=[functional['targeted'],targeted['worker_receipt'],ref(native_request),ref(native_receipt)]
    business=load(functional['business']);compat=load(functional['V1_bidirectional'])
    need(business['status']=='passed' and business['production_files']==production and business['native_business_children_each']==80 and set(business['functionals'])=={'normal','sanitized'},'own business80+80 required')
    need(compat['status']=='passed' and compat['main_V1_cases']==84 and compat['extra_cases']==8 and compat['AB_native_actions']==compat['C_native_actions']==4,'actual V1 84+8/AB4/C4 required')
    assertions['business_SML']=dict(normal_children=80,sanitized_children=80);evidence['business_SML']=[functional['business']]
    assertions['v1_bidirectional']=dict(main_cases=84,extra_cases=8,AB_actions=4,C_actions=4);evidence['v1_bidirectional']=[functional['V1_bidirectional'],functional['business']]
    consumer=load(functional['consumers']);runtime=load(functional['shared_runtimes']);life=load(functional['lifecycle52'])
    need(consumer['status']==runtime['status']==life['status']=='passed','own consumer/runtime/lifecycle status differs')
    own=consumer['source_identity'];need(runtime['source_identity']==life['candidate_identity']==own,'consumer/runtime/lifecycle candidate source identities differ')
    business_identity=load(own);need(business_identity['production_files']==production and business_identity['core_identity']==identity['identity'] and business_identity['core_member_count']==154,'business exact14/identity154 differs')
    need(life['actual_native_children']==52 and life['normal_model_cases']==life['sanitized_model_cases']==25 and life['pool_cases']==2 and len(life['cases'])==52 and life['no_retries'] is True,'actual lifecycle52 inventory differs')
    assertions['consumers']=dict(install_subproject_passed=True,shared_runtime_passed=True);evidence['consumers']=[functional['consumers'],functional['shared_runtimes'],own]
    assertions['lifecycle']=dict(actual_native_children=52,normal_model_cases=25,sanitized_model_cases=25,pool_cases=2);evidence['lifecycle']=[functional['lifecycle52'],own]
    diagnostic_path=D/'validation/diag/M2_MECHANISM_RESULT.json';diagnostic=read(diagnostic_path);dynamic=load(diagnostic['mechanism']);result=dynamic['result']
    need(diagnostic['status']=='passed' and diagnostic['actual_compiles']==diagnostic['actual_diagnostic_native']==1 and diagnostic['new_control_compiles']==diagnostic['new_control_runs']==diagnostic['retries']==0 and diagnostic['cleanup']==dict(live=[],process_groups=[],zombies=[]) and diagnostic['actual_outer_seconds']<=300,'actual one-M2 diagnostic inventory/cleanup differs')
    for key in ('freeze','admission','audit','compile','observer','internal_result','queue_request','queue_result','queue_manifest'):checked(diagnostic[key])
    frozen=load(diagnostic['freeze']);need(diagnostic['source_runtime_input_files']==6492 and diagnostic['source_runtime_before_after_unchanged'] is True and frozen['control']==diagnostic['control'],'actual diagnostic source/common-control bridge differs')
    for r in diagnostic['control'].values():
        if isinstance(r,dict) and set(r)=={'path','identity'}:checked(r)
    need(diagnostic['control']['audit']['identity']['sha256']=='7b834986bd69ea3c8ca0eb0fe0e6940da3bf44f52abc8d69922f5e2a0b9af503','M2 control must be original admitted C14 audit')
    need(result['status']=='mechanism_hit' and result['candidate']=='M2' and result['diagnostic_only'] is True and result['speed_inferred'] is False,'actual M2 mechanism hit required')
    need(result['external_audit_sha256']==dict(control=diagnostic['control']['audit']['identity']['sha256'],candidate=diagnostic['audit']['identity']['sha256']),'actual mechanism audit pins differ')
    need(load(dynamic['actual_checker'])==result and dynamic['new_checker_runs']==dynamic['new_diagnostic_native']==0,'preserved actual checker output differs')
    checked(dynamic['checker']);mutate=[v for v in result['scopes'] if v['scope']=='mutate']
    need(len(mutate)==1 and mutate[0]['nonempty_misses']>0 and mutate[0]['data_reads_removed']>0 and mutate[0]['canonical_calls_removed']>0,'positive M2 primary mutate mechanism not observed')
    assertions['mechanism']=dict(actual_nonempty_path_observed=True,same_predeclared_C14_control=True,new_control_runs=0,diagnostic_candidate_runs=1)
    evidence['mechanism']=[ref(diagnostic_path),diagnostic['mechanism'],diagnostic['audit'],diagnostic['control']['audit'],dynamic['actual_checker'],dynamic['checker']]
    common=[ref(F),ref(CORE),identity]
    return dict(identity=identity,source=source,production=production,common=common,assertions=assertions,evidence=evidence,core=core,
        mechanism_review_pins=dict(sealed_execution_result=fid(diagnostic_path),candidate_audit=diagnostic['audit']['identity'],shared_control_audit=diagnostic['control']['audit']['identity'],actual_checker_output=dynamic['actual_checker']['identity'],sealed_dynamic_result=diagnostic['mechanism']['identity'],checker_source=dynamic['checker']['identity'],core_screen_gate_binding=fid(CORE)))

def reviews(path,f,names):
    mapping=read(path)
    need(mapping['kind']=='M2-screen-independent-gate-review-map' and mapping['candidate']==CANDIDATE and mapping['candidate_identity']==f['identity'] and mapping['source154']==f['source'] and mapping['production14']==f['production'],'review map exact M2 identity154/14 required')
    need(set(mapping['gates'])==set(names),'review map exact gate inventory required')
    envelopes={}
    for name in names:
        item=mapping['gates'][name];envelope=item['independent_review']
        need(set(envelope)=={'status','record'} and envelope['status']=='passed','real independent review envelope required: '+name)
        review_path=checked(envelope['record']);need(review_path.resolve()!=Path(path).resolve(),'map cannot review itself')
        actual=read(review_path);need(actual['status'] in ('pass','passed') and not actual.get('mismatches',[]),'base independent review did not pass: '+name)
        if name in THREE:
            typed=load(item['typed_gate'])
            need(item['assertions']==PARENT_ASSERTIONS[name] and typed['kind']=='v2-screen-gate-receipt' and typed['gate']==name and typed['status']=='passed' and typed['candidate_identity']==f['identity'] and typed['source154']==f['source'] and typed['production14']==f['production'] and typed['assertions']==PARENT_ASSERTIONS[name] and typed['independent_review']==envelope,'parent actual typed gate not accepted for exact M2: '+name)
            for r in typed['evidence']:checked(r)
            need(actual['candidate_identity']==f['identity'],'parent independent typed review candidate differs')
        elif name=='mechanism':
            need(review_path.resolve()==(D/'reviews/M2_MECHANISM_EXECUTION_ACCEPTANCE.json').resolve() and fid(review_path)['sha256']=='4d8deaf53cfbaf22c244a7ec3f1d738b1191e5b34dfa96f81f46166bc4866e70','actual root M2 mechanism bridge required')
            need(actual['candidate']==CANDIDATE and actual['mechanism_hit'] is True and actual['logical_work_and_call_conservation_passed'] is True and actual['actual_candidate_diagnostic_native']==1 and actual['new_control_compiles']==actual['new_control_runs']==0,'actual root mechanism bridge scope differs')
            need(checked(actual['mechanism_result'])==D/'validation/diag/M2_MECHANISM_RESULT.json' and checked(actual['dynamic_result'])==D/'validation/diag/M2_DYNAMIC_RESULT.json','root mechanism bridge result differs')
            underlying=load(actual['independent_underlying_review'])
            need(underlying['status']=='passed' and underlying['candidate']==CANDIDATE and underlying['mechanism_outcome']=='mechanism_hit' and all(underlying['pins'][k]==v for k,v in f['mechanism_review_pins'].items()),'underlying independent M2 mechanism pins/status differ')
        else:
            need(review_path.resolve()==(D/'reviews/M2_CAPTURE_FUNCTIONAL_ACCEPTANCE.json').resolve() and fid(review_path)['sha256']=='dc43c3e27c650d241d1dbb4ef002635fe99280277a9866431eb07d2d9eec5dca','actual root M2 functional bridge required')
            need(actual['candidate']==CANDIDATE and actual['candidate_identity']==f['identity'] and checked(actual['functional_gates'])==F and actual['source154_each_full_bound'] is True,'root functional bridge candidate/source differs')
            underlying=load(actual['independent_underlying_review'])
            need(underlying['status']=='passed' and underlying['M2_full_functional_gates_passed'] is True and underlying['candidate_identity']==f['identity'] and underlying['full_functional_gates']['identity']==fid(F) and underlying['all_evidence_M2_specific'] is True and underlying['R1_or_M1_status_substitution'] is False,'actual independent own M2 full review required')
            counts=dict(targeted=8,normal_library=73,sanitized_library=73,business_children_normal=80,business_children_sanitized=80,V1_main=84,V1_extras=8,AB_native_actions=4,C_native_actions=4,lifecycle_normal_models=25,lifecycle_sanitized_models=25,lifecycle_pool_probes=2,lifecycle_native_children=52)
            need(all(underlying['counts'][k]==v for k,v in counts.items()),'independent actual M2 full counts differ')
        envelopes[name]=envelope
    return envelopes

def prepare(review_map):
    f=facts();accepted=reviews(review_map,f,EIGHT);reports={}
    for name in EIGHT+THREE:
        evidence=f['evidence'].get(name,[ref(CORE)])
        for r in evidence:checked(r)
        record=dict(kind='v2-screen-gate-receipt',gate=name,status='passed' if name in EIGHT else 'pending_parent_independent_acceptance',candidate_identity=f['identity'],source154=f['source'],production14=f['production'],
            assertions=f['assertions'][name] if name in EIGHT else {k:None for k in PARENT_ASSERTIONS[name]},evidence=evidence,source_binding_evidence=f['common'],
            independent_review=accepted[name] if name in EIGHT else dict(status='pending',record=None),performance_admitted=False,screen_admitted=False,
            source_scope='Exact uninstrumented M2 candidate154/14; mechanism instrumentation is separately admitted and bridged, never claimed byte-identical to bare154.')
        path=S/'gate-receipts-v1'/(name+'.json');write(path,record);reports[name]=ref(path)
    write(S/'GATES.json',dict(kind='v2-screen-complete-gates-admission',status='pending_parent_codegen_safety_engineering_acceptance',candidate_identity=f['identity'],gates=reports,passed_gates=list(EIGHT),pending_gates=list(THREE),review_map=ref(review_map),no_freeze_admission=True))
    write(S/'PARENT_GATE_REVIEW_REQUEST.json',dict(kind='M2-screen-parent-three-gate-review-request',status='pending_parent_independent_review',candidate=CANDIDATE,candidate_identity=f['identity'],source154=f['source'],production14=f['production'],pending_gates=list(THREE),required_assertions=PARENT_ASSERTIONS,typed_gates=reports,core_proof=ref(CORE),
        limitations=['Core proof is finite partial source/codegen/safety evidence, not final gate acceptance.','No R1 cold-body equality or historical53-field template applies to M2.','Engineering must use M2 actual uninstrumented profile/cost/resource facts before screen outcomes; diagnostic timings do not establish speed.'],screen_admitted=False,performance_admitted=False))
    return dict(status='8_actual_reviewed_3_pending',gates=ref(S/'GATES.json'),compile=0,native=0,checker=0,queue=0,freeze=0)

def complete(review_map):
    f=facts();accepted=reviews(review_map,f,THREE);old=read(S/'GATES.json')
    need(old['candidate_identity']==f['identity'] and old['passed_gates']==list(EIGHT) and old['pending_gates']==list(THREE),'original eight/three gate scope differs')
    records=dict(old['gates'])
    for name in EIGHT:
        v=load(records[name]);need(v['status']=='passed' and v['independent_review']['status']=='passed','original reviewed eight gate changed');checked(v['independent_review']['record'])
    for name in THREE:
        original=load(records[name]);need(original['status']=='pending_parent_independent_acceptance','original pending receipt required')
        original.update(status='passed',assertions=PARENT_ASSERTIONS[name],independent_review=accepted[name]);original['evidence']+=[ref(S/'PARENT_GATE_REVIEW_REQUEST.json'),accepted[name]['record']]
        path=S/'gate-receipts-v2'/(name+'.json');write(path,original);records[name]=ref(path)
    write(S/'GATES_v2.json',dict(kind='v2-screen-complete-gates-admission',status='passed',candidate_identity=f['identity'],gates=records,previous_pending=ref(S/'GATES.json'),parent_review_map=ref(review_map),freeze_not_executed=True,screen_admitted=False,performance_admitted=False))
    return dict(status='all11_reviewed_not_frozen',gates=ref(S/'GATES_v2.json'),compile=0,native=0,checker=0,queue=0,freeze=0)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('prepare','complete'));p.add_argument('--review-map',type=Path,required=True);a=p.parse_args()
    print(json.dumps(prepare(a.review_map) if a.action=='prepare' else complete(a.review_map),sort_keys=True))
if __name__=='__main__':main()
