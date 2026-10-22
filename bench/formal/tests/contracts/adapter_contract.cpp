// Compile once per adapter using the same SC_ADAPTER_HEADER/TYPE as the driver.
#include "adapter.hpp"
#include SC_ADAPTER_HEADER
#include <iostream>

namespace {
using A=SC_ADAPTER_TYPE;
void need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F&& fn){bool rejected=false;try{fn();}catch(...){rejected=true;}need(rejected,"expected operation rejection");}
sc::Mutation erase(std::uint64_t seq,std::uint64_t id,sc::Status expected){sc::Mutation m;m.sequence=seq;m.stage=7;m.code=sc::MutationCode::erase_record;m.id=id;m.expected=expected;return m;}
void contracts(){
    sc::Snapshot initial;initial.epoch=7;
    for(std::uint64_t id=1;id<=3;++id){sc::Record r;r.id=id;r.score=-std::int64_t(id);r.name=id==1?"\xc3\xa9\xf0\x9f\x98\x80":"";r.payload={0,255};r.attrs={{"",{}},{"z",{0,255}}};initial.records.push_back(r);}
    auto state=A::build(initial);need(A::export_state(state)==initial,"native build/export");
    auto native_wire=A::emit(state);auto span=sc::wire_view(native_wire);sc::Bytes input(span.begin(),span.end());
    state={};state=A::load(input);std::fill(input.begin(),input.end(),0xa5);sc::Bytes{}.swap(input);
    need(A::export_state(state)==initial,"detached input lifetime");
    std::vector<sc::Query> qs={{0,sc::QueryCode::record,1,{}},{1,sc::QueryCode::attr,1,""},{2,sc::QueryCode::record,9,{}},{3,sc::QueryCode::scan,0,{}}};
    std::vector<sc::QueryResult> values(qs.size());A::query(state,qs,values);need(values==sc::query_reference(initial,qs),"native query boundaries");
    std::vector<sc::Mutation> operations={erase(0,2,sc::Status::ok),erase(1,2,sc::Status::not_found),erase(2,99,sc::Status::not_found)};
    A::mutate(state,operations);auto expected=initial;for(auto& m:operations)need(sc::apply_reference(expected,m)==m.expected,"reference deletion");
    need(A::export_state(state)==expected,"bulk erase preserves sequential statuses and stable order");
    auto missing=erase(0,999,sc::Status::ok);rejects([&]{A::mutate(state,std::span(&missing,1));});
    need(A::export_state(state)==expected,"rejected missing erase state");
    sc::Mutation append;append.sequence=0;append.stage=8;append.code=sc::MutationCode::append_record;append.id=4;append.record.id=4;append.record.name="new";
    A::mutate(state,std::span(&append,1));need(sc::apply_reference(expected,append)==sc::Status::ok,"append reference");
    need(A::export_state(state)==expected,"native append");rejects([&]{A::mutate(state,std::span(&append,1));});
    sc::Mutation names;names.stage=2;names.code=sc::MutationCode::replace_name;names.id=1;names.text=std::string(128,'x');
    A::mutate(state,std::span(&names,1));sc::apply_reference(expected,names);
    sc::Mutation samples;samples.stage=3;samples.code=sc::MutationCode::append_samples;samples.id=1;samples.samples={0,0xffffffff};
    A::mutate(state,std::span(&samples,1));sc::apply_reference(expected,samples);
    sc::Mutation attr;attr.stage=6;attr.code=sc::MutationCode::insert_attr;attr.id=1;attr.text="\xc3\xa9";attr.bytes={0,128,255};
    A::mutate(state,std::span(&attr,1));sc::apply_reference(expected,attr);
    need(A::export_state(state)==expected,"variable-length/UTF8 native mutation");
    auto output=A::emit(state);state={};auto restored=A::load(sc::wire_view(output));
    need(A::export_state(restored)==expected,"independent native checkpoint");restored={};
    auto empty=A::build(sc::Snapshot{});auto ewire=A::emit(empty);empty={};auto eload=A::load(sc::wire_view(ewire));need(A::export_state(eload)==sc::Snapshot{},"empty state");
}
}
int main(){try{contracts();std::cout<<A::library_id<<" adapter contracts passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}catch(...){std::cerr<<"non-standard adapter error\n";return 1;}}
