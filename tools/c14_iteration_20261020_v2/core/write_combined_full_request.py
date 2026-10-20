"""Register remaining combined gates; immutable initial jobs are never repeated."""
import hashlib,json
from pathlib import Path
R=Path(__file__).resolve().parents[3];H=Path(__file__).resolve().parent
D=R/'docs/coordination/c14-iteration-20261020-v2/core'
W='/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def fid(p):
 b=p.read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
def ref(p):return {'path':W+'/'+p.relative_to(R).as_posix(),'identity':fid(p)}
old=read(D/'REQUESTS_M2_FULL.json');q=json.loads(json.dumps(old).replace('m2-exact-owns-v1','r1-m2-combined-v1').replace('m2_full','combined_full').replace('m2-full','combined-full').replace('m2-business','combined-business').replace('M2','COMBINED'))
initial=D/'REQUESTS_COMBINED_INITIAL.json';iq=read(initial)
identity=R/'build/c14-iteration-20261020-v2/candidates/r1-m2-combined-v1/IDENTITY.json';i=read(identity)
q.update(candidate=i['candidate'],candidate_identity=fid(identity),source_members=i['members'],production14=iq['production14'],candidate_main_sha256=iq['production14']['xoffsetdatastructure.hpp']['sha256'],initial_request=ref(initial),selection=iq['selection'])
q.pop('same_frozen_COMBINED',None);q['root_selected_exact_two_patch_union']=True
q['initial_reuse']='First three exact initial jobs normal73/SAN73/external4 are separately registered and immutable. Require actual same-request successful results, never rerun them as part of remaining jobs.'
q['initial_external_authoritative_argv']=iq['jobs'][2]['argv']
q['initial_external_metadata_limit']='The immutable external request inherited an informational worker_argv pointing to the old M2 request. It is never read by run_native_targets; only initial.jobs[2].argv is the registered executor and correctly names REQUESTS_COMBINED_EXTERNAL.json. Do not execute the inherited informational field or any old M2 request. Actual commands/source154 are combined-only and are checked again by the finalizer.'
q['initial_expected_outputs']=['validation/combined_full/COMBINED_FULL_RESULT.json','/root/xoffset-c14-iteration/20261020/v2/validation/combined-external-v1/receipt.json']
q['jobs']=q['jobs'][2:]
q['jobs'][0]['argv']=['/usr/bin/python3','-I','-B',W+'/tools/c14_iteration_20261020_v2/core/prepare_combined_business.py']
q['jobs'][0]['depends_on']=['actual own immutable initial normal73/SAN73/external4 all passed and six repository source/compile/assertion/SAN/CTest/JUnit scopes verified']
q['jobs'][0]['expected']='COMBINED_INITIAL_GATES and honest COMBINED_FULL_SOURCE_BINDING, then retained original leased business prepare with request/initial/full/source154/14/launcher PREPARED bindings.'
q['jobs'][-1]['expected']='COMBINED_FUNCTIONAL_GATES.json binding own initial/full/targeted/SML/V1/consumer/runtime/lifecycle52 actual source+receipt+tool closure; no timing or QF admission.'
q['prerequisites']=['root FINAL_CANDIDATE_SELECTION and COMBINED_SOURCE_ACCEPTANCE exact identities','same combined initial actual full73x2 plus4 external programs and6 repo targeted case scopes, never standalone candidate substitutes','current A/compiler/libc++/runtime/dependency/host/backing/original lease admission']
q['C_dependency']='No new diagnostic/control/mechanism or72 experiment authorized; component mechanisms and retention are independently accepted. Actual combination performance remains unknown.'
q['source_binding_bridge']='validation/combined_full/COMBINED_FULL_SOURCE_BINDING.json distinguishes current complete request identity from immutable initial request identity, binds own actual initial/targeted gate result and selected source acceptance.'
q['all_new_outputs']='/root/xoffset-c14-iteration/20261020/v2/validation/combined-* and docs/coordination/c14-iteration-20261020-v2/validation/combined_full/'
q['control_reuse']='Read-only same-identity baseline/competitors/reference tools preserved; new actual combined candidate outputs only; no R1/M2/M1 native or full pass substitutes.'
q['final_binding']='Finalizer rechecks complete request/tool/outer61B, immutable initial, actual own full source154/JUnit/inventory/compileDB/CTest scopes, external4, bridge/PREPARED and all business/V1/package/runtime/matrix results.'
q['legacy_evidence_limit']='Earlier component wrapper scope remains disclosed. Every new combined job binds actual61B launcher and original current runtime closure before/after; no historical pin is invented.'
q['repo_targeted_scope_policy']='Six exact repository fixtures run in actual combined project full normal/SAN with O3, assertions, P2996/libc++/pthread and same tests/CMake/CTest/no-arg/failpoint source. Effective project C++26 dialect can be c++26 (manual old targeted used gnu++26); this difference is explicitly reported for independent scope review. Missing assertions/SAN/source/case/command scope stops prepare; only exact missing cases may be separately admitted, never silently skipped.'
files=[p for p in (H/'combined_full').rglob('*') if p.is_file() and '__pycache__' not in p.parts]
files += [H/n for n in ['combined_full_jobs.py','verify_combined_initial.py','prepare_combined_business.py','finalize_combined_full.py','run_native_targets.py','derive_combined_full.py','prepare_combined.py','write_combined_initial.py','write_combined_full_request.py','test_r1_thread_handoff_v2.cpp','test_m2_owns.cpp']]
files += [initial,D/'REQUESTS_COMBINED_EXTERNAL.json',identity,D/'COMBINED_SOURCE_PROOF.json',D/'COMBINED_SOURCE_REVIEW.md',D.parent/'reviews/COMBINED_SOURCE_ACCEPTANCE.json',D.parent/'reviews/FINAL_CANDIDATE_SELECTION.json']
q['tool_files']={W+'/'+p.relative_to(R).as_posix():fid(p) for p in sorted(set(files))}
q['tool_files'][q['compiler_launcher']['path']]=q['compiler_launcher']['identity']
q['tool_derivation']=ref(H/'combined_full/DERIVATION.json');q['review']='core/COMBINED_SOURCE_REVIEW.md and actual independent request/tool review before finite execution'
q['new_72_authorized']=False;q['new_diagnostic_authorized']=False
with (D/'REQUESTS_COMBINED_FULL.json').open('x',encoding='utf-8') as f:json.dump(q,f,indent=2,ensure_ascii=False);f.write('\n')
print(json.dumps({'request':fid(D/'REQUESTS_COMBINED_FULL.json'),'remaining_jobs':len(q['jobs']),'bound_tools_and_inputs':len(q['tool_files']),'initial_unchanged':True,'native':0}))
