import json,sys,math,statistics as st,collections
rows=[json.loads(l) for l in open(sys.argv[1])]; X=sys.argv[2]
by=collections.defaultdict(dict)
for r in rows:
    p=r['phase_ns']; d=dict(p); d['read_sum']=p['query_before']+p['query_after']; by[r['round']][r['lib']]=d
# t quantile one-sided 95% with Bonferroni over 12 -> use t(1-0.05/12, n-1) approx via normal+correction
from statistics import NormalDist
def tq(p,df):
    z=NormalDist().inv_cdf(p); return z+(z**3+z)/(4*df)+(5*z**5+16*z**3+3*z)/(96*df**2)
for comp,metric,goal in [(c,m,g) for c in ['cista','flatbuffers','capnproto','cereal'] for m,g in [('read_sum',1.05),('mutate',1.05),('emit_after',0.20)]]:
    ls=[math.log(by[r][X][metric]/by[r][comp][metric]) for r in by if X in by[r] and comp in by[r]]
    n=len(ls); m=st.mean(ls); s=st.stdev(ls); q=tq(1-0.05/12,n-1)
    ub=math.exp(m+q*s/math.sqrt(n)); print('%-12s %-10s n=%d geo=%.3f ub=%.3f goal=%.2f %s'%(comp,metric,n,math.exp(m),ub,goal,'PASS' if ub<=goal else 'fail'))
