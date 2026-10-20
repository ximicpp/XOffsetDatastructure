"""Run one existing full normal/SAN CMake gate against frozen R1.

Invoke outside the lease: the retained entry dispatches the original supervisor.
No C diagnostic or performance prerequisite is introduced into functional gates.
"""
import argparse
import datetime
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys

R = Path(__file__).resolve().parents[3]
H = Path(__file__).resolve().parent / "r1_full"
spec = importlib.util.spec_from_file_location("r1_full_entry", H / "full_entry.py")
J = importlib.util.module_from_spec(spec); spec.loader.exec_module(J)
DOC = R / "docs/coordination/c14-iteration-20261020-v2/validation/r1_full"
Q = R / "docs/coordination/c14-iteration-20261020-v2/core/REQUESTS_R1_FULL.json"
IDENT = R / "build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1/IDENTITY.json"

def ref(p): return {"path":str(p),"identity":J.fid(p)}

def prepare():
    q = J.read(Q)
    J.require(q["status"] == "ready_for_validation_review" and not q["timing_admitted"], "finite R1 request required")
    for p, v in q["tool_files"].items(): J.require(J.fid(p) == v, "tool changed: " + p)
    J.require(J.fid(IDENT) == q["candidate_identity"], "frozen R1 identity changed")
    identity = J.read(IDENT)
    source = IDENT.parent / "source"
    for n,v in identity["members"].items():
        J.require(J.fid(source/n)=={"bytes":v["bytes"],"sha256":v["candidate_sha256"]},"R1 member changed: "+n)
    material=J.read(R/'handoff/github-20261019/C14_MATERIALIZATION.json')
    prod={n:{"bytes":identity["members"][n]["bytes"],"sha256":identity["members"][n]["candidate_sha256"]} for n in material["production_files"]}
    bpath=R/'docs/coordination/c14-iteration-20261020-v2/validation/R1_NATIVE_RESULT.json'
    b=J.read(bpath);J.require(b['status']=='passed' and b['actual_native_executions']==10,"R1 actual B10 required")
    cgpath=R/'docs/coordination/c14-iteration-20261019-v1/validation/R1_CODEGEN_RESULT.json'
    cg=J.read(cgpath);J.require(cg['status']=='passed' and cg['candidate_members']==identity['members'],"retained R1 codegen source differs")
    DOC.mkdir(parents=True,exist_ok=True)
    s0=R/'docs/coordination/c14-iteration-20261020-v2/validation/S0_RESULT.json'
    J.require(J.read(s0)['status']=='passed','current phase A required')
    copied=DOC/'S0_RESULT.json'
    if copied.exists():J.require(J.fid(copied)==J.fid(s0),'A copied binding changed')
    else:shutil.copyfile(s0,copied)
    binding={'status':'passed','scope':'source/input binding to existing R1 codegen and actual B10; not a new test or performance result',
        'candidate_identity_file':J.fid(IDENT),'candidate_members':identity['members'],
        'production_files':{'candidate':prod},'request_identity':J.fid(Q),
        'codegen':ref(cgpath),'targeted':ref(bpath),'current_A':ref(s0)}
    bridge=DOC/'R1_FULL_SOURCE_BINDING.json'
    if bridge.exists():J.require(J.read(bridge)==binding,'R1 source binding changed')
    else:J.write(bridge,binding)
    return identity,prod,material,source

def main():
    p=argparse.ArgumentParser();p.add_argument('mode',choices=('normal','sanitized'));args=p.parse_args()
    identity,prod,material,source=prepare()
    inv=J.read(Path('/root/xoffset-receive-completion/20261019/v1/full-final-v4-normal-build/test-inventory.json'))
    tests=inv['runtime_test_programs']+inv['subprocess_scenarios']+inv['tooling_checks']
    q={'schema_version':1,'stage':'S2_targeted','candidate':'R1','job_id':'r1-full-v1-'+args.mode,'mode':args.mode,
       'baseline_main_sha256':material['production_files']['xoffsetdatastructure.hpp']['sha256'],
       'production_files':prod,'overlays':{n:{'path':str(source/n),'identity':v} for n,v in prod.items() if v!=material['production_files'][n]},
       'targets':inv['runtime_test_programs'],'tests':tests,'runner_identity':J.fid(J.RUNNER),
       'entry_identity':J.fid(H/'full_entry.py'),'tool_files':{str(p):J.fid(p) for p in J.FIXED_TOOLS},
       'backing_free_bytes':shutil.disk_usage('/mnt/c').free,
       'backing_observed_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
    request=DOC/('R1_FULL_'+args.mode.upper()+'_REQUEST_v1.json')
    J.write(request,q)
    # Existing entry retains actual inventory, assertions, dependency locks,
    # source snapshot, original shared supervisor/session and 2400s bound.
    sys.argv=[str(H/'full_entry.py'),'run','--request',str(request)]
    try:J.main()
    except SystemExit as e:
        if e.code not in (None,0):raise
    receipts=[J.OUTPUT/('r1-full-v1-'+m)/'receipt.json' for m in ('normal','sanitized')]
    if all(p.exists() for p in receipts):
        for p in receipts:
            v=J.read(p);J.require(v['status']=='passed' and sorted(v['junit_names'])==sorted(tests) and v['production_files']==prod,'actual full inventory failed')
        J.write(DOC/'R1_FULL_RESULT.json',{'status':'passed','candidate':identity['candidate'],
            'request_identity':J.fid(Q),'normal_count':len(tests),'sanitized_count':len(tests),'production_files':prod,
            'normal_receipt':ref(receipts[0]),'sanitized_receipt':ref(receipts[1]),'performance_samples':0})

if __name__=='__main__':main()
