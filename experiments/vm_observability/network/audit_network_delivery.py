#!/usr/bin/env python3
"""Read-only delivery verification; never executes a native program."""
import argparse,hashlib,json,sys
from pathlib import Path,PurePosixPath
sys.dont_write_bytecode=True

def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):h.update(block)
    return h.hexdigest()
def read(p):return json.loads(p.read_text())
def require(ok,why):
    if not ok:raise ValueError(why)
def inventory(root):
    files={}
    for p in sorted(root.rglob('*')):
        require(not p.is_symlink(),'delivery symlink')
        if p.is_file() and p not in (root/'result.json',root/'COMPLETE'):files[p.relative_to(root).as_posix()]=sha(p)
    return files

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--record',type=Path,required=True);ap.add_argument('--repository',type=Path,default=Path.cwd());a=ap.parse_args();p=a.record.resolve();repo=a.repository.resolve();d=read(p/'result.json')
    require(d['schema']=='n3-network-delivery-v1' and d['status']=='passed' and d['timing_evidence'] is False,'delivery profile')
    require((p/'COMPLETE').read_text().strip()==sha(p/'result.json'),'delivery completion identity');require(inventory(p)==d['files'],'delivery file inventory')
    references=read(p/'references.json');accounted=0
    for name,digest in references.items():
        rel=PurePosixPath(name);require(not rel.is_absolute() and '..' not in rel.parts,'unsafe delivery reference');f=repo/name
        require(f.is_file() and not f.is_symlink() and sha(f)==digest,'delivery reference changed: '+name)
        if name.startswith(('build/vm-observability/network/','docs/validation/vm-observability/network/')):accounted+=f.stat().st_size
    accounted+=sum(q.stat().st_size for q in p.rglob('*') if q.is_file());require(accounted==d['accounted_network_bytes']<=536870912,'delivery evidence budget')
    frozen=p/'source/run_network_observation.py';m={'__file__':str(frozen),'__name__':'network_delivery_frozen_runner'};exec(compile(frozen.read_text(),str(frozen),'exec'),m)
    m['audit'](repo/d['build']);m['audit'](repo/d['collection'])
    summary=read(p/'summary/summary.json');require(summary['workload_processes']==32 and summary['valid_traces']==16 and summary['dropped_events']==0 and summary['max_observed_events_per_thread']<=2048,'delivery workload budgets')
    controls=read(p/'package-controls.json');require(controls['cpp_executions']==0 and len(controls['controls'])==12 and sum(c['status']=='rejected_as_required' for c in controls['controls'])==11,'delivery offline controls')
    print(json.dumps({'status':'audited','cpp_executions':0,'workload_processes':32,'valid_traces':16,'accounted_network_bytes':accounted,'references':len(references)}))
if __name__=='__main__':main()
