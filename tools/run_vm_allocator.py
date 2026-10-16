#!/usr/bin/env python3
"""Stage III isolated source generation, serial experiments and fail-closed offline audit."""
from __future__ import annotations
import argparse, datetime, hashlib, importlib.util, json, math, os, platform, shutil, statistics, subprocess, sys, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
VARIANTS=('F','A-core','A-wire','B-core','B-wire','C-linear','C-index')
BOOST=('container','core','config','throw_exception','static_assert','container_hash','type_traits','winapi','intrusive','predef')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,obj):Path(p).write_text(json.dumps(obj,indent=2,sort_keys=True)+'\n')
def now():return datetime.datetime.now(datetime.timezone.utc).isoformat()
def manifest(p):return {str(f.relative_to(p)):sha(f) for f in sorted(p.rglob('*')) if f.is_file() and '__pycache__' not in f.parts}
def verify(p,files):
    for name,digest in files.items():
        if not (p/name).is_file() or sha(p/name)!=digest:raise RuntimeError('input changed/missing: '+str(p/name))
def run(argv,timeout=300):
    return subprocess.run(list(map(str,argv)),capture_output=True,text=True,timeout=timeout,check=True)
def module(name,root=ROOT):
    spec=importlib.util.spec_from_file_location(name,root/'tools'/f'{name}.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

def probe(source,variant):
    anchor='        const void* root_type_identity_ = nullptr;\n        std::uint64_t m_epoch = 0;\n    };'
    if source.count(anchor)!=1:raise RuntimeError('probe anchor drift')
    host='sizeof(free_blocks_)+free_blocks_.capacity()*sizeof(FreeBlock)'
    valid='true'
    if variant.startswith('C-'):host='free_blocks_.metadata_capacity_bytes()';valid='free_blocks_.invariants_hold()'
    extra='''
    public:
        template<class F> void s3_visit_free(F&& visit) const {
            for(const auto& block:free_blocks_) visit(block.user_offset,block.user_bytes,block.alignment);
        }
        std::size_t s3_host_bytes() const {return HOST;}
        bool s3_validate_index() const {return VALID;}
'''.replace('HOST',host).replace('VALID',valid)
    if variant.startswith('C-'):
        source=source.replace('            std::vector<Slot> slots_;','        public:\n            std::vector<Slot> slots_;')
        extra+='        bool s3_bad_index_control() {auto saved=free_blocks_.head_;free_blocks_.head_=free_blocks_.storage_size()+1;bool caught=!free_blocks_.invariants_hold();free_blocks_.head_=saved;return caught;}\n'
        hook='            std::uint64_t next_sequence_ = 1;'
        if source.count(hook)!=1:raise RuntimeError('sequence probe drift')
        source=source.replace(hook,'        public:\n'+hook+'\n        private:')
        extra+='        void s3_force_sequence_overflow() {free_blocks_.next_sequence_=(std::numeric_limits<std::uint64_t>::max)();}\n'
    else:extra+='        void s3_force_sequence_overflow() {}\n        bool s3_bad_index_control() {return true;}\n'
    source=diagnose(source,variant)
    return source.replace(anchor,anchor[:-6]+extra+'    };')

def diagnose(source,variant):
    fields=('core_scans','core_moved_bytes','core_new','core_whole','core_split','wire_scans','wire_split','wire_reuse','wire_retire','wire_flush','wire_transfer_blocks','expand_attempts','expand_existing','expand_grown','element_migration_bytes','metadata_allocation_calls','metadata_allocated_bytes','max_owner_metadata_capacity')
    anchor='namespace XOffsetDatastructure {'
    if source.count(anchor)!=1:raise RuntimeError('diagnostic namespace drift')
    prefix='namespace s3_diagnostics {\n'+''.join('inline std::uint64_t '+f+'=0;\n' for f in fields)+'}\n'
    source=source.replace(anchor,prefix+anchor)
    def patch(old,new,count=1):
        nonlocal source
        if source.count(old)!=count:raise RuntimeError('diagnostic anchor drift: '+old[:90])
        source=source.replace(old,new)
    if variant.startswith('C-'):
        patch('return slot.block.user_bytes >= bytes && slot.block.alignment >= alignment;', 'return slot.block.user_bytes >= bytes && slot.block.alignment >= alignment;') if False else None
        old='return slot.block.user_bytes >= bytes && slot.block.alignment >= alignment &&'
        patch(old,'++s3_diagnostics::core_scans;\n                '+old)
        patch('free_blocks_.erase_slot(donor);','++s3_diagnostics::core_whole;\n                free_blocks_.erase_slot(donor);')
    else:
        old='const auto& block = free_blocks_[i];' if variant=='F' or variant.endswith('wire') else 'const auto block = free_blocks_[i];'
        patch(old,'++s3_diagnostics::core_scans;\n                '+old)
        old='free_blocks_.erase(free_blocks_.begin() + static_cast<std::ptrdiff_t>(i));'
        patch(old,'++s3_diagnostics::core_whole; s3_diagnostics::core_moved_bytes+=(free_blocks_.size()-i-1)*sizeof(FreeBlock);\n                    '+old)
    old='free_blocks_.push_back(\n                    {header->user_offset, header->total_bytes, header->user_bytes, header->alignment});'
    host='sizeof(free_blocks_)+free_blocks_.capacity()*sizeof(FreeBlock)' if not variant.startswith('C-') else 'free_blocks_.metadata_capacity_bytes()'
    slot='sizeof(FreeBlock)' if not variant.startswith('C-') else 'free_blocks_.metadata_slot_bytes()'
    new='const auto old_metadata_capacity=free_blocks_.capacity();\n                '+old+'\n                if(free_blocks_.capacity()!=old_metadata_capacity) {++s3_diagnostics::metadata_allocation_calls;s3_diagnostics::metadata_allocated_bytes+=free_blocks_.capacity()*'+slot+';}\n                s3_diagnostics::max_owner_metadata_capacity=std::max<std::uint64_t>(s3_diagnostics::max_owner_metadata_capacity,'+host+');'
    patch(old,new)
    patch('sm->used_end = std::max<std::uint64_t>(sm->used_end, block_end);','++s3_diagnostics::core_new;\n            sm->used_end = std::max<std::uint64_t>(sm->used_end, block_end);')
    patch('bool try_expand_impl(void* ptr, std::uint32_t bytes, std::size_t alignment) {','bool try_expand_impl(void* ptr, std::uint32_t bytes, std::size_t alignment) {\n            ++s3_diagnostics::expand_attempts;')
    patch('if (bytes <= header->user_bytes) return true;','if (bytes <= header->user_bytes) {++s3_diagnostics::expand_existing;return true;}')
    patch('arena()->used_end = offset + bytes;','++s3_diagnostics::expand_grown;\n            arena()->used_end = offset + bytes;')
    patch('auto* block = freelist_block_from_offset(state, *link);','++s3_diagnostics::wire_scans;\n                            auto* block = freelist_block_from_offset(state, *link);')
    patch('state->free_bytes -= block->block_bytes;','++s3_diagnostics::wire_reuse;\n                                state->free_bytes -= block->block_bytes;')
    patch('state->free_bytes += aligned_bytes;','++s3_diagnostics::wire_retire;\n            state->free_bytes += aligned_bytes;')
    patch('if (!state || !arena) return;\n            for (std::uint32_t i = 0; i < freelist_class_count; ++i)','if (!state || !arena) return;\n            ++s3_diagnostics::wire_flush;\n            for (std::uint32_t i = 0; i < freelist_class_count; ++i)')
    patch('arena->deallocate(block);','++s3_diagnostics::wire_transfer_blocks;\n                    arena->deallocate(block);')
    patch('auto old_cap = capacity_;\n                for (size_type i = 0; i < size_; ++i)','auto old_cap = capacity_;\n                s3_diagnostics::element_migration_bytes+=std::uint64_t(size_)*sizeof(T);\n                for (size_type i = 0; i < size_; ++i)')
    if variant.endswith('core'):
        patch('free_blocks_[i] = residual;','++s3_diagnostics::core_split;\n                                                free_blocks_[i] = residual;')
        if variant=='B-core':patch('free_blocks_[i] = prefix;','++s3_diagnostics::core_split;\n                                        free_blocks_[i] = prefix;')
    if variant.endswith('wire'):
        patch('return reinterpret_cast<void*>(result_address);','++s3_diagnostics::wire_split;\n            return reinterpret_cast<void*>(result_address);')
    return source

def prepared(path, external=True):
    meta=json.loads((path/'prepared.json').read_text())
    if meta.get('status')!='prepared':raise RuntimeError('preparation incomplete')
    verify(path,meta['files'])
    if not external:return meta
    for filename,digest in meta['external_files'].items():
        if sha(filename)!=digest:raise RuntimeError('toolchain/runtime changed: '+filename)
    for dep,identity in meta['dependencies'].items():
        directory=Path(meta['deps'])/dep
        if run(['git','-C',directory,'rev-parse','HEAD']).stdout.strip()!=identity or run(['git','-C',directory,'status','--porcelain']).stdout.strip():raise RuntimeError('dependency changed')
    return meta

def prepare(a):
    out=a.out.resolve();out.mkdir(parents=True,exist_ok=False);state={'status':'running','started':now()};write(out/'prepared.json',state)
    try:
        if shutil.disk_usage(out).free<3*1024**3:raise RuntimeError('less than 3GiB disk reserve')
        common=[a.compiler,'-O3','-std=c++26','-freflection','-fexpansion-statements','-stdlib=libc++','-pthread']
        if platform.system()=='Darwin':common+=['-arch',platform.machine(),'-isysroot',a.sdk,'-mmacosx-version-min=10.15']
        for lib in BOOST:common+=['-isystem',str(a.deps.resolve()/'boost/libs'/lib/'include')]
        common+=['-isystem',str(a.deps.resolve()/'typelayout/include'),'-L'+a.library,'-Wl,-rpath,'+a.library]
        compiler=run([a.compiler,'--version']).stdout
        source=a.source.resolve();inputs=out/'inputs';inputs.mkdir()
        for name in ['xoffsetdatastructure.hpp','xoffset_virtual_memory.hpp']:shutil.copy2(source/name,inputs/name)
        for directory in ['tests','tools']:shutil.copytree(ROOT/directory,inputs/directory,ignore=shutil.ignore_patterns('__pycache__'))
        input_identity=manifest(inputs);original=(inputs/'xoffsetdatastructure.hpp').read_text()
        expected=json.loads((ROOT/'tools/stage3-preregister.json').read_text())
        for name,digest in expected['baseline_headers'].items():
            if sha(source/name)!=digest:raise RuntimeError('F baseline hash mismatch')
        commands=[]
        for variant in a.variants.split(','):
            if variant not in VARIANTS:raise RuntimeError('unknown variant')
            folder=out/variant;folder.mkdir();snap=folder/'source';snap.mkdir()
            for name in ['xoffsetdatastructure.hpp','xoffset_virtual_memory.hpp']:shutil.copy2(inputs/name,snap/name)
            for directory in ['tests','tools']:shutil.copytree(inputs/directory,snap/directory)
            generated=original
            if variant!='F':
                gen='stage3_candidates_index' if variant.startswith('C-') else ('stage3_candidates_core' if variant.endswith('core') else 'stage3_candidates_wire')
                generated=module(gen,inputs).transform(original,variant)
            if variant=='B-core':
                policy=snap/'tests/test_storage_policies.cpp';data=policy.read_text()
                old='    assert(reused == left && buffer.get_size() == logical);'
                if data.count(old)!=1:raise RuntimeError('B Core policy-test anchor drift')
                new='''    // Generated B-core policy expectation: high-end allocation may change the user address.
    // Preserve the F assertion in the production test; check all reuse safety contracts here.
    assert(buffer.get_size() == logical);
    const auto start=reinterpret_cast<std::uintptr_t>(left);
    const auto result=reinterpret_cast<std::uintptr_t>(reused);
    XBufferCore::AllocationHeader physical;
    std::memcpy(&physical,static_cast<const char*>(reused)-sizeof(physical),sizeof(physical));
    assert(result>=start && result+physical.user_bytes==start+2048);
    assert(physical.user_bytes>=1800 && physical.total_bytes==16ull+physical.user_bytes);
    assert(physical.user_offset+std::uint64_t(physical.user_bytes)==buffer.arena()->used_end);
    assert(result%8==0);'''
                policy.write_text(data.replace(old,new))
            (snap/'xoffsetdatastructure.hpp').write_text(generated)
            diag=folder/'diagnostic';diag.mkdir()
            for name in ['xoffset_virtual_memory.hpp']:shutil.copy2(snap/name,diag/name)
            (diag/'xoffsetdatastructure.hpp').write_text(probe(generated,variant))
            # Relative ../header includes in tests and harnesses select their actual snapshot.
            for directory in ['tests','tools']:shutil.copytree(snap/directory,diag/directory)
            fields=['core_scans','core_moved_bytes','core_new','core_whole','core_split','wire_scans','wire_split','wire_reuse','wire_retire','wire_flush','wire_transfer_blocks','expand_attempts','expand_existing','expand_grown','element_migration_bytes','metadata_allocation_calls','metadata_allocated_bytes','max_owner_metadata_capacity']
            reporter='\nint main(int argc,char** argv) {int code=s3_payload_main(argc,argv);std::cerr<<"S3DIAG:{";'
            for i,field in enumerate(fields):reporter+='std::cerr<<"'+(',' if i else '')+'\\\"'+field+'\\\":"<<s3_diagnostics::'+field+';'
            reporter+='std::cerr<<"}\\n";return code;}\n'
            for filename in ['bench_vm_followup.cpp','bench_vm_automatic_trace.cpp']:
                data=(diag/'tools'/filename).read_text();anchor_main='int main(int argc, char** argv)'
                if data.count(anchor_main)!=1:raise RuntimeError('diagnostic main drift')
                statement='followup::print(s);' if filename=='bench_vm_followup.cpp' else 'automatic_trace::run(c);'
                if data.count(statement)!=1:raise RuntimeError('diagnostic success-return anchor drift')
                data=data.replace(statement,statement+'\n        return 0;')
                (diag/'tools'/('observe_'+filename)).write_text(data.replace(anchor_main,'int s3_payload_main(int argc, char** argv)')+reporter)
            targets={'base':('diagnostic/tools/test_vm_allocator_base.cpp',['-DS3_PROBE=1']),
                     'containers':('source/tools/bench_vm_allocator_containers.cpp',['-DNDEBUG']),
                     'model':('diagnostic/tests/test_allocator_model.cpp',['-DS3_PROBE=1']+(['-DS3_WHOLE=1'] if variant=='F' or variant.startswith('C-') else [])),
                     'failures':('diagnostic/tests/test_allocator_failures.cpp',['-DS3_PROBE=1']),
                     'micro':('source/tools/bench_vm_allocator.cpp',['-DNDEBUG']),
                     'diag':('diagnostic/tools/bench_vm_allocator.cpp',['-DS3_PROBE=1','-DS3_DIAGNOSTIC=1']),
                     'fragment_diag':('diagnostic/tools/observe_bench_vm_followup.cpp',['-DS3_PROBE=1','-DXOFFSET_BENCH_FOLLOWUP=1']),
                     'trace_diag':('diagnostic/tools/observe_bench_vm_automatic_trace.cpp',['-DS3_PROBE=1']),
                     'fragment':('source/tools/bench_vm_followup.cpp',['-DNDEBUG','-DXOFFSET_BENCH_FOLLOWUP=1']),
                     'trace':('source/tools/bench_vm_automatic_trace.cpp',['-DNDEBUG']),
                     'policies':('source/tests/test_storage_policies.cpp',[]),
                     'tail':('source/tests/test_tail_expansion.cpp',[]),
                     'core':('diagnostic/tools/test_vm_allocator_core.cpp',['-DS3_PROBE=1']),
                     'wire':('diagnostic/tools/test_vm_allocator_wire.cpp',['-DS3_PROBE=1']),
                     'index':('diagnostic/tools/test_vm_allocator_index.cpp',['-DS3_PROBE=1'])}
            for name in a.targets.split(','):
                if name=='core' and variant not in ['F','A-core','B-core']:continue
                if name=='wire' and variant not in ['F','A-wire','B-wire']:continue
                if name=='index' and variant not in ['F','C-linear','C-index']:continue
                if name=='tail' and variant!='F':continue
                rel,flags=targets[name];cmd=common+flags+['-I',str(folder/rel.split('/')[0]),str(folder/rel),'-o',str(folder/name)]
                started=now();r=run(cmd,timeout=300);(folder/f'compile-{name}.log').write_text(r.stdout+r.stderr)
                commands.append({'variant':variant,'target':name,'argv':list(map(str,cmd)),'started':started,'finished':now(),'returncode':r.returncode})
                write(out/'commands.json',commands);print(variant,name,'built',flush=True)
        verify(inputs,input_identity)
        shutil.copy2(inputs/'tools/run_vm_allocator.py',out/'runner.py');shutil.copy2(inputs/'tools/stage3-preregister.json',out/'preregister.json')
        ext={str(Path(a.compiler).resolve()):sha(a.compiler)}
        for n in ['libc++.1.0.dylib','libc++abi.1.0.dylib']:
            p=Path(a.library)/n
            if p.exists():ext[str(p)]=sha(p)
        deps={d:run(['git','-C',a.deps/d,'rev-parse','HEAD']).stdout.strip() for d in ['boost','typelayout']}
        state.update(status='prepared',finished=now(),compiler=compiler,deps=str(a.deps.resolve()),dependencies=deps,external_files=ext,
            variants=a.variants.split(','),targets=a.targets.split(','),files=manifest(out),platform=platform.platform())
        state['files'].pop('prepared.json',None);write(out/'prepared.json',state)
    except BaseException as e:
        record_failure(out,state,e);write(out/'prepared.json',state);raise

def environment():
    result={'at':now(),'load_average':os.getloadavg(),'platform':platform.platform()}
    for key,cmd in [('processes',['ps','-Ao','pid,pcpu,pmem,comm']),('power',['pmset','-g','batt']),('thermal',['pmset','-g','therm']),('swap',['sysctl','vm.swapusage'])]:
        try:result[key]=run(cmd,10).stdout
        except Exception as e:result[key]={'unavailable':str(e)}
    return result

def cases(suite):
    if suite=='containers':return [{'id':f'{shape}-pool{pool}','target':'containers','args':['--shape',shape,'--pool',str(pool),'--count','200000','--cycles','4']} for shape in ['sequential','interleaved'] for pool in [0,1]]
    if suite=='micro':return [{'id':f'{shape}-{n}-{order}','target':'micro','args':['--workload','lookup','--shape',shape,'--size',str(n),'--order',order,'--rounds','16']} for n in [8,64,512,4096] for shape in ['head','dense','tail','alignment','miss'] for order in ['forward','reverse','random']]
    if suite=='fragment':return [{'id':w,'target':'fragment','args':['--workload',w,'--size','1024','--cycles','8','--enabled','0']} for w in ['core_split','cache_split','core_coalesce']]
    if suite=='trace':return [{'id':f'i{i}-c{c}-b{b}-l{l}-{mode}','target':'trace','args':['--mode',mode,'--count','1000000','--initial',str(i),'--cadence',str(c),'--burst',str(b),'--lifetime',str(l)]} for i in [4096,65536] for c in [256,1024] for b in [1,8] for l in [0,16] for mode in ['preallocated','explicit','automatic']]
    if suite=='anchors':return [{'id':w,'target':'legacy','args':['--workload',w,'--size',str(n),'--cycles','8','--reserve','0']} for w,n in [('save_verified',64*1024*1024),('save_repeat',64*1024*1024),('load_verified',65536),('append_preallocated',10000000),('grow_equal_reserve',128*1024*1024)]]
    raise RuntimeError('unknown suite')
def normalized(row):
    checksum=row.get('semantic_checksum',row.get('checksum'))
    operations=row.get('operations',row.get('operation_count',row.get('size')))
    if checksum is None or operations is None:raise RuntimeError('missing semantic identity')
    elapsed=row.get('elapsed_ms',row.get('metrics',{}).get('elapsed_ms'))
    if type(elapsed) not in (int,float) or not math.isfinite(elapsed) or elapsed<=0:raise RuntimeError('invalid elapsed')
    if type(checksum) is not int or type(operations) is not int or operations<=0:raise RuntimeError('invalid semantic count/checksum')
    identity=tuple((key,json.dumps(row[key],sort_keys=True)) for key in ['workload','shape','order','size','rounds','pool','mode','count','initial','maximum','cadence','burst','lifetime','trace_checksum','cycles','enabled','operations','retired_values_checked','total_requested_payload_bytes'] if key in row)
    if 'diagnostic' in row and row['diagnostic'] is not False:raise RuntimeError('diagnostic timing sample')
    return (checksum,operations,identity),elapsed

def validate_sample(case,row):
    flags=dict(zip(case['args'][::2],case['args'][1::2]))
    for flag,value in flags.items():
        key=flag[2:].replace('-','_')
        if key=='reserve':key='requested_reserve_bytes'
        if key=='cycles' and key not in row:key='requested_cycles'
        expected=(value=='1') if key=='enabled' else (int(value) if value.isdecimal() else value)
        if row.get(key)!=expected:raise RuntimeError('sample parameter differs from requested case: '+key)
    if row.get('workload')=='lookup':
        expected=int(flags['--rounds'])*(int(flags['--size']) if flags['--shape'] in ['head','dense'] else 1)
        if row.get('operations')!=expected:raise RuntimeError('sample completed wrong workload count')
    if row.get('workload')=='containers' and row.get('operations')!=int(flags['--count'])*int(flags['--cycles']):raise RuntimeError('sample completed wrong container count')

def window(a):
    prep=a.prepared.resolve();meta=prepared(prep);identity=sha(prep/'prepared.json');out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
    state={'status':'running','started':now(),'prepared':str(prep),'prepared_sha256':identity,'environment_before':environment(),'suite':a.suite,'before':a.before,'after':a.after,'pairs':8,'warmup_pairs':1}
    write(out/'result.json',state);rows=[]
    try:
        selected=cases(a.suite)
        if a.filter:
            wanted=a.filter.split(',')
            if len(wanted)!=len(set(wanted)) or not set(wanted)<=set(c['id'] for c in selected):raise RuntimeError('invalid workload filter')
            selected=[c for c in selected if c['id'] in wanted]
        if not selected:raise RuntimeError('empty workload selection')
        state['cases']=selected;write(out/'result.json',state)
        with (out/'raw.jsonl').open('w') as f:
            for case in selected:
                expected=None
                for phase,pairs in [('warmup',1),('measured',8)]:
                    for pair in range(pairs):
                        labels=['before','after'] if pair%2==0 else ['after','before']
                        for position,label in enumerate(labels):
                            variant=getattr(a,label);target=case['target']
                            if target=='legacy':target='micro' # dispatches the unchanged historical main
                            cmd=[prep/variant/target]+case['args'];r=run(cmd,300);row=json.loads(r.stdout);semantic,_=normalized(row);validate_sample(case,row)
                            if expected is None:expected=semantic
                            if semantic!=expected:raise RuntimeError('semantic/input mismatch')
                            row={'case':case['id'],'phase':phase,'pair':pair,'position':position,'label':label,'variant':variant,'sequence':len(rows),'command':list(map(str,cmd)),'sample':row}
                            rows.append(row);f.write(json.dumps(row)+'\n');f.flush()
                print(case['id'],'complete',flush=True)
        prepared(prep)
        if sha(prep/'prepared.json')!=identity:raise RuntimeError('prepared manifest changed')
        state.update(status='complete',finished=now(),environment_after=environment(),raw_sha256=sha(out/'raw.jsonl'),samples=len(rows))
        write(out/'result.json',state);(out/'COMPLETE').write_text(sha(out/'result.json')+'\n');audit(out)
    except BaseException as e:
        record_failure(out,state,e);raise

def record_failure(out,state,error):
    state.update(status='failed',finished=now(),error=str(error))
    if (out/'COMPLETE').exists():(out/'COMPLETE').unlink()
    if isinstance(error,(subprocess.CalledProcessError,subprocess.TimeoutExpired)):
        def decoded(value):return value.decode(errors='replace') if isinstance(value,bytes) else (value or '')
        detail={'argv':list(map(str,error.cmd)),'stdout':decoded(error.stdout),'stderr':decoded(error.stderr)}
        if isinstance(error,subprocess.TimeoutExpired):detail['timeout_seconds']=error.timeout
        else:detail['returncode']=error.returncode
        state['failed_command']=detail;(out/'failed-command.log').write_text(detail['stdout']+detail['stderr'])
    write(out/'result.json',state)

def distribution(v):
    s=sorted(v);return {'median':statistics.median(s),'min':s[0],'max':s[-1],'q1':percentile(s,.25),'q3':percentile(s,.75)}
def percentile(v,p):
    x=(len(v)-1)*p;i=int(x);return v[i]*(1-x+i)+v[min(i+1,len(v)-1)]*(x-i)
def audit(out, prepared_override=None):
    state=json.loads((out/'result.json').read_text())
    if not (out/'COMPLETE').is_file() or (out/'COMPLETE').read_text().strip()!=sha(out/'result.json'):raise RuntimeError('completion hash missing/changed')
    prep=Path(prepared_override or state['prepared'])
    if sha(prep/'prepared.json')!=state['prepared_sha256']:raise RuntimeError('prepared manifest hash mismatch')
    meta=prepared(prep,external=False)
    if state.get('pairs')!=8 or state.get('warmup_pairs')!=1:raise RuntimeError('invalid pair budget')
    canonical={c['id']:c for c in cases(state['suite'])}
    declarations=state.get('cases',[])
    if not declarations or len({c['id'] for c in declarations})!=len(declarations):raise RuntimeError('duplicate/empty cases')
    if any(canonical.get(c['id'])!=c for c in declarations):raise RuntimeError('case definition changed')
    if state.get('status')!='complete':raise RuntimeError('incomplete window')
    if sha(out/'raw.jsonl')!=state['raw_sha256']:raise RuntimeError('raw hash mismatch')
    rows=[json.loads(line) for line in (out/'raw.jsonl').read_text().splitlines()]
    expected_count=len(state['cases'])*18
    if len(rows)!=expected_count or len(rows)!=state['samples']:raise RuntimeError('missing/extra samples')
    if {r['case'] for r in rows}!=set(c['id'] for c in declarations):raise RuntimeError('unknown/missing case rows')
    if [r['sequence'] for r in rows]!=list(range(len(rows))):raise RuntimeError('broken sequence')
    summaries=[]
    for case in state['cases']:
        group=[r for r in rows if r['case']==case['id']];identities=set();ratios=[];split={0:[],1:[]}
        for phase,count in [('warmup',1),('measured',8)]:
            for pair in range(count):
                both=[r for r in group if r['phase']==phase and r['pair']==pair]
                if len(both)!=2 or {r['label'] for r in both}!={'before','after'}:raise RuntimeError('missing/duplicate pair')
                both.sort(key=lambda r:r['sequence']);order=['before','after'] if pair%2==0 else ['after','before']
                if [r['label'] for r in both]!=order or [r['position'] for r in both]!=[0,1]:raise RuntimeError('unbalanced/invalid order')
                values={}
                for r in both:
                    if r['variant']!=state[r['label']] or r['variant'] not in meta['variants']:raise RuntimeError('variant label mismatch')
                    target=case['target'] if case['target']!='legacy' else 'micro'
                    expected_command=[str(Path(state['prepared'])/r['variant']/target)]+case['args']
                    if r.get('command')!=expected_command:raise RuntimeError('command mismatch')
                    semantic,elapsed=normalized(r['sample']);validate_sample(case,r['sample']);identities.add(semantic);values[r['label']]=elapsed
                if phase=='measured':
                    if values['after']<=0:raise RuntimeError('zero measured time')
                    ratio=values['before']/values['after'];ratios.append(ratio);split[pair%2].append(ratio)
        if len(identities)!=1:raise RuntimeError('semantic mismatch')
        # Conservative small-sample distribution-free interval for the median: [min,max].
        summaries.append({'case':case['id'],'paired_before_over_after':distribution(ratios),'median_coverage_interval':{'low':min(ratios),'high':max(ratios),'coverage_at_least':1-2/2**8,'method':'min/max order statistics for 8 independent paired values; independence is an assumption'},'order_groups':{str(k):distribution(v) for k,v in split.items()}})
    return summaries

def validate(a):
    prep=a.prepared.resolve();meta=prepared(prep);identity=sha(prep/'prepared.json');out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
    result={'status':'running','started':now(),'prepared_sha256':identity,'prepared':str(prep),'commands':[]};write(out/'result.json',result)
    try:
        requested=getattr(a,'targets',None)
        mandatory=set(requested.split(',')) if requested else {'model','failures','policies'}
        if not mandatory or not mandatory<=set(meta['targets']):raise RuntimeError('required validation targets missing')
        actual=[]
        selected=getattr(a,'variants',None)
        selected=selected.split(',') if selected else meta['variants']
        if not selected:raise RuntimeError('empty validation selection')
        if len(set(selected))!=len(selected) or not set(selected)<=set(meta['variants']):raise RuntimeError('invalid validation variant selection')
        result['selected_variants']=selected
        for variant in selected:
            targets=[t for t in ['model','failures','policies'] if not requested or t in mandatory]
            for name in ['core','wire','index','base']:
                allowed={'core':['F','A-core','B-core'],'wire':['F','A-wire','B-wire'],'index':['F','C-linear','C-index'],'base':list(VARIANTS)}
                if name in meta['targets'] and variant in allowed[name] and (not requested or name in mandatory):targets.append(name)
            if variant=='F' and 'tail' in meta['targets'] and (not requested or 'tail' in mandatory):targets.append('tail')
            for target in targets:
                if target not in meta['targets']:continue
                argv=[prep/variant/target]
                if target=='model':argv += [str(a.operations),str(a.seeds)]
                if target in ['core','wire','index','base']:argv += [variant]
                if target=='wire':argv += [str(a.operations),str(a.seeds)]
                started=now();r=run(argv,1800);log=f'{variant}-{target}.log';(out/log).write_text(r.stdout+r.stderr)
                actual.append((variant,target))
                result['commands'].append({'variant':variant,'target':target,'argv':list(map(str,argv)),'started':started,'finished':now(),'returncode':r.returncode,'log':log,'log_sha256':sha(out/log)})
                write(out/'result.json',result);print(variant,target,'passed',flush=True)
        prepared(prep)
        if sha(prep/'prepared.json')!=identity:raise RuntimeError('prepared manifest changed')
        if not actual:raise RuntimeError('empty validation selection')
        result.update(status='complete',finished=now(),operations=a.operations,seeds=a.seeds,required_targets=sorted(mandatory),actual_targets=actual);write(out/'result.json',result);(out/'COMPLETE').write_text(sha(out/'result.json')+'\n')
    except BaseException as e:
        record_failure(out,result,e);raise

def audit_validation(out,prepared_override=None):
    state=json.loads((out/'result.json').read_text())
    if state.get('status')!='complete' or (out/'COMPLETE').read_text().strip()!=sha(out/'result.json'):raise RuntimeError('incomplete/changed validation')
    prep=Path(prepared_override or state['prepared'])
    if sha(prep/'prepared.json')!=state['prepared_sha256']:raise RuntimeError('prepared identity changed')
    meta=prepared(prep,external=False)
    actual=state.get('actual_targets',[])
    required=set(state.get('required_targets',[]))
    if not actual or not required or not required<=set(meta['targets']):raise RuntimeError('empty/missing required validation set')
    seen=set()
    for command in state['commands']:
        key=(command['variant'],command['target'])
        if key in seen or list(key) not in actual or command['returncode']!=0:raise RuntimeError('duplicate/unexpected/failed validation command')
        seen.add(key)
        variant,target=key
        if variant not in meta['variants'] or target not in meta['targets']:raise RuntimeError('unprepared validation target')
        argv=[str(Path(state['prepared'])/variant/target)]
        if target=='model':argv += [str(state['operations']),str(state['seeds'])]
        if target in ['core','wire','index','base']:argv += [variant]
        if target=='wire':argv += [str(state['operations']),str(state['seeds'])]
        if argv!=command['argv']:raise RuntimeError('validation command mismatch')
        path=out/command['log']
        if not path.is_file() or sha(path)!=command['log_sha256']:raise RuntimeError('validation log missing/changed')
    if seen!=set(map(tuple,actual)):raise RuntimeError('validation command inventory mismatch')
    selected=state.get('selected_variants',meta['variants'])
    if not selected or not set(selected)<=set(meta['variants']):raise RuntimeError('invalid validation variant inventory')
    if set(c[0] for c in seen)!=set(selected):raise RuntimeError('validation variant inventory mismatch')
    for variant in selected:
        applicable={t for t in required if t not in ['core','wire','index','base','tail'] or (t=='base') or (t=='tail' and variant=='F') or (t=='core' and variant in ['F','A-core','B-core']) or (t=='wire' and variant in ['F','A-wire','B-wire']) or (t=='index' and variant in ['F','C-linear','C-index'])}
        if not {(variant,t) for t in applicable}<=seen:raise RuntimeError('mandatory per-variant validation missing')
    return {'status':'audited','commands':len(seen)}

def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='mode',required=True)
    q=sub.add_parser('prepare');q.add_argument('--out',type=Path,required=True);q.add_argument('--source',type=Path,default=ROOT/'build/stage3-baseline/source');q.add_argument('--variants',default=','.join(VARIANTS));q.add_argument('--targets',default='model,failures,micro,diag,fragment,trace,policies,tail');q.add_argument('--compiler',default='/usr/local/bin/clang++');q.add_argument('--sdk',default='/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk');q.add_argument('--library',default='/usr/local/lib');q.add_argument('--deps',type=Path,default=ROOT/'external')
    q=sub.add_parser('validate');q.add_argument('--prepared',type=Path,required=True);q.add_argument('--out',type=Path,required=True);q.add_argument('--operations',type=int,default=10000);q.add_argument('--seeds',type=int,default=8);q.add_argument('--targets');q.add_argument('--variants')
    q=sub.add_parser('window');q.add_argument('--prepared',type=Path,required=True);q.add_argument('--out',type=Path,required=True);q.add_argument('--before',default='F');q.add_argument('--after',default='F');q.add_argument('--suite',choices=['micro','fragment','trace','anchors','containers'],default='micro');q.add_argument('--filter')
    q=sub.add_parser('audit');q.add_argument('out',type=Path)
    q=sub.add_parser('audit-validation');q.add_argument('out',type=Path)
    a=p.parse_args()
    if a.mode=='prepare':prepare(a)
    elif a.mode=='validate':validate(a)
    elif a.mode=='window':window(a)
    elif a.mode=='audit-validation':print(json.dumps(audit_validation(a.out),indent=2))
    else:print(json.dumps(audit(a.out),indent=2))
if __name__=='__main__':
    try:main()
    except Exception as e:print(str(e),file=sys.stderr);sys.exit(1)
