"""Exactly one new R1 candidate -S compile; reuse the pinned C14 control."""
import argparse
import importlib.util
from pathlib import Path
import shutil
import subprocess

H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('cg',H/'m1_codegen_job.py')
C=importlib.util.module_from_spec(s);s.loader.exec_module(C);J=C.J
Q=J.ROOT/'docs/coordination/c14-iteration-20261019-v1/core/REQUESTS_R1_CODEGEN.json'
OUT=J.OUTPUT/'r1-v1';LEASE=J.OUTPUT/'leases/r1-v1-codegen'
A=J.DOC/'R1_CODEGEN_JOB_ADMISSION.json'

def main():
    p=argparse.ArgumentParser();p.add_argument('--worker',action='store_true');args=p.parse_args()
    q=J.read(Q)
    if args.worker:
        a=J.read(A);J.require(J.fid(Q)==a['request_identity'],'R1 request changed')
        holder=J.read(J.LOCK/'queue.lock');J.require(holder['status']=='running' and Path(holder['out'])==LEASE,'wrong lease')
        for path,v in a['inputs'].items():J.require(J.fid(path)==v,'R1 input changed: '+path)
        source=OUT/'source';source.mkdir(parents=True,exist_ok=False)
        for n,v in a['members'].items():
            src=Path(a['candidate_source'])/n
            J.require(J.fid(src)=={'bytes':v['bytes'],'sha256':v['candidate_sha256']},'R1 member changed: '+n)
            dst=source/n;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,dst)
        (OUT/'codegen').mkdir()
        with (OUT/'codegen/compile.log').open('xb') as log:r=subprocess.run(a['argv'],cwd=J.ROOT,stdout=log,stderr=subprocess.STDOUT,env={'PATH':'/usr/bin:/bin','LANG':'C','LC_ALL':'C','TZ':'UTC'})
        for path,v in a['inputs'].items():J.require(J.fid(path)==v,'R1 input changed after compile: '+path)
        J.write(OUT/'codegen/receipt.json',{'returncode':r.returncode,'argv':a['argv'],'request_identity':a['request_identity'],
                 'members':a['members'],'inputs':a['inputs'],'lease_holder':holder,'actual_compiles':1,'native_executions':0,'timing_samples':0})
        raise SystemExit(r.returncode)
    J.require(not OUT.exists() and not LEASE.exists(),'fresh R1 output required')
    J.require('No blocking source defect found' in (Q.parent/'REVIEW_R1.md').read_text(),'independent R1 source gate missing')
    inputs={str(J.ROOT/n):J.fid(J.ROOT/n) for n in q['input_bindings']}
    for n,v in q['input_bindings'].items():J.require(inputs[str(J.ROOT/n)]==v,'R1 request binding mismatch: '+n)
    ref=q['reference_control'];J.require(J.fid(ref['path'])=={'bytes':ref['bytes'],'sha256':ref['sha256']},'reference control changed')
    inputs[ref['path']]=J.fid(ref['path']);inputs[str(Q)]=J.fid(Q);inputs[str(H/'r1_codegen_job.py')]=J.fid(__file__)
    inputs[str(Q.parent/'REVIEW_R1.md')]=J.fid(Q.parent/'REVIEW_R1.md')
    previous=J.read(C.ADMISSION)
    inputs.update(previous['tool_files']);inputs.update(previous['experiment_files'])
    candidate=J.ROOT/'build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1'
    identity=J.read(candidate/'IDENTITY.json')
    for n,v in identity['members'].items():J.require(J.fid(candidate/'source'/n)=={'bytes':v['bytes'],'sha256':v['candidate_sha256']},'R1 source mismatch')
    job=q['jobs'][1];J.require(job['compile_budget']==1 and job['native_budget']==0,'unexpected R1 budget')
    argv=list(job['argv'])
    argv=[('-I'+str(OUT/'source')) if x=='-I/root/xoffset-c14-iteration/20261019/v1/core/r1-canonical-hotcold-v1/source' else x for x in argv]
    J.write(A,{'status':'passed','scope':'one R1 -S candidate compile only','request_identity':J.fid(Q),'argv':argv,
          'candidate_source':str(candidate/'source'),'members':identity['members'],'inputs':inputs,
          'reference_control':ref,'write_scope':'Exact derivative snapshot staged in validation/r1-v1/source; core remains read-only.'})
    cmd=['/usr/bin/python3','-I','-B',str(J.RUNNER),'run','--owner','W1','--kind','build','--out',str(LEASE),'--timeout-seconds','240','--wait-seconds','0','--','/usr/bin/python3','-I','-B',str(Path(__file__).resolve()),'--worker']
    J.write(J.DOC/'R1_CODEGEN_DISPATCH_REQUEST.json',{'argv':cmd,'admission':J.fid(A)})
    r=subprocess.run(cmd,cwd=J.ROOT,env={'PATH':'/usr/bin:/bin','LANG':'C','LC_ALL':'C','TZ':'UTC'})
    J.write(J.DOC/'R1_CODEGEN_DISPATCH_RESULT.json',{'returncode':r.returncode,'lease_result':str(LEASE/'result.json')})
    raise SystemExit(r.returncode)

if __name__=='__main__':main()
