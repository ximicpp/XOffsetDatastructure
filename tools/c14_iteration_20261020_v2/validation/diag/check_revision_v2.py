"""Only six changed-branch Python regression cases for the stable v2 observer."""
import fcntl
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('current',H.parent/'verify_current.py')
V=importlib.util.module_from_spec(s);s.loader.exec_module(V)
DOC=V.DOC/'diag';OUT=V.OUTPUT/'diag/dummy-v2-subset';Q=V.OUTPUT/'diag/dummy-v2-subset-queue'
RUNNER=V.OLD/'experiments/vm_coordination/run_job.py';LOCK=V.OLD/'build/vm-coordination'
CASES=('normal','event_hang','partial_hang','collector_error','register_error','log_limit')
def write(p,v):
    with Path(p).open('x',encoding='utf-8') as f:json.dump(v,f,indent=2);f.write('\n')
def verify_tools():
    tools=V.read(DOC/'TOOLS_PREPARED_v2.json')['tools']
    assert all(V.fid(H/n)==v for n,v in tools.items()),'stable v2 tool changed'
    return tools
def worker():
    tools=verify_tools();q=V.read(Q/'request.json')
    with (LOCK/'queue.lock').open('r+') as f:
        try:fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:pass
        else:raise RuntimeError('original lease not held')
        f.seek(0);holder=json.load(f)
    assert holder['status']=='running' and holder['run_id']==q['run_id'] and holder['out']==str(Q) and holder['supervisor_pid']==os.getppid()
    assert q['command']==['/usr/bin/python3','-B',str(Path(__file__).resolve()),'worker'] and q['lock_dir']==str(LOCK) and q['timeout_seconds']==30 and q['owner']=='W3' and q['kind']=='validation'
    import dummy_selfcheck as S
    selected={x[0]:x for x in S.CASES};S.CASES=tuple(selected[x] for x in CASES)
    result=S.run(OUT)
    assert result['status']=='passed' and result['actual_python_dummy_children']==6
    assert all(V.fid(H/n)==v for n,v in tools.items())
def main():
    if sys.argv[1:] == ['worker']:worker();return
    assert not sys.argv[1:] and not OUT.exists() and not Q.exists(),'one fresh targeted revision check'
    tools=verify_tools()
    cmd=['/usr/bin/python3','-I','-B',str(RUNNER),'run','--owner','W3','--kind','validation','--out',str(Q),'--timeout-seconds','30','--wait-seconds','0','--','/usr/bin/python3','-B',str(Path(__file__).resolve()),'worker']
    write(DOC/'DUMMY_DISPATCH_v2.json',dict(command=cmd,selected_cases=CASES,tools=tools,subset_driver=V.fid(__file__),native_cpp=0,projection_bytes=8*1024**2))
    r=subprocess.run(cmd,cwd=V.ROOT,env=dict(os.environ,PATH='/usr/bin:/bin'));q=V.read(Q/'result.json')
    receipt=V.read(OUT/'receipt.json') if (OUT/'receipt.json').is_file() else None
    ok=r.returncode==0 and q['status']=='complete' and all(not v for v in q['cleanup'].values()) and receipt and receipt['status']=='passed'
    write(DOC/'DUMMY_RESULT_v2.json',dict(status='passed' if ok else 'failed',tools=tools,selected_cases=CASES,actual_result=receipt,queue_result=q,historical18_belong_to_previous_revision=True,current_revision_cases=6,not_all19_cases_rerun=True,new_cpp_compiles=0,new_real_diagnostics=0,diagnostic_admitted=False))
    assert ok,'targeted revision cases failed; preserve and diagnose'
    print('6 changed-branch observer cases passed; no C++ or real diagnostics')
if __name__=='__main__':main()
