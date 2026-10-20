def summary(samples):
    need(len(samples)==144 and [s['job'] for s in samples]==schedule(),'complete exact role/schedule144 required')
    pairs={i:{} for i in range(72)}
    for sample in samples:
        job=sample['job'];values=sample['values'];need(set(values)==set(METRICS) and all(type(x)is int and 0<=x<=2**64-1 for x in values.values()),'missing/negative/noncanonical/nonfinite metric')
        need(job['role'] not in pairs[job['pair']],'duplicate role');pairs[job['pair']][job['role']]=values
    result={};undefined=[];primary=POLICY['primary']
    for metric in METRICS:
        if any(p['candidate'][metric]==0 or p['control'][metric]==0 for p in pairs.values()):
            result[metric]=dict(n=72,df=71,status='undefined_log_contrast_zero_preserved');undefined.append(metric);continue
        xs=[math.log(p['candidate'][metric]/p['control'][metric]) for p in pairs.values()];mean=statistics.mean(xs);se=statistics.stdev(xs)/math.sqrt(72)
        t=POLICY['primary_t'] if metric==primary else POLICY['auxiliary_t']
        item=dict(n=72,df=71,mean_log=mean,se_log=se,point=math.exp(mean),upper=math.exp(mean+t*se))
        if metric!=primary:item['lower']=math.exp(mean-t*se)
        need(all(math.isfinite(v) for v in item.values()),'nonfinite inferential output');result[metric]=item
    regressions=[m for m in METRICS if m!=primary and m not in undefined and result[m]['lower']>1]
    improvement=not undefined and result[primary]['upper']<1
    return dict(metrics=result,primary=primary,undefined_metrics=undefined,primary_improvement=improvement,confirmed_auxiliary_regression=regressions,statistically_eligible=improvement and not regressions,engineering_and_independent_audit_still_required=True,performance_accepted=False,QF_admitted=False)
