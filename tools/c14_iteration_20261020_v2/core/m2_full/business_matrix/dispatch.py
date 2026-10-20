"""Fixed existing52 matrix only; no timing or general scheduling interface."""
from pathlib import Path
import argparse,ast,hashlib,json,os,subprocess,sys,shutil
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parent))
from common import *
RUNNER=R/'experiments/vm_coordination/run_job.py'
QUEUE=B/'queues/production-C-matrix-v2'
def need(ok,message):
    if not ok:raise ValueError(message)
def stable(p):
    b=Path(p).read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
def verify_tools():
    m=read(HERE/'TOOL_MANIFEST.json')
    for n,v in m['files'].items():need(stable(HERE/n)==v,'matrix tool changed '+n)
    for p in HERE.glob('*.py'):ast.parse(p.read_text(encoding='utf-8'))
def worker():
    verify_tools();holder=read(R/'build/vm-coordination/queue.lock')
    need(holder['status']=='running' and Path(holder['out'])==QUEUE,'original active matrix lease required')
    prepared=read(C/'PREPARED.json')
    need(prepared['status']=='prepared','M2 business prepared input required')
    for p,v in prepared['bindings'].items():need(stable(p)==v,'M2 bound input differs '+p)
    for leaf in ('final-business-functional-v2/result.json','package-standard-v2/result.json','package-runtime-v2/result.json'):
        p=B.parent/'m2-business-v1'/leaf;need(read(p)['status']=='passed','required business/consumer gate '+leaf)
    base=B.parent/'m2-full-v1-normal/source';source=B/'source'
    need(not source.exists(),'fresh matrix source required')
    shutil.copytree(base,source)
    meta=B.parent/'m2-business-v1/metadata'
    for rel in ('experiments/serialization_comparison','tools/serialization_comparison'):
        shutil.copytree(meta/rel,source/rel)
    members={p.relative_to(source).as_posix():stable(p) for p in source.rglob('*') if p.is_file()}
    ident=C/'candidate-source-identity-v2.json';prod=read(ident)['production_files']
    for n,v in prod.items():need(members[n]==v,'matrix production differs '+n)
    material=B/'source-identity.json';write(material,{'source':str(source),'production_files':prod,'identity':read(ident),'full_expected_ctest':73})
    bindings=dict(prepared['bindings'])
    for p in HERE.glob('*'):
        if p.is_file():bindings[str(p)]=stable(p)
    reference=Path('/root/xoffset-serialization-comparison/20261017/implementation/prepared-v1/reference.py')
    spec={'schema_version':1,'kind':'c14-m2-production-matrix-input-v1','source':str(source),
      'materialization':{'path':str(material),'identity':stable(material)},
      'candidate_identity':{'path':str(ident),'identity':stable(ident)},'full_receipts':prepared['full_receipts'],
      'bindings':bindings,'source_members':members,'reference':{'path':str(reference),'identity':stable(reference)}}
    request=C/'MATRIX_INPUT.json';write(request,spec)
    p=subprocess.run(['/usr/bin/python3','-B',str(HERE/'worker.py'),'--spec',str(request)],env=env(),cwd=ROOT)
    verify_tools();raise SystemExit(p.returncode)
def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=('check','run','worker'));a=p.parse_args();verify_tools()
    if a.action=='check':print(json.dumps({'status':'source_ready','planned_native_children':52,'normal':26,'SAN':26,'compiles_executed':0,'native_executed':0,'cap_seconds':3600}));return
    if a.action=='worker':worker();return
    need(not (B/'production-C-matrix-v2').exists() and not QUEUE.exists(),'fresh one-attempt matrix outputs required')
    argv=['/usr/bin/python3','-B',str(RUNNER),'run','--owner','W1','--kind','validation','--out',str(QUEUE),
      '--timeout-seconds','3600','--wait-seconds','0','--','/usr/bin/python3','-B',str(HERE/'dispatch.py'),'worker']
    write(C/'matrix-dispatch-request.json',{'argv':argv,'tool_manifest':stable(HERE/'TOOL_MANIFEST.json'),'planned_native_children':52,'timing_samples':0})
    result=subprocess.run(argv,cwd=ROOT,env=env());write(C/'matrix-dispatch-result.json',{'returncode':result.returncode});raise SystemExit(result.returncode)
if __name__=='__main__':main()
