"""One offline audit of the closed M1 attempt; never launches a native child."""
import importlib.util
from pathlib import Path
import subprocess
import json

H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('cg',H/'m1_codegen_job.py')
C=importlib.util.module_from_spec(s);s.loader.exec_module(C);J=C.J
D=J.DOC/'screen';OUT=J.OUTPUT/'M1-screen-20261019-v1'
QUEUE=J.OUTPUT/'m1-screen-audit-v1/queue'
PIN='e57468ed00157a0154f220b4c67a29879c3fc14bebb9ff04d65f621e4ed11685'

def main():
    restored=J.read(D/'RESTORED.json')
    J.require(restored['status']=='passed' and restored['user_processes_restored'] is True,'restore before audit')
    J.require(restored['freeze_sha256']==PIN,'restoration freeze differs')
    J.require(not QUEUE.exists(),'one fresh offline audit only')
    worker=H/'screen/m1_screen.py'
    sealed=(OUT/'status.json').is_file() and (OUT/'manifest.json').is_file()
    action='audit' if sealed else 'failure'
    postrun=None
    if sealed:
        p=subprocess.run(['/usr/bin/python3','-B',str(worker),'pin','--out',str(OUT)],capture_output=True,check=True)
        postrun=json.loads(p.stdout)['postrun_pin']
        J.write(D/'POSTRUN_PIN.json',{'postrun_pin':postrun,'freeze_sha256':PIN,
            'screen_manifest':J.fid(OUT/'manifest.json'),'queue_manifest':J.fid(OUT/'queue/manifest.json'),
            'captured_before_offline_audit':True,'timing_dispatch':J.fid(D/'TIMING_DISPATCH_RESULT.json')})
    argv=['/usr/bin/python3','-B',str(worker),action,'--out',str(OUT),'--restoration',str(D/'RESTORED.json'),
          '--result',str(D/'AUDIT.json' if sealed else D/'FAILURE.json'),'--freeze-sha256',PIN]
    if sealed:argv+=['--postrun-pin',postrun]
    argv+=['--audit-queue',str(QUEUE)]
    cmd=['/usr/bin/python3','-B',str(J.RUNNER),'run','--owner','W3','--kind','validation','--out',str(QUEUE),
         '--timeout-seconds','3600','--wait-seconds','0','--',*argv]
    J.write(D/'AUDIT_DISPATCH_REQUEST.json',{'argv':cmd,'sealed':sealed,'postrun_pin':postrun,
        'restored':J.fid(D/'RESTORED.json'),'native_budget':0,'timing_budget':0})
    r=subprocess.run(cmd,cwd=J.ROOT)
    J.write(D/'AUDIT_DISPATCH_RESULT.json',{'returncode':r.returncode,'queue_result':J.read(QUEUE/'result.json')})
    raise SystemExit(r.returncode)

if __name__=='__main__':main()
