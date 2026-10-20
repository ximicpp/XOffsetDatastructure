"""M2 paths around the original command, environment and global lease helpers."""
from pathlib import Path
import importlib.util, json, subprocess, os

H=Path(__file__).resolve().parent
ROOT=Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
R=Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发')
B=Path('/root/xoffset-c14-iteration/20261020/v2/validation/m2-business-v1')
C=ROOT/'docs/coordination/c14-iteration-20261020-v2/validation/m2_full/business'
RUNNER=R/'experiments/vm_coordination/run_job.py'
spec=importlib.util.spec_from_file_location('original_receive_common',R/'build/receive-completion-20261019/common.py')
original=importlib.util.module_from_spec(spec);spec.loader.exec_module(original)
P,P1,fid,read,env=original.P,original.P1,original.fid,original.read,original.env

def write(path,value):
    path=Path(path)
    with path.open('x',encoding='utf-8') as stream:json.dump(value,stream,indent=2,ensure_ascii=False);stream.write('\n')

def queue(label,script,args=(),timeout=2100,kind='validation'):
    argv=['/usr/bin/python3','-B',str(RUNNER),'run','--owner','W1','--kind',kind,
          '--out',str(B/'queues'/label),'--timeout-seconds',str(timeout),'--wait-seconds','0','--',
          '/usr/bin/python3','-B',str(script),*map(str,args)]
    write(C/(label+'-request.json'),{'argv':argv,'script':fid(script)})
    result=subprocess.run(argv,capture_output=True,text=True,env=env())
    write(C/(label+'-receipt.json'),{'returncode':result.returncode,'stdout':result.stdout,'stderr':result.stderr})
    if result.returncode:raise RuntimeError('Preserved failure: '+label)
