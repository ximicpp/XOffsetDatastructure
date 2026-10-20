#!/usr/bin/env python3
"""One bounded diagnostic child, a nonblocking event pipe, one wait4 reaper.

This module never starts a process when imported. Root runs actual jobs and
dummy selfchecks under the existing shared run_job lease.
"""
from __future__ import annotations
import argparse
import ctypes
import errno
import hashlib
import json
import math
import os
from pathlib import Path
import re
import resource
import selectors
import signal
import sys
import time

HERE=Path(__file__).resolve().parent
INTERFACE=json.loads((HERE/'INTERFACE.json').read_bytes())
LABELS=set(INTERFACE['labels'])
LIMITS=INTERFACE['limits']
GiB=1024**3

def encoded(value):return (json.dumps(value,sort_keys=True,indent=2,allow_nan=False)+'\n').encode()
def exclusive(path,value):
    with Path(path).open('xb') as f:f.write(encoded(value));f.flush();os.fsync(f.fileno())
def file_id(path):
    p=Path(path);h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1024**2),b''):h.update(b)
    return {'bytes':p.stat().st_size,'sha256':h.hexdigest()}
def persist(f,data):f.write(data);f.flush();os.fsync(f.fileno())
def require(v,m):
    if not v:raise ValueError(m)

def parse_event(raw):
    require(len(raw)<=128 and raw.endswith(b'\n'),'event line over128 or unterminated')
    text=raw[:-1].decode('ascii')
    parts=text.split(' ')
    require(len(parts) in (4,8) and parts[0]=='XD1' and re.fullmatch(r'0|[1-9][0-9]*',parts[1]) is not None,'bad event schema')
    require(re.fullmatch(r'[1-9][0-9]*',parts[2]) is not None and int(parts[2])<2**64,'invalid child monotonic ns')
    seq=int(parts[1]);label=parts[3]
    require(0<=seq<64 and label in LABELS,'sequence/label outside fixed interface')
    require((len(parts)==8)==label.startswith('emit.pod'),'POD arity differs')
    values=[]
    for v in parts[4:]:
        require(re.fullmatch(r'0|[1-9][0-9]*',v) is not None and int(v)<2**64,'invalid POD u64')
        values.append(int(v))
    return {'sequence':seq,'child_monotonic_ns':parts[2],'label':label,'values':values}

def section(label,previous):
    if label.startswith('emit.pod'):return previous
    phases=('load','query_before','mutate','emit_after','retire_modified_state','reload','query_after','cleanup','after_t8')
    if re.fullmatch(r't[0-8]',label):return phases[int(label[1:])]
    if label.endswith('.before'):return label[:-7]
    return 'after:'+label

def group_members(pgid,session):
    result=[]
    for p in Path('/proc').iterdir():
        if not p.name.isdecimal():continue
        try:
            raw=(p/'stat').read_text();fields=raw[raw.rfind(')')+2:].split()
            if int(fields[2])==pgid and int(fields[3])==session:result.append({'pid':int(p.name),'state':fields[0],'ppid':int(fields[1]),'start_ticks':int(fields[19])})
        except (OSError,ValueError,IndexError):continue
    return result

def supervise(command,out,environment,bodies=(),timeout=20.0,close_reader=None,dummy=False,fault=None,binding=None,queue_request=None,absolute_job_deadline_ns=None):
    """No waitpid/Popen: the only successful reaping syscall is wait4 below."""
    require(os.name=='posix' and sys.platform.startswith('linux'),'Linux wait4/pipe2 required')
    require(command and Path(command[0]).is_absolute(),'absolute executable argv required')
    require(type(environment) is dict and all(type(k)==str and type(v)==str for k,v in environment.items()),'explicit string environment required')
    require((dummy and 0<timeout<=2) or (not dummy and timeout==20),'unregistered child deadline')
    require(close_reader is None or dummy,'reader-fault injection is dummy-only')
    require(fault is None or dummy,'collector/reaper fault injection is dummy-only')
    if absolute_job_deadline_ns is not None:require(time.monotonic_ns()+int((timeout+.5)*1e9)<=absolute_job_deadline_ns,'whole job cannot fit one child and fixed cleanup; do not launch')
    libc=ctypes.CDLL(None,use_errno=True)
    require(libc.prctl(36,1,0,0,0)==0,'cannot establish wrapper as child subreaper')
    out=Path(out);require(out.is_absolute() and not out.exists(),'fresh absolute output required');out.mkdir(parents=True)
    bodies=[Path(p) for p in bodies]
    require(len(bodies) in (0,4) and all(p.is_absolute() and not p.exists() for p in bodies),'zero dummy bodies or four fresh absolute body outputs required')
    require(dummy or len(bodies)==4,'real diagnostic must declare all four output bodies')
    event_r,event_w=os.pipe2(os.O_NONBLOCK|os.O_CLOEXEC)
    stdout_r,stdout_w=os.pipe2(os.O_CLOEXEC);stderr_r,stderr_w=os.pipe2(os.O_CLOEXEC)
    os.set_blocking(stdout_r,False);os.set_blocking(stderr_r,False);os.set_inheritable(event_w,True)
    child_env={**environment,'XOFFSET_DIAG_EVENT_FD':str(event_w)}
    if dummy:child_env['XOFFSET_DUMMY_CONTROL']=str(out)
    files={n:(out/(n+'.raw')).open('xb') for n in ('events','stdout','stderr')};parsed=(out/'events.jsonl').open('xb')
    sel=selectors.DefaultSelector();session=os.getsid(0)
    # fsync launch intent before fork: retained even if exec/wrapper later fails.
    launch=time.monotonic_ns();deadline=launch+int(timeout*1e9)
    exclusive(out/'launch-before.json',{'schema':'xoffset-diag-launch-v1','command':command,'environment':environment,
        'event_fd_environment':'XOFFSET_DIAG_EVENT_FD','started_monotonic_ns':str(launch),'timeout_seconds':timeout,
        'bodies':list(map(str,bodies)),'limits':LIMITS,'interface':file_id(HERE/'INTERFACE.json'),'wrapper':file_id(__file__),
        'dummy':dummy,'reader_fault_injection':close_reader,'binding':binding,'queue_request':queue_request,'absolute_job_deadline_ns':str(absolute_job_deadline_ns) if absolute_job_deadline_ns is not None else None})
    pid=os.fork()
    if pid==0:
        try:
            os.setpgid(0,0) # remain in inherited session, as required by run_job
            os.close(event_r);os.close(stdout_r);os.close(stderr_r)
            fd=os.open('/dev/null',os.O_RDONLY);os.dup2(fd,0);os.close(fd)
            os.dup2(stdout_w,1);os.dup2(stderr_w,2)
            if stdout_w not in (1,2):os.close(stdout_w)
            if stderr_w not in (1,2):os.close(stderr_w)
            resource.setrlimit(resource.RLIMIT_FSIZE,(LIMITS['body_file_bytes'],)*2)
            resource.setrlimit(resource.RLIMIT_AS,(LIMITS['address_space_bytes'],)*2)
            # Close inherited coordinator/watcher descriptors except our pipe.
            for name in os.listdir('/proc/self/fd'):
                n=int(name)
                if n>2 and n!=event_w:
                    try:os.close(n)
                    except OSError:pass
            os.execve(command[0],command,child_env)
        except BaseException as error:
            try:os.write(2,('diag exec: '+type(error).__name__+': '+str(error)+'\n').encode()[:4096])
            except OSError:pass
            os._exit(127)
    os.close(event_w);os.close(stdout_w);os.close(stderr_w)
    early_error=None
    direct_start_ticks=None
    try:
        try:os.setpgid(pid,pid)
        except OSError as e:
            if e.errno not in (errno.EACCES,errno.ESRCH):raise
        direct_record=next((m for m in group_members(pid,session) if m['pid']==pid),None)
        direct_start_ticks=direct_record['start_ticks'] if direct_record else None
        exclusive(out/'launched.json',{'pid':pid,'process_group':pid,'session':session,'start_ticks':direct_start_ticks,'launch_return_monotonic_ns':str(time.monotonic_ns())})
    except BaseException as error:early_error=type(error).__name__+': '+str(error)
    fds={'events':event_r,'stdout':stdout_r,'stderr':stderr_r}
    for name,fd in fds.items():sel.register(fd,selectors.EVENT_READ,name)
    counts={n:0 for n in fds};stored={n:0 for n in fds};events=[];errors=[];buffer=b'';last_section='before_main'
    killed=False;kill_reason=None;status=None;usage=None;reaped_at=None;successful_wait4=0;last_seq=-1;reader_closed=False;reaper_error=None;kill_result=None;poll_calls=0;received_event_lines=0;kill_attempts=[]
    cleanup_deadline=deadline+500_000_000
    def close_fd(fd):
        try:sel.unregister(fd)
        except KeyError:pass
        try:os.close(fd)
        except OSError:pass
    def kill(reason):
        nonlocal killed,kill_reason,kill_result
        if not killed:
            killed=True;kill_reason=reason
            try:exclusive(out/'kill-request.json',{'reason':reason,'monotonic_ns':str(time.monotonic_ns()),'last_event':events[-1] if events else None,'last_section':last_section})
            except OSError as error:errors.append('kill_record_write: '+str(error))
        # Never signal a stale numeric PGID. Re-prove current identity/ancestry,
        # then signal only individually rechecked members of this owned group.
        members=group_members(pid,session);owned={m['pid'] for m in members if m['ppid']==os.getpid() or (m['pid']==pid and direct_start_ticks is not None and m['start_ticks']==direct_start_ticks)}
        changed=True
        while changed:
            new=owned|{m['pid'] for m in members if m['ppid'] in owned};changed=new!=owned;owned=new
        outcomes=[]
        for member in reversed(members):
            if member['pid'] not in owned:continue
            current=next((m for m in group_members(pid,session) if m['pid']==member['pid']),None)
            if current is None or current['start_ticks']!=member['start_ticks']:
                outcomes.append({'pid':member['pid'],'result':'identity_changed_or_gone'});continue
            try:os.kill(member['pid'],signal.SIGKILL);outcomes.append({'pid':member['pid'],'start_ticks':member['start_ticks'],'result':'signal_sent'})
            except ProcessLookupError:outcomes.append({'pid':member['pid'],'result':'ESRCH'})
            except OSError as error:outcomes.append({'pid':member['pid'],'result':type(error).__name__+': '+str(error)})
        kill_result='verified_owned_PID_signals' if outcomes else 'no_verified_current_owned_members'
        kill_attempts.append({'reason':reason,'monotonic_ns':str(time.monotonic_ns()),'members':members,'signals':outcomes})
    if close_reader=='before_launch':
        close_fd(event_r);reader_closed=True;errors.append('dummy_reader_closed_before_launch');exclusive(out/'reader-closed.json',{'closed':True})
    try:
        if early_error:raise RuntimeError(early_error)
        while status is None or sel.get_map():
            now=time.monotonic_ns()
            if status is None:
                poll_calls+=1
                try:got,child_status,child_usage=os.wait4(pid,os.WNOHANG)
                except ChildProcessError as error:
                    reaper_error='ECHILD: '+str(error);errors.append(reaper_error);kill('reaper_error');break
                if got:
                    if fault=='echild_after_reap':
                        reaper_error='injected ECHILD after sole kernel reap';errors.append(reaper_error);kill('reaper_error');break
                    status=child_status;usage=child_usage;reaped_at=time.monotonic_ns();successful_wait4+=1
                    exclusive(out/'wait4-return.json',{'pid':got,'status':status,'returncode':os.waitstatus_to_exitcode(status),'returned_monotonic_ns':str(reaped_at)})
                elif now>=deadline:kill('timeout')
                if status is None and now>=cleanup_deadline:errors.append('unreaped_at_cleanup_deadline');break
            if status is not None and time.monotonic_ns()-reaped_at>250_000_000 and sel.get_map():
                errors.append('pipe_not_closed_after_child_exit');kill('descendant_pipe_open')
                for key in list(sel.get_map().values()):close_fd(key.fd)
                break
            total=sum(p.stat().st_size for p in bodies if p.exists())
            if total>LIMITS['combined_body_bytes']:kill('body_limit')
            remaining=max(0,(deadline-time.monotonic_ns())/1e9) if status is None else .05
            for key,_ in sel.select(min(.05,remaining)):
                name=key.data
                try:data=os.read(key.fd,65536)
                except BlockingIOError:continue
                if not data:close_fd(key.fd);continue
                counts[name]+=len(data);available=LIMITS['combined_log_bytes']-sum(stored.values())
                keep=data[:max(0,available)]
                if keep:persist(files[name],keep);stored[name]+=len(keep)
                if len(keep)!=len(data):kill('log_limit')
                if name!='events':continue
                buffer+=keep
                while b'\n' in buffer:
                    line,buffer=buffer.split(b'\n',1);line+=b'\n'
                    received_event_lines+=1
                    if received_event_lines>64:
                        errors.append('event_count_over64');kill('event_count_limit');buffer=b'';break
                    try:
                        event=parse_event(line)
                        require(len(events)<64,'event_count_over64')
                        require(event['sequence']==last_seq+1,'event_sequence_gap_or_duplicate')
                        last_seq=event['sequence'];event['received_monotonic_ns']=str(time.monotonic_ns())
                        require(launch<=int(event['child_monotonic_ns'])<=int(event['received_monotonic_ns']),'child event outside wrapper monotonic interval')
                        events.append(event);last_section=section(event['label'],last_section)
                        persist(parsed,json.dumps(event,sort_keys=True,separators=(',',':')).encode()+b'\n')
                    except (ValueError,UnicodeError) as error:errors.append(str(error))
                    if fault=='collector_error_after_first' and events:raise OSError(errno.ENOSPC,'injected collector write failure after retained first event')
                    if close_reader=='after_first_event' and events and not reader_closed:
                        close_fd(event_r);reader_closed=True;errors.append('dummy_reader_closed_after_first_event');exclusive(out/'reader-closed.json',{'closed':True});break
                if len(buffer)>128:errors.append('overlong_unterminated_event');kill('event_line_limit')
    except BaseException as error:
        errors.append('collector_error: '+type(error).__name__+': '+str(error));kill('collector_error')
    finally:
        # Exceptional Python paths also kill and reap once, retaining event files.
        if status is None and reaper_error is None:
            kill('wrapper_exception' if kill_reason is None else kill_reason)
            while time.monotonic_ns()<cleanup_deadline:
                poll_calls+=1
                try:got,new_status,new_usage=os.wait4(pid,os.WNOHANG)
                except ChildProcessError as error:reaper_error='ECHILD: '+str(error);errors.append(reaper_error);break
                if got:status=new_status;usage=new_usage;reaped_at=time.monotonic_ns();successful_wait4+=1;break
                time.sleep(.005)
        for key in list(sel.get_map().values()):close_fd(key.fd)
        sel.close()
        for f in (*files.values(),parsed):
            try:f.flush();os.fsync(f.fileno())
            except OSError as error:errors.append('close_log_write: '+str(error))
            finally:f.close()
    remaining=group_members(pid,session);auxiliary_reaped=[]
    if remaining:
        errors.append('owned_group_present_after_parent');kill('owned_group_remaining')
        until=min(cleanup_deadline,time.monotonic_ns()+250_000_000)
        while remaining and time.monotonic_ns()<until:
            kill('owned_group_remaining')
            for member in remaining:
                if member['pid']==pid:continue
                try:
                    got,s,r=os.wait4(member['pid'],os.WNOHANG)
                    if got:auxiliary_reaped.append({'pid':got,'wait_status':s,'returncode':os.waitstatus_to_exitcode(s)})
                except ChildProcessError:pass
            time.sleep(.005);remaining=group_members(pid,session)
    if remaining:errors.append('cleanup_incomplete')
    if buffer:errors.append('truncated_event_at_eof')
    if not events:errors.append('no_complete_events')
    if events and events[-1]['label']!='main.end':errors.append('missing_main_end')
    missing=sorted(LABELS-{e['label'] for e in events})
    if missing:errors.append('missing_fixed_boundaries')
    duplicate_labels=len({e['label'] for e in events})!=len(events)
    if duplicate_labels:errors.append('duplicate_fixed_boundary')
    returned=reaped_at if reaped_at is not None else time.monotonic_ns()
    wall=(returned-launch)/1e9;rc=os.waitstatus_to_exitcode(status) if status is not None else None
    if usage is not None and usage.ru_maxrss*1024>LIMITS['address_space_bytes']:errors.append('observed_RSS_over16GiB')
    if returned>deadline:errors.append('child_deadline_exceeded')
    if wall>timeout+.25:errors.append('deadline_cleanup_over250ms')
    launch_error=(out/'stderr.raw').read_bytes()[:4096].decode('ascii',errors='replace') if rc==127 and (out/'stderr.raw').read_bytes().startswith(b'diag exec:') else None
    receipt={'schema':'xoffset-diag-receipt-v1','status':'complete' if rc==0 and not killed and not errors else 'failed',
        'command':command,'pid':pid,'pgid':pid,'session':session,'dummy':dummy,'timeout_seconds':timeout,'timed_out':kill_reason=='timeout',
        'kill_reason':kill_reason,'kill_result':kill_result,'kill_attempts':kill_attempts,'returncode':rc,'wait_status':status,'successful_wait4_calls':successful_wait4,
        'wait4_poll_calls':poll_calls,'reaper_error':reaper_error,'launch_error':launch_error,'cleanup_deadline_monotonic_ns':str(cleanup_deadline),'cleanup_remaining':remaining,
        'auxiliary_owned_descendants_reaped':auxiliary_reaped,
        'started_monotonic_ns':str(launch),'deadline_monotonic_ns':str(deadline),'returned_monotonic_ns':str(reaped_at) if reaped_at is not None else None,'wall_seconds':wall,
        'user_cpu_seconds':usage.ru_utime if usage is not None else None,'system_cpu_seconds':usage.ru_stime if usage is not None else None,'cpu_seconds':usage.ru_utime+usage.ru_stime if usage is not None else None,
        'minor_faults':usage.ru_minflt if usage is not None else None,'major_faults':usage.ru_majflt if usage is not None else None,'max_rss_kib':usage.ru_maxrss if usage is not None else None,
        'rusage_scope':'complete fork/exec direct child lifetime (kernel wait4), including its waited descendants, not only timed stages',
        'events':events,'errors':errors,'received_event_lines':received_event_lines,'missing_boundaries':missing,'last_event':events[-1] if events else None,'last_section':last_section,
        'received_log_bytes':counts,'persisted_log_bytes':stored,'body_bytes':{str(p):p.stat().st_size for p in bodies if p.exists()},
        'files':{p.name:file_id(p) for p in out.iterdir() if p.is_file()},'real_diag':not dummy,
        'instrumented':True,'eligible_for_performance':False,'semantic_validation':'not_performed_by_observer','retry':0,
        'observer_complete':not errors and events[-1]['label']=='main.end' if events else False,'binding':binding,'queue_request':queue_request,
        'artifacts':{str(p):{'exists':p.exists(),'identity':file_id(p) if p.exists() else None,'validated':False} for p in bodies},
        'finished_monotonic_ns':str(time.monotonic_ns()),'performance_inference':None}
    exclusive(out/'receipt.json',receipt);return receipt

def lease_check(queue):
    """Dummy selfchecks and actual diag share the old real global lease."""
    import fcntl
    runner=Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py')
    lock=runner.parents[2]/'build/vm-coordination';q=json.loads((Path(queue)/'request.json').read_bytes())
    command=q['command'];expected=[str(Path(__file__).resolve()),*sys.argv[1:]]
    require(command[0] in (str(Path(sys.executable).absolute()),str(Path(sys.executable).resolve())) and command[1:] in (expected,['-B',*expected]),'exact direct queue argv required')
    require(q['lock_dir']==str(lock) and q['runner_sha256']==file_id(runner)['sha256'] and q['out']==str(Path(queue)),'original global runner/lock mismatch')
    require(q['owner']=='W3' and q['kind'] in ('diagnostic','validation') and q['cwd']==str(Path.cwd()),'wrong diagnostic queue owner/kind/cwd')
    fd=os.open(lock/'queue.lock',os.O_RDONLY)
    try:
        try:fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:pass
        else:raise ValueError('global lease is not held')
        record=json.loads(os.read(fd,65536))
    finally:os.close(fd)
    require(record['supervisor_pid']==os.getppid() and record['run_id']==q['run_id'] and record['status']=='running','not direct original lease child')
    return q

def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='action',required=True)
    a=sub.add_parser('run');a.add_argument('--out',type=Path,required=True);a.add_argument('--environment',type=Path,required=True);a.add_argument('--queue',type=Path,required=True);a.add_argument('--binding',type=Path,required=True);a.add_argument('--body',type=Path,action='append',required=True);a.add_argument('command',nargs=argparse.REMAINDER)
    a=sub.add_parser('selfcheck');a.add_argument('--out',type=Path,required=True);a.add_argument('--queue',type=Path,required=True)
    args=p.parse_args()
    try:
        q=lease_check(args.queue)
        if args.action=='selfcheck':
            require(q['timeout_seconds']==30,'dummy selfcheck outer cap must be30s')
            import dummy_selfcheck
            result=dummy_selfcheck.run(args.out)
        else:
            require(q['timeout_seconds']<=60,'single diag wrapper outercap exceeds60s')
            command=args.command[1:] if args.command[:1]==['--'] else args.command
            binding=json.loads(args.binding.read_bytes())
            require(binding['role'] in ('control','R1') and binding['instrumented'] is True and binding['eligible_for_performance'] is False,'actual diagnostic role/binding required')
            require({'source_identity','driver','binary','compile_receipt','hook'}<=set(binding['files']),'source/driver/binary/build/hook binding missing')
            before={n:file_id(v['path']) for n,v in binding['files'].items()}
            require(all(before[n]==v['identity'] for n,v in binding['files'].items()),'before diagnostic binding changed')
            result=supervise(command,args.out,json.loads(args.environment.read_bytes()),args.body,binding=binding,queue_request=q)
            after={n:file_id(v['path']) for n,v in binding['files'].items()}
            exclusive(args.out/'binding-after.json',{'before':before,'after':after,'matched':before==after})
            require(before==after,'after diagnostic binding changed; preserve failed run')
        print(json.dumps({'status':result['status'],'receipt':str(args.out/'receipt.json'),'dummy':args.action=='selfcheck'},sort_keys=True))
        return 0 if result['status'] in ('complete','passed') else 1
    except (ValueError,OSError,KeyError) as error:
        print(json.dumps({'status':'rejected','reason':type(error).__name__+': '+str(error)},sort_keys=True));return 1

if __name__=='__main__':raise SystemExit(main())
