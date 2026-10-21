// No timer. Independent per-arena-byte truth, real registration and typed load.
#include "xoffsetdatastructure.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory_resource>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace xo = XOffsetDatastructure;
namespace wd = xo::detail;
using Index = wd::WireValidationByteIndex;
struct SingleWordRoot {
    xo::XVector<std::uint8_t> left;
    xo::XVector<std::uint8_t> right;
};
XOFFSET_REGISTER_SCHEMA_NAME(SingleWordRoot, "test.v8.SingleWordRoot")

namespace {
const char* stage = "entry";
std::size_t endpoint_cases{}, register_cases{}, injected_failures{}, typed_rejections{};
void need(bool yes, const char* what) { if (!yes) throw std::runtime_error(what); }
struct Image {
    std::size_t count, extent, prefix, requested, directory, pages;
    std::pair<std::size_t,std::size_t> bounds;
    std::vector<std::uint64_t> words;
    bool operator==(const Image&) const = default;
};
Image image(const Index& x) {
    Image r{x.size(),x.extent(),x.dense_prefix_bytes(),x.dense_requested_bytes(),
            x.directory_entries(),x.allocated_pages(),x.coverage_bounds(),{}};
    for (std::size_t w=0;w<x.extent()/64+(x.extent()%64!=0);++w) r.words.push_back(x.word_at(w));
    return r;
}
template<std::size_t N>
void exact(const Index& x, const std::array<unsigned char,N>& bytes, std::size_t count) {
    need((x.extent()==N || (x.extent()==0 && count==0)) && x.size()==count,"oracle extent/count differs");
    for (std::size_t w=0;w<N/64+(N%64!=0);++w) {
        std::uint64_t expected=0;
        // Deliberately not the candidate's masks, page arithmetic or summaries.
        for (unsigned b=0;b<64 && w*64+b<N;++b)
            if (bytes[w*64+b]) expected|=std::uint64_t{1}<<b;
        need(x.word_at(w)==expected,"precise byte occupancy differs");
    }
}
template<std::size_t N>
bool any(const std::array<unsigned char,N>& bytes,std::size_t b,std::size_t e) {
    for (auto p=b;p<e;++p) if (bytes[p]) return true;
    return false;
}
struct Resource final : std::pmr::memory_resource {
    std::size_t attempts{}, allocations{}, frees{}, live{}, peak{}, largest{};
    std::size_t fail_at=(std::numeric_limits<std::size_t>::max)();
    bool poison=true;
    void* do_allocate(std::size_t n,std::size_t a) override {
        if (attempts++==fail_at) throw std::bad_alloc();
        auto* p=std::pmr::new_delete_resource()->allocate(n,a);
        if (poison) std::memset(p,0xa5,n);
        ++allocations;live+=n;peak=std::max(peak,live);largest=std::max(largest,n);return p;
    }
    void do_deallocate(void* p,std::size_t n,std::size_t a) override {
        std::pmr::new_delete_resource()->deallocate(p,n,a);++frees;live-=n;
    }
    bool do_is_equal(const std::pmr::memory_resource& r) const noexcept override { return this==&r; }
};
void balanced(const Resource& r) { need(r.live==0 && r.allocations==r.frees,"scratch resource not balanced"); }

void all_word_endpoints() {
    constexpr std::size_t extent=192, offset=64;
    // Every nonempty interval in a word, including bit63 and a full64 mask,
    // for both target sets, with empty, owned/free holes, and full summaries.
    for (unsigned pattern=0;pattern<4;++pattern)
        for (unsigned incoming=0;incoming<2;++incoming)
            for (std::size_t lo=0;lo<64;++lo) for (std::size_t hi=lo+1;hi<=64;++hi) {
                Index owned,free;
                std::array<unsigned char,extent> o{},f{};
                owned.insert_disjoint(0,1,extent);o[0]=1;
                free.insert_disjoint(191,192,extent);f[191]=1;
                std::size_t oc=1,fc=1;
                for (std::size_t b=0;b<64;++b) {
                    if ((pattern==1 && b%7==1) || pattern==2) {
                        owned.insert_disjoint(offset+b,offset+b+1,extent);o[offset+b]=1;++oc;
                    } else if ((pattern==1 && b%11==2) || pattern==3) {
                        free.insert_disjoint(offset+b,offset+b+1,extent);f[offset+b]=1;++fc;
                    }
                }
                const auto before_o=image(owned),before_f=image(free);
                const unsigned expected=any(o,offset+lo,offset+hi)?1:any(f,offset+lo,offset+hi)?2:0;
                unsigned result=99;
                need(Index::try_insert_dense_single_page(owned,free,bool(incoming),offset+lo,offset+hi,extent,result),
                     "eligible single-word range was unhandled");
                need(result==expected,"single-word conflict priority differs");
                if (expected) need(image(owned)==before_o && image(free)==before_f,"rejected word published state");
                else {
                    auto& bytes=incoming?f:o;
                    for (auto p=offset+lo;p<offset+hi;++p) bytes[p]=1;
                    if (incoming) ++fc;else ++oc;
                }
                exact(owned,o,oc);exact(free,f,fc);++endpoint_cases;
            }
}

void actual_registration_and_fallback_oracle() {
    constexpr std::size_t extent=8193;
    for (unsigned mode=0;mode<4;++mode) {
        alignas(64) std::array<char,extent> arena{};
        wd::WireValidationContext ctx{.base=arena.data(),.end=arena.data()+extent,
            .owned_ranges=Index{std::pmr::new_delete_resource(),bool(mode&1)},
            .free_ranges=Index{std::pmr::new_delete_resource(),bool(mode&2)}};
        std::array<unsigned char,extent> o{},f{};std::size_t oc=0,fc=0;
        auto step=[&](bool is_free,std::size_t begin,std::size_t count) {
            need(begin<=extent,"invalid fixture pointer");
            const auto before_o=image(ctx.owned_ranges),before_f=image(ctx.free_ranges);
            const char* expected=nullptr;
            if (count>extent-begin) expected="bounds-before-overlap";
            else if (count && any(o,begin,begin+count)) expected=is_free?
                "wire validation detected free-list overlap with live ranges":
                "wire validation detected overlapping dynamic ranges";
            else if (count && any(f,begin,begin+count)) expected=is_free?
                "wire validation detected overlapping free-list ranges":
                "wire validation detected live/free overlap";
            bool caught=false;
            try {
                if (is_free) wd::register_free_range(arena.data()+begin,count,ctx,"bounds-before-overlap");
                else wd::register_owned_range(arena.data()+begin,count,ctx,"bounds-before-overlap");
            } catch (const xo::XException& e) {caught=true;need(expected && std::string(e.what())==expected,"real first-error differs");}
            need(caught==bool(expected),"real registration acceptance differs");
            if (caught) need(image(ctx.owned_ranges)==before_o && image(ctx.free_ranges)==before_f,"rejection mutated indexes");
            else if (count) {auto& bytes=is_free?f:o;for (auto p=begin;p<begin+count;++p) bytes[p]=1;if(is_free)++fc;else++oc;}
            exact(ctx.owned_ranges,o,oc);exact(ctx.free_ranges,f,fc);++register_cases;
        };
        // Both sets prepared; free earlier in the same word must not hide owned.
        step(false,8192,1);step(true,8000,1);step(false,114,1);step(true,65,1);
        step(false,64,64);step(true,64,64); // both domains conflict, owned wins
        step(false,100,1);step(false,120,1);step(true,121,1);
        step(false,122,1);step(false,121,1);step(true,100,1);
        step(false,63,2);step(true,4095,2); // crossword and crosspage fallback
        step(false,4096,1);step(true,4097,1);
        for (std::size_t b=192;b<256;++b) step(false,b,1); // partial -> full
        step(false,192,64);step(true,255,1);
        step(true,256,64);step(false,300,1); // whole-word summary then partial reject
        step(false,extent,0);step(true,0,0);step(false,extent,1);step(true,extent-1,2);
        std::uint64_t seed=0x1288eea455002211ULL;
        constexpr std::array<std::size_t,11> counts{0,1,2,7,8,31,63,64,65,4095,4097};
        for (unsigned i=0;i<384;++i) {
            seed=seed*6364136223846793005ULL+1442695040888963407ULL;
            step(bool(seed&1),std::size_t((seed>>17)%(extent+1)),counts[(seed>>45)%counts.size()]);
        }
        const auto before_o=image(ctx.owned_ranges),before_f=image(ctx.free_ranges);
        wd::register_owned_range(nullptr,123,ctx,"null");wd::register_free_range(nullptr,123,ctx,"null");
        need(image(ctx.owned_ranges)==before_o && image(ctx.free_ranges)==before_f,"null changed state");
    }
}

void poisoned_prepared_and_boundaries() {
    Resource r;
    {
        Index owned(&r),free(&r);
        owned.insert_disjoint(100,101,8192);free.insert_disjoint(5000,5001,8192);
        need(owned.word_at(2)==0 && free.word_at(2)==0,"uninitialized data leaked through empty summary");
        r.fail_at=r.attempts;
        auto invoke=[&](bool is_free,std::size_t b,std::size_t e,unsigned expected) {
            const auto before_o=image(owned),before_f=image(free);const auto requests=r.attempts;
            unsigned result=77;
            need(Index::try_insert_dense_single_page(owned,free,is_free,b,e,8192,result),"prepared word unhandled");
            need(result==expected && r.attempts==requests,"prepared path allocated or misclassified");
            if (expected) need(image(owned)==before_o && image(free)==before_f,"prepared reject changed state");
        };
        invoke(false,120,121,0); // both bounds disjoint but neighboring bit100 exists
        need(owned.word_at(1)==((std::uint64_t{1}<<36)|(std::uint64_t{1}<<56)),"bbox lost neighbor");
        invoke(false,128,192,0); // full64: no shift64 and no physical data read
        need(owned.word_at(2)==UINT64_MAX,"full summary synthesis failed");
        invoke(true,160,161,1);invoke(false,191,192,1);
        invoke(true,255,256,0);invoke(true,192,255,0); // mask union promotes full
        need(free.word_at(3)==UINT64_MAX,"partial union full promotion failed");
        invoke(false,200,201,2);invoke(true,192,256,2);
        // Word crossing remains handled by old B; page crossing remains unhandled.
        invoke(false,319,321,0);
        const auto before_o=image(owned),before_f=image(free);
        for (auto pair:std::array<std::pair<std::size_t,std::size_t>,5>{{
            {0,0},{4095,4097},{8192,8193},{(std::numeric_limits<std::size_t>::max)()-63,(std::numeric_limits<std::size_t>::max)()},
            {1,(std::numeric_limits<std::size_t>::max)()}}}) {
            unsigned untouched=77;
            need(!Index::try_insert_dense_single_page(owned,free,false,pair.first,pair.second,8192,untouched),"invalid/page path handled");
            need(untouched==77,"unhandled result modified");
        }
        unsigned untouched=77;
        need(!Index::try_insert_dense_single_page(owned,free,false,400,401,12288,untouched),"extent growth handled without prepare");
        need(image(owned)==before_o && image(free)==before_f,"unhandled path mutated state");
    }
    balanced(r);
}

void high_offsets_and_budget() {
    Resource r;
    {
        Index owned(&r),free(&r);
        constexpr auto limit=Index::dense_extent_limit;
        owned.insert_disjoint(0,1,limit);free.insert_disjoint(1,2,limit);
        need(owned.dense_requested_bytes()<=Index::dense_byte_budget &&
             free.dense_requested_bytes()<=Index::dense_byte_budget && r.largest<=Index::dense_byte_budget,
             "dense per-index hard budget exceeded");
        const auto requests=r.attempts;r.fail_at=requests;
        unsigned overlap=77;
        need(Index::try_insert_dense_single_page(owned,free,false,limit-64,limit,limit,overlap) && overlap==0,
             "last dense whole word unhandled");
        need(owned.word_at(limit/64-1)==UINT64_MAX && r.attempts==requests,"last dense word allocated or differs");
    }
    balanced(r);
    Resource tail;
    {
        Index owned(&tail),free(&tail);
        const auto high=Index::dense_extent_limit+4096;
        owned.insert_disjoint(high+1,high+2,high+4096); // initial extent > hard dense limit
        need(owned.dense_prefix_bytes()==0 && owned.allocated_pages()==1,"high sparse fallback changed representation");
        const auto requests=tail.attempts;unsigned result=77;
        need(!Index::try_insert_dense_single_page(owned,free,false,high+3,high+4,high+4096,result) && result==77,
             "high sparse target accepted as dense");
        need(tail.attempts==requests && owned.word_at(high/64)==2,"high sparse occupancy changed");
    }
    balanced(tail);
}

void fallback_oom_rollback() {
    for (unsigned scenario=0;scenario<4;++scenario)
        for (unsigned point=0;point<(scenario==3?3u:1u);++point) {
            Resource r;
            {
                alignas(64) std::array<char,16384> arena{};
                wd::WireValidationContext ctx{.base=arena.data(),.end=arena.data()+4096,
                    .owned_ranges=Index{&r,scenario!=3},.free_ranges=Index{&r,scenario!=3}};
                if (scenario==1 || scenario==2) wd::register_owned_range(arena.data()+100,1,ctx,"prime");
                if (scenario==2) ctx.end=arena.data()+8192;
                if (scenario==3) ctx.end=arena.data()+arena.size();
                const auto before_o=image(ctx.owned_ranges),before_f=image(ctx.free_ranges);const auto live=r.live;
                r.fail_at=r.attempts+point;bool caught=false;
                try {
                    if (scenario==1) wd::register_free_range(arena.data()+200,1,ctx,"cold free");
                    else if (scenario==3) wd::register_owned_range(arena.data()+4095,4098,ctx,"two partial pages");
                    else wd::register_owned_range(arena.data()+120,1,ctx,"cold or growth");
                } catch (const std::bad_alloc&) {caught=true;}
                need(caught,"expected real fallback allocation failure missing");
                need(image(ctx.owned_ranges)==before_o && image(ctx.free_ranges)==before_f && r.live==live,
                     "fallback failure published bits/bounds/count/extent or leaked");
                ++injected_failures;
            }
            balanced(r);
        }
}

template<class T> T read(const std::string& wire,std::size_t p) {
    need(p<=wire.size() && sizeof(T)<=wire.size()-p,"fixture read bounds");
    T value{};std::memcpy(&value,wire.data()+p,sizeof(value));return value;
}
template<class T> void write(std::string& wire,std::size_t p,T value) {
    need(p<=wire.size() && sizeof(T)<=wire.size()-p,"fixture write bounds");
    std::memcpy(wire.data()+p,&value,sizeof(value));
}
void point_right_at(std::string& wire,const xo::XWireHeaderV2& h,std::size_t payload_offset) {
    need(h.extent_count==0,"fixture wire is sparse; payload offsets assume a dense wire");
    const auto origin=sizeof(h)+h.root_offset+offsetof(SingleWordRoot,right)+offsetof(xo::XFixedVector<std::uint8_t>,data_delta);
    const auto target=sizeof(h)+payload_offset;
    const auto delta=static_cast<std::int64_t>(target)-static_cast<std::int64_t>(origin);
    need(delta>=INT32_MIN && delta<=INT32_MAX,"fixture rel32 range");
    write(wire,origin,static_cast<std::int32_t>(delta));
}
void rejects_unchanged(const std::string& malformed,const char* expected) {
    const auto before=malformed;bool caught=false;
    try {(void)xo::XBuffer::load_verified<SingleWordRoot>(malformed);}
    catch(const xo::XException& e) {caught=true;need(std::string(e.what())==expected,"typed rejection error priority differs");}
    need(caught && malformed==before,"typed hostile wire accepted or caller bytes mutated");++typed_rejections;
}
void typed_load_malicious_wire() {
    auto buffer=xo::XBuffer::create<SingleWordRoot>(32768);
    auto& root=buffer.root<SingleWordRoot>();root.left.resize(16);root.right.resize(16);
    for (unsigned i=0;i<16;++i) {root.left[i]=std::uint8_t(i+1);root.right[i]=std::uint8_t(100+i);}
    const auto left_offset=std::size_t(reinterpret_cast<const char*>(root.left.data())-static_cast<const char*>(buffer.get_address()));
    // V2: no block header; the live block is exactly [data, data+round8(16)).
    const auto left_begin=left_offset;
    need(left_begin%8==0 && left_begin/64==(left_offset+16-1)/64,"fixture live allocation is not one word");
    const auto original=buffer.save_verified<SingleWordRoot>();const auto h=read<xo::XWireHeaderV2>(original,0);
    need(h.hole_count==0 && h.hole_bytes==0,"fresh fixture unexpectedly has holes");
    auto loaded=xo::XBuffer::load_verified<SingleWordRoot>(original);
    need(loaded.root<SingleWordRoot>().left[7]==8 && loaded.root<SingleWordRoot>().right[7]==107,"valid typed control differs");
    auto live_overlap=original;point_right_at(live_overlap,h,left_offset);
    rejects_unchanged(live_overlap,"wire validation detected overlapping dynamic ranges");

    auto retired=xo::XBuffer::create<SingleWordRoot>(32768);
    auto& r=retired.root<SingleWordRoot>();r.left.resize(16);r.right.resize(16);
    for(unsigned i=0;i<16;++i) {r.left[i]=std::uint8_t(i+1);r.right[i]=std::uint8_t(100+i);}
    r.left.reserve(32); // right is live after old left, so the old16 block retires.
    const auto valid=retired.save_verified<SingleWordRoot>();const auto header=read<xo::XWireHeaderV2>(valid,0);
    // V2: the retired block travels in the wire hole table, not an in-arena free list.
    need(header.hole_count==1 && header.hole_bytes==16,"fixture must contain exactly one retired allocation");
    const auto hole=read<xo::XBufferCore::WireHole>(valid,sizeof(header)+header.used_bytes);
    const std::size_t free_payload=hole.offset;
    const auto begin=free_payload;
    need(hole.bytes==16 && begin%8==0 && begin/64==(begin+hole.bytes-1)/64,"fixture retired block is not one word16");
    auto good=xo::XBuffer::load_verified<SingleWordRoot>(valid);
    need(good.root<SingleWordRoot>().left[7]==8 && good.root<SingleWordRoot>().right[7]==107,"valid retired typed control differs");
    auto live_free=valid;point_right_at(live_free,header,free_payload);
    rejects_unchanged(live_free,"wire validation detected live/free overlap");
    // Original controls remain reloadable after both malformed attempts.
    (void)xo::XBuffer::load_verified<SingleWordRoot>(original);
    (void)xo::XBuffer::load_verified<SingleWordRoot>(valid);
}
}

int main() {try {
    stage="all-word-endpoints";all_word_endpoints();
    stage="actual-registration-oracle";actual_registration_and_fallback_oracle();
    stage="poisoned-prepared-and-boundaries";poisoned_prepared_and_boundaries();
    stage="high-offsets-and-budget";high_offsets_and_budget();
    stage="fallback-oom-rollback";fallback_oom_rollback();
    stage="typed-load-malicious-wire";typed_load_malicious_wire();
    std::cout<<"{\"groups\":6,\"endpoint_cases\":"<<endpoint_cases<<",\"registration_cases\":"<<register_cases
             <<",\"actual_upstream_failures\":"<<injected_failures<<",\"typed_rejections\":"<<typed_rejections
             <<",\"timing_sample\":false}\n";return 0;
} catch(const std::exception& e) {std::cerr<<stage<<": "<<e.what()<<'\n';return 1;}}
