"""Register independent M2 functional gates, retaining all live R1 bytes."""
import copy
import hashlib
import json
from pathlib import Path
r=Path(__file__).resolve().parents[3]
h=Path(__file__).resolve().parent
d=r/'docs/coordination/c14-iteration-20261020-v2/core'
w='/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发'
def fid(p):
    b=p.read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
q=json.loads((d/'REQUESTS_R1_FULL.json').read_text(encoding='utf-8'))
q=json.loads(json.dumps(q).replace('r1-canonical-hotcold-v1','m2-exact-owns-v1').replace('r1-full','m2-full').replace('r1_full','m2_full').replace('r1-business','m2-business').replace('R1','M2'))
identity_path=r/'build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1/IDENTITY.json'
identity=json.loads(identity_path.read_text(encoding='utf-8'))
material=json.loads((r/'handoff/github-20261019/C14_MATERIALIZATION.json').read_text(encoding='utf-8'))
prod={n:{'bytes':identity['members'][n]['bytes'],'sha256':identity['members'][n]['candidate_sha256']} for n in material['production_files']}
files=[p for p in (h/'m2_full').rglob('*') if p.is_file() and '__pycache__' not in p.parts]
files += [h/'m2_full_jobs.py',h/'finalize_m2_full.py',h/'derive_m2_full.py',Path(__file__)]
q['tool_files']={w+'/'+p.relative_to(r).as_posix():fid(p) for p in sorted(files)}
launcher='/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/bin/clang++-libcxx'
launcher_id={'bytes':61,'sha256':'c8ea06a7c58557e3b89d09873dc88457d7b0c5d9af3f08cf0449093f2fbd1b87'}
q['tool_files'][launcher]=launcher_id
q['compiler_launcher']={'path':launcher,'identity':launcher_id}
q.update(candidate=identity['candidate'],candidate_identity=fid(identity_path),source_members=identity['members'],
         production14=prod,candidate_main_sha256=prod['xoffsetdatastructure.hpp']['sha256'],same_frozen_M2=True)
q.pop('same_frozen_R1',None)
q['tool_derivation']={'path':w+'/'+(h/'m2_full/DERIVATION.json').relative_to(r).as_posix(),'identity':fid(h/'m2_full/DERIVATION.json')}
q['review']='core/REVIEW_M2_FULL.md final source/path/identity review required before launch'
q['prerequisites']=['current A/runtime/dependency/source/host/backing/real global lease passed','actual M2 targeted4normal+4SAN and same-source specific static4-to-1 codegen passed']
q['C_dependency']='M2 complete functional gates may progress independently of C tooling/results; dynamic mechanism remains a separate missing gate and cannot be replaced by static4-to-1 or full correctness.'
q['source_binding_bridge']='validation/m2_full/M2_FULL_SOURCE_BINDING.json binds actual existing targeted8/static codegen/source154 and explicit current launcher, not dynamic-hit or performance evidence'
q['control_reuse']='Immutable fixtures/oracles/control profiles/V1 references retained exactly; M2 candidate builds/results are new. Neither R1 nor M1 status substitutes. R1 running tool/request closure is untouched.'
q['final_binding']='Single finalizer verifies this approved request, all tools+61B launcher, full receipt/child-request/JUnit/inventory/compileDB/source154, current bridge and PREPARED bindings before publishing M2_FUNCTIONAL_GATES.json.'
q['all_new_outputs']='/root/xoffset-c14-iteration/20261020/v2/validation/m2-* and docs/coordination/c14-iteration-20261020-v2/validation/m2_full/'
q['legacy_evidence_limit']='Old M2 targeted/static results did not explicitly pin the61B outer wrapper; their actual reviewed scope is retained without retroactive proof or rerunning them. All future M2 full/business builds explicitly bind the launcher above; business PREPARED binding is inherited by matrix workers.'
for job in q['jobs']:
    job['argv']=[x.replace('/r1_full_jobs.py','/m2_full_jobs.py').replace('/finalize_r1_full.py','/finalize_m2_full.py') for x in job['argv']]
    if job['id'].startswith('M2-FULL-normal'):
        job['depends_on']=['actual M2 targeted8 and specific static codegen passed']
q['jobs'][-1]['expected']='M2_FUNCTIONAL_GATES.json with current request/tool/launcher/source154/actual full+business+consumer+lifecycle closure; no dynamic-hit or performance admission'
with (d/'REQUESTS_M2_FULL.json').open('x',encoding='utf-8') as f:json.dump(q,f,indent=2,ensure_ascii=False);f.write('\n')
ready=json.loads((d/'READY.json').read_text(encoding='utf-8'))
ready['M2_full']='REQUESTS_M2_FULL.json: independent actual M2 full/SML/V1/consumer/lifecycle gate recipes with explicit61B launcher; R1 tools/requests untouched; review pending'
(d/'READY.json').write_text(json.dumps(ready,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'request':'REQUESTS_M2_FULL.json','tool_files':len(q['tool_files']),'jobs':len(q['jobs']),'R1_modified':False,'core_cpp_execution':0}))
