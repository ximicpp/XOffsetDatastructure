#!/usr/bin/env python3
"""Auxiliary actual-runtime invalid-recorder controls, never timing evidence."""
import argparse,json,os,subprocess,sys
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parent))
from observe import sha, fresh, finish, require
from audit_events import validate_trace

def main():
    a=argparse.ArgumentParser();a.add_argument('--binary',type=Path,required=True);a.add_argument('--out',type=Path,required=True);args=a.parse_args()
    root=Path(__file__).resolve().parents[2];lock=json.loads((root/'build/vm-coordination/queue.lock').read_text())
    require(lock['status']=='running' and lock['kind']=='diagnostic' and lock['owner']=='W2' and lock['supervisor_pid']==os.getppid(),'must use W4 diagnostic queue')
    out=fresh(args.out);commands=[];results={}
    for case in ('threads','labels','allocator'):
        command=[str(args.binary.resolve()),'--case',case];r=subprocess.run(command,capture_output=True,text=True,timeout=15)
        (out/(case+'.stdout')).write_text(r.stdout);(out/(case+'.stderr')).write_text(r.stderr);require(r.returncode==0,'runtime control failed '+case+':'+r.stderr)
        trace=json.loads(r.stdout)
        if case=='allocator':
            summary=validate_trace(trace);require(summary['event_count']==3 and len(summary['allocations']['outstanding'])==0,'allocator control ledger')
            results[case]=summary
        else:
            require(trace['valid'] is False and trace['dropped']>0,'runtime did not invalidate '+case)
            try:validate_trace(trace)
            except ValueError:pass
            else:raise RuntimeError('invalid runtime trace accepted')
            if case=='threads':require(trace['thread_count']==16 and len(trace['events'])==16,'thread exhaustion control did not reach capacity')
            results[case]={'status':'invalid_trace_rejected','dropped':trace['dropped'],'recorded':len(trace['events'])}
        commands.append({'argv':command,'returncode':r.returncode})
    (out/'summaries.json').write_text(json.dumps(results,indent=2)+'\n')
    here=Path(__file__).resolve().parent
    finish(out,{'schema':'n3-auxiliary-controls-v1','kind':'actual_runtime_controls','cases':['threads','labels','allocator'],'commands':commands,'binary_sha256':sha(args.binary),'source_sha256':{name:sha(here/name) for name in ('runtime_controls.cpp','n3_runtime.hpp','verify_runtime_controls.py','audit_events.py','event_kinds.json')},'queue_run_id':lock['run_id']})
    print(json.dumps({'status':'passed','actual_runtime_controls':3,'timing_evidence':False}))
if __name__=='__main__':
    try:main()
    except Exception as e:print(type(e).__name__+': '+str(e),file=sys.stderr);raise SystemExit(1)
