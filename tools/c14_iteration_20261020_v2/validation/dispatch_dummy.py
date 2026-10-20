"""One fixed30s observer Python-dummy preflight using the original lease."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('current',H/'verify_current.py')
V=importlib.util.module_from_spec(s);s.loader.exec_module(V)
D=V.DOC/'diag';T=H/'diag';OUT=V.OUTPUT/'diag/dummy-v1';Q=V.OUTPUT/'diag/dummy-v1-queue'
def write(p,v):
    with p.open('x',encoding='utf-8') as f:json.dump(v,f,indent=2);f.write('\n')
def main():
    tools=V.read(D/'TOOLS_PREPARED.json')['tools']
    assert all(V.fid(T/n)==v for n,v in tools.items()),'prepared tool changed'
    assert (D/'INTERFACE.json').read_bytes()==(T/'INTERFACE.json').read_bytes(),'interface copies differ'
    assert not OUT.exists() and not Q.exists(),'one fresh dummy preflight'
    runner=V.OLD/'experiments/vm_coordination/run_job.py'
    assert V.fid(runner)['sha256']=='6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909'
    argv=['/usr/bin/python3','-I','-B',str(runner),'run','--owner','W3','--kind','validation','--out',str(Q),'--timeout-seconds','30','--wait-seconds','0','--','/usr/bin/python3','-B',str(T/'diag_wrapper.py'),'selfcheck','--out',str(OUT),'--queue',str(Q)]
    write(D/'DUMMY_DISPATCH_REQUEST.json',dict(argv=argv,tools=tools,cpp_compiles=0,real_diag_native=0,python_dummy_budget=18,artifact_projection_bytes=22*1024**2))
    r=subprocess.run(argv,cwd=V.ROOT,env=dict(os.environ,PATH='/usr/bin:/bin'))
    result=V.read(OUT/'receipt.json') if (OUT/'receipt.json').is_file() else None
    queue=V.read(Q/'result.json')
    passed=r.returncode==0 and result and result['status']=='passed' and queue['status']=='complete' and all(not v for v in queue['cleanup'].values())
    write(D/'DUMMY_RESULT.json',dict(status='passed' if passed else 'failed',returncode=r.returncode,result=result,queue_result=queue,tools_after={n:V.fid(T/n) for n in tools},real_diag_started=False,diagnostic_admitted=False))
    assert passed,'dummy preflight failed: preserve files and diagnose; no real diagnostics admitted'
    print('18 observer Python dummy cases passed;0 C++/real diag')
if __name__=='__main__':main()
