#pragma once
#include "views.hpp"
#include "snapshot_generated.h"
#include <flatbuffers/flatbuffers.h>
#include <unordered_set>

namespace sc {
namespace fb_detail {
inline auto make_attr(const Attr& a){auto p=std::make_unique<sc_fb::AttrT>();p->key=a.key;p->value=a.value;return p;}
inline auto make_record(const Record& r){auto p=std::make_unique<sc_fb::RecordT>();p->id=r.id;p->revision=r.revision;p->score=r.score;p->name=r.name;p->samples=r.samples;p->payload=r.payload;p->attrs.reserve(r.attrs.size());for(const auto& a:r.attrs)p->attrs.push_back(make_attr(a));return p;}
struct RecordView {
    const sc_fb::RecordT& r;
    std::uint64_t id()const{return r.id;}
    std::uint32_t revision()const{return r.revision;}
    std::int64_t score()const{return r.score;}
    std::string_view name()const{return r.name;}
    std::span<const std::uint32_t> samples()const{return r.samples;}
    std::span<const std::uint8_t> payload()const{return r.payload;}
    std::size_t attrs_size()const{return r.attrs.size();}
    auto attr(std::size_t i)const{return PlainAttrView{*r.attrs[i]};}
};
struct SnapshotView {
    const sc_fb::SnapshotT& s;
    std::uint32_t schema_version()const{return s.schema_version;}
    std::uint64_t epoch()const{return s.epoch;}
    std::size_t size()const{return s.records.size();}
    auto record(std::size_t i)const{return RecordView{*s.records[i]};}
};
}
struct FlatBuffersAdapter {
    static constexpr std::string_view library_id="flatbuffers-v25.12.19";
    static constexpr std::string_view commit="7e163021e59cca4f8e1e35a7c828b5c6b7915953";
    static constexpr std::string_view trust_mode="trusted-object-api-no-upfront-verifier";
    using State=std::unique_ptr<sc_fb::SnapshotT>;
    static State build(const Snapshot& s){
        auto p=std::make_unique<sc_fb::SnapshotT>();p->schema_version=s.schema_version;p->epoch=s.epoch;p->records.reserve(s.records.size());
        for(const auto& r:s.records)p->records.push_back(fb_detail::make_record(r));return p;
    }
    static State load(std::span<const std::uint8_t> b){
        if(b.size()<8||b.size()>wire_limit||!sc_fb::SnapshotBufferHasIdentifier(b.data()))throw std::runtime_error("FlatBuffers trusted wire framing mismatch");
        // Trusted-only path. UnPack allocates an independent generated native
        // object tree, so the caller's input can be overwritten/freed on return.
        return State{sc_fb::GetSnapshot(b.data())->UnPack()};
    }
    static auto emit(const State& s){
        flatbuffers::FlatBufferBuilder builder(1024);
        auto root=sc_fb::Snapshot::Pack(builder,s.get());sc_fb::FinishSnapshotBuffer(builder,root);
        return builder.Release();
    }
    static Snapshot export_state(const State& s){return export_view(fb_detail::SnapshotView{*s});}
    static void query(const State& s,std::span<const Query> q,std::span<QueryResult> out){query_view(fb_detail::SnapshotView{*s},q,out);}
    static void mutate(State& s,std::span<const Mutation> ops){
        std::size_t offset=0;
        while(offset<ops.size()){
            auto stop=offset+1;while(stop<ops.size()&&ops[stop].stage==ops[offset].stage)++stop;
            auto group=ops.subspan(offset,stop-offset);offset=stop;
            if(group.front().code==MutationCode::erase_record){
                std::unordered_set<std::uint64_t> removed;removed.reserve(group.size());
                for(const auto& m:group){
                    const auto at=find_record(fb_detail::SnapshotView{*s},m.id);
                    const bool found=at<s->records.size()&&!removed.contains(m.id);
                    if((found?Status::ok:Status::not_found)!=m.expected)throw std::runtime_error("FlatBuffers bulk erase status mismatch");
                    if(found)removed.insert(m.id);
                }
                s->records.erase(std::remove_if(s->records.begin(),s->records.end(),[&](const auto& r){return removed.contains(r->id);}),s->records.end());
                continue;
            }
        for(const auto& m:group){
            Status status=Status::ok;
            if(m.code==MutationCode::set_epoch)s->epoch=m.epoch;
            else if(m.code==MutationCode::append_record){
                if(!s->records.empty()&&s->records.back()->id>=m.record.id)throw std::runtime_error("append record id is not increasing");
                s->records.push_back(fb_detail::make_record(m.record));
            }else{
                auto it=std::lower_bound(s->records.begin(),s->records.end(),m.id,[](const auto& r,auto id){return r->id<id;});
                if(it==s->records.end()||(*it)->id!=m.id)status=Status::not_found;
                else if(m.code==MutationCode::erase_record)s->records.erase(it);
                else{
                    auto& r=**it;
                    switch(m.code){
                        case MutationCode::update_score:r.score=m.score;r.revision=m.revision;break;
                        case MutationCode::replace_name:r.name=m.text;break;
                        case MutationCode::append_samples:r.samples.insert(r.samples.end(),m.samples.begin(),m.samples.end());break;
                        case MutationCode::replace_payload:r.payload=m.bytes;break;
                        case MutationCode::replace_attr:
                        case MutationCode::insert_attr:{
                            auto a=std::lower_bound(r.attrs.begin(),r.attrs.end(),m.text,[](const auto& x,const auto& key){return x->key<key;});
                            const bool found=a!=r.attrs.end()&&(*a)->key==m.text;
                            if(m.code==MutationCode::replace_attr){if(!found)status=Status::not_found;else (*a)->value=m.bytes;}
                            else {if(found)throw std::runtime_error("duplicate attribute insertion");r.attrs.insert(a,fb_detail::make_attr({m.text,m.bytes}));}
                            break;
                        }
                        default:throw std::runtime_error("unknown mutation opcode");
                    }
                }
            }
            if(status!=m.expected)throw std::runtime_error("FlatBuffers mutation status mismatch");
        }
        }
    }
};
}
