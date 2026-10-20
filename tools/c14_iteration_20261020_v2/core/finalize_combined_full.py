"""Bind actual COMBINED functional/source receipts, including final closure rechecks."""
import hashlib
import json
from pathlib import Path
R=Path(__file__).resolve().parents[3]
D=R/'docs/coordination/c14-iteration-20261020-v2/validation/combined_full'
V=Path('/root/xoffset-c14-iteration/20261020/v2/validation')
B=V/'combined-business-v1';M=V/'combined-business-matrix-v1/production-C-matrix-v2'
Q=R/'docs/coordination/c14-iteration-20261020-v2/core/REQUESTS_COMBINED_FULL.json'
INITIAL=Q.with_name('REQUESTS_COMBINED_INITIAL.json')
import importlib.util
spec=importlib.util.spec_from_file_location('combined_initial_binding',Path(__file__).with_name('verify_combined_initial.py'));G=importlib.util.module_from_spec(spec);spec.loader.exec_module(G)
def read(p):return json.loads(Path(p).read_text(encoding='utf-8'))
def fid(p):
    p=Path(p);b=p.read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
def ref(p):return {'path':str(p),'identity':fid(p)}
def need(ok,msg):
    if not ok:raise ValueError(msg)
def exact_ref(v):
    p=Path(v['path']);need(fid(p)==v['identity'],'reference changed: '+str(p));return p
def passed(p):
    v=read(p);need(v['status']=='passed','missing actual pass: '+str(p));return v
def main():
    q=read(Q);prod=q['production14']
    for p,v in q['tool_files'].items():need(fid(p)==v,'COMBINED final tool/launcher changed: '+p)
    exact_ref(q['compiler_launcher'])
    identity=R/'build/c14-iteration-20261020-v2/candidates/r1-m2-combined-v1/IDENTITY.json'
    need(fid(identity)==q['candidate_identity'],'COMBINED identity changed')
    full=passed(D/'COMBINED_FULL_RESULT.json')
    need(full['request_identity']==fid(INITIAL) and full['production_files']==prod and full['normal_count']==full['sanitized_count']==73,'COMBINED full closure differs')
    checked={}
    for mode,key in (('normal','normal_receipt'),('sanitized','sanitized_receipt')):
        path=exact_ref(full[key]);receipt=passed(path)
        req=D/('COMBINED_FULL_'+mode.upper()+'_REQUEST_v1.json')
        need(receipt['request']['path']==str(req) and {k:receipt['request'][k] for k in ('bytes','sha256')}==fid(req),'COMBINED actual full child request changed')
        invpath=path.parent/'build/test-inventory.json';inv=read(invpath)
        expected=inv['runtime_test_programs']+inv['subprocess_scenarios']+inv['tooling_checks']
        need(inv['mode']=='full' and len(expected)==73 and sorted(receipt['junit_names'])==sorted(expected),'COMBINED actual full inventory/JUnit differs')
        need(receipt['inventory_identity']==fid(invpath) and receipt['junit_identity']==fid(path.parent/'ctest.xml') and
             receipt['compile_commands_identity']==fid(path.parent/'build/compile_commands.json'),'COMBINED full files changed')
        for name,value in q['source_members'].items():need(fid(path.parent/'source'/name)=={'bytes':value['bytes'],'sha256':value['candidate_sha256']},'COMBINED full source154 changed: '+mode+'/'+name)
        need(receipt['production_files']==prod,'COMBINED full child production differs')
        checked[mode]={'receipt':ref(path),'request':ref(req),'junit':ref(path.parent/'ctest.xml'),'inventory':ref(invpath)}
    bridge=read(D/'COMBINED_FULL_SOURCE_BINDING.json')
    need(bridge['request_identity']==fid(Q) and bridge['candidate_members']==q['source_members'] and bridge['production_files']['candidate']==prod,'COMBINED source bridge changed')
    for name in ('initial_gates','source_acceptance','selection','current_A','compiler_launcher'):exact_ref(bridge[name])
    need(bridge['initial_request_identity']==fid(INITIAL) and read(exact_ref(bridge['initial_gates']))==G.verify(),'combined own initial/targeted closure changed')
    prepared=read(D/'business/PREPARED.json')
    need(prepared['status']=='prepared' and prepared['production_files']==prod and prepared['requests']==fid(Q),'COMBINED prepared source/request differs')
    for p,v in prepared['bindings'].items():need(fid(p)==v,'COMBINED prepared input changed: '+p)
    for mode,value in prepared['full_receipts'].items():need(exact_ref(value)==Path(full['normal_receipt' if mode=='normal' else 'sanitized_receipt']['path']),'COMBINED business full ref differs')
    targeted=passed(D/'COMBINED_INITIAL_GATES.json');need(targeted['external_programs']==4 and targeted['repository_programs_each_mode']==6 and targeted['unique_targeted_sources']==8,'combined targeted scope incomplete')
    business=passed(B/'final-business-functional-v2/result.json');compat=passed(B/'business-compatibility-driver-v2/result.json')
    passed(B/'package-standard-v2/result.json');passed(B/'package-runtime-v2/result.json');matrix=passed(M/'result.json')
    need(business['native_business_children_each']==80 and business['main_V1_cases']==84 and business['extra_cases']==8,'COMBINED SML/V1 inventory differs')
    need(compat['main_V1_cases']==84 and compat['extra_cases']==8 and compat['AB_native_actions']==compat['C_native_actions']==4,'COMBINED V1 bidirectional inventory differs')
    need(matrix['normal_model_cases']==25 and matrix['sanitized_model_cases']==25 and matrix['pool_cases']==2 and matrix['actual_native_children']==52,'COMBINED lifecycle52 incomplete')
    need(not matrix['all_custom_support_proven_by_business_matrix'] and not matrix['standard_lifetime_proof_replaced_by_SAN'],'COMBINED matrix scope overclaim')
    for source in (V/'combined-full-v1-normal/source',V/'combined-full-v1-sanitized/source',Path(matrix['source'])):
        need({n:fid(source/n) for n in prod}==prod,'COMBINED source14 changed: '+str(source))
    result={'status':'passed','candidate':'r1-m2-combined-v1','scope':'Actual own combined functional/source closure; no independent candidate substitution and no combination performance evidence',
        'request':ref(Q),'candidate_identity':ref(identity),'production_files':prod,'full':ref(D/'COMBINED_FULL_RESULT.json'),'full_closure':checked,
        'targeted':ref(D/'COMBINED_INITIAL_GATES.json'),'business':ref(B/'final-business-functional-v2/result.json'),
        'V1_bidirectional':ref(B/'business-compatibility-driver-v2/result.json'),'consumers':ref(B/'package-standard-v2/result.json'),
        'shared_runtimes':ref(B/'package-runtime-v2/result.json'),'lifecycle52':ref(M/'result.json'),
        'source_review':ref(D.parent.parent/'core/COMBINED_SOURCE_REVIEW.md'),'prepared':ref(D/'business/PREPARED.json'),
        'tool_files':q['tool_files'],'compiler_launcher':q['compiler_launcher'],'source154_each_full_rechecked':True,
        'dynamic_mechanism':'The two independently retained mechanisms are component evidence only; actual combination performance is unproven. No new diagnostic/control/72 run is authorized.',
        'source_safety':'Exact root-selected R1+M2 patch union; acquire each remaining call, byte-identical active body and current-node data/capacity/miss-only-size/null/ADL/bidirectional invariants. No new pointer lifetime or callback boundary.',
        'historical_text_residue':'Inherited prepare description of M1 r2/preflight remains historical explanatory text only; actual COMBINED references above are authoritative.',
        'performance_accepted':False,'screen_admitted':False,'QF_admitted':False}
    with (D/'COMBINED_FUNCTIONAL_GATES.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2);f.write('\n')
    print('COMBINED full73/73 + targeted six repo scopes each mode/external4 + SML80/80 + V1/consumers/lifecycle52 bound; no QF admission')
if __name__=='__main__':main()
