"""Exercise actual preflight failures; never starts a build or C++ process."""
import datetime
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('candidate_job',HERE/'candidate_job.py')
job=importlib.util.module_from_spec(spec); spec.loader.exec_module(job)
out=job.DOC/'entry-check-v2'; out.mkdir(exist_ok=False)
material=job.read(job.ROOT/'handoff/github-20261019/C14_MATERIALIZATION.json')
windows=job.read(job.DOC/'windows-host.json')
q={'schema_version':1,'stage':'S2_targeted','candidate':'M1','job_id':'preflight-only-never-run',
   'mode':'normal','baseline_main_sha256':material['production_files']['xoffsetdatastructure.hpp']['sha256'],
   'production_files':material['production_files'],'overlays':{},'targets':['test_scalar_resize'],
   'tests':['test_scalar_resize'],'runner_identity':job.fid(job.RUNNER),
   'entry_identity':job.fid(HERE/'candidate_job.py'),
   'tool_files':{str(p):job.fid(p) for p in job.FIXED_TOOLS},
   'backing_free_bytes':next(d['Free'] for d in windows['disks'] if d['Name']=='C'),
   'backing_observed_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
results=[]
for name,change,success,reason in [
    ('valid-preflight',{},True,''),
    ('empty-test-inventory',{'tests':[]},False,'explicit unique tests required'),
    ('wrong-baseline',{'baseline_main_sha256':'0'*64},False,'wrong baseline'),
    ('source-byte-mismatch',{'overlays':{'xoffsetdatastructure.hpp':{'path':str(job.ROOT/'xoffsetdatastructure.hpp'),'identity':{'bytes':1,'sha256':'0'*64}}}},False,'changed source input'),
    ('stale-backing-observation',{'backing_observed_utc':'2020-01-01T00:00:00+00:00'},False,'refresh actual Windows backing')]:
    path=out/(name+'.json'); job.write(path,dict(q,**change))
    r=subprocess.run([sys.executable,'-I','-B',str(HERE/'candidate_job.py'),'check','--request',str(path)],capture_output=True,text=True,timeout=30)
    passed=(r.returncode==0)==success and (success or reason in r.stderr)
    results.append({'case':name,'passed':passed,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
job.write(out/'result.json',{'status':'passed' if all(r['passed'] for r in results) else 'rejected',
          'cases':results,'new_cpp_compiles':0,'new_cpp_executions':0,'leases_acquired':0})
print(json.dumps({'status':'passed' if all(r['passed'] for r in results) else 'rejected','cases':len(results),'out':str(out)}))
raise SystemExit(0 if all(r['passed'] for r in results) else 1)
