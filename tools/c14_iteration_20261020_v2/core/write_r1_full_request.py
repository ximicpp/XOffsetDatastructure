"""Register exact R1 full/consumer worker entry points without running them."""
import hashlib
import json
from pathlib import Path
r=Path(__file__).resolve().parents[3]
here=Path(__file__).resolve().parent
docs=r/'docs/coordination/c14-iteration-20261020-v2/core'
w='/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发'
def fid(p):
    b=p.read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
identity_path=r/'build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1/IDENTITY.json'
identity=json.loads(identity_path.read_text(encoding='utf-8'))
mat=json.loads((r/'handoff/github-20261019/C14_MATERIALIZATION.json').read_text(encoding='utf-8'))
prod={n:{'bytes':identity['members'][n]['bytes'],'sha256':identity['members'][n]['candidate_sha256']} for n in mat['production_files']}
files=[p for p in (here/'r1_full').rglob('*') if p.is_file() and '__pycache__' not in p.parts]
files += [here/'r1_full_jobs.py',here/'finalize_r1_full.py',here/'derive_r1_full.py',Path(__file__)]
bindings={w+'/'+p.relative_to(r).as_posix():fid(p) for p in sorted(files)}
tool=w+'/tools/c14_iteration_20261020_v2/core'
entry=tool+'/r1_full'
jobs=[{'id':'R1-FULL-normal-v1','depends_on':['actual R1 B10 and retained codegen passed'],
       'argv':['/usr/bin/python3','-I','-B',tool+'/r1_full_jobs.py','normal'],'timeout_seconds':2400,
       'expected':'new r1-full-v1-normal receipt/source154/deps/build/inventory/JUnit73, three configure checks separate'},
      {'id':'R1-FULL-sanitized-v1','depends_on':['R1-FULL-normal-v1'],
       'argv':['/usr/bin/python3','-I','-B',tool+'/r1_full_jobs.py','sanitized'],'timeout_seconds':2400,
       'expected':'new r1-full-v1-sanitized receipt/JUnit73 and R1_FULL_RESULT.json binding both actual inventories'}]
for stage,timeout in [('prepare',300),('bench',2400),('functional',None),('package',2400),('runtime',2400)]:
    jobs.append({'id':'R1-BUSINESS-'+stage+'-v1','depends_on':[jobs[-1]['id']],
        'argv':['/usr/bin/python3','-B',entry+'/business/dispatch.py',stage],
        'timeout_seconds':timeout,
        'budget_note':'functional dispatch retains sequential normal1800/audit600/SAN1800/audit600/V1compat1500 original jobs, not a nested supervisor' if timeout is None else 'retained finite stage budget',
        'expected':{'prepare':'source154/14/full73x2 metadata identity, exact original adapter/protocol identity-literal derivation, fixed vendor/input binding; zero native',
                    'bench':'new R1 normal/SAN candidate original-driver and gates; exact existing control profiles referenced without rebuilding them',
                    'functional':'S/M/L 80 normal+80 SAN, original per-stage truth and V1 main84+extra8, AB4/C4 bidirectional actions',
                    'package':'storage/full installed/subproject/relocated-prefix/archive C consumption and actual dependencies',
                    'runtime':'both existing shared-runtime consumers and their dependency/ABI/state checks'}[stage]})
jobs += [{'id':'R1-LIFECYCLE52-v1','depends_on':[jobs[-1]['id']],
          'argv':['/usr/bin/python3','-B',entry+'/business_matrix/dispatch.py','run'],
          'timeout_seconds':3600,'expected':'25 normal+25 SAN models+2 small-pool probes, source/author/reference/tool closure and original semantics/counts'},
         {'id':'R1-FINAL-FUNCTIONAL-v1','depends_on':['R1-LIFECYCLE52-v1'],
          'argv':['/usr/bin/python3','-I','-B',tool+'/finalize_r1_full.py'],
          'timeout_seconds':120,'expected':'R1_FUNCTIONAL_GATES.json referencing only actual new R1 full/business/consumer/lifecycle passes; no performance admission'}]
q={'schema_version':1,'status':'ready_for_validation_review','candidate':identity['candidate'],
   'candidate_identity':fid(identity_path),'source_members':identity['members'],'production14':prod,
   'baseline_main_sha256':'66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3',
   'candidate_main_sha256':prod['xoffsetdatastructure.hpp']['sha256'],
   'source_modified':False,'same_frozen_R1':True,'timing_admitted':False,'screen_admitted':False,'QF_admitted':False,
   'tool_files':bindings,'tool_derivation':{'path':entry+'/DERIVATION.json','identity':fid(here/'r1_full/DERIVATION.json')},
   'review':'core/REVIEW_R1_FULL.md actual final tools/request review required before first launch',
   'prerequisites':['v2 A current source154/14/runtime/dependency/lease/backing passed','actual R1 B10 passed and existing R1 codegen source identity matches'],
   'C_dependency':'No C diagnostic prerequisite for independent functional work; if C fails for a tool precondition, these safely bound gates may proceed as orchestrator directs. No C failure is erased or retried.',
   'source_binding_bridge':'validation/r1_full/R1_FULL_SOURCE_BINDING.json is explicitly source/input metadata referencing existing actual codegen and B10, not a synthesized test result',
   'full_result':'validation/r1_full/R1_FULL_RESULT.json is created only after the two new actual inventory/JUnit receipts pass',
   'all_new_outputs':'/root/xoffset-c14-iteration/20261020/v2/validation/r1-* and docs/coordination/c14-iteration-20261020-v2/validation/r1_full/',
   'global_runner':'/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py',
   'execution_rule':'Invoke these dispatchers outside the lease; their retained original entrypoints acquire the single real global lock and supervise one session. Never wrap an already dispatching coordinator in another held lease.',
   'resource_rule':'Retained full entry checks current WSL/backing8GiB and <=15min timestamp per request; current host/backing/RSS and original business/package/matrix limits remain required at validation admission. No historical evidence is deleted.',
   'control_reuse':'Same-profile control toolchain/dependency/full-control and original pinned V1/reference artifacts remain read-only exact references; candidate R1 builds/tests are new. M1 passed status never substitutes for R1.',
   'failure_rule':'One fresh identity/output per listed gate. Preserve failure, diagnose before any source/tool revision. No retries or performance-window admission is created by this functional request.',
   'jobs':jobs}
with (docs/'REQUESTS_R1_FULL.json').open('x',encoding='utf-8') as f:json.dump(q,f,indent=2,ensure_ascii=False);f.write('\n')
ready=json.loads((docs/'READY.json').read_text(encoding='utf-8'))
ready['R1_full']='REQUESTS_R1_FULL.json: exact retained-tool derivation and finite new R1 full/SML/V1/package/runtime/lifecycle requests ready for validation review; no C dependency or performance admission'
(docs/'READY.json').write_text(json.dumps(ready,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'request':'REQUESTS_R1_FULL.json','tool_files':len(bindings),'jobs':len(jobs),'native_executed_by_core':0}))
