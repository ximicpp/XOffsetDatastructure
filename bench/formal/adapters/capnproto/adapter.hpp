#pragma once
#include "views.hpp"
#include "snapshot.capnp.h"
#include <capnp/message.h>
#include <capnp/orphan.h>
#include <capnp/serialize.h>
#include <cstring>
#include <unordered_set>

namespace sc {
namespace cap_view {
inline std::string_view text(capnp::Text::Reader s){return {s.cStr(),s.size()};}
inline std::span<const std::uint8_t> data(capnp::Data::Reader b){return {b.begin(),b.size()};}
struct AttrView {
    sc_cap::Attr::Reader a;
    std::string_view key()const{return text(a.getKey());}
    auto value()const{return data(a.getValue());}
};
struct RecordView {
    sc_cap::Record::Reader r;
    auto id()const{return r.getId();}auto revision()const{return r.getRevision();}auto score()const{return r.getScore();}
    auto name()const{return text(r.getName());}auto samples()const{return r.getSamples();}auto payload()const{return data(r.getPayload());}
    auto attrs_size()const{return r.getAttrs().size();}auto attr(std::size_t i)const{return AttrView{r.getAttrs()[static_cast<unsigned>(i)]};}
};
struct SnapshotView {
    sc_cap::Snapshot::Reader s;
    auto schema_version()const{return s.getSchemaVersion();}auto epoch()const{return s.getEpoch();}auto size()const{return s.getRecords().size();}
    auto record(std::size_t i)const{return RecordView{s.getRecords()[static_cast<unsigned>(i)]};}
};
inline unsigned count(std::size_t n,std::uint64_t limit){if(n>limit||n>std::numeric_limits<unsigned>::max())throw std::runtime_error("capnp field count limit");return static_cast<unsigned>(n);}
inline capnp::Data::Reader bytes(std::span<const std::uint8_t> b){return {b.data(),b.size()};}
inline kj::StringPtr string(std::string_view s){return {s.data(),s.size()};}
inline void assign(sc_cap::Record::Builder r,const Record& source){
    r.setId(source.id);r.setRevision(source.revision);r.setScore(source.score);r.setName(string(source.name));
    auto samples=r.initSamples(count(source.samples.size(),Limits{}.max_sequence));
    for(unsigned i=0;i<samples.size();++i)samples.set(i,source.samples[i]);
    r.setPayload(bytes(source.payload));auto attrs=r.initAttrs(count(source.attrs.size(),Limits{}.max_attrs));
    for(unsigned i=0;i<attrs.size();++i){attrs[i].setKey(string(source.attrs[i].key));attrs[i].setValue(bytes(source.attrs[i].value));}
}
inline unsigned position(capnp::List<sc_cap::Record>::Builder records,std::uint64_t id){
    unsigned lo=0,hi=records.size();while(lo<hi){auto mid=lo+(hi-lo)/2;if(records[mid].getId()<id)lo=mid+1;else hi=mid;}
    return lo<records.size()&&records[lo].getId()==id?lo:records.size();
}
inline void status(const Mutation& op,Status actual){if(op.expected!=actual)throw std::runtime_error("capnp mutation status mismatch");}
} // namespace cap_view

struct CapnProtoAdapter {
    static constexpr std::string_view library_id="capnproto-v1.5.0";
    static constexpr std::string_view commit="373e61ec89e2359f1c362e9b2eadc552f4779306";
    static constexpr std::string_view trust_mode="T-trusted-bounded-reader-builder-detached;U-not-established";
    using State=std::unique_ptr<capnp::MallocMessageBuilder>;
    static State build(const Snapshot& s){
        auto state=std::make_unique<capnp::MallocMessageBuilder>();auto root=state->initRoot<sc_cap::Snapshot>();
        root.setSchemaVersion(s.schema_version);root.setEpoch(s.epoch);
        auto records=root.initRecords(cap_view::count(s.records.size(),Limits{}.max_records));
        for(unsigned i=0;i<records.size();++i)cap_view::assign(records[i],s.records[i]);return state;
    }
    static State load(std::span<const std::uint8_t> wire){
        if(wire.size()>wire_limit||wire.size()%sizeof(capnp::word)!=0||wire.empty())throw std::runtime_error("capnp wire size/alignment framing");
        auto aligned=kj::heapArray<capnp::word>(wire.size()/sizeof(capnp::word));std::memcpy(aligned.begin(),wire.data(),wire.size());
        capnp::ReaderOptions options;options.traversalLimitInWords=wire_limit/sizeof(capnp::word);options.nestingLimit=64;
        capnp::FlatArrayMessageReader reader(aligned.asPtr(),options);
        if(reader.getEnd()!=aligned.end())throw std::runtime_error("capnp trailing words");
        auto root=reader.getRoot<sc_cap::Snapshot>();
        if(root.getSchemaVersion()!=1||root.getRecords().size()>Limits{}.max_records)throw std::runtime_error("capnp schema/record count");
        auto state=std::make_unique<capnp::MallocMessageBuilder>();state->setRoot(root);
        // Copy finishes before reader/aligned input die; state has no input lease.
        return state;
    }
    static kj::Array<capnp::word> emit(const State& state){return capnp::messageToFlatArray(*state);}
    static Snapshot export_state(const State& state){return export_view(cap_view::SnapshotView{state->getRoot<sc_cap::Snapshot>().asReader()});}
    static void query(const State& state,std::span<const Query> queries,std::span<QueryResult> results){query_view(cap_view::SnapshotView{state->getRoot<sc_cap::Snapshot>().asReader()},queries,results);}

    static void erase_stage(State& state,std::span<const Mutation> operations){
        auto root=state->getRoot<sc_cap::Snapshot>();auto records=root.getRecords();
        std::unordered_set<std::uint64_t> removed;removed.reserve(operations.size());
        for(auto& op:operations){
            bool found=cap_view::position(records,op.id)!=records.size()&&removed.find(op.id)==removed.end();
            cap_view::status(op,found?Status::ok:Status::not_found);if(found)removed.insert(op.id);
        }
        if(removed.empty())return;
        auto old=root.disownRecords();auto source=old.getReader();
        auto result=root.initRecords(cap_view::count(source.size()-removed.size(),Limits{}.max_records));unsigned to=0;
        for(unsigned i=0;i<source.size();++i)if(removed.find(source[i].getId())==removed.end())result.setWithCaveats(to++,source[i]);
    }
    static void append_stage(State& state,std::span<const Mutation> operations){
        auto root=state->getRoot<sc_cap::Snapshot>();auto records=root.getRecords();
        std::vector<const Mutation*> additions;additions.reserve(operations.size());std::unordered_set<std::uint64_t> ids;ids.reserve(operations.size());
        for(auto& op:operations){
            if(cap_view::position(records,op.id)!=records.size()||!ids.insert(op.id).second)throw std::runtime_error("capnp append duplicate id");
            cap_view::status(op,Status::ok);additions.push_back(&op);
        }
        if(additions.empty())return;
        std::sort(additions.begin(),additions.end(),[](auto* a,auto* b){return a->id<b->id;});
        auto old=root.disownRecords();auto source=old.getReader();
        auto result=root.initRecords(cap_view::count(source.size()+additions.size(),Limits{}.max_records));
        unsigned before=0,to=0;std::size_t added=0;
        while(before<source.size()||added<additions.size()){
            if(added<additions.size()&&(before==source.size()||additions[added]->id<source[before].getId()))cap_view::assign(result[to++],additions[added++]->record);
            else result.setWithCaveats(to++,source[before++]);
        }
    }
    static void mutate(State& state,std::span<const Mutation> operations){
        std::size_t offset=0;
        while(offset<operations.size()){
            auto stop=offset+1;while(stop<operations.size()&&operations[stop].stage==operations[offset].stage)++stop;
            auto group=operations.subspan(offset,stop-offset);offset=stop;
            if(group.front().code==MutationCode::erase_record){erase_stage(state,group);continue;}
            if(group.front().code==MutationCode::append_record){append_stage(state,group);continue;}
            auto root=state->getRoot<sc_cap::Snapshot>();
            for(auto& op:group){
                if(op.code==MutationCode::set_epoch){root.setEpoch(op.epoch);cap_view::status(op,Status::ok);continue;}
                auto records=root.getRecords();auto at=cap_view::position(records,op.id);
                if(at==records.size()){cap_view::status(op,Status::not_found);continue;}
                auto record=records[at];Status actual=Status::ok;
                switch(op.code){
                case MutationCode::update_score:record.setScore(op.score);record.setRevision(op.revision);break;
                case MutationCode::replace_name:record.setName(cap_view::string(op.text));break;
                case MutationCode::replace_payload:record.setPayload(cap_view::bytes(op.bytes));break;
                case MutationCode::append_samples:{
                    auto old=record.disownSamples();auto source=old.getReader();
                    auto result=record.initSamples(cap_view::count(source.size()+op.samples.size(),Limits{}.max_sequence));
                    for(unsigned i=0;i<source.size();++i)result.set(i,source[i]);
                    for(std::size_t i=0;i<op.samples.size();++i)result.set(static_cast<unsigned>(source.size()+i),op.samples[i]);break;
                }
                case MutationCode::replace_attr:case MutationCode::insert_attr:{
                    auto attrs=record.getAttrs();unsigned pos=0,hi=attrs.size();
                    while(pos<hi){auto mid=pos+(hi-pos)/2;if(wire::byte_less(cap_view::text(attrs[mid].getKey()),op.text))pos=mid+1;else hi=mid;}
                    bool found=pos<attrs.size()&&cap_view::text(attrs[pos].getKey())==op.text;
                    if(op.code==MutationCode::replace_attr){if(found)attrs[pos].setValue(cap_view::bytes(op.bytes));else actual=Status::not_found;}
                    else {
                        if(found)throw std::runtime_error("capnp insert duplicate attr");
                        auto old=record.disownAttrs();auto source=old.getReader();auto result=record.initAttrs(cap_view::count(source.size()+1,Limits{}.max_attrs));
                        for(unsigned i=0;i<pos;++i)result.setWithCaveats(i,source[i]);
                        result[pos].setKey(cap_view::string(op.text));result[pos].setValue(cap_view::bytes(op.bytes));
                        for(unsigned i=pos;i<source.size();++i)result.setWithCaveats(i+1,source[i]);
                    }break;
                }
                default:throw std::runtime_error("capnp unknown mutation");
                }
                cap_view::status(op,actual);
            }
        }
    }
};
} // namespace sc
