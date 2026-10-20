"""One root-owned W1 matrix gate; 50 model children + two small-pool probes."""
from pathlib import Path
import argparse,hashlib,json,os,re,shlex,shutil,stat,sys,time
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
from common import B,H,C,P,P1,fid,write,env
import checks as K

OUT=B/'production-C-matrix-v2';QUEUE=B/'queues/production-C-matrix-v2'
SOURCE=B/'source';MATERIALIZATION=B/'source-identity.json'
CANDIDATE_IDENTITY=C/'candidate-source-identity-v2.json'
PRIOR_INVENTORY=B/'full-final-v3-normal-logs/inventory-acceptance.json'
PRIOR_INVENTORY_ID={'bytes':6933,'sha256':'833550b07a2dbce9cb0b51d3beec5da62689086f9f25bae3c9d70912f55ead96'}
ADDED_TESTS={'test_canonical_cold_no_callback','test_canonical_zero_target'}
AUTHOR_PINS={'matrix':{'path':H/'production-matrix-v2','bytes':2491,'sha256':'3eb479ea56570c453729961ba6d985b4a84eac660724e0079a78e7edc04bf53a'},
 'pool':{'path':H/'receive-pool-probe/v1','bytes':3824,'sha256':'e3d1a43008512d980f5661286a3088168062667e8568480435d97efefded3d79'}}
CORE_HEADERS=('xoffsetdatastructure.hpp','xoffset_virtual_memory.hpp','xoffset_verified_paths.hpp',
 'xoffset_core_lifetime.hpp','xoffset_verified_receive.hpp','receive_raw.hpp','receive_raw_fwd.hpp','receive_future.hpp')
BUSINESS=('experiments/serialization_comparison/adapters/xoffset/adapter.hpp',
 'experiments/serialization_comparison/include/adapter.hpp','experiments/serialization_comparison/include/model.hpp',
 'experiments/serialization_comparison/include/views.hpp','experiments/serialization_comparison/include/sha256.hpp')
REFERENCE=Path('/root/xoffset-serialization-comparison/20261017/implementation/prepared-v1/reference.py')
REFERENCE_ID={'bytes':21135,'sha256':'2ce49fc651773833865588b389ee5352fc9161c8f14b9cfb34c27f5f1376394f'}

def need(ok,message):K.need(ok,message)
def exact(path,identity):
    path=Path(path);need(stat.S_ISREG(path.lstat().st_mode) and fid(path)==identity,'changed regular file '+str(path))
def verify_source(source,production,business):
    for name,value in {**production,**business}.items():exact(source/name,value)
def atomic_status(status):
    name=OUT/('status-publishing-'+str(status['status_revision'])+'.json')
    write(name,status);os.replace(name,OUT/'status.json');status['status_revision']+=1
def bounded_command(out,name,args,timeout,started,sanitized=False):
    remaining=3600-(time.monotonic()-started)
    need(remaining>0,'outer 3600s exhausted before command launch')
    return P.command(out,name,args,min(timeout,remaining),env(sanitized))
def author_snapshot(label):
    declaration=AUTHOR_PINS[label];original=declaration['path'];receipt=original/'receipt.json'
    actual=fid(receipt);need(actual['sha256']==declaration['sha256'] and
      (declaration['bytes'] is None or actual['bytes']==declaration['bytes']),'frozen '+label+' receipt')
    value=K.read(receipt);destination=OUT/('author-'+label);destination.mkdir()
    for name,identity in value['files'].items():
        need(not Path(name).is_absolute() and '..' not in Path(name).parts,'author relative member')
        exact(original/name,identity);target=destination/name;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(original/name,target);exact(target,identity)
    shutil.copyfile(receipt,destination/'receipt.json')
    return destination,value,actual
def prerequisite(spec):
    need(spec['kind']=='c14-m1-production-matrix-input-v1','current M1 input required')
    source=Path(spec['source']);need(source==SOURCE,'exact current matrix source')
    exact(MATERIALIZATION,spec['materialization']['identity'])
    record=K.read(MATERIALIZATION);production=record['production_files']
    exact(CANDIDATE_IDENTITY,spec['candidate_identity']['identity'])
    need(K.read(CANDIDATE_IDENTITY)['production_files']==production and len(production)==14,'current production14 identity')
    for mode,ref in spec['full_receipts'].items():
        exact(ref['path'],ref['identity']);r=K.read(ref['path'])
        need(r['status']=='passed' and r['production_files']==production and len(r['junit_names'])==len(set(r['junit_names']))==73,'current actual full73 '+mode)
    for path,value in spec['bindings'].items():exact(path,value)
    for name,value in spec['source_members'].items():exact(source/name,value)
    need(Path(spec['reference']['path'])==REFERENCE and spec['reference']['identity']==REFERENCE_ID,'fixed original reference')
    exact(REFERENCE,REFERENCE_ID)
    return source,production

def build_binding(profile,tag,author,source,production,business,started):
    sanitized=profile=='sanitized';build=OUT/(profile+'-'+tag+'-build');cmake=P1/'tools/cmake/bin/cmake'
    args=[cmake,'-G','Ninja','-S',author,'-B',build,'-DCMAKE_BUILD_TYPE=Release',
      '-DCMAKE_CXX_COMPILER='+str(P1/'tools/p2996-profile/bin/clang++-libcxx'),
      '-DCMAKE_CXX_FLAGS=-stdlib=libc++ -pthread','-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG',
      '-DXC_SOURCE='+str(source)]
    if tag=='matrix':args+=['-DXC_OWNER_RUNTIME_SOURCE='+str(source/'snapshot_index_runtime.cpp'),'-DXC_SCRATCH_RUNTIME_SOURCE='+str(source/'validation_scratch.cpp')]
    if sanitized:args+=['-DXC_SANITIZERS=ON','-DXC_SANITIZER_RESOURCE='+str(P1/'tools/p2996-profile/sanitizer-resource')]
    prefix=profile+'-'+tag
    bounded_command(OUT,prefix+'-configure',args,180,started,sanitized)
    targets=['matrix_abde','matrix_receive','matrix_receive_specials'] if tag=='matrix' else ['receive_pool_probe']
    bounded_command(OUT,prefix+'-build',[cmake,'--build',build,'--target',*targets,'--parallel','2'],900,started,sanitized)
    binding=K.read(build/('matrix-source-binding.json' if tag=='matrix' else 'probe-source-binding.json'))
    need(binding['selected_main_sha256']==production['xoffsetdatastructure.hpp']['sha256'],'CMake header content pin')
    expected_headers=set(CORE_HEADERS) if tag=='pool' else {n for n in production if n.endswith('.hpp')} - {'receive_future.hpp'}
    need(set(binding['headers'])==expected_headers,'exact frozen CMake binding header inventory')
    for name,value in binding['headers'].items():need(value==production[name],'configure header record '+name)
    bounded_command(OUT,prefix+'-deps',['/usr/bin/ninja','-C',build,'-t','deps'],60,started,sanitized)
    dep_path=OUT/(prefix+'-deps.stdout');sections=dep_path.read_text().split('\n\n')
    rows=K.read(build/'compile_commands.json');expected={str(author/(t+'.cpp')):t for t in targets}
    if tag=='matrix':expected.update({str(source/'snapshot_index_runtime.cpp'):'matrix_snapshot_index_runtime',str(source/'validation_scratch.cpp'):'matrix_validation_scratch'})
    need(len(rows)==len(expected) and {str(Path(r['file']).resolve()) for r in rows}==set(expected),'exact compiler TU inventory')
    dependencies=set();objects=[]
    for row in rows:
        file=str(Path(row['file']).resolve());target=expected[file];argv=row.get('arguments') or shlex.split(row['command'])
        optim=[x for x in argv if re.fullmatch(r'-O(?:[0-3gsz]|fast)',x)];need(optim and all(x=='-O3' for x in optim),'effective O3')
        if target in targets:
            need([x for x in argv if x.startswith('-std=')][-1]=='-std=gnu++26' and '-freflection' in argv and '-fexpansion-statements' in argv,'actual GNU26/reflection flags')
            definitions=[x for x in argv if x in ('-DNDEBUG','-UNDEBUG') or x.startswith('-DNDEBUG=')]
            need(definitions and definitions[-1]=='-UNDEBUG','assertions active')
            need(any('MAIN_SHA256=' in x and production['xoffsetdatastructure.hpp']['sha256'] in x for x in argv),'compiled actual main macro')
        if sanitized:need('-fsanitize=address,undefined' in argv and '-fno-omit-frame-pointer' in argv and
            [x for x in argv if x.startswith('-resource-dir=')]==['-resource-dir='+str(P1/'tools/p2996-profile/sanitizer-resource')],'fixed compatible sanitizer')
        else:need(not any(x.startswith('-fsanitize=') for x in argv),'normal sanitizer separation')
        obj=argv[argv.index('-o')+1];selected=[x for x in sections if x.startswith(obj+':')]
        need(len(selected)==1 and 'VALID' in selected[0].splitlines()[0],'actual VALID Ninja object section')
        deps={x.strip() for x in selected[0].splitlines()[1:] if x.strip()};need(file in deps,'actual TU missing in deps');dependencies.update(deps)
        if target in targets:
            required_headers=tuple(binding['headers']) if tag=='pool' else (tuple(n for n in production if n.endswith('.hpp')) if target=='matrix_abde' else CORE_HEADERS)
            for name in required_headers:
                need({x for x in deps if Path(x).name==name}=={str(source/name)},'actual selected own header '+name)
            if tag=='matrix':
                for name in BUSINESS:need(str(source/name) in deps,'actual model/view/adapter closure '+name)
                need(str(author/'host_meter.hpp') in deps,'actual meter closure')
        objects.append({'target':target,'argv':argv,'actual_dependencies':sorted(deps)})
    if tag=='matrix':
        for name in production:need(str(source/name) in dependencies,'all fourteen actual production inputs '+name)
    binaries={t:fid(build/'bin'/t) for t in targets}
    runtimes={}
    if tag=='matrix':
        for name in ('libmatrix_snapshot_index_runtime.so','libmatrix_validation_scratch.so'):runtimes[name]=fid(build/'bin'/name)
    linkage={}
    for target in targets:
        label=prefix+'-'+target+'-linkage'
        bounded_command(OUT,label,['/usr/bin/ldd',build/'bin'/target],60,started,sanitized)
        path=OUT/(label+'.stdout');text=path.read_text();need('not found' not in text,'unresolved linked runtime')
        for name in runtimes:
            if name in text:need(name+' => '+str(build/'bin'/name)+' ' in text,'linked runtime came from another path')
        if target=='matrix_abde':need(all(name in text for name in runtimes),'ABDE explicit shared runtimes missing')
        if target=='matrix_receive':need('libmatrix_validation_scratch.so' in text,'C explicit scratch runtime missing')
        linkage[target]=fid(path)
    record={'binding':fid(build/('matrix-source-binding.json' if tag=='matrix' else 'probe-source-binding.json')),
      'compile_commands':fid(build/'compile_commands.json'),'Ninja_deps':fid(dep_path),'objects':objects,'binaries':binaries,'runtimes':runtimes,'actual_linkage':linkage}
    write(OUT/(prefix+'-compiled-binding.json'),record);return build,record

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--spec',type=Path,required=True);args=ap.parse_args()
    authority=P.queue_authority(QUEUE);started=time.monotonic();spec=K.read(args.spec)
    source,production=prerequisite(spec);OUT.mkdir(parents=True,exist_ok=False)
    write(OUT/'authority.json',authority)
    status={'schema_version':1,'status':'preparing','status_revision':0,'planned_model_children':50,'planned_pool_children':2,
      'attempted':[],'successful_children':0,'error':None,'performance_samples':0};atomic_status(status)
    try:
        business={name:fid(source/name) for name in BUSINESS};author,author_receipt,author_pin=author_snapshot('matrix');pool,pool_receipt,pool_pin=author_snapshot('pool')
        tools={str(p):fid(p) for p in (HERE/'worker.py',HERE/'checks.py',H/'common.py',Path(P.__file__))}
        write(OUT/'registration.json',{'spec':fid(args.spec),'source':str(source),'materialization':spec['materialization'],
          'candidate_identity':spec['candidate_identity'],'production_files':production,'business_files':business,
          'matrix_author':author_pin,'pool_author':pool_pin,'reference':spec['reference'],'execution_tools':tools,
          'records':[1,16,256,16384],'modes':['once','repeat16','shared3'],'profiles':['normal','sanitized'],
          'planned_native_children':52,'outer_seconds':3600,'child_seconds':240,'no_retries':True,'formal_competitor_measurement':False})
        fixtures={}
        for n in (1,16,256,16384):
            exact(REFERENCE,spec['reference']['identity']);directory=OUT/'fixtures'/str(n)
            bounded_command(OUT,'fixture-'+str(n),['/usr/bin/python3','-B',REFERENCE,'--out',directory,'--n',str(n),'--seed','xoffset-competitor-v1'],120,started)
            fixture=K.read(directory/'fixture.json');need(type(fixture['n']) is int and fixture['n']==n and fixture['seed']=='xoffset-competitor-v1' and fixture['schema']=='sc-fixture-v1','fixed generated fixture')
            need(len(fixture['stages'])==9 and [x['stage'] for x in fixture['stages']]==list(range(1,10)),'original nine-stage fixture')
            for stage in fixture['stages']:
                need(type(stage['stage']) is int and type(stage['mutation_count']) is int and stage['mutation_count']>=0 and
                    re.fullmatch('[0-9a-f]{64}',stage['canonical_sha256']) is not None,'fixture stage metadata')
            for name,identity in fixture['files'].items():
                need(Path(name).name==name and '/' not in name and '\\' not in name,'flat fixture member')
                exact(directory/name,identity)
            fixtures[n]={'directory':str(directory),'manifest':fid(directory/'fixture.json'),
              'input':fid(directory/'input.canonical.bin'),'trace':fid(directory/'trace.bin'),'stages':fixture['stages']}
        bindings={};cases=[];statistics={}
        for profile in ('normal','sanitized'):
            build,bindings[profile]=build_binding(profile,'matrix',author,source,production,business,started)
            pool_build,pool_binding=build_binding(profile,'pool',pool,source,production,business,started);bindings[profile]['pool']=pool_binding
            jobs=[]
            for target in ('matrix_abde','matrix_receive'):
                for n in (1,16,256,16384):
                    for mode in ('once','repeat16','shared3'):jobs.append((target,n,mode))
            jobs += [('matrix_receive_specials',16,None),('receive_pool_probe',None,None)]
            totals={}
            for target,n,mode in jobs:
                label='-'.join(str(x) for x in (profile,target,n,mode) if x is not None);result=OUT/(label+'.json')
                binary=(pool_build if target=='receive_pool_probe' else build)/'bin'/target
                exact(binary,(pool_binding if target=='receive_pool_probe' else bindings[profile])['binaries'][target])
                for name,value in bindings[profile]['runtimes'].items():exact(build/'bin'/name,value)
                argv=[binary];fixture=fixtures[n] if n is not None else None
                if fixture:
                    directory=Path(fixture['directory']);exact(directory/'input.canonical.bin',fixture['input']);exact(directory/'trace.bin',fixture['trace'])
                    argv+=[directory/'input.canonical.bin']
                    if target!='matrix_abde':argv+=[directory/'trace.bin']
                    if mode is not None:argv +=[str(n),mode]
                argv+=[result]
                remaining=3600-(time.monotonic()-started);need(remaining>0 and not result.exists(),'fresh child and remaining budget')
                status['status']='running';status['attempted'].append({'label':label,'target':target,'profile':profile,'records':n,'mode':mode,'argv':list(map(str,argv))});atomic_status(status)
                receipt=P.command(OUT,label,argv,min(240,remaining),env(profile=='sanitized'))
                need(receipt['returncode']==0 and receipt['timed_out'] is False and receipt['exception_type'] is None and
                  type(receipt['pid']) is int and receipt['pid']>0 and K.u(receipt['elapsed_diagnostic_ns'])<=240*10**9,'successful native child strict budget')
                need((OUT/(label+'.stderr')).stat().st_size==0 and (OUT/(label+'.stdout')).stat().st_size==0,'no unexplained native stderr/stdout')
                raw=K.read(result);main_sha=production['xoffsetdatastructure.hpp']['sha256']
                if target=='matrix_abde':values=K.abde(raw,main_sha,fixture,n,mode)
                elif target=='matrix_receive':values=K.receive(raw,main_sha,fixture,n,mode)
                elif target=='matrix_receive_specials':K.specials(raw,main_sha,fixture);values={'functional_groups':3}
                else:K.pool(raw,main_sha);values={'independent_pool_children':1}
                for key,value in values.items():totals[target+'.'+key]=totals.get(target+'.'+key,0)+value
                cases.append({'profile':profile,'target':target,'records':n,'mode':mode,'result':{'path':str(result),'identity':fid(result)},
                  'receipt':{'path':str(OUT/(label+'.receipt.json')),'identity':fid(OUT/(label+'.receipt.json'))}})
                status['successful_children']+=1;atomic_status(status)
            need(totals['matrix_abde.cycles']==totals['matrix_receive.cycles']==72 and totals['matrix_abde.receiver_completions']==totals['matrix_receive.receiver_lifecycles']==80,'per-profile cycles/lifecycles')
            need(totals['matrix_receive.successful_finishes']==160 and totals['matrix_receive.operation_completions']==232 and
              totals['matrix_receive.cancelled_finish_rejections']==72 and totals['matrix_receive.validated_stages']==720 and
              totals['matrix_receive.source_verified_full_query_batches']==240,'complete C semantic/count aggregate')
            statistics[profile]=totals
        need(status['successful_children']==len(status['attempted'])==len(cases)==52,'complete fifty model plus two pool children')
        verify_source(source,production,business);exact(REFERENCE,spec['reference']['identity'])
        for path,value in spec['bindings'].items():exact(path,value)
        for name,value in spec['source_members'].items():exact(source/name,value)
        for path,value in tools.items():exact(path,value)
        for label,original in AUTHOR_PINS.items():
            for name,value in (author_receipt if label=='matrix' else pool_receipt)['files'].items():exact(original['path']/name,value)
        need(time.monotonic()-started<=3600,'strict complete worker budget before publication')
        write(OUT/'result.json',{'schema_version':1,'status':'passed','normal_model_cases':25,'sanitized_model_cases':25,
          'pool_cases':2,'actual_native_children':52,'source':str(source),'materialization':spec['materialization'],
          'candidate_identity':spec['candidate_identity'],'matrix_author':author_pin,'pool_author':pool_pin,
          'compiled_bindings':bindings,'fixtures':fixtures,'cases':cases,'actual_counts':statistics,
          'full_value_stage_digest_operation_status_queries_checked_by_pinned_native_source':True,
          'root_raw_audit_scope':'strict identities, schema, native receipts, counts and accounting; full values/query vectors checked internally by pinned CPP',
          'global_VM_active_retained_measured':False,'small_pool_is_independent_not_512MiB_RSS':True,
          'all_custom_support_proven_by_business_matrix':False,'standard_lifetime_proof_replaced_by_SAN':False,
          'formal_competitor_measurement':False,'old_save_target_substitution':False,'no_retries':True,
          'global_queue_cleanup_and_full_exit_budget_require_root_receipt':True})
        status['status']='passed'
    except BaseException as error:
        status['status']='failed-prefix';status['error']=type(error).__name__+': '+str(error);raise
    finally:
        status['elapsed_seconds']=time.monotonic()-started;atomic_status(status)

if __name__=='__main__':main()
