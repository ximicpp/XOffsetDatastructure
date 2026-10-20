"""Accept exactly the eight registered native M1 correctness runs."""
import importlib.util
from pathlib import Path

H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('cg',H/'m1_codegen_job.py')
C=importlib.util.module_from_spec(s);s.loader.exec_module(C);J=C.J
out=C.OUT/'correctness'
lease=J.OUTPUT/'leases/m1-v2-correctness'
J.require(J.read(lease/'result.json')['status']=='complete','correctness lease did not complete')
J.require(J.read(C.OUT/'correctness-exit.json')['returncode']==0,'correctness worker failed')
q=J.read(C.QPATH);a=J.read(C.ADMISSION)
J.require(J.fid(C.QPATH)==a['request_identity'],'request changed')
commands=J.read(out/'commands.json')
tests=['test_scalar_resize','test_custom_future_lifetime','test_core_lifetime','test_m1_scalar_values']
expected=[profile+'-'+test for profile in ('normal','san') for test in tests]
J.require(len(commands)==16 and all(x['exit_code']==0 for x in commands),'expected 8 compile and 8 actual successful run records')
cases=[]
for i,name in enumerate(expected):
    compile,run=commands[2*i:2*i+2]
    J.require(compile['argv'][-1]==str(out/name) and run['argv']==[str(out/name)],'actual test inventory mismatch')
    J.require('-UNDEBUG' in compile['argv'] and compile['argv'].index('-UNDEBUG')>compile['argv'].index('-DNDEBUG'),'test assertion policy mismatch')
    J.require(('-fsanitize=address,undefined' in compile['argv'])==name.startswith('san-'),'sanitizer argv mismatch')
    cases.append({'name':name,'status':'passed','binary':J.fid(out/name),
                  'compile_log':J.fid(out/(name+'-compile.log')),'run_log':J.fid(out/(name+'-run.log')),
                  'compile':compile,'run':run})
result={'status':'passed','stage':'S2_targeted','candidate':'m1-default-scalar-v2',
        'request_identity':a['request_identity'],'candidate_identity_file':J.fid(Path(a['candidate_source']).parent/'IDENTITY.json'),
        'candidate_members':a['candidate_members'],'codegen_admission':J.fid(C.OUT/'CODEGEN_ADMISSION.json'),
        'lease_result':{'path':str(lease/'result.json'),**J.fid(lease/'result.json')},
        'cases':cases,'normal_passed':4,'san_passed':4,'actual_cpp_compiles':8,'actual_native_runs':8,
        'timing_samples':0,'scope':'Only the four registered test programs in two correctness profiles; full business/V1/consumer gates pending.'}
J.write(J.DOC/'M1_CORRECTNESS_RESULT.json',result)
print('M1 v2 targeted normal4/SAN4 passed; 8 compile + 8 native; no timing')
