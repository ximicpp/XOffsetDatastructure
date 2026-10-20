"""Bind actual M2 functional/source receipts, including final closure rechecks."""
import hashlib
import json
from pathlib import Path
R=Path(__file__).resolve().parents[3]
D=R/'docs/coordination/c14-iteration-20261020-v2/validation/m2_full'
V=Path('/root/xoffset-c14-iteration/20261020/v2/validation')
B=V/'m2-business-v1';M=V/'m2-business-matrix-v1/production-C-matrix-v2'
Q=R/'docs/coordination/c14-iteration-20261020-v2/core/REQUESTS_M2_FULL.json'
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
    for p,v in q['tool_files'].items():need(fid(p)==v,'M2 final tool/launcher changed: '+p)
    exact_ref(q['compiler_launcher'])
    identity=R/'build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1/IDENTITY.json'
    need(fid(identity)==q['candidate_identity'],'M2 identity changed')
    full=passed(D/'M2_FULL_RESULT.json')
    need(full['request_identity']==fid(Q) and full['production_files']==prod and full['normal_count']==full['sanitized_count']==73,'M2 full closure differs')
    checked={}
    for mode,key in (('normal','normal_receipt'),('sanitized','sanitized_receipt')):
        path=exact_ref(full[key]);receipt=passed(path)
        req=D/('M2_FULL_'+mode.upper()+'_REQUEST_v1.json')
        need(receipt['request']['path']==str(req) and {k:receipt['request'][k] for k in ('bytes','sha256')}==fid(req),'M2 actual full child request changed')
        invpath=path.parent/'build/test-inventory.json';inv=read(invpath)
        expected=inv['runtime_test_programs']+inv['subprocess_scenarios']+inv['tooling_checks']
        need(inv['mode']=='full' and len(expected)==73 and sorted(receipt['junit_names'])==sorted(expected),'M2 actual full inventory/JUnit differs')
        need(receipt['inventory_identity']==fid(invpath) and receipt['junit_identity']==fid(path.parent/'ctest.xml') and
             receipt['compile_commands_identity']==fid(path.parent/'build/compile_commands.json'),'M2 full files changed')
        for name,value in q['source_members'].items():need(fid(path.parent/'source'/name)=={'bytes':value['bytes'],'sha256':value['candidate_sha256']},'M2 full source154 changed: '+mode+'/'+name)
        need(receipt['production_files']==prod,'M2 full child production differs')
        checked[mode]={'receipt':ref(path),'request':ref(req),'junit':ref(path.parent/'ctest.xml'),'inventory':ref(invpath)}
    bridge=read(D/'M2_FULL_SOURCE_BINDING.json')
    need(bridge['request_identity']==fid(Q) and bridge['candidate_members']==q['source_members'] and bridge['production_files']['candidate']==prod,'M2 source bridge changed')
    for name in ('codegen','targeted','targeted_worker','static_review','current_A','compiler_launcher'):exact_ref(bridge[name])
    prepared=read(D/'business/PREPARED.json')
    need(prepared['status']=='prepared' and prepared['production_files']==prod and prepared['requests']==fid(Q),'M2 prepared source/request differs')
    for p,v in prepared['bindings'].items():need(fid(p)==v,'M2 prepared input changed: '+p)
    for mode,value in prepared['full_receipts'].items():need(exact_ref(value)==Path(full['normal_receipt' if mode=='normal' else 'sanitized_receipt']['path']),'M2 business full ref differs')
    targeted=passed(D.parent/'review/M2_NATIVE_RESULT.json');need(targeted['actual_native_executions']==8,'actual M2 targeted8 incomplete')
    business=passed(B/'final-business-functional-v2/result.json');compat=passed(B/'business-compatibility-driver-v2/result.json')
    passed(B/'package-standard-v2/result.json');passed(B/'package-runtime-v2/result.json');matrix=passed(M/'result.json')
    need(business['native_business_children_each']==80 and business['main_V1_cases']==84 and business['extra_cases']==8,'M2 SML/V1 inventory differs')
    need(compat['main_V1_cases']==84 and compat['extra_cases']==8 and compat['AB_native_actions']==compat['C_native_actions']==4,'M2 V1 bidirectional inventory differs')
    need(matrix['normal_model_cases']==25 and matrix['sanitized_model_cases']==25 and matrix['pool_cases']==2 and matrix['actual_native_children']==52,'M2 lifecycle52 incomplete')
    need(not matrix['all_custom_support_proven_by_business_matrix'] and not matrix['standard_lifetime_proof_replaced_by_SAN'],'M2 matrix scope overclaim')
    for source in (V/'m2-full-v1-normal/source',V/'m2-full-v1-sanitized/source',Path(matrix['source'])):
        need({n:fid(source/n) for n in prod}==prod,'M2 source14 changed: '+str(source))
    result={'status':'passed','candidate':'m2-exact-owns-v1','scope':'Actual M2 functional/source gates only; not dynamic-hit or performance evidence',
        'request':ref(Q),'candidate_identity':ref(identity),'production_files':prod,'full':ref(D/'M2_FULL_RESULT.json'),'full_closure':checked,
        'targeted':ref(D.parent/'review/M2_NATIVE_RESULT.json'),'business':ref(B/'final-business-functional-v2/result.json'),
        'V1_bidirectional':ref(B/'business-compatibility-driver-v2/result.json'),'consumers':ref(B/'package-standard-v2/result.json'),
        'shared_runtimes':ref(B/'package-runtime-v2/result.json'),'lifecycle52':ref(M/'result.json'),
        'source_review':ref(D.parent.parent/'core/REVIEW_M2.md'),'prepared':ref(D/'business/PREPARED.json'),
        'tool_files':q['tool_files'],'compiler_launcher':q['compiler_launcher'],'source154_each_full_rechecked':True,
        'dynamic_mechanism':'Still separate: predeclared exact common C14 control plus one M2 candidate observation; static4-to-1 is not a dynamic hit or performance result.',
        'source_safety':'One current-node data capture, original capacity containment, miss-only size and null-safe end, same recursive expression/order and bidirectional caller checks. No owns predicate cache or new pointer lifetime boundary.',
        'historical_text_residue':'Inherited prepare description of M1 r2/preflight remains historical explanatory text only; actual M2 references above are authoritative.',
        'performance_accepted':False,'screen_admitted':False,'QF_admitted':False}
    with (D/'M2_FUNCTIONAL_GATES.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2);f.write('\n')
    print('M2 full73/73 + SML80/80 + V1/consumers/lifecycle52 bound; dynamic mechanism and performance separate')
if __name__=='__main__':main()
