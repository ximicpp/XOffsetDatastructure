#!/usr/bin/env python3
"""One finite C14 then R1 diagnostic job. Prepare is source-only; run is leased.

No retries, scheduling service, dummy execution, timing campaign or Q/F action.
"""
from __future__ import annotations
import time
ENTRY_NS=time.monotonic_ns()
import argparse
import ctypes
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import selectors
import shlex
import shutil
import signal
import sys

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
DOC=ROOT/'docs/coordination/c14-iteration-20261020-v2'
MATERIAL=Path('/root/xoffset-c14-iteration/20261020/v2/validation/common-diag-final-v1')
OUTPUT=Path('/root/xoffset-c14-iteration/20261020/v2/validation/phase-c-original-run-v1')
RUNNER=Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py')
LOCK=RUNNER.parents[2]/'build/vm-coordination'
AUDIT=Path('/root/xoffset-receive-completion/20261019/v1/functional-tools-v2')
GiB=1024**3
POLICY=dict(roles=['c14','r1'],compile_budget=2,native_budget=2,compile_seconds=120,native_seconds=20,
    outer_seconds=600,role_audit_seconds=60,seal_reserve_seconds=30,identity_and_seal_projection_seconds=100,
    peak_new_allocated_bytes=4*GiB,allocated_limit_bytes=4*GiB,guest_reserve_bytes=2*GiB,backing_reserve_bytes=2*GiB,
    address_space_bytes=16*GiB,rss_limit_bytes=16*GiB,log_limit_bytes=1024**2,compile_file_limit_bytes=128*1024**2,
    producer_per_role_bytes=64*1024**2,producer_both_roles_bytes=128*1024**2,
    expected_wire_bytes=17830872,retry=0,warmup=0,extra_validate=0,extra_emit=0,performance_inference=False)
ORDER=['main.begin','input.read.before','input.read.after','input.decode.before','input.decode.after','input.hash.before','input.hash.after',
    't0','t1','t2','t3','t4','emit.pod0','emit.pod1','emit.pod2','t5','t6','t7','t8',
    'postaudit.load.before','postaudit.load.after','postaudit.export.before','postaudit.export.after','postaudit.hash.before','postaudit.hash.after',
    'truth.replay.before','truth.replay.after','write.wire.before','write.wire.after','write.canonical.before','write.canonical.after',
    'write.queries.before','write.queries.after','write.result.before','write.result.after','main.end']
FIELDS=['canonical','inactive','active','owns','vectors','capacity_hits','nontrivial_misses','nonempty_misses','vector_data_reads']
LIMITATIONS=[
    'Diagnostic hooks/events/syscalls and stdout allocation contaminate these timings; no speed, AB, confidence, screening or Q/F claim.',
    'The t4-post-emit flush is charged to its adjacent diagnostic segment and is only attributable as observer work.',
    'peak_rss_after_audit_kib includes added replay and observer allocations; it is not the bare driver peak.',
    'Emit samples are nested intervals; preparation includes string/allocator/observer work, not pure malloc; overlapping faults must not be added twice.',
    'A retained last boundary localizes an interval; it does not prove that boundary caused a timeout.',
    'No current observation can retrospectively establish why the closed M1 window timed out.']

def need(v,m):
    if not v:raise ValueError(m)
def fid(p):
    p=Path(p);h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1024**2),b''):h.update(b)
    return {'bytes':p.stat().st_size,'sha256':h.hexdigest()}
def read(p):
    def pairs(items):
        d={}
        for k,v in items:need(k not in d,'duplicate JSON key');d[k]=v
        return d
    def bad(v):raise ValueError('nonfinite JSON '+v)
    return json.loads(Path(p).read_text(encoding='utf-8-sig'),object_pairs_hook=pairs,parse_constant=bad)
def encoded(v):return (json.dumps(v,sort_keys=True,indent=2,allow_nan=False)+'\n').encode()
def write(p,v):
    with Path(p).open('xb',buffering=0) as f:f.write(encoded(v))
def load_observer():
    sys.path.insert(0,str(HERE));import diag_wrapper as D;return D
def load_audit():
    sys.path.insert(0,str(AUDIT));import campaign as G;sys.path.pop(0);return G
def integer(v):return type(v) is int and 0<=v<2**64
def same_id(p,v):
    actual=fid(p);return actual['sha256']==v['sha256'] and ('bytes' not in v or int(v['bytes'])==actual['bytes'])
def collect_file(records,p):
    p=Path(p);need(p.is_absolute() and p.is_file(),'missing frozen file '+str(p));identity=fid(p);records[str(p)]=identity;records[str(p.resolve())]=identity
def collect_headers(records,p):
    p=Path(p)
    need(p.is_dir(),'configured include root unavailable: '+str(p))
    for f in p.rglob('*'):
        if f.is_file():collect_file(records,f)

def free_observation():
    mem={}
    for line in Path('/proc/meminfo').read_text().splitlines():
        k,v=line.split(':',1)
        if k in ('MemTotal','MemAvailable','SwapFree'):mem[k]=int(v.split()[0])*1024
    return dict(guest_free_bytes=shutil.disk_usage('/root').free,C_backing_free_bytes=shutil.disk_usage('/mnt/c').free,
        memory_bytes=mem,observed_monotonic_ns=str(time.monotonic_ns()))
def check_resources():
    r=free_observation();need(r['guest_free_bytes']>=6*GiB and r['C_backing_free_bytes']>=6*GiB,'4GiB bounded peak plus2GiB reserve cannot fit guest/backing');return r

def prepare(out,resource_out):
    need(not OUTPUT.exists(),'one C output prefix already exists')
    draft_path=DOC/'core/REQUESTS_PHASE_C_DRAFT.json';draft=read(draft_path)
    mpath=MATERIAL/'MATERIALIZATION.json';m=read(mpath)
    need(m['status']=='materialized_not_compiled_or_admitted' and draft['role_order']==['c14','r1'],'unexpected C materialization/proposal')
    need(m['emit_calls_per_run']==1 and m['truth_replay_emit_calls']==0 and m['same_driver_adapter_hook_event_header'] is True,'owning/output contract differs')
    files={};headers=[];roles={}
    inherited_path=os.environ.get('PATH','');ps=shutil.which('ps',path=inherited_path)
    need(ps is not None and Path(ps).is_absolute(),'actual existing runner PATH cannot resolve ps')
    for p in (Path(__file__),HERE/'diag_wrapper.py',HERE/'dummy_child.py',HERE/'dummy_selfcheck.py',HERE/'event_channel.hpp',HERE/'prepare_final_driver.py',HERE/'INTERFACE.json',RUNNER,Path(sys.executable).resolve(),Path('/usr/bin/python3'),Path(ps),mpath,draft_path,DOC/'validation/diag/SOURCE_READY.json',DOC/'validation/S0_RESULT.json',DOC/'reviews/C_OBSERVER_SCOPE_ACCEPTANCE.json',DOC/'reviews/OBSERVER_REQUIREMENTS.md',DOC/'core/DYNAMIC_ACCEPTANCE.md',DOC/'core/DYNAMIC_ACCEPTANCE_BRIDGE.json',ROOT/'tools/c14_iteration_20261020_v2/core/check_dynamic_acceptance.py'):
        collect_file(files,p)
    need(fid(HERE/'diag_wrapper.py')['sha256']=='48ddd6262b371a96ec1eabd2c57c57aa392e909e831869c5dae634de1d92fc34','accepted observer revision differs')
    need(fid(HERE/'INTERFACE.json')['sha256']=='7bbba10ea8351e6a3af576de7cbe8963c9733b5e144075444427b0fe7cddf20c','fixed36 interface differs')
    for p in (AUDIT/'campaign.py',AUDIT/'core.py',AUDIT/'protocol.py'):collect_file(files,p)
    acceptance=read(DOC/'reviews/C_OBSERVER_SCOPE_ACCEPTANCE.json')
    need(acceptance['status']=='component_source_and_affected_dummy_checks_passed' and acceptance['wrapper_sha256']==fid(HERE/'diag_wrapper.py')['sha256'],'observer component acceptance absent')
    old_interface=HERE/'revisions/targeted-v2/INTERFACE.json';collect_file(files,old_interface)
    bridge=acceptance['interface_identity_bridge'];old_data=read(old_interface);new_data=read(HERE/'INTERFACE.json')
    need(fid(old_interface)['sha256']==bridge['dummy_interface_sha256'] and fid(HERE/'INTERFACE.json')['sha256']==bridge['current_interface_sha256'],'accepted interface identity bridge differs')
    old_labels=old_data.pop('labels');new_labels=new_data.pop('labels')
    need(old_data==new_data and len(old_labels)==34 and len(new_labels)==36 and [v for v in new_labels if v not in ('truth.replay.before','truth.replay.after')]==old_labels,'interface change exceeds accepted two-label data scope')
    collect_file(files,DOC/'validation/diag/DUMMY_RESULT_v2.json')
    need(same_id(DOC/'validation/diag/DUMMY_RESULT_v2.json',{'sha256':acceptance['current_dummy_result_sha256']}),'accepted affected dummy result changed')
    for p,v in m['inputs'].items():need(same_id(p,v),'materialization provenance changed: '+p);collect_file(files,p)
    for p,v in m['tools'].items():need(same_id(p,v),'materialization tool changed: '+p);collect_file(files,p)
    for p,v in draft['runtime_files'].items():need(same_id(p,v),'fixed runtime changed: '+p);collect_file(files,p)
    for p in ('/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/bin/clang++-libcxx','/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/bin/clang++','/opt/p2996-toolchain/bin/clang++'):
        collect_file(files,p)
    old_profile=Path('/root/xoffset-receive-completion/20261019/v1/profile-normal-v2.json');collect_file(files,old_profile)
    for label,p in read(old_profile)['closure'].items():
        if label.startswith('tool-runtime-') or label.startswith('taskset-host-'):collect_file(files,p)
    # -MD does not enumerate the linker, shell launcher helpers, CRT or linker
    # scripts. Pin the already configured GNU11/system link scope explicitly.
    link_required=['/bin/sh','/usr/bin/dirname','/usr/bin/ld','/usr/bin/as','/lib64/ld-linux-x86-64.so.2',
        '/usr/lib/x86_64-linux-gnu/Scrt1.o','/usr/lib/x86_64-linux-gnu/crt1.o','/usr/lib/x86_64-linux-gnu/crti.o','/usr/lib/x86_64-linux-gnu/crtn.o',
        '/usr/lib/gcc/x86_64-linux-gnu/11/crtbeginS.o','/usr/lib/gcc/x86_64-linux-gnu/11/crtendS.o',
        '/usr/lib/gcc/x86_64-linux-gnu/11/libgcc.a','/usr/lib/gcc/x86_64-linux-gnu/11/libgcc_eh.a','/usr/lib/gcc/x86_64-linux-gnu/11/libgcc_s.so',
        '/usr/lib/x86_64-linux-gnu/libc.so','/usr/lib/x86_64-linux-gnu/libc_nonshared.a','/usr/lib/x86_64-linux-gnu/libm.so','/usr/lib/x86_64-linux-gnu/libpthread.a']
    for p in link_required:collect_file(files,p)
    link_library_patterns=['libc.so.*','libm.so.*','libmvec.so.*','libpthread.so.*','libgcc_s.so.*','libctf*.so*','libjansson.so*','libz.so*','libzstd.so*','libsframe.so*','libbfd*.so*','libopcodes*.so*','liblzma.so*']
    for directory in (Path('/lib/x86_64-linux-gnu'),Path('/usr/lib/x86_64-linux-gnu')):
        for pattern in link_library_patterns:
            for p in directory.glob(pattern):
                if p.is_file():collect_file(files,p)
    for role in ('c14','r1','m2'):
        r=m['roles'][role];base=Path(r['source'])
        for name,v in r['files'].items():need(same_id(base/name,v),'actual final source changed: '+role+'/'+name);collect_file(files,base/name)
        collect_file(files,base/'FINAL_SOURCE_IDENTITY.json')
        need(len(r['source_154'])==154 and len(r['production_14'])==14,'full154/14 identity missing')
        for name,v in r['source_154'].items():need(same_id(base/'source'/name,v),'actual154 source differs')
        if role=='m2':continue
        old=draft['role_jobs'][role]['final_validation_source'];argv=[str(v).replace(old,str(base)) for v in draft['role_jobs'][role]['compile_argv']]
        binary=OUTPUT/role/'diagnostic';argv[argv.index('-o')+1]=str(binary)
        argv+=['-MD','-MF',str(OUTPUT/role/'dependencies.d')]
        need('-O3' in argv and '-DNDEBUG' in argv and '-UNDEBUG' not in argv and not any('sanitize' in v for v in argv),'wrong diagnostic compile profile')
        for v in argv:
            if v.startswith('-I'):headers.append(v[2:])
        bodies={k:str(OUTPUT/role/'bodies'/n) for k,n in (('wire','final-wire.bin'),('canonical','final-canonical.bin'),('queries','queries.bin'),('native','native.json'))}
        native=[str(binary),'run','--wire',str(ROOT/'handoff/continuation-20261019-v1/portable-inputs/old-V8-initial-wire.bin'),'--trace',str(ROOT/'handoff/continuation-20261019-v1/portable-inputs/fixed-M/trace.bin'),'--wire-out',bodies['wire'],'--canonical-out',bodies['canonical'],'--queries-out',bodies['queries'],'--result',bodies['native']]
        adapter=(base/'experiment/adapters/xoffset/adapter.hpp').read_text();match=re.search(r'library_id\s*=\s*"([^"\n]+)"',adapter);need(match is not None,'actual adapter label missing')
        roles[role]=dict(source=str(base),compile_argv=argv,native_argv=native,binary=str(binary),bodies=bodies,
            library=match.group(1),environment=dict(PATH='/usr/bin:/bin',LANG='C',LC_ALL='C',TZ='UTC',TMPDIR=str(OUTPUT/role/'tmp')),
            production_14=r['production_14'],source_154=r['source_154'])
    headers+=['/opt/p2996-toolchain/include/c++/v1','/opt/p2996-toolchain/include/x86_64-unknown-linux-gnu/c++/v1','/opt/p2996-toolchain/lib/clang/21/include','/usr/include','/usr/lib/gcc/x86_64-linux-gnu/11/include']
    for p in sorted(set(headers)):collect_headers(files,p)
    fixture=ROOT/'handoff/continuation-20261019-v1/portable-inputs/fixed-M'
    for p in fixture.iterdir():
        if p.is_file():collect_file(files,p)
    collect_file(files,ROOT/'handoff/continuation-20261019-v1/portable-inputs/old-V8-initial-wire.bin')
    resources=dict(schema='phase-c-resources-v1',policy=POLICY,observation=check_resources(),admission_peak_bytes=4*GiB,
        bounded_items=dict(two_bodies=128*1024**2,two_binary_and_two_compile_temp_file_caps=512*1024**2,logs_and_metadata_budget=128*1024**2),
        compile_child_native_audit_projection_seconds=2*(120+2+20.5+60)+100,
        memory_policy='AS hard16GiB and observed RSS<=16GiB; current MemAvailable is recorded, not a guarantee of future allocation success.')
    need(resources['compile_child_native_audit_projection_seconds']<=600,'fixed total budget projection fails')
    write(resource_out,resources);collect_file(files,resource_out)
    f=dict(schema='phase-c-freeze-v1',status='draft_pending_independent_admission',policy=POLICY,output=str(OUTPUT),queue=str(OUTPUT/'queue'),
        materialization=str(mpath),materialization_identity=fid(mpath),fixed_files=files,include_roots=sorted(set(headers)),
        fixture=str(fixture),wire=str(ROOT/'handoff/continuation-20261019-v1/portable-inputs/old-V8-initial-wire.bin'),
        event_order=ORDER,roles=roles,resources=dict(path=str(resource_out),identity=fid(resource_out)),link_required=link_required,link_library_patterns=link_library_patterns,
        M2_control_predeclaration=dict(draft=draft['M2_control_predeclaration'],all_three_actual_sources_bound=True,unchanged_driver_adapter_hook_event_header=True),
        inherited_PATH=inherited_path,runner_ps_path=ps,runner_python_lexical_path='/usr/bin/python3',runtime_bridge=dict(profile=str(old_profile),current_A=str(DOC/'validation/S0_RESULT.json')),
        dynamic_acceptance_inputs=[str(DOC/'core/DYNAMIC_ACCEPTANCE.md'),str(DOC/'core/DYNAMIC_ACCEPTANCE_BRIDGE.json'),str(ROOT/'tools/c14_iteration_20261020_v2/core/check_dynamic_acceptance.py')],
        dynamic_acceptance_execution='offline after successful externally pinned role audits; no new native or repeated mechanism run',
        limitations=LIMITATIONS,performance_admitted=False)
    write(out,f);return {'status':'draft_frozen_not_admitted','freeze':str(out),'freeze_sha256':fid(out)['sha256'],'resources':str(resource_out),'fixed_files':len(files)}

def verify_files(f):
    result={}
    for p,v in f['fixed_files'].items():
        actual=fid(p);need(actual==v,'frozen input/tool/runtime changed: '+p);result[p]=actual
    return result

def leased(queue):
    q=read(queue/'request.json');expected=[str(Path(sys.executable).absolute()),'-B',str(Path(__file__).resolve()),*sys.argv[1:]]
    need(q['command']==expected,'runner must directly invoke exact Python/-B/coordinator/argv')
    need(q['owner']=='W3' and q['kind']=='diagnostic' and q['timeout_seconds']==600 and q['wait_seconds']==0 and q['lock_dir']==str(LOCK) and q['out']==str(queue) and q['runner_sha256']==fid(RUNNER)['sha256'] and q['cwd']==str(Path.cwd()),'original600s queue binding differs')
    fd=os.open(LOCK/'queue.lock',os.O_RDONLY)
    try:
        try:fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:pass
        else:raise ValueError('original shared lease is not held')
        holder=json.loads(os.read(fd,65536))
    finally:os.close(fd)
    need(holder['status']=='running' and holder['supervisor_pid']==os.getppid() and holder['run_id']==q['run_id'] and holder['owner']=='W3' and holder['kind']=='diagnostic','not direct current leased coordinator')
    need(os.getsid(0)==os.getpid(),'original supervisor session contract differs')
    return dict(request=q,request_identity=fid(queue/'request.json'),holder=holder,coordinator_pid=os.getpid(),session=os.getsid(0))

def allocated(root):
    seen=set();total=0
    for p in (root,*root.rglob('*')):
        st=p.lstat();need(not p.is_symlink(),'output contains symlink')
        if (st.st_dev,st.st_ino) not in seen:total+=st.st_blocks*512;seen.add((st.st_dev,st.st_ino))
    return total

def stop_owned(D,pid,session):
    members=D.group_members(pid,session);owned={m['pid'] for m in members if m['ppid']==os.getpid()}
    changed=True
    while changed:
        nxt=owned|{m['pid'] for m in members if m['ppid'] in owned};changed=nxt!=owned;owned=nxt
    signals=[]
    for m in reversed(members):
        if m['pid'] not in owned:continue
        now=next((v for v in D.group_members(pid,session) if v['pid']==m['pid']),None)
        if now is None or now['start_ticks']!=m['start_ticks']:continue
        try:os.kill(m['pid'],signal.SIGKILL);signals.append(m)
        except ProcessLookupError:pass
    return signals

def compile_one(D,role,job_deadline):
    argv=role['compile_argv'];rdir=Path(role['binary']).parent;start=time.monotonic_ns();deadline=start+120*10**9;cleanup=deadline+2*10**9
    need(cleanup+int(110.5*1e9)<job_deadline,'whole job cannot fit compile/native/audit/seal; no compile launch')
    write(rdir/'compile-attempt.json',dict(argv=argv,environment=role['environment'],started_monotonic_ns=str(start),deadline_monotonic_ns=str(deadline),timeout_seconds=120))
    sel=selectors.DefaultSelector() # Construct before fork: EMFILE cannot orphan a live compiler.
    lr,lw=os.pipe2(os.O_CLOEXEC);os.set_blocking(lr,False);session=os.getsid(0);pid=os.fork()
    if pid==0:
        try:
            os.setpgid(0,0);os.close(lr);fd=os.open('/dev/null',os.O_RDONLY);os.dup2(fd,0);os.close(fd);os.dup2(lw,1);os.dup2(lw,2);os.close(lw)
            resource.setrlimit(resource.RLIMIT_FSIZE,(POLICY['compile_file_limit_bytes'],)*2);resource.setrlimit(resource.RLIMIT_AS,(16*GiB,)*2)
            os.chdir(rdir);os.execve(argv[0],argv,role['environment'])
        except BaseException as e:
            try:os.write(2,str(e).encode()[:4096])
            except OSError:pass
            os._exit(127)
    os.close(lw);status=None;usage=None;returned=None;stored=0;received=0;reason=None;reap_count=0;signals=[];open_pipe=True
    try:
        try:os.setpgid(pid,pid)
        except ProcessLookupError:pass
        except PermissionError:pass
        sel.register(lr,selectors.EVENT_READ)
        with (rdir/'compile.log').open('xb',buffering=0) as log:
            while status is None or open_pipe:
                now=time.monotonic_ns()
                if status is None:
                    got,s,u=os.wait4(pid,os.WNOHANG)
                    if got:status=s;usage=u;returned=time.monotonic_ns();reap_count+=1
                    elif now>=deadline:reason=reason or 'timeout';signals+=stop_owned(D,pid,session)
                    if now>=cleanup:reason=reason or 'unreaped_at_cleanup_deadline';break
                if status is not None and now-returned>250_000_000 and open_pipe:reason=reason or 'descendant_pipe_open';signals+=stop_owned(D,pid,session);break
                for _,_mask in sel.select(.02):
                    try:b=os.read(lr,65536)
                    except BlockingIOError:continue
                    if not b:sel.unregister(lr);open_pipe=False;continue
                    received+=len(b);keep=b[:max(0,1024**2-stored)]
                    if keep:D.persist(log,keep);stored+=len(keep)
                    if len(keep)!=len(b):reason=reason or 'log_limit';signals+=stop_owned(D,pid,session)
    except BaseException as e:reason=reason or type(e).__name__+': '+str(e);signals+=stop_owned(D,pid,session)
    finally:
        if status is None:
            signals+=stop_owned(D,pid,session)
            while time.monotonic_ns()<cleanup:
                try:got,s,u=os.wait4(pid,os.WNOHANG)
                except ChildProcessError:reason=reason or 'ECHILD';break
                if got:status=s;usage=u;returned=time.monotonic_ns();reap_count+=1;break
                time.sleep(.005)
        sel.close();os.close(lr)
    left=D.group_members(pid,session);adopted=[];until=min(cleanup,time.monotonic_ns()+250_000_000)
    while left and time.monotonic_ns()<until:
        signals+=stop_owned(D,pid,session)
        for member in left:
            if member['pid']==pid:continue
            try:
                got,s,_u=os.wait4(member['pid'],os.WNOHANG)
                if got:adopted.append(dict(pid=got,status=s))
            except ChildProcessError:pass
        time.sleep(.005);left=D.group_members(pid,session)
    if left:reason=reason or 'cleanup_incomplete'
    if returned is None or returned>deadline:reason=reason or 'compile120s_exceeded'
    rc=os.waitstatus_to_exitcode(status) if status is not None else None
    result=dict(status='passed' if rc==0 and reason is None else 'failed',argv=argv,environment=role['environment'],pid=pid,session=session,
        started_monotonic_ns=str(start),returned_monotonic_ns=str(returned) if returned is not None else None,timeout_seconds=120,reason=reason,returncode=rc,
        successful_wait4_calls=reap_count,cleanup_remaining=left,adopted_reaped=adopted,signals=signals,log_received_bytes=received,log_persisted_bytes=stored,
        max_rss_kib=usage.ru_maxrss if usage is not None else None,binary=fid(role['binary']) if Path(role['binary']).is_file() else None)
    write(rdir/'compile-receipt.json',result);need(result['status']=='passed' and reap_count==1,'compile failed; stop remaining C roles')
    need(usage.ru_maxrss*1024<=16*GiB,'compiler RSS exceeded16GiB')
    return result

def dependencies(role,frozen):
    dep=Path(role['binary']).parent/'dependencies.d';raw=dep.read_text().replace('\\\n',' ');need(':' in raw,'compiler dependency output absent')
    paths=shlex.split(raw.split(':',1)[1],posix=True);ids={}
    for p in paths:
        resolved=Path(p) if Path(p).is_absolute() else Path(role['binary']).parent/p;resolved=str(resolved.resolve())
        need(resolved in frozen,'actual compiler dependency not frozen: '+resolved);need(fid(resolved)==frozen[resolved],'compiled dependency changed: '+resolved);ids[resolved]=frozen[resolved]
    need(str(Path(role['source'])/'experiment/driver.cpp') in ids,'actual final driver dependency absent')
    need(str(Path(role['source'])/'experiment/event_channel.hpp') in ids,'event channel not compiled')
    need(str(Path(role['source'])/'source/c14_common_diag.hpp') in ids,'core POD hook not compiled')
    return dict(file=fid(dep),members=ids)

def check_counts(v):
    need(set(v)==set(FIELDS) and all(integer(v[k]) for k in FIELDS),'counter schema/value differs')
    need(v['canonical']==v['inactive']+v['active'],'canonical classification not conserved')
    need(v['capacity_hits']<=v['vectors'] and v['nontrivial_misses']<=v['vectors']-v['capacity_hits'] and v['nonempty_misses']<=v['nontrivial_misses'],'owns counter subset not conserved')
def audit_role(D,G,role,wr,f):
    begin=time.monotonic_ns();need(wr['status']=='complete' and wr['returncode']==0 and not wr['timed_out'] and wr['kill_reason'] is None and wr['reaper_error'] is None and wr['successful_wait4_calls']==1 and wr['cleanup_remaining']==[],'observer/child failed; stop remaining roles')
    start=int(wr['started_monotonic_ns']);end=int(wr['returned_monotonic_ns'])
    need(end-start<=20*10**9 and end<=int(wr['deadline_monotonic_ns']),'true wait4 child exceeded20s')
    need(wr['command']==role['native_argv'] and wr['max_rss_kib']*1024<=16*GiB,'child argv/RSS differs')
    rdir=Path(role['binary']).parent;observe=rdir/'observe';raw=(observe/'events.raw').read_bytes();lines=raw.splitlines(keepends=True)
    need(len(lines)==36 and [D.parse_event(v)['label'] for v in lines]==ORDER,'raw event order/count differs')
    need(len(wr['events'])==36 and [v['label'] for v in wr['events']]==ORDER and wr['observer_complete'] is True,'parsed fixed36 stream incomplete')
    previous=start;received=start;events={}
    for i,(line,event) in enumerate(zip(lines,wr['events'])):
        parsed=D.parse_event(line);need(parsed=={k:event[k] for k in ('sequence','child_monotonic_ns','label','values')} and parsed['sequence']==i,'event raw/parsed/seq differs')
        child=int(parsed['child_monotonic_ns']);recv=int(event['received_monotonic_ns'])
        need(previous<=child<=end and received<=recv<=int(wr['finished_monotonic_ns']) and child<=recv,'event monotonic/causal bounds differ');previous=child;received=recv;events[event['label']]=event
    need((observe/'stderr.raw').stat().st_size==0,'native stderr is nonempty')
    records=[G.C.parse(v) for v in (observe/'stdout.raw').read_bytes().splitlines()]
    need(len(records)==3 and [v['kind'] for v in records]==['c14-emit-samples-v1','c14-common-counts-v1','c14-nine-stage-truth-v1'],'stdout diagnostic JSON inventory differs')
    emit,counts,truth=records
    need(set(emit)=={'kind','seen','invocations','samples','event_errors','last_errno','channel_clock_status','mask_status','restore_status'},'emit JSON schema differs')
    need(integer(emit['seen']) and integer(emit['invocations']) and emit['seen']==511 and emit['invocations']==1 and len(emit['samples'])==9,'owning invocation/seen/sample count differs')
    previous=int(events['t3']['child_monotonic_ns']);last_values=None
    keys={'index','ns','utime_us','stime_us','minor','major','bytes','clock_status','rusage_status'}
    for i,s in enumerate(emit['samples']):
        need(set(s)==keys and all(integer(v) for v in s.values()) and s['index']==i and s['clock_status']==s['rusage_status']==0,'emit sample schema/status differs')
        need(previous<=s['ns']<=int(events['t4']['child_monotonic_ns']),'emit sample clock outside owning interval');previous=s['ns']
        if last_values is not None:need(all(s[k]>=last_values[k] for k in ('utime_us','stime_us','minor','major')),'emit cumulative resource reversed')
        last_values=s
    at=emit['samples'];wire=POLICY['expected_wire_bytes']
    need(at[0]['bytes']==0 and at[1]['bytes']==at[6]['bytes'] and at[2]['bytes']==at[5]['bytes'] and at[1]['bytes']+at[2]['bytes']==wire and all(at[i]['bytes']==wire for i in (3,4,7,8)),'emit byte ledger differs')
    need(events['emit.pod0']['values']==[511,1,at[0]['ns'],at[8]['ns']] and events['emit.pod1']['values']==[at[i]['ns'] for i in (3,4,5,6)] and events['emit.pod2']['values']==[at[i]['minor'] for i in (0,4,6,8)],'emit external POD differs from full samples')
    need(set(counts)=={'kind','mutation_seen','emit_seen','emit_invocations','counts','phases','mutations','event_errors','last_errno','channel_clock_status','mask_status','restore_status'},'count JSON schema differs')
    need(all(integer(counts[k]) for k in ('mutation_seen','emit_seen','emit_invocations')) and counts['mutation_seen']==1022 and counts['emit_seen']==511 and counts['emit_invocations']==1 and len(counts['phases'])==9 and len(counts['mutations'])==10,'counts mask/inventory differs')
    for v in [counts['counts'],*counts['phases'],*counts['mutations']]:check_counts(v)
    need(all(counts['phases'][0][k]==0 for k in FIELDS) and counts['phases'][8]==counts['counts'],'phase counters do not bracket primary')
    for a,b in zip(counts['phases'],counts['phases'][1:]):need(all(a[k]<=b[k] for k in FIELDS),'phase cumulative counters reversed')
    for a,b in zip(counts['mutations'],counts['mutations'][1:]):need(all(a[k]<=b[k] for k in FIELDS),'mutation cumulative counters reversed')
    need(all(counts['mutations'][0][k]==0 for k in FIELDS) and counts['mutations'][9]==counts['phases'][3],'mutation scopes do not bracket primary mutate')
    def delta(a,b):
        need(all(a[k]>=b[k] for k in FIELDS),'scope cumulative counts reversed')
        v={k:a[k]-b[k] for k in FIELDS};check_counts(v);return v
    phase_deltas=[delta(counts['phases'][i+1],counts['phases'][i]) for i in range(8)]
    mutation_deltas=[delta(counts['mutations'][i],counts['phases'][2] if i==1 else counts['mutations'][i-1]) for i in range(1,10)]
    need(all(sum(v[k] for v in mutation_deltas)==phase_deltas[2][k] for k in FIELDS),'nine stages do not conserve primary mutate (stage1 must subtractP2)')
    for v in (emit,counts):need(all(integer(v[k]) and v[k]==0 for k in ('event_errors','last_errno','channel_clock_status','mask_status','restore_status')),'producer channel/syscall error')
    fixture=G.C.read(Path(f['fixture'])/'fixture.json')
    need(set(truth)=={'kind','status','stages','query_before','query_after','owning_emit_calls','work','replay_resources_before_final_JSON_flush'},'replay JSON schema differs')
    need(truth['status']=='passed' and truth['stages']==[v['canonical_sha256'] for v in fixture['stages']] and truth['query_before'] is True and truth['query_after'] is True and integer(truth['owning_emit_calls']) and truth['owning_emit_calls']==0,'independent9-stage replay truth differs')
    expected=dict(input_reads=1,loads=1,initial_exports=1,initial_trace_check=1,stage_mutate_calls=9,stage_exports=9,stage_hashes=9,mutation_operations=fixture['mutation_count'],adapter_query_calls=2,queries_per_call=fixture['query_count'])
    need(truth['work']==expected and all(integer(v) for v in truth['work'].values()),'replay workload differs')
    rr=truth['replay_resources_before_final_JSON_flush'];need(set(rr)=={'start_ns','end_ns','user_us','system_us','minor_faults','major_faults','start_clock_status','start_rusage_status','end_clock_status','end_rusage_status'} and all(integer(v) for v in rr.values()),'replay resource schema differs')
    need(int(events['truth.replay.before']['child_monotonic_ns'])<=rr['start_ns']<=rr['end_ns']<=int(events['truth.replay.after']['child_monotonic_ns']) and all(rr[k]==0 for k in ('start_clock_status','start_rusage_status','end_clock_status','end_rusage_status')),'replay resource boundaries/status differs')
    need(rr['minor_faults']<=wr['minor_faults'] and rr['major_faults']<=wr['major_faults'],'replay faults exceed full-child observation')
    bodies={k:Path(v) for k,v in role['bodies'].items()};need(sum(v.stat().st_size for v in bodies.values())<=64*1024**2,'actual64MiB producer role body cap exceeded')
    # native_result requires a child interval, not the wrapper's later seal time.
    native_receipt=dict(started_monotonic_ns=wr['started_monotonic_ns'],finished_monotonic_ns=wr['returned_monotonic_ns'],timeout_seconds=20)
    row=G.native_result(bodies['native'],dict(library=role['library']),Path(f['fixture']),Path(f['wire']),bodies['wire'],bodies['canonical'],bodies['queries'],native_receipt,17830872)
    need(time.monotonic_ns()-begin<=60*10**9,'fixed role audit60s exceeded')
    return dict(status='passed',native=row,emission=emit,mechanism=counts,replay=truth,body_identities={k:fid(v) for k,v in bodies.items()},
        observer_receipt=fid(observe/'receipt.json'),native_child_receipt=native_receipt,wrapper_finished_monotonic_ns=wr['finished_monotonic_ns'],
        count_conservation=dict(status='passed',phase_deltas=phase_deltas,mutation_deltas=mutation_deltas,stage1_baseline='M1-P2',read_sum={k:phase_deltas[1][k]+phase_deltas[6][k] for k in FIELDS}),
        audit_seconds=(time.monotonic_ns()-begin)/1e9,diagnostic_only=True,mechanism_gate_outcome='not_run_use_predeclared_offline_checker',limitations=LIMITATIONS)

def run(frozen,external,admission,queue):
    need(fid(frozen)['sha256']==external,'external C freeze pin differs');f=read(frozen)
    need(f['schema']=='phase-c-freeze-v1' and f['policy']==POLICY and f['event_order']==ORDER and f['output']==str(OUTPUT) and queue==OUTPUT/'queue','fixed C scope differs')
    need(os.environ.get('PATH','')==f['inherited_PATH'] and shutil.which('ps')==f['runner_ps_path'],'runner inherited PATH/ps resolution changed')
    a=read(admission);need(a['kind']=='phase-c-independent-admission' and a['status']=='passed' and a['freeze_sha256']==external and a['R1_B_passed'] is True and a['interface36_scope_accepted'] is True and a['resources_passed'] is True and a['final_source_and_tools_passed'] is True,'independent same-freeze C admission missing')
    need({'R1_B','interface36_acceptance','source_review','resource_review'}<=set(a['evidence']),'concrete admission evidence references missing')
    for ref in a['evidence'].values():need(set(ref)=={'path','identity'} and same_id(ref['path'],ref['identity']),'admission evidence changed')
    need(fid(a['evidence']['interface36_acceptance']['path'])==fid(DOC/'reviews/C_OBSERVER_SCOPE_ACCEPTANCE.json'),'actual reviewed interface bridge reference differs')
    lease=leased(queue);need(set(p.name for p in OUTPUT.iterdir())=={'queue'},'C attempt prefix already used')
    D=load_observer();G=load_audit();need(ctypes.CDLL(None).prctl(36,1,0,0,0)==0,'coordinator subreaper unavailable')
    deadline=ENTRY_NS+600*10**9;write(OUTPUT/'lease.json',lease);shutil.copyfile(frozen,OUTPUT/'freeze.json');shutil.copyfile(admission,OUTPUT/'admission.json')
    before=verify_files(f);write(OUTPUT/'identity-before.json',before);write(OUTPUT/'resources-before.json',check_resources())
    attempted=[];completed=[];error=None
    try:
        for role_name in POLICY['roles']:
            need(deadline-time.monotonic_ns()>=int(232.5*1e9),'remaining600s cannot fit one fixed role; stop without launching')
            check_resources();need(allocated(OUTPUT)<4*GiB,'C physical allocated ceiling exceeded')
            role=f['roles'][role_name];rdir=OUTPUT/role_name;rdir.mkdir();(rdir/'tmp').mkdir();(rdir/'bodies').mkdir();attempted.append(role_name)
            compiled=compile_one(D,role,deadline);dep=dependencies(role,f['fixed_files']);write(rdir/'compiled-dependencies.json',dep)
            need(deadline-time.monotonic_ns()>=int(110.5*1e9),'whole job cannot fit native20.5/audit60/seal30; do not launch')
            verify_files(f);check_resources();need(deadline-time.monotonic_ns()>=int(110.5*1e9),'identity recheck consumed native/audit/seal reserve; do not launch')
            binding=dict(role='control' if role_name=='c14' else 'R1',instrumented=True,eligible_for_performance=False,
                files={k:dict(path=str(p),identity=fid(p)) for k,p in {'source_identity':Path(role['source'])/'FINAL_SOURCE_IDENTITY.json','driver':Path(role['source'])/'experiment/driver.cpp','binary':Path(role['binary']),'compile_receipt':rdir/'compile-receipt.json','hook':Path(role['source'])/'source/c14_common_diag.hpp','event_channel':Path(role['source'])/'experiment/event_channel.hpp'}.items()})
            write(rdir/'native-attempt.json',dict(argv=role['native_argv'],binding=binding,job_deadline_monotonic_ns=str(deadline)))
            wr=D.supervise(role['native_argv'],rdir/'observe',role['environment'],list(role['bodies'].values()),binding=binding,queue_request=lease['request'],absolute_job_deadline_ns=deadline)
            need(all(same_id(v['path'],v['identity']) for v in binding['files'].values()),'binary/build/source binding changed across child')
            result=audit_role(D,G,role,wr,f);write(rdir/'AUDIT.json',result)
            need(allocated(OUTPUT)<=4*GiB,'C physical allocated ceiling exceeded');verify_files(f);completed.append(role_name)
    except BaseException as e:error=type(e).__name__+': '+str(e)
    try:after=verify_files(f)
    except BaseException as e:after={'identity_error':str(e)};error=error or 'final input/tool/runtime identity mismatch'
    write(OUTPUT/'identity-after.json',after)
    try:
        write(OUTPUT/'resources-after.json',check_resources());need(allocated(OUTPUT)+1024**2<=4*GiB,'final physical ceiling lacks1MiB receipt reserve')
    except BaseException as e:error=error or 'final resource check failed: '+str(e)
    if time.monotonic_ns()>deadline:error=error or 'whole C job600s exceeded'
    result=dict(schema='phase-c-result-v1',status='passed' if error is None and completed==['c14','r1'] else 'failed',error=error,
        attempted_roles=attempted,completed_roles=completed,unexecuted_roles=POLICY['roles'][len(attempted):],
        compile_attempts=[r for r in attempted if (OUTPUT/r/'compile-attempt.json').exists()],native_attempts=[r for r in attempted if (OUTPUT/r/'native-attempt.json').exists()],
        freeze_sha256=external,started_monotonic_ns=str(ENTRY_NS),finished_monotonic_ns=str(time.monotonic_ns()),elapsed_seconds=(time.monotonic_ns()-ENTRY_NS)/1e9,
        actual_allocated_bytes=allocated(OUTPUT),limitations=LIMITATIONS,timing_samples=0,performance_admitted=False,mechanism_gate_outcome='not_run_use_predeclared_offline_checker',outer_runner_cleanup_still_required=True)
    write(OUTPUT/'RESULT.json',result);need(result['status']=='passed','C stopped; preserve prefix: '+str(error));return result

def main():
    p=argparse.ArgumentParser(description=__doc__);s=p.add_subparsers(dest='action',required=True)
    a=s.add_parser('prepare');a.add_argument('--out',type=Path,required=True);a.add_argument('--resources-out',type=Path,required=True)
    a=s.add_parser('run');a.add_argument('--frozen',type=Path,required=True);a.add_argument('--freeze-sha256',required=True);a.add_argument('--admission',type=Path,required=True);a.add_argument('--queue',type=Path,required=True)
    a=p.parse_args()
    try:
        r=prepare(a.out,a.resources_out) if a.action=='prepare' else run(a.frozen,a.freeze_sha256,a.admission,a.queue)
        print(json.dumps(r,sort_keys=True));return 0
    except (ValueError,OSError,KeyError) as e:print(json.dumps({'status':'rejected','reason':type(e).__name__+': '+str(e)}));return 1

if __name__=='__main__':raise SystemExit(main())
