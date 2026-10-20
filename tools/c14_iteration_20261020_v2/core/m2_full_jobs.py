"""M2-only full CMake/CTest via the unchanged retained execution implementation."""
import argparse
import datetime
import importlib.util
from pathlib import Path
import shutil
import sys

R=Path(__file__).resolve().parents[3]
H=Path(__file__).resolve().parent/'m2_full'
s=importlib.util.spec_from_file_location('m2_retained_full_entry',H/'full_entry.py')
J=importlib.util.module_from_spec(s);s.loader.exec_module(J)
DOC=R/'docs/coordination/c14-iteration-20261020-v2/validation/m2_full'
Q=R/'docs/coordination/c14-iteration-20261020-v2/core/REQUESTS_M2_FULL.json'
IDENT=R/'build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1/IDENTITY.json'
WRAPPER=Path('/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/bin/clang++-libcxx')
WRAPPER_ID={'bytes':61,'sha256':'c8ea06a7c58557e3b89d09873dc88457d7b0c5d9af3f08cf0449093f2fbd1b87'}
def ref(p):return {'path':str(p),'identity':J.fid(p)}

def prepare():
    q=J.read(Q)
    J.require(q['status']=='ready_for_validation_review' and not q['timing_admitted'],'finite M2 functional request required')
    for p,v in q['tool_files'].items():J.require(J.fid(p)==v,'M2 tool/launcher changed: '+p)
    J.require(J.fid(WRAPPER)==q['compiler_launcher']['identity']==WRAPPER_ID,'explicit outer compiler launcher differs')
    J.require(J.fid(IDENT)==q['candidate_identity'],'M2 frozen identity changed')
    identity=J.read(IDENT);source=IDENT.parent/'source';actual={}
    for n,v in identity['members'].items():
        actual[n]=J.fid(source/n)
        J.require(actual[n]=={'bytes':v['bytes'],'sha256':v['candidate_sha256']},'M2 member changed: '+n)
    material=J.read(R/'handoff/github-20261019/C14_MATERIALIZATION.json')
    prod={n:actual[n] for n in material['production_files']}
    review=R/'docs/coordination/c14-iteration-20261020-v2/validation/review'
    native_path=review/'M2_NATIVE_RESULT.json';native=J.read(native_path)
    native_worker=review/'M2_NATIVE_WORKER_RECEIPT.json';nw=J.read(native_worker)
    J.require(native['status']=='passed' and native['actual_native_executions']==8 and native['production_files']==prod,'actual M2 targeted8 required')
    J.require(nw['status']=='passed' and nw['source154_before']==nw['source154_after']==actual,'same actual M2 targeted source154 required')
    cgpath=review/'M2_CODEGEN_RESULT.json';cg=J.read(cgpath)
    cga=review/'M2_CODEGEN_ADMISSION.json';mechanism_review=review/'M2_CODEGEN_MECHANISM_REVIEW.json'
    J.require(cg['status']=='passed' and J.read(cga)['candidate_members']==identity['members'] and J.read(mechanism_review)['status']=='passed','same-source M2 static codegen required')
    DOC.mkdir(parents=True,exist_ok=True)
    s0=R/'docs/coordination/c14-iteration-20261020-v2/validation/S0_RESULT.json'
    J.require(J.read(s0)['status']=='passed','current A/S0 required')
    copied=DOC/'S0_RESULT.json'
    if copied.exists():J.require(J.fid(copied)==J.fid(s0),'M2 copied A binding changed')
    else:shutil.copyfile(s0,copied)
    binding={'status':'passed','scope':'M2 source/input bridge to actual static codegen and targeted8 only; not dynamic-hit or performance evidence',
        'candidate_identity_file':J.fid(IDENT),'candidate_members':identity['members'],'production_files':{'candidate':prod},
        'request_identity':J.fid(Q),'codegen':ref(cgpath),'targeted':ref(native_path),'targeted_worker':ref(native_worker),
        'static_review':ref(mechanism_review),'current_A':ref(s0),'compiler_launcher':ref(WRAPPER)}
    bridge=DOC/'M2_FULL_SOURCE_BINDING.json'
    if bridge.exists():J.require(J.read(bridge)==binding,'M2 source bridge changed')
    else:J.write(bridge,binding)
    return identity,prod,material,source

def main():
    p=argparse.ArgumentParser();p.add_argument('mode',choices=('normal','sanitized'));args=p.parse_args()
    identity,prod,material,source=prepare()
    inv=J.read(Path('/root/xoffset-receive-completion/20261019/v1/full-final-v4-normal-build/test-inventory.json'))
    tests=inv['runtime_test_programs']+inv['subprocess_scenarios']+inv['tooling_checks']
    q={'schema_version':1,'stage':'S2_targeted','candidate':'M2','job_id':'m2-full-v1-'+args.mode,'mode':args.mode,
       'baseline_main_sha256':material['production_files']['xoffsetdatastructure.hpp']['sha256'],'production_files':prod,
       'overlays':{n:{'path':str(source/n),'identity':v} for n,v in prod.items() if v!=material['production_files'][n]},
       'targets':inv['runtime_test_programs'],'tests':tests,'runner_identity':J.fid(J.RUNNER),'entry_identity':J.fid(H/'full_entry.py'),
       'tool_files':{str(p):J.fid(p) for p in J.FIXED_TOOLS},'compiler_launcher':ref(WRAPPER),
       'backing_free_bytes':shutil.disk_usage('/mnt/c').free,'backing_observed_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
    J.write(DOC/('M2_FULL_'+args.mode.upper()+'_REQUEST_v1.json'),q)
    sys.argv=[str(H/'full_entry.py'),'run','--request',str(DOC/('M2_FULL_'+args.mode.upper()+'_REQUEST_v1.json'))]
    try:J.main()
    except SystemExit as e:
        if e.code not in (None,0):raise
    J.require(J.fid(WRAPPER)==WRAPPER_ID,'outer compiler launcher changed after full gate')
    receipts=[J.OUTPUT/('m2-full-v1-'+m)/'receipt.json' for m in ('normal','sanitized')]
    if all(p.exists() for p in receipts):
        for p in receipts:
            v=J.read(p);J.require(v['status']=='passed' and sorted(v['junit_names'])==sorted(tests) and v['production_files']==prod,'actual M2 full inventory failed')
        J.write(DOC/'M2_FULL_RESULT.json',{'status':'passed','candidate':identity['candidate'],'request_identity':J.fid(Q),
            'normal_count':len(tests),'sanitized_count':len(tests),'production_files':prod,
            'normal_receipt':ref(receipts[0]),'sanitized_receipt':ref(receipts[1]),'compiler_launcher':ref(WRAPPER),'performance_samples':0})
if __name__=='__main__':main()
