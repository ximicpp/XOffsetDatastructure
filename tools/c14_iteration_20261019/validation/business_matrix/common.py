"""Original matrix helpers with explicit current write paths only."""
from pathlib import Path
import importlib.util
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
R=Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发')
H=R/'build/receive-completion-20261019'
B=Path('/root/xoffset-c14-iteration/20261019/v1/validation/m1-business-matrix-v2')
C=ROOT/'docs/coordination/c14-iteration-20261019-v1/validation/business'
spec=importlib.util.spec_from_file_location('original_common_matrix',H/'common.py')
original=importlib.util.module_from_spec(spec);spec.loader.exec_module(original)
P,P1,fid,env=original.P,original.P1,original.fid,original.env
read=original.read
def write(path,value):
    import json
    with Path(path).open('x',encoding='utf-8') as f:json.dump(value,f,indent=2,ensure_ascii=False);f.write('\n')
