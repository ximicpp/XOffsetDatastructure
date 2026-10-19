// Source/functional/resource regression only; no clocks or benchmark fixture.
#include "xoffsetdatastructure.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory_resource>
#include <string>
#include <vector>

namespace xo=XOffsetDatastructure;
namespace wd=xo::detail;
using Index=wd::WireValidationByteIndex;
namespace {
void need(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
struct Resource final:std::pmr::memory_resource {
    struct Entry{void* ptr{};std::size_t bytes{},alignment{};};
    std::array<Entry,128> entries{};
    std::size_t attempts{},allocations{},deallocations{},requested{},released{},live{};
    std::size_t fail_at=(std::numeric_limits<std::size_t>::max)();
    void* do_allocate(std::size_t n,std::size_t a)override{
        if(attempts++==fail_at)throw std::bad_alloc();
        auto* p=std::pmr::new_delete_resource()->allocate(n,a);
        auto slot=std::find_if(entries.begin(),entries.end(),[](const auto& e){return !e.ptr;});
        if(slot==entries.end()){std::pmr::new_delete_resource()->deallocate(p,n,a);throw std::runtime_error("ledger exhausted");}
        *slot={p,n,a};++allocations;requested+=n;live+=n;
        // A missed any/full guard must not silently work because malloc was zero.
        std::memset(p,0xa5,n);return p;
    }
    void do_deallocate(void* p,std::size_t n,std::size_t a)override{
        auto slot=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return e.ptr==p;});
        need(slot!=entries.end()&&slot->bytes==n&&slot->alignment==a,"deallocation identity mismatch");
        std::pmr::new_delete_resource()->deallocate(p,n,a);*slot={};++deallocations;released+=n;live-=n;
    }
    bool do_is_equal(const std::pmr::memory_resource& other)const noexcept override{return this==&other;}
    void balanced()const{need(!live&&requested==released&&allocations==deallocations,"resource lifetime imbalance");}
};
struct State {
    std::size_t extent{},size{},prefix{},dense_bytes{},directory{},pages{};
    std::vector<std::uint64_t> words;
    bool operator==(const State&)const=default;
};
State snapshot(const Index& x){
    State out{x.extent(),x.size(),x.dense_prefix_bytes(),x.dense_requested_bytes(),x.directory_entries(),x.allocated_pages(),{}};
    for(std::size_t w=0;w<x.extent()/64+(x.extent()%64!=0);++w)out.words.push_back(x.word_at(w));
    return out;
}
struct Range{std::size_t begin,end;};
bool overlaps(const std::vector<Range>& values,std::size_t b,std::size_t e){
    for(const auto& x:values)if(b<x.end&&x.begin<e)return true;
    return false;
}
std::uint64_t oracle_word(const std::vector<std::uint8_t>& bytes,std::size_t word){
    std::uint64_t result=0;
    for(unsigned bit=0;bit<64;++bit){const auto at=word*64+bit;if(at<bytes.size()&&bytes[at])result|=std::uint64_t{1}<<bit;}
    return result;
}
void verify(const Index& x,const std::vector<Range>& values,const std::vector<std::uint8_t>& bytes){
    need(x.size()==values.size(),"registration count differs from interval oracle");
    const auto arena_bytes=bytes.size();
    for(std::size_t w=0;w<arena_bytes/64+(arena_bytes%64!=0);++w)
        need(x.word_at(w)==oracle_word(bytes,w),"byte-exact word differs from independent byte coverage");
    need(x.word_at(arena_bytes/64+(arena_bytes%64!=0))==0,"word outside extent became coverage");
}

std::size_t oracle_cases=0,failures=0;
void independent_pairwise_oracle(){
    constexpr std::size_t N=12289;
    for(unsigned mode=0;mode<3;++mode){
        alignas(64)std::array<char,N> arena{};
        wd::WireValidationContext ctx{.base=arena.data(),.end=arena.data()+arena.size(),
            .owned_ranges=Index{std::pmr::new_delete_resource(),mode!=1},
            .free_ranges=Index{std::pmr::new_delete_resource(),mode==0}};
        std::vector<Range> owned,free;
        std::vector<std::uint8_t> owned_bytes(N),free_bytes(N);
        std::uint64_t seed=0x4f3a219b5c7dULL;
        auto step=[&](bool is_free,std::size_t b,std::size_t length){
            const auto before_owned=snapshot(ctx.owned_ranges),before_free=snapshot(ctx.free_ranges);
            std::string expected;
            if(length&&length>N-b)expected="oracle bounds";
            else if(length&&overlaps(owned,b,b+length))expected=is_free?
                "wire validation detected free-list overlap with live ranges":"wire validation detected overlapping dynamic ranges";
            else if(length&&overlaps(free,b,b+length))expected=is_free?
                "wire validation detected overlapping free-list ranges":"wire validation detected live/free overlap";
            bool caught=false;
            try{if(is_free)wd::register_free_range(arena.data()+b,length,ctx,"oracle bounds");
                else wd::register_owned_range(arena.data()+b,length,ctx,"oracle bounds");}
            catch(const xo::XException& e){caught=true;need(e.what()==expected,"first error category differs from oracle");}
            need(caught==!expected.empty(),"accept/reject differs from pairwise oracle");
            if(caught){need(snapshot(ctx.owned_ranges)==before_owned&&snapshot(ctx.free_ranges)==before_free,"rejection changed coverage/count/extent");}
            else if(length){
                (is_free?free:owned).push_back({b,b+length});
                auto& bytes=is_free?free_bytes:owned_bytes;
                std::fill(bytes.begin()+b,bytes.begin()+b+length,std::uint8_t{1});
            }
            verify(ctx.owned_ranges,owned,owned_bytes);verify(ctx.free_ranges,free,free_bytes);++oracle_cases;
        };
        // Independent byte intervals cross words/pages, including non-word
        // alignments. No bitmap mask algorithm is used in this reference.
        for(std::size_t b=0;b<256;b+=3)step((b/3)%2,b,1);
        for(unsigned i=0;i<768;++i){
            seed=seed*6364136223846793005ULL+1442695040888963407ULL;
            const auto b=std::size_t((seed>>17)%(N+1));
            constexpr std::array<std::size_t,12> lengths{0,1,2,7,31,63,64,65,127,255,4096,4097};
            step((seed&1)!=0,b,lengths[(seed>>45)%lengths.size()]);
        }
    }
}

void summary_transitions_and_poison(){
    for(bool dense:{false,true}){
        Resource r;
        {
            Index x(&r,dense);
            x.insert_disjoint(1,2,8193);need(x.word_at(0)==2&&!x.overlaps(2,64),"poison read as empty word data");
            x.insert_disjoint(0,1,8193);x.insert_disjoint(2,64,8193);
            need(x.word_at(0)==UINT64_MAX&&x.overlaps(63,64),"partial to full word transition");
            x.insert_disjoint(64,4096,8193);x.insert_disjoint(4096,8192,8193);
            x.insert_disjoint(8192,8193,8193);
            for(std::size_t w=0;w<128;++w)need(x.word_at(w)==UINT64_MAX,"full-word/page synthesis differs");
            need(x.word_at(128)==1&&x.word_at(129)==0&&!x.overlaps(8193,8194),"last-page padding admitted");
            need(x.size()==6&&x.allocated_pages()==(dense?0u:2u),"partial Page lost on promotion or full-page allocated data");
        }
        r.balanced();
        Resource tagged;
        {
            Index x(&tagged,dense);x.insert_disjoint(4096,8192,12288);
            need(!x.overlaps(0,4096)&&x.overlaps(4097,4098)&&!x.overlaps(8192,12288),"full page guard lost exact boundaries");
            need(x.word_at(0)==0&&x.word_at(64)==UINT64_MAX&&x.word_at(128)==0,"empty/full data words read poison");
            need(x.allocated_pages()==0,"full page should use summary/tag");
        }
        tagged.balanced();
    }
}

void threshold_and_sparse_budget(){
    constexpr auto budget=Index::dense_byte_budget,limit=Index::dense_extent_limit;
    static_assert(budget==8u*1024u*1024u+2048u);
    static_assert(limit==65089536u);
    for(auto extent:{std::size_t{1},std::size_t{4096},limit-1,limit,limit+1,std::size_t{512u*1024u*1024u}}){
        Resource r;
        {
            Index x(&r);x.insert_disjoint(extent-1,extent,extent);
            need(x.word_at((extent-1)/64)==(std::uint64_t{1}<<((extent-1)%64)),"sparse last byte mismatch");
            need(!x.overlaps(0,extent-1)&&!x.overlaps(extent,extent+1),"sparse empty region or padding mismatch");
            const auto pages=extent/4096+(extent%4096!=0);
            if(extent<=limit){need(x.dense_requested_bytes()==pages*528&&r.requested==pages*528&&r.requested<=budget,"dense hard byte budget exceeded");}
            else need(!x.dense_prefix_bytes()&&x.allocated_pages()==1&&r.requested==pages*sizeof(std::uintptr_t)+528,"large sparse path reserved dense input-sized bytes");
        }
        r.balanced();
    }
}

void prepare_failures_and_growth(){
    // PMR-boundary throws, not a claim of injecting global operator-new here.
    // Existing real-global-new/typed-load sweeps remain separate unchanged tests.
    for(unsigned scenario=0;scenario<3;++scenario){
        const unsigned points=scenario==0?1u:3u;
        for(unsigned point=0;point<points;++point){
            Resource r;
            {
                Index x(&r,scenario!=1);
                if(scenario==2)x.insert_disjoint(8,9,4095);
                const auto before=snapshot(x);const auto live=r.live;
                r.fail_at=r.attempts+point;bool caught=false;
                try{
                    if(scenario==0)x.insert_disjoint(1,2,4096);
                    else x.insert_disjoint(4097,12287,16384);
                }catch(const std::bad_alloc&){caught=true;}
                need(caught&&snapshot(x)==before&&r.live==live,"prepare failure committed coverage or leaked resource");++failures;
                r.fail_at=(std::numeric_limits<std::size_t>::max)();
                if(scenario==0)x.insert_disjoint(1,2,4096);else x.insert_disjoint(4097,12287,16384);
                need(x.size()==(scenario==2?2u:1u)&&x.overlaps(4097,4098)==(scenario!=0),"recovery failed after allocation error");
                if(scenario==2){
                    need(x.dense_prefix_bytes()==4096&&x.overlaps(8,9),"dense prefix moved/lost on tail growth");
                    x.insert_disjoint(4096,4097,16384);x.insert_disjoint(12287,12288,16384);
                    for(std::size_t w=64;w<192;++w)need(x.word_at(w)==UINT64_MAX,"grown paged partial-to-full mismatch");
                }
            }
            r.balanced();
        }
    }
}

void global_owned_priority_and_isolation(){
    alignas(64)std::array<char,12288> arena{};
    for(bool owned_dense:{false,true}){
        Resource a,b;
        {
            wd::WireValidationContext ctx{.base=arena.data(),.end=arena.data()+arena.size(),
                .owned_ranges=Index{&a,owned_dense},.free_ranges=Index{&b,!owned_dense}};
            wd::register_free_range(arena.data(),1,ctx,"bounds");
            wd::register_owned_range(arena.data()+8192,1,ctx,"bounds");
            const auto o=snapshot(ctx.owned_ranges),f=snapshot(ctx.free_ranges);
            for(bool incoming_free:{false,true}){
                bool caught=false;
                try{if(incoming_free)wd::register_free_range(arena.data(),12288,ctx,"bounds");
                    else wd::register_owned_range(arena.data(),12288,ctx,"bounds");}
                catch(const xo::XException& e){caught=true;need(std::string(e.what())==(incoming_free?
                    "wire validation detected free-list overlap with live ranges":"wire validation detected overlapping dynamic ranges"),"early free hit hid later owned hit");}
                need(caught&&snapshot(ctx.owned_ranges)==o&&snapshot(ctx.free_ranges)==f,"global-priority rejection changed state");
            }
        }
        a.balanced();b.balanced();
    }
    Resource owned,free;
    {
        wd::WireValidationContext ctx{.base=arena.data(),.end=arena.data()+arena.size(),
            .owned_ranges=Index{&owned},.free_ranges=Index{&free}};
        wd::register_owned_range(arena.data(),1,ctx,"bounds");const auto before=snapshot(ctx.owned_ranges);
        free.fail_at=0;bool caught=false;
        try{wd::register_free_range(arena.data()+128,1,ctx,"bounds");}catch(const std::bad_alloc&){caught=true;}
        need(caught&&snapshot(ctx.owned_ranges)==before&&ctx.free_ranges.empty()&&free.live==0,"second index resource failure modified first index");
        free.fail_at=(std::numeric_limits<std::size_t>::max)();wd::register_free_range(arena.data()+128,1,ctx,"bounds");++failures;
    }
    owned.balanced();free.balanced();
}
}
int main(){try{
    independent_pairwise_oracle();summary_transitions_and_poison();threshold_and_sparse_budget();
    prepare_failures_and_growth();global_owned_priority_and_isolation();
    std::cout<<"{\"pairwise_oracle_operations\":"<<oracle_cases<<",\"PMR_failure_points\":"<<failures
             <<",\"dense_extent_limit\":"<<Index::dense_extent_limit<<",\"dense_logical_byte_budget\":"<<Index::dense_byte_budget
             <<",\"groups\":5,\"timing_sample\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
