"""Bind R1 codegen evidence, without admitting correctness or timing."""
import importlib.util
from pathlib import Path

H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('r1',H/'r1_codegen_job.py')
R=importlib.util.module_from_spec(s);s.loader.exec_module(R);J=R.J
a=J.read(R.A);q=J.read(R.Q)
J.require(J.read(R.LEASE/'result.json')['status']=='complete','R1 compile lease failed')
J.require(J.read(R.OUT/'codegen/receipt.json')['returncode']==0,'R1 compiler failed')
J.require(J.fid(R.Q)==a['request_identity'],'R1 request changed')
evidence={}
for role,path in [('control',Path(q['reference_control']['path'])),('candidate',R.OUT/'codegen/candidate-driver.s')]:
    lines=path.read_text().splitlines()
    start=next(i for i,l in enumerate(lines) if l.startswith('_ZN2sc10query_view') and ': #' in l)
    end=next(i for i in range(start+1,len(lines)) if lines[i].startswith('.Lfunc_end'))
    calls=[(start+i+1,l.strip()) for i,l in enumerate(lines[start:end]) if 'callq' in l and 'canonical_bytes_at' in l]
    loads=[(start+i+1,l.strip()) for i,l in enumerate(lines[start:end]) if 'canonical_active_count_' in l]
    evidence[role]={'assembly':{'path':str(path),**J.fid(path)},'query_symbol':lines[start].split(':')[0],
       'query_start_line':start+1,'query_assembly_lines':end-start,'canonical_calls':calls,'count_loads':loads}
    if role=='control':
        begin=next(i for i,l in enumerate(lines) if l.startswith('_ZN20XOffsetDatastructure11XBufferCore18canonical_bytes_atEPKvmm:'))
        load=next(i for i in range(begin,len(lines)) if 'canonical_active_count_' in lines[i])
        evidence[role]['pushes_before_ledger_load']=sum('pushq' in l for l in lines[begin:load])
        evidence[role]['helper_entry_excerpt']=lines[begin:load+4]
    else:
        evidence[role]['inactive_branch_excerpts']=[{'line':line-3,'assembly':lines[line-4:line+9]} for line,_ in loads[:3]]
J.require(len(evidence['control']['canonical_calls'])==16 and len(evidence['control']['count_loads'])==0,'unexpected control query code')
J.require(len(evidence['candidate']['canonical_calls'])==14 and len(evidence['candidate']['count_loads'])==14,'expected per-call inline ledger guard missing')
J.require(evidence['control']['pushes_before_ledger_load']==5,'baseline prologue differs')
result={'status':'passed','stage':'R1_codegen_opportunity','candidate':'r1-canonical-hotcold-v1',
    'request_identity':a['request_identity'],'candidate_members':a['members'],'input_files':a['inputs'],'evidence':evidence,
    'conclusion':'Actual query_view retains16 out-of-line canonical calls in C14 control. R1 has14 separately emitted acquire-ledger loads with zero branches bypassing14 active-helper calls. The original helper pays5 pushes before its zero test. Candidate inactive calls bypass those pushes; active helper remains out-of-line. No runtime hit frequency or speed established.',
    'cost_limit':'Candidate .s grows from1779270 to1840476 bytes and query assembly from944 to960 lines. These are assembly-text sizes, not executable text bytes or cycle costs; active-path and code-footprint costs need later review.',
    'actual_new_compiles':1,'reused_control_compiles':1,'new_native_runs':0,'timing_samples':0,
    'correctness_admitted':False,'screening_admitted':False,
    'next':'Core must register exact targeted normal/SAN plus one actual fixed-M active/inactive observation; no additional job inferred from this codegen-only request.'}
J.write(J.DOC/'R1_CODEGEN_RESULT.json',result)
print('R1 actual codegen opportunity passed; 1 new compile, no native or timing')
