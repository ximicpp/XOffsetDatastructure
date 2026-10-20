"""Original matrix helpers with explicit current write paths only."""
from pathlib import Path
import importlib.util
HERE=Path(__file__).resolve().parent
ROOT=Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
R=Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发')
H=R/'build/receive-completion-20261019'
B=Path('/root/xoffset-c14-iteration/20261020/v2/validation/m2-business-matrix-v1')
C=ROOT/'docs/coordination/c14-iteration-20261020-v2/validation/m2_full/business'
spec=importlib.util.spec_from_file_location('original_common_matrix',H/'common.py')
original=importlib.util.module_from_spec(spec);spec.loader.exec_module(original)
P,P1,fid,env=original.P,original.P1,original.fid,original.env
read=original.read
def write(path,value):
    import json
    with Path(path).open('x',encoding='utf-8') as f:json.dump(value,f,indent=2,ensure_ascii=False);f.write('\n')
