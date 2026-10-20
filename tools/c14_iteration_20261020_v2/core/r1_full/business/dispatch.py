"""Concrete R1 business stage dispatch, with the existing shared runner only.

check performs AST/file-identity checks only. Other actions require full73+73.
Run outside a lease; each heavy worker owns its original global lease.
"""
from pathlib import Path
import argparse, ast, hashlib, json, os, subprocess, sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
ROOT=Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
DOC=ROOT/'docs/coordination/c14-iteration-20261020-v2/validation/r1_full/business'
B=Path('/root/xoffset-c14-iteration/20261020/v2/validation/r1-business-v1')
OLD=Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发')
RUNNER=OLD/'experiments/vm_coordination/run_job.py'

def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def fid(p):
    b=Path(p).read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
def need(ok,message):
    if not ok:raise ValueError(message)
def write(p,v):
    with Path(p).open('x',encoding='utf-8') as f:json.dump(v,f,indent=2,ensure_ascii=False);f.write('\n')

def check():
    manifest=read(HERE/'TOOL_MANIFEST.json')
    for n,v in manifest['files'].items():need(fid(HERE/n)==v,'tool changed: '+n)
    for p in HERE.rglob('*.py'):ast.parse(p.read_text(encoding='utf-8-sig'))
    return {'status':'source_ready','tool_files':len(manifest['files']),'compiles':0,'native_runs':0,'timing_samples':0,
      'full_prerequisite':'R1_FULL_RESULT plus two actual73 receipts; checked again by prepare',
      'stages':{'prepare':300,'bench':2400,'functional':'normal1800/audit600/SAN1800/audit600/V1-compat1500',
                'package':2400,'runtime':2400},'runner':str(RUNNER),'output':str(B)}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('stage',choices=('check','prepare','bench','functional','package','runtime'));a=ap.parse_args()
    state=check()
    if a.stage=='check':print(json.dumps(state));return
    need(read(DOC.parent/'R1_FULL_RESULT.json')['status']=='passed','current R1 full gates required')
    sys.path.insert(0,str(HERE/'workers'));import common as C
    if a.stage=='functional':
        # Original dispatcher is deliberately outside the lease: it creates the
        # original functional80 and audit jobs sequentially, never nested locks.
        argv=['/usr/bin/python3','-B',str(HERE/'workers/dispatch.py'),'--inputs',str(DOC/'inputs.json')]
    else:
        timeout={'prepare':300,'bench':2400,'package':2400,'runtime':2400}[a.stage]
        script={'prepare':HERE/'prepare.py','bench':HERE/'workers/bench-profiles-worker.py',
          'package':HERE/'workers/package/package-worker.py','runtime':HERE/'workers/package/runtime-worker.py'}[a.stage]
        args=[] if a.stage=='prepare' else ['--inputs',str(DOC/'inputs.json')] if a.stage=='bench' else ['--spec',str(DOC/'package-input.json')]
        label={'prepare':'r1-business-prepare-v1','bench':'bench-profiles-v2','package':'package-standard-v2','runtime':'package-runtime-v2'}[a.stage]
        lease=B.parent/'leases'/label if a.stage=='prepare' else B/'queues'/label
        argv=['/usr/bin/python3','-B',str(RUNNER),'run','--owner','W1','--kind','validation','--out',str(lease),
          '--timeout-seconds',str(timeout),'--wait-seconds','0','--','/usr/bin/python3','-B',str(script),*args]
    write(DOC/(a.stage+'-dispatch-request.json'),{'argv':argv,'tool_manifest':fid(HERE/'TOOL_MANIFEST.json'),'scope':'R1 correctness only; zero timing'})
    p=subprocess.run(argv,env=C.env(),cwd=ROOT)
    check()
    write(DOC/(a.stage+'-dispatch-result.json'),{'returncode':p.returncode,'argv':argv})
    raise SystemExit(p.returncode)

if __name__=='__main__':main()
