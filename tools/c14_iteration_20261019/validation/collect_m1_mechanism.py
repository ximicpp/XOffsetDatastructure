"""Check the one fixed-M pair's native truth and nine-stage work conservation."""
import importlib.util
import re
from pathlib import Path

H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('cg',H/'m1_codegen_job.py')
C=importlib.util.module_from_spec(s);s.loader.exec_module(C);J=C.J
out=C.OUT/'mechanism';lease=J.OUTPUT/'leases/m1-v2-mechanism'
J.require(J.read(lease/'result.json')['status']=='complete','mechanism lease failed')
J.require(J.read(C.OUT/'mechanism-exit.json')['returncode']==0,'mechanism worker failed')
q=J.read(C.QPATH);a=J.read(C.ADMISSION)
J.require(J.fid(C.QPATH)==a['request_identity'],'request changed')
commands=J.read(out/'commands.json')
J.require(len(commands)==4 and all(x['exit_code']==0 for x in commands),'one compile and one native/control/candidate required')
rows={};artifacts={};truth={}
keys=['entries','capacity','tail','fresh','elements','binding_evaluations','bulk_elements']
for role in ('control','candidate'):
    log=out/(role+'-fixed-m.log')
    rows[role]=[{k:int(v) for k,v in re.findall(r'(stage|entries|capacity|tail|fresh|elements|binding_evaluations|bulk_elements)=(\d+)',line)} for line in log.read_text().splitlines() if line.startswith('M1 stage=')]
    J.require([r['stage'] for r in rows[role]]==list(range(9)),'exactly nine stage rows required')
    truth[role]=J.read(out/role/'validation.json')
    J.require(truth[role]['status']=='passed' and len(truth[role]['stages'])==9,'original native truth failed')
    artifacts[role]={n:{'path':str(out/role/n),**J.fid(out/role/n)} for n in ('validation.json','DIAGNOSTIC_IDENTITY.json','diagnostic_driver.cpp','fixed-m')}
    artifacts[role]['run_log']={'path':str(log),**J.fid(log)}
J.require(truth['control']==truth['candidate'],'canonical nine-stage native truth mismatch')
for left,right in zip(rows['control'],rows['candidate']):
    J.require(all(left[k]==right[k] for k in ('entries','capacity','tail','fresh','elements')),'logical path work changed')
    J.require(left['binding_evaluations']==left['elements'] and right['binding_evaluations']==0 and right['bulk_elements']==left['elements'],'targeted work conservation failed')
totals={role:{k:sum(r[k] for r in rr) for k in keys} for role,rr in rows.items()}
J.require(totals['control']['elements']>0,'fixed M never reached M1')
result={'status':'passed','stage':'S1/S2_M1_mechanism','candidate':'m1-default-scalar-v2',
    'request_identity':a['request_identity'],'full_candidate_members':a['candidate_members'],
    'codegen_admission':J.fid(C.OUT/'CODEGEN_ADMISSION.json'),
    'rows':rows,'totals':totals,'artifacts':artifacts,'commands':commands,
    'lease_result':{'path':str(lease/'result.json'),**J.fid(lease/'result.json')},
    'native_truth':'Original validate path checks fixed input canonical, before/after query answers, all9 stage canonical digests, owning emit and fully checked reload.',
    'conclusion':'Fixed M reaches fresh scalar path and candidate replaces per-element binding with typed bulk initialization. Counts do not establish cycles, elapsed benefit, full business gates or screening admission.',
    'actual_cpp_compiles':2,'actual_native_runs':2,'timing_samples':0,'screening_admitted':False}
J.write(J.DOC/'M1_MECHANISM_RESULT.json',result)
print('M1 fixed-M mechanism passed:',totals)
