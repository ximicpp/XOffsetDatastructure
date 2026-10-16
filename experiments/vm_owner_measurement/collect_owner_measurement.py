#!/usr/bin/env python3
"""D023's single-window collector and offline provenance audit. No builds.

prepare creates an unapproved proposal. collect is W3-only and requires a
separate exact parent approval; this module never upgrades native producer JSON.
"""
from __future__ import annotations
import argparse
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import resource
import selectors
import shutil
import subprocess
import sys
import time
import types
import uuid

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
DESIGN_SHA = '9465f8bfbed6fcc9557e935501834f31f17eed340674a1e18600ad0047f1d243'
QUEUE_SHA = '6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909'
D022_SHA = '0113de64a4e83f3c4d10a710b92be9a70ccc4cadaf69807ab747e4ca3fbf865d'
D027_SHA = '9e0ea9ed42921997fbe1f7cf9ace698d5268f0abcc765b4ee47faa9e78f739db'
BASELINE = {'xoffsetdatastructure.hpp':'9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2',
            'xoffset_virtual_memory.hpp':'22f181b52c11ce442a5576ffc0dc35af1828f77deb4424f387d0acfa658239ad'}
LIMIT = 536870912
LOG_LIMIT = 2 * 1024 * 1024
TOOLS = ('collect_owner_measurement.py', 'audit_owner_measurement.py')
NATIVE_ENV = {'PATH':'/usr/bin:/bin:/usr/sbin:/sbin','LC_ALL':'C','LANG':'C','TZ':'UTC'}
REF_FIELDS = ('parent_acceptance','build_result','functional_result','checker','contract','layout_correction','toolkit_result','recovery_result')
APPROVAL_FIELDS = {'schema','decision','status','approved_by','native_execution_authorized',
 'producer_functional_parent_accepted','proposal_sha256','design_sha256','producer_binding_sha256',
 'producer_parent_acceptance_sha256','run_id','formal_execution_role','max_windows'}


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for data in iter(lambda:f.read(1048576), b''):h.update(data)
    return h.hexdigest()


def read_json_bytes(data):
    def unique(pairs):
        obj = {}
        for key, value in pairs:
            require(key not in obj, 'duplicate JSON field: '+key);obj[key] = value
        return obj
    return json.loads(data.decode('utf-8'), object_pairs_hook=unique,
                      parse_constant=lambda value:(_ for _ in ()).throw(ValueError('nonfinite JSON: '+value)))


def read(path):return read_json_bytes(Path(path).read_bytes())


def dump(value):return json.dumps(value, indent=2, sort_keys=True, allow_nan=False)+'\n'
def now():return datetime.datetime.now(datetime.timezone.utc).isoformat()


def write(path, value, *, exclusive=False):
    path = Path(path)
    if exclusive:
        with path.open('x') as f:f.write(dump(value));f.flush();os.fsync(f.fileno())
    else:
        temporary = path.with_name(path.name+'.tmp')
        with temporary.open('x') as f:f.write(dump(value));f.flush();os.fsync(f.fileno())
        os.replace(temporary,path)
    fd = os.open(path.parent,os.O_RDONLY)
    try:os.fsync(fd)
    finally:os.close(fd)


def preserve_approval(source, target, expected_sha, *, data=None):
    """Copy the externally approved bytes without changing JSON serialization."""
    if data is None:data=Path(source).read_bytes()
    require(hashlib.sha256(data).hexdigest()==expected_sha,'approval bytes changed before preservation')
    target=Path(target)
    with target.open('xb') as f:
        f.write(data);f.flush();os.fsync(f.fileno())
    require(sha(target)==expected_sha,'preserved approval SHA mismatch')
    fd=os.open(target.parent,os.O_RDONLY)
    try:os.fsync(fd)
    finally:os.close(fd)


def module(path, name):
    m = types.ModuleType(name);m.__file__ = str(Path(path).resolve());sys.modules[name] = m
    exec(compile(Path(path).read_bytes(),str(path),'exec'),m.__dict__);return m


def inventory(root, omit=()):
    files = {}
    for p in sorted(Path(root).rglob('*')):
        require(not p.is_symlink() and (p.is_file() or p.is_dir()),'nonregular evidence: '+str(p))
        if p.is_file() and p.relative_to(root).as_posix() not in omit:files[p.relative_to(root).as_posix()] = sha(p)
    return files


def budget(repo, extra=0):
    total = sum(p.stat().st_size for root in (repo/'build/vm-owner-measurement',repo/'docs/perf/vm-owner-scaling')
                if root.exists() for p in root.rglob('*') if p.is_file())
    require(total+extra<=LIMIT,'D023 512MiB evidence budget exceeded');return total


def owned(path, repo, prefix):
    path = Path(path).resolve();root = repo/prefix
    require(path.is_relative_to(root.resolve()) and path != root.resolve(),'output outside D023 owned root');return path


def safe_relative(value):
    require(type(value) is str,'reference path must be text');rel=PurePosixPath(value)
    require(not rel.is_absolute() and '..' not in rel.parts and str(rel)==value,'unsafe relative path');return value


def python_identity():
    # Fixed direct modules plus the executable, without spawning a subprocess.
    files = {}
    for name in ('argparse','datetime','fcntl','hashlib','json','os','pathlib','platform','resource',
                 'selectors','shutil','subprocess','types','uuid','statistics','math','decimal','fractions'):
        __import__(name);m=sys.modules[name];path=getattr(m,'__file__',None)
        if path and Path(path).is_file():files[str(Path(path).resolve())]=sha(path)
    return {'executable':str(Path(sys.executable).absolute()),'resolved_executable':str(Path(sys.executable).resolve()),
            'executable_sha256':sha(sys.executable),'version':sys.version,'implementation':sys.implementation.name,
            'system':platform.system(),'machine':platform.machine(),'module_files':files}


def reference(repo, item):
    require(type(item) is dict and set(item)=={'path','sha256'},'bad producer reference')
    p=repo/safe_relative(item['path']);require(p.is_file() and not p.is_symlink() and sha(p)==item['sha256'],'changed producer reference: '+str(p));return p


def producer_authority(binding, acceptance, build, functional, toolkit):
    """D027 accepts the exact native26, unchanged build and D022 supplement."""
    require(binding['parent_acceptance']['sha256']==D027_SHA and acceptance['decision']=='D027'
        and acceptance['status']=='accepted_scoped_functionality_and_build','D027 final producer acceptance required')
    require(acceptance['workload_processes']==26 and acceptance['successful_processes']==18
        and acceptance['expected_failure_processes']==8 and acceptance['duplicate_or_missing_cases'] is False
        and acceptance['formal_timing_execution_authorized'] is False and acceptance['timing_evidence'] is False,'D027 scope differs')
    for name in ('build_result','functional_result','toolkit_result','recovery_result'):
        ref=binding[name];require(acceptance['reviewed_files_sha256'].get(ref['path'])==ref['sha256'],'D027 accepted reference differs: '+name)
    require(binding['checker']['sha256']==acceptance['checker_sha256']==toolkit['files']['audit_owner_scaling.py']
        and binding['contract']['sha256']==toolkit['files']['contract.json']
        and binding['layout_correction']['sha256']==toolkit['files']['layout-correction.json']==D022_SHA,'D022 accepted semantic supplement differs')
    require(toolkit['status']=='passed' and toolkit['kind']=='toolkit' and toolkit['decision']=='D022'
        and toolkit['timing_evidence'] is False and toolkit['build_result_sha256']==binding['build_result']['sha256'],'accepted toolkit linkage differs')
    require(build['binaries']==toolkit['binaries']==acceptance['binaries']
        and build['files']['common/owner_scaling.cpp']==toolkit['source_sha256']==acceptance['harness_sha256'],'D027 native source/binaries differ')
    require(functional['status']=='passed' and functional['timing_evidence'] is False and functional['workload_processes']==26
        and functional['successful_processes']==18 and functional['expected_failure_processes']==8
        and functional['case_ids']==acceptance['case_ids'] and len(set(functional['case_ids']))==26
        and functional['build_result_sha256']==binding['build_result']['sha256']
        and functional['checker_sha256']==binding['checker']['sha256']
        and functional['toolkit_result_sha256']==binding['toolkit_result']['sha256']
        and functional['recovery_result_sha256']==binding['recovery_result']['sha256'],'accepted26 functional evidence differs')


def verify_binding(repo, binding):
    require(type(binding) is dict and set(binding)=={'schema',*REF_FIELDS},'producer binding fields differ')
    require(binding['schema']=='owner-measurement-producer-binding-v1','producer binding schema')
    paths={name:reference(repo,binding[name]) for name in REF_FIELDS}
    # Final functional acceptance is separate from the two design/erratum decisions.
    acceptance=read(paths['parent_acceptance'])
    require(acceptance.get('status') not in ('accepted_design_implementation_dispatched','design_frozen_tool_implementation_authorized')
            and acceptance.get('decision') not in ('D018','D022'),'design/erratum is not final producer acceptance')
    build=read(paths['build_result']);root=paths['build_result'].parent
    require(build.get('schema_version')==1 and build.get('kind')=='build' and build.get('status')=='passed'
            and build.get('timing_evidence') is False,'producer build profile')
    actual=inventory(root,('result.json',));require(actual==build['files'],'producer build file inventory differs')
    binary=root/'owner_scaling_timing';require(sha(binary)==build['binaries']['timing']==build['files']['owner_scaling_timing'],'original timing binary identity')
    require(build['files']['common/owner_scaling.cpp']==sha(root/'common/owner_scaling.cpp'),'native source identity')
    for name,digest in BASELINE.items():require(build['files']['input/timing/'+name]==digest,'not fixed F original headers')
    command=next((c['argv'] for c in build['commands'] if '-DN3_DIAGNOSTIC=0' in c['argv']),None)
    require(command is not None and command[0].endswith('clang++') and '-DN3_DIAGNOSTIC=1' not in command,'not original timing build')
    for name,digest in BASELINE.items():
        matches=[p for p,h in build['closures']['timing'].items() if Path(p).name==name]
        require(len(matches)==1 and build['closures']['timing'][matches[0]]==digest,'original include closure differs')
    contract=read(paths['contract']);require(contract.get('decision')=='D018' and contract.get('contract_revision')==2
        and contract.get('native_layout_erratum',{}).get('decision')=='D022','corrected producer contract required')
    require(contract['baseline_headers']==BASELINE and contract['workload']['timing_requests_per_allocate_or_free_phase']==8192,'producer workload binding')
    require(binding['layout_correction']['sha256']==contract['native_layout_erratum']['decision_sha256']==D022_SHA,'D022 correction identity')
    producer_authority(binding,acceptance,build,read(paths['functional_result']),read(paths['toolkit_result']))
    checker=module(paths['checker'],'_owner_measurement_binding_checker')
    require(getattr(checker,'EFFECTIVE_ALIGNMENT',None)==8 and getattr(checker,'BLOCK_STRIDE',None)==24
            and getattr(checker,'INITIAL_USED_END',None)==48,'D022 corrected semantic checker required')
    # The parent approves exact opaque final functional evidence via its SHA.
    # Do not reinterpret those original26 records as timing samples.
    return {'binary':str(binary),'binary_sha256':sha(binary),'build':build,'paths':paths,
            'external_files':{**{p:h for p,h in build['closures']['timing'].items() if not p.startswith('artifact:')},**build['runtime'],**build['sdk_settings'],
                              build['compiler']['resolved']:build['compiler']['sha256']}}


def prepare(repo, out, binding_path=None):
    out=owned(out,repo,'build/vm-owner-measurement');require(not out.exists(),'proposal directory must be fresh')
    design=repo/'docs/coordination/vm-followup/contracts/owner-measurement-v1/design.json'
    require(sha(design)==DESIGN_SHA,'D023 frozen design changed')
    statistics=module(HERE/'audit_owner_measurement.py','_owner_measurement_statistics')
    producer=None;bound=None
    if binding_path is not None:
        active_queue({'execution':{'repository':str(repo)}},None,'validation',
                     validation_argv=[str(Path(sys.executable).absolute()),str(Path(__file__).absolute()),*sys.argv[1:]],owners=('W2','W3'))
        producer=read(binding_path);bound=verify_binding(repo,producer)
    budget(repo,16*1024*1024);out.mkdir(parents=True);(out/'tools').mkdir();shutil.copy2(design,out/'design.json')
    for name in TOOLS:shutil.copy2(HERE/name,out/'tools'/name)
    queue=repo/'experiments/vm_coordination/run_job.py';require(sha(queue)==QUEUE_SHA,'accepted queue changed');shutil.copy2(queue,out/'tools/queue-runner.py')
    write(out/'schedule.json',statistics.schedule(),exclusive=True)
    execution={'repository':str(repo),'collector':str(out/'tools/collect_owner_measurement.py'),
               'proposal':str(out/'proposal.json'),'approval':str(repo/'docs/coordination/vm-followup/contracts/owner-measurement-v1/window-approval.json'),
               'window':str(repo/'build/vm-owner-measurement/window-v1'),
               'queue_receipt':str(repo/'build/vm-owner-measurement/receipts/window-v1'),
               'native_window_marker':str(repo/'build/vm-owner-measurement/native-window.json')}
    producer_info=None
    if producer is not None:
        (out/'producer').mkdir();write(out/'producer/binding.json',producer,exclusive=True)
        for name,path in bound['paths'].items():shutil.copy2(path,out/'producer'/(name+('.py' if name=='checker' else '.json')))
        build_root=bound['paths']['build_result'].parent
        for name in ('owner_scaling_timing','timing.d','common/owner_scaling.cpp','input/timing/xoffsetdatastructure.hpp','input/timing/xoffset_virtual_memory.hpp'):
            target=out/'producer'/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(build_root/name,target)
        producer_info={'binding_sha256':sha(out/'producer/binding.json'),
                       'parent_acceptance_sha256':producer['parent_acceptance']['sha256'],'binary':bound['binary'],
                       'binary_sha256':bound['binary_sha256'],'external_files':bound['external_files']}
    proposal={'schema':'owner-measurement-proposal-v1','decision':'D023','status':'unapproved_proposal' if producer else 'unapproved_template',
              'native_execution_authorized':False,'producer_final_binding_pending':producer is None,'run_id':uuid.uuid4().hex,
              'design_sha256':DESIGN_SHA,'schedule_sha256':sha(out/'schedule.json'),'python':python_identity(),
              'native_environment':NATIVE_ENV,
              'producer':producer_info,'execution':execution,'created_at':now(),'files':inventory(out)}
    write(out/'proposal.json',proposal,exclusive=True);(out/'COMPLETE').write_text(sha(out/'proposal.json')+'\n');budget(repo)
    return proposal


def audit_proposal(root, require_bound=False):
    p=read(root/'proposal.json');require(p['schema']=='owner-measurement-proposal-v1' and p['decision']=='D023'
        and p['native_execution_authorized'] is False,'proposal native claims')
    require((root/'COMPLETE').read_text().strip()==sha(root/'proposal.json'),'proposal completion identity')
    require(inventory(root,('proposal.json','COMPLETE'))==p['files'],'proposal file inventory')
    require(p['design_sha256']==sha(root/'design.json')==DESIGN_SHA,'frozen design binding')
    require(p['native_environment']==NATIVE_ENV,'native controlled environment differs')
    e=p['execution'];repo=Path(e['repository']);require(repo.is_absolute(),'execution repository must be absolute')
    require(e['collector']==str(Path(e['proposal']).parent/'tools/collect_owner_measurement.py')
        and Path(e['proposal']).parent.is_relative_to(repo/'build/vm-owner-measurement')
        and e['window']==str(repo/'build/vm-owner-measurement/window-v1')
        and e['queue_receipt']==str(repo/'build/vm-owner-measurement/receipts/window-v1')
        and e['native_window_marker']==str(repo/'build/vm-owner-measurement/native-window.json')
        and e['approval']==str(repo/'docs/coordination/vm-followup/contracts/owner-measurement-v1/window-approval.json'),'canonical execution paths differ')
    stats=module(root/'tools/audit_owner_measurement.py','_owner_measurement_statistics')
    require(read(root/'schedule.json')==stats.schedule() and p['schedule_sha256']==sha(root/'schedule.json'),'frozen78 schedule differs')
    require(sha(root/'tools/queue-runner.py')==QUEUE_SHA,'queue source binding')
    if require_bound:require(p['producer'] is not None and p['status']=='unapproved_proposal' and p['producer_final_binding_pending'] is False,'producer final binding pending')
    if p['producer'] is None:require(p['status']=='unapproved_template' and p['producer_final_binding_pending'] is True,'unbound proposal flags')
    else:
        info=p['producer'];binding=read(root/'producer/binding.json');require(info['binding_sha256']==sha(root/'producer/binding.json'),'producer binding hash')
        for name in REF_FIELDS:
            path=root/'producer'/(name+('.py' if name=='checker' else '.json'));require(sha(path)==binding[name]['sha256'],'copied producer reference differs')
        build=read(root/'producer/build_result.json');require(info['parent_acceptance_sha256']==binding['parent_acceptance']['sha256'],'producer acceptance hash')
        require(info['binary_sha256']==sha(root/'producer/owner_scaling_timing')==build['binaries']['timing'],'copied original binary differs')
        external={**{p:h for p,h in build['closures']['timing'].items() if not p.startswith('artifact:')},**build['runtime'],**build['sdk_settings'],build['compiler']['resolved']:build['compiler']['sha256']}
        require(info['external_files']==external,'producer external closure/runtime differs')
        for name in ('timing.d','common/owner_scaling.cpp','input/timing/xoffsetdatastructure.hpp','input/timing/xoffset_virtual_memory.hpp'):
            require(sha(root/'producer'/name)==build['files'][name],'producer source/include copy differs')
        for name,digest in BASELINE.items():require(build['files']['input/timing/'+name]==digest,'copied source is not fixed F')
        contract=read(root/'producer/contract.json');require(contract['contract_revision']==2 and contract['native_layout_erratum']['decision_sha256']==binding['layout_correction']['sha256']==D022_SHA,'copied D022 contract differs')
        acceptance=read(root/'producer/parent_acceptance.json');require(acceptance.get('decision') not in ('D018','D022') and acceptance.get('status')!='accepted_design_implementation_dispatched','design acceptance cannot bind final producer')
        producer_authority(binding,acceptance,build,read(root/'producer/functional_result.json'),read(root/'producer/toolkit_result.json'))
    return p


def verify_approval(root, approval_path, expected_sha, *, raw_bytes=None):
    require(Path(approval_path).is_file() and not Path(approval_path).is_symlink(),'independent parent approval missing/nonregular')
    if raw_bytes is None:raw_bytes=Path(approval_path).read_bytes()
    require(type(expected_sha) is str and len(expected_sha)==64 and hashlib.sha256(raw_bytes).hexdigest()==expected_sha,'independent parent approval SHA mismatch')
    a=read_json_bytes(raw_bytes);require(set(a)==APPROVAL_FIELDS and a['schema']=='owner-measurement-approval-v1','approval schema')
    require(a['proposal_sha256']==sha(root/'proposal.json'),'independent approval binds a foreign proposal')
    proposal=audit_proposal(root,True)
    for key,value in {'status':'approved','approved_by':'parent','native_execution_authorized':True,
        'producer_functional_parent_accepted':True,'proposal_sha256':sha(root/'proposal.json'),'design_sha256':DESIGN_SHA,
        'producer_binding_sha256':proposal['producer']['binding_sha256'],
        'producer_parent_acceptance_sha256':proposal['producer']['parent_acceptance_sha256'],
        'run_id':proposal['run_id'],'formal_execution_role':'W3','max_windows':1}.items():
        require(type(a[key]) is type(value) and a[key]==value,'approval differs: '+key)
    require(type(a['decision']) is str and a['decision'] not in ('D018','D022','D023',''),'separate exact window approval required')
    return proposal,a


def collector_argv(proposal, approval_sha):
    e=proposal['execution'];return [proposal['python']['executable'],e['collector'],'collect','--proposal',e['proposal'],
                                  '--approval',e['approval'],'--approval-sha256',approval_sha]


def verify_request(request, proposal, approval_sha):
    e=proposal['execution']
    for key,value in {'owner':'W3','kind':'timing','timeout_seconds':900,'runner_sha256':QUEUE_SHA,
                      'lock_dir':str(Path(e['repository'])/'build/vm-coordination'),
                      'cwd':e['repository'],'out':e['queue_receipt'],'command':collector_argv(proposal,approval_sha),
                      'source_headers':BASELINE}.items():
        require(request.get(key)==value and (type(request.get(key)) is type(value) or key=='timeout_seconds' and type(request.get(key)) in (int,float)), 'queue request differs: '+key)
    require(type(request.get('run_id')) is str and bool(request['run_id']),'missing queue run_id')


def active_queue(proposal, approval_sha, kind='timing', *, validation_argv=None, owners=('W3',)):
    repo=Path(proposal['execution']['repository']);path=repo/'build/vm-coordination/queue.lock'
    require(sha(repo/'experiments/vm_coordination/run_job.py')==QUEUE_SHA,'live queue identity changed')
    with path.open('r+') as f:
        try:fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:pass
        else:fcntl.flock(f,fcntl.LOCK_UN);raise ValueError('default queue not held')
        metadata=json.load(f)
    require(metadata.get('status')=='running' and metadata.get('owner') in owners and metadata.get('kind')==kind
            and metadata.get('supervisor_pid')==os.getppid(),'must be direct authorized default-queue child')
    request=read(Path(metadata['out'])/'request.json')
    if kind=='timing':verify_request(request,proposal,approval_sha)
    else:require(kind=='validation' and request['owner'] in owners and request['kind']=='validation'
        and request['lock_dir']==str(repo/'build/vm-coordination') and request['runner_sha256']==QUEUE_SHA
        and 0<request['timeout_seconds']<=900 and request['source_headers']==BASELINE and request['cwd']==str(repo)
        and validation_argv is not None and request['command']==validation_argv,'validation queue differs')
    require(request['run_id']==metadata['run_id'],'running queue identity differs');return {'metadata':metadata,'request':request}


def verify_receipt(receipt, proposal, approval_sha, captured, queue_source=None):
    # W4's accepted offline verifier plus D023-specific authority/command linkage.
    queue_source=queue_source or Path(proposal['execution']['collector']).parent/'queue-runner.py'
    require(sha(queue_source)==QUEUE_SHA,'receipt verifier source identity')
    q=module(queue_source,'_owner_measurement_queue_auditor')
    q.audit(receipt);request=read(receipt/'request.json');result=read(receipt/'result.json');verify_request(request,proposal,approval_sha)
    require(request==captured['request'] and request['run_id']==captured['metadata']['run_id'],'foreign timing receipt')
    require(captured['metadata']['status']=='running' and captured['metadata']['owner']=='W3'
        and captured['metadata']['kind']=='timing' and captured['metadata']['out']==request['out'],'captured queue differs')
    for key,value in {'owner':'W3','kind':'timing','run_id':request['run_id'],'status':'complete','reason':'command_exit','command_returncode':0}.items():require(type(result.get(key)) is type(value) and result.get(key)==value,'timing receipt result differs: '+key)
    require(result['cleanup']['live']==[] and result['cleanup']['process_groups']==[],'timing receipt has remaining processes')
    return {'request_sha256':sha(receipt/'request.json'),'result_sha256':sha(receipt/'result.json'),'manifest_sha256':sha(receipt/'manifest.json'),'run_id':request['run_id']}


def child_argv(binary, slot):
    return [binary,'--owners',str(slot['owners']),'--threads',str(slot['threads']),
            '--requests','8192','--control','none']


def execute_child(argv, directory):
    """Bound logs while preserving exact argv/exit/timeout; no child retries."""
    record={'argv':argv,'environment':NATIVE_ENV,'started_at':now(),'timeout_seconds':30,'timed_out':False,'returncode':None}
    write(directory/'command.json',record,exclusive=True)
    def cap():resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_FSIZE,(LOG_LIMIT,LOG_LIMIT))
    started=time.monotonic();process=None
    try:
        with (directory/'stdout.json').open('xb') as stdout,(directory/'stderr.log').open('xb') as stderr:
            process=subprocess.Popen(argv,stdout=subprocess.PIPE,stderr=subprocess.PIPE,preexec_fn=cap,env=NATIVE_ENV)
            try:
                with selectors.DefaultSelector() as selector:
                    selector.register(process.stdout,selectors.EVENT_READ,(stdout,0));selector.register(process.stderr,selectors.EVENT_READ,(stderr,0))
                    while selector.get_map():
                        remaining=30-(time.monotonic()-started)
                        if remaining<=0:record['timed_out']=True;raise TimeoutError('native child exceeded30s')
                        for key,_ in selector.select(min(remaining,0.1)):
                            data=os.read(key.fd,65536)
                            if not data:selector.unregister(key.fileobj);continue
                            stream,used=key.data;require(used+len(data)<=LOG_LIMIT,'native log bound exceeded');stream.write(data);selector.modify(key.fileobj,selectors.EVENT_READ,(stream,used+len(data)))
                record['returncode']=process.wait(timeout=max(.001,30-(time.monotonic()-started)))
            finally:
                if process.poll() is None:process.kill();process.wait(timeout=5)
                if record['returncode'] is None:record['returncode']=process.returncode
                process.stdout.close();process.stderr.close()
    except BaseException as error:
        if isinstance(error,(TimeoutError,subprocess.TimeoutExpired)):record['timed_out']=True
        record['error_type']=type(error).__name__;record['reason']=str(error);raise
    finally:
        record['finished_at']=now();write(directory/'command.json',record)
    require(record['returncode']==0 and not record['timed_out'],'native child failed');return record


def collect(root, approval_path, approval_sha):
    approval_bytes=Path(approval_path).read_bytes() if Path(approval_path).is_file() else None
    proposal,approval=verify_approval(root,approval_path,approval_sha,raw_bytes=approval_bytes);e=proposal['execution'];repo=Path(e['repository'])
    require(Path(__file__).resolve()==Path(e['collector']).resolve(),'collector entry differs from frozen proposal')
    require(str(approval_path.absolute())==e['approval'] and str(root/'proposal.json')==e['proposal'],'execution approval/proposal path differs')
    require(python_identity()==proposal['python'],'Python identity differs from proposal')
    require(not any(name.startswith(('DYLD_','LD_')) for name in os.environ),'loader override environment is forbidden')
    binding=read(root/'producer/binding.json');bound=verify_binding(repo,binding)
    require(bound['binary']==proposal['producer']['binary'] and bound['binary_sha256']==proposal['producer']['binary_sha256'],'live accepted producer differs')
    for name,digest in proposal['producer']['external_files'].items():require(sha(name)==digest,'producer runtime/include identity changed: '+name)
    queue=active_queue(proposal,approval_sha);out=owned(e['window'],repo,'build/vm-owner-measurement');marker=repo/'build/vm-owner-measurement/native-window.json'
    require(not marker.exists() and not out.exists(),'D023 single window already consumed; no resume/retry')
    budget(repo,2*LOG_LIMIT+4*1024*1024);out.mkdir(parents=True);(out/'samples').mkdir()
    preserve_approval(approval_path,out/'approval.json',approval_sha,data=approval_bytes);shutil.copytree(root,out/'proposal');write(out/'coordination.json',queue,exclusive=True)
    state={'schema':'owner-measurement-window-v1','status':'running','run_id':proposal['run_id'],'proposal_sha256':sha(root/'proposal.json'),
           'approval_sha256':approval_sha,'native_timing_evidence_claim':False,'measurement_qualification':'pending_completed_receipt',
           'coordination':queue,'attempts':[],'started_at':now()}
    write(marker,{'schema':'owner-measurement-one-window-v1','run_id':proposal['run_id'],'proposal_sha256':state['proposal_sha256'],
                  'approval_sha256':approval_sha,'queue_run_id':queue['metadata']['run_id'],'started_at':now()},exclusive=True)
    shutil.copy2(marker,out/'window-marker.json')
    write(out/'window.json',state,exclusive=True)
    stats=module(root/'tools/audit_owner_measurement.py','_owner_measurement_statistics');checker=module(root/'producer/checker.py','_owner_measurement_producer_checker');records=[]
    try:
        for slot in stats.schedule():
            budget(repo,2*LOG_LIMIT+1024*1024);require(sha(bound['binary'])==bound['binary_sha256'],'native binary changed before child')
            for name,digest in read(root/'producer/build_result.json')['runtime'].items():require(sha(name)==digest,'native runtime changed before child')
            directory=out/'samples'/f"{slot['index']:02d}";directory.mkdir();argv=child_argv(bound['binary'],slot)
            attempt={'slot':slot,'argv':argv,'status':'started'};state['attempts'].append(attempt);write(out/'window.json',state)
            command=execute_child(argv,directory);report=read(directory/'stdout.json');audit=stats.audit_native(report,slot,checker)
            require(sha(bound['binary'])==bound['binary_sha256'],'native binary changed across child')
            require((directory/'stderr.log').stat().st_size==0,'unexpected native stderr');write(directory/'audit.json',audit,exclusive=True)
            attempt.update(status='complete',command=command,stdout_sha256=sha(directory/'stdout.json'),stderr_sha256=sha(directory/'stderr.log'))
            write(out/'window.json',state);records.append({'slot':slot,'report':report})
        for name,digest in proposal['producer']['external_files'].items():require(sha(name)==digest,'producer changed across window: '+name)
        require(sha(bound['binary'])==bound['binary_sha256'],'native binary changed across window')
        write(out/'summary.json',stats.summarize(records),exclusive=True);state.update(status='native_complete_pending_receipt',finished_at=now())
        write(out/'window.json',state);budget(repo)
    except BaseException as error:
        state.update(status='failed_no_retry',finished_at=now(),error_type=type(error).__name__,reason=str(error));write(out/'window.json',state);raise
    return state


def audit_window(window):
    proposal=audit_proposal(window/'proposal',True);state=read(window/'window.json');approval_sha=state['approval_sha256']
    verify_approval(window/'proposal',window/'approval.json',approval_sha)
    require(state['schema']=='owner-measurement-window-v1' and state['status']=='native_complete_pending_receipt'
        and state['native_timing_evidence_claim'] is False and state['measurement_qualification']=='pending_completed_receipt'
        and state['run_id']==proposal['run_id'] and state['proposal_sha256']==sha(window/'proposal/proposal.json'),'window incomplete or mismatched')
    require(state['coordination']==read(window/'coordination.json'),'window coordination mismatch')
    marker=read(window/'window-marker.json')
    for key,value in {'schema':'owner-measurement-one-window-v1','run_id':proposal['run_id'],'proposal_sha256':state['proposal_sha256'],
                      'approval_sha256':approval_sha,'queue_run_id':state['coordination']['metadata']['run_id']}.items():require(marker.get(key)==value,'window marker differs: '+key)
    stats=module(window/'proposal/tools/audit_owner_measurement.py','_owner_measurement_statistics');checker=module(window/'proposal/producer/checker.py','_owner_measurement_producer_checker')
    require(len(state['attempts'])==78 and set(p.name for p in (window/'samples').iterdir())=={f'{i:02d}' for i in range(78)},'missing/extra native samples')
    records=[]
    for slot,attempt in zip(stats.schedule(),state['attempts']):
        directory=window/'samples'/f"{slot['index']:02d}";command=read(directory/'command.json');argv=child_argv(proposal['producer']['binary'],slot)
        require(set(p.name for p in directory.iterdir())=={'command.json','stdout.json','stderr.log','audit.json'},'foreign/missing sample files')
        require(attempt['slot']==slot and attempt['argv']==argv and attempt['status']=='complete' and attempt['command']==command,'sample slot/attempt mismatch')
        require(command['argv']==argv and type(command['returncode']) is int and command['returncode']==0
                and command['timed_out'] is False and command['timeout_seconds']==30 and command['environment']==NATIVE_ENV,'sample execution failed/mismatched')
        require(attempt['stdout_sha256']==sha(directory/'stdout.json') and attempt['stderr_sha256']==sha(directory/'stderr.log') and (directory/'stderr.log').stat().st_size==0,'raw native bytes differ')
        report=read(directory/'stdout.json');require(stats.audit_native(report,slot,checker)==read(directory/'audit.json'),'semantic audit mismatch');records.append({'slot':slot,'report':report})
    require(stats.summarize(records)==read(window/'summary.json'),'summary recomputation differs');return proposal,state


def archive(window, receipt, out):
    proposal=read(window/'proposal/proposal.json');state=read(window/'window.json');repo=Path(proposal['execution']['repository'])
    expected=[proposal['python']['executable'],proposal['execution']['collector'],'archive','--window',proposal['execution']['window'],'--receipt',proposal['execution']['queue_receipt'],'--out',str(out.resolve())]
    require(Path(__file__).resolve()==Path(proposal['execution']['collector']).resolve(),'archive tool differs from frozen proposal')
    queue=active_queue(proposal,state['approval_sha256'],'validation',validation_argv=expected)
    proposal,state=audit_window(window)
    linkage=verify_receipt(receipt,proposal,state['approval_sha256'],state['coordination'],window/'proposal/tools/queue-runner.py')
    out=owned(out,repo,'docs/perf/vm-owner-scaling');require(not out.exists(),'archive must be fresh');budget(repo,sum(p.stat().st_size for root in (window,receipt) for p in root.rglob('*') if p.is_file())+4*1024*1024)
    out.mkdir(parents=True);shutil.copytree(window,out/'window');shutil.copytree(receipt,out/'timing-receipt')
    wrapper={'schema':'owner-measurement-qualified-window-v1','status':'passed','design_sha256':DESIGN_SHA,
             'run_id':proposal['run_id'],'native_reports_timing_evidence':False,'measurement_authorized':True,
             'qualification_scope':'Single approved current-F descriptive window; each24cell retains independent noise qualification',
             'proposal_sha256':state['proposal_sha256'],'approval_sha256':state['approval_sha256'],'receipt':linkage,
             'archive_coordination':queue,'cpp_processes':78,'prerun_processes':6,'measured_processes':72,
             'confidence_interval':False,'tail_inference':False,'optimization_or_adoption_conclusion':False,
             'relocation_audit':{'status':'pending'},'files':{}}
    # Audit the copied full window and receipt at their new location once.
    audit_window(out/'window');verify_receipt(out/'timing-receipt',proposal,state['approval_sha256'],state['coordination'],out/'window/proposal/tools/queue-runner.py')
    wrapper['relocation_audit']={'status':'passed','scope':'complete copied window, producer/tool bundle and completed timing receipt','cpp_executions':0}
    wrapper['files']=inventory(out,('result.json','COMPLETE'));write(out/'result.json',wrapper,exclusive=True);budget(repo,65);(out/'COMPLETE').write_text(sha(out/'result.json')+'\n');return wrapper


def audit_archive(root):
    r=read(root/'result.json');require(r['schema']=='owner-measurement-qualified-window-v1' and r['status']=='passed' and r['measurement_authorized'] is True and r['native_reports_timing_evidence'] is False,'archive qualification profile')
    require((root/'COMPLETE').read_text().strip()==sha(root/'result.json') and inventory(root,('result.json','COMPLETE'))==r['files'],'archive inventory')
    p,state=audit_window(root/'window');link=verify_receipt(root/'timing-receipt',p,state['approval_sha256'],state['coordination'],root/'window/proposal/tools/queue-runner.py')
    require(link==r['receipt'] and r['approval_sha256']==state['approval_sha256'] and r['proposal_sha256']==state['proposal_sha256'] and r['run_id']==p['run_id'] and r['design_sha256']==DESIGN_SHA,'archive authority identity')
    require(r['relocation_audit']['status']=='passed' and r['relocation_audit']['cpp_executions']==0 and r['cpp_processes']==78 and r['measured_processes']==72 and r['prerun_processes']==6,'archive bounds/relocation')
    require(r['confidence_interval'] is False and r['tail_inference'] is False and r['optimization_or_adoption_conclusion'] is False,'forbidden inference');return r


def main():
    ap=argparse.ArgumentParser(description=__doc__);sub=ap.add_subparsers(dest='command',required=True)
    x=sub.add_parser('prepare');x.add_argument('--repo',type=Path,required=True);x.add_argument('--out',type=Path,required=True);x.add_argument('--binding',type=Path)
    x=sub.add_parser('collect');x.add_argument('--proposal',type=Path,required=True);x.add_argument('--approval',type=Path,required=True);x.add_argument('--approval-sha256',required=True)
    x=sub.add_parser('archive');x.add_argument('--window',type=Path,required=True);x.add_argument('--receipt',type=Path,required=True);x.add_argument('--out',type=Path,required=True)
    x=sub.add_parser('audit');x.add_argument('--record',type=Path,required=True);x.add_argument('--kind',choices=('proposal','window','archive'),required=True)
    a=ap.parse_args()
    try:
        if a.command=='prepare':r=prepare(a.repo.resolve(),a.out,a.binding)
        elif a.command=='collect':r=collect(a.proposal.resolve().parent,a.approval,a.approval_sha256)
        elif a.command=='archive':r=archive(a.window.resolve(),a.receipt.resolve(),a.out)
        else:r={'proposal':audit_proposal,'window':audit_window,'archive':audit_archive}[a.kind](a.record.resolve())
        print(json.dumps({'status':'passed','command':a.command,'native_processes_started_by_this_command':78 if a.command=='collect' else 0}));return 0
    except Exception as error:print(type(error).__name__+': '+str(error),file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
