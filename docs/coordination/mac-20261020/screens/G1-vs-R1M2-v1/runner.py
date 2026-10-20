#!/usr/bin/env python3
"""Mac A/A pilot (NON-FORMAL): 72 balanced AB/BA pairs of two byte-identical binaries.
Purpose: estimate per-child durations, noise and A/A interval widths to set budgets."""
import json, random, subprocess, sys, tempfile, time, math, statistics as st
V='/Volumes/XOffsetCS/mac-validation'; CMD=V+'/runs/c14-functional-normal/commands'; P=V+'/prepared-v1/M/'
A,B,out=sys.argv[1],sys.argv[2],sys.argv[3]; pairs=int(sys.argv[4]) if len(sys.argv)>4 else 72
rng=random.Random(20261020); orders=['AB']*(pairs//2)+['BA']*(pairs//2); rng.shuffle(orders)
def run(b):
    with tempfile.TemporaryDirectory(dir='/Volumes/XOffsetCS/diag/aa') as d:
        t=time.monotonic()
        subprocess.run([b,'run','--wire',CMD+'/xoffset-M.input.wire','--trace',P+'trace.bin','--wire-out',d+'/w','--canonical-out',d+'/c','--queries-out',d+'/q','--result',d+'/r.json'],check=True,capture_output=True)
        wall=time.monotonic()-t; r=json.load(open(d+'/r.json'))
    return wall,{k:int(v) for k,v in r['phase_ns'].items()}
with open(out,'w') as f:
    for i,o in enumerate(orders):
        res={}
        for c in o: res[c]=run(A if c=='A' else B)
        f.write(json.dumps({'pair':i,'order':o,'A':res['A'],'B':res['B']})+'\n')
