"""Bind only actual R1 functional receipts. No screen/measurement admission."""
import hashlib
import json
from pathlib import Path
R=Path(__file__).resolve().parents[3]
D=R/'docs/coordination/c14-iteration-20261020-v2/validation/r1_full'
V=Path('/root/xoffset-c14-iteration/20261020/v2/validation')
B=V/'r1-business-v1';M=V/'r1-business-matrix-v1/production-C-matrix-v2'
def read(p):return json.loads(Path(p).read_text(encoding='utf-8'))
def fid(p):
    p=Path(p);b=p.read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
def ref(p):return {'path':str(p),'identity':fid(p)}
def need(ok,msg):
    if not ok:raise ValueError(msg)
def passed(p):
    v=read(p);need(v['status']=='passed','missing actual pass: '+str(p));return v
def main():
    full=passed(D/'R1_FULL_RESULT.json');prod=full['production_files']
    need(full['normal_count']==full['sanitized_count']==73,'R1 original full inventory incomplete')
    targeted=passed(D.parent/'R1_NATIVE_RESULT.json');need(targeted['actual_native_executions']==10,'actual B10 incomplete')
    business=passed(B/'final-business-functional-v2/result.json')
    compat=passed(B/'business-compatibility-driver-v2/result.json')
    passed(B/'package-standard-v2/result.json');passed(B/'package-runtime-v2/result.json')
    matrix=passed(M/'result.json')
    need(business['native_business_children_each']==80 and business['main_V1_cases']==84 and business['extra_cases']==8,'R1 SML/V1 actual inventory changed')
    need(compat['main_V1_cases']==84 and compat['extra_cases']==8 and compat['AB_native_actions']==compat['C_native_actions']==4,'R1 bidirectional V1 inventory changed')
    need(matrix['normal_model_cases']==25 and matrix['sanitized_model_cases']==25 and matrix['pool_cases']==2 and matrix['actual_native_children']==52,'actual R1 lifecycle52 incomplete')
    need(not matrix['all_custom_support_proven_by_business_matrix'] and not matrix['standard_lifetime_proof_replaced_by_SAN'],'matrix scope overclaim')
    for source in (V/'r1-full-v1-normal/source',V/'r1-full-v1-sanitized/source',Path(matrix['source'])):
        need({n:fid(source/n) for n in prod}==prod,'R1 source14 changed: '+str(source))
    result={'status':'passed','candidate':'r1-canonical-hotcold-v1','scope':'Complete R1 functional/source gates only; no performance retention or screening/qualification admission',
        'production_files':prod,'full':ref(D/'R1_FULL_RESULT.json'),'targeted':ref(D.parent/'R1_NATIVE_RESULT.json'),
        'business':ref(B/'final-business-functional-v2/result.json'),'V1_bidirectional':ref(B/'business-compatibility-driver-v2/result.json'),
        'consumers':ref(B/'package-standard-v2/result.json'),'shared_runtimes':ref(B/'package-runtime-v2/result.json'),
        'lifecycle52':ref(M/'result.json'),
        'source_review':ref(R/'docs/coordination/c14-iteration-20261019-v1/core/REVIEW_R1.md'),
        'native_review':ref(D.parent.parent/'core/REVIEW_R1_NATIVE.md'),
        'source_safety_scope':'Canonical wrapper retains acquire per call; cold body and record/TLS/anchor/retirement ordering unchanged. Actual callback/OOM/lifecycle evidence and source proof are complementary; SAN does not prove all ISO rules.',
        'control_reuse':'Retained same-profile control and historical V1 references keep their exact identities. Candidate R1 functional results are new and do not borrow M1 pass status.',
        'performance_accepted':False,'screen_admitted':False,'QF_admitted':False}
    with (D/'R1_FUNCTIONAL_GATES.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2);f.write('\n')
    print('R1 functional full73/73 + SML80/80 + V1/consumers/lifecycle52 bound; no performance admission')
if __name__=='__main__':main()
