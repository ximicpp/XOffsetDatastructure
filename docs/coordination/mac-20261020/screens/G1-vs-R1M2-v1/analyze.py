import json,math,statistics as st,sys
from statistics import NormalDist
def tq(p,df):  # Cornish-Fisher t quantile approximation
    z=NormalDist().inv_cdf(p); return z+(z**3+z)/(4*df)+(5*z**5+16*z**3+3*z)/(96*df**2)+(3*z**7+19*z**5+17*z**3-15*z)/(384*df**3)
rows=[json.loads(l) for l in open(sys.argv[1])]; n=len(rows); df=n-1
def val(x,m):
    if m=='read_sum': return x['query_before']+x['query_after']
    if m=='total': return sum(x.values())
    return x[m]
out={}
def ci(m,p):
    ls=[math.log(val(r['B'][1],m)/val(r['A'][1],m)) for r in rows]; mu=st.mean(ls); se=st.stdev(ls)/math.sqrt(n); return mu,se
mu,se=ci('mutate',None); q=tq(1-0.05/4,df); ub=math.exp(mu+q*se); out['primary_mutate']={'geo':math.exp(mu),'upper':ub}
veto=False; aux={}
for m in ['read_sum','load','query_before','query_after','emit_after','retire_modified_state','reload','cleanup','total']:
    mu,se=ci(m,None); q=tq(1-0.05/72,df); lo,hi=math.exp(mu-q*se),math.exp(mu+q*se); aux[m]={'geo':math.exp(mu),'low':lo,'high':hi}; veto|=lo>1
out['auxiliary']=aux; out['veto']=veto; out['promote']=out['primary_mutate']['upper']<1 and not veto; out['n_pairs']=n
print(json.dumps(out,indent=1))
