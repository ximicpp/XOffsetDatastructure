#pragma once
#include "sha256.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sc {
using Bytes=std::vector<std::uint8_t>;
struct Attr {std::string key;Bytes value;bool operator==(const Attr&) const=default;};
struct Record {
    std::uint64_t id{};std::uint32_t revision{};std::int64_t score{};std::string name;
    std::vector<std::uint32_t> samples;Bytes payload;std::vector<Attr> attrs;
    bool operator==(const Record&) const=default;
};
struct Snapshot {std::uint32_t schema_version{1};std::uint64_t epoch{};std::vector<Record> records;bool operator==(const Snapshot&) const=default;};
enum class Status:std::uint8_t {ok=0,not_found=1};
enum class QueryCode:std::uint8_t {record=1,attr=2,scan=3};
struct Query {std::uint64_t sequence{};QueryCode code{};std::uint64_t id{};std::string key;bool operator==(const Query&) const=default;};
struct QueryResult {std::uint64_t sequence{};Status status{};std::array<std::uint64_t,8> values{};bool operator==(const QueryResult&) const=default;};
enum class MutationCode:std::uint8_t {update_score=1,replace_name=2,append_samples=3,replace_payload=4,replace_attr=5,insert_attr=6,erase_record=7,append_record=8,set_epoch=9};
struct Mutation {
    std::uint64_t sequence{};std::uint32_t stage{};MutationCode code{};std::uint64_t id{};Status expected{Status::ok};
    std::int64_t score{};std::uint32_t revision{};std::string text;std::vector<std::uint32_t> samples;
    Bytes bytes;Record record;std::uint64_t epoch{};
    bool operator==(const Mutation&) const=default;
};
struct StageExpected {std::uint32_t stage{};std::uint64_t mutation_count{};Digest256 canonical_digest{};bool operator==(const StageExpected&) const=default;};
struct Trace {
    Digest256 input_digest{};std::vector<Query> queries;std::vector<Mutation> mutations;
    std::vector<StageExpected> stages;std::vector<QueryResult> before,after;
    bool operator==(const Trace&) const=default;
};
struct Limits {
    std::uint64_t max_bytes=512ULL<<20,max_records=1048576,max_sequence=16777216,max_string=1ULL<<20,
        max_blob=64ULL<<20,max_attrs=4096,max_queries=4194304,max_mutations=2097152,max_total_elements=67108864;
};

inline Bytes encode_canonical(const Snapshot&);
inline Snapshot decode_canonical(std::span<const std::uint8_t>,const Limits& = {});
inline Bytes encode_trace(const Trace&);
inline Trace decode_trace(std::span<const std::uint8_t>,const Limits& = {});
inline Bytes encode_query_results(std::span<const QueryResult>);
inline std::vector<QueryResult> decode_query_results(std::span<const std::uint8_t>,const Limits& = {});
inline Bytes encode_query_pair(std::span<const QueryResult>,std::span<const QueryResult>);
inline std::pair<std::vector<QueryResult>,std::vector<QueryResult>> decode_query_pair(std::span<const std::uint8_t>,const Limits& = {});
inline std::vector<QueryResult> query_reference(const Snapshot&,std::span<const Query>);
inline Status apply_reference(Snapshot&,const Mutation&);
inline void validate_snapshot(const Snapshot&,const Limits& = {});

namespace wire {
[[noreturn]] inline void fail(const char* message) {throw std::runtime_error(message);}
inline bool text_valid(std::string_view s) {
    for(std::size_t i=0;i<s.size();) {
        auto a=static_cast<std::uint8_t>(s[i++]);
        if(a==0) return false;
        if(a<0x80) continue;
        unsigned n;std::uint32_t cp;
        if(a>=0xc2 && a<=0xdf){n=1;cp=a&31;}
        else if(a>=0xe0 && a<=0xef){n=2;cp=a&15;}
        else if(a>=0xf0 && a<=0xf4){n=3;cp=a&7;}
        else return false;
        if(n>s.size()-i) return false;
        for(unsigned j=0;j<n;++j){auto b=static_cast<std::uint8_t>(s[i++]);if((b&0xc0)!=0x80)return false;cp=(cp<<6)|(b&63);}
        if((n==1&&cp<0x80)||(n==2&&cp<0x800)||(n==3&&cp<0x10000)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)) return false;
    }
    return true;
}
inline bool byte_less(std::string_view a,std::string_view b) {
    return std::lexicographical_compare(a.begin(),a.end(),b.begin(),b.end(),[](char x,char y){return static_cast<std::uint8_t>(x)<static_cast<std::uint8_t>(y);});
}
inline void valid_text(std::string_view s,const Limits& l){if(s.size()>l.max_string||!text_valid(s))fail("invalid UTF-8 text or text limit");}
struct Writer {
    Bytes b;
    void raw(std::span<const std::uint8_t> s){if(s.size()>Limits{}.max_bytes-b.size())fail("encoded byte limit");b.insert(b.end(),s.begin(),s.end());}
    void magic(std::string_view s){raw({reinterpret_cast<const std::uint8_t*>(s.data()),s.size()});}
    void u8(std::uint8_t x){if(b.size()==Limits{}.max_bytes)fail("encoded byte limit");b.push_back(x);}
    void u32(std::uint32_t x){for(unsigned i=0;i<4;++i)u8(std::uint8_t(x>>(8*i)));}
    void u64(std::uint64_t x){for(unsigned i=0;i<8;++i)u8(std::uint8_t(x>>(8*i)));}
    void i64(std::int64_t x){u64(std::bit_cast<std::uint64_t>(x));}
    void blob(std::span<const std::uint8_t> x){u64(x.size());raw(x);}
    void text(std::string_view x){valid_text(x,Limits{});blob({reinterpret_cast<const std::uint8_t*>(x.data()),x.size()});}
};
struct Reader {
    std::span<const std::uint8_t> b;const Limits& l;std::size_t p{};std::uint64_t elements{};
    Reader(std::span<const std::uint8_t> data,const Limits& limits):b(data),l(limits){if(b.size()>l.max_bytes)fail("input byte limit");}
    std::span<const std::uint8_t> raw(std::uint64_t n){if(n>b.size()-p)fail("truncated input");auto s=b.subspan(p,static_cast<std::size_t>(n));p+=static_cast<std::size_t>(n);return s;}
    std::uint8_t u8(){return raw(1)[0];}
    std::uint32_t u32(){auto s=raw(4);std::uint32_t x=0;for(unsigned i=0;i<4;++i)x|=std::uint32_t(s[i])<<(8*i);return x;}
    std::uint64_t u64(){auto s=raw(8);std::uint64_t x=0;for(unsigned i=0;i<8;++i)x|=std::uint64_t(s[i])<<(8*i);return x;}
    std::int64_t i64(){return std::bit_cast<std::int64_t>(u64());}
    std::size_t count(std::uint64_t maximum,std::uint64_t minimum=1){
        auto n=u64();if(n>maximum||n>(b.size()-p)/minimum||n>l.max_total_elements-elements)fail("count/aggregate limit or truncated collection");
        elements+=n;return static_cast<std::size_t>(n);
    }
    Bytes blob(std::uint64_t maximum){auto n=u64();if(n>maximum)fail("blob limit");auto s=raw(n);return {s.begin(),s.end()};}
    Bytes blob(){return blob(l.max_blob);}
    std::string text(){auto data=blob(l.max_string);std::string s(data.begin(),data.end());valid_text(s,l);return s;}
    void magic(std::string_view x){auto s=raw(x.size());if(!std::equal(s.begin(),s.end(),x.begin()))fail("wrong magic");}
    void end(){if(p!=b.size())fail("trailing bytes");}
    Digest256 digest(){Digest256 d;auto s=raw(32);std::copy(s.begin(),s.end(),d.begin());return d;}
};
inline void write_record(Writer& w,const Record& r){
    w.u64(r.id);w.u32(r.revision);w.i64(r.score);w.text(r.name);w.u64(r.samples.size());
    for(auto x:r.samples)w.u32(x);w.blob(r.payload);w.u64(r.attrs.size());
    for(const auto& a:r.attrs){w.text(a.key);w.blob(a.value);}
}
inline Record read_record(Reader& r){
    Record x;x.id=r.u64();x.revision=r.u32();x.score=r.i64();x.name=r.text();
    auto ns=r.count(r.l.max_sequence,4);x.samples.reserve(ns);for(std::size_t i=0;i<ns;++i)x.samples.push_back(r.u32());
    x.payload=r.blob();auto na=r.count(r.l.max_attrs,16);x.attrs.reserve(na);
    for(std::size_t i=0;i<na;++i){Attr a;a.key=r.text();a.value=r.blob();x.attrs.push_back(std::move(a));}return x;
}
inline void valid_results(std::span<const QueryResult> values){
    if(values.size()>Limits{}.max_queries)fail("query result limit");
    for(std::size_t i=0;i<values.size();++i){auto& q=values[i];
        if(q.sequence!=i||(q.status!=Status::ok&&q.status!=Status::not_found))fail("invalid query result");
        if(q.status==Status::not_found&&std::any_of(q.values.begin(),q.values.end(),[](auto x){return x!=0;}))fail("nonzero not_found result");
    }
}
inline void write_results(Writer& w,std::span<const QueryResult> results){
    valid_results(results);w.u64(results.size());for(auto& q:results){w.u64(q.sequence);w.u8(static_cast<std::uint8_t>(q.status));for(auto v:q.values)w.u64(v);}
}
inline std::vector<QueryResult> read_results(Reader& r){
    auto n=r.count(r.l.max_queries,73);std::vector<QueryResult> out;out.reserve(n);
    for(std::size_t i=0;i<n;++i){QueryResult q;q.sequence=r.u64();q.status=static_cast<Status>(r.u8());for(auto& v:q.values)v=r.u64();out.push_back(q);}
    valid_results(out);return out;
}
inline void valid_trace(const Trace& t){
    if(t.queries.size()>Limits{}.max_queries||t.mutations.size()>Limits{}.max_mutations||t.before.size()!=t.queries.size()||t.after.size()!=t.queries.size())fail("trace result/operation count");
    for(std::size_t i=0;i<t.queries.size();++i){auto& q=t.queries[i];auto c=static_cast<unsigned>(q.code);
        if(q.sequence!=i||c<1||c>3||(c!=2&&!q.key.empty())||(c==3&&q.id!=0))fail("query order/code/unused field");valid_text(q.key,Limits{});
    }
    std::array<std::uint64_t,9> counts{};unsigned last=1;
    for(std::size_t i=0;i<t.mutations.size();++i){auto& m=t.mutations[i];auto c=static_cast<unsigned>(m.code);
        if(m.sequence!=i||c<1||c>9||m.stage!=c||m.stage<last||(m.id==0)!=(c==9)||(m.expected!=Status::ok&&m.expected!=Status::not_found))fail("mutation order/code/stage/id/status");
        if(c==8){if(m.id!=m.record.id)fail("append id mismatch");Snapshot s;s.records.push_back(m.record);validate_snapshot(s);}
        if((c==2||c==5||c==6))valid_text(m.text,Limits{});
        if(m.samples.size()>Limits{}.max_sequence||m.bytes.size()>Limits{}.max_blob)fail("mutation field limit");
        ++counts[m.stage-1];last=m.stage;
    }
    if(t.stages.size()!=9)fail("nine stage contracts required");
    for(std::size_t i=0;i<9;++i)if(t.stages[i].stage!=i+1||t.stages[i].mutation_count!=counts[i])fail("stage contract count/order");
    valid_results(t.before);valid_results(t.after);
}
inline std::uint64_t byte_sum(std::span<const std::uint8_t> b){std::uint64_t n=0;for(auto x:b)n+=x;return n;}
inline std::uint64_t text_sum(std::string_view s){std::uint64_t n=0;for(auto x:s)n+=static_cast<std::uint8_t>(x);return n;}
} // namespace wire

inline void validate_snapshot(const Snapshot& s,const Limits& l){
    if(s.schema_version!=1||s.records.size()>l.max_records)wire::fail("schema/record limit");
    std::uint64_t prev=0,total=s.records.size();
    if(total>l.max_total_elements)wire::fail("aggregate element limit");
    for(auto& r:s.records){
        if(r.id<=prev)wire::fail("record ids must be positive and strictly ordered");prev=r.id;
        wire::valid_text(r.name,l);
        if(r.payload.size()>l.max_blob||r.samples.size()>l.max_sequence||r.attrs.size()>l.max_attrs)wire::fail("record field limit");
        auto add=r.samples.size()+r.attrs.size();if(add>l.max_total_elements-total)wire::fail("aggregate element limit");total+=add;
        std::string_view last;bool first=true;
        for(auto& a:r.attrs){wire::valid_text(a.key,l);if((!first&&!wire::byte_less(last,a.key))||a.value.size()>l.max_blob)wire::fail("attribute order/limit");last=a.key;first=false;}
    }
}
inline Bytes encode_canonical(const Snapshot& s){
    validate_snapshot(s);wire::Writer w;w.magic("SCCAN001");w.u32(s.schema_version);w.u64(s.epoch);w.u64(s.records.size());
    for(auto& r:s.records)wire::write_record(w,r);return std::move(w.b);
}
inline Snapshot decode_canonical(std::span<const std::uint8_t> bytes,const Limits& l){
    wire::Reader r(bytes,l);r.magic("SCCAN001");Snapshot s;s.schema_version=r.u32();s.epoch=r.u64();
    auto n=r.count(l.max_records,52);s.records.reserve(n);for(std::size_t i=0;i<n;++i)s.records.push_back(wire::read_record(r));
    r.end();validate_snapshot(s,l);return s;
}
inline Bytes encode_query_results(std::span<const QueryResult> results){wire::Writer w;w.magic("SCQUERY1");w.u32(1);wire::write_results(w,results);return std::move(w.b);}
inline std::vector<QueryResult> decode_query_results(std::span<const std::uint8_t> bytes,const Limits& l){
    wire::Reader r(bytes,l);r.magic("SCQUERY1");if(r.u32()!=1)wire::fail("query version");auto out=wire::read_results(r);r.end();return out;
}
inline Bytes encode_query_pair(std::span<const QueryResult> before,std::span<const QueryResult> after){
    wire::Writer w;w.magic("SCQPAIR1");w.u32(1);w.blob(encode_query_results(before));w.blob(encode_query_results(after));return std::move(w.b);
}
inline std::pair<std::vector<QueryResult>,std::vector<QueryResult>> decode_query_pair(std::span<const std::uint8_t> bytes,const Limits& l){
    wire::Reader r(bytes,l);r.magic("SCQPAIR1");if(r.u32()!=1)wire::fail("query pair version");
    auto a=decode_query_results(r.blob(l.max_bytes),l),b=decode_query_results(r.blob(l.max_bytes),l);r.end();return {std::move(a),std::move(b)};
}
inline Bytes encode_trace(const Trace& t){
    wire::valid_trace(t);wire::Writer w;w.magic("SCTRAC01");w.u32(1);w.raw(t.input_digest);w.u64(t.queries.size());
    for(auto& q:t.queries){w.u64(q.sequence);w.u8(static_cast<std::uint8_t>(q.code));w.u64(q.id);w.text(q.key);}
    w.u64(t.mutations.size());
    for(auto& m:t.mutations){
        w.u64(m.sequence);w.u32(m.stage);w.u8(static_cast<std::uint8_t>(m.code));w.u64(m.id);w.u8(static_cast<std::uint8_t>(m.expected));
        switch(m.code){
        case MutationCode::update_score:w.i64(m.score);w.u32(m.revision);break;
        case MutationCode::replace_name:w.text(m.text);break;
        case MutationCode::append_samples:w.u64(m.samples.size());for(auto x:m.samples)w.u32(x);break;
        case MutationCode::replace_payload:w.blob(m.bytes);break;
        case MutationCode::replace_attr:case MutationCode::insert_attr:w.text(m.text);w.blob(m.bytes);break;
        case MutationCode::erase_record:break;
        case MutationCode::append_record:wire::write_record(w,m.record);break;
        case MutationCode::set_epoch:w.u64(m.epoch);break;
        }
    }
    w.u64(t.stages.size());for(auto& s:t.stages){w.u32(s.stage);w.u64(s.mutation_count);w.raw(s.canonical_digest);}
    wire::write_results(w,t.before);wire::write_results(w,t.after);return std::move(w.b);
}
inline Trace decode_trace(std::span<const std::uint8_t> bytes,const Limits& l){
    wire::Reader r(bytes,l);r.magic("SCTRAC01");if(r.u32()!=1)wire::fail("trace version");Trace t;t.input_digest=r.digest();
    auto nq=r.count(l.max_queries,25);t.queries.reserve(nq);
    for(std::size_t i=0;i<nq;++i){Query q;q.sequence=r.u64();q.code=static_cast<QueryCode>(r.u8());q.id=r.u64();q.key=r.text();t.queries.push_back(std::move(q));}
    auto nm=r.count(l.max_mutations,22);t.mutations.reserve(nm);
    for(std::size_t i=0;i<nm;++i){
        Mutation m;m.sequence=r.u64();m.stage=r.u32();m.code=static_cast<MutationCode>(r.u8());m.id=r.u64();m.expected=static_cast<Status>(r.u8());
        switch(m.code){
        case MutationCode::update_score:m.score=r.i64();m.revision=r.u32();break;
        case MutationCode::replace_name:m.text=r.text();break;
        case MutationCode::append_samples:{auto n=r.count(l.max_sequence,4);m.samples.reserve(n);for(std::size_t j=0;j<n;++j)m.samples.push_back(r.u32());break;}
        case MutationCode::replace_payload:m.bytes=r.blob();break;
        case MutationCode::replace_attr:case MutationCode::insert_attr:m.text=r.text();m.bytes=r.blob();break;
        case MutationCode::erase_record:break;
        case MutationCode::append_record:m.record=wire::read_record(r);break;
        case MutationCode::set_epoch:m.epoch=r.u64();break;
        default:wire::fail("unknown mutation");
        }
        t.mutations.push_back(std::move(m));
    }
    auto ns=r.count(9,44);for(std::size_t i=0;i<ns;++i){StageExpected s;s.stage=r.u32();s.mutation_count=r.u64();s.canonical_digest=r.digest();t.stages.push_back(s);}
    t.before=wire::read_results(r);t.after=wire::read_results(r);r.end();wire::valid_trace(t);return t;
}
inline std::vector<QueryResult> query_reference(const Snapshot& s,std::span<const Query> queries){
    std::vector<QueryResult> out;out.reserve(queries.size());
    for(auto& q:queries){QueryResult z;z.sequence=q.sequence;auto& v=z.values;
        if(q.code==QueryCode::scan){
            v[0]=s.records.size();for(auto& r:s.records){v[1]+=r.id;v[2]+=r.revision;v[3]+=static_cast<std::uint64_t>(r.score);v[4]+=r.name.size();for(auto x:r.samples)v[5]+=x;v[6]+=r.payload.size();v[7]+=r.attrs.size();}
        }else{
            auto it=std::lower_bound(s.records.begin(),s.records.end(),q.id,[](const auto& r,auto id){return r.id<id;});
            if(it==s.records.end()||it->id!=q.id)z.status=Status::not_found;
            else if(q.code==QueryCode::record){
                v={it->revision,static_cast<std::uint64_t>(it->score),it->name.size(),wire::text_sum(it->name),it->samples.size(),0,it->payload.size(),wire::byte_sum(it->payload)};
                for(auto x:it->samples)v[5]+=x;
            }else if(q.code==QueryCode::attr){
                auto a=std::lower_bound(it->attrs.begin(),it->attrs.end(),q.key,[](const auto& x,const auto& key){return wire::byte_less(x.key,key);});
                if(a==it->attrs.end()||a->key!=q.key)z.status=Status::not_found;
                else v={a->value.size(),wire::byte_sum(a->value),a->value.empty()?0u:a->value.front(),a->value.empty()?0u:a->value.back(),0,0,0,0};
            }else wire::fail("unknown query");
        }
        out.push_back(z);
    }return out;
}
inline Status apply_reference(Snapshot& s,const Mutation& m){
    if(m.code==MutationCode::set_epoch){s.epoch=m.epoch;return Status::ok;}
    auto it=std::lower_bound(s.records.begin(),s.records.end(),m.id,[](auto& r,auto id){return r.id<id;});
    bool found=it!=s.records.end()&&it->id==m.id;
    if(m.code==MutationCode::append_record){if(found)wire::fail("append existing record");s.records.insert(it,m.record);return Status::ok;}
    if(!found)return Status::not_found;
    switch(m.code){
    case MutationCode::update_score:it->score=m.score;it->revision=m.revision;break;
    case MutationCode::replace_name:wire::valid_text(m.text,Limits{});it->name=m.text;break;
    case MutationCode::append_samples:it->samples.insert(it->samples.end(),m.samples.begin(),m.samples.end());break;
    case MutationCode::replace_payload:it->payload=m.bytes;break;
    case MutationCode::replace_attr:case MutationCode::insert_attr:{
        auto a=std::lower_bound(it->attrs.begin(),it->attrs.end(),m.text,[](auto& x,auto& key){return wire::byte_less(x.key,key);});
        bool exists=a!=it->attrs.end()&&a->key==m.text;
        if(m.code==MutationCode::replace_attr){if(!exists)return Status::not_found;a->value=m.bytes;}
        else{if(exists)wire::fail("insert existing attribute");it->attrs.insert(a,Attr{m.text,m.bytes});}break;
    }
    case MutationCode::erase_record:s.records.erase(it);break;
    default:wire::fail("unknown mutation");
    }return Status::ok;
}
} // namespace sc
