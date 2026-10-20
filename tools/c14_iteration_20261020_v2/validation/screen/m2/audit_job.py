"""One independent v2 offline audit dispatch, after actual host restoration."""
import argparse,subprocess
from pathlib import Path
import screen as S
H=Path(__file__).resolve().parent
D=S.ROOT/'docs/coordination/c14-iteration-20261020-v2/validation/screen/M2'
OUT=Path('/root/xoffset-c14-iteration/20261020/v2/validation/M2-screen-20261020-v2-v1');QUEUE=OUT.parent/'m2-screen-audit-v1/queue'
def main():
    p=argparse.ArgumentParser();p.add_argument('--freeze-sha256',required=True);a=p.parse_args()
    f=S.check_freeze(OUT/'freeze.json',a.freeze_sha256);r=S.C.read(D/'RESTORED.json')
    S.need(r['status']=='passed' and r['freeze_sha256']==a.freeze_sha256 and r['user_processes_restored'] is True,'restore before any offline audit')
    S.need(not QUEUE.exists() and not (D/'AUDIT_DISPATCH_REQUEST.json').exists(),'one new offline audit only')
    sealed=(OUT/'status.json').is_file() and (OUT/'manifest.json').is_file();action='audit' if sealed else 'failure';postrun=S.postrun_pin(OUT) if sealed else None
    if sealed:S.write(D/'POSTRUN_PIN.json',dict(postrun_pin=postrun,freeze_sha256=a.freeze_sha256,screen_manifest=S.fid(OUT/'manifest.json'),queue_manifest=S.fid(OUT/'queue/manifest.json'),captured_before_offline_audit=True))
    argv=['/usr/bin/python3','-B',str(H/'screen.py'),action,'--out',str(OUT),'--restoration',str(D/'RESTORED.json'),'--result',str(D/('AUDIT.json' if sealed else 'FAILURE.json')),'--freeze-sha256',a.freeze_sha256]
    if sealed:argv+=['--postrun-pin',postrun]
    argv+=['--audit-queue',str(QUEUE)]
    cmd=['/usr/bin/python3','-B',str(S.RUNNER),'run','--owner','W3','--kind','validation','--out',str(QUEUE),'--timeout-seconds','3600','--wait-seconds','0','--',*argv]
    S.write(D/'AUDIT_DISPATCH_REQUEST.json',dict(argv=cmd,sealed=sealed,postrun_pin=postrun,restored=S.fid(D/'RESTORED.json'),native_budget=0,timing_budget=0));r=subprocess.run(cmd,cwd=S.ROOT)
    S.write(D/'AUDIT_DISPATCH_RESULT.json',dict(returncode=r.returncode,queue_result=S.C.read(QUEUE/'result.json')));raise SystemExit(r.returncode)
if __name__=='__main__':main()
