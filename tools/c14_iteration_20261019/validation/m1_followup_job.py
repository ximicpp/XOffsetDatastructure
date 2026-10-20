"""Execute just the admitted M1 correctness or fixed-M job; existing lease only."""
import argparse
import importlib.util
from pathlib import Path
import shutil
import subprocess

HERE=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('cg',HERE/'m1_codegen_job.py')
C=importlib.util.module_from_spec(s);s.loader.exec_module(C)
J=C.J

def verify(mode):
    a=J.read(C.ADMISSION);q=J.read(C.QPATH);cg=J.read(C.OUT/'CODEGEN_ADMISSION.json')
    J.require(J.fid(C.QPATH)==a['request_identity']==cg['request_identity'],'request/codegen closure changed')
    J.require(cg['status']=='passed','codegen admission missing')
    for p,value in {**a['tool_files'],**a['experiment_files']}.items():J.require(J.fid(p)==value,'admitted tool/input changed: '+p)
    for n,value in a['candidate_members'].items():J.require(J.fid(C.OUT/'staged/source'/n)=={'bytes':value['bytes'],'sha256':value['candidate_sha256']},'staged source changed: '+n)
    for n,value in q['input_bindings'].items():J.require(J.fid(J.ROOT/n)==value,'core request input changed: '+n)
    J.require(shutil.disk_usage('/mnt/c').free>=8*1024**3 and shutil.disk_usage('/root').free>=8*1024**3,'8GiB backing reserve required')
    job=next(x for x in q['jobs'] if x['id']=='M1-'+mode.upper()+'-v2')
    argv=list(job['argv']);argv[argv.index('--source')+1]=str(C.OUT/'staged/source')
    if mode=='mechanism':J.require(J.read(J.DOC/'M1_CORRECTNESS_RESULT.json')['status']=='passed','correctness gate required')
    return a,q,job,argv

def main():
    p=argparse.ArgumentParser();p.add_argument('mode',choices=('correctness','mechanism'));p.add_argument('--worker',action='store_true');args=p.parse_args()
    mode=args.mode;a,q,job,argv=verify(mode)
    lease=J.OUTPUT/('leases/m1-v2-'+mode);out=C.OUT/mode
    admission=J.DOC/('M1_'+mode.upper()+'_JOB_ADMISSION.json')
    if args.worker:
        holder=J.read(J.LOCK/'queue.lock');J.require(holder['status']=='running' and Path(holder['out'])==lease,'wrong current lease')
        frozen=J.read(admission);J.require(frozen['request_identity']==J.fid(C.QPATH) and frozen['worker_identity']==J.fid(__file__),'followup admission changed')
        r=subprocess.run(argv,cwd=J.ROOT,env={'PATH':'/usr/bin:/bin','LANG':'C','LC_ALL':'C','TZ':'UTC'})
        verify(mode)
        J.write(C.OUT/(mode+'-exit.json'),{'returncode':r.returncode,'request_identity':a['request_identity'],'lease_holder':holder})
        raise SystemExit(r.returncode)
    J.require(not out.exists() and not lease.exists(),'fresh followup outputs required')
    J.write(admission,{'status':'passed','scope':mode+' only, no screening','request_identity':a['request_identity'],
        'codegen_admission_identity':J.fid(C.OUT/'CODEGEN_ADMISSION.json'),'worker_identity':J.fid(__file__),
        'argv':argv,'native_budget':job['native_budget'],'compile_budget':job['compile_budget'],'deadline_seconds':job['deadline_seconds']})
    cmd=['/usr/bin/python3','-I','-B',str(J.RUNNER),'run','--owner','W1','--kind','validation','--out',str(lease),
        '--timeout-seconds',str(job['deadline_seconds']),'--wait-seconds','0','--','/usr/bin/python3','-I','-B',str(Path(__file__).resolve()),mode,'--worker']
    J.write(J.DOC/('M1_'+mode.upper()+'_DISPATCH_REQUEST.json'),{'argv':cmd,'admission':J.fid(admission)})
    r=subprocess.run(cmd,cwd=J.ROOT,env={'PATH':'/usr/bin:/bin','LANG':'C','LC_ALL':'C','TZ':'UTC'})
    J.write(J.DOC/('M1_'+mode.upper()+'_DISPATCH_RESULT.json'),{'returncode':r.returncode,'lease_result':str(lease/'result.json'),'stage_output':str(out)})
    raise SystemExit(r.returncode)

if __name__=='__main__':main()
