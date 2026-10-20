"""Bind actual completed M1 correctness gates before the unique screen freeze."""
import importlib.util
from pathlib import Path

H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('cg',H/'m1_codegen_job.py')
C=importlib.util.module_from_spec(s);s.loader.exec_module(C);J=C.J
DOC=J.DOC;B=J.OUTPUT/'m1-business-v2';M=J.OUTPUT/'m1-business-matrix-v2/production-C-matrix-v2'

def ref(p):return {'path':str(p),'identity':J.fid(p)}
def passed(p):
    d=J.read(p);J.require(d['status']=='passed','actual gate did not pass: '+str(p));return d

cg=passed(DOC/'M1_CODEGEN_RESULT.json');prod=cg['production_files']['candidate']
for p in (DOC/'M1_MECHANISM_RESULT.json',DOC/'M1_CORRECTNESS_RESULT.json',DOC/'M1_FULL_RESULT.json',DOC/'M1_IDENTITY_EQUIVALENCE.json'):passed(p)
business=passed(B/'final-business-functional-v2/result.json')
compat=passed(B/'business-compatibility-driver-v2/result.json')
package=passed(B/'package-standard-v2/result.json');runtime=passed(B/'package-runtime-v2/result.json');matrix=passed(M/'result.json')
J.require(business['native_business_children_each']==80 and business['main_V1_cases']==84 and business['extra_cases']==8,'business original inventory changed')
J.require(compat['main_V1_cases']==84 and compat['extra_cases']==8 and compat['AB_native_actions']==compat['C_native_actions']==4,'V1 inventory changed')
J.require(matrix['normal_model_cases']==25 and matrix['sanitized_model_cases']==25 and matrix['pool_cases']==2 and matrix['actual_native_children']==52,'actual52 matrix inventory incomplete')
J.require(matrix['all_custom_support_proven_by_business_matrix'] is False and matrix['standard_lifetime_proof_replaced_by_SAN'] is False,'matrix scope overclaim')
for source in (J.OUTPUT/'m1-full-v2-normal/source',J.OUTPUT/'m1-full-v2-sanitized/source',Path(matrix['source'])):
    J.require({n:J.fid(source/n) for n in prod}==prod,'production14 changed: '+str(source))
consumers={'status':'passed','candidate':'m1-default-scalar-v2','production_files':prod,
    'package':ref(B/'package-standard-v2/result.json'),'runtime':ref(B/'package-runtime-v2/result.json'),
    'lifecycle_matrix52':ref(M/'result.json'),'scope':'Actual M1 installed/subproject/relocated prefix/archive C consumers, both shared runtimes and original52 lifecycle/pool models. No standalone source ZIP full-suite claim.'}
J.write(DOC/'M1_CONSUMER_GATE.json',consumers)
review=J.ROOT/'docs/coordination/c14-iteration-20261019-v1/core/REVIEW_M1.md'
J.require('no blocking production-source defect found' in review.read_text().lower(),'M1 source review not passed')
safety={'status':'passed','candidate':'m1-default-scalar-v2','production_files':prod,'source_review':ref(review),
    'identity_equivalence':ref(DOC/'M1_IDENTITY_EQUIVALENCE.json'),'targeted':ref(DOC/'M1_CORRECTNESS_RESULT.json'),
    'full':ref(DOC/'M1_FULL_RESULT.json'),'business':ref(B/'final-business-functional-v2/result.json'),'lifecycle52':ref(M/'result.json'),
    'scope':'Ordinary default scalar fresh suffix only; both custom frontier flags/future custom lifetime excluded. Typed value initialization, full array lifetime, transaction/prefix/self-alias/OOM/callback/owner contracts retain source proof and actual registered counterexamples. No claim that SAN proves every custom lifetime or ISO rule.',
    'cost_controls':'M1 source adds no cache/allocation/storage or cross-operation state. Same fresh entries/elements, no page preparation or work moved out of timed phases. Resource limits and auxiliary cost evidence remain independently enforced by screen.',
    'performance_accepted':False,'screen_attempts':0}
J.write(DOC/'M1_SAFETY_GATE.json',safety)
files={'codegen':DOC/'M1_CODEGEN_RESULT.json','mechanism':DOC/'M1_MECHANISM_RESULT.json','targeted':DOC/'M1_CORRECTNESS_RESULT.json',
    'full_normal':J.OUTPUT/'m1-full-v2-normal/receipt.json','full_san':J.OUTPUT/'m1-full-v2-sanitized/receipt.json',
    'business_SML':B/'final-business-functional-v2/result.json','v1_bidirectional':B/'business-compatibility-driver-v2/result.json',
    'consumers':DOC/'M1_CONSUMER_GATE.json','safety':DOC/'M1_SAFETY_GATE.json'}
gates={}
for name,p in files.items():
    value=J.fid(p);value['bytes']=str(value['bytes']);gates[name]={'path':str(p),'identity':value}
identity=J.ROOT/'docs/coordination/c14-iteration-20261019-v1/core/M1_IDENTITY.json'
J.write(DOC/'screen/GATES.json',{'status':'passed','candidate_identity_sha256':J.fid(identity)['sha256'],'gates':gates,
    'identity_serialization_equivalence':ref(DOC/'M1_IDENTITY_EQUIVALENCE.json'),'production14':prod,
    'root_scope':'All required M1 safety/functional gates completed; freeze/review/quiet/resource must still pass; no Q/F authorization.'})
print('M1 actual full73/73 + business80/80 + V1 + consumers +52 matrix gates bound; no screen run yet')
