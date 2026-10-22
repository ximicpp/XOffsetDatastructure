// Functional oracle tests only. No benchmark clock or candidate library.
#include "model.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
template<class F> void rejects(F&& f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}require(rejected,"malformed input was accepted");}
sc::Bytes read(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("missing fixture: "+path.string());return {std::istreambuf_iterator<char>(f),{}};}
void local_contracts(){
    require(sc::sha256_hex(std::string_view{})=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA empty");
    require(sc::sha256_hex("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA abc");
    require(sc::sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")=="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1","SHA two blocks");
    require(sc::sha256_hex(std::string(1000000,'a'))=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","SHA million a");
    for(std::size_t n: {0u,1u,55u,56u,63u,64u,65u,127u,128u}){
        sc::Bytes b(n,0xa5);auto digest=sc::sha256(b);require(sc::hex_digest(digest).size()==64,"SHA length");
    }
    sc::Snapshot empty;auto b=sc::encode_canonical(empty);require(b.size()==28,"empty encoded size");
    require(sc::decode_canonical(b)==empty,"empty roundtrip");
    sc::Snapshot minimal;sc::Record mr;mr.id=1;minimal.records.push_back(mr);
    auto minimal_bytes=sc::encode_canonical(minimal);require(minimal_bytes.size()==80&&sc::decode_canonical(minimal_bytes)==minimal,"minimal record lower bound 52");
    for(std::size_t n=0;n<b.size();++n)rejects([&]{sc::decode_canonical(std::span(b).first(n));});
    auto trailing=b;trailing.push_back(0);rejects([&]{sc::decode_canonical(trailing);});
    auto bad_count=b;for(unsigned i=20;i<28;++i)bad_count[i]=255;rejects([&]{sc::decode_canonical(bad_count);});
    auto bad_version=b;bad_version[8]=2;rejects([&]{sc::decode_canonical(bad_version);});
    sc::Snapshot s;sc::Record r;r.id=1;r.revision=0xffffffff;r.score=std::numeric_limits<std::int64_t>::min();
    r.name="\xc3\xa9\xf0\x9f\x98\x80";r.samples={0,0xffffffff};r.payload={0,128,255};r.attrs={{"",{}},{"z",{0,255}},{"\xc3\xa9",{1}}};s.records.push_back(r);
    auto coded=sc::encode_canonical(s);require(sc::decode_canonical(coded)==s,"signed/nonascii roundtrip");
    auto limit=sc::Limits{};limit.max_records=0;rejects([&]{sc::decode_canonical(coded,limit);});
    limit=sc::Limits{};limit.max_total_elements=1;rejects([&]{sc::decode_canonical(coded,limit);});
    auto corrupt=coded;corrupt[56]=0xff;rejects([&]{sc::decode_canonical(corrupt);});
    corrupt=coded;corrupt[56]=0;rejects([&]{sc::decode_canonical(corrupt);});
    for(auto text:{std::string("\xc0\x80"),std::string("\xed\xa0\x80"),std::string("\xf4\x90\x80\x80"),std::string("\xe2\x82")})require(!sc::wire::text_valid(text),"invalid UTF8");
    auto duplicate=s;duplicate.records.push_back(r);rejects([&]{sc::encode_canonical(duplicate);});
    auto unsorted=s;std::reverse(unsorted.records[0].attrs.begin(),unsorted.records[0].attrs.end());rejects([&]{sc::encode_canonical(unsorted);});
    std::vector<sc::Query> queries={{0,sc::QueryCode::record,1,{}},{1,sc::QueryCode::record,999,{}},{2,sc::QueryCode::attr,1,""},{3,sc::QueryCode::scan,0,{}}};
    auto q=sc::query_reference(s,queries);require(q[0].values[1]==(1ULL<<63)&&q[1].status==sc::Status::not_found&&q[2].values[0]==0,"query values/status");
    auto qb=sc::encode_query_results(q);require(sc::decode_query_results(qb)==q,"query roundtrip");
    qb[28]=2;rejects([&]{sc::decode_query_results(qb);});
}
void fixture_contract(const std::filesystem::path& dir){
    auto raw=read(dir/"input.canonical.bin"),trace_raw=read(dir/"trace.bin"),expected=read(dir/"final.canonical.bin");
    auto state=sc::decode_canonical(raw);auto trace=sc::decode_trace(trace_raw);
    require(sc::encode_canonical(state)==raw,"independent canonical encoding");
    require(sc::encode_trace(trace)==trace_raw,"independent trace encoding");
    require(sc::sha256(raw)==trace.input_digest,"input binding");
    auto before=sc::query_reference(state,trace.queries);
    require(before==trace.before,"before oracle");
    require(sc::encode_query_results(before)==read(dir/"query-before.bin"),"before encoding");
    std::size_t index=0;
    for(auto& stage:trace.stages){
        std::size_t count=0;
        while(index<trace.mutations.size()&&trace.mutations[index].stage==stage.stage){
            auto& m=trace.mutations[index++];require(sc::apply_reference(state,m)==m.expected,"mutation status");++count;
        }
        require(count==stage.mutation_count,"stage count");
        require(sc::sha256(sc::encode_canonical(state))==stage.canonical_digest,"stage digest");
    }
    require(index==trace.mutations.size()&&sc::encode_canonical(state)==expected,"final complete state");
    auto after=sc::query_reference(state,trace.queries);require(after==trace.after,"after oracle");
    require(sc::encode_query_results(after)==read(dir/"query-after.bin"),"after encoding");
    require(sc::decode_canonical(expected)==state,"final independent reload");
    auto qbad=trace_raw;qbad[52]=0xff;rejects([&]{sc::decode_trace(qbad);});
    auto extra=trace_raw;extra.push_back(0);rejects([&]{sc::decode_trace(extra);});
    auto limit=sc::Limits{};limit.max_queries=0;rejects([&]{sc::decode_trace(trace_raw,limit);});
    std::cout<<"fixture "<<dir.filename().string()<<" input="<<sc::sha256_hex(raw)<<" final="<<sc::sha256_hex(expected)<<" pair="<<sc::sha256_hex(sc::encode_query_pair(before,after))<<"\n";
}
}
int main(int argc,char** argv){try{local_contracts();for(int i=1;i<argc;++i)fixture_contract(argv[i]);std::cout<<"model contracts passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
