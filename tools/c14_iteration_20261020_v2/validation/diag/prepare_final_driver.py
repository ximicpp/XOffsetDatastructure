"""Materialize the one final diagnostic driver, no compile/native execution.

Root invokes this source-only stage under the original global lease. All three
roles are derived before either C native run; M2 shares the exact final tools.
"""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import shutil

HERE=Path(__file__).resolve().parent
HOOK='55221d97734679cfda69cef3f590b80574e54b313afbfa6918316d6d477ed689'
DRIVER='5c6aa00b666a2f998318af731a87e67bb5fec89c264f77fe87df2690e019f2db'
ADAPTER='335114f010466a453bf56cf9aed23c77bebd48b7288828e5df41753e5c1d4766'
OUT=Path('/root/xoffset-c14-iteration/20261020/v2/validation/common-diag-final-v1')

def fid(p):
    p=Path(p);return {'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
def require(v,m):
    if not v:raise ValueError(m)
def once(text,before,after):
    require(text.count(before)==1,'unsupported driver context (must be unique): '+before[:100]);return text.replace(before,after,1)
def inventory(root):return {p.relative_to(root).as_posix():fid(p) for p in sorted(root.rglob('*')) if p.is_file()}
def write(p,value):
    with p.open('x',encoding='utf-8',newline='\n') as f:json.dump(value,f,sort_keys=True,indent=2);f.write('\n')

HELPERS=r'''
void diag_emit_flush(){
    const auto& e=c14_diag::emit;
    need(e.invocations==1&&e.seen==511,"diagnostic owning emit invocation/seen mismatch");
    for(unsigned i=0;i<9;++i){const auto& s=e.at[i];need(!s.clock_status&&!s.rusage_status&&s.ns,"emit sample syscall failed");if(i)need(s.ns>=e.at[i-1].ns,"emit sample clock reversed");}
    const std::uint64_t p0[]={e.seen,e.invocations,e.at[0].ns,e.at[8].ns};c14_event::event("emit.pod0",p0);
    const std::uint64_t p1[]={e.at[3].ns,e.at[4].ns,e.at[5].ns,e.at[6].ns};c14_event::event("emit.pod1",p1);
    const std::uint64_t p2[]={e.at[0].minor,e.at[4].minor,e.at[6].minor,e.at[8].minor};c14_event::event("emit.pod2",p2);
    std::ostringstream z;z<<"{\"kind\":\"c14-emit-samples-v1\",\"seen\":"<<e.seen<<",\"invocations\":"<<e.invocations<<",\"samples\":[";
    for(unsigned i=0;i<9;++i){if(i)z<<',';const auto& s=e.at[i];z<<"{\"index\":"<<i<<",\"ns\":"<<s.ns<<",\"utime_us\":"<<s.utime_us<<",\"stime_us\":"<<s.stime_us<<",\"minor\":"<<s.minor<<",\"major\":"<<s.major<<",\"bytes\":"<<s.bytes<<",\"clock_status\":"<<s.clock_status<<",\"rusage_status\":"<<s.rusage_status<<'}';}
    z<<"],\"event_errors\":"<<c14_event::state.errors<<",\"last_errno\":"<<c14_event::state.last_error<<",\"channel_clock_status\":"<<c14_event::state.clock_status<<",\"mask_status\":"<<c14_event::state.mask_status<<",\"restore_status\":"<<c14_event::state.restore_status<<"}\n";c14_event::json(z.str());need(c14_event::good(),"diagnostic emit flush/channel failed");
}
void diag_counts(std::ostream& z,const c14_diag::Counts& c){
    z<<"{\"canonical\":"<<c.canonical<<",\"inactive\":"<<c.inactive<<",\"active\":"<<c.active<<",\"owns\":"<<c.owns<<",\"vectors\":"<<c.vectors<<",\"capacity_hits\":"<<c.capacity_hits<<",\"nontrivial_misses\":"<<c.nontrivial_misses<<",\"nonempty_misses\":"<<c.nonempty_misses<<",\"vector_data_reads\":"<<c.vector_data_reads<<'}';
}
void diag_counts_flush(){
    need(!c14_diag::enabled&&c14_diag::mutation_seen==1022,"primary nine mutation groups not observed");
    std::ostringstream z;z<<"{\"kind\":\"c14-common-counts-v1\",\"mutation_seen\":"<<c14_diag::mutation_seen<<",\"emit_seen\":"<<c14_diag::emit.seen<<",\"emit_invocations\":"<<c14_diag::emit.invocations<<",\"counts\":";diag_counts(z,c14_diag::counts);
    z<<",\"phases\":[";for(unsigned i=0;i<9;++i){if(i)z<<',';diag_counts(z,c14_diag::phases[i]);}
    z<<"],\"mutations\":[";for(unsigned i=0;i<10;++i){if(i)z<<',';diag_counts(z,c14_diag::mutations[i]);}
    z<<"],\"event_errors\":"<<c14_event::state.errors<<",\"last_errno\":"<<c14_event::state.last_error<<",\"channel_clock_status\":"<<c14_event::state.clock_status<<",\"mask_status\":"<<c14_event::state.mask_status<<",\"restore_status\":"<<c14_event::state.restore_status<<"}\n";c14_event::json(z.str());need(c14_event::good(),"diagnostic count flush/channel failed");
}
// Complete independent replay after t8. There is deliberately no owning emit.
void diag_truth_replay(const Opts& o,const sc::Trace& trace){
    need(!c14_diag::enabled&&trace.stages.size()==9,"post-primary replay precondition");
    const auto first=c14_diag::sample();need(!first.clock_status&&!first.rusage_status&&first.ns,"replay start sample failed");
    auto original=read(arg(o,"--wire"));auto replay=A::load(original);std::fill(original.begin(),original.end(),0xa5);sc::Bytes{}.swap(original);
    {auto initial=A::export_state(replay);check_trace(trace,initial);}
    std::vector<sc::QueryResult> answers(trace.queries.size());A::query(replay,trace.queries,answers);need(answers==trace.before,"replay initial adapter query mismatch");
    std::size_t next=0,stage_calls=0;std::ostringstream z;z<<"{\"kind\":\"c14-nine-stage-truth-v1\",\"status\":\"passed\",\"stages\":[";
    for(std::size_t i=0;i<9;++i){const auto& expected=trace.stages[i];need(expected.stage==i+1,"trace stages not consecutive1..9");const auto start=next;
        while(next<trace.mutations.size()&&trace.mutations[next].stage==expected.stage)++next;
        need(next-start==expected.mutation_count,"replay stage operation count mismatch");
        A::mutate(replay,std::span(trace.mutations).subspan(start,next-start));auto canonical=sc::encode_canonical(A::export_state(replay));
        ++stage_calls;
        need(sc::sha256(canonical)==expected.canonical_digest,"replay stage canonical mismatch");if(i)z<<',';z<<quote(sc::sha256_hex(canonical));
    }
    need(next==trace.mutations.size(),"replay unconsumed mutations");A::query(replay,trace.queries,answers);need(answers==trace.after,"replay final adapter query mismatch");replay={};
    const auto last=c14_diag::sample();need(!last.clock_status&&!last.rusage_status&&last.ns>=first.ns,"replay end sample failed");
    need(last.utime_us>=first.utime_us&&last.stime_us>=first.stime_us&&last.minor>=first.minor&&last.major>=first.major,"replay cumulative resource reversed");
    z<<"],\"query_before\":true,\"query_after\":true,\"owning_emit_calls\":0,\"work\":{\"input_reads\":1,\"loads\":1,\"initial_exports\":1,\"initial_trace_check\":1,\"stage_mutate_calls\":"<<stage_calls<<",\"stage_exports\":"<<stage_calls<<",\"stage_hashes\":"<<stage_calls<<",\"mutation_operations\":"<<next<<",\"adapter_query_calls\":2,\"queries_per_call\":"<<trace.queries.size()<<"},\"replay_resources_before_final_JSON_flush\":{\"start_ns\":"<<first.ns<<",\"end_ns\":"<<last.ns<<",\"user_us\":"<<last.utime_us-first.utime_us<<",\"system_us\":"<<last.stime_us-first.stime_us<<",\"minor_faults\":"<<last.minor-first.minor<<",\"major_faults\":"<<last.major-first.major<<",\"start_clock_status\":"<<first.clock_status<<",\"start_rusage_status\":"<<first.rusage_status<<",\"end_clock_status\":"<<last.clock_status<<",\"end_rusage_status\":"<<last.rusage_status<<"}}\n";c14_event::json(z.str());need(c14_event::good(),"diagnostic replay flush/channel failed");
}
'''

MAIN=r'''int main(int argc,char** argv){
    c14_event::event("main.begin");
    try{
        if(argc<2||std::string_view(argv[1])!="run")throw std::runtime_error("diagnostic accepts only original run");Opts o;
        for(int i=2;i<argc;i+=2){if(i+1>=argc||!o.emplace(argv[i],argv[i+1]).second)throw std::runtime_error("missing/duplicate option");}
        run(o);c14_event::event("main.end");need(c14_event::good(),"diagnostic final channel failed");return 0;
    }catch(const std::exception& e){std::cerr<<"serialization comparison rejected: "<<e.what()<<'\n';return 2;}
}
'''

def derive(driver):
    driver=once(driver,'#include "adapter.hpp"','#include "adapter.hpp"\n#include "event_channel.hpp"')
    driver=once(driver,'void write(const std::string& p,std::span<const std::uint8_t> b){',
        'std::size_t diag_body_bytes=0;unsigned diag_body_files=0;\nvoid write(const std::string& p,std::span<const std::uint8_t> b){\n'
        '    constexpr std::size_t limit=64ULL*1024*1024;\n'
        '    if(diag_body_files>=4||diag_body_bytes>limit||b.size()>limit-diag_body_bytes)throw std::runtime_error("diagnostic64MiB body budget exceeded before fopen");\n'
        '    diag_body_bytes+=b.size();++diag_body_files;')
    start=driver.index('void validate(const Opts& o){');end=driver.index('void run(const Opts& o){',start)
    driver=driver[:start]+HELPERS+'\n'+driver[end:]
    driver=once(driver,'    auto input=read(arg(o,"--wire"));auto trace_bytes=read(arg(o,"--trace"));const auto trace=sc::decode_trace(trace_bytes);',
        '    c14_event::event("input.read.before");auto input=read(arg(o,"--wire"));auto trace_bytes=read(arg(o,"--trace"));c14_event::event("input.read.after");\n'
        '    c14_event::event("input.decode.before");const auto trace=sc::decode_trace(trace_bytes);c14_event::event("input.decode.after");')
    driver=once(driver,'    const auto input_sha=sc::sha256_hex(input),trace_sha=sc::sha256_hex(trace_bytes);',
        '    c14_event::event("input.hash.before");const auto input_sha=sc::sha256_hex(input),trace_sha=sc::sha256_hex(trace_bytes);c14_event::event("input.hash.after");')
    driver=once(driver,'const auto t0=Clock::now();','const auto t0=Clock::now();c14_event::event("t0");')
    for i in range(1,8):
        before=f'const auto t{i}=Clock::now();c14_diag::phase({i});'
        after=before+f'c14_event::event("t{i}");'
        if i==4:after+='diag_emit_flush();'
        driver=once(driver,before,after)
    driver=once(driver,'c14_diag::enabled=false;auto primary_usage=usage();','c14_diag::enabled=false;auto primary_usage=usage();c14_event::event("t8");diag_counts_flush();')
    driver=once(driver,'    auto audited=A::load(sc::wire_view(final_wire));sc::Bytes canonical;std::uint64_t record_count=0;',
        '    c14_event::event("postaudit.load.before");auto audited=A::load(sc::wire_view(final_wire));c14_event::event("postaudit.load.after");sc::Bytes canonical;std::uint64_t record_count=0;')
    driver=once(driver,'    {auto model=A::export_state(audited);record_count=model.records.size();canonical=sc::encode_canonical(model);}audited={};',
        '    c14_event::event("postaudit.export.before");{auto model=A::export_state(audited);record_count=model.records.size();canonical=sc::encode_canonical(model);}audited={};c14_event::event("postaudit.export.after");')
    driver=once(driver,'    need(!trace.stages.empty()&&sc::sha256(canonical)==trace.stages.back().canonical_digest,"post-audit canonical mismatch");',
        '    c14_event::event("postaudit.hash.before");need(!trace.stages.empty()&&sc::sha256(canonical)==trace.stages.back().canonical_digest,"post-audit canonical mismatch");c14_event::event("postaudit.hash.after");\n'
        '    c14_event::event("truth.replay.before");diag_truth_replay(o,trace);c14_event::event("truth.replay.after");')
    original='    write(arg(o,"--wire-out"),sc::wire_view(final_wire));write(arg(o,"--canonical-out"),canonical);write(arg(o,"--queries-out"),queries);write(arg(o,"--result"),result.str());'
    final=[]
    for key,opt,value in (('wire','--wire-out','sc::wire_view(final_wire)'),('canonical','--canonical-out','canonical'),('queries','--queries-out','queries'),('result','--result','result.str()')):
        final.append(f'    c14_event::event("write.{key}.before");write(arg(o,"{opt}"),{value});c14_event::event("write.{key}.after");')
    driver=once(driver,original,'\n'.join(final)+'\n    need(diag_body_files==4,"diagnostic four body writes required");')
    main=driver.index('int main(int argc,char** argv){');driver=driver[:main]+MAIN
    require(driver.count('A::emit(')==1 and 'validate(o)' not in driver,'unexpected second owning emit/validate')
    return driver

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--request',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    root=a.root.resolve();require(a.out==OUT and not a.out.exists(),'only fresh common-diag-final-v1 output allowed')
    request=json.loads(a.request.read_text(encoding='utf-8-sig'));require(request['role_order']==['c14','r1'] and request['compile_budget']==2 and request['native_budget']==2,'unexpected diagnostic proposal')
    require(request['validation_must_complete_before_final_freeze'],'missing original validation obligations')
    material_path=root/'handoff/github-20261019/C14_MATERIALIZATION.json';material=json.loads(material_path.read_text());names=list(material['non_external_files']);require(len(names)==154,'154-member materialization required')
    prepared_root=root/'build/c14-iteration-20261020-v2/diag-prepared';plans={};inputs={str(a.request):fid(a.request),str(material_path):fid(material_path)}
    for role in ('c14','r1','m2'):
        src=prepared_root/(role+'-v2');identity_path=src/'DIAGNOSTIC_IDENTITY.json';identity=json.loads(identity_path.read_text())
        for name,declared in identity['generated'].items():
            f=src/Path(name.replace('\\','/'));require(fid(f)==declared,'prepared generated input changed: '+str(f));inputs[str(f)]=fid(f)
            require(request['input_files'][str(f)]==fid(f),'draft does not pin actual prepared member: '+str(f))
        require(identity['hook_sha256']==HOOK and fid(src/'source/c14_common_diag.hpp')['sha256']==HOOK,'unsupported hook revision')
        require(fid(src/'experiment/driver.cpp')['sha256']==DRIVER and fid(src/'experiment/adapters/xoffset/adapter.hpp')['sha256']==ADAPTER,'unsupported driver/adapter revision')
        require(request['input_files'][str(identity_path)]==fid(identity_path),'unbound prepared identity');inputs[str(identity_path)]=fid(identity_path)
        base=root/Path(identity['input_source'].replace('\\','/'))
        source_members={n:fid(src/'source'/n) for n in names};base_members={n:fid(base/n) for n in names}
        for n in names:
            if n!='xoffsetdatastructure.hpp':require(source_members[n]==base_members[n],'unexplained prepared source difference: '+role+'/'+n)
        require(set(inventory(src/'source'))==set(names)|{'c14_common_diag.hpp'},'prepared source has unexpected members')
        expected_prepared={'source/'+n for n in names}|{'source/c14_common_diag.hpp','experiment/driver.cpp','experiment/adapters/xoffset/adapter.hpp','DIAGNOSTIC_IDENTITY.json'}
        require(set(inventory(src))==expected_prepared,'prepared generation contains unregistered files')
        original=(src/'experiment/driver.cpp').read_text();driver=derive(original)
        plans[role]={'prepared':src,'identity':identity,'source_members':source_members,'base':base,'base_members':base_members,'driver':driver,'original':original}
    require(len({v['driver'] for v in plans.values()})==1,'roles do not share final driver')
    tools={str(f):fid(f) for f in (Path(__file__).resolve(),HERE/'event_channel.hpp',HERE/'INTERFACE.json')}
    require(fid(HERE/'INTERFACE.json')['sha256']=='7bbba10ea8351e6a3af576de7cbe8963c9733b5e144075444427b0fe7cddf20c','unsupported fixed36 event contract')
    a.out.mkdir(parents=True)
    records={}
    for role,plan in plans.items():
        dest=a.out/role;src=plan['prepared'];shutil.copytree(src,dest)
        (dest/'experiment/driver.cpp').write_text(plan['driver'],encoding='utf-8',newline='\n')
        shutil.copyfile(HERE/'event_channel.hpp',dest/'experiment/event_channel.hpp')
        diff=''.join(difflib.unified_diff(plan['original'].splitlines(True),plan['driver'].splitlines(True),fromfile='prepared/experiment/driver.cpp',tofile='final/experiment/driver.cpp'))
        (dest/'DRIVER_DIFF.patch').write_text(diff,encoding='utf-8',newline='\n')
        require({n:fid(dest/'source'/n) for n in names}==plan['source_members'],'copied154 source identity changed')
        require({n:fid(src/'source'/n) for n in names}==plan['source_members'],'prepared154 source changed during copy')
        require({n:fid(plan['base']/n) for n in names}==plan['base_members'],'source origin changed during copy')
        generated=inventory(dest);records[role]={'source':str(dest),'prepared_source':str(src),'prepared_identity':plan['identity'],'source_154':plan['source_members'],'production_14':{n:plan['source_members'][n] for n in material['production_files']},'files':generated}
        records[role].update(source_origin=str(plan['base']),source_origin_154=plan['base_members'])
        write(dest/'FINAL_SOURCE_IDENTITY.json',{'schema':'c14-final-diag-source-v1','role':role,'inputs':inputs,'tools':tools,'record':records[role],'diagnostic_only':True,'eligible_for_performance':False,'compile_count':0,'native_count':0})
    require(all(fid(p)==v for p,v in inputs.items()),'input changed during materialization')
    require(all(fid(p)==v for p,v in tools.items()),'tool changed during materialization')
    write(a.out/'MATERIALIZATION.json',{'status':'materialized_not_compiled_or_admitted','inputs':inputs,'tools':tools,'roles':records,'same_driver_adapter_hook_event_header':True,'emit_calls_per_run':1,'truth_replay_emit_calls':0,'per_role_producer_body_cap':64*1024**2,'both_C_roles_body_cap':128*1024**2,'compile_count':0,'native_count':0})
    print(json.dumps({'status':'materialized_not_compiled_or_admitted','out':str(a.out),'roles':list(records),'Cxx_compiles':0,'native_runs':0}))

if __name__=='__main__':main()
