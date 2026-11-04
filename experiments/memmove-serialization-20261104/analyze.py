"""Recover the completed fixed-budget window; never execute a benchmark."""
from pathlib import Path
import json,hashlib,statistics,math
from campaign import interval,ratio,PROCESSES,T975,MODES
HERE=Path(__file__).resolve().parent;OUT=HERE/'measurement-v1';ROOT=HERE.parent.parent
contract=json.loads((OUT/'preregister.json').read_text())
for name,expected in contract['identity'].items():
    assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==expected,name
phases={phase:[json.loads((OUT/f'{phase}-{i:02}.json').read_text()) for i in range(PROCESSES)] for phase in ['aa','comparison']}
assert len(list(OUT.glob('aa-*.json')))==12 and len(list(OUT.glob('comparison-*.json')))==12
cases=[]
for index in range(8):
    aa=[p['cases'][index] for p in phases['aa']];comp=[p['cases'][index] for p in phases['comparison']]
    key={k:comp[0][k] for k in ['bytes','nodes','holes','iterations']}
    assert all({k:c[k] for k in key}==key for c in aa+comp)
    assert all(len(v)==12 for c in aa+comp for v in c['samples'].values())
    assert all(x>0 for c in aa for m in ['raw_a','raw_b'] for x in c['samples'][m])
    aa_ratios=[statistics.median(b/a for a,b in zip(c['samples']['raw_a'],c['samples']['raw_b'])) for c in aa]
    aa_deltas=[statistics.median(b-a for a,b in zip(c['samples']['raw_a'],c['samples']['raw_b'])) for c in aa]
    ci=ratio(aa_ratios,T975);delta=interval(aa_deltas,T975)
    qualified=ci[1]>=.97 and ci[2]<=1.03 and (key['bytes']>4096 or delta[1]>=-15 and delta[2]<=15)
    result={**key,'aa_ratio_ci':ci,'aa_delta_ci_ns':delta,'qualified':qualified,'modes':{}}
    for mode in MODES:
        baseline='raw_slot' if mode=='image' else 'raw_a'
        times=[statistics.median(c['samples'][mode]) for c in comp]
        paired=[statistics.median(b/a for a,b in zip(c['samples'][baseline],c['samples'][mode])) for c in comp]
        diffs=[statistics.median(b-a for a,b in zip(c['samples'][baseline],c['samples'][mode])) for c in comp]
        zero=sum(x<=0 for c in comp for x in c['samples'][mode])
        rci=ratio(paired) if not zero else None;dci=interval(diffs)
        if mode in ['packed','image']:assert not zero,'strict-copy timing must be resolvable'
        gate=dci[2]<=50 if key['bytes']<=4096 else bool(rci and rci[2]<=1.10)
        result['modes'][mode]={'median_ns':statistics.median(times),'ratio_ci':rci,'delta_ci_ns':dci,'nonpositive_samples':zero,'strict_gate':bool(qualified and gate) if mode in ['packed','image'] else None}
    query_ratios=[]
    for c in comp:
        if any(x<=0 for m in ['native_query','proxy_query'] for x in c['samples'][m]):query_ratios=[];break
        query_ratios.append(statistics.median(b/a for a,b in zip(c['samples']['native_query'],c['samples']['proxy_query'])))
    result['proxy_to_native_query_ci']=ratio(query_ratios) if query_ratios else None
    result['prepare_ns']={k:statistics.median(c[k] for c in comp) for k in ['build_ns','freeze_ns','pack_ns','controlled_build_ns','seal_ns','context_prepare_ns']}
    cases.append(result)
summary={'analysis_recovery':'Original campaign collected all 24 processes then failed on log(0) for tiny diagnostic queries. This offline-only recovery changes no raw data, sample selection, budget, thresholds or strict-copy statistics; unresolved query ratios are null.',
 'cases':cases,'qualified_cases':sum(c['qualified'] for c in cases),'strict_all_cases':all(c['qualified'] and c['modes']['image']['strict_gate'] and c['modes']['packed']['strict_gate'] for c in cases),
 'process_peak_rss_bytes_range':[min(p['resources']['process_peak_rss_bytes'] for p in phases['comparison']),max(p['resources']['process_peak_rss_bytes'] for p in phases['comparison'])],
 'raw_files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in OUT.glob('*.json') if p.name!='summary-recovered.json'},'analyzer_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
(OUT/'summary-recovered.json').write_text(json.dumps(summary,indent=2))
print('QUALIFIED',summary['qualified_cases'],'ALL_STRICT',summary['strict_all_cases'])
for c in cases:
 print(c['bytes'],c['nodes'],c['holes'],'AA',c['aa_ratio_ci'],'QUAL',c['qualified'])
 for m in ['packed','image','dense_span','old_save','verified_reload','direct_reload','verified_read','image_read']:
  v=c['modes'][m];print(m,'ns',round(v['median_ns'],2),'ratio',v['ratio_ci'],'delta95',round(v['delta_ci_ns'][2],2),'gate',v['strict_gate'])
 print('PREP',c['prepare_ns'],'QUERY_RATIO',c['proxy_to_native_query_ci'])
