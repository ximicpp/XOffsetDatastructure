#include "adapter.hpp"
#include SC_ADAPTER_HEADER
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <sys/resource.h>

namespace {
using A=SC_ADAPTER_TYPE;
using Clock=std::chrono::steady_clock;
using Opts=std::map<std::string,std::string>;
sc::Bytes read(const std::string& p){
    const auto n=std::filesystem::file_size(p);
    if(n>sc::wire_limit)throw std::runtime_error("input exceeds 512 MiB");
    std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("cannot open input");
    sc::Bytes b(static_cast<std::size_t>(n));
    if(n&&!f.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(n)))throw std::runtime_error("short input read");return b;
}
void write(const std::string& p,std::span<const std::uint8_t> b){
    auto* f=std::fopen(p.c_str(),"wbx");if(!f)throw std::runtime_error("output exists or cannot create: "+p);
    const bool good=b.empty()||std::fwrite(b.data(),1,b.size(),f)==b.size();auto rc=std::fclose(f);
    if(!good||rc)throw std::runtime_error("output write failed: "+p);
}
void write(const std::string& p,const std::string& s){write(p,{reinterpret_cast<const std::uint8_t*>(s.data()),s.size()});}
std::string arg(const Opts& o,const std::string& k){auto i=o.find(k);if(i==o.end())throw std::runtime_error("missing option "+k);return i->second;}
void allow(const Opts& o,std::initializer_list<std::string_view> keys){for(const auto& [k,v]:o){(void)v;if(std::find(keys.begin(),keys.end(),k)==keys.end())throw std::runtime_error("unknown option "+k);}}
std::string quote(std::string_view s){std::string z="\"";for(unsigned char c:s){if(c=='"'||c=='\\')z+='\\';if(c<32)throw std::runtime_error("control character in identity");z+=char(c);}return z+'"';}
void need(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
std::uint64_t duration(Clock::time_point a,Clock::time_point b){return std::chrono::duration_cast<std::chrono::nanoseconds>(b-a).count();}
std::uint64_t cpu(const rusage& r){return std::uint64_t(r.ru_utime.tv_sec+r.ru_stime.tv_sec)*1000000000ULL+std::uint64_t(r.ru_utime.tv_usec+r.ru_stime.tv_usec)*1000ULL;}
rusage usage(){rusage r{};if(getrusage(RUSAGE_SELF,&r))throw std::runtime_error("getrusage failed");return r;}
void check_trace(const sc::Trace& t,const sc::Snapshot& s){
    need(t.stages.size()==9,"trace must contain nine stages");
    need(sc::sha256(sc::encode_canonical(s))==t.input_digest,"trace/input logical identity mismatch");
    need(sc::query_reference(s,t.queries)==t.before,"independent initial query mismatch");
}
void validate(const Opts& o){
    allow(o,{"--wire","--trace","--result"});auto input=read(arg(o,"--wire"));auto trace=sc::decode_trace(read(arg(o,"--trace")));
    auto state=A::load(input);std::fill(input.begin(),input.end(),0xa5);sc::Bytes{}.swap(input);
    auto initial=A::export_state(state);check_trace(trace,initial);
    std::vector<sc::QueryResult> q(trace.queries.size());A::query(state,trace.queries,q);need(q==trace.before,"adapter initial query mismatch");
    std::size_t next=0;std::ostringstream digests;digests<<'[';
    for(std::size_t si=0;si<trace.stages.size();++si){const auto& expected=trace.stages[si];auto start=next;
        while(next<trace.mutations.size()&&trace.mutations[next].stage==expected.stage)++next;
        need(next-start==expected.mutation_count,"stage operation count mismatch");
        A::mutate(state,std::span(trace.mutations).subspan(start,next-start));
        auto canonical=sc::encode_canonical(A::export_state(state));
        need(sc::sha256(canonical)==expected.canonical_digest,"adapter stage canonical mismatch");
        if(si)digests<<',';digests<<quote(sc::sha256_hex(canonical));
    }
    digests<<']';need(next==trace.mutations.size(),"unconsumed mutations");
    A::query(state,trace.queries,q);need(q==trace.after,"adapter final query mismatch");
    auto final_wire=A::emit(state);state={};auto reloaded=A::load(sc::wire_view(final_wire));
    need(sc::sha256(sc::encode_canonical(A::export_state(reloaded)))==trace.stages.back().canonical_digest,"validation reload mismatch");
    reloaded={};
    write(arg(o,"--result"),"{\"schema_version\":1,\"kind\":\"serialization-comparison-validation\",\"library\":"+quote(A::library_id)+",\"status\":\"passed\",\"stages\":"+digests.str()+"}\n");
}
void run(const Opts& o){
    allow(o,{"--wire","--trace","--wire-out","--canonical-out","--queries-out","--result"});
    auto input=read(arg(o,"--wire"));auto trace_bytes=read(arg(o,"--trace"));const auto trace=sc::decode_trace(trace_bytes);
    const auto input_sha=sc::sha256_hex(input),trace_sha=sc::sha256_hex(trace_bytes);
    // Functional validation already proved this input/trace pair; no DTO or native
    // state, query answer or final wire is prepared before the primary timer.
    std::vector<sc::QueryResult> before(trace.queries.size()),after(trace.queries.size());
    auto initial_usage=usage();const auto t0=Clock::now();
    auto state=A::load(input);sc::Bytes{}.swap(input);const auto t1=Clock::now();
    A::query(state,trace.queries,before);const auto t2=Clock::now();
    A::mutate(state,trace.mutations);const auto t3=Clock::now();
    auto final_wire=A::emit(state);const auto t4=Clock::now();
    state={};const auto t5=Clock::now();
    auto reloaded=A::load(sc::wire_view(final_wire));const auto t6=Clock::now();
    A::query(reloaded,trace.queries,after);const auto t7=Clock::now();
    reloaded={};const auto t8=Clock::now();auto primary_usage=usage();
    need(before==trace.before&&after==trace.after,"timed adapter query result mismatch");
    // Post-audit depends solely on published owning bytes. No old state survives.
    auto audited=A::load(sc::wire_view(final_wire));sc::Bytes canonical;std::uint64_t record_count=0;
    {auto model=A::export_state(audited);record_count=model.records.size();canonical=sc::encode_canonical(model);}audited={};
    need(!trace.stages.empty()&&sc::sha256(canonical)==trace.stages.back().canonical_digest,"post-audit canonical mismatch");
    auto queries=sc::encode_query_pair(before,after);auto qb=sc::encode_query_results(before),qa=sc::encode_query_results(after);auto all_usage=usage();
    auto result=std::ostringstream{};
    result<<"{\"schema_version\":1,\"kind\":\"serialization-comparison-native-result\",\"library\":"<<quote(A::library_id)<<",\"status\":\"passed\",\"phase_ns\":{";
    const char* names[]={"load","query_before","mutate","emit_after","retire_modified_state","reload","query_after","cleanup"};
    Clock::time_point points[]={t0,t1,t2,t3,t4,t5,t6,t7,t8};
    for(int i=0;i<8;++i){if(i)result<<',';result<<quote(names[i])<<':'<<quote(std::to_string(duration(points[i],points[i+1])));}
    result<<"},\"total_ns\":"<<quote(std::to_string(duration(t0,t8)));
    auto field=[&](std::string_view k,const std::string& v){result<<','<<quote(k)<<':'<<quote(v);};
    field("input_sha256",input_sha);field("trace_sha256",trace_sha);field("final_wire_sha256",sc::sha256_hex(sc::wire_view(final_wire)));field("final_canonical_sha256",sc::sha256_hex(canonical));
    field("queries_sha256",sc::sha256_hex(queries));field("query_before_sha256",sc::sha256_hex(qb));field("query_after_sha256",sc::sha256_hex(qa));
    field("peak_rss_before_audit_kib",std::to_string(primary_usage.ru_maxrss));field("peak_rss_after_audit_kib",std::to_string(all_usage.ru_maxrss));
    field("cpu_ns",std::to_string(cpu(primary_usage)-cpu(initial_usage)));field("minor_faults",std::to_string(primary_usage.ru_minflt-initial_usage.ru_minflt));
    field("major_faults",std::to_string(primary_usage.ru_majflt-initial_usage.ru_majflt));field("record_count",std::to_string(record_count));result<<"}\n";
    write(arg(o,"--wire-out"),sc::wire_view(final_wire));write(arg(o,"--canonical-out"),canonical);write(arg(o,"--queries-out"),queries);write(arg(o,"--result"),result.str());
}
void stagesnap(const Opts& o){
    // DIAGNOSTIC ONLY: snapshot after stages<k, then loop [load snapshot, stage k] for profilers.
    allow(o,{"--wire","--trace","--stage","--iters"});auto input=read(arg(o,"--wire"));const auto trace=sc::decode_trace(read(arg(o,"--trace")));
    const auto k=std::stoul(arg(o,"--stage"));const auto iters=std::stoul(arg(o,"--iters"));
    std::vector<std::pair<std::size_t,std::size_t>> ranges;std::size_t next=0;
    for(const auto& e:trace.stages){auto start=next;while(next<trace.mutations.size()&&trace.mutations[next].stage==e.stage)++next;ranges.push_back({start,next-start});}
    auto state=A::load(input);for(std::size_t si=0;si<k;++si)A::mutate(state,std::span(trace.mutations).subspan(ranges[si].first,ranges[si].second));
    auto snap=A::emit(state);state={};std::uint64_t total=0;
    for(std::size_t it=0;it<iters;++it){auto st=A::load(sc::wire_view(snap));auto a=Clock::now();A::mutate(st,std::span(trace.mutations).subspan(ranges[k].first,ranges[k].second));total+=duration(a,Clock::now());}
    std::cout<<total/iters<<"\n";
}
void stageloop(const Opts& o){
    // DIAGNOSTIC ONLY: repeat [load, stages<k, stage k] for sampling profilers.
    allow(o,{"--wire","--trace","--stage","--iters"});const auto input=read(arg(o,"--wire"));const auto trace=sc::decode_trace(read(arg(o,"--trace")));
    const auto k=std::stoul(arg(o,"--stage"));const auto iters=std::stoul(arg(o,"--iters"));std::uint64_t total=0;
    for(std::size_t it=0;it<iters;++it){auto copy=input;auto state=A::load(copy);std::size_t next=0;
        for(std::size_t si=0;si<=k&&si<trace.stages.size();++si){const auto& e=trace.stages[si];auto start=next;
            while(next<trace.mutations.size()&&trace.mutations[next].stage==e.stage)++next;
            auto a=Clock::now();A::mutate(state,std::span(trace.mutations).subspan(start,next-start));if(si==k)total+=duration(a,Clock::now());}}
    std::cout<<total/iters<<"\n";
}
void stages(const Opts& o){
    // DIAGNOSTIC ONLY: per-stage mutate timing (not a formal boundary).
    allow(o,{"--wire","--trace"});auto input=read(arg(o,"--wire"));const auto trace=sc::decode_trace(read(arg(o,"--trace")));
    auto state=A::load(input);sc::Bytes{}.swap(input);std::size_t next=0;std::cout<<'[';
    for(std::size_t si=0;si<trace.stages.size();++si){const auto& e=trace.stages[si];auto start=next;
        while(next<trace.mutations.size()&&trace.mutations[next].stage==e.stage)++next;
        auto a=Clock::now();A::mutate(state,std::span(trace.mutations).subspan(start,next-start));auto b=Clock::now();
        if(si)std::cout<<',';std::cout<<duration(a,b);}
    std::cout<<"]\n";
}
}
int main(int argc,char** argv){
    try{
        if(argc<2)throw std::runtime_error("expected describe/encode/inspect/validate/run");std::string command=argv[1];Opts o;
        for(int i=2;i<argc;i+=2){if(i+1>=argc||!o.emplace(argv[i],argv[i+1]).second)throw std::runtime_error("missing/duplicate option");}
        if(command=="describe"){
            allow(o,{});std::cout<<"{\"schema_version\":1,\"library\":"<<quote(A::library_id)<<",\"commit\":"<<quote(A::commit)<<",\"trust_mode\":"<<quote(A::trust_mode)<<",\"load_contract\":\"detached\",\"capabilities\":{\"trusted_mutable\":true,\"untrusted_mutable\":\"not_established\",\"borrowed_read\":\"not_established\",\"native_graph\":\"not_established\"}}\n";
        }else if(command=="encode"){
            allow(o,{"--canonical","--wire"});auto model=sc::decode_canonical(read(arg(o,"--canonical")));auto state=A::build(model);auto wire=A::emit(state);write(arg(o,"--wire"),sc::wire_view(wire));
        }else if(command=="inspect"){
            allow(o,{"--wire","--canonical"});auto wire=read(arg(o,"--wire"));auto state=A::load(wire);std::fill(wire.begin(),wire.end(),0xa5);sc::Bytes{}.swap(wire);
            auto canonical=sc::encode_canonical(A::export_state(state));write(arg(o,"--canonical"),canonical);
        }else if(command=="validate")validate(o);
        else if(command=="run")run(o);
        else if(command=="stages")stages(o);
        else if(command=="stageloop")stageloop(o);
        else if(command=="stagesnap")stagesnap(o);
        else throw std::runtime_error("unknown command");
        return 0;
    }catch(const std::exception& e){std::cerr<<"serialization comparison rejected: "<<e.what()<<'\n';return 2;}
}
