#!/usr/bin/env python3
"""Mac formal acceptance (pre-registered). usage: formal.py NAME
Binaries fixed below; FREEZE.json written before any run; then raw.jsonl; then RESULT.json."""
import hashlib, json, math, os, platform, random, shutil, statistics as st, subprocess, sys, tempfile, time
from statistics import NormalDist
NAME=sys.argv[1]; S=f'/Volumes/XOffsetCS/formal/{NAME}'
if os.path.exists(S): sys.exit('exists')
os.makedirs(S+'/bin')
V='/Volumes/XOffsetCS/mac-validation'; DG='/Volumes/XOffsetCS/diag'
LIBS={  # id: (binary, wire library name)
 'xoffset_cand':(DG+'/build-w7/bin/sc_xoffset','xoffset'),
 'xoffset_cand_aa':(DG+'/build-w7/bin/sc_xoffset','xoffset'),
 'xoffset_c14':(V+'/bench/c14-normal/bin/sc_xoffset','xoffset'),
 'xoffset_r1m2':(V+'/bench/r1m2-normal/bin/sc_xoffset','xoffset'),
 'cista':(V+'/bench/c14-normal/bin/sc_cista','cista'),
 'flatbuffers':(V+'/bench/c14-normal/bin/sc_flatbuffers','flatbuffers'),
 'capnproto':(V+'/bench/c14-normal/bin/sc_capnproto','capnproto'),
 'cereal':(V+'/bench/c14-normal/bin/sc_cereal','cereal')}
for k,(b,w) in LIBS.items(): shutil.copy2(b,f'{S}/bin/{k}')
shutil.copy2(__file__,S+'/formal.py')
h=lambda p: hashlib.sha256(open(p,'rb').read()).hexdigest()
CYCLES=65; ROUNDS_PER_CYCLE=3; WARMUP_CYCLES=1
fr={'schema':'xoffset-mac-formal-v1','name':NAME,
 'candidate':'W7 = R1+M2 + W2 (fixed-address canonical window, lazy reservation, 32 GiB Darwin) + G1 + G + MV; source /Volumes/XOffsetCS/candidates/w7',
 'binaries':{k:h(f'{S}/bin/{k}') for k in LIBS},'formal_py_sha256':h(S+'/formal.py'),
 'metadata':'restore alpha-era serialization_comparison adapters/driver (final-v4 C++ side not transferred); competitors cista 0.16 / FlatBuffers 25.12.19 / Cap\'n Proto 1.5.0 / cereal 1.3.2 at locked commits, modes unchanged, built natively on Mac',
 'fixture':'M from prepared-v1 regenerated on Mac (S equals golden); each library uses its own M input wire from the Mac functional run',
 'design':f'{CYCLES} macro cycles x {ROUNDS_PER_CYCLE} rounds; each round runs all {len(LIBS)} binaries once in an order shuffled by random.Random(20261020+round); first {WARMUP_CYCLES} cycle discarded (pre-registered); inference on per-cycle mean log ratios, df={CYCLES-WARMUP_CYCLES-1}',
 'primary':{'family':'12 = {cista,flatbuffers,capnproto,cereal} x {read_sum<=1.05, mutate<=1.05, emit_after<=0.20}; ratio xoffset_cand/competitor','rule':'joint one-sided 95% upper bound, critical t(1-0.05/12, df); an item passes iff upper bound <= its threshold'},
 'aa':{'pair':'xoffset_cand_aa/xoffset_cand','rule':'two-sided t(1-0.05/(2*8), df) intervals for 8 phases must lie within [1/1.05, 1.05]; failure invalidates the batch (reported, not rerun)'},
 'auxiliary':{'items':'reload ratio vs each competitor; cand/C14 and cand/R1M2 for read_sum, mutate, emit_after, reload, total','rule':'two-sided t(1-0.05/(2*14), df); report only, no gate'},
 'finality':'single run; no resampling, deletion, trend removal or rerun; any child failure aborts the batch as failed; child timeout 30 s',
 'host':platform.platform(),'frozen_epoch':time.time(),'pmset_therm':subprocess.run(['pmset','-g','therm'],capture_output=True,text=True).stdout}
json.dump(fr,open(S+'/FREEZE.json','w'),indent=1)
CMD=V+'/runs/c14-functional-normal/commands'; T=V+'/prepared-v1/M/trace.bin'
def run(k):
    w=LIBS[k][1]
    with tempfile.TemporaryDirectory(dir=S) as d:
        p=subprocess.run([f'{S}/bin/{k}','run','--wire',f'{CMD}/{w}-M.input.wire','--trace',T,'--wire-out',d+'/w','--canonical-out',d+'/c','--queries-out',d+'/q','--result',d+'/r.json'],capture_output=True,timeout=30)
        if p.returncode: raise SystemExit(f'child failed {k}: {p.stderr[-300:]}')
        return {kk:int(v) for kk,v in json.load(open(d+'/r.json'))['phase_ns'].items()}
with open(S+'/raw.jsonl','w') as f:
    for rnd in range(CYCLES*ROUNDS_PER_CYCLE):
        order=list(LIBS); random.Random(20261020+rnd).shuffle(order)
        for k in order: f.write(json.dumps({'round':rnd,'cycle':rnd//ROUNDS_PER_CYCLE,'lib':k,'phase_ns':run(k)})+'\n')
# analysis
def tq(p,df):
    z=NormalDist().inv_cdf(p); return z+(z**3+z)/(4*df)+(5*z**5+16*z**3+3*z)/(96*df**2)+(3*z**7+19*z**5+17*z**3-15*z)/(384*df**3)
rows=[json.loads(l) for l in open(S+'/raw.jsonl')]
by={}
for r in rows:
    p=r['phase_ns']; p['read_sum']=p['query_before']+p['query_after']; p['total']=sum(v for kk,v in r['phase_ns'].items() if kk not in('read_sum',))
    by.setdefault(r['round'],{})[r['lib']]=p
cycles=range(WARMUP_CYCLES,CYCLES); df=len(cycles)-1
def stat(num,den,m):
    cm=[st.mean(math.log(by[rr][num][m]/by[rr][den][m]) for rr in range(c*ROUNDS_PER_CYCLE,(c+1)*ROUNDS_PER_CYCLE)) for c in cycles]
    return st.mean(cm), st.stdev(cm)/math.sqrt(len(cm))
res={'name':NAME,'df':df,'primary':{},'aa':{},'auxiliary':{}}
q1=tq(1-0.05/12,df); passed=0
for comp in ['cista','flatbuffers','capnproto','cereal']:
    for m,goal in [('read_sum',1.05),('mutate',1.05),('emit_after',0.20)]:
        mu,se=stat('xoffset_cand',comp,m); ub=math.exp(mu+q1*se); ok=ub<=goal; passed+=ok
        res['primary'][f'{comp}:{m}']={'geo':math.exp(mu),'upper':ub,'threshold':goal,'pass':ok}
res['primary_passed']=passed
qa=tq(1-0.05/16,df); aa_ok=True
for m in ['load','query_before','mutate','emit_after','retire_modified_state','reload','query_after','cleanup']:
    mu,se=stat('xoffset_cand_aa','xoffset_cand',m); lo,hi=math.exp(mu-qa*se),math.exp(mu+qa*se); ok=lo>=1/1.05 and hi<=1.05; aa_ok&=ok
    res['aa'][m]={'geo':math.exp(mu),'low':lo,'high':hi,'ok':ok}
res['aa_valid']=aa_ok
qx=tq(1-0.05/28,df)
for comp in ['cista','flatbuffers','capnproto','cereal']:
    mu,se=stat('xoffset_cand',comp,'reload'); res['auxiliary'][f'{comp}:reload']={'geo':math.exp(mu),'low':math.exp(mu-qx*se),'high':math.exp(mu+qx*se)}
for base in ['xoffset_c14','xoffset_r1m2']:
    for m in ['read_sum','mutate','emit_after','reload','total']:
        mu,se=stat('xoffset_cand',base,m); res['auxiliary'][f'cand/{base}:{m}']={'geo':math.exp(mu),'low':math.exp(mu-qx*se),'high':math.exp(mu+qx*se)}
json.dump(res,open(S+'/RESULT.json','w'),indent=1)
print('primary passed',passed,'/12  aa_valid',aa_ok)
for k,v in res['primary'].items(): print('%-24s geo %.3f ub %.3f thr %.2f %s'%(k,v['geo'],v['upper'],v['threshold'],'PASS' if v['pass'] else 'fail'))
