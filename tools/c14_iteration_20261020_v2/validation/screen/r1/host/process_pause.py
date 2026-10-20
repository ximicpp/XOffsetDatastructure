"""Two-name Windows pause guard; no benchmark wrapper and no queue creation."""
from __future__ import annotations
import argparse,base64,datetime,hashlib,json,os,re,subprocess,time,uuid
from pathlib import Path,PureWindowsPath

HERE=Path(__file__).resolve().parent
STATE=Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/docs/coordination/c14-iteration-20261020-v2/validation/screen/R1/host/process-state-v1')
PS=Path('/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe')
ALLOWED=('client_ue4-Win64-Shipping','TGitCache')

class PauseError(RuntimeError):pass
def need(ok,message):
    if not ok:raise PauseError(message)
def fid(path):
    path=Path(path);need(path.is_file() and not path.is_symlink(),'regular file required: '+str(path))
    data=path.read_bytes();return {'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
def unique(items):
    result={}
    for k,v in items:need(k not in result,'duplicate JSON key');result[k]=v
    return result
def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'),object_pairs_hook=unique,
        parse_constant=lambda x:(_ for _ in ()).throw(PauseError('nonfinite JSON')))
def put(path,value):
    path=Path(path);temporary=path.with_name(path.name+'.publishing')
    owned=None
    try:
        with temporary.open('x',encoding='utf-8',newline='\n') as stream:
            state=os.fstat(stream.fileno());owned=(state.st_dev,state.st_ino)
            json.dump(value,stream,indent=2,sort_keys=True,ensure_ascii=True,allow_nan=False);stream.write('\n')
        # Link publishes a fully closed file without replacing any earlier record.
        os.link(temporary,path)
    finally:
        if owned is not None and temporary.exists():
            state=temporary.lstat()
            if (state.st_dev,state.st_ino)==owned:temporary.unlink()
def real_c_path(path):
    path=Path(path).absolute()
    need(str(path).startswith('/mnt/c/') and path.resolve()==path,'owned real absolute C: path required')
    return path
def winpath(path):
    result=subprocess.run(['/usr/bin/wslpath','-w',str(path)],capture_output=True,text=True,check=True,timeout=10)
    return result.stdout.strip()
def quote(value):return "'"+str(value).replace("'","''")+"'"

def validate_plan(plan):
    need(type(plan.get('schema_version'))is int and plan['schema_version']==1 and
        plan.get('kind')=='cursor-process-pause-plan-v1' and plan.get('read_only') is True,'read-only process plan schema')
    need(plan.get('allowed_names')==list(ALLOWED),'only two exact authorized process names')
    owner=plan.get('controller_owner_sid');need(type(owner)is str and re.fullmatch(r'S-1-[0-9-]+',owner),'Windows owner SID')
    targets=plan.get('targets');need(type(targets)is list and len(targets)<=16,'bounded explicit PID plan')
    seen=set()
    for target in targets:
        need(set(target)=={'pid','name','image_path','owner_sid','creation_filetime','start_time_utc'},'exact target identity fields')
        pid=target['pid'];need(type(pid)is int and 0<pid<2**32 and pid not in seen,'PID integer/duplicate');seen.add(pid)
        need(type(target['name'])is str and target['name'].casefold() in {n.casefold() for n in ALLOWED},'unauthorized process name')
        path=PureWindowsPath(target['image_path']);need(path.is_absolute() and path.suffix.casefold()=='.exe' and
            path.stem.casefold()==target['name'].casefold(),'actual absolute image/name identity')
        need(target['owner_sid']==owner,'another Windows owner')
        need(type(target['creation_filetime'])is str and re.fullmatch('[0-9]{1,20}',target['creation_filetime']),'exact FILETIME decimal string')
        need(type(target['start_time_utc'])is str and target['start_time_utc'].endswith('Z'),'exact UTC start time')
    return plan

def prepare_plan(path,helper=None):
    helper=real_c_path(helper or HERE/'pause-control.ps1');path=real_c_path(path)
    need(not path.exists(),'fresh plan file required')
    args=[str(PS),'-NoProfile','-NonInteractive','-WindowStyle','Hidden','-File',winpath(helper),
        '-Mode','Plan','-PlanFile',winpath(path),'-HelperSha256',fid(helper)['sha256']]
    result=subprocess.run(args,capture_output=True,timeout=30)
    put(path.with_name(path.name+'.invocation.json'),{'argv':args,'returncode':result.returncode,
        'stdout_base64':base64.b64encode(result.stdout).decode(),'stderr_base64':base64.b64encode(result.stderr).decode(),
        'helper':fid(helper),'read_only_no_suspend':True})
    need(result.returncode==0 and path.is_file(),'read-only process plan failed')
    validate_plan(read(path));return {'path':str(path),'identity':fid(path)}

def prerequisites(reference):
    need(set(reference)=={'path','identity'} and fid(reference['path'])==reference['identity'],'technical prerequisite record pin')
    value=read(reference['path'])
    need(type(value.get('schema_version'))is int and value['schema_version']==1 and
        value.get('kind')=='cursor-process-pause-prerequisites-v1','technical prerequisite schema')
    need(value.get('C_compilation_active') is False and value.get('all_agents_quiet') is True and
        value.get('controlled_queues_drained') is True and value.get('prelude_lease_owner')=='W1' and
        value.get('prelude_lease_kind')=='validation','compile stopped/agents quiet/drained original W1 prelude required')
    authority=value['authority'];need(fid(authority['path'])==authority['identity'],'actual prelude lease authority pin')
    record=read(authority['path']);need(type(record.get('run_id'))is str and re.fullmatch('[0-9a-f]{32}',record['run_id']) and
        record.get('queue')==value['prelude_queue'] and record.get('lock','').endswith('/build/vm-coordination/queue.lock'),
        'original stable lease authority required')
    return value

class ProcessPause:
    def __init__(self,plan_ref,prerequisite_ref,state_dir=STATE,maximum_seconds=780,helper=None):
        need(type(plan_ref)is dict and set(plan_ref)=={'path','identity'},'explicit original plan reference required')
        self.plan_file=real_c_path(plan_ref['path']);self.plan_identity=plan_ref['identity'];self.prerequisite_ref=prerequisite_ref
        self.state_dir=real_c_path(state_dir);need(self.state_dir==STATE,'single fresh declared state directory')
        self.maximum_seconds=maximum_seconds
        need(type(maximum_seconds)is int and 1<=maximum_seconds<=780,'watchdog limit1..780 seconds')
        self.helper=real_c_path(helper or HERE/'pause-control.ps1');self.ready=None;self.completed=None
    def start_and_leave_running(self):
        need(fid(self.plan_file)==self.plan_identity,'original reviewed plan identity changed')
        prerequisites(self.prerequisite_ref);plan=validate_plan(read(self.plan_file))
        helper_id=fid(self.helper);need(plan['helper_sha256']==helper_id['sha256'],'plan/helper identity mismatch')
        self.state_dir.mkdir(parents=True,exist_ok=False)
        token=uuid.uuid4().hex
        request={'schema_version':1,'kind':'cursor-process-pause-request-v1','run_token':token,
            'plan':{'path':str(self.plan_file),'identity':self.plan_identity},'prerequisites':self.prerequisite_ref,
            'maximum_seconds':self.maximum_seconds,'helper':helper_id,'module':fid(Path(__file__)),
            'state_dir':str(self.state_dir),'background_window_style':'Hidden','benchmark_argv_modified':False}
        put(self.state_dir/'request.json',request)
        arguments=['-NoProfile','-NonInteractive','-File',winpath(self.helper),'-Mode','Guard',
            '-PlanFile',winpath(self.plan_file),'-PlanSha256',request['plan']['identity']['sha256'],
            '-HelperSha256',helper_id['sha256'],'-StateDirectory',winpath(self.state_dir),
            '-RunToken',token,'-MaximumSeconds',str(self.maximum_seconds)]
        commandline=subprocess.list2cmdline(arguments)
        launcher="$exe=Join-Path $PSHOME 'powershell.exe';$p=Start-Process -WindowStyle Hidden -FilePath $exe -ArgumentList "+quote(commandline)+" -PassThru;[Console]::Write($p.Id)"
        encoded=base64.b64encode(launcher.encode('utf-16-le')).decode('ascii')
        need(len(encoded)<28000,'bounded Windows launch command')
        try:
            result=subprocess.run([str(PS),'-NoProfile','-NonInteractive','-EncodedCommand',encoded],capture_output=True,timeout=20)
            (self.state_dir/'launch.stdout').write_bytes(result.stdout);(self.state_dir/'launch.stderr').write_bytes(result.stderr)
            put(self.state_dir/'launch.json',{'returncode':result.returncode,'stdout':fid(self.state_dir/'launch.stdout'),
                'stderr':fid(self.state_dir/'launch.stderr')})
            need(result.returncode==0 and result.stdout.strip().isdigit(),'hidden independent pause guard launch failed')
            deadline=time.monotonic()+30
            while not (self.state_dir/'ready.json').exists() and time.monotonic()<deadline:time.sleep(.1)
            need((self.state_dir/'ready.json').is_file(),'pause readiness deadline')
            self.ready=read(self.state_dir/'ready.json')
            need(self.ready.get('status')=='ready' and self.ready.get('run_token')==token and
                self.ready.get('actual_suspended')==len(plan['targets']) and self.ready.get('targets')==plan['targets'],
                'pause setup/identity failed; timing must not start')
            need(not (self.state_dir/'completed.json').exists(),'pause guard already restored; timing must not start')
            put(self.state_dir/'ready-observed.json',{'run_token':token,'ready':fid(self.state_dir/'ready.json'),
                'observed_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
                'scope':'WSL controller observed all-target ready before returning; no timing child launched here'})
            return self.ready
        except BaseException as original:
            try:stop(self.state_dir)
            except BaseException as cleanup:
                put(self.state_dir/'enter-restore-error.json',{'original':repr(original),'restore':repr(cleanup),
                    'timing_must_not_start':True,'independent_watchdog_remains_bounded':True})
            raise
    def __enter__(self):self.start_and_leave_running();return self
    def __exit__(self,kind,value,trace):self.completed=stop(self.state_dir);return False

def stop(state_dir=STATE):
    state=real_c_path(state_dir);need(state==STATE,'fixed state directory')
    request=read(state/'request.json');token=request['run_token'];path=state/'stop.json'
    if not path.exists():
        try:put(path,{'stop_requested':True,'run_token':token,'requested_time_ns':str(time.time_ns()),
            'requested_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()})
        except FileExistsError:
            published=time.monotonic()+2
            while not path.exists() and time.monotonic()<published:time.sleep(.01)
    need(read(path).get('run_token')==token,'another stop token')
    deadline=time.monotonic()+30
    while not (state/'completed.json').exists() and time.monotonic()<deadline:time.sleep(.1)
    need((state/'completed.json').is_file(),'resume receipt unavailable; watchdog deadline still applies')
    result=read(state/'completed.json')
    need(result.get('run_token')==token and result.get('status')=='passed' and result.get('restored') is True,
        'guard restoration failed/expired; reject timing: '+str(result))
    return result

def verify_coverage(state_dir,timing_request_ref,timing_result_ref):
    state=real_c_path(state_dir);request=read(state/'request.json');ready=read(state/'ready.json');done=read(state/'completed.json')
    observed=read(state/'ready-observed.json');stopped=read(state/'stop.json')
    need(fid(request['plan']['path'])==request['plan']['identity'],'original reviewed plan changed after pause')
    plan=validate_plan(read(request['plan']['path']))
    for value in (timing_request_ref,timing_result_ref):need(fid(value['path'])==value['identity'],'timing metadata changed')
    timing_request=read(timing_request_ref['path']);timing=read(timing_result_ref['path'])
    need(ready['run_token']==done['run_token']==request['run_token'] and ready['status']=='ready' and
        done['status']=='passed' and done['restored'] is True and done['reason']=='requested_stop','bounded guard must complete normally')
    need(observed['run_token']==stopped['run_token']==request['run_token'] and observed['ready']==fid(state/'ready.json') and
        stopped['stop_requested'] is True,'controller ready/stop causal records')
    need(type(ready['actual_suspended'])is int and ready['actual_suspended']==len(plan['targets'])==len(done['targets']) and
        ready['targets']==plan['targets']==[target['identity'] for target in done['targets']],
        'exact plan/ready/completed target count and identities')
    need(all(t['suspended'] is True and t['resume_attempted'] is True and type(t['resume_status'])is int and
        t['resume_status']>=0 and t['exited_before_resume'] is False and
        type(t['wait_before_resume'])is int and t['wait_before_resume']==258 and
        type(t['wait_after_resume'])is int and t['wait_after_resume']==258 for t in done['targets']),
        'each paused target must have exactly one successful owned resume and remain live')
    need(timing['run_id']==timing_request['run_id'] and timing['kind']==timing_request['kind']=='timing' and
        timing['status']=='complete' and type(timing['command_returncode'])is int and timing['command_returncode']==0 and
        all(timing['cleanup'][k]==[] for k in ('live','process_groups','zombies')),'actual original timing queue must pass')
    parse=lambda text:datetime.datetime.fromisoformat(text.replace('Z','+00:00'))
    need(parse(observed['observed_utc'])<=parse(timing['started_at'])<=parse(timing['finished_at'])<=parse(stopped['requested_utc']),
        'pause interval did not cover complete actual timing queue; reject without rerun')
    return {'status':'passed','actual_suspended':ready['actual_suspended'],'all_suspended_utc':ready['all_suspended_utc'],
        'ready_observed_utc':observed['observed_utc'],'stop_requested_utc':stopped['requested_utc'],
        'timing_started_at':timing['started_at'],'timing_finished_at':timing['finished_at'],
        'resume_started_utc':done['resume_started_utc'],'timing_run_id':timing['run_id'],'no_remeasurement':True,
        'coverage_basis':'WSL ready-observed <= actual queue interval <= WSL stop request; guard requested_stop proves it stayed paused until stop. Raw Windows timestamps retained; no cross-clock offset assumption.'}

def main():
    ap=argparse.ArgumentParser();sub=ap.add_subparsers(dest='mode',required=True)
    plan=sub.add_parser('plan');plan.add_argument('--out',type=Path,required=True)
    start=sub.add_parser('start');start.add_argument('--plan',type=Path,required=True);start.add_argument('--plan-sha256',required=True)
    start.add_argument('--prerequisites',type=Path,required=True)
    start.add_argument('--prerequisites-sha256',required=True);start.add_argument('--maximum-seconds',type=int,default=780)
    sub.add_parser('stop')
    cover=sub.add_parser('coverage');cover.add_argument('--timing-request',type=Path,required=True);cover.add_argument('--timing-result',type=Path,required=True)
    cover.add_argument('--out',type=Path,required=True)
    args=ap.parse_args()
    if args.mode=='plan':result=prepare_plan(args.out)
    elif args.mode=='start':
        expected=fid(args.prerequisites);need(expected['sha256']==args.prerequisites_sha256,'prerequisite external SHA')
        plan_id=fid(args.plan);need(plan_id['sha256']==args.plan_sha256,'reviewed plan external SHA')
        result=ProcessPause({'path':str(args.plan),'identity':plan_id},{'path':str(args.prerequisites),'identity':expected},maximum_seconds=args.maximum_seconds).start_and_leave_running()
    elif args.mode=='stop':result=stop()
    else:
        result=verify_coverage(STATE,{'path':str(args.timing_request),'identity':fid(args.timing_request)},
            {'path':str(args.timing_result),'identity':fid(args.timing_result)})
        put(args.out,result)
    print(json.dumps(result,sort_keys=True),flush=True)

if __name__=='__main__':main()
