#!/usr/bin/env python3
"""One M2 candidate against the predeclared, completed C14 control. No retries."""
from __future__ import annotations
import time
ENTRY_NS=time.monotonic_ns()
import argparse
import ctypes
import fcntl
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys

HERE=Path(__file__).resolve().parent
C_SHA='9d7b3585cdc05a2c3444b931702da27bad9c914944478fcf0b61a878f86e60bb'
if hashlib.sha256((HERE/'phase_c_job.py').read_bytes()).hexdigest()!=C_SHA:
    raise ValueError('frozen C implementation changed before import')
sys.path.insert(0,str(HERE))
import phase_c_job as C

DOC=C.DOC/'validation/diag'
OUTPUT=Path('/root/xoffset-c14-iteration/20261020/v2/validation/m2-mechanism-original-run-v1')
CFREEZE=DOC/'C_FREEZE_DRAFT.json'
CRESULT=DOC/'C_RESULT.json'
MDRAFT=C.DOC/'core/REQUESTS_M2_MECHANISM_DRAFT.json'
CHECKER=C.ROOT/'tools/c14_iteration_20261020_v2/core/check_dynamic_acceptance.py'
CF_SHA='250ce7daf7e1fb20513c8977928524c008aa6079211dbb66b9f5ef7efc4c7f0c'
CONTROL_SHA='7b834986bd69ea3c8ca0eb0fe0e6940da3bf44f52abc8d69922f5e2a0b9af503'
POLICY=dict(C.POLICY,roles=['m2'],compile_budget=1,native_budget=1,outer_seconds=300,
    control_new_compiles_or_runs=0,identity_and_seal_projection_seconds=90,
    producer_both_roles_bytes=64*1024**2)
PROJECTION=120+2+20.5+60+90 # 292.5; includes checker, identity and seal work.

def core_pin():
    C.need(C.fid(HERE/'phase_c_job.py')['sha256']==C_SHA,'frozen C implementation changed')

def collect_ref(files,ref):
    C.need(set(ref)=={'path','identity'} and C.same_id(ref['path'],ref['identity']),'completed control reference changed')
    C.collect_file(files,ref['path'])

def control_evidence(files,cf):
    r=C.read(CRESULT)
    C.need(r['status']=='passed' and r['actual_compiles']==2 and r['actual_diagnostic_native']==2 and r['retries']==0 and r['source_runtime_before_after_unchanged'] is True,'phase C did not complete exactly two audited runs')
    C.need(r['cleanup']==dict(live=[],process_groups=[],zombies=[]) and r['actual_outer_seconds']<=600,'phase C outer cleanup/cap failed')
    C.need(r['freeze']['path']==str(CFREEZE) and r['freeze']['identity']['sha256']==CF_SHA,'wrong completed C freeze')
    for key in ('freeze','admission','internal_result','queue_manifest','queue_request','queue_result'):collect_ref(files,r[key])
    qpath=Path(r['queue_request']['path']).parent;q=C.read(qpath/'request.json');qr=C.read(qpath/'result.json')
    C.need(q['run_id']==qr['run_id']==r['run_id'] and q['owner']==qr['owner']=='W3' and q['kind']==qr['kind']=='diagnostic' and q['timeout_seconds']==600 and q['wait_seconds']==0,'completed C queue identity differs')
    expected=['/usr/bin/python3','-B',str(HERE/'phase_c_job.py'),'run','--frozen',str(CFREEZE),'--freeze-sha256',CF_SHA,'--admission',r['admission']['path'],'--queue',str(qpath)]
    C.need(q['command']==expected and q['runner_sha256']==C.fid(C.RUNNER)['sha256'] and q['lock_dir']==str(C.LOCK) and q['out']==str(qpath),'completed C exact original command proof differs')
    C.need(qr['status']=='complete' and qr['reason']=='command_exit' and qr['command_returncode']==0 and qr['cleanup']==r['cleanup'],'completed C runner failure')
    manifest=C.read(qpath/'manifest.json')['files'];names={v['path'] for v in manifest}
    C.need(len(manifest)==6 and names=={'request.json','result.json','events.json','stdout.log','stderr.log','supervisor.log'},'C queue manifest inventory differs')
    C.need({p.name for p in qpath.iterdir()}==names|{'manifest.json'},'C queue extra/missing files')
    for v in manifest:
        p=qpath/v['path'];C.need(C.fid(p)=={k:v[k] for k in ('bytes','sha256')},'completed C queue byte mismatch');C.collect_file(files,p)
    control=r['roles']['c14'];C.need(control['audit']['identity']['sha256']==CONTROL_SHA,'wrong shared C14 control audit')
    for key in ('audit','compile','observer'):collect_ref(files,control[key])
    audit=C.read(control['audit']['path']);C.need(audit['status']=='passed' and audit['diagnostic_only'] is True,'shared control audit failed')
    role=cf['roles']['c14'];base=Path(role['binary']).parent
    for p in (base/'native-attempt.json',base/'compiled-dependencies.json',base/'dependencies.d',base/'compile-attempt.json',base/'compile.log',base/'observe/events.raw',base/'observe/stdout.raw',base/'observe/stderr.raw'):
        C.collect_file(files,p)
    native=C.read(base/'native-attempt.json')
    C.need(native['argv']==role['native_argv'],'shared control original argv differs')
    for ref in native['binding']['files'].values():collect_ref(files,ref)
    compile_receipt=C.read(control['compile']['path'])
    C.need(compile_receipt['argv']==role['compile_argv'] and compile_receipt['environment']==role['environment'] and compile_receipt['status']=='passed' and compile_receipt['binary']==C.fid(role['binary']),'shared control compiled binary/profile differs')
    for name,p in role['bodies'].items():
        C.need(C.fid(p)==audit['body_identities'][name],'shared control body changed');C.collect_file(files,p)
    return dict(result=dict(path=str(CRESULT),identity=C.fid(CRESULT)),audit=control['audit'],binary=dict(path=role['binary'],identity=C.fid(role['binary'])),
        compile=control['compile'],observer=control['observer'],run_id=r['run_id'],queue_manifest=r['queue_manifest'])

def prepare(out,resources_out,request_out):
    core_pin();C.need(not OUTPUT.exists(),'one M2 prefix already exists')
    C.need(C.fid(CFREEZE)['sha256']==CF_SHA,'predeclared C freeze changed');cf=C.read(CFREEZE)
    C.need(cf['schema']=='phase-c-freeze-v1' and cf['policy']==C.POLICY and cf['event_order']==C.ORDER,'C source/observer policy differs')
    pre=cf['M2_control_predeclaration']
    C.need(pre['all_three_actual_sources_bound'] is True and pre['unchanged_driver_adapter_hook_event_header'] is True and pre['draft']['declared_before_any_C_run'] is True and pre['draft']['control_role']=='c14','M2 common control was not predeclared')
    C.need(os.environ.get('PATH','')==cf['inherited_PATH'] and shutil.which('ps')==cf['runner_ps_path'],'original PATH/ps bridge differs')
    files=dict(cf['fixed_files']);C.verify_files(cf)
    for p in (Path(__file__),CFREEZE,CRESULT,MDRAFT):C.collect_file(files,p)
    draft=C.read(MDRAFT)
    C.need(draft['candidate_compiles']==draft['native_budget']==1 and draft['control_new_compiles_or_runs']==0 and draft['timeout_seconds']==300 and draft['native_timeout_seconds']==20 and draft['retry']==0 and draft['timing_admitted'] is False and draft['screening_admitted'] is False,'M2 one-candidate draft differs')
    for p,v in draft['input_files'].items():C.need(C.same_id(p,v),'M2 draft input changed: '+p);C.collect_file(files,p)
    control=control_evidence(files,cf)
    material=C.read(cf['materialization']);mr=material['roles']['m2'];source=Path(mr['source'])
    control_source=Path(cf['roles']['c14']['source'])
    for name in ('experiment/driver.cpp','experiment/adapters/xoffset/adapter.hpp','source/c14_common_diag.hpp','experiment/event_channel.hpp'):
        C.need(C.fid(source/name)==C.fid(control_source/name),'M2 cannot share C14 control after instrumentation change: '+name)
    for name,v in mr['files'].items():C.need(C.same_id(source/name,v) and files.get(str(source/name))==C.fid(source/name),'actual M2 source absent from original C closure')
    old=draft['role_job']['final_validation_source'];argv=[str(v).replace(old,str(source)) for v in draft['role_job']['compile_argv']]
    binary=OUTPUT/'m2/diagnostic';argv[argv.index('-o')+1]=str(binary);argv+=['-MD','-MF',str(OUTPUT/'m2/dependencies.d')]
    reference=cf['roles']['c14']['compile_argv']
    normalized=[v.replace(str(source),str(control_source)).replace(str(OUTPUT/'m2'),str(Path(cf['roles']['c14']['binary']).parent)) for v in argv]
    C.need(normalized==reference,'candidate compile differs beyond frozen source/output role paths')
    role=json.loads(json.dumps(cf['roles']['c14']))
    role.update(source=str(source),compile_argv=argv,binary=str(binary),production_14=mr['production_14'],source_154=mr['source_154'])
    old_output=str(Path(cf['roles']['c14']['binary']).parent)
    role['native_argv']=[v.replace(old_output,str(OUTPUT/'m2')) for v in role['native_argv']]
    role['bodies']={k:v.replace(old_output,str(OUTPUT/'m2')) for k,v in role['bodies'].items()}
    role['environment']['TMPDIR']=str(OUTPUT/'m2/tmp')
    resources=dict(schema='m2-mechanism-resources-v1',policy=POLICY,observation=C.check_resources(),admission_peak_bytes=4*C.GiB,
        projection_seconds=PROJECTION,projection=dict(compile=120,compile_cleanup=2,native_and_cleanup=20.5,audit=60,identities_checker_and_seal=90),
        producer_per_role_bytes=64*1024**2,control_new_output_bytes=0,original_C_resource_policy=cf['resources'])
    C.need(PROJECTION<=300,'M2 finite budget cannot fit; do not launch')
    C.write(resources_out,resources);C.collect_file(files,resources_out)
    freeze=dict(schema='m2-mechanism-freeze-v1',status='draft_pending_independent_admission',policy=POLICY,output=str(OUTPUT),queue=str(OUTPUT/'queue'),
        fixed_files=files,role=role,fixture=cf['fixture'],wire=cf['wire'],event_order=C.ORDER,control=control,
        original_C_freeze=dict(path=str(CFREEZE),identity=C.fid(CFREEZE)),resources=dict(path=str(resources_out),identity=C.fid(resources_out)),
        inherited_PATH=cf['inherited_PATH'],runner_ps_path=cf['runner_ps_path'],runner_python_lexical_path=cf['runner_python_lexical_path'],
        dynamic_acceptance_inputs=cf['dynamic_acceptance_inputs'],checker=str(CHECKER),limitations=C.LIMITATIONS,performance_admitted=False)
    C.write(out,freeze);sha=C.fid(out)['sha256']
    admission=DOC/'M2_MECHANISM_ADMISSION.json'
    command=['/usr/bin/python3','-B',str(Path(__file__).resolve()),'run','--frozen',str(out),'--freeze-sha256',sha,'--admission',str(admission),'--queue',str(OUTPUT/'queue')]
    C.write(request_out,dict(schema='m2-mechanism-request-draft-v1',status='not_submitted_requires_independent_same_freeze_admission',
        runner=str(C.RUNNER),owner='W3',kind='diagnostic',timeout_seconds=300,wait_seconds=0,lock_dir=str(C.LOCK),cwd=str(Path.cwd()),out=str(OUTPUT/'queue'),command=command,
        freeze=dict(path=str(out),identity=C.fid(out)),resources=dict(path=str(resources_out),identity=C.fid(resources_out)),compiles=1,native_runs=1,new_control_runs=0,retry=0))
    return dict(status='draft_frozen_not_admitted',freeze=str(out),freeze_sha256=sha,resources=str(resources_out),request=str(request_out),fixed_files=len(files))

def leased(queue):
    q=C.read(queue/'request.json');expected=[str(Path(sys.executable).absolute()),'-B',str(Path(__file__).resolve()),*sys.argv[1:]]
    C.need(q['command']==expected and q['owner']=='W3' and q['kind']=='diagnostic' and q['timeout_seconds']==300 and q['wait_seconds']==0 and q['lock_dir']==str(C.LOCK) and q['out']==str(queue) and q['cwd']==str(Path.cwd()) and q['runner_sha256']==C.fid(C.RUNNER)['sha256'],'exact original300s W3 direct command lease differs')
    fd=os.open(C.LOCK/'queue.lock',os.O_RDONLY)
    try:
        try:fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:pass
        else:raise ValueError('original shared lease not held')
        holder=json.loads(os.read(fd,65536))
    finally:os.close(fd)
    C.need(holder['status']=='running' and holder['supervisor_pid']==os.getppid() and holder['run_id']==q['run_id'] and holder['owner']=='W3' and holder['kind']=='diagnostic' and os.getsid(0)==os.getpid(),'not direct original supervised job')
    return dict(request=q,request_identity=C.fid(queue/'request.json'),holder=holder,coordinator_pid=os.getpid(),session=os.getsid(0))

def run(frozen,external,admission,queue):
    core_pin();C.need(C.fid(frozen)['sha256']==external,'external M2 freeze pin differs');f=C.read(frozen)
    C.need(f['schema']=='m2-mechanism-freeze-v1' and f['policy']==POLICY and f['event_order']==C.ORDER and f['output']==str(OUTPUT) and queue==OUTPUT/'queue','fixed one-M2 scope differs')
    C.need(os.environ.get('PATH','')==f['inherited_PATH'] and shutil.which('ps')==f['runner_ps_path'],'runner inherited PATH/ps changed')
    a=C.read(admission)
    C.need(a['kind']=='m2-mechanism-independent-admission' and a['status']=='passed' and a['freeze_sha256']==external and all(a[k] is True for k in ('M2_business_gates_passed','shared_C14_control_accepted','resources_passed','source_tools_passed')),'independent M2 same-freeze admission absent')
    C.need({'M2_business','shared_control','source_review','resource_review'}<=set(a['evidence']),'M2 admission evidence missing')
    for ref in a['evidence'].values():C.need(set(ref)=={'path','identity'} and C.same_id(ref['path'],ref['identity']),'M2 admission evidence changed')
    C.need(a['evidence']['shared_control']==f['control']['result'],'admission must accept actual completed C result')
    lease=leased(queue);C.need({p.name for p in OUTPUT.iterdir()}=={'queue'},'M2 attempt prefix already used')
    D=C.load_observer();G=C.load_audit();C.need(ctypes.CDLL(None).prctl(36,1,0,0,0)==0,'subreaper unavailable')
    deadline=ENTRY_NS+300*10**9
    C.write(OUTPUT/'lease.json',lease);shutil.copyfile(frozen,OUTPUT/'freeze.json');shutil.copyfile(admission,OUTPUT/'admission.json')
    attempted=False;compiled=False;native_attempted=False;audited=False;error=None;mechanism=None
    try:
        C.write(OUTPUT/'identity-before.json',C.verify_files(f));C.write(OUTPUT/'resources-before.json',C.check_resources())
        C.need(deadline-time.monotonic_ns()>=int(232.5*1e9),'cannot fit compile122/native20.5/audit60/seal30; no launch')
        C.need(C.allocated(OUTPUT)<4*C.GiB,'physical allocated ceiling exceeded')
        role=f['role'];rdir=OUTPUT/'m2';rdir.mkdir();(rdir/'tmp').mkdir();(rdir/'bodies').mkdir();attempted=True
        C.compile_one(D,role,deadline);compiled=True;C.write(rdir/'compiled-dependencies.json',C.dependencies(role,f['fixed_files']))
        C.verify_files(f);C.check_resources()
        C.need(deadline-time.monotonic_ns()>=int(110.5*1e9),'identity work consumed native/audit/seal reserve; no native launch')
        C.need(C.allocated(OUTPUT)<4*C.GiB,'physical allocated ceiling exceeded before native')
        binding=dict(role='M2',instrumented=True,eligible_for_performance=False,shared_control=f['control'],
            files={k:dict(path=str(p),identity=C.fid(p)) for k,p in {'source_identity':Path(role['source'])/'FINAL_SOURCE_IDENTITY.json','driver':Path(role['source'])/'experiment/driver.cpp','binary':Path(role['binary']),'compile_receipt':rdir/'compile-receipt.json','hook':Path(role['source'])/'source/c14_common_diag.hpp','event_channel':Path(role['source'])/'experiment/event_channel.hpp'}.items()})
        C.write(rdir/'native-attempt.json',dict(argv=role['native_argv'],binding=binding,job_deadline_monotonic_ns=str(deadline)));native_attempted=True
        wr=D.supervise(role['native_argv'],rdir/'observe',role['environment'],list(role['bodies'].values()),binding=binding,queue_request=lease['request'],absolute_job_deadline_ns=deadline)
        C.need(all(C.same_id(v['path'],v['identity']) for v in binding['files'].values()),'M2 binary/build/source changed across child')
        audit=C.audit_role(D,G,role,wr,f);C.write(rdir/'AUDIT.json',audit);audited=True
        C.verify_files(f);C.need(deadline-time.monotonic_ns()>=30*10**9,'checker/final seal reserve exhausted')
        spec=importlib.util.spec_from_file_location('m2_predeclared_checker',f['checker']);checker=importlib.util.module_from_spec(spec);spec.loader.exec_module(checker)
        control=checker.read_pinned(Path(f['control']['audit']['path']),CONTROL_SHA)
        candidate_sha=C.fid(rdir/'AUDIT.json')['sha256'];candidate=checker.read_pinned(rdir/'AUDIT.json',candidate_sha)
        mechanism=checker.compare('m2',control,candidate)
        mechanism['external_audit_sha256']=dict(control=CONTROL_SHA,candidate=candidate_sha)
        C.write(OUTPUT/'MECHANISM.json',mechanism)
    except BaseException as e:error=type(e).__name__+': '+str(e)
    try:C.write(OUTPUT/'identity-after.json',C.verify_files(f))
    except BaseException as e:error=error or 'final identity failure: '+str(e)
    try:
        C.write(OUTPUT/'resources-after.json',C.check_resources());C.need(C.allocated(OUTPUT)+1024**2<=4*C.GiB,'final4GiB ceiling lacks1MiB receipt reserve')
    except BaseException as e:error=error or 'final resource failure: '+str(e)
    if time.monotonic_ns()>deadline:error=error or 'whole M2 job300s exceeded'
    result=dict(schema='m2-mechanism-result-v1',status='passed' if error is None and audited and mechanism is not None else 'failed',error=error,
        compile_attempts=int(attempted),compiles_passed=int(compiled),native_attempts=int(native_attempted),audits_passed=int(audited),new_control_compiles=0,new_control_runs=0,retry=0,
        freeze_sha256=external,control=f['control'],mechanism_outcome=mechanism['status'] if mechanism is not None else 'not_completed',
        started_monotonic_ns=str(ENTRY_NS),finished_monotonic_ns=str(time.monotonic_ns()),elapsed_seconds=(time.monotonic_ns()-ENTRY_NS)/1e9,
        actual_allocated_bytes=C.allocated(OUTPUT),limitations=C.LIMITATIONS,timing_samples=0,performance_admitted=False,screen_admitted=False,QF_admitted=False,outer_runner_cleanup_still_required=True)
    C.write(OUTPUT/'RESULT.json',result);C.need(result['status']=='passed','M2 stopped; retain prefix: '+str(error));return result

def main():
    p=argparse.ArgumentParser(description=__doc__);s=p.add_subparsers(dest='action',required=True)
    a=s.add_parser('prepare');a.add_argument('--out',type=Path,required=True);a.add_argument('--resources-out',type=Path,required=True);a.add_argument('--request-out',type=Path,required=True)
    a=s.add_parser('run');a.add_argument('--frozen',type=Path,required=True);a.add_argument('--freeze-sha256',required=True);a.add_argument('--admission',type=Path,required=True);a.add_argument('--queue',type=Path,required=True)
    a=p.parse_args()
    try:
        r=prepare(a.out,a.resources_out,a.request_out) if a.action=='prepare' else run(a.frozen,a.freeze_sha256,a.admission,a.queue)
        print(json.dumps(r,sort_keys=True));return 0
    except (ValueError,OSError,KeyError,TypeError) as e:print(json.dumps(dict(status='rejected',reason=type(e).__name__+': '+str(e))));return 1

if __name__=='__main__':raise SystemExit(main())
