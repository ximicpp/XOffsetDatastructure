#pragma once
#include "adapter.hpp"

namespace sc {
template<class A> struct PlainAttrView {
    const A& a;
    std::string_view key()const{return {a.key.data(),a.key.size()};}
    std::span<const std::uint8_t> value()const{return {a.value.data(),a.value.size()};}
};
template<class R> struct PlainRecordView {
    const R& r;
    std::uint64_t id()const{return r.id;}
    std::uint32_t revision()const{return r.revision;}
    std::int64_t score()const{return r.score;}
    std::string_view name()const{return {r.name.data(),r.name.size()};}
    std::span<const std::uint32_t> samples()const{return {r.samples.data(),r.samples.size()};}
    std::span<const std::uint8_t> payload()const{return {r.payload.data(),r.payload.size()};}
    std::size_t attrs_size()const{return r.attrs.size();}
    auto attr(std::size_t i)const{return PlainAttrView{r.attrs[i]};}
};
template<class S> struct PlainSnapshotView {
    const S& s;
    std::uint32_t schema_version()const{return s.schema_version;}
    std::uint64_t epoch()const{return s.epoch;}
    std::size_t size()const{return s.records.size();}
    auto record(std::size_t i)const{return PlainRecordView{s.records[i]};}
};
template<class V> std::size_t find_record(const V& v,std::uint64_t id){
    std::size_t l=0,h=v.size();
    while(l<h){auto m=l+(h-l)/2;if(v.record(m).id()<id)l=m+1;else h=m;}
    return l<v.size()&&v.record(l).id()==id?l:v.size();
}
template<class Range> std::uint64_t byte_sum(const Range& r){
    std::uint64_t out=0;for(std::size_t i=0;i<r.size();++i)out+=std::uint8_t(r[i]);return out;
}
template<class V> void query_view(const V& v,std::span<const Query> qs,std::span<QueryResult> out){
    if(out.size()!=qs.size())throw std::runtime_error("query sink length mismatch");
    for(std::size_t qi=0;qi<qs.size();++qi){
        const auto& q=qs[qi];QueryResult z{};z.sequence=q.sequence;z.status=Status::ok;
        if(q.code==QueryCode::scan){
            z.values[0]=v.size();
            for(std::size_t i=0;i<v.size();++i){auto r=v.record(i);
                z.values[1]+=r.id();z.values[2]+=r.revision();z.values[3]+=std::uint64_t(r.score());
                z.values[4]+=r.name().size();auto ss=r.samples();
                for(std::size_t j=0;j<ss.size();++j)z.values[5]+=ss[j];
                z.values[6]+=r.payload().size();z.values[7]+=r.attrs_size();
            }
        }else{
            auto pos=find_record(v,q.id);
            if(pos==v.size())z.status=Status::not_found;
            else {auto r=v.record(pos);
                if(q.code==QueryCode::record){
                    auto n=r.name();auto ss=r.samples();auto p=r.payload();std::uint64_t sum=0;
                    for(std::size_t j=0;j<ss.size();++j)sum+=ss[j];
                    z.values={r.revision(),std::uint64_t(r.score()),n.size(),byte_sum(n),ss.size(),sum,p.size(),byte_sum(p)};
                }else if(q.code==QueryCode::attr){
                    std::size_t l=0,h=r.attrs_size();
                    while(l<h){auto m=l+(h-l)/2;if(r.attr(m).key()<q.key)l=m+1;else h=m;}
                    if(l==r.attrs_size()||r.attr(l).key()!=q.key)z.status=Status::not_found;
                    else {auto a=r.attr(l).value();z.values={a.size(),byte_sum(a),a.empty()?0:std::uint64_t(a[0]),a.empty()?0:std::uint64_t(a[a.size()-1]),0,0,0,0};}
                }else throw std::runtime_error("unknown query code");
            }
        }
        out[qi]=z;
    }
}
template<class V> Snapshot export_view(const V& v){
    Snapshot s;s.schema_version=v.schema_version();s.epoch=v.epoch();s.records.reserve(v.size());
    for(std::size_t i=0;i<v.size();++i){auto r=v.record(i);Record d;
        d.id=r.id();d.revision=r.revision();d.score=r.score();auto n=r.name();d.name.assign(n.data(),n.size());
        auto ss=r.samples();d.samples.reserve(ss.size());for(std::size_t j=0;j<ss.size();++j)d.samples.push_back(ss[j]);
        auto pp=r.payload();d.payload.assign(pp.begin(),pp.end());d.attrs.reserve(r.attrs_size());
        for(std::size_t j=0;j<r.attrs_size();++j){auto a=r.attr(j);auto k=a.key();auto b=a.value();
            d.attrs.push_back({std::string(k.data(),k.size()),Bytes(b.begin(),b.end())});}
        s.records.push_back(std::move(d));
    }
    return s;
}
} // namespace sc
