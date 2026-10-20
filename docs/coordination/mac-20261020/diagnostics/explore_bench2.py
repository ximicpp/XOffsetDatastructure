#!/usr/bin/env python3
"""Exploratory (NON-FORMAL) phase timing on macOS: runs sc_* `run` repeatedly in
randomized order. Diagnostic only; not a performance acceptance."""
import json, os, random, subprocess, sys, tempfile, statistics, time
V='/Volumes/XOffsetCS/mac-validation'; CMD=V+'/runs/c14-functional-normal/commands'
P=V+'/prepared-v1/'
size=sys.argv[1]; rounds=int(sys.argv[2]); out=sys.argv[3]
libs={'xoffset_c14':(V+'/bench/c14-normal/bin/sc_xoffset','xoffset'),
      'xoffset_r1m2':(V+'/bench/r1m2-normal/bin/sc_xoffset','xoffset'),
      'cista':(V+'/bench/c14-normal/bin/sc_cista','cista'),
      'flatbuffers':(V+'/bench/c14-normal/bin/sc_flatbuffers','flatbuffers'),
      'capnproto':(V+'/bench/c14-normal/bin/sc_capnproto','capnproto'),
      'cereal':(V+'/bench/c14-normal/bin/sc_cereal','cereal')}
extra=sys.argv[4:]
if extra and extra[0]=='--only':
    extra=extra[1:]; libs={}
for e in extra:
    k,b=e.split('=',1); libs[k]=(b,'xoffset')
rng=random.Random(20261020)
with open(out,'w') as f:
  for r in range(rounds):
    order=list(libs); rng.shuffle(order)
    for k in order:
      b,w=libs[k]
      with tempfile.TemporaryDirectory(dir='/Volumes/XOffsetCS/diag') as d:
        p=subprocess.run([b,'run','--wire',f'{CMD}/{w}-{size}.input.wire','--trace',P+size+'/trace.bin',
          '--wire-out',d+'/w','--canonical-out',d+'/c','--queries-out',d+'/q','--result',d+'/r.json'],capture_output=True,text=True)
        if p.returncode: print(k,p.stderr,file=sys.stderr); sys.exit(1)
        res=json.load(open(d+'/r.json'))
      f.write(json.dumps({'round':r,'lib':k,'phase_ns':{a:int(v) for a,v in res['phase_ns'].items()},'minor_faults':int(res['minor_faults'])})+'\n')
