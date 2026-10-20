#!/usr/bin/env python3
"""R1-only capture/freeze/run/audit. Native actions require the original lease.

No compilation, warmup, retries, replacement samples, Q or F entry points.
The copied campaign execute/native_result are used without weakening validation.
"""
from __future__ import annotations
import argparse
import fcntl
import hashlib
import math
import os
from pathlib import Path
import platform
import shutil
import statistics
import re
import shlex
import sys
import time
import campaign as G
import core as C

HERE=Path(__file__).resolve().parent
RUNNER=G.RUNNER
LOCK=RUNNER.parents[2]/'build/vm-coordination'
GiB=1024**3
PHASES=G.PHASES
METRICS=(*PHASES,'read_sum','total')
POLICY=dict(candidate='R1',primary='read_sum',pairs=72,children=144,AB=36,BA=36,
            warmup=0,retry=0,replacement=0,df=71,primary_family=4,auxiliary_family=36,
            primary_t=2.289930854179058,auxiliary_t=3.327935133019535,
            command_seconds=600,child_seconds=20,audit_seconds=3600,
            allocated_limit_bytes=4*GiB,rss_limit_bytes=16*GiB,free_reserve_bytes=2*GiB,
            seal_seconds=30,nonchild_projection_seconds=60,
            semantic_policy='original full owning output; full native_result then byte-exact oracle dedup',
            threshold='primary_upper < 1 and no auxiliary_lower > 1 and all engineering gates pass',
            cancellation_alpha_recycled=False)
ROOT=Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
DESIGN_PATH=ROOT/'docs/coordination/c14-iteration-20261020-v2/core/SCREEN_DESIGN_R1.json'
DESIGN_SHA='3c835b01d1b84dbd06a28d0660a932fc97b4148f68bc71703a674f146309410d'
DESIGN=C.read(DESIGN_PATH)
C.require(C.sha(DESIGN_PATH)==DESIGN_SHA and DESIGN['candidate']=='r1-canonical-hotcold-v1', 'exact candidate design changed')
GATES=('codegen','mechanism','targeted','full_normal','full_san','business_SML','v1_bidirectional','consumers','lifecycle','safety','engineering')


def need(v,m): C.require(v,m)
def fid(p): return C.file_id(p)
def pin(v): return hashlib.sha256(C.encoded(v)).hexdigest()
def write(p,v): C.write(p,v)
def path(p): return C.absolute(p)

def tools():
    return {str(p):fid(p) for p in (HERE/'screen.py',HERE/'campaign.py',HERE/'core.py',HERE/'protocol.py',HERE/'execute.py',HERE/'audit_job.py',HERE/'finalize.py',HERE/'host/process_pause.py',HERE/'host/pause-control.ps1',DESIGN_PATH,ROOT/'docs/coordination/c14-iteration-20261020-v2/reviews/STATISTICAL_PRECHECK.md',ROOT/'docs/coordination/c14-iteration-20261020-v2/reviews/SCREEN_SCHEDULE_REVIEW.md',RUNNER,Path(sys.executable).resolve())}

def schedule():
    """Independent specified SHA stream; never a sample/native retry."""
    seed=DESIGN['randomization']['seed_utf8'];orders=['AB']*36+['BA']*36;counter=0
    for i in range(71,0,-1):
        m=i+1;limit=(2**256//m)*m
        while True:
            x=int.from_bytes(hashlib.sha256(seed.encode()+b'\0'+counter.to_bytes(8,'big')).digest(),'big');counter+=1
            if x<limit:break
        j=x%m;orders[i],orders[j]=orders[j],orders[i]
    pairs=[];children=[];jobs=[]
    for i,order in enumerate(orders):
        pairs.append(dict(pair_index=i,pair_id=f'p{i:03d}',order=order,child_ordinals=[2*i,2*i+1]))
        for j,symbol in enumerate(order):
            role='control' if symbol=='A' else 'candidate'
            children.append(dict(ordinal=2*i+j,pair_index=i,pair_id=f'p{i:03d}',within_pair=j,symbol=symbol,role=role))
            jobs.append(dict(id=f'p{i:03d}-{j+1}-{role}',pair=i,order=order,role=role,ordinal=2*i+j,symbol=symbol))
    raw=dict(pairs=pairs,children=children)
    canonical=__import__('json').dumps(raw,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode()
    need(counter==DESIGN['randomization']['hash_draws'] and raw['pairs']==DESIGN['randomization']['pairs'] and raw['children']==DESIGN['randomization']['children'],'independent schedule reconstruction differs')
    need(hashlib.sha256(canonical).hexdigest()==DESIGN['randomization']['schedule_sha256'] and orders.count('AB')==orders.count('BA')==36,'balanced randomized schedule digest differs')
    return jobs

def checked_file(record):
    need(set(record)=={'path','identity'},'file binding requires path and identity')
    p=path(record['path']);actual=fid(p);expected=record['identity']
    need(set(expected)=={'bytes','sha256'} and int(actual['bytes'])==int(expected['bytes']) and actual['sha256']==expected['sha256'],'file identity changed: '+str(p));return p

def source_snapshot(cfg):
    material=C.read(checked_file(cfg['materialization']))['production_files'];identity=C.read(checked_file(cfg['candidate_identity']))
    need(len(material)==14 and identity['candidate']=='r1-canonical-hotcold-v1' and identity['member_count']==len(identity['members'])==154,'independent exact154/14 candidate required')
    result={}
    for role in ('control','candidate'):
        root=path(cfg['sources'][role]);actual_root=path(cfg['actual_sources'][role]);members={};actual_members={}
        for n,record in identity['members'].items():
            C.relative(n);v=fid(root/n);v['bytes']=int(v['bytes']);expected=record['candidate_sha256' if role=='candidate' else 'baseline_sha256']
            need(v['sha256']==expected,'materialized154 differs: '+role+'/'+n);members[n]=v;actual_members[n]=fid(actual_root/n);actual_members[n]['bytes']=int(actual_members[n]['bytes'])
        production={n:members[n] for n in material};need({n:actual_members[n] for n in material}==production,'actual compiled production14 differs')
        differences=[n for n in members if members[n]!=actual_members[n]]
        if differences:
            # The accepted historical C14 binary has identical153 code/input
            # members but its packaging SOURCE_MANIFEST has historical bytes.
            exception=cfg['control_manifest_exception']
            need(role=='control' and differences==['SOURCE_MANIFEST.json'] and exception is not None,'unregistered compiled154 source mismatch')
            need(exception['status']=='passed' and exception['kind']=='control-source-manifest-metadata-equivalence' and exception['baseline153_and_production14_identical'] is True,'explicit root manifest-equivalence review required')
            need(str(checked_file(exception['actual_manifest']))==str(actual_root/'SOURCE_MANIFEST.json'),'actual historical manifest differs')
            checked_file(exception['provenance'])
        if role=='control':need(all(production[n]['sha256']==material[n]['sha256'] for n in material),'C14 control14 differs')
        digest=hashlib.sha256(__import__('json').dumps(production,sort_keys=True,separators=(',',':')).encode()).hexdigest()
        need(digest==cfg['binaries'][role]['source_content_sha256'],'actual adapter14 content hash differs')
        result[role]=dict(materialized_root=str(root),actual_compiled_source=str(actual_root),members=members,actual_members154=actual_members,production=production,actual_source_manifest=fid(actual_root/'SOURCE_MANIFEST.json'),metadata_only_differences=differences)
    return result

def bindings(cfg):
    need(set(cfg['binaries'])=={'control','candidate'},'exact two binary roles')
    binaries={r:fid(checked_file(e['binary'])) for r,e in cfg['binaries'].items()}
    for role,e in cfg['binaries'].items():
        need(e['prefix']==['/usr/bin/taskset','-c',str(cfg['cpu'])],'fresh registered taskset prefix differs')
        opts=e['effective_argv'];need('-O3' in opts and '-DNDEBUG' in opts and '-UNDEBUG' not in opts and '-freflection' in opts and '-fexpansion-statements' in opts,'bare original optimized P2996 profile required')
        need(not any('sanitize' in x or x in ('-include','-include-pch') or 'common_diag' in x or 'C14_DIAG' in x for x in opts),'diagnostic/SAN/forced instrumentation rejected')
        metadata=C.read(checked_file(e['metadata']));entry=metadata['binaries']['xoffset']
        need(all(e[k]==entry[k] for k in ('library','commit','trust_mode','load_contract','source_content_sha256')) and e['binary']['path']==entry['path'] and e['load_contract']=='detached','actual profile metadata differs')
        rows=C.read(checked_file(e['compile_commands']));rows=[r for r in rows if 'CMakeFiles/sc_xoffset.dir/' in r.get('command','')+' '+r.get('output','')]
        need(len(rows)==1 and (rows[0].get('arguments') or shlex.split(rows[0]['command']))==opts,'actual compile row differs')
        adapter=checked_file(e['adapter']).read_text()
        for key in ('library','commit','trust_mode','source_content_sha256'):
            found=re.search(r'\b'+('library_id' if key=='library' else key)+r'\s*=\s*"([^"\n]+)"',adapter)
            need(found is not None and found.group(1)==e[key],'actual adapter literal differs')
    required={'compiler','wrapper','profile_wrapper','libcxx','runtime','driver','adapter','dependencies','helper','request','full_functional','full_request','cost_policy','preconditions'}
    need(required<=set(cfg['closure']),'complete runtime/driver/receipt/cost closure missing')
    checked_file(DESIGN['compiler_launcher'])
    sources=source_snapshot(cfg)
    for role,key in (('control','control-dependencies'),('candidate','dependencies')):
        dependency=C.read(path(cfg['closure'][key]))
        need(dependency['status']=='passed' and dependency['candidate_actual_header_binding_verified'] is True and dependency['production_files']==sources[role]['production'],'actual compiled-dependency production closure differs: '+role)
        need(dependency['new_binary']['sha256']==binaries[role]['sha256'] and int(dependency['new_binary']['bytes'])==int(binaries[role]['bytes']) and dependency['profile']['sha256']==cfg['binaries'][role]['metadata']['identity']['sha256'],'actual compiled binary/profile binding differs: '+role)
    closure={n:C.tree_id(path(p)) for n,p in cfg['closure'].items()}
    fixture=path(cfg['fixture']);G.fixture_check(fixture,16384)
    return dict(sources=sources,binaries=binaries,closure=closure,tools=tools(),design=fid(DESIGN_PATH),input_policy=input_policy(cfg),
                metadata={r:fid(checked_file(e['metadata'])) for r,e in cfg['binaries'].items()},fixture=C.inventory(fixture,exclude=()),input_wire=fid(checked_file(cfg['input_wire'])),environment=cfg['environment'])

def config(root,out,output,control_profile,candidate_profile,preconditions):
    root=path(root);need(root==ROOT,'sole C14 root required');pre=C.read(preconditions)
    need(pre['status']=='passed' and pre['kind']=='v2-screen-preconditions' and pre['candidate']=='r1-canonical-hotcold-v1','fresh independently reviewed candidate preconditions required')
    cpu=pre['cpu'];need(type(cpu)is int and cpu>=0 and cpu in os.sched_getaffinity(0),'actual registered CPU unavailable')
    checked_file(pre['affinity_observation']);checked_file(pre['cost_policy']);checked_file(pre['functional_summary']);checked_file(pre['full_request'])
    policy=C.read(checked_file(pre['input_policy']));staged=C.read(checked_file(policy['staging_receipt']))
    need(pre['host_policy']['power_guid'] and pre['host_policy']['change_power'] is False,'register actual unchanged power condition')
    profiles={r:C.read(p) for r,p in (('control',control_profile),('candidate',candidate_profile))};need(profiles['control']['environment']==profiles['candidate']['environment'],'two profile environments differ')
    closure={};entries={};actual_sources={}
    for role,profile in profiles.items():
        need(profile['lock_dir']==str(LOCK) and profile['platform_class']=='wsl2-vm-x86_64','original lease/platform required')
        source=path(profile['closure']['candidate-source-tree']);actual_sources[role]=str(source)
        comp=path(profile['closure']['compile-commands']);rows=[r for r in C.read(comp) if 'CMakeFiles/sc_xoffset.dir/' in r.get('command','')+' '+r.get('output','')];need(len(rows)==1,'single original driver compile row')
        adapter=path(profile['closure'].get('candidate-adapter',str(source/'experiments/serialization_comparison/adapters/xoffset/adapter.hpp')))
        entry=profile['binaries']['xoffset'];entries[role]={k:entry[k] for k in ('library','commit','trust_mode','load_contract','source_content_sha256')}
        entries[role].update(binary=dict(path=entry['path'],identity=fid(entry['path'])),prefix=['/usr/bin/taskset','-c',str(cpu)],metadata=dict(path=str(control_profile if role=='control' else candidate_profile),identity=fid(control_profile if role=='control' else candidate_profile)),compile_commands=dict(path=str(comp),identity=fid(comp)),adapter=dict(path=str(adapter),identity=fid(adapter)),effective_argv=rows[0].get('arguments') or shlex.split(rows[0]['command']))
        for key,p in profile['closure'].items():
            if Path(p).is_file():closure[role+'-'+key]=str(path(p))
        closure[role+'-profile']=str(control_profile if role=='control' else candidate_profile)
    need(entries['control']['library']=='xoffset-receive-completion-v1' and entries['candidate']['library']=='xoffset-c14-r1-canonical-hotcold-v1','actual C14/candidate labels differ')
    cpp=path(entries['candidate']['adapter']['path']).parents[2]/'driver.cpp'
    closure['control-dependencies']=str(path(control_profile).parent/'compiled-source-binding-normal-v2.json')
    closure.update(compiler='/opt/p2996-toolchain/bin/clang++',wrapper=DESIGN['compiler_launcher']['path'],profile_wrapper='/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/bin/clang++',libcxx='/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu/libc++.so.1',runtime='/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu/libc++abi.so.1',driver=str(cpp),adapter=entries['candidate']['adapter']['path'],dependencies=str(path(candidate_profile).parent/'compiled-source-binding-normal-v2.json'),helper=str(path(DESIGN['source']['candidate'])/'xoffset_core_lifetime.hpp'),request=str(DESIGN_PATH),full_functional=pre['functional_summary']['path'],full_request=pre['full_request']['path'],cost_policy=pre['cost_policy']['path'],preconditions=str(path(preconditions)),taskset='/usr/bin/taskset',host_loader='/lib64/ld-linux-x86-64.so.2',host_libc='/lib/x86_64-linux-gnu/libc.so.6')
    cfg=dict(schema='r1-screen-config-v2',candidate='r1-canonical-hotcold-v1',attempt_id='R1-screen-20261020-v2-v1',output=str(path(output)),backing_mount='/mnt/c',cpu=cpu,environment=profiles['candidate']['environment'],sources={r:DESIGN['source'][r] for r in ('control','candidate')},actual_sources=actual_sources,binaries=entries,closure=closure,materialization=DESIGN['baseline_materialization'],candidate_identity=DESIGN['candidate_identity'],fixture=str(ROOT/'handoff/continuation-20261019-v1/portable-inputs/fixed-M'),input_wire=dict(path=str(ROOT/'handoff/continuation-20261019-v1/portable-inputs/old-V8-initial-wire.bin'),identity=fid(ROOT/'handoff/continuation-20261019-v1/portable-inputs/old-V8-initial-wire.bin')),preconditions=dict(path=str(path(preconditions)),identity=fid(preconditions)),control_manifest_exception=pre['control_manifest_exception'],host_policy=pre['host_policy'],actual_commit_policy='Legacy adapter commit is inherited benchmark base metadata only; exact source154/14, actual deps and content hash identify this candidate.')
    cfg.update(input_policy=pre['input_policy'],fixture=staged['fixture'],input_wire=dict(path=staged['wire'],identity=fid(staged['wire'])))
    cfg['closure'].update(input_policy=pre['input_policy']['path'],input_staging=policy['staging_receipt']['path'])
    validate_cfg(cfg);write(out,cfg);return dict(status='config_bound_not_screen_admitted',config_identity=fid(out),timing_samples=0)

def validate_cfg(cfg):
    need(cfg['schema']=='r1-screen-config-v2' and cfg['candidate']=='r1-canonical-hotcold-v1' and cfg['attempt_id']=='R1-screen-20261020-v2-v1','exact independent candidate/attempt only')
    need(path(cfg['output'])==Path('/root/xoffset-c14-iteration/20261020/v2/validation/R1-screen-20261020-v2-v1'),'single new candidate output domain')
    need(cfg['environment'] and all(type(k)is str and type(v)is str for k,v in cfg['environment'].items()) and cfg['backing_mount']=='/mnt/c','explicit unchanged environment/C backing')
    input_policy(cfg)
    checked_file(cfg['preconditions']);return bindings(cfg)

def active(queue,kind,maximum):
    need(platform.system()=='Linux' and 'microsoft' in platform.release().lower(),'WSL2 only')
    queue=path(queue);q=C.read(queue/'request.json');actual=Path('/proc/self/cmdline').read_bytes().split(b'\0')[:-1]
    expected=['/usr/bin/python3','-B',str(HERE/'screen.py'),*sys.argv[1:]]
    need(q['command']==expected and actual==[s.encode() for s in expected],'exact actual direct worker command required')
    need(q['owner']=='W3' and q['kind']==kind and q['runner_sha256']==C.sha(RUNNER) and q['lock_dir']==str(LOCK) and q['out']==str(queue) and q['cwd']==str(ROOT)==str(Path.cwd()) and q['timeout_seconds']==maximum,'own original queue/cwd/cap required')
    with (LOCK/'queue.lock').open('r+') as stream:
        try:fcntl.flock(stream,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:pass
        else:fcntl.flock(stream,fcntl.LOCK_UN);raise C.ProtocolError('original global lease not held')
        stream.seek(0);record=C.parse(stream.read())
    need(record['status']=='running' and record['supervisor_pid']==os.getppid() and record['run_id']==q['run_id'] and record['owner']=='W3' and record['kind']==kind and record['out']==str(queue),'own active lease out/run/supervisor required')
    need((Path('/proc')/str(os.getppid())/'cmdline').read_bytes().split(b'\0')[:-1]==[b'/usr/bin/python3',str(RUNNER).encode(),b'_worker',str(queue).encode()],'actual original supervisor command required')
    return dict(request=q,request_identity=fid(queue/'request.json'),lease_record=record)

def queue_audit(queue,kind,maximum,expected_command=None):
    queue=path(queue); manifest=C.read(queue/'manifest.json')
    names={'events.json','request.json','result.json','stderr.log','stdout.log','supervisor.log'}
    need(set(manifest)=={'schema_version','files'} and manifest['schema_version']==1 and type(manifest['files'])==list,'bad original queue manifest')
    records={r['path']:{'bytes':str(r['bytes']),'sha256':r['sha256']} for r in manifest['files']}
    need(len(manifest['files'])==6 and set(records)==names and set(p.name for p in queue.iterdir())==names|{'manifest.json'},'queue six-file inventory differs')
    need(all(not (queue/n).is_symlink() and fid(queue/n)==records[n] for n in names),'queue file differs from pinned manifest')
    q,r=C.read(queue/'request.json'),C.read(queue/'result.json')
    need(q['runner_sha256']==C.sha(RUNNER) and q['lock_dir']==str(LOCK) and q['owner']=='W3' and q['kind']==kind and q['out']==str(queue) and q['timeout_seconds']==maximum,'queue request identity/cap differs')
    need(r['run_id']==q['run_id'] and r['owner']==q['owner'] and r['kind']==q['kind'] and r['cleanup']=={'live':[],'process_groups':[],'zombies':[]},'queue result/cleanup differs')
    need(r['command_elapsed_seconds']>=0 and r['command_elapsed_seconds']<=maximum+q['termination_grace_seconds']+1,'outer queue exceeds cap and cleanup allowance')
    if expected_command is not None:need(q['command']==expected_command,'offline exact queue command differs')
    from datetime import datetime
    wall=(datetime.fromisoformat(r['finished_at'])-datetime.fromisoformat(q['created_at'])).total_seconds()
    need(wall>=r['command_elapsed_seconds']-.001 and wall<=maximum+q['wait_seconds']+q['termination_grace_seconds']+1,'outer queue total duration differs')
    return q,r

def argv(cfg,role,prefix):
    return G.command(dict(path=cfg['binaries'][role]['binary']['path'],prefix=cfg['binaries'][role]['prefix']),'run',
                     '--wire',cfg['input_wire']['path'],'--trace',path(cfg['fixture'])/'trace.bin',
                     '--wire-out',prefix.with_suffix('.wire'),'--canonical-out',prefix.with_suffix('.canonical'),
                     '--queries-out',prefix.with_suffix('.queries'),'--result',prefix.with_suffix('.native.json'))

def native(cfg,role,prefix,receipt,wire_size=None):
    row=G.native_result(prefix.with_suffix('.native.json'),cfg['binaries'][role],path(cfg['fixture']),path(cfg['input_wire']['path']),
                        prefix.with_suffix('.wire'),prefix.with_suffix('.canonical'),prefix.with_suffix('.queries'),receipt,wire_size)
    phases={n:C.decimal(row['phase_ns'][n]) for n in PHASES}
    # total_ns includes timer bookkeeping; infer the registered total from phases.
    need(C.decimal(row['peak_rss_after_audit_kib'])*1024<=16*GiB,'RSS limit exceeded')
    values={**phases,'read_sum':phases['query_before']+phases['query_after'],'total':sum(phases.values())}
    return row,values

def byte_equal(a,b):
    if Path(a).stat().st_size!=Path(b).stat().st_size: return False
    with Path(a).open('rb') as x,Path(b).open('rb') as y:
        while True:
            u,v=x.read(1024*1024),y.read(1024*1024)
            if u!=v:return False
            if not u:return True

def allocated(directory):
    seen=set(); total=0; apparent=0
    for p in (Path(directory),*Path(directory).rglob('*')):
        s=p.lstat(); need(not p.is_symlink(),'output symlink disallowed')
        apparent+=s.st_size
        key=(s.st_dev,s.st_ino)
        if key not in seen: total+=s.st_blocks*512; seen.add(key)
    return dict(unique_allocated_bytes=total,apparent_bytes=apparent,unique_inodes=len(seen))

def screen_allocated(out,f):
    # Include both retained functional captures and the frozen metadata directory.
    roots=(path(out),path(f['capture']),path(f['frozen_path']).parent,Path('/root/xoffset-c14-iteration/20261020/v2/validation/screen-inputs-v1'))
    return sum(allocated(p)['unique_allocated_bytes'] for p in roots)

def resources(cfg,peak):
    guest=shutil.disk_usage(path(cfg['output']).parent).free
    backing=shutil.disk_usage('/mnt/c').free
    need(guest>=peak+2*GiB and backing>=peak+2*GiB,'guest/C backing cannot cover peak plus 2GiB reserve')
    return dict(guest_free_bytes=guest,C_backing_free_bytes=backing,required_peak_bytes=peak,reserve_bytes=2*GiB)

def capture(config,out,queue,request,request_sha256,review):
    cfg=C.read(config);before=validate_cfg(cfg);out=path(out)
    need(C.hexsha(request_sha256) and C.sha(request)==request_sha256,'exact finite resource-capture request required')
    req=C.read(request);admit=C.read(review)
    need(req['kind']=='v2-screen-finite-resource-capture' and req['candidate']=='r1-canonical-hotcold-v1' and req['config_identity']==fid(config) and req['runtime']==before,'finite capture source/input/tool/runtime closure differs')
    need(req['output']==str(out) and req['queue']==str(queue) and req['roles']==['control','candidate'] and req['children']==2 and req['child_seconds']==20 and req['outer_seconds']==120 and req['compile']==0 and req['retry']==0 and req['timing_inference_samples']==0,'exact one-control/one-candidate finite capture budget required')
    need(admit['kind']=='independent-resource-capture-admission' and admit['status']=='passed' and admit['candidate']=='r1-canonical-hotcold-v1' and admit['request_sha256']==request_sha256,'independent exact capture admission required')
    need(out==Path('/root/xoffset-c14-iteration/20261020/v2/validation/r1-screen-resource-capture-v1') and queue==out/'queue' and set(p.name for p in out.iterdir())=={'queue'},'one new resource-capture prefix required')
    lease=active(queue,'functional',120);write(out/'lease.json',lease);C.copy_file(config,out/'config.json');C.copy_file(request,out/'capture-request.json');C.copy_file(review,out/'capture-review.json');write(out/'runtime-before.json',before)
    resources(cfg,14*64*1024**2);(out/'commands').mkdir();completed=[];error=None;started=time.monotonic()
    try:
        for role in ('control','candidate'):
            need(time.monotonic()-started<=60,'capture must leave complete second child and sealing room')
            prefix=out/'commands'/role;t=time.monotonic();receipt=G.execute(argv(cfg,role,prefix),prefix,20,cfg['environment'],64*1024**2)
            _,values=native(cfg,role,prefix,receipt)
            need(receipt['returncode']==0 and receipt['timed_out'] is False and receipt['launch_error'] is None and prefix.with_suffix('.stderr').stat().st_size==0,'full bare resource child failed')
            need(byte_equal(prefix.with_suffix('.canonical'),path(cfg['fixture'])/'final.canonical.bin') and byte_equal(prefix.with_suffix('.queries'),path(cfg['fixture'])/'queries.bin'),'capture full byte truth mismatch')
            completed.append(dict(role=role,receipt=fid(prefix.with_suffix('.receipt.json')),wire_bytes=prefix.with_suffix('.wire').stat().st_size,canonical_bytes=prefix.with_suffix('.canonical').stat().st_size,queries_bytes=prefix.with_suffix('.queries').stat().st_size,full_process_seconds=(int(receipt['finished_monotonic_ns'])-int(receipt['started_monotonic_ns']))/1e9,online_child_audit_seconds=time.monotonic()-t,values=values))
    except (C.ProtocolError,OSError,ValueError) as exception:error=type(exception).__name__+': '+str(exception)
    try:after=bindings(cfg)
    except (C.ProtocolError,OSError,ValueError) as exception:after=dict(identity_error=str(exception))
    write(out/'runtime-after.json',after)
    if after!=before:error='resource capture before/after closure differs'
    if time.monotonic()-started>120:error='capture outer120s budget exceeded'
    C.seal(out,dict(schema='r1-functional-resource-capture-v2',status='passed' if error is None else 'failed',error=error,completed=completed,elapsed_seconds=time.monotonic()-started,request_sha256=request_sha256,timing_inference_samples=0,warmup=False,retries=0))
    need(error is None,'finite resource capture closed: '+str(error));return dict(status='resource_capture_complete_not_timing_inference',roles=2,new_compiles=0,retry=0)

def projection(cfg,capture_dir):
    if path(capture_dir).is_dir():return project_capture(cfg,path(capture_dir))
    # Registration references already completed own functional M runs. It never
    # executes a capture, and cannot consume the closed old M1 timing prefix.
    registration=path(capture_dir);s=C.read(registration)
    need(s['kind']=='v2-screen-existing-functional-projection' and s['status']=='passed' and s['candidate']=='r1-canonical-hotcold-v1','existing full bare functional projection review required')
    need(s['candidate_identity']==cfg['candidate_identity'] and s['screen_config_identity']==fid(checked_file(s['screen_config'])),'projection identity/config binding differs')
    need(C.read(checked_file(s['screen_config']))==cfg,'projection config differs')
    need(s['not_timing_inference'] is True and s['no_old_screen_samples'] is True and s['diagnostic_observer_runs_used'] is False,'projection must exclude timing prefixes/diagnostic runs')
    need(set(s['roles'])=={'control','candidate'},'two existing bare-run roles required')
    observed=[]
    for role,item in s['roles'].items():
        prefix=path(item['output_prefix']);receipt_prefix=path(item['receipt_prefix']);receipt=C.read(checked_file(item['receipt']));fixture=path(item['fixture']);wire=checked_file(item['input_wire'])
        need('screen' not in str(prefix).lower() and 'diag' not in str(prefix).lower(),'screen/diagnostic prefix cannot be a functional projection')
        need(item['profile']==cfg['binaries'][role]['metadata'] and item['binary']==cfg['binaries'][role]['binary'],'actual role profile/binary differs')
        G.fixture_check(fixture,16384)
        need(all(fid(fixture/n)==fid(path(cfg['fixture'])/n) for n in (*G.FILES,'fixture.json')) and fid(wire)==cfg['input_wire']['identity'],'existing M fixture/input bytes differ')
        need(os.stat(wire).st_dev==os.stat(cfg['input_wire']['path']).st_dev and os.stat(fixture).st_dev==os.stat(cfg['fixture']).st_dev,'root functional process cannot project a different mounted storage domain')
        checked_file(item['same_storage_domain_review'])
        expected=G.command(dict(path=cfg['binaries'][role]['binary']['path'],prefix=item['actual_prefix']),'run','--wire',str(wire),'--trace',fixture/'trace.bin','--wire-out',prefix.with_suffix('.wire'),'--canonical-out',prefix.with_suffix('.canonical'),'--queries-out',prefix.with_suffix('.queries'),'--result',prefix.with_suffix('.native.json'))
        need(receipt['argv']==expected and receipt['execution_environment']==cfg['environment'] and receipt['returncode']==0 and receipt['timed_out'] is False and receipt['launch_error'] is None,'existing complete process receipt/argv differs')
        need(item['actual_prefix'] in ([],cfg['binaries'][role]['prefix']) and receipt_prefix.with_suffix('.stderr').stat().st_size==0,'unexpected functional wrapper/stderr')
        need(checked_file(item['receipt'])==receipt_prefix.with_suffix('.receipt.json'),'raw receipt path differs')
        need(set(item['body_files'])=={'wire','canonical','queries','native.json','stdout','stderr'},'all actual output/receipt bodies required')
        for ext,reference in item['body_files'].items():
            actual=receipt_prefix.with_suffix('.'+ext) if ext in ('stdout','stderr') else prefix.with_suffix('.'+ext)
            need(checked_file(reference)==actual,'actual functional output body reference differs')
        q=checked_file(item['queue_request']);r=checked_file(item['queue_result']);qr=C.read(q);rr=C.read(r)
        need(qr['runner_sha256']==C.sha(RUNNER) and qr['lock_dir']==str(LOCK) and qr['kind']=='functional' and rr['run_id']==qr['run_id'] and rr['status']=='complete' and rr['command_returncode']==0 and rr['cleanup']=={'live':[],'process_groups':[],'zombies':[]},'original successful functional lease/cleanup required')
        checked_file(item['full_audit']);checked_file(item['runtime_before']);checked_file(item['runtime_after']);checked_file(item['functional_manifest'])
        need(C.read(item['runtime_before']['path'])==C.read(item['runtime_after']['path']),'functional before/after runtime changed')
        row=G.native_result(prefix.with_suffix('.native.json'),cfg['binaries'][role],fixture,wire,prefix.with_suffix('.wire'),prefix.with_suffix('.canonical'),prefix.with_suffix('.queries'),receipt)
        need(C.decimal(row['peak_rss_after_audit_kib'])*1024<=16*GiB,'functional full-run RSS exceeds16GiB')
        seconds=(int(receipt['finished_monotonic_ns'])-int(receipt['started_monotonic_ns']))/1e9
        need(0<seconds<=20,'actual full process cannot fit child20s')
        margin=item['process_plus_online_audit_bound_seconds'];need(type(margin) in (int,float) and math.isfinite(margin) and margin>=seconds+item['registered_online_audit_margin_seconds'] and item['registered_online_audit_margin_seconds']>0,'explicit positive full-body audit/dedup margin required')
        checked_file(item['margin_review'])
        observed.append(dict(role=role,full_process_seconds=seconds,online_child_audit_seconds=margin,wire_bytes=prefix.with_suffix('.wire').stat().st_size,canonical_bytes=prefix.with_suffix('.canonical').stat().st_size,queries_bytes=prefix.with_suffix('.queries').stat().st_size))
    wire=max(r['wire_bytes'] for r in observed);canonical=max(r['canonical_bytes'] for r in observed);queries=max(r['queries_bytes'] for r in observed);limit=max(wire,canonical,queries,1024**2)
    metadata=64*1024**2;retained=allocated(registration)['unique_allocated_bytes']
    staging=input_policy(cfg)
    peak=144*((wire+4095)//4096*4096)+canonical+queries+7*limit+metadata+retained+staging['actual_allocated_bytes']
    online=144*max(r['online_child_audit_seconds'] for r in observed)+POLICY['nonchild_projection_seconds']+staging['elapsed_seconds']
    need(peak<=4*GiB and online<=600-POLICY['seal_seconds'],'complete-process/full-body-audit or physical projection cannot fit registered cap')
    return dict(wire_max_bytes=wire,wire_bytes_by_role={r['role']:r['wire_bytes'] for r in observed},canonical_bytes=canonical,queries_bytes=queries,file_limit_bytes=limit,all_wire_unique=True,metadata_reserve_bytes=metadata,retained_capture_allocated_bytes=retained,temporary_slots=7,physical_peak_bytes=peak,logical_output_bytes=144*(wire+canonical+queries),projected_online_seconds=online,observed_complete_functional=observed,staging_engineering_cost=staging,capture_manifest=fid(registration),actual_free=resources(cfg,peak))

def freeze(config,capture_dir,gates,out):
    cfg=C.read(config);before=validate_cfg(cfg);gate=C.read(gates)
    need(gate['kind']=='v2-screen-complete-gates-admission' and gate['status']=='passed' and gate['candidate_identity']==cfg['candidate_identity'],'same actual candidate complete gate admission required')
    need(set(gate['gates'])==set(GATES),'all eleven candidate gate dependencies required')
    records={}
    expected={'codegen':{'candidate_compiles':1},'mechanism':{'actual_nonempty_path_observed':True,'static_actual_codegen_passed':True,'unhooked_screen_binary':True},'targeted':{'normal':int('5'),'sanitized':int('5')},'full_normal':{'ctest_passed':73},'full_san':{'ctest_passed':73},'business_SML':{'normal_children':80,'sanitized_children':80},'v1_bidirectional':{'main_cases':84,'extra_cases':8,'AB_actions':4,'C_actions':4},'consumers':{'install_subproject_passed':True,'shared_runtime_passed':True},'lifecycle':{'actual_native_children':52,'normal_model_cases':25,'sanitized_model_cases':25,'pool_cases':2},'safety':{'representation_and_lifetime_preserved':True},'engineering':{'predeclared_cost_policy_satisfied':True}}
    for name,reference in gate['gates'].items():
        p=checked_file(reference);e=C.read(p)
        need(e['kind']=='v2-screen-gate-receipt' and e['gate']==name and e['status']=='passed' and e['candidate_identity']==cfg['candidate_identity'],'typed actual candidate gate receipt required: '+name)
        need(e['source154']==before['sources']['candidate']['members'] and e['production14']==before['sources']['candidate']['production'],'gate source154/14 differs: '+name)
        need(all(e['assertions'].get(k)==v for k,v in expected[name].items()),'actual gate counts/assertions differ: '+name)
        need(e['evidence'] and all(checked_file(r).is_file() for r in e['evidence']),'real frozen evidence refs required: '+name)
        need(e['independent_review']['status']=='passed' and checked_file(e['independent_review']['record']).is_file(),'independent raw evidence review required: '+name)
        records[name]=dict(path=str(p),identity=fid(p))
    need(not path(cfg['output']).exists(),'attempt output already exists; replacement/retry forbidden')
    projection_record=projection(cfg,path(capture_dir));need(before==bindings(cfg),'identity changed while assembling freeze')
    out=C.fresh(out)
    record=dict(schema='r1-screen-freeze-v2',frozen_path=str(out/'freeze.json'),config=cfg,config_identity=fid(config),design=dict(path=str(DESIGN_PATH),identity=fid(DESIGN_PATH)),policy=POLICY,schedule=schedule(),randomization=DESIGN['randomization'],runtime=before,gates=records,gate_admission=dict(path=str(path(gates)),identity=fid(gates)),capture=str(path(capture_dir)),projection=projection_record,performance_accepted=False,independent_review_required=True)
    write(out/'freeze.json',record)
    return dict(status='frozen_pending_independent_review',freeze_sha256=C.sha(out/'freeze.json'),output=cfg['output'])

def check_freeze(frozen,external):
    need(C.hexsha(external) and C.sha(frozen)==external,'external freeze pin mismatch')
    f=C.read(frozen);need(f['schema']=='r1-screen-freeze-v2' and f['policy']==POLICY and f['schedule']==schedule() and f['design']==dict(path=str(DESIGN_PATH),identity=fid(DESIGN_PATH)),'fixed v2 policy/design/randomized schedule mismatch')
    return f

def dedup(cfg,prefix,store,ledger):
    # Called only after native_result and complete byte-exact oracle comparisons.
    for ext,oracle in (('.canonical',path(cfg['fixture'])/'final.canonical.bin'),('.queries',path(cfg['fixture'])/'queries.bin')):
        p=prefix.with_suffix(ext); need(byte_equal(p,oracle),'byte-exact dedup oracle mismatch')
        destination=store/('oracle'+ext)
        before=dict(path=str(p),identity=fid(p),inode=p.stat().st_ino,blocks=p.stat().st_blocks)
        if not destination.exists():os.link(p,destination)
        else:
            need(byte_equal(p,destination),'dedup store differs')
            # No deletion on a failed or partial child; only validated full bodies.
            temporary=p.with_suffix(ext+'.validated-link'); os.link(destination,temporary); os.replace(temporary,p)
        ledger.append(dict(**before,oracle=str(oracle),oracle_identity=fid(oracle),byte_exact=True,store=str(destination),after_inode=p.stat().st_ino,after_blocks=p.stat().st_blocks))

def run(frozen,external,review,host,out,queue):
    start=time.monotonic(); deadline=start+600
    f=check_freeze(frozen,external); cfg=f['config'];out=path(out)
    need(out==path(cfg['output']) and queue==out/'queue' and set(p.name for p in out.iterdir())=={'queue'},'attempt prefix already used')
    lease=active(queue,'timing',600)
    need(cfg['cpu'] in os.sched_getaffinity(0),'fresh registered CPU no longer allowed')
    for p,kind in ((review,'independent-screen-admission'),(host,'host-quiet-admission')):
        a=C.read(p);need(a['kind']==kind and a['status']=='passed' and a['freeze_sha256']==external,'independent/host quiet admission not bound to freeze')
    h=C.read(host);need(h['quiet'] is True and h['original_global_lease'] is True and h['restoration_ready'] is True,'host quiet/restoration not ready')
    need(C.decimal(h['ready_monotonic_ns'],True)<=time.monotonic_ns(),'host ready timestamp is in future')
    C.copy_file(frozen,out/'freeze.json'); write(out/'freeze-pin.json',{'sha256':external});C.copy_file(review,out/'review.json');C.copy_file(host,out/'host-admission.json');write(out/'lease.json',lease)
    write(out/'execution-context.json',dict(frozen=str(path(frozen)),review=dict(path=str(path(review)),identity=fid(review)),host=dict(path=str(path(host)),identity=fid(host))))
    write(out/'runtime-before.json',bindings(cfg)); need(C.read(out/'runtime-before.json')==f['runtime'],'runtime differs from freeze')
    for r in f['gates'].values():checked_file(r)
    checked_file(f['gate_admission']);capture_path=path(f['capture']);need(fid(capture_path/'manifest.json' if capture_path.is_dir() else capture_path)==f['projection']['capture_manifest'],'registered functional projection changed');resources(cfg,f['projection']['physical_peak_bytes'])
    (out/'commands').mkdir();(out/'dedup').mkdir();attempted=[];completed=[];ledger=[];error=None
    try:
        for job in schedule():
            need(deadline-time.monotonic()>=20+POLICY['seal_seconds'],'no room for worst-case child plus seal; close prefix')
            usage=screen_allocated(out,f);limit=f['projection']['file_limit_bytes']
            need(usage+7*limit+1024**2<=4*GiB,'physical cap cannot cover next child')
            resources(cfg,7*limit+1024**2)
            prefix=out/'commands'/job['id'];attempted.append(job)
            write(out/'commands'/(job['id']+'.attempt.json'),job)
            receipt=G.execute(argv(cfg,job['role'],prefix),prefix,20,cfg['environment'],limit)
            row,values=native(cfg,job['role'],prefix,receipt,f['projection']['wire_bytes_by_role'][job['role']])
            dedup(cfg,prefix,out/'dedup',ledger)
            write(out/'commands'/(job['id']+'.dedup.json'),ledger[-2:])
            completed.append(dict(job=job,values=values))
            need(screen_allocated(out,f)<=4*GiB,'physical output cap exceeded')
    except (C.ProtocolError,OSError,ValueError) as e:error=type(e).__name__+': '+str(e)
    try:after=bindings(cfg)
    except (C.ProtocolError,OSError,ValueError) as e:after={'identity_error':str(e)}
    write(out/'runtime-after.json',after)
    if after!=f['runtime']:error='runtime/source/tool identity changed'
    elapsed=time.monotonic()-start
    if elapsed>600:error='600 second command cap exceeded'
    write(out/'allocation.json',allocated(out))
    C.seal(out,dict(schema='r1-screen-run-v2',status='complete' if error is None else 'failed',error=error,
                    attempted=attempted,completed=completed,unexecuted=schedule()[len(attempted):],elapsed_seconds=elapsed,
                    performance_accepted=False,user_restoration_required_before_audit=True))
    need(error is None,'screen closed: '+str(error))
    return dict(status='complete_pending_restoration_and_independent_audit',timing_children=len(completed))

def summary(samples):
    need(len(samples)==144 and [s['job'] for s in samples]==schedule(),'complete exact role/schedule144 required')
    pairs={i:{} for i in range(72)}
    for sample in samples:
        job=sample['job'];values=sample['values'];need(set(values)==set(METRICS) and all(type(x)is int and 0<=x<=2**64-1 for x in values.values()),'missing/negative/noncanonical/nonfinite metric')
        need(job['role'] not in pairs[job['pair']],'duplicate role');pairs[job['pair']][job['role']]=values
    result={};undefined=[];primary=POLICY['primary']
    for metric in METRICS:
        if any(p['candidate'][metric]==0 or p['control'][metric]==0 for p in pairs.values()):
            result[metric]=dict(n=72,df=71,status='undefined_log_contrast_zero_preserved');undefined.append(metric);continue
        xs=[math.log(p['candidate'][metric]/p['control'][metric]) for p in pairs.values()];mean=statistics.mean(xs);se=statistics.stdev(xs)/math.sqrt(72)
        t=POLICY['primary_t'] if metric==primary else POLICY['auxiliary_t']
        item=dict(n=72,df=71,mean_log=mean,se_log=se,point=math.exp(mean),upper=math.exp(mean+t*se))
        if metric!=primary:item['lower']=math.exp(mean-t*se)
        need(all(math.isfinite(v) for v in item.values()),'nonfinite inferential output');result[metric]=item
    regressions=[m for m in METRICS if m!=primary and m not in undefined and result[m]['lower']>1]
    improvement=not undefined and result[primary]['upper']<1
    return dict(metrics=result,primary=primary,undefined_metrics=undefined,primary_improvement=improvement,confirmed_auxiliary_regression=regressions,statistically_eligible=improvement and not regressions,engineering_and_independent_audit_still_required=True,performance_accepted=False,QF_admitted=False)

def postrun_pin(out):
    return hashlib.sha256(b'R1-SCREEN-POSTRUN-v2\0'+bytes.fromhex(C.sha(Path(out)/'manifest.json'))+bytes.fromhex(C.sha(Path(out)/'queue/manifest.json'))).hexdigest()

def audit(out,external,postrun,restoration,result,audit_queue):
    began=time.monotonic();out=path(out);f=check_freeze(out/'freeze.json',external);cfg=f['config']
    audit_lease=active(audit_queue,'validation',3600)
    restored=C.read(restoration);need(restored['status']=='passed' and restored['freeze_sha256']==external and restored['user_processes_restored'] is True,'user restoration must precede offline audit')
    need(C.hexsha(postrun) and postrun_pin(out)==postrun,'external complete postrun pin differs')
    C.check_inventory(out);status=C.read(out/'status.json');need(status['schema']=='r1-screen-run-v2','wrong run bundle')
    need(C.inventory(path(cfg['fixture']),exclude=())==f['runtime']['fixture'] and fid(path(cfg['input_wire']['path']))==f['runtime']['input_wire'],'actual oracle/input differs from frozen')
    need(status['attempted']==schedule()[:len(status['attempted'])] and status['unexecuted']==schedule()[len(status['attempted']):],'not exact attempt prefix')
    need(C.read(out/'runtime-before.json')==f['runtime'],'before identity differs')
    need(status['status']=='failed' or C.read(out/'runtime-after.json')==f['runtime'],'successful run identity differs')
    q,qr=queue_audit(out/'queue','timing',600,C.read(out/'lease.json')['request']['command'])
    context=C.read(out/'execution-context.json');command=q['command'];need(len(command)>=3,'short original command')
    requested=command[0];tail=command[1:]
    if tail[0]=='-B':tail=tail[1:]
    need(tail==[str(HERE/'screen.py'),'run','--frozen',context['frozen'],'--freeze-sha256',external,'--review',context['review']['path'],'--host',context['host']['path'],'--out',str(out),'--queue',str(out/'queue')],'offline exact R1 command differs; use documented argv order')
    need(str(Path(requested).resolve()) in f['runtime']['tools'] and fid(Path(requested).resolve())==f['runtime']['tools'][str(Path(requested).resolve())],'requested Python differs from frozen')
    need(context['frozen']==f['frozen_path'] and fid(out/'review.json')==context['review']['identity'] and fid(out/'host-admission.json')==context['host']['identity'],'execution input binding differs')
    need(q==C.read(out/'lease.json')['request'] and q['runner_sha256']==f['runtime']['tools'][str(RUNNER)]['sha256'],'queue provenance differs')
    need(q['kind']=='timing' and q['owner']=='W3' and q['lock_dir']==str(LOCK),'wrong global queue namespace')
    previous=0;first=None;samples=[];validated_uncommitted=[]
    for index,job in enumerate(status['attempted']):
        need(time.monotonic()-began<3600,'audit exceeded 3600s')
        prefix=out/'commands'/job['id'];receipt=C.read(prefix.with_suffix('.receipt.json'))
        need(receipt['argv']==argv(cfg,job['role'],prefix) and receipt['execution_environment']==cfg['environment'] and receipt['timeout_seconds']==20 and C.decimal(receipt['file_limit_bytes'])==f['projection']['file_limit_bytes'],'receipt binding differs')
        s,e=int(receipt['started_monotonic_ns']),int(receipt['finished_monotonic_ns'])
        need(s>=previous and e>=s,'overlapping/backward children'); previous=e
        if first is None:first=s
        try:
            need(receipt['returncode']==0 and receipt['timed_out'] is False and receipt['launch_error'] is None and prefix.with_suffix('.stderr').stat().st_size==0 and e-s<=20e9,'failed child')
            row,values=native(cfg,job['role'],prefix,receipt,f['projection']['wire_bytes_by_role'][job['role']])
            need(byte_equal(prefix.with_suffix('.canonical'),path(cfg['fixture'])/'final.canonical.bin') and byte_equal(prefix.with_suffix('.queries'),path(cfg['fixture'])/'queries.bin'),'full byte oracle mismatch')
            d=C.read(out/'commands'/(job['id']+'.dedup.json'))
            need(len(d)==2 and all(x['byte_exact'] is True and Path(x['path']).stat().st_ino==Path(x['store']).stat().st_ino and fid(x['path'])==x['identity']==x['oracle_identity'] for x in d),'dedup evidence mismatch')
        except (C.ProtocolError,OSError,ValueError):
            need(status['status']=='failed' and index==len(status['attempted'])-1,'unreported failure/continued sampling');break
        item=dict(job=job,values=values)
        if index<len(status['completed']):samples.append(item)
        else:
            need(status['status']=='failed' and index==len(status['attempted'])-1,'uncommitted nonfinal child')
            validated_uncommitted.append(item)
    need(samples==status['completed'],'completed ledger differs from raw receipts')
    host=C.read(out/'host-admission.json')
    need(first is None or C.decimal(host['ready_monotonic_ns'],True)<=first<=previous<=C.decimal(restored['resumed_monotonic_ns'],True),'sampling outside host-ready/resumed interval')
    need(restored['power_before']==restored['power_after']==cfg['host_policy']['power_guid'],'fresh registered power preservation evidence differs')
    complete=status['status']=='complete'
    if complete:
        need(len(samples)==144 and len(status['attempted'])==144 and status['elapsed_seconds']<=600,'incomplete successful attempt')
        need(qr['status']=='complete' and qr['command_returncode']==0 and qr['command_elapsed_seconds']<=600,'outer queue failed or exceeded cap')
    need(screen_allocated(out,f)<=4*GiB,'physical allocation cap exceeded')
    report=dict(schema='r1-screen-audit-v2',status='passed' if complete else 'preserved_failure',freeze_sha256=external,
                run_manifest=fid(out/'manifest.json'),postrun_pin=postrun,queue_result=fid(out/'queue/result.json'),restoration=fid(restoration),
                children=len(samples),summary=summary(samples) if complete else None,elapsed_seconds=time.monotonic()-began,
                validated_uncommitted=validated_uncommitted,audit_lease=audit_lease,
                engineering_and_independent_recalculation_still_required=True,performance_accepted=False)
    need(time.monotonic()-began<3600,'complete audit exceeded 3600s')
    write(result,report);return report

def failure(out,external,restoration,result,audit_queue):
    """Archive unsealed/outer-killed failure without mutating or replacing it."""
    began=time.monotonic();lease=active(audit_queue,'validation',3600);out=path(out)
    f=check_freeze(out/'freeze.json',external)
    restored=C.read(restoration);need(restored['status']=='passed' and restored['freeze_sha256']==external and restored['user_processes_restored'] is True,'restore user processes before failure audit')
    q,r=queue_audit(out/'queue','timing',600)
    need(not (out/'manifest.json').exists() or not (out/'status.json').exists() or r['status']!='complete' or r['command_returncode']!=0,'complete successful window requires full audit')
    inventory=C.inventory(out,exclude=(),exclude_roots=('queue',))
    report=dict(schema='r1-screen-unsealed-failure-v2',status='failed_preserved',freeze_sha256=external,
        window_closed=True,retry=0,replacement=0,queue_manifest=fid(out/'queue/manifest.json'),queue_result=r,
        original_prefix_files=inventory,original_prefix_pin=pin(inventory),restoration=fid(restoration),
        audit_lease=lease,performance_accepted=False,QF_admitted=False)
    need(time.monotonic()-began<3600,'failure audit exceeded cap');write(result,report);return report

def input_policy(cfg):
    policy=C.read(checked_file(cfg['input_policy']))
    need(policy['kind']=='v2-screen-staged-input-policy' and policy['status']=='passed' and policy['candidate']=='r1-canonical-hotcold-v1' and policy['all_roles_same_paths'] is True and policy['API_model_trace_unchanged'] is True,'independent candidate staged-input policy approval required')
    staged=C.read(checked_file(policy['staging_receipt']));checked_file(policy['independent_review'])
    need(staged['status']=='staged_pending_screen_policy_admission' and staged['new_compiles']==staged['new_native']==staged['new_timing_samples']==0 and staged['file_count']==len(staged['files'])==9,'actual source-only staging receipt required')
    need(cfg['fixture']==staged['fixture'] and cfg['input_wire']['path']==staged['wire'],'selected common staged paths differ')
    base=Path('/root/xoffset-c14-iteration/20261020/v2/validation/screen-inputs-v1');original=ROOT/'handoff/continuation-20261019-v1/portable-inputs'
    expected={str(original/'fixed-M'/n):str(base/'fixed-M'/n) for n in ('final.canonical.bin','fixture.json','frozen-reference.py','input.canonical.bin','queries.bin','query-after.bin','query-before.bin','trace.bin')};expected[str(original/'old-V8-initial-wire.bin')]=str(base/'initial-wire.bin')
    need({r['source']:r['target'] for r in staged['files']}==expected,'exact original9 to native WSL staged9 mapping required')
    for record in staged['files']:
        for name in ('source','target'):checked_file(dict(path=record[name],identity=record['identity']))
    need(sum(r['identity']['bytes'] for r in staged['files'])==staged['bytes'] and staged['actual_allocated_bytes']>=staged['bytes'] and 0<staged['elapsed_seconds']<=60,'actual staging cost/bytes required')
    checked_file(staged['tool']);checked_file(staged['freeze']);cresult=C.read(checked_file(policy['C_result']))
    need(cresult['status']=='passed' and cresult['actual_diagnostic_native']==2 and policy['C_result']['identity']['sha256']=='c13e761ba41a67a7abcac600fbcea2a1edfe517032c782ccc98f1d41cbeab2c4','exact actual C diagnostic basis required')
    for role in ('c14','r1'):checked_file(cresult['roles'][role]['observer'])
    need(policy['staging_engineering_cost_accounted'] is True and policy['diagnostic_times_not_AB_speed'] is True,'charge staging engineering work; diagnostics cannot establish performance')
    return dict(policy=dict(path=str(checked_file(cfg['input_policy'])),identity=fid(cfg['input_policy']['path'])),staging=policy['staging_receipt'],bytes=staged['bytes'],elapsed_seconds=staged['elapsed_seconds'],actual_allocated_bytes=allocated(base)['unique_allocated_bytes'],files=staged['files'])


def project_capture(cfg,directory):
    C.check_inventory(directory);s=C.read(directory/'status.json')
    need(s['schema']=='r1-functional-resource-capture-v2' and s['status']=='passed' and [r['role'] for r in s['completed']]==['control','candidate'],'one exact complete two-role resource capture required')
    need(C.read(directory/'config.json')==cfg and C.read(directory/'runtime-before.json')==C.read(directory/'runtime-after.json')==bindings(cfg),'capture exact full closure differs')
    request=C.read(directory/'capture-request.json');admission=C.read(directory/'capture-review.json')
    need(s['request_sha256']==C.sha(directory/'capture-request.json')==admission['request_sha256'] and request['candidate']=='r1-canonical-hotcold-v1' and admission['status']=='passed' and admission['kind']=='independent-resource-capture-admission','actual finite capture admission differs')
    q,r=queue_audit(directory/'queue','functional',120,C.read(directory/'lease.json')['request']['command'])
    need(r['status']=='complete' and r['command_returncode']==0 and r['command_elapsed_seconds']<=120,'finite capture outer failed')
    pre=C.read(checked_file(cfg['preconditions']));tail=pre['projection_tail_margin_seconds'];need(type(tail) in (int,float) and math.isfinite(tail) and tail>0,'predeclared positive dedup/ledger/full audit tail margin required')
    for item in s['completed']:
        role=item['role'];prefix=directory/'commands'/role;receipt=C.read(prefix.with_suffix('.receipt.json'))
        need(receipt['argv']==argv(cfg,role,prefix) and receipt['execution_environment']==cfg['environment'] and receipt['returncode']==0 and receipt['timed_out'] is False and receipt['timeout_seconds']==20 and fid(prefix.with_suffix('.receipt.json'))==item['receipt'] and prefix.with_suffix('.stderr').stat().st_size==0,'finite capture raw process receipt differs')
        _,values=native(cfg,role,prefix,receipt,item['wire_bytes']);elapsed=(int(receipt['finished_monotonic_ns'])-int(receipt['started_monotonic_ns']))/1e9
        need(values==item['values'] and elapsed==item['full_process_seconds'] and 0<elapsed<=20 and item['online_child_audit_seconds']>=elapsed,'complete process and full-body audit time differs')
        need(item['canonical_bytes']==prefix.with_suffix('.canonical').stat().st_size and item['queries_bytes']==prefix.with_suffix('.queries').stat().st_size,'capture body sizes differ')
    wire=max(x['wire_bytes'] for x in s['completed']);canonical=max(x['canonical_bytes'] for x in s['completed']);queries=max(x['queries_bytes'] for x in s['completed']);limit=max(wire,canonical,queries,1024**2);retained=allocated(directory)['unique_allocated_bytes'];staging=input_policy(cfg)
    peak=144*((wire+4095)//4096*4096)+canonical+queries+7*limit+64*1024**2+retained+staging['actual_allocated_bytes']
    online=144*(max(x['online_child_audit_seconds'] for x in s['completed'])+tail)+POLICY['nonchild_projection_seconds']+staging['elapsed_seconds']
    need(peak<=4*GiB and online<=600-POLICY['seal_seconds'],'actual complete process/audit+tail/staging or physical projection cannot fit; do not launch screen')
    return dict(wire_max_bytes=wire,wire_bytes_by_role={x['role']:x['wire_bytes'] for x in s['completed']},canonical_bytes=canonical,queries_bytes=queries,file_limit_bytes=limit,all_wire_unique=True,metadata_reserve_bytes=64*1024**2,retained_capture_allocated_bytes=retained,temporary_slots=7,physical_peak_bytes=peak,logical_output_bytes=144*(wire+canonical+queries),projected_online_seconds=online,process_and_audit_by_role=s['completed'],predeclared_tail_margin_seconds=tail,staging_engineering_cost=staging,capture_manifest=fid(directory/'manifest.json'),actual_free=resources(cfg,peak))


def request_capture(config,out,request):
    cfg=C.read(config);runtime=validate_cfg(cfg);out=path(out)
    need(out==Path('/root/xoffset-c14-iteration/20261020/v2/validation/r1-screen-resource-capture-v1') and not out.exists(),'one fresh registered finite capture path')
    record=dict(kind='v2-screen-finite-resource-capture',status='pending_independent_parent_admission',candidate='r1-canonical-hotcold-v1',config=dict(path=str(path(config)),identity=fid(config)),config_identity=fid(config),runtime=runtime,output=str(out),queue=str(out/'queue'),roles=['control','candidate'],children=2,child_seconds=20,outer_seconds=120,compile=0,retry=0,timing_inference_samples=0,body_file_limit_bytes=64*1024**2,RSS_limit_bytes=16*GiB,allocated_limit_bytes=4*GiB,free_reserve_bytes=2*GiB,input_policy=cfg['input_policy'],staging_engineering_cost=input_policy(cfg),purpose='Complete bare process+full-body hash/oracle auditing resource feasibility only; no warmup/sample selection/AB difference/confidence or old prefix.')
    write(request,record);return dict(status='finite_request_pending_independent_admission',request_sha256=C.sha(request),compile=0,native=0,queue=0)


def main():
    p=argparse.ArgumentParser(description=__doc__); sub=p.add_subparsers(dest='action',required=True)
    a=sub.add_parser('config')
    for n in ('root','out','output','control-profile','candidate-profile','preconditions'):a.add_argument('--'+n,type=Path,required=True)
    a=sub.add_parser('capture')
    for n in ('config','out','queue','request','review'):a.add_argument('--'+n,type=Path,required=True)
    a.add_argument('--request-sha256',required=True)
    a=sub.add_parser('request-capture')
    for n in ('config','out','request'):a.add_argument('--'+n,type=Path,required=True)
    a=sub.add_parser('freeze')
    for n in ('config','capture','gates','out'):a.add_argument('--'+n,type=Path,required=True)
    a=sub.add_parser('run')
    for n in ('frozen','review','host','out','queue'):a.add_argument('--'+n,type=Path,required=True)
    a.add_argument('--freeze-sha256',required=True)
    for command in ('audit','failure'):
        a=sub.add_parser(command)
        for n in ('out','restoration','result'):a.add_argument('--'+n,type=Path,required=True)
        a.add_argument('--freeze-sha256',required=True)
        if command=='audit':a.add_argument('--postrun-pin',required=True)
        a.add_argument('--audit-queue',type=Path,required=True)
    a=sub.add_parser('pin');a.add_argument('--out',type=Path,required=True)
    args=p.parse_args()
    try:
        if args.action=='config':r=config(args.root,args.out,args.output,args.control_profile,args.candidate_profile,args.preconditions)
        elif args.action=='capture':r=capture(args.config,args.out,args.queue,args.request,args.request_sha256,args.review)
        elif args.action=='request-capture':r=request_capture(args.config,args.out,args.request)
        elif args.action=='freeze':r=freeze(args.config,args.capture,args.gates,args.out)
        elif args.action=='run':r=run(args.frozen,args.freeze_sha256,args.review,args.host,args.out,args.queue)
        elif args.action=='pin':r=dict(postrun_pin=postrun_pin(args.out),authorization=False)
        elif args.action=='failure':r=failure(args.out,args.freeze_sha256,args.restoration,args.result,args.audit_queue)
        else:r=audit(args.out,args.freeze_sha256,args.postrun_pin,args.restoration,args.result,args.audit_queue)
        print(C.encoded(r).decode(),end='');return 0
    except (C.ProtocolError,OSError,ValueError,KeyError) as e:
        print(C.encoded(dict(status='rejected',reason=type(e).__name__+': '+str(e),performance_accepted=False)).decode(),end='');return 1

if __name__=='__main__':raise SystemExit(main())
