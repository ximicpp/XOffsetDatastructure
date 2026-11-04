"""One bounded, preregistered measurement campaign. Never retry samples."""
from pathlib import Path
import subprocess,json,time,hashlib,statistics,math,os
HERE=Path(__file__).resolve().parent
OUT=HERE/'measurement-v1'
PROCESSES=12
BLOCKS=12
T95=1.795884819
T975=2.200985160
MODES=['packed','image','dense_span','old_save','verified_reload','direct_reload','verified_read','image_read','native_query','proxy_query']
def interval(values,t=T95):
    mean=statistics.mean(values);half=t*statistics.stdev(values)/math.sqrt(len(values))
    return mean,mean-half,mean+half
def ratio(values,t=T95):return [math.exp(v) for v in interval([math.log(v) for v in values],t)]
def run():
    OUT.mkdir(exist_ok=False)
    lock=HERE/'measurement.lock'
    fd=os.open(lock,os.O_CREAT|os.O_EXCL|os.O_WRONLY);os.write(fd,str(os.getpid()).encode());os.close(fd)
    root=HERE.parent.parent
    identity={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'xoffsetdatastructure.hpp',root/'xoffset_dense_export.hpp',root/'xoffset_image_view.hpp',root/'xoffset_image_builder.hpp',HERE/'bench.cpp',HERE/'campaign.py',HERE/'perf/bin/Release/bench_memmove']}
    contract={'processes_per_phase':PROCESSES,'blocks_per_process':BLOCKS,'ordering':'forward/reverse alternates by block and process; all cases in fixed order',
      'pages':'preallocated and touched; warmup 64 operations per mode; hot pages only','a_a_gate':'95% two-sided process CI within [0.97,1.03]; small-message absolute difference CI within +/-15ns',
      'ratio_gate':'per-case process-paired one-sided95 upper <=1.10 for >=64KiB','small_gate':'one-sided95 upper of extra latency <=50ns for <=4KiB',
      'comparison':'12 AA processes then 12 comparison processes; fixed budget. Comparisons for failed AA cases are diagnostic only; no retries or extra samples',
      'image_baseline':'raw_slot writes the identical destination as image; packed/dense use raw_a destination',
      'scope':'same-process immutable provenance; proxy contract differs from native mutable verified API; preparation and query costs reported separately','identity':identity}
    (OUT/'preregister.json').write_text(json.dumps(contract,indent=2))
    try:
        phases={}
        for phase in ['aa','comparison']:
            phases[phase]=[]
            for i in range(PROCESSES):
                path=OUT/f'{phase}-{i:02}.json'
                t=time.time()
                with path.open('w') as out, (OUT/f'{phase}-{i:02}.stderr').open('w') as err:
                    r=subprocess.run([str(HERE/'perf/bin/Release/bench_memmove'),phase,str(i)],stdout=out,stderr=err)
                if r.returncode:raise RuntimeError(f'{phase} process {i} exit {r.returncode}; no retry')
                data=json.loads(path.read_text());assert len(data['cases'])==8
                for case in data['cases']:assert all(len(v)==BLOCKS for v in case['samples'].values())
                phases[phase].append(data)
                print(json.dumps({'phase':phase,'process':i,'seconds':time.time()-t}),flush=True)
                time.sleep(1)
        cases=[]
        for index in range(8):
            aa=[p['cases'][index] for p in phases['aa']];comp=[p['cases'][index] for p in phases['comparison']]
            key={k:comp[0][k] for k in ['bytes','nodes','holes','iterations']}
            aa_ratios=[statistics.median(b/a for a,b in zip(c['samples']['raw_a'],c['samples']['raw_b'])) for c in aa]
            aa_deltas=[statistics.median(b-a for a,b in zip(c['samples']['raw_a'],c['samples']['raw_b'])) for c in aa]
            ci=ratio(aa_ratios,T975);delta=interval(aa_deltas,T975)
            qualified=ci[1]>=.97 and ci[2]<=1.03 and (key['bytes']>4096 or delta[1]>=-15 and delta[2]<=15)
            result={**key,'aa_ratio_ci':ci,'aa_delta_ci_ns':delta,'qualified':qualified,'modes':{}}
            for mode in MODES:
                baseline='raw_slot' if mode=='image' else 'raw_a'
                times=[statistics.median(c['samples'][mode]) for c in comp]
                ratios=[statistics.median(b/a for a,b in zip(c['samples'][baseline],c['samples'][mode])) for c in comp]
                diffs=[statistics.median(b-a for a,b in zip(c['samples'][baseline],c['samples'][mode])) for c in comp]
                rci=ratio(ratios);dci=interval(diffs)
                gate=dci[2]<=50 if key['bytes']<=4096 else rci[2]<=1.10
                result['modes'][mode]={'median_ns':statistics.median(times),'ratio_ci':rci,'delta_ci_ns':dci,'strict_gate':bool(qualified and gate) if mode in ['packed','image'] else None}
            result['prepare_ns']={k:statistics.median(c[k] for c in comp) for k in ['build_ns','freeze_ns','pack_ns','controlled_build_ns','seal_ns','context_prepare_ns']}
            cases.append(result)
        summary={'cases':cases,'qualified_cases':sum(c['qualified'] for c in cases),
          'strict_all_cases':all(c['qualified'] and c['modes']['image']['strict_gate'] and c['modes']['packed']['strict_gate'] for c in cases),
          'raw_files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in OUT.glob('*.json') if p.name!='summary.json'}}
        (OUT/'summary.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary),flush=True)
    finally:lock.unlink()
if __name__=='__main__':run()
