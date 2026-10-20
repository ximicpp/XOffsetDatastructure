"""Write immutable finite initial recipes; no compile/native or queue launch."""
import copy,hashlib,json
from pathlib import Path
R=Path(__file__).resolve().parents[3];H=Path(__file__).resolve().parent
D=R/'docs/coordination/c14-iteration-20261020-v2/core'
W='/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发'
V='/root/xoffset-c14-iteration/20261020/v2/validation'
LAUNCHER='/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/bin/clang++-libcxx'
LID={'bytes':61,'sha256':'c8ea06a7c58557e3b89d09873dc88457d7b0c5d9af3f08cf0449093f2fbd1b87'}
RUNNER='/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def fid(p):
 b=p.read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
def wp(p):return W+'/'+p.relative_to(R).as_posix()
def ref(p):return {'path':wp(p),'identity':fid(p)}
def write(p,v):
 with p.open('x',encoding='utf-8') as f:json.dump(v,f,indent=2,ensure_ascii=False);f.write('\n')
i=R/'build/c14-iteration-20261020-v2/candidates/r1-m2-combined-v1/IDENTITY.json';identity=read(i);source=wp(i.parent/'source')
material=read(R/'handoff/github-20261019/C14_MATERIALIZATION.json');prod={n:{'bytes':identity['members'][n]['bytes'],'sha256':identity['members'][n]['candidate_sha256']} for n in material['production_files']}
oldm=read(D/'REQUESTS_M2_NATIVE.json');oldr=read(D/'REQUESTS_R1_NATIVE_v2.json')
external=copy.deepcopy(oldm);external.update(candidate=identity['candidate'],source=source,source_members=identity['members'],production_main_sha256=prod['xoffsetdatastructure.hpp']['sha256'],
 output=V+'/combined-external-v1',task_timeout_seconds=900,compile_budget=4,native_budget=4,commands=[],input_files={},candidate_modified=False,baseline_modified=False,codegen_recompiled=False)
for path,value in oldm['input_files'].items():
 if not path.startswith(W+'/') and not path.startswith(oldm['source']+'/'):external['input_files'][path]=value
for q,fixture in [(oldr,'test_r1_thread_handoff_v2.cpp'),(oldm,'test_m2_owns.cpp')]:
 for mode in ('normal','san'):
  job=next(j for j in q['commands'] if j['kind']=='compile' and j['label'].startswith(mode+'-') and any(x.endswith('/'+fixture) for x in j['argv']))
  argv=[x.replace(q['source'],source) for x in job['argv']];binary=external['output']+'/'+mode+'-'+Path(fixture).stem;argv[argv.index('-o')+1]=binary
  external['commands'] += [{'label':mode+'-'+Path(fixture).stem+'-compile','kind':'compile','timeout_seconds':120,'argv':argv},
                           {'label':mode+'-'+Path(fixture).stem+'-run','kind':'native','timeout_seconds':20,'argv':[binary]}]
  assert '-UNDEBUG' in argv and all(q['source'] not in x for x in argv)
  p=H/fixture;external['input_files'][wp(p)]=fid(p)
for n,v in identity['members'].items():external['input_files'][source+'/'+n]={'bytes':v['bytes'],'sha256':v['candidate_sha256']}
for p in (H/'run_native_targets.py',Path(__file__),i,D/'COMBINED_SOURCE_PROOF.json',D/'COMBINED_SOURCE_REVIEW.md',D.parent/'reviews/FINAL_CANDIDATE_SELECTION.json'):
 external['input_files'][wp(p)]=fid(p)
external['input_files'][LAUNCHER]=LID
external['coverage']='Only existing test_r1_thread_handoff_v2 and test_m2_owns, each normal/SAN:4 programs; six repository targeted require actual combined full/JUnit/compileDB scope binding.'
external['review_gate']='Combined exact source self-review and independent finite validation admission; no independent-candidate result substitution.'
external['next']='Own full73x2 + six repository targeted scope binding and complete business/V1/consumers/runtime/lifecycle; no new72/diagnostic/QF.'
ep=D/'REQUESTS_COMBINED_EXTERNAL.json';write(ep,external)
tools=[H/'combined_full_jobs.py',H/'combined_full/full_entry.py',H/'run_native_targets.py',Path(__file__),i,D/'COMBINED_SOURCE_PROOF.json',D/'COMBINED_SOURCE_REVIEW.md',ep,D.parent/'reviews/FINAL_CANDIDATE_SELECTION.json',D.parent/'validation/S0_RESULT.json']
tool_files={wp(p):fid(p) for p in tools};tool_files[LAUNCHER]=LID
q={'schema_version':1,'status':'ready_for_validation_review','candidate':identity['candidate'],'candidate_identity':fid(i),'source_members':identity['members'],'production14':prod,
 'selection':ref(D.parent/'reviews/FINAL_CANDIDATE_SELECTION.json'),'compiler_launcher':{'path':LAUNCHER,'identity':LID},'tool_files':tool_files,
 'external_request':ref(ep),'initial_tools_immutable':True,'later_business_tools_not_part_of_initial_closure':True,
 'jobs':[{'id':'COMBINED-FULL-'+m+'-v1','argv':['/usr/bin/python3','-I','-B',wp(H/'combined_full_jobs.py'),m],'timeout_seconds':2400,'expected':'Own actual73 inventory/JUnit/compileDB/source154 '+m} for m in ('normal','sanitized')]+
 [{'id':'COMBINED-EXTERNAL4-v1','argv':['/usr/bin/python3','-I','-B',RUNNER,'run','--owner','W1','--kind','validation','--out',V+'/combined-external-v1-lease','--timeout-seconds','900','--wait-seconds','0','--',
  '/usr/bin/python3','-I','-B',wp(H/'run_native_targets.py'),'--request',wp(ep)],'timeout_seconds':900,'expected':'Own two unchanged external fixtures normal/SAN;4 successful native and4 compiles'}],
 'execution_rule':'Invoke each dispatcher outside held lease; each acquires original shared supervisor. External run_native_targets runs only inside that one original leased session. No nested lease/new lock.',
 'six_repository_targeted':['test_canonical_zero_target','test_canonical_cold_no_callback','test_canonical_registry_v2','test_owner_identity','test_same_arena_move','test_locator_soundness'],
 'repo_targeted_rule':'Require actual combined fixture source identity, compile/assertion/SAN/subprocess/failpoint policy and matching73 JUnit/compileDB; supplement only explicitly identified missing scope under separate finite admission.',
 'failure_rule':'Stop/preserve prefix on failure; diagnose before any revision; no unregistered retry.',
 'scope':'Initial correctness only. Remaining complete functional request may be prepared independently without modifying these pinned tools/request. Completed exact initial results are referenced, never repeated.',
 'timing_admitted':False,'screen_admitted':False,'QF_admitted':False,'new_72_authorized':False,'new_diagnostic_authorized':False}
write(D/'REQUESTS_COMBINED_INITIAL.json',q)
print(json.dumps({'initial':fid(D/'REQUESTS_COMBINED_INITIAL.json'),'external':fid(ep),'initial_jobs':3,'programs_external':4,'core_execution':0}))
