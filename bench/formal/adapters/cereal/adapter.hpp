#pragma once
#include "views.hpp"
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <istream>
#include <sstream>
#include <streambuf>
#include <unordered_set>

// These are the native std-container state and ordinary cereal serializers.
// There is no alternate DTO used for measured query/mutation.
namespace cereal {
template<class A> void serialize(A& a,sc::Attr& x){a(x.key,x.value);}
template<class A> void serialize(A& a,sc::Record& x){a(x.id,x.revision,x.score,x.name,x.samples,x.payload,x.attrs);}
template<class A> void serialize(A& a,sc::Snapshot& x){a(x.schema_version,x.epoch,x.records);}
}
namespace sc {
struct CerealAdapter {
    static constexpr std::string_view library_id="cereal-v1.3.2";
    static constexpr std::string_view commit="ebef1e929807629befafbb2918ea1a08c7194554";
    static constexpr std::string_view trust_mode="T-trusted-binary-native-std-detached;U-not-established";
    using State=std::unique_ptr<Snapshot>;
    static State build(const Snapshot& s){return std::make_unique<Snapshot>(s);}
    static State load(std::span<const std::uint8_t> bytes){
        if(bytes.size()>wire_limit)throw std::runtime_error("cereal wire limit");
        struct ReadBuffer:std::streambuf {
            explicit ReadBuffer(std::span<const std::uint8_t> data){
                if(data.empty()){setg(nullptr,nullptr,nullptr);return;}
                auto* begin=const_cast<char*>(reinterpret_cast<const char*>(data.data()));
                // std::istream reads this get area; it never writes caller bytes.
                setg(begin,begin,begin+data.size());
            }
        } buffer(bytes);
        std::istream input(&buffer);auto state=std::make_unique<Snapshot>();
        {cereal::BinaryInputArchive archive(input);archive(*state);}
        if(input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("cereal trailing bytes");
        if(state->schema_version!=1)throw std::runtime_error("cereal schema version");
        return state;
    }
    static std::string emit(const State& state){
        std::ostringstream output(std::ios::binary|std::ios::out);
        {cereal::BinaryOutputArchive archive(output);archive(*state);}
        return std::move(output).str();
    }
    static Snapshot export_state(const State& state){return *state;}
    static void query(const State& state,std::span<const Query> qs,std::span<QueryResult> results){query_view(PlainSnapshotView{*state},qs,results);}
    static void mutate(State& state,std::span<const Mutation> operations){
        auto& s=*state;
        std::size_t offset=0;
        while(offset<operations.size()){
            auto stop=offset+1;while(stop<operations.size()&&operations[stop].stage==operations[offset].stage)++stop;
            const auto group=operations.subspan(offset,stop-offset);offset=stop;
            if(group.front().code==MutationCode::erase_record){
                std::unordered_set<std::uint64_t> removed;removed.reserve(group.size());
                for(const auto& op:group){
                    const auto pos=std::lower_bound(s.records.begin(),s.records.end(),op.id,[](const auto& r,auto id){return r.id<id;});
                    const bool found=pos!=s.records.end()&&pos->id==op.id&&removed.find(op.id)==removed.end();
                    if((found?Status::ok:Status::not_found)!=op.expected)throw std::runtime_error("cereal erase status mismatch");
                    if(found)removed.insert(op.id);
                }
                s.records.erase(std::remove_if(s.records.begin(),s.records.end(),[&](const auto& r){return removed.find(r.id)!=removed.end();}),s.records.end());
                continue;
            }
            for(const auto& op:group){
            Status actual=Status::ok;
            if(op.code==MutationCode::set_epoch)s.epoch=op.epoch;
            else {
                auto pos=std::lower_bound(s.records.begin(),s.records.end(),op.id,[](const auto& r,auto id){return r.id<id;});
                const bool found=pos!=s.records.end()&&pos->id==op.id;
                if(op.code==MutationCode::append_record){
                    if(found)throw std::runtime_error("cereal append duplicate id");
                    s.records.insert(pos,op.record);
                }else if(!found)actual=Status::not_found;
                else switch(op.code){
                case MutationCode::update_score:pos->score=op.score;pos->revision=op.revision;break;
                case MutationCode::replace_name:pos->name=op.text;break;
                case MutationCode::append_samples:pos->samples.insert(pos->samples.end(),op.samples.begin(),op.samples.end());break;
                case MutationCode::replace_payload:pos->payload=op.bytes;break;
                case MutationCode::replace_attr:case MutationCode::insert_attr:{
                    auto a=std::lower_bound(pos->attrs.begin(),pos->attrs.end(),op.text,[](const auto& x,const auto& key){return wire::byte_less(x.key,key);});
                    const bool exists=a!=pos->attrs.end()&&a->key==op.text;
                    if(op.code==MutationCode::replace_attr){if(exists)a->value=op.bytes;else actual=Status::not_found;}
                    else {if(exists)throw std::runtime_error("cereal insert duplicate key");pos->attrs.insert(a,Attr{op.text,op.bytes});}
                    break;
                }
                case MutationCode::erase_record:s.records.erase(pos);break;
                default:throw std::runtime_error("cereal unknown mutation");
                }
            }
            if(actual!=op.expected)throw std::runtime_error("cereal mutation status mismatch");
            }
        }
    }
};
} // namespace sc
