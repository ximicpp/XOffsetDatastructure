#!/usr/bin/env python3
"""Mac pre-registered 72-pair screen. usage:
screen.py NAME BASE_BIN CAND_BIN PRIMARY(comma) "description" BASE_SRC CAND_SRC
Writes FREEZE.json before any run, then raw.jsonl, then RESULT.json. Refuses existing dirs."""
import hashlib, json, math, os, platform, random, shutil, statistics as st, subprocess, sys, tempfile, time
from statistics import NormalDist
name, base, cand, primary, desc, bsrc, csrc = sys.argv[1:8]
primary = primary.split(',')
S = f'/Volumes/XOffsetCS/screens/{name}'
if os.path.exists(S): sys.exit(f'exists {S}')
os.makedirs(S)
shutil.copy2(base, S+'/base_sc_xoffset'); shutil.copy2(cand, S+'/cand_sc_xoffset'); shutil.copy2(__file__, S+'/screen.py')
h = lambda p: hashlib.sha256(open(p,'rb').read()).hexdigest()
PH = ['load','query_before','mutate','emit_after','retire_modified_state','reload','query_after','cleanup']
ALL = PH + ['read_sum','total']
aux = [m for m in ALL if m not in primary and m not in ('query_before','query_after') or (m in ('query_before','query_after') and 'read_sum' not in primary)]
aux = [m for m in ALL if m not in primary]
fr = {'schema':'xoffset-mac-screen-v3-c','name':name,'description':desc,'base_source':bsrc,'candidate_source':csrc,
 'base_binary_sha256':h(S+'/base_sc_xoffset'),'candidate_binary_sha256':h(S+'/cand_sc_xoffset'),'screen_py_sha256':h(S+'/screen.py'),
 'fixture':'M (prepared-v1, regenerated on Mac; S matches golden)','pairs':72,'order':'36 AB + 36 BA shuffled by random.Random(20261020)',
 'primary':{'metrics':primary,'rule':'one-sided 95% upper bound of geometric mean cand/base, critical t(1-0.05/4,71); promotion requires every primary upper bound < 1'},
 'auxiliary':{'metrics':aux,'rule':'one-sided 95% upper bound per item, critical t(0.95,71), no Bonferroni (user choice C, 2026-10-20 19:44Z); non-inferiority (user D1/Q12): read_sum and query_before/query_after upper bound must be <= 1.01, every other auxiliary upper bound <= 1.02, otherwise promotion is vetoed'},
 'finality':'single run; no resampling, deletion, trend removal or re-run; child timeout 20 s; any child failure aborts the screen as failed',
 'host':platform.platform(),'frozen_epoch':time.time(),
 'pmset_therm':subprocess.run(['pmset','-g','therm'],capture_output=True,text=True).stdout}
json.dump(fr, open(S+'/FREEZE.json','w'), indent=1)
V='/Volumes/XOffsetCS/mac-validation'; W=V+'/runs/c14-functional-normal/commands/xoffset-M.input.wire'; T=V+'/prepared-v1/M/trace.bin'
rng = random.Random(20261020); orders = ['AB']*36+['BA']*36; rng.shuffle(orders)
def run(b):
    with tempfile.TemporaryDirectory(dir=S) as d:
        p = subprocess.run([b,'run','--wire',W,'--trace',T,'--wire-out',d+'/w','--canonical-out',d+'/c','--queries-out',d+'/q','--result',d+'/r.json'],capture_output=True,timeout=20)
        if p.returncode: raise SystemExit(f'child failed {b}: {p.stderr[-300:]}')
        return {k:int(v) for k,v in json.load(open(d+'/r.json'))['phase_ns'].items()}
with open(S+'/raw.jsonl','w') as f:
    for i,o in enumerate(orders):
        r={}
        for c in o: r[c]=run(S+('/base_sc_xoffset' if c=='A' else '/cand_sc_xoffset'))
        f.write(json.dumps({'pair':i,'order':o,'A':r['A'],'B':r['B']})+'\n')
def tq(p,df):
    z=NormalDist().inv_cdf(p); return z+(z**3+z)/(4*df)+(5*z**5+16*z**3+3*z)/(96*df**2)+(3*z**7+19*z**5+17*z**3-15*z)/(384*df**3)
rows=[json.loads(l) for l in open(S+'/raw.jsonl')]; n=len(rows)
def val(x,m): return x['query_before']+x['query_after'] if m=='read_sum' else (sum(x.values()) if m=='total' else x[m])
def ms(m):
    ls=[math.log(val(r['B'],m)/val(r['A'],m)) for r in rows]; return st.mean(ls), st.stdev(ls)/math.sqrt(n)
res={'name':name,'n_pairs':n,'primary':{},'auxiliary':{}}
ok=True
for m in primary:
    mu,se=ms(m); ub=math.exp(mu+tq(1-0.05/4,n-1)*se); res['primary'][m]={'geo':math.exp(mu),'upper':ub}; ok&=ub<1
veto=False
for m in aux:
    mu,se=ms(m); q=tq(0.95,n-1); lo,hi=math.exp(mu-q*se),math.exp(mu+q*se); lim=1.01 if m in ('read_sum','query_before','query_after') else 1.02; res['auxiliary'][m]={'geo':math.exp(mu),'low':lo,'high':hi,'limit':lim}; veto|=hi>lim
res['veto']=veto; res['promote']=ok and not veto
json.dump(res,open(S+'/RESULT.json','w'),indent=1)
print(name,'promote=',res['promote'],{m:(round(v['geo'],4),round(v['upper'],4)) for m,v in res['primary'].items()},'veto=',veto,
      {m:round(v['geo'],3) for m,v in res['auxiliary'].items() if v['high']>v['limit'] or v['high']<1})
