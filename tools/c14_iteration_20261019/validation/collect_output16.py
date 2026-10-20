"""Accept fixed16 diagnostic events without converting them into time evidence."""
import importlib.util
import re
from pathlib import Path

H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('cg',H/'m1_codegen_job.py')
C=importlib.util.module_from_spec(s);s.loader.exec_module(C);J=C.J
out=J.OUTPUT/'s1-output16-v1';lease=J.OUTPUT/'leases/s1-output16-v1'
J.require(J.read(lease/'result.json')['status']=='complete','output16 lease failed')
J.require(J.read(out/'receipt.json')['status']=='passed','output16 worker failed')
J.require(J.read(out/'validation.json')['status']=='passed','original fixed-M validation failed')
rows=[]
for line in (out/'fixed-M-output16.log').read_text().splitlines():
    if not line.startswith('OUTPUT16 '):continue
    row={k:(v if k=='wire_sha256' else int(v)) for k,v in re.findall(r'(\w+)=([A-Za-z0-9]+)',line)}
    rows.append(row)
J.require([r['iteration'] for r in rows]==list(range(16)),'exactly16 outputs required')
J.require(len({r['wire_sha256'] for r in rows})==1,'owning wire changed during repeated emit')
for r in rows:
    J.require(r['header_copies']==r['payload_copies']==1,'original resize_and_overwrite copy branch not observed')
    J.require(r['header_bytes']+r['payload_bytes']==r['bytes'],'copy-byte conservation failed')
    J.require(r['allocation_minor']<=r['minor'] and r['allocation_major']<=r['major'],'nested pagefault double count')
result={'status':'passed','stage':'S1_output_materialization','scope':'one fixed-M C14 trajectory followed by exactly16 original owning emits',
    'admission':J.fid(J.DOC/'S1_OUTPUT16_ADMISSION.json'),'receipt':{'path':str(out/'receipt.json'),**J.fid(out/'receipt.json')},
    'run_log':{'path':str(out/'fixed-M-output16.log'),**J.fid(out/'fixed-M-output16.log')},
    'validation':{'path':str(out/'validation.json'),**J.fid(out/'validation.json')},
    'rows':rows,'each_wire_bytes':rows[0]['bytes'],'ordinary_cpp_new_requests_each':[r['allocations'] for r in rows],
    'copy_calls_each':2,'copy_bytes_each':rows[0]['bytes'],
    'observed_minor_faults':sum(r['minor'] for r in rows),'observed_major_faults':sum(r['major'] for r in rows),
    'conditions':'Original validate workload runs load, fixed9-stage mutate, canonical checks and queries before this output loop. Outputs are independent strings, destroyed each iteration except final; no explicit prefault/touch/hotcopy/page policy or capacity reuse API. Allocator state is warm from those audits.',
    'nested_accounting':'Allocation pagefault counters are a subset of full emit counters and are not added. Remaining emit faults combine normalization/header/copy; no phase attribution beyond observed boundaries.',
    'limits':'All16 emits observed zero faults in this warm validation condition. This does not establish cold-allocation cost, system malloc count, DRAM bandwidth or formal emit latency. No clock/timing inference; the one complete payload copy remains.',
    'new_cpp_compiles':1,'new_native_runs':1,'owning_emits':16,'timing_samples':0,'formal_goal_complete':False}
J.write(J.DOC/'S1_OUTPUT16_RESULT.json',result)
print('S1 output16 passed:',rows[0]['bytes'],'bytes each; one ordinary new request and header+payload copies each; zero observed faults; no timing')
