"""M1-only bounded host guard and exact frozen timing dispatch, then restoration.

No user approval is requested. A factual cross-chat quiet declaration is needed
because this executor cannot infer other chats' inactivity from its own agents.
"""
import argparse
import base64
import importlib.util
import re
from pathlib import Path
import subprocess
import time

H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('cg',H/'m1_codegen_job.py')
C=importlib.util.module_from_spec(s);s.loader.exec_module(C);J=C.J
s=importlib.util.spec_from_file_location('pause',H/'host/process_pause.py')
P=importlib.util.module_from_spec(s);s.loader.exec_module(P)
D=J.DOC/'screen';HD=J.DOC/'host';OUT=J.OUTPUT/'M1-screen-20261019-v1'
PRELUDE=J.OUTPUT/'leases/m1-screen-host-prelude-v1'
PIN='e57468ed00157a0154f220b4c67a29879c3fc14bebb9ff04d65f621e4ed11685'
BALANCED='381b4222-f694-41f0-9685-ff5bb260df2e'

def power():
    a=['/mnt/c/Windows/System32/powercfg.exe','/getactivescheme']
    r=subprocess.run(a,capture_output=True,timeout=15)
    J.require(r.returncode==0,'cannot read current power scheme')
    m=re.search(rb'[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}',r.stdout)
    J.require(m is not None and m.group().decode().lower()==BALANCED,'current power differs from frozen Balanced condition; do not change it')
    return {'guid':BALANCED,'argv':a,'stdout_base64':base64.b64encode(r.stdout).decode()}

def quiet():
    q=J.read(D/'GLOBAL_QUIET.json')
    J.require(q['status']=='passed' and q['freeze_sha256']==PIN and q['all_other_chats_and_agents_quiet'] is True and q['no_other_heavy_jobs'] is True,'actual cross-chat quiet fact required')
    J.require('evidence' in q and q['evidence'],'quiet declaration must name actual observed/arranged scope')
    return q

def prelude():
    holder=J.read(J.LOCK/'queue.lock')
    J.require(holder['status']=='running' and Path(holder['out'])==PRELUDE and holder['owner']=='W1','actual W1 prelude lease required')
    quiet();power_before=power()
    authority=HD/'prelude-authority.json'
    J.write(authority,{'queue':str(PRELUDE),'run_id':holder['run_id'],'lock':str(J.LOCK/'queue.lock'),'holder':holder,'request':J.fid(PRELUDE/'request.json')})
    prerequisite=HD/'prelude-prerequisites.json'
    J.write(prerequisite,{'schema_version':1,'kind':'cursor-process-pause-prerequisites-v1',
        'C_compilation_active':False,'all_agents_quiet':True,'controlled_queues_drained':True,
        'prelude_lease_owner':'W1','prelude_lease_kind':'validation','prelude_queue':str(PRELUDE),
        'authority':{'path':str(authority),'identity':J.fid(authority)},'quiet_evidence':J.fid(D/'GLOBAL_QUIET.json')})
    plan=P.prepare_plan(HD/'process-plan-final.json')
    guard=P.ProcessPause(plan,{'path':str(prerequisite),'identity':J.fid(prerequisite)},maximum_seconds=780)
    guard.start_and_leave_running()
    J.write(HD/'PRELUDE_READY.json',{'status':'passed','freeze_sha256':PIN,'ready_monotonic_ns':str(time.monotonic_ns()),
        'power_before':power_before,'plan':plan,'guard_ready':J.fid(P.STATE/'ready.json'),
        'guard_module':J.fid(H/'host/process_pause.py'),'guard_helper':J.fid(H/'host/pause-control.ps1'),
        'controller':J.fid(__file__),'standalone_bounded_watchdog_seconds':780})

def main():
    p=argparse.ArgumentParser();p.add_argument('--prelude',action='store_true');a=p.parse_args()
    if a.prelude:prelude();return
    J.require(J.fid(D/'frozen-v1/freeze.json')['sha256']==PIN,'freeze changed')
    review=J.read(D/'REVIEW.json');J.require(review['status']=='passed' and review['freeze_sha256']==PIN,'exact independent freeze review required')
    q=quiet();J.require(not OUT.exists() and not PRELUDE.exists() and not P.STATE.exists(),'one fresh M1 attempt required')
    cmd=['/usr/bin/python3','-B',str(J.RUNNER),'run','--owner','W1','--kind','validation','--out',str(PRELUDE),
        '--timeout-seconds','60','--wait-seconds','0','--','/usr/bin/python3','-B',str(Path(__file__).resolve()),'--prelude']
    J.write(HD/'PRELUDE_DISPATCH_REQUEST.json',{'argv':cmd,'controller':J.fid(__file__),'quiet':J.fid(D/'GLOBAL_QUIET.json'),'watchdog_derivation':J.fid(HD/'WATCHDOG_DERIVATION.json')})
    timing=None;returncode=None;restored=False;restore_error=None
    try:
        r=subprocess.run(cmd,cwd=J.ROOT);J.require(r.returncode==0,'host prelude failed; restore before any audit')
        ready=J.read(HD/'PRELUDE_READY.json')
        J.write(D/'HOST.json',{'kind':'host-quiet-admission','status':'passed','freeze_sha256':PIN,'quiet':True,
            'original_global_lease':True,'restoration_ready':True,'ready_monotonic_ns':ready['ready_monotonic_ns'],
            'quiet_evidence':J.fid(D/'GLOBAL_QUIET.json'),'prelude_ready':J.fid(HD/'PRELUDE_READY.json'),
            'watchdog_ready':J.fid(P.STATE/'ready.json'),'actual_plan':ready['plan'],'power_before':ready['power_before'],
            'controller':J.fid(__file__),'power_changed':False})
        argv=['/usr/bin/python3','-B',str(J.RUNNER),'run','--owner','W3','--kind','timing','--out',str(OUT/'queue'),
           '--timeout-seconds','600','--wait-seconds','0','--','/usr/bin/python3','-B',str(H/'screen/m1_screen.py'),'run',
           '--frozen',str(D/'frozen-v1/freeze.json'),'--freeze-sha256',PIN,'--review',str(D/'REVIEW.json'),
           '--host',str(D/'HOST.json'),'--out',str(OUT),'--queue',str(OUT/'queue')]
        J.write(D/'TIMING_DISPATCH_REQUEST.json',{'argv':argv,'freeze_sha256':PIN,'host':J.fid(D/'HOST.json'),'review':J.fid(D/'REVIEW.json')})
        returncode=subprocess.run(argv,cwd=J.ROOT).returncode
        if (OUT/'queue/result.json').exists():timing=J.read(OUT/'queue/result.json')
    finally:
        if P.STATE.exists() and (P.STATE/'request.json').exists():
            try:
                completed=P.stop();restored=completed.get('status')=='passed'
            except BaseException as e:restore_error=type(e).__name__+': '+str(e)
        ready=J.read(HD/'PRELUDE_READY.json') if (HD/'PRELUDE_READY.json').exists() else {}
        power_after={};power_error=None
        try:power_after=power()
        except BaseException as e:power_error=type(e).__name__+': '+str(e)
        power_before=ready.get('power_before',{})
        J.write(D/'RESTORED.json',{'status':'passed' if restored and power_error is None else 'rejected','freeze_sha256':PIN,
            'user_processes_restored':restored,'resumed_monotonic_ns':str(time.monotonic_ns()),
            'power_before':power_before.get('guid'),'power_after':power_after.get('guid'),
            'power_before_evidence':power_before,'power_after_evidence':power_after,'power_changed':False,
            'guard_completed':J.fid(P.STATE/'completed.json') if (P.STATE/'completed.json').exists() else None,
            'error':restore_error,'power_error':power_error})
    J.require(restored,'user process restoration incomplete; do not audit')
    J.require(power_error is None,'power verification failed; rejected restoration receipt retained')
    if timing and timing['status']=='complete':
        coverage=P.verify_coverage(P.STATE,{'path':str(OUT/'queue/request.json'),'identity':J.fid(OUT/'queue/request.json')},
            {'path':str(OUT/'queue/result.json'),'identity':J.fid(OUT/'queue/result.json')})
        J.write(HD/'TIMING_PAUSE_COVERAGE.json',coverage)
    J.write(D/'TIMING_DISPATCH_RESULT.json',{'returncode':returncode,'restored':restored,'timing_result':timing,'screen_attempts':1,'retries':0})
    raise SystemExit(returncode if returncode is not None else 1)

if __name__=='__main__':main()
