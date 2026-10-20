"""New M1 full normal/SAN inventory through the existing targeted CMake entry."""
import importlib.util
import datetime
from pathlib import Path
import subprocess
import sys

H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('cg',H/'m1_codegen_job.py')
C=importlib.util.module_from_spec(s);s.loader.exec_module(C);J=C.J
material=J.read(J.ROOT/'handoff/github-20261019/C14_MATERIALIZATION.json')
a=J.read(C.ADMISSION)
J.require(J.fid(C.QPATH)==a['request_identity'],'request changed')
J.require(J.read(J.DOC/'M1_MECHANISM_RESULT.json')['status']=='passed','M1 mechanism required')
inv=J.read(Path('/root/xoffset-receive-completion/20261019/v1/full-final-v4-normal-build/test-inventory.json'))
tests=inv['runtime_test_programs']+inv['subprocess_scenarios']+inv['tooling_checks']
for mode in ('normal','sanitized'):
    prod={n:{'bytes':a['candidate_members'][n]['bytes'],'sha256':a['candidate_members'][n]['candidate_sha256']} for n in material['production_files']}
    overlays={n:{'path':str(Path(a['candidate_source'])/n),'identity':prod[n]} for n in prod if prod[n]!=material['production_files'][n]}
    job={'schema_version':1,'stage':'S2_targeted','candidate':'M1','job_id':'m1-full-v2-'+mode,'mode':mode,
       'baseline_main_sha256':material['production_files']['xoffsetdatastructure.hpp']['sha256'],'production_files':prod,
       'overlays':overlays,'targets':inv['runtime_test_programs'],'tests':tests,
       'runner_identity':J.fid(J.RUNNER),'entry_identity':J.fid(H/'candidate_job.py'),
       'tool_files':{str(p):J.fid(p) for p in J.FIXED_TOOLS},
       'backing_free_bytes':__import__('shutil').disk_usage('/mnt/c').free,
       'backing_observed_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
    request=J.DOC/('M1_FULL_'+mode.upper()+'_REQUEST_r2.json');J.write(request,job)
    r=subprocess.run(['/usr/bin/python3','-I','-B',str(H/'candidate_job.py'),'run','--request',str(request)])
    J.require(r.returncode==0,'full '+mode+' failed; preserve and diagnose')
    receipt=J.read(J.OUTPUT/job['job_id']/'receipt.json')
    J.require(receipt['status']=='passed' and sorted(receipt['junit_names'])==sorted(tests),'full actual inventory failed')
J.write(J.DOC/'M1_FULL_RESULT.json',{'status':'passed','candidate':'m1-default-scalar-v2','request_identity':a['request_identity'],
       'normal_count':len(tests),'sanitized_count':len(tests),'production_files':prod,
       'normal_receipt':{'path':str(J.OUTPUT/'m1-full-v2-normal/receipt.json'),**J.fid(J.OUTPUT/'m1-full-v2-normal/receipt.json')},
       'sanitized_receipt':{'path':str(J.OUTPUT/'m1-full-v2-sanitized/receipt.json'),**J.fid(J.OUTPUT/'m1-full-v2-sanitized/receipt.json')},
       'performance_samples':0})
print('M1 full normal/SAN passed',len(tests),'each',flush=True)
