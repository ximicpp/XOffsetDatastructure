"""Bind actual immutable initial results, then use retained business prepare."""
import importlib.util,json,subprocess,sys
from pathlib import Path
H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('combined_initial_binding',H/'verify_combined_initial_v2.py');M=importlib.util.module_from_spec(s);s.loader.exec_module(M)
R,D,INITIAL,verify,read,fid,ref,need=M.R,M.D,M.Q,M.verify,M.read,M.fid,M.ref,M.need
Q=R/'docs/coordination/c14-iteration-20261020-v2/core/REQUESTS_COMBINED_FULL.json'
def write_once(p,v):
 if p.exists():need(read(p)==v,'prepared bridge identity changed: '+str(p));return
 with p.open('x',encoding='utf-8') as f:json.dump(v,f,indent=2);f.write('\n')
def main():
 q=read(Q)
 for p,v in q['tool_files'].items():need(fid(p)==v,'combined final tool changed: '+p)
 need(fid(INITIAL)==q['initial_request']['identity'],'initial request changed')
 gates=verify();gp=D/'COMBINED_INITIAL_GATES_v2.json';write_once(gp,gates)
 ident=R/'build/c14-iteration-20261020-v2/candidates/r1-m2-combined-v1/IDENTITY.json';i=read(ident)
 source_acceptance=R/'docs/coordination/c14-iteration-20261020-v2/reviews/COMBINED_SOURCE_ACCEPTANCE.json'
 bridge={'status':'passed','scope':'Honest combined source/input bridge to actual own initial gates and root selected source; no independent R1/M2 native/codegen substitute',
  'candidate_identity_file':fid(ident),'candidate_members':i['members'],'production_files':{'candidate':q['production14']},'request_identity':fid(Q),'initial_request_identity':fid(INITIAL),
  'initial_gates':ref(gp),'corrective_addendum':ref(M.CORRECTIVE),'source_acceptance':ref(source_acceptance),'selection':q['selection'],'current_A':ref(D/'S0_RESULT.json'),'compiler_launcher':q['compiler_launcher']}
 write_once(D/'COMBINED_FULL_SOURCE_BINDING.json',bridge)
 result=subprocess.run(['/usr/bin/python3','-I','-B',str(H/'combined_full/business/dispatch.py'),'prepare'],cwd=R)
 if result.returncode:raise SystemExit(result.returncode)
if __name__=='__main__':main()
