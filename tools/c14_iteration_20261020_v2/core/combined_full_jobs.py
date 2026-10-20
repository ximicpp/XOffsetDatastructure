"""Combined-only initial full73 normal/SAN; original supervisor owns execution."""
import argparse,datetime,importlib.util,shutil,sys
from pathlib import Path
R=Path(__file__).resolve().parents[3];H=Path(__file__).resolve().parent/'combined_full'
spec=importlib.util.spec_from_file_location('combined_full_entry',H/'full_entry.py');J=importlib.util.module_from_spec(spec);spec.loader.exec_module(J)
D=R/'docs/coordination/c14-iteration-20261020-v2/validation/combined_full'
Q=R/'docs/coordination/c14-iteration-20261020-v2/core/REQUESTS_COMBINED_INITIAL.json'
I=R/'build/c14-iteration-20261020-v2/candidates/r1-m2-combined-v1/IDENTITY.json'
WRAPPER=Path('/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/bin/clang++-libcxx')
WID={'bytes':61,'sha256':'c8ea06a7c58557e3b89d09873dc88457d7b0c5d9af3f08cf0449093f2fbd1b87'}
def ref(p):return {'path':str(p),'identity':J.fid(p)}
def prepare():
 q=J.read(Q)
 J.require(q['status']=='ready_for_validation_review' and not q['timing_admitted'],'finite combined initial request required')
 for p,v in q['tool_files'].items():J.require(J.fid(p)==v,'initial tool/input changed: '+p)
 J.require(J.fid(I)==q['candidate_identity'] and J.fid(WRAPPER)==q['compiler_launcher']['identity']==WID,'combined identity/outer launcher changed')
 identity=J.read(I);source=I.parent/'source'
 for n,v in identity['members'].items():J.require(J.fid(source/n)=={'bytes':v['bytes'],'sha256':v['candidate_sha256']},'combined source changed: '+n)
 selection=R/'docs/coordination/c14-iteration-20261020-v2/reviews/FINAL_CANDIDATE_SELECTION.json'
 J.require(J.fid(selection)==q['selection']['identity'] and J.read(selection)['selected_candidate']==identity['candidate'],'root final selection changed')
 for v in J.read(selection)['components']:J.require(J.fid(v['path'])==v['identity'],'component retention changed')
 material=J.read(R/'handoff/github-20261019/C14_MATERIALIZATION.json');prod={n:{'bytes':identity['members'][n]['bytes'],'sha256':identity['members'][n]['candidate_sha256']} for n in material['production_files']}
 J.require(prod==q['production14'],'combined production14 differs')
 D.mkdir(parents=True,exist_ok=True);s0=R/'docs/coordination/c14-iteration-20261020-v2/validation/S0_RESULT.json';J.require(J.read(s0)['status']=='passed','current S0 required')
 if (D/'S0_RESULT.json').exists():J.require(J.fid(D/'S0_RESULT.json')==J.fid(s0),'S0 bridge changed')
 else:shutil.copyfile(s0,D/'S0_RESULT.json')
 return identity,prod,material,source
def main():
 p=argparse.ArgumentParser();p.add_argument('mode',choices=('normal','sanitized'));a=p.parse_args();identity,prod,material,source=prepare()
 inv=J.read(Path('/root/xoffset-receive-completion/20261019/v1/full-final-v4-normal-build/test-inventory.json'));tests=inv['runtime_test_programs']+inv['subprocess_scenarios']+inv['tooling_checks'];J.require(len(tests)==73,'original73 inventory required')
 child={'schema_version':1,'stage':'S2_targeted','candidate':'COMBINED','job_id':'combined-full-v1-'+a.mode,'mode':a.mode,
  'baseline_main_sha256':material['production_files']['xoffsetdatastructure.hpp']['sha256'],'production_files':prod,
  'overlays':{n:{'path':str(source/n),'identity':v} for n,v in prod.items() if v!=material['production_files'][n]},'targets':inv['runtime_test_programs'],'tests':tests,
  'runner_identity':J.fid(J.RUNNER),'entry_identity':J.fid(H/'full_entry.py'),'tool_files':{str(p):J.fid(p) for p in J.FIXED_TOOLS},'compiler_launcher':ref(WRAPPER),
  'backing_free_bytes':shutil.disk_usage('/mnt/c').free,'backing_observed_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
 path=D/('COMBINED_FULL_'+a.mode.upper()+'_REQUEST_v1.json');J.write(path,child);sys.argv=[str(H/'full_entry.py'),'run','--request',str(path)]
 try:J.main()
 except SystemExit as e:
  if e.code not in (None,0):raise
 J.require(J.fid(WRAPPER)==WID,'outer launcher changed after full')
 receipts=[J.OUTPUT/('combined-full-v1-'+m)/'receipt.json' for m in ('normal','sanitized')]
 if all(p.exists() for p in receipts):
  for p in receipts:
   v=J.read(p);J.require(v['status']=='passed' and sorted(v['junit_names'])==sorted(tests) and v['production_files']==prod,'combined full actual result differs')
  J.write(D/'COMBINED_FULL_RESULT.json',{'status':'passed','candidate':identity['candidate'],'request_identity':J.fid(Q),'normal_count':73,'sanitized_count':73,'production_files':prod,
   'normal_receipt':ref(receipts[0]),'sanitized_receipt':ref(receipts[1]),'compiler_launcher':ref(WRAPPER),'performance_samples':0})
if __name__=='__main__':main()
