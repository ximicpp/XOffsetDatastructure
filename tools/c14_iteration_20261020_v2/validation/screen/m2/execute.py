"""One v2 candidate host prelude/timing dispatch; restoration always precedes audit.

No actions occur on import. Root alone invokes after exact independent freeze
and host admission. No old PIN, target PID, creation time or handle is embedded.
"""
from pathlib import Path
import argparse,base64,datetime,fcntl,importlib.util,json,os,re,subprocess,sys,time
import screen as S
H=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('pause',H/'host/process_pause.py');P=importlib.util.module_from_spec(spec);spec.loader.exec_module(P)
D=S.ROOT/'docs/coordination/c14-iteration-20261020-v2/validation/screen/M2'
HD=D/'host';OUT=Path('/root/xoffset-c14-iteration/20261020/v2/validation/M2-screen-20261020-v2-v1')
PRELUDE=OUT.parent/'leases/m2-screen-host-prelude-v1'
def ref(p):
    identity=S.fid(p);identity['bytes']=int(identity['bytes']);return dict(path=str(p),identity=identity)
def power(guid):
    argv=['/mnt/c/Windows/System32/powercfg.exe','/getactivescheme'];r=subprocess.run(argv,capture_output=True,timeout=15)
    m=re.search(rb'[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}',r.stdout)
    S.need(r.returncode==0 and m is not None and m.group().decode().lower()==guid,'actual current frozen power condition differs; no scheme switch')
    return dict(guid=guid,argv=argv,stdout_base64=base64.b64encode(r.stdout).decode())
def prerequisites(a,f):
    quiet=S.C.read(a.quiet);review=S.C.read(a.review);host=S.C.read(a.host_review)
    for record,kind in ((quiet,'v2-screen-global-quiet'),(review,'independent-screen-admission'),(host,'v2-screen-host-admission')):
        S.need(record['status']=='passed' and record['kind']==kind and record['freeze_sha256']==a.freeze_sha256 and record['candidate']=='m2-exact-owns-v1','fresh exact candidate quiet/review/host admission required')
    S.need(quiet['all_other_chats_and_agents_quiet'] is True and quiet['no_other_heavy_jobs'] is True and quiet['evidence'] and quiet['C_compilation_active'] is False,'actual all-agent host quiet facts required')
    age=(datetime.datetime.now(datetime.timezone.utc)-datetime.datetime.fromisoformat(quiet['observed_utc'].replace('Z','+00:00'))).total_seconds();S.need(0<=age<=900,'fresh quiet observation required')
    S.need(host['allowed_names']==list(P.ALLOWED) and host['fresh_plan_required'] is True and host['no_old_pid_or_state'] is True and host['watchdog_seconds']==780 and host['restoration_before_audit'] is True,'fresh bounded two-name recovery registration required')
    grant=S.C.read(S.checked_file(host['standing_human_authorization']));S.checked_file(host['original_human_authorization'])
    S.need(grant['answer']=='允许临时暂停并恢复' and grant['authorized_targets']==list(P.ALLOWED),'actual standing two-name human authorization differs')
    S.need(grant['original_root_record']==host['original_human_authorization'],'standing/original authorization provenance differs')
    S.need(host['power_guid']==f['config']['host_policy']['power_guid'] and host['change_power'] is False,'new actual unchanged power policy required')
    S.checked_file(host['watchdog_derivation']);return quiet,host
def own_prelude():
    with (S.LOCK/'queue.lock').open('r+') as stream:
        try:fcntl.flock(stream,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:pass
        else:fcntl.flock(stream,fcntl.LOCK_UN);raise S.C.ProtocolError('original lease not held')
        stream.seek(0);holder=json.load(stream)
    q=S.C.read(PRELUDE/'request.json');expected=['/usr/bin/python3','-B',str(H/'execute.py'),*sys.argv[1:]]
    S.need(holder['status']=='running' and holder['out']==str(PRELUDE) and holder['run_id']==q['run_id'] and holder['supervisor_pid']==os.getppid() and holder['owner']==q['owner']=='W1' and holder['kind']==q['kind']=='validation','own W1 prelude out/run/supervisor required')
    S.need(q['command']==expected and q['timeout_seconds']==60 and q['lock_dir']==str(S.LOCK) and q['out']==str(PRELUDE) and q['cwd']==str(S.ROOT) and q['runner_sha256']==S.C.sha(S.RUNNER),'exact original prelude command/cwd/cap required')
    S.need(Path('/proc/self/cmdline').read_bytes().split(b'\0')[:-1]==[x.encode() for x in expected] and (Path('/proc')/str(os.getppid())/'cmdline').read_bytes().split(b'\0')[:-1]==[b'/usr/bin/python3',str(S.RUNNER).encode(),b'_worker',str(PRELUDE).encode()],'actual own worker/supervisor argv required')
    return dict(queue=str(PRELUDE),run_id=q['run_id'],lock=str(S.LOCK/'queue.lock'),holder=holder,request=S.fid(PRELUDE/'request.json'))
def prelude(a,f):
    authority=own_prelude();quiet,host=prerequisites(a,f);before=power(host['power_guid']);S.need(not P.STATE.exists(),'new independent guard state required')
    HD.mkdir(parents=True,exist_ok=True);S.write(HD/'prelude-authority.json',authority)
    pre=HD/'prelude-prerequisites.json';S.write(pre,dict(schema_version=1,kind='cursor-process-pause-prerequisites-v1',C_compilation_active=False,all_agents_quiet=True,controlled_queues_drained=True,prelude_lease_owner='W1',prelude_lease_kind='validation',prelude_queue=str(PRELUDE),authority=ref(HD/'prelude-authority.json'),quiet_evidence=ref(a.quiet)))
    plan=P.prepare_plan(HD/'process-plan-v1.json');guard=P.ProcessPause(plan,ref(pre),maximum_seconds=780);guard.start_and_leave_running()
    S.write(HD/'PRELUDE_READY.json',dict(status='passed',freeze_sha256=a.freeze_sha256,ready_monotonic_ns=str(time.monotonic_ns()),power_before=before,plan=plan,guard_ready=ref(P.STATE/'ready.json'),controller=ref(H/'execute.py'),standalone_bounded_watchdog_seconds=780))
def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=('run','prelude'))
    for name in ('frozen','review','quiet','host-review'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--freeze-sha256',required=True);a=p.parse_args();f=S.check_freeze(a.frozen,a.freeze_sha256)
    S.need(f['config']['output']==str(OUT),'independent output domain required')
    if a.action=='prelude':prelude(a,f);return
    _,host=prerequisites(a,f);S.need(not OUT.exists() and not PRELUDE.exists() and not P.STATE.exists() and not (D/'TIMING_DISPATCH_REQUEST.json').exists(),'one fresh candidate attempt only; no retry')
    HD.mkdir(parents=True,exist_ok=True)
    tail=['--frozen',str(a.frozen),'--review',str(a.review),'--quiet',str(a.quiet),'--host-review',str(a.host_review),'--freeze-sha256',a.freeze_sha256]
    cmd=['/usr/bin/python3','-B',str(S.RUNNER),'run','--owner','W1','--kind','validation','--out',str(PRELUDE),'--timeout-seconds','60','--wait-seconds','0','--','/usr/bin/python3','-B',str(H/'execute.py'),'prelude',*tail]
    S.write(D/'PRELUDE_DISPATCH_REQUEST.json',dict(argv=cmd,freeze_sha256=a.freeze_sha256,host_review=ref(a.host_review),quiet=ref(a.quiet),watchdog_budget='60 prelude +30 handoff +600 timing +queue cleanup/30 restoration+margin <=780; guard begins during prelude, no audit inside pause'))
    rc=None;timing=None;restored=False;error=None;power_error=None;after={};ready={}
    try:
        r=subprocess.run(cmd,cwd=S.ROOT);S.need(r.returncode==0,'host prelude failed; restoration first')
        ready=S.C.read(HD/'PRELUDE_READY.json');S.need(time.monotonic_ns()-int(ready['ready_monotonic_ns'])<=30_000_000_000,'handoff exceeded30s; close attempt without timing')
        prerequisites(a,f);power(host['power_guid']);S.need(not (P.STATE/'completed.json').exists(),'guard already restored')
        h=D/'HOST.json';S.write(h,dict(kind='host-quiet-admission',status='passed',freeze_sha256=a.freeze_sha256,quiet=True,original_global_lease=True,restoration_ready=True,ready_monotonic_ns=ready['ready_monotonic_ns'],quiet_evidence=ref(a.quiet),prelude_ready=ref(HD/'PRELUDE_READY.json'),watchdog_ready=ref(P.STATE/'ready.json'),actual_plan=ready['plan'],power_before=ready['power_before'],controller=ref(H/'execute.py'),power_changed=False))
        argv=['/usr/bin/python3','-B',str(S.RUNNER),'run','--owner','W3','--kind','timing','--out',str(OUT/'queue'),'--timeout-seconds','600','--wait-seconds','0','--','/usr/bin/python3','-B',str(H/'screen.py'),'run','--frozen',str(a.frozen),'--freeze-sha256',a.freeze_sha256,'--review',str(a.review),'--host',str(h),'--out',str(OUT),'--queue',str(OUT/'queue')]
        S.write(D/'TIMING_DISPATCH_REQUEST.json',dict(argv=argv,freeze_sha256=a.freeze_sha256,host=ref(h),review=ref(a.review)));rc=subprocess.run(argv,cwd=S.ROOT).returncode
        if (OUT/'queue/result.json').exists():timing=S.C.read(OUT/'queue/result.json')
    except BaseException as exception:error=type(exception).__name__+': '+str(exception)
    finally:
        if (P.STATE/'request.json').exists():
            try:restored=P.stop().get('status')=='passed'
            except BaseException as exception:error=(error or '')+'; restoration '+type(exception).__name__+': '+str(exception)
        if (HD/'PRELUDE_READY.json').exists():ready=S.C.read(HD/'PRELUDE_READY.json')
        try:after=power(host['power_guid'])
        except BaseException as exception:power_error=type(exception).__name__+': '+str(exception)
        S.write(D/'RESTORED.json',dict(status='passed' if restored and power_error is None else 'rejected',freeze_sha256=a.freeze_sha256,user_processes_restored=restored,resumed_monotonic_ns=str(time.monotonic_ns()),power_before=ready.get('power_before',{}).get('guid'),power_after=after.get('guid'),power_before_evidence=ready.get('power_before'),power_after_evidence=after,power_changed=False,guard_completed=ref(P.STATE/'completed.json') if (P.STATE/'completed.json').exists() else None,error=error,power_error=power_error))
    if restored and timing and timing['status']=='complete' and timing['command_returncode']==0:
        coverage=P.verify_coverage(P.STATE,{'path':str(OUT/'queue/request.json'),'identity':P.fid(OUT/'queue/request.json')},{'path':str(OUT/'queue/result.json'),'identity':P.fid(OUT/'queue/result.json')});S.write(HD/'TIMING_PAUSE_COVERAGE.json',coverage)
    S.write(D/'TIMING_DISPATCH_RESULT.json',dict(returncode=rc,restored=restored,error=error,timing_result=timing,screen_attempts=1,retries=0))
    S.need(restored and power_error is None,'restoration verification failed; preserve records and do not audit')
    raise SystemExit(rc if rc is not None else 1)
if __name__=='__main__':main()
