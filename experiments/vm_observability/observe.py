#!/usr/bin/env python3
"""F diagnostic generation, gated builds/semantic collection, portable offline audit."""
from __future__ import annotations
import argparse, datetime, hashlib, json, os, platform, shutil, shlex, subprocess, sys
from pathlib import Path, PurePosixPath
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.dont_write_bytecode=True
sys.path.insert(0,str(HERE))
from generate import BASELINE, generate
from audit_events import validate_trace
BOOST=('container','core','config','throw_exception','static_assert','container_hash','type_traits','winapi','intrusive','predef')
CASES=('owner_basic','owner_transfer','core_metadata','copy_wire','pool_reclaim','threads','failure_registry','failure_metadata','failure_vm','recorder_overflow')

def require(ok,why):
    if not ok: raise RuntimeError(why)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def now():return datetime.datetime.now(datetime.timezone.utc).isoformat()
def write(p,obj):Path(p).write_text(json.dumps(obj,indent=2,sort_keys=True)+'\n')
def fresh(p):p=Path(p).resolve();p.mkdir(parents=True,exist_ok=False);return p
def manifest(p):
    result={}
    for f in sorted(p.rglob('*')):
        require(not f.is_symlink(),'symlink in evidence')
        if f.is_file() and f.name not in {'result.json','COMPLETE'}:result[f.relative_to(p).as_posix()]=sha(f)
    return result

def finish(p,d):
    d.update(status='passed',finished_at=now(),timing_evidence=False,files=manifest(p));write(p/'result.json',d);(p/'COMPLETE').write_text(sha(p/'result.json')+'\n')

def audit_files(p):
    d=json.loads((p/'result.json').read_text());require(d.get('schema')=='n3-artifact-v1' and d.get('status')=='passed' and d.get('timing_evidence') is False,'failed or timing record')
    require((p/'COMPLETE').read_text().strip()==sha(p/'result.json'),'invalid completion identity')
    for name,digest in d['files'].items():
        rel=PurePosixPath(name);require(not rel.is_absolute() and '..' not in rel.parts,'unsafe inventory path');require(sha(p/name)==digest,'changed file: '+name)
    require(manifest(p)==d['files'],'inventory changed');return d

def active_job(kind):
    lock=ROOT/'build/vm-coordination/queue.lock'
    record=json.loads(lock.read_text())
    require(record.get('status')=='running' and record.get('owner') in {'W2','W3'},'must run inside active W4 queue job')
    require(record.get('kind') in kind and record.get('supervisor_pid')==os.getppid(),'queue kind/parent mismatch')
    request=json.loads((Path(record['out'])/'request.json').read_text())
    require(Path(request['command'][1]).resolve()==Path(__file__).resolve(),'queue command is not this runner')
    return {'run_id':record['run_id'],'owner':record['owner'],'kind':record['kind'],'request':request}

def prepare(a):
    p=fresh(a.out);source=a.source.resolve();generated,anchors=generate(source)
    for folder in ('original','diagnostic'):(p/folder).mkdir()
    for name in BASELINE:
        shutil.copy2(source/name,p/'original'/name);(p/'diagnostic'/name).write_text(generated[name])
    for name in ('n3_runtime.hpp','n3_probe.cpp','generate.py','observe.py','audit_events.py','contract.json','coverage.json','event_kinds.json'):
        shutil.copy2(HERE/name,p/name)
    for folder in ('original','diagnostic'):
        shutil.copy2(HERE/'n3_runtime.hpp',p/folder/'n3_runtime.hpp')
    shutil.copy2(source/'tools/run_vm_automatic_trace.py',p/'n5_generator.py')
    (p/'original/tools').mkdir();shutil.copy2(source/'tools/run_vm_automatic_trace.py',p/'original/tools/run_vm_automatic_trace.py')
    write(p/'anchors.json',anchors)
    finish(p,{'schema':'n3-artifact-v1','kind':'prepared','created_at':now(),'baseline_headers':BASELINE,'anchors':len(anchors),'source_root':str(source)})
    print(json.dumps({'status':'passed','kind':'prepared','out':str(p),'anchors':len(anchors)}))

def call(cmd,p,name,timeout=240):
    r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=timeout)
    (p/(name+'.stdout')).write_text(r.stdout);(p/(name+'.stderr')).write_text(r.stderr)
    require(r.returncode==0,'command failed '+name+': '+r.stderr[:1000])
    return {'argv':list(map(str,cmd)),'returncode':r.returncode}

def build(a):
    job=active_job({'build'});prep=a.prepared.resolve();parent=audit(prep);require(parent['kind']=='prepared','not prepared')
    require(sha(prep/'observe.py')==sha(__file__),'runner differs from prepared source')
    p=fresh(a.out);shutil.copytree(prep,p/'input');external={str(a.compiler.resolve()):sha(a.compiler)}
    cmd=[str(a.compiler),'-O2','-std=c++26','-freflection','-fexpansion-statements','-stdlib=libc++','-pthread']
    if platform.system()=='Darwin':cmd+=['-arch',platform.machine(),'-isysroot',str(a.sdk),'-mmacosx-version-min=10.15']
    for lib in BOOST:cmd+=['-isystem',str(a.deps/'boost/libs'/lib/'include')]
    cmd+=['-isystem',str(a.deps/'typelayout/include'),'-L'+str(a.library),'-Wl,-rpath,'+str(a.library)]
    deps={}
    for dep in ('boost','typelayout'):
        r=subprocess.run(['git','-C',str(a.deps/dep),'rev-parse','HEAD'],capture_output=True,text=True,check=True);deps[dep]=r.stdout.strip()
    for name in ('libc++.1.0.dylib','libc++abi.1.0.dylib'):
        file=a.library/name
        if file.exists():external[str(file.resolve())]=sha(file)
    commands=[]
    for folder,flag in [('original',0),('diagnostic',1)]:
        depfile=p/('dependencies-'+folder+'.d')
        argv=cmd+['-MD','-MF',str(depfile),f'-DN3_DIAGNOSTIC={flag}','-I',str(p/'input'/folder),str(p/'input/n3_probe.cpp'),'-o',str(p/('n3_'+folder))]
        commands.append(call(argv,p,'compile-'+folder))
        dependency_text=depfile.read_text().replace('\\\n',' ')
        for name in shlex.split(dependency_text.split(':',1)[1]):
            path=Path(name).resolve()
            if not path.is_relative_to(p):external[str(path)]=sha(path)
    version=subprocess.run([a.compiler,'--version'],capture_output=True,text=True,check=True).stdout
    finish(p,{'schema':'n3-artifact-v1','kind':'build','created_at':now(),'parent_sha256':sha(prep/'result.json'),'queue_job':job,'commands':commands,'compiler_version':version,'external_files':external,'dependencies':deps,'baseline_headers':BASELINE})
    print(json.dumps({'status':'passed','kind':'build','out':str(p)}))

def check_case(report, event_kinds_path=None):
    require(set(report)=={'schema','case','status','semantic','trace'} and report['schema']=='n3-probe-v1','invalid probe schema')
    name=report['case'];require(name in CASES,'unknown case');sem=report['semantic']
    require(type(sem) is dict and all(type(v) is int and v>=0 for v in sem.values()),'invalid semantics')
    expected={
      'owner_basic':{'owners':1,'lookup_calls':2,'checksum':77},
      'owner_transfer':{'checksum':38,'owners':2},
      'core_metadata':{'owners':2,'core_new':11,'core_returns':12,'donor_user_bytes':32,'checksum':11},
      'copy_wire':{'checksum':88,'relocated_bytes':16,'saved_frames':1,'payload_loads':2},
      'pool_reclaim':{'checksum':19,'hits':1,'misses':1},
      'threads':{'checksum':11,'worker_threads':2,'owners':2},
      'failure_registry':{'checksum':82},'failure_metadata':{'checksum':32},'failure_vm':{'checksum':31},'recorder_overflow':{'checksum':17000}}
    extra_keys={'copy_wire':{'wire_bytes','payload_bytes'},'pool_reclaim':{'page_bytes'}}.get(name,set())
    require(set(sem)==set(expected[name])|extra_keys and all(sem[k]==v for k,v in expected[name].items()),'semantic case constants differ')
    if name=='copy_wire':require(sem['payload_bytes']>0 and sem['wire_bytes']>sem['payload_bytes'],'wire extents invalid')
    if name=='pool_reclaim':require(sem['page_bytes']>0 and sem['page_bytes']<65536,'test requires page smaller than 65536')
    trace=report['trace']
    if name=='recorder_overflow':
        require(report['status']=='passed_control' and trace['valid'] is False and trace['dropped']>0,'overflow not detected')
        rejected=False
        try:validate_trace(trace, event_kinds_path=event_kinds_path)
        except ValueError:rejected=True
        require(rejected,'invalid recorder accepted');return {'control':'invalid_record_rejected'}
    require(report['status']=='passed','case failed');summary=validate_trace(trace, event_kinds_path=event_kinds_path)
    def events(kind):return [x for x in trace['events'] if x['kind']==kind]
    def count(kind,n):require(len(events(kind))==n,f'{name}: {kind} count != {n}')
    def total(kind,n):require(sum(x['bytes'] for x in events(kind))==n,f'{name}: {kind} bytes != {n}')
    require(not summary.get('registry',{}).get('active',{}),'owner registry leaked')
    require(all(item['domain']==2 for item in summary['allocations']['outstanding']),'fixed probe leaked non-registry host requests')
    if name=='owner_basic':
        for k,n in [('registry_register',1),('registry_erase',1),('registry_lookup_hit',2),('core_new',1),('core_return',1),('core_scan',0)]:count(k,n)
        total('copy_bytes_string_extent',4096)
    elif name=='owner_transfer':
        count('registry_register',2);count('registry_erase',2);count('registry_rebind',4);count('registry_lookup_miss',1);count('copy_bytes_string_extent',0)
        empty=[e for e in events('core_metadata_capacity') if e['owner']==0]
        require(len(empty)==4 and all(e['bytes']==0 for e in empty),'moved-from owner inherited outer Scope')
        second=events('registry_register')[1]['sequence']
        require(not [e for e in events('host_allocate') if e['value']==2 and e['sequence']>second],'transfer allocated registry metadata')
    elif name=='core_metadata':
        for k,n in [('registry_register',2),('registry_erase',2),('core_new',11),('core_return',12),('core_reuse',1),('core_scan',1)]:count(k,n)
        total('core_erase_movement_estimate',7*16)
        alloc=[e for e in events('host_allocate') if e['value']==1]
        require(len(alloc)==7 and len({e['owner'] for e in alloc})==2,'two-owner metadata allocations')
        require(sum(e['bytes'] for e in alloc)==(1+2+4+8+1+2+4)*16,'metadata requested allocation byte total')
    elif name=='copy_wire':
        total('save_payload_copy',sem['payload_bytes']);total('load_payload_copy',2*sem['payload_bytes'])
        total('vector_relocated_element',16);total('string_relocate_copy',3)
        count('save_header_copy',1);count('load_header_copy',3)
        require(all(e['owner']!=0 for e in events('load_payload_copy')+events('save_payload_copy')),'copy owner missing')
    elif name=='threads':
        count('registry_register',2);count('registry_erase',2);count('core_new',2);count('core_reuse',2)
        require(len({e['thread'] for e in events('registry_register')})==2,'thread attribution lost')
        require(len({e['owner'] for e in events('core_new')})==2,'owner attribution lost')
    elif name=='failure_registry':count('registry_register_failed',2);count('host_allocate_failed',1)
    elif name=='failure_metadata':count('core_return_failed',1);count('core_return',1);count('host_allocate_failed',1)
    elif name=='pool_reclaim':
        count('pool_scan',1);require(len(events('pool_lock'))>=5,'pool lock missing');require(len(events('os_unmap'))>=2,'unmap missing')
    elif name=='failure_vm':
        count('vm_injected_failure',2);require([e['value'] for e in events('vm_injected_failure')]==[4,8],'wrong injected failure reasons')
        require(len(events('os_replace'))+len(events('os_reclaim'))==1,'actual reclaim retry missing')
        require(not events('os_reclaim_failed'),'injected rejection is not an actual failed syscall')
        total('grow_zero',65536)
    return summary

def collect(a):
    job=active_job({'diagnostic','functional','validation'});b=a.build.resolve();parent=audit(b);require(parent['kind']=='build','not build')
    require(sha(b/'input/observe.py')==sha(__file__),'runner differs from frozen build')
    for name in ('audit_events.py','event_kinds.json','contract.json'):
        require(sha(b/'input'/name)==sha(HERE/name),'collector import/contract differs from frozen build: '+name)
    for file,digest in parent['external_files'].items():require(sha(file)==digest,'external compiler/runtime changed')
    p=fresh(a.out);(p/'tool').mkdir();
    for name in ('observe.py','generate.py','audit_events.py','event_kinds.json'):shutil.copy2(b/'input'/name,p/'tool'/name)
    shutil.copy2(b/'result.json',p/'build-result.json');shutil.copy2(b/'input/contract.json',p/'contract.json');shutil.copy2(b/'input/event_kinds.json',p/'event_kinds.json');commands=[];summaries={}
    for case in CASES:
        for profile in ('original','diagnostic'):
            name=case+'-'+profile;commands.append(call([b/('n3_'+profile),'--case',case],p,name,timeout=30))
        original=json.loads((p/(case+'-original.stdout')).read_text());report=json.loads((p/(case+'-diagnostic.stdout')).read_text())
        require(original['trace'] is None and original['case']==case and report['case']==case,'profile/case mismatch')
        require(original['semantic']==report['semantic'],'original/diagnostic semantic difference')
        summaries[case]=check_case(report,p/'event_kinds.json')
    write(p/'summaries.json',summaries)
    finish(p,{'schema':'n3-artifact-v1','kind':'collection','created_at':now(),'queue_job':job,'commands':commands,'cases':list(CASES),'executed_runner_sha256':sha(__file__),'build_result_sha256':sha(b/'result.json'),'binary_hashes':{name:sha(b/name) for name in ('n3_original','n3_diagnostic')},'semantic_parity_scope':'metadata failure injection and overflow are diagnostic controls; parity there compares checksum only'})
    audit(p);print(json.dumps({'status':'passed','kind':'collection','out':str(p),'cases':len(CASES)}))

def audit(p):
    d=audit_files(p)
    if d['kind']=='prepared':
        require(d['baseline_headers']==BASELINE,'F declaration differs')
        for name,digest in BASELINE.items():require(sha(p/'original'/name)==digest,'original F changed')
        require(sha(p/'generate.py')==sha(HERE/'generate.py'),'generator revision differs; use frozen observer')
        generated,anchors=generate(p/'original')
        require(anchors==json.loads((p/'anchors.json').read_text()),'anchor evidence differs')
        for name,text in generated.items():require((p/'diagnostic'/name).read_text()==text,'generated diagnostic header differs')
        require(all(sha(p/folder/'n3_runtime.hpp')==sha(p/'n3_runtime.hpp') for folder in ('original','diagnostic')),'runtime header differs')
    elif d['kind']=='build':
        audit(p/'input');require(len(d['commands'])==2 and all(x['returncode']==0 for x in d['commands']),'build commands missing')
        require(d['parent_sha256']==sha(p/'input/result.json'),'prepared identity differs')
    elif d['kind']=='collection':
        require(d['cases']==list(CASES),'case set differs');require(len(d['commands'])==20 and all(x['returncode']==0 for x in d['commands']),'execution receipt missing')
        require(sha(p/'build-result.json')==d['build_result_sha256'],'build result differs')
        for index,case in enumerate(CASES):
            for offset,profile in enumerate(('original','diagnostic')):
                argv=d['commands'][2*index+offset]['argv']
                require(len(argv)==3 and Path(argv[0]).name=='n3_'+profile and argv[1:]==['--case',case],'case/profile command mismatch')
        parent=json.loads((p/'build-result.json').read_text())
        require(parent['schema']=='n3-artifact-v1' and parent['status']=='passed' and parent['kind']=='build' and parent['timing_evidence'] is False and parent['baseline_headers']==BASELINE,'invalid build parent')
        for name,digest in BASELINE.items():require(parent['files']['input/original/'+name]==digest,'copied build is not fixed F')
        require(len(parent['commands'])==2 and all(c['returncode']==0 for c in parent['commands']),'copied build commands missing')
        for index,profile in enumerate(('original','diagnostic')):
            flags=[v for v in parent['commands'][index]['argv'] if str(v).startswith('-DN3_DIAGNOSTIC')]
            require(flags==['-DN3_DIAGNOSTIC='+str(index)],'copied binary profile differs')
        require(set(d['binary_hashes'])=={'n3_original','n3_diagnostic'},'binary hash set differs')
        require(all(parent['files'][name]==digest for name,digest in d['binary_hashes'].items()),'binary identity mismatch')
        require(d['executed_runner_sha256']==parent['files']['input/observe.py'],'executed runner identity differs')
        require(sha(__file__)==d['executed_runner_sha256'] and sha(HERE/'audit_events.py')==parent['files']['input/audit_events.py'],'offline checker revision differs; use frozen tool/observe.py')
        for name in ('observe.py','generate.py','audit_events.py','event_kinds.json'):
            require(sha(p/'tool'/name)==parent['files']['input/'+name],'frozen checker differs')
        require(sha(p/'contract.json')==parent['files']['input/contract.json'] and sha(p/'event_kinds.json')==parent['files']['input/event_kinds.json'],'contract/catalog differ from build')
        expected_files={'build-result.json','contract.json','event_kinds.json','summaries.json'}|{'tool/'+name for name in ('observe.py','generate.py','audit_events.py','event_kinds.json')}|{case+'-'+profile+'.'+stream for case in CASES for profile in ('original','diagnostic') for stream in ('stdout','stderr')}
        require(set(d['files'])==expected_files,'collection evidence case inventory differs')
        computed={}
        for case in CASES:
            report=json.loads((p/(case+'-diagnostic.stdout')).read_text());original=json.loads((p/(case+'-original.stdout')).read_text())
            require(report['case']==case and original['case']==case and original['trace'] is None and original['schema']=='n3-probe-v1' and original['status']==('passed_control' if case=='recorder_overflow' else 'passed') and original['semantic']==report['semantic'],'raw semantic mismatch')
            computed[case]=check_case(report,p/'event_kinds.json')
        require(computed==json.loads((p/'summaries.json').read_text()),'recomputed counters differ')
    else:raise RuntimeError('unknown artifact kind')
    return d

def main():
    parser=argparse.ArgumentParser();sub=parser.add_subparsers(dest='command',required=True)
    p=sub.add_parser('prepare');p.add_argument('--source',type=Path,default=ROOT);p.add_argument('--out',type=Path,required=True)
    b=sub.add_parser('build');b.add_argument('--prepared',type=Path,required=True);b.add_argument('--out',type=Path,required=True)
    for key in ('compiler','sdk','library','deps'):b.add_argument('--'+key,type=Path,required=True)
    c=sub.add_parser('collect');c.add_argument('--build',type=Path,required=True);c.add_argument('--out',type=Path,required=True)
    q=sub.add_parser('audit');q.add_argument('--record',type=Path,required=True)
    a=parser.parse_args()
    try:
        if a.command=='prepare':prepare(a)
        elif a.command=='build':build(a)
        elif a.command=='collect':collect(a)
        else:d=audit(a.record.resolve());print(json.dumps({'status':'audited','kind':d['kind']}))
        return 0
    except Exception as e:print(type(e).__name__+': '+str(e),file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
