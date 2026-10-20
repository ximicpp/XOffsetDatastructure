#!/usr/bin/env python3
"""M1-only capture/freeze/run/audit. Native actions require the original lease.

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
POLICY=dict(candidate='M1',primary='mutate',pairs=72,children=144,AB=36,BA=36,
            warmup=0,retry=0,replacement=0,df=71,primary_family=4,auxiliary_family=36,
            primary_t=2.289930854179058,auxiliary_t=3.327935133019535,
            command_seconds=600,child_seconds=20,audit_seconds=3600,
            allocated_limit_bytes=4*GiB,rss_limit_bytes=16*GiB,free_reserve_bytes=2*GiB,
            seal_seconds=30,nonchild_projection_seconds=60,
            semantic_policy='original full owning output; full native_result then byte-exact oracle dedup',
            threshold='primary_upper < 1 and no auxiliary_lower > 1 and all engineering gates pass',
            cancellation_alpha_recycled=False)
GATES=('codegen','mechanism','targeted','full_normal','full_san','business_SML','v1_bidirectional','consumers','safety')

def need(v,m): C.require(v,m)
def fid(p): return C.file_id(p)
def pin(v): return hashlib.sha256(C.encoded(v)).hexdigest()
def write(p,v): C.write(p,v)
def path(p): return C.absolute(p)

def tools():
    return {str(p):fid(p) for p in (HERE/'m1_screen.py',HERE/'campaign.py',HERE/'core.py',HERE/'protocol.py',RUNNER,Path(sys.executable).resolve())}

def schedule():
    # Alternating pair order is fully deterministic; each ID is used once.
    return [dict(id=f'p{i:03d}-{j+1}-{r}',pair=i,order='AB' if i%2==0 else 'BA',role=r)
            for i in range(72) for j,r in enumerate(('control','candidate') if i%2==0 else ('candidate','control'))]

def checked_file(record):
    need(set(record)=={'path','identity'},'file binding requires path and identity')
    p=path(record['path']); need(fid(p)==record['identity'],'file identity changed: '+str(p)); return p

def source_snapshot(cfg):
    result={}
    material=C.read(checked_file(cfg['materialization']))['production_files']
    need(len(material)==14,'fourteen production files required')
    identity=C.read(checked_file(cfg['candidate_identity']))
    need(identity['candidate']=='m1-default-scalar-v2' and identity['member_count']==154 and len(identity['members'])==154,'M1 v2 full identity required')
    for role in ('control','candidate'):
        root=path(cfg['sources'][role]); members={}
        for n,record in identity['members'].items():
            C.relative(n)
            actual=fid(root/n)
            expected=record['candidate_sha256' if role=='candidate' else 'baseline_sha256']
            need(actual['sha256']==expected,'154-member source differs: '+role+'/'+n)
            members[n]=actual
        production={n:members[n] for n in material}
        if role=='control': need(all(production[n]['sha256']==material[n]['sha256'] for n in material),'control production is not C14')
        actual=path(cfg['actual_sources'][role])
        actual_production={n:fid(actual/n) for n in material}
        need(actual_production==production,'actual compiled production closure differs: '+role)
        result[role]=dict(materialized_root=str(root),actual_compiled_source=str(actual),members=members,production=production,
                          actual_source_manifest=fid(actual/'SOURCE_MANIFEST.json'))
    return result

def bindings(cfg):
    need(set(cfg['binaries'])=={'control','candidate'},'exactly two binaries required')
    binaries={r:fid(checked_file(e['binary'])) for r,e in cfg['binaries'].items()}
    for role,e in cfg['binaries'].items():
        need(e['prefix']==['/usr/bin/taskset','-c','2'],'newly registered M1 CPU2 prefix required')
        need(e['load_contract']=='detached' and e['library'] and e['trust_mode'],'actual adapter identity required')
        need(e['effective_argv'] and '-O3' in e['effective_argv'] and '-DNDEBUG' in e['effective_argv'] and '-UNDEBUG' not in e['effective_argv'],'bare performance -O3 -DNDEBUG required')
        need(not any('sanitize' in a or 'include'==a.lstrip('-') for a in e['effective_argv']),'sanitizer/forced instrumentation disallowed')
        metadata=C.read(checked_file(e['metadata']))
        entry=metadata['binaries']['xoffset']
        need(all(e[k]==entry[k] for k in ('library','commit','trust_mode','load_contract','source_content_sha256')) and e['binary']['path']==entry['path'],'actual profile adapter metadata differs')
        compile_rows=C.read(checked_file(e['compile_commands']))
        matches=[row for row in compile_rows if 'CMakeFiles/sc_xoffset.dir/' in row.get('command','')+' '+row.get('output','')]
        need(len(matches)==1 and (matches[0].get('arguments') or shlex.split(matches[0]['command']))==e['effective_argv'],'actual compile argv differs')
        adapter=checked_file(e['adapter']).read_text()
        for key in ('library','commit','trust_mode','source_content_sha256'):
            literal='library_id' if key=='library' else key
            found=re.search(r'\b'+literal+r'\s*=\s*"([^"\n]+)"',adapter)
            need(found is not None and found.group(1)==e[key],'actual adapter literal differs: '+role+'/'+key)
    required={'compiler','wrapper','libcxx','runtime','driver','adapter','dependencies','helper','request'}
    need(required<=set(cfg['closure']),'compiler/runtime/driver/helper/dependency closure missing')
    closure={n:C.tree_id(path(p)) for n,p in cfg['closure'].items()}
    fixture=path(cfg['fixture']); G.fixture_check(fixture,16384)
    return dict(sources=source_snapshot(cfg),binaries=binaries,closure=closure,tools=tools(),
                metadata={r:fid(checked_file(e['metadata'])) for r,e in cfg['binaries'].items()},
                fixture=C.inventory(fixture,exclude=()),input_wire=fid(checked_file(cfg['input_wire'])),environment=cfg['environment'])

def config(root,out,output,control_profile,candidate_profile):
    """Bind concrete current profile files; no executable is launched."""
    root=path(root);doc=root/'docs/coordination/c14-iteration-20261019-v1'
    profiles={r:C.read(p) for r,p in (('control',control_profile),('candidate',candidate_profile))}
    need(profiles['control']['environment']==profiles['candidate']['environment'],'control/candidate environment differs')
    closure={};entries={};sources={};actual_sources={}
    for role,profile in profiles.items():
        need(profile['lock_dir']==str(LOCK) and profile['platform_class']=='wsl2-vm-x86_64','actual profile lease/platform differs')
        source=path(profile['closure']['candidate-source-tree']);actual_sources[role]=str(source);sources[role]=str(root if role=='control' else source)
        compile_file=path(profile['closure']['compile-commands'])
        rows=[r for r in C.read(compile_file) if 'CMakeFiles/sc_xoffset.dir/' in r.get('command','')+' '+r.get('output','')]
        need(len(rows)==1,'single actual sc_xoffset compilation required')
        adapter=path(profile['closure'].get('candidate-adapter',str(source/'experiments/serialization_comparison/adapters/xoffset/adapter.hpp')))
        entry=profile['binaries']['xoffset'];entries[role]={k:entry[k] for k in ('library','commit','trust_mode','load_contract','source_content_sha256')}
        entries[role].update(binary=dict(path=entry['path'],identity=fid(entry['path'])),prefix=[],
            metadata=dict(path=str(control_profile if role=='control' else candidate_profile),identity=fid(control_profile if role=='control' else candidate_profile)),
            compile_commands=dict(path=str(compile_file),identity=fid(compile_file)),adapter=dict(path=str(adapter),identity=fid(adapter)),
            effective_argv=rows[0].get('arguments') or shlex.split(rows[0]['command']))
        # Bind actual dependency and runtime files, not large unused vendor trees.
        for label,p in profile['closure'].items():
            if Path(p).is_file():closure[role+'-'+label]=str(path(p))
        closure[role+'-profile']=str(control_profile if role=='control' else candidate_profile)
        closure[role+'-adapter']=str(adapter)
        entries[role]['prefix']=['/usr/bin/taskset','-c','2']
    need(entries['control']['library']=='xoffset-receive-completion-v1' and entries['candidate']['library']=='xoffset-c14-m1-default-scalar-v2','wrong actual M1/C14 labels')
    cc=entries['candidate']['effective_argv'][0]
    closure.update(compiler='/opt/p2996-toolchain/bin/clang++',wrapper=cc,
        libcxx='/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu/libc++.so.1',
        runtime='/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu/libc++abi.so.1',
        driver=str(path(profiles['candidate']['closure']['compile-commands']).parent.parent/'metadata/experiments/serialization_comparison/driver.cpp'),
        adapter=entries['candidate']['adapter']['path'],dependencies=str(path(candidate_profile).parent/'compiled-source-binding-normal-v2.json'),
        helper=str(path(sources['candidate'])/'xoffset_core_lifetime.hpp'),request=str(doc/'core/REQUESTS.json'),
        taskset='/usr/bin/taskset',host_loader='/lib64/ld-linux-x86-64.so.2',host_libc='/lib/x86_64-linux-gnu/libc.so.6')
    cfg=dict(schema='m1-screen-config-v1',attempt_id='M1-screen-20261019-v1',output=str(path(output)),backing_mount='/mnt/c',
        environment=profiles['candidate']['environment'],sources=sources,actual_sources=actual_sources,binaries=entries,closure=closure,
        materialization=dict(path=str(root/'handoff/github-20261019/C14_MATERIALIZATION.json'),identity=fid(root/'handoff/github-20261019/C14_MATERIALIZATION.json')),
        candidate_identity=dict(path=str(doc/'core/M1_IDENTITY.json'),identity=fid(doc/'core/M1_IDENTITY.json')),
        fixture=str(root/'handoff/continuation-20261019-v1/portable-inputs/fixed-M'),
        input_wire=dict(path=str(root/'handoff/continuation-20261019-v1/portable-inputs/old-V8-initial-wire.bin'),identity=fid(root/'handoff/continuation-20261019-v1/portable-inputs/old-V8-initial-wire.bin')),
        affinity_policy='new 2026-10-19 current-machine registration: taskset CPU2 explicitly authorized after allowed0..31/online verification; recheck before native')
    validate_cfg(cfg);write(out,cfg)
    return dict(status='config_bound_not_admitted',config_identity=fid(out),timing_samples=0)

def validate_cfg(cfg):
    need(cfg['schema']=='m1-screen-config-v1','wrong config schema')
    need(cfg['attempt_id']=='M1-screen-20261019-v1','fixed one-attempt ID required')
    need(path(cfg['output']).is_relative_to(Path('/root/xoffset-c14-iteration/20261019/v1/validation')),'screen output outside authorized WSL domain')
    need(cfg['environment'] and all(type(k)==str and type(v)==str for k,v in cfg['environment'].items()),'explicit environment required')
    need(cfg['backing_mount']=='/mnt/c','C backing budget required')
    need(path(cfg['fixture']).name=='fixed-M','original fixed-M oracle required')
    need(Path(cfg['input_wire']['path']).name=='old-V8-initial-wire.bin','original old-V8 input required')
    return bindings(cfg)

def active(queue,kind,maximum):
    need(platform.system()=='Linux' and 'microsoft' in platform.release().lower(),'WSL2 only')
    queue=path(queue); q=C.read(queue/'request.json')
    argv=[str(Path(__file__).resolve()),*sys.argv[1:]]
    need(q.get('command') in ([str(Path(sys.executable).absolute()),*argv],[str(Path(sys.executable).absolute()),'-B',*argv],[str(Path(sys.executable).resolve()),*argv],[str(Path(sys.executable).resolve()),'-B',*argv]),'must be exact direct queue command')
    need(q.get('owner')=='W3' and q.get('kind')==kind and q.get('runner_sha256')==C.sha(RUNNER) and q.get('lock_dir')==str(LOCK) and q.get('out')==str(queue) and q.get('cwd')==str(Path.cwd()),'original global queue request mismatch')
    need(q.get('timeout_seconds',q.get('max_seconds'))==maximum,'outer timeout differs from fixed cap')
    fd=os.open(LOCK/'queue.lock',os.O_RDONLY)
    try:
        try: fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError: pass
        else:
            fcntl.flock(fd,fcntl.LOCK_UN); raise C.ProtocolError('original global lease not held')
        record=C.parse(os.read(fd,65536))
    finally: os.close(fd)
    need(record.get('status')=='running' and record.get('supervisor_pid')==os.getppid() and record.get('run_id')==q['run_id'] and record.get('owner')=='W3','not direct leased worker')
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
    roots=(path(out),path(f['capture']),path(f['frozen_path']).parent)
    return sum(allocated(p)['unique_allocated_bytes'] for p in roots)

def resources(cfg,peak):
    guest=shutil.disk_usage(path(cfg['output']).parent).free
    backing=shutil.disk_usage('/mnt/c').free
    need(guest>=peak+2*GiB and backing>=peak+2*GiB,'guest/C backing cannot cover peak plus 2GiB reserve')
    return dict(guest_free_bytes=guest,C_backing_free_bytes=backing,required_peak_bytes=peak,reserve_bytes=2*GiB)

def capture(config,out,queue):
    cfg=C.read(config); before=validate_cfg(cfg); out=path(out)
    need(queue==out/'queue' and set(p.name for p in out.iterdir())=={'queue'},'capture prefix exists')
    lease=active(queue,'functional',120); write(out/'lease.json',lease)
    need(2 in os.sched_getaffinity(0),'registered CPU2 no longer allowed')
    write(out/'config.json',cfg); write(out/'runtime-before.json',before)
    (out/'commands').mkdir(); completed=[]; error=None; started=time.monotonic()
    try:
        for role in ('control','candidate'):
            prefix=out/'commands'/role; t=time.monotonic()
            receipt=G.execute(argv(cfg,role,prefix),prefix,20,cfg['environment'],64*1024**2)
            row,values=native(cfg,role,prefix,receipt)
            need(byte_equal(prefix.with_suffix('.canonical'),path(cfg['fixture'])/'final.canonical.bin') and byte_equal(prefix.with_suffix('.queries'),path(cfg['fixture'])/'queries.bin'),'capture byte oracle mismatch')
            completed.append(dict(role=role,receipt=fid(prefix.with_suffix('.receipt.json')),wire_bytes=prefix.with_suffix('.wire').stat().st_size,
                                  canonical_bytes=prefix.with_suffix('.canonical').stat().st_size,queries_bytes=prefix.with_suffix('.queries').stat().st_size,
                                  full_process_seconds=(int(receipt['finished_monotonic_ns'])-int(receipt['started_monotonic_ns']))/1e9,
                                  online_child_audit_seconds=time.monotonic()-t,values=values))
    except (C.ProtocolError,OSError,ValueError) as e: error=str(e)
    try: after=bindings(cfg)
    except (C.ProtocolError,OSError,ValueError) as e: after={'identity_error':str(e)}
    write(out/'runtime-after.json',after)
    if before!=after:error='before/after identity mismatch'
    C.seal(out,dict(schema='m1-functional-capture-v1',status='passed' if error is None else 'failed',error=error,
                    completed=completed,elapsed_seconds=time.monotonic()-started,timing_inference_samples=0,warmup=False))
    need(error is None,'capture failed: '+str(error))
    return C.read(out/'status.json')

def projection(cfg,capture_dir):
    C.check_inventory(capture_dir); s=C.read(capture_dir/'status.json')
    need(s['schema']=='m1-functional-capture-v1' and s['status']=='passed' and [r['role'] for r in s['completed']]==['control','candidate'],'two-role successful functional capture required')
    need(C.read(capture_dir/'config.json')==cfg,'capture config differs')
    q,r=queue_audit(capture_dir/'queue','functional',120,C.read(capture_dir/'lease.json')['request']['command'])
    need(r['status']=='complete' and r['command_returncode']==0 and r['command_elapsed_seconds']<=120,'capture outer queue failed')
    for item in s['completed']:
        role=item['role'];prefix=capture_dir/'commands'/role;receipt=C.read(prefix.with_suffix('.receipt.json'))
        need(receipt['argv']==argv(cfg,role,prefix) and receipt['execution_environment']==cfg['environment'] and receipt['returncode']==0 and not receipt['timed_out'] and receipt['timeout_seconds']==20,'capture actual receipt differs')
        need(prefix.with_suffix('.stderr').stat().st_size==0 and fid(prefix.with_suffix('.receipt.json'))==item['receipt'],'capture receipt/stderr invalid')
        _,values=native(cfg,role,prefix,receipt,item['wire_bytes'])
        elapsed=(int(receipt['finished_monotonic_ns'])-int(receipt['started_monotonic_ns']))/1e9
        need(values==item['values'] and item['full_process_seconds']==elapsed and item['online_child_audit_seconds']>=elapsed,'capture raw times/values differ')
        need(item['canonical_bytes']==prefix.with_suffix('.canonical').stat().st_size and item['queries_bytes']==prefix.with_suffix('.queries').stat().st_size,'capture actual sizes differ')
    wire=max(r['wire_bytes'] for r in s['completed']); canonical=max(r['canonical_bytes'] for r in s['completed']); queries=max(r['queries_bytes'] for r in s['completed'])
    limit=max(wire,canonical,queries,1024**2)
    # Assume every wire is distinct. Only byte-exact oracle canonical/query dedup.
    # Seven file-size ceilings bound one child including stdout/stderr/receipt.
    metadata=64*1024**2
    retained=allocated(capture_dir)['unique_allocated_bytes']
    peak=144*((wire+4095)//4096*4096)+canonical+queries+7*limit+metadata+retained
    online=144*max(r['online_child_audit_seconds'] for r in s['completed'])+POLICY['nonchild_projection_seconds']
    need(peak<=4*GiB,'predicted physical allocation exceeds 4GiB')
    need(online<=600-POLICY['seal_seconds'],'complete process/audit time projection cannot fit 600s and seal reserve')
    return dict(wire_max_bytes=wire,wire_bytes_by_role={r['role']:r['wire_bytes'] for r in s['completed']},canonical_bytes=canonical,queries_bytes=queries,file_limit_bytes=limit,
                all_wire_unique=True,metadata_reserve_bytes=metadata,retained_capture_allocated_bytes=retained,temporary_slots=7,physical_peak_bytes=peak,
                logical_output_bytes=144*(wire+canonical+queries),projected_online_seconds=online,
                capture_manifest=fid(capture_dir/'manifest.json'),actual_free=resources(cfg,peak))

def freeze(config,capture_dir,gates,out):
    cfg=C.read(config); before=validate_cfg(cfg); capture_dir=path(capture_dir)
    need(before==C.read(capture_dir/'runtime-before.json')==C.read(capture_dir/'runtime-after.json'),'capture identity does not match freeze')
    gate=C.read(gates); need(gate['status']=='passed' and gate['candidate_identity_sha256']==cfg['candidate_identity']['identity']['sha256'],'same-candidate gate admission required')
    need(set(gate['gates'])==set(GATES),'all M1 gates required')
    gate_records={n:dict(path=str(checked_file(r)),identity=r['identity']) for n,r in gate['gates'].items()}
    need(not path(cfg['output']).exists(),'attempt output already exists')
    out=C.fresh(out)
    record=dict(schema='m1-screen-freeze-v1',frozen_path=str(out/'freeze.json'),config=cfg,config_identity=fid(config),policy=POLICY,schedule=schedule(),
                runtime=before,gates=gate_records,gate_admission=dict(path=str(path(gates)),identity=fid(gates)),
                capture=str(capture_dir),projection=projection(cfg,capture_dir),performance_accepted=False)
    need(before==bindings(cfg),'identity changed during freeze')
    write(out/'freeze.json',record)
    return dict(status='frozen_pending_independent_review',freeze_sha256=C.sha(out/'freeze.json'),output=cfg['output'])

def check_freeze(frozen,external):
    need(C.hexsha(external) and C.sha(frozen)==external,'external freeze pin mismatch')
    f=C.read(frozen);need(f['schema']=='m1-screen-freeze-v1' and f['policy']==POLICY and f['schedule']==schedule(),'fixed freeze policy/schedule mismatch')
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
    need(2 in os.sched_getaffinity(0),'registered CPU2 no longer allowed')
    for p,kind in ((review,'independent-screen-admission'),(host,'host-quiet-admission')):
        a=C.read(p);need(a['kind']==kind and a['status']=='passed' and a['freeze_sha256']==external,'independent/host quiet admission not bound to freeze')
    h=C.read(host);need(h['quiet'] is True and h['original_global_lease'] is True and h['restoration_ready'] is True,'host quiet/restoration not ready')
    need(C.decimal(h['ready_monotonic_ns'],True)<=time.monotonic_ns(),'host ready timestamp is in future')
    C.copy_file(frozen,out/'freeze.json'); write(out/'freeze-pin.json',{'sha256':external});C.copy_file(review,out/'review.json');C.copy_file(host,out/'host-admission.json');write(out/'lease.json',lease)
    write(out/'execution-context.json',dict(frozen=str(path(frozen)),review=dict(path=str(path(review)),identity=fid(review)),host=dict(path=str(path(host)),identity=fid(host))))
    write(out/'runtime-before.json',bindings(cfg)); need(C.read(out/'runtime-before.json')==f['runtime'],'runtime differs from freeze')
    for r in f['gates'].values():checked_file(r)
    checked_file(f['gate_admission']);resources(cfg,f['projection']['physical_peak_bytes'])
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
    C.seal(out,dict(schema='m1-screen-run-v1',status='complete' if error is None else 'failed',error=error,
                    attempted=attempted,completed=completed,unexecuted=schedule()[len(attempted):],elapsed_seconds=elapsed,
                    performance_accepted=False,user_restoration_required_before_audit=True))
    need(error is None,'screen closed: '+str(error))
    return dict(status='complete_pending_restoration_and_independent_audit',timing_children=len(completed))

def summary(samples):
    need(len(samples)==144,'exactly 144 children required')
    pairs={i:{} for i in range(72)}
    for s in samples:
        job=s['job'];need(job['role'] not in pairs[job['pair']],'duplicate role in pair');pairs[job['pair']][job['role']]=s['values']
    result={}; undefined=[]
    for metric in METRICS:
        if any(p['candidate'][metric]<=0 or p['control'][metric]<=0 for p in pairs.values()):
            result[metric]=dict(n=72,df=71,status='undefined_log_contrast_zero_preserved');undefined.append(metric);continue
        xs=[math.log(p['candidate'][metric]/p['control'][metric]) for p in pairs.values()]
        mean=statistics.mean(xs);se=statistics.stdev(xs)/math.sqrt(72)
        t=POLICY['primary_t'] if metric=='mutate' else POLICY['auxiliary_t']
        result[metric]=dict(n=72,df=71,mean_log=mean,se_log=se,point=math.exp(mean),upper=math.exp(mean+t*se))
        if metric!='mutate':result[metric]['lower']=math.exp(mean-t*se)
    return dict(metrics=result,undefined_metrics=undefined,primary_improvement=not undefined and result['mutate']['upper']<1,
                confirmed_auxiliary_regression=[m for m in METRICS if m!='mutate' and m not in undefined and result[m]['lower']>1],
                performance_accepted=False,QF_admitted=False)

def postrun_pin(out):
    return hashlib.sha256(b'M1-SCREEN-POSTRUN-v1\0'+bytes.fromhex(C.sha(Path(out)/'manifest.json'))+bytes.fromhex(C.sha(Path(out)/'queue/manifest.json'))).hexdigest()

def audit(out,external,postrun,restoration,result,audit_queue):
    began=time.monotonic();out=path(out);f=check_freeze(out/'freeze.json',external);cfg=f['config']
    audit_lease=active(audit_queue,'validation',3600)
    restored=C.read(restoration);need(restored['status']=='passed' and restored['freeze_sha256']==external and restored['user_processes_restored'] is True,'user restoration must precede offline audit')
    need(C.hexsha(postrun) and postrun_pin(out)==postrun,'external complete postrun pin differs')
    C.check_inventory(out);status=C.read(out/'status.json');need(status['schema']=='m1-screen-run-v1','wrong run bundle')
    need(C.inventory(path(cfg['fixture']),exclude=())==f['runtime']['fixture'] and fid(path(cfg['input_wire']['path']))==f['runtime']['input_wire'],'actual oracle/input differs from frozen')
    need(status['attempted']==schedule()[:len(status['attempted'])] and status['unexecuted']==schedule()[len(status['attempted']):],'not exact attempt prefix')
    need(C.read(out/'runtime-before.json')==f['runtime'],'before identity differs')
    need(status['status']=='failed' or C.read(out/'runtime-after.json')==f['runtime'],'successful run identity differs')
    q,qr=queue_audit(out/'queue','timing',600,C.read(out/'lease.json')['request']['command'])
    context=C.read(out/'execution-context.json');command=q['command'];need(len(command)>=3,'short original command')
    requested=command[0];tail=command[1:]
    if tail[0]=='-B':tail=tail[1:]
    need(tail==[str(HERE/'m1_screen.py'),'run','--frozen',context['frozen'],'--freeze-sha256',external,'--review',context['review']['path'],'--host',context['host']['path'],'--out',str(out),'--queue',str(out/'queue')],'offline exact M1 command differs; use documented argv order')
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
    need(restored['power_before']==restored['power_after']=='381b4222-f694-41f0-9685-ff5bb260df2e','Balanced power preservation evidence differs')
    complete=status['status']=='complete'
    if complete:
        need(len(samples)==144 and len(status['attempted'])==144 and status['elapsed_seconds']<=600,'incomplete successful attempt')
        need(qr['status']=='complete' and qr['command_returncode']==0 and qr['command_elapsed_seconds']<=600,'outer queue failed or exceeded cap')
    need(screen_allocated(out,f)<=4*GiB,'physical allocation cap exceeded')
    report=dict(schema='m1-screen-audit-v1',status='passed' if complete else 'preserved_failure',freeze_sha256=external,
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
    report=dict(schema='m1-screen-unsealed-failure-v1',status='failed_preserved',freeze_sha256=external,
        window_closed=True,retry=0,replacement=0,queue_manifest=fid(out/'queue/manifest.json'),queue_result=r,
        original_prefix_files=inventory,original_prefix_pin=pin(inventory),restoration=fid(restoration),
        audit_lease=lease,performance_accepted=False,QF_admitted=False)
    need(time.monotonic()-began<3600,'failure audit exceeded cap');write(result,report);return report

def main():
    p=argparse.ArgumentParser(description=__doc__); sub=p.add_subparsers(dest='action',required=True)
    a=sub.add_parser('config')
    for n in ('root','out','output','control-profile','candidate-profile'):a.add_argument('--'+n,type=Path,required=True)
    a=sub.add_parser('capture')
    for n in ('config','out','queue'):a.add_argument('--'+n,type=Path,required=True)
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
        if args.action=='config':r=config(args.root,args.out,args.output,args.control_profile,args.candidate_profile)
        elif args.action=='capture':r=capture(args.config,args.out,args.queue)
        elif args.action=='freeze':r=freeze(args.config,args.capture,args.gates,args.out)
        elif args.action=='run':r=run(args.frozen,args.freeze_sha256,args.review,args.host,args.out,args.queue)
        elif args.action=='pin':r=dict(postrun_pin=postrun_pin(args.out),authorization=False)
        elif args.action=='failure':r=failure(args.out,args.freeze_sha256,args.restoration,args.result,args.audit_queue)
        else:r=audit(args.out,args.freeze_sha256,args.postrun_pin,args.restoration,args.result,args.audit_queue)
        print(C.encoded(r).decode(),end='');return 0
    except (C.ProtocolError,OSError,ValueError,KeyError) as e:
        print(C.encoded(dict(status='rejected',reason=type(e).__name__+': '+str(e),performance_accepted=False)).decode(),end='');return 1

if __name__=='__main__':raise SystemExit(main())
