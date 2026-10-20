"""Record independently inspected original-driver M1 codegen and full v2 binding."""
import importlib.util
import re
from pathlib import Path

HERE=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('cg',HERE/'m1_codegen_job.py')
C=importlib.util.module_from_spec(s);s.loader.exec_module(C)
J=C.J
a=J.read(C.ADMISSION);q=J.read(C.QPATH)
J.require(J.fid(C.QPATH)==a['request_identity'],'request changed')
J.require(J.read(C.LEASE/'result.json')['status']=='complete','codegen lease did not complete')
commands=J.read(C.OUT/'codegen/commands.json')
J.require(len(commands)==2 and all(x['exit_code']==0 and '-O3' in x['argv'] and '-DNDEBUG' in x['argv'] and '-UNDEBUG' not in x['argv'] and '-S' in x['argv'] for x in commands),'not two successful bare -S compilations')
for p,v in {**a['tool_files'],**a['experiment_files']}.items():J.require(J.fid(p)==v,'input changed: '+p)
material=J.read(J.ROOT/'handoff/github-20261019/C14_MATERIALIZATION.json')
production={}
for role,source in [('control',J.VENDOR),('candidate',C.OUT/'staged/source')]:
    production[role]={n:J.fid(source/n) for n in material['production_files']}
    for n,v in a['candidate_members'].items():
        if role=='candidate':J.require(J.fid(source/n)=={'bytes':v['bytes'],'sha256':v['candidate_sha256']},'staged member changed: '+n)
    if role=='control':J.require(production[role]==material['production_files'],'baseline production changed')
evidence={}
for role in ('control','candidate'):
    p=C.OUT/'codegen'/(role+'-driver.s');lines=p.read_text().splitlines()
    evidence[role]={'assembly':{'path':str(p),**J.fid(p)},'scalar_functions':{}}
    for ty in ('j','h'):
        start=next(i for i,l in enumerate(lines) if l.startswith('_ZN20XOffsetDatastructure12XFixedVectorI'+ty+'E16resize_construct') and ': #' in l)
        end=next(i for i in range(start+1,len(lines)) if lines[i].startswith('.Lfunc_end'))
        block=lines[start:end]
        label='.LBB128_' if ty=='j' else '.LBB131_'
        fresh_start=next(i for i,l in enumerate(block) if l==label+'26:')
        fresh_end=next(i for i in range(fresh_start+1,len(block)) if block[i]==label+('30:' if role=='control' else '28:'))
        fresh=block[fresh_start:fresh_end]
        calls=[l.strip() for l in fresh if 'callq' in l]
        J.require(any('bindingEv' in l for l in calls) if role=='control' else calls==['callq\tmemset@PLT'],'unexpected fresh scalar call closure')
        evidence[role]['scalar_functions']['uint32' if ty=='j' else 'uint8']={'symbol':lines[start].split(':')[0],
           'function_start_line':start+1,'fresh_start_line':start+fresh_start+1,'fresh_end_line':start+fresh_end,
           'fresh_calls':calls,'fresh_assembly':fresh}
decision={'status':'passed','scope':'bare codegen opportunity only; actual fixed-M hit and safety/normal/SAN remain separate',
    'candidate_main_sha256':q['main_sha256'],'request_identity':a['request_identity'],
    'candidate_identity_file':J.fid(Path(a['candidate_source']).parent/'IDENTITY.json'),
    'candidate_members':a['candidate_members'],'production_files':production,
    'job_admission':J.fid(C.ADMISSION),'tools':a['tool_files'],'experiment_files':a['experiment_files'],
    'codegen':evidence,'commands':commands,'lease_result':{'path':str(C.LEASE/'result.json'),**J.fid(C.LEASE/'result.json')},
    'conclusion':'Both actual uint8/uint32 fresh suffix loops retain a binding call per element in control. Candidate fresh suffix uses one compiler-generated memset for typed value initialization. Existing capacity/general paths retain binding; prefix relocation/retirement remain. Binding callee contains actual canonical_bytes_at calls. No timing benefit established.',
    'v1_v2_distinction':'Admission binds all154 members and all14 production identities, including v2 xoffset_core_lifetime.hpp; shared main hash alone is insufficient.',
    'new_cpp_compiles':2,'native_executions':0,'timing_samples':0}
J.write(C.OUT/'CODEGEN_ADMISSION.json',decision)
J.write(J.DOC/'M1_CODEGEN_RESULT.json',decision)
print('M1 v2 bare codegen mechanism opportunity passed; full closure bound; 2 compiles, 0 native, 0 timing')
