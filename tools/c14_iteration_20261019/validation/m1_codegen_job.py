"""One bounded M1 v2 original-driver codegen job on the existing global lease."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('entry',HERE/'candidate_job.py')
J=importlib.util.module_from_spec(spec); spec.loader.exec_module(J)
R=J.ROOT
QPATH=R/'docs/coordination/c14-iteration-20261019-v1/core/REQUESTS.json'
DOC=J.DOC
OUT=J.OUTPUT/'m1-v2'
LEASE=J.OUTPUT/'leases/m1-v2-codegen'
ADMISSION=DOC/'M1_CODEGEN_JOB_ADMISSION.json'

def inspect():
    q=J.read(QPATH)
    J.require(J.read(DOC/'S0_RESULT.json')['status']=='passed','S0 required')
    J.require(q['candidate']=='m1-default-scalar-v2' and not q['timing_admitted'],'unexpected candidate/request')
    for n,value in q['input_bindings'].items(): J.require(J.fid(R/n)==value,'core request binding mismatch: '+n)
    candidate=R/'build/c14-iteration-20261019-v1/candidates/m1-default-scalar-v2'
    identity=J.read(candidate/'IDENTITY.json')
    for n,value in identity['members'].items():
        J.require(J.fid(candidate/'source'/n)=={'bytes':value['bytes'],'sha256':value['candidate_sha256']},'candidate member mismatch: '+n)
    J.require(identity['member_count']==154,'154-member candidate required')
    J.require(J.fid(candidate/'source/xoffsetdatastructure.hpp')['sha256']==q['main_sha256'],'main mismatch')
    job=next(x for x in q['jobs'] if x['id']=='M1-CODEGEN-v2')
    J.require(job['compile_budget']==2 and job['native_budget']==0 and job['deadline_seconds']==600,'unexpected finite codegen budget')
    argv=list(job['argv'])
    # Validation owns its snapshot; do not materialize into the core write domain.
    argv[argv.index('--source')+1]=str(OUT/'staged/source')
    experiment=Path(argv[argv.index('--experiment')+1])
    tools={str(p):J.fid(p) for p in [J.RUNNER,HERE/'m1_codegen_job.py',R/'tools/c14_iteration_20261019/core/run_m1.py',J.TOOLS/'p2996-profile/bin/clang++-libcxx']}
    s0=J.read(DOC/'S0_RESULT.json')
    for p,value in s0['toolchain']['files'].items():
        J.require(J.fid(p)['sha256']==value['actual'],'fixed profile tool/runtime changed: '+p)
        tools[p]=J.fid(p)
    experiment_files={str(p):J.fid(p) for p in sorted(experiment.rglob('*')) if p.is_file()}
    return q,candidate,identity,argv,tools,experiment_files

def worker():
    a=J.read(ADMISSION)
    J.require(J.fid(QPATH)==a['request_identity'],'core request changed after admission')
    for p,value in {**a['tool_files'],**a['experiment_files']}.items(): J.require(J.fid(p)==value,'admitted input changed: '+p)
    holder=J.read(J.LOCK/'queue.lock')
    J.require(holder['status']=='running' and Path(holder['out'])==LEASE,'real lease binding mismatch')
    source=OUT/'staged/source'; source.mkdir(parents=True,exist_ok=False)
    for n,value in a['candidate_members'].items():
        original=Path(a['candidate_source'])/n
        expected={'bytes':value['bytes'],'sha256':value['candidate_sha256']}
        J.require(J.fid(original)==expected,'candidate changed: '+n)
        p=source/n; p.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(original,p)
        J.require(J.fid(p)==expected,'copy changed: '+n)
    J.write(OUT/'staged/receipt.json',{'status':'passed','members':154,'source':str(source),
            'request':a['request_identity'],'candidate_members':a['candidate_members'],
            'note':'Exact validation-owned staging replaces requested core-owned stage location; no source edit.'})
    done=subprocess.run(a['argv'],cwd=R,env={'PATH':'/usr/bin:/bin','LANG':'C','LC_ALL':'C','TZ':'UTC'})
    for p,value in {**a['tool_files'],**a['experiment_files']}.items(): J.require(J.fid(p)==value,'input changed during codegen: '+p)
    J.write(OUT/'codegen-exit.json',{'status':'passed' if done.returncode==0 else 'rejected','returncode':done.returncode,
            'lease_holder':holder,'request':a['request_identity'],'native_executions':0,'compile_budget':2,
            'independent_machine_code_admission':'pending'})
    raise SystemExit(done.returncode)

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=('admit-run','worker'));args=p.parse_args()
    if args.action=='worker': worker();return
    q,candidate,identity,argv,tools,experiment_files=inspect()
    J.require(not OUT.exists() and not LEASE.exists(),'fresh M1 output/lease directories required')
    J.require(shutil.disk_usage('/root').free>8*1024**3 and shutil.disk_usage('/mnt/c').free>8*1024**3,'insufficient backing/WSL output budget')
    admission={'status':'passed','scope':'M1 v2 codegen only; not mechanism/performance admission',
       'request_path':str(QPATH),'request_identity':J.fid(QPATH),'argv':argv,
       'tool_files':tools,'experiment_files':experiment_files,'candidate_members':identity['members'],
       'candidate_source':str(candidate/'source'),'candidate_main_sha256':q['main_sha256'],
       'write_scope_adaptation':'Stage exact candidate in validation/m1-v2/staged/source instead of core; original candidate remains read-only.',
       'compile_budget':2,'native_budget':0,'timing_samples':0,'deadline_seconds':600,
       'disk_free_bytes':{'WSL':shutil.disk_usage('/root').free,'C_backing':shutil.disk_usage('/mnt/c').free}}
    J.write(ADMISSION,admission)
    LEASE.parent.mkdir(parents=True,exist_ok=True)
    run=['/usr/bin/python3','-I','-B',str(J.RUNNER),'run','--owner','W1','--kind','build','--out',str(LEASE),
         '--timeout-seconds','600','--wait-seconds','0','--','/usr/bin/python3','-I','-B',str(Path(__file__).resolve()),'worker']
    J.write(DOC/'M1_CODEGEN_DISPATCH_REQUEST.json',{'argv':run,'admission':J.fid(ADMISSION)})
    r=subprocess.run(run,cwd=R,env={'PATH':'/usr/bin:/bin','LANG':'C','LC_ALL':'C','TZ':'UTC'})
    J.write(DOC/'M1_CODEGEN_DISPATCH_RESULT.json',{'returncode':r.returncode,'lease_result':str(LEASE/'result.json'),
             'stage_receipt':str(OUT/'codegen-exit.json'),'native_budget':0})
    raise SystemExit(r.returncode)

if __name__=='__main__': main()
