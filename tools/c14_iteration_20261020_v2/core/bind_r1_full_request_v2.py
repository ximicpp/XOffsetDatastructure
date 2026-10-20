"""Preserve the v1 execution recipe, strengthen final closure in a v2 envelope."""
import hashlib
import json
from pathlib import Path
r=Path(__file__).resolve().parents[3]
h=Path(__file__).resolve().parent
d=r/'docs/coordination/c14-iteration-20261020-v2/core'
w='/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发'
def fid(p):
    b=p.read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
inner=d/'REQUESTS_R1_FULL.json';q=json.loads(inner.read_text(encoding='utf-8'))
q['inner_request']={'path':w+'/'+inner.relative_to(r).as_posix(),'identity':fid(inner)}
q['revision']='v2 envelope strengthens only final request/tool/source154/receipt/PREPARED closure recheck. Frozen v1 tool derivation and worker execution recipes remain unchanged; no native attempts or budgets added.'
for p in (h/'finalize_r1_full_bound.py',Path(__file__)):
    q['tool_files'][w+'/'+p.relative_to(r).as_posix()]=fid(p)
last=q['jobs'][-1]
last['argv'][-1]=w+'/'+(h/'finalize_r1_full_bound.py').relative_to(r).as_posix()
last['expected'] += ' plus R1_FUNCTIONAL_GATES_BINDING.json for the current outer/inner request, source154, tools, all full references and prepared closure'
with (d/'REQUESTS_R1_FULL_v2.json').open('x',encoding='utf-8') as f:json.dump(q,f,indent=2,ensure_ascii=False);f.write('\n')
ready=json.loads((d/'READY.json').read_text(encoding='utf-8'))
ready['R1_full']='REQUESTS_R1_FULL_v2.json active outer envelope (inner v1 recipe preserved); final full/source154/receipt/tools/PREPARED binding recheck added; no extra native budget'
(d/'READY.json').write_text(json.dumps(ready,indent=2)+'\n',encoding='utf-8')
print('R1 full v2 envelope ready; v1 recipe/tool identities preserved; no native execution')
