// Independent range/bounds/metadata and real PMR-failure regression. No timer.
#include "xoffsetdatastructure.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <memory_resource>
#include <string>
#include <vector>

namespace xo = XOffsetDatastructure;
namespace wd = xo::detail;
using Index = wd::WireValidationByteIndex;
namespace {
void need(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Range { std::size_t begin, end; };
bool overlap(const std::vector<Range>& ranges, std::size_t begin, std::size_t end) {
    for (const auto& range : ranges) if (begin < range.end && range.begin < end) return true;
    return false;
}
std::pair<std::size_t,std::size_t> envelope(const std::vector<Range>& ranges) {
    if (ranges.empty()) return {0,0};
    auto lo = ranges.front().begin, hi = ranges.front().end;
    for (const auto& range : ranges) { lo = std::min(lo,range.begin); hi = std::max(hi,range.end); }
    return {lo,hi};
}
struct Snapshot {
    std::size_t size, extent;
    std::pair<std::size_t,std::size_t> bounds;
    std::vector<std::uint64_t> words;
    bool operator==(const Snapshot&) const = default;
};
Snapshot snapshot(const Index& index) {
    Snapshot out{index.size(), index.extent(), index.coverage_bounds(), {}};
    for (std::size_t word=0; word<index.extent()/64+(index.extent()%64!=0); ++word)
        out.words.push_back(index.word_at(word));
    return out;
}
std::size_t oracle_operations=0, resource_failures=0, metadata_cases=0;

void compare_to_independent_intervals() {
    constexpr std::size_t N=12289;
    for (unsigned mode=0; mode!=3; ++mode) {
        alignas(64) std::array<char,N> arena{};
        wd::WireValidationContext ctx{.base=arena.data(), .end=arena.data()+arena.size(),
            .owned_ranges=Index{std::pmr::new_delete_resource(),mode!=1},
            .free_ranges=Index{std::pmr::new_delete_resource(),mode==0}};
        std::vector<Range> owned, free;
        auto step=[&](bool incoming_free,std::size_t begin,std::size_t bytes) {
            const auto before_owned=snapshot(ctx.owned_ranges), before_free=snapshot(ctx.free_ranges);
            std::string expected;
            if (bytes && bytes>N-begin) expected="bounds-first";
            else if (bytes && overlap(owned,begin,begin+bytes)) expected=incoming_free?
                "wire validation detected free-list overlap with live ranges":
                "wire validation detected overlapping dynamic ranges";
            else if (bytes && overlap(free,begin,begin+bytes)) expected=incoming_free?
                "wire validation detected overlapping free-list ranges":"wire validation detected live/free overlap";
            bool caught=false;
            try {
                if (incoming_free) wd::register_free_range(arena.data()+begin,bytes,ctx,"bounds-first");
                else wd::register_owned_range(arena.data()+begin,bytes,ctx,"bounds-first");
            } catch (const xo::XException& e) { caught=true; need(e.what()==expected,"first-error oracle differs"); }
            need(caught==!expected.empty(),"pairwise accept/reject differs");
            if (caught) need(snapshot(ctx.owned_ranges)==before_owned && snapshot(ctx.free_ranges)==before_free,
                             "rejection changed coverage/bbox/count/extent");
            else if (bytes) (incoming_free?free:owned).push_back({begin,begin+bytes});
            need(ctx.owned_ranges.coverage_bounds()==envelope(owned) &&
                 ctx.free_ranges.coverage_bounds()==envelope(free),"coverage bounds differ from independent envelope");
            ++oracle_operations;
        };
        step(false,4096,1);step(true,0,1);step(false,8192,1);
        step(false,0,8193);step(true,0,8193); // an early free hit cannot hide later owned
        step(false,N,1);step(true,N-1,2); // validated backing bounds remain first
        step(false,0,0);step(true,N,0);
        for (std::size_t i=0;i!=192;++i) step(i%2!=0,1024+17*i,1);
        // Deliberate holes within the aggregate envelope still need exact fallback.
        std::uint64_t random=0x768abcd321ULL;
        for (unsigned i=0;i!=320;++i) {
            random=random*6364136223846793005ULL+1442695040888963407ULL;
            const auto begin=std::size_t((random>>21)%(N+1));
            constexpr std::array<std::size_t,8> lengths{0,1,2,7,63,64,65,4097};
            step((random&1)!=0,begin,lengths[(random>>47)%lengths.size()]);
        }
        const auto before=snapshot(ctx.owned_ranges);
        wd::register_owned_range(nullptr,999,ctx,"null remains ignored");
        need(snapshot(ctx.owned_ranges)==before,"null behavior changed");
        need(Index::overlap_priority(ctx.owned_ranges,ctx.free_ranges,N,
             (std::numeric_limits<std::size_t>::max)())==0,"outside validated extent query");
    }
}

struct Resource final:std::pmr::memory_resource {
    std::size_t attempts{},fail_at=(std::numeric_limits<std::size_t>::max)(),live{},allocations{},deallocations{};
    void* do_allocate(std::size_t bytes,std::size_t alignment) override {
        if (attempts++==fail_at) throw std::bad_alloc();
        auto* p=std::pmr::new_delete_resource()->allocate(bytes,alignment);
        ++allocations;live+=bytes;return p;
    }
    void do_deallocate(void* p,std::size_t bytes,std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(p,bytes,alignment);++deallocations;live-=bytes;
    }
    bool do_is_equal(const std::pmr::memory_resource& other)const noexcept override{return this==&other;}
};
void prepare_failure_does_not_publish_bbox() {
    // Direct real upstream resource boundaries; inherited global-new/typed-load
    // tests remain enabled and are not replaced by these narrower failures.
    for (unsigned mode=0;mode!=3;++mode) for (unsigned point=0;point!=(mode==0?1u:3u);++point) {
        Resource resource;
        {
            Index index(&resource,mode!=1);
            if (mode==2) index.insert_disjoint(8,9,4095);
            const auto before=snapshot(index);const auto live=resource.live;
            resource.fail_at=resource.attempts+point;bool caught=false;
            try {
                if (mode==0) index.insert_disjoint(32,33,4096);
                else index.insert_disjoint(4097,12287,16384);
            } catch (const std::bad_alloc&) {caught=true;}
            need(caught && snapshot(index)==before && resource.live==live,"failed prepare published bbox or coverage");
            ++resource_failures;
            resource.fail_at=(std::numeric_limits<std::size_t>::max)();
            if (mode==0) index.insert_disjoint(32,33,4096);
            else index.insert_disjoint(4097,12287,16384);
            const auto expected=mode==0?std::pair<std::size_t,std::size_t>{32,33}:
                std::pair<std::size_t,std::size_t>{mode==2?8u:4097u,12287};
            need(index.coverage_bounds()==expected,"recovery envelope differs");
        }
        need(resource.live==0 && resource.allocations==resource.deallocations,"resource leak after bbox failure");
    }
}

// V2: blocks carry no header. register_allocation derives the extent from the
// owner's request (round8(request)), enforces max(alignment, 8) and registers
// exactly that extent. This replaces the V1 header metadata checks.
void derived_extent_error_order() {
    alignas(64) std::array<char,4096> arena{};
    constexpr const char* bounds="wire validation: allocation out of bounds";
    auto one=[&](const char* ptr,std::size_t request,std::size_t alignment,bool incoming_free,
                 const char* expected,std::size_t fixture_offset=0,std::size_t fixture_bytes=0,
                 bool fixture_free=false) {
        wd::WireValidationContext ctx{.base=arena.data(),.end=arena.data()+arena.size()};
        if (fixture_bytes) {
            if (fixture_free) wd::register_free_range(arena.data()+fixture_offset,fixture_bytes,ctx,"fixture");
            else wd::register_owned_range(arena.data()+fixture_offset,fixture_bytes,ctx,"fixture");
        }
        const auto owned=snapshot(ctx.owned_ranges),free=snapshot(ctx.free_ranges);
        const auto owned_bytes=ctx.owned_bytes,free_bytes=ctx.free_bytes;
        bool caught=false;
        try {wd::register_allocation(ptr,request,alignment,ctx,incoming_free);}
        catch (const xo::XException& e) {caught=true;need(std::string(e.what())==expected,"derived-extent first-error category differs");}
        need(caught==(expected[0]!='\0'),"derived-extent accept/reject differs");
        if (caught) {
            need(snapshot(ctx.owned_ranges)==owned && snapshot(ctx.free_ranges)==free &&
                 ctx.owned_bytes==owned_bytes && ctx.free_bytes==free_bytes,
                 "derived-extent rejection changed index envelope/state");
        } else {
            // The registered extent is exactly round8(request) at ptr.
            const auto begin=static_cast<std::size_t>(ptr-arena.data());
            const auto block=(request+7u)&~std::size_t(7u);
            const auto& target=incoming_free?ctx.free_ranges:ctx.owned_ranges;
            const auto& before=incoming_free?free:owned;
            auto was_covered=[&](std::size_t x){
                return x/64<before.words.size()&&((before.words[x/64]>>(x%64))&1u)!=0;
            };
            const auto lo=begin>=64?begin-64:0,hi=std::min(arena.size(),begin+block+64);
            for (auto x=lo;x<hi;++x)
                need(target.overlaps(x,x+1)==(was_covered(x)||(x>=begin&&x<begin+block)),
                     "derived extent is not round8(request)");
            need((incoming_free?ctx.free_bytes-free_bytes:ctx.owned_bytes-owned_bytes)==block,
                 "coverage byte count is not the derived extent");
        }
        ++metadata_cases;
    };
    for (bool incoming_free:{false,true}) {
        const char* live_overlap=incoming_free?"wire validation detected free-list overlap with live ranges":
            "wire validation detected overlapping dynamic ranges";
        const char* free_overlap=incoming_free?"wire validation detected overlapping free-list ranges":
            "wire validation detected live/free overlap";
        // Accepted: exact multiples of 8 and requests rounded up to 8.
        for (std::size_t request:{std::size_t{1},std::size_t{7},std::size_t{8},std::size_t{13},std::size_t{16},std::size_t{4095-127}})
            one(arena.data()+128,request,8,incoming_free,"");
        one(arena.data()+128,16,1,incoming_free,""); // alignment below 8 is raised to 8
        one(arena.data()+128,16,64,incoming_free,"");
        one(arena.data()+4088,8,8,incoming_free,""); // last block ends exactly at the extent
        // Every block is 8-byte aligned regardless of the element alignment.
        for (std::size_t offset:{std::size_t{1},std::size_t{4},std::size_t{17},std::size_t{124},std::size_t{132}})
            for (std::size_t alignment:{std::size_t{1},std::size_t{4},std::size_t{8}})
                one(arena.data()+offset,8,alignment,incoming_free,bounds);
        one(arena.data()+136,16,64,incoming_free,bounds); // 8-aligned, not 64-aligned
        one(arena.data()+136,16,16,incoming_free,bounds);
        // The derived (rounded) extent is bounds-checked, not the raw request.
        one(arena.data()+4088,9,8,incoming_free,bounds);
        one(arena.data()+4080,17,8,incoming_free,bounds);
        one(arena.data()+arena.size(),1,1,incoming_free,bounds);
        one(nullptr,8,8,incoming_free,bounds);
        // Rounding reaches a neighbouring block: [128,152) vs a block at 144.
        one(arena.data()+128,17,8,incoming_free,live_overlap,144,8,false);
        one(arena.data()+128,17,8,incoming_free,free_overlap,144,8,true);
        one(arena.data()+128,16,8,incoming_free,"",144,8,false); // same request without rounding: disjoint
        one(arena.data()+128,16,8,incoming_free,"",144,8,true);
        // Bounds remain the first error even when the block would also overlap.
        one(arena.data()+132,8,8,incoming_free,bounds,128,16,false);
    }
}
}
int main(){try{
    compare_to_independent_intervals();prepare_failure_does_not_publish_bbox();derived_extent_error_order();
    std::cout<<"{\"groups\":3,\"independent_interval_operations\":"<<oracle_operations
             <<",\"actual_resource_failure_points\":"<<resource_failures<<",\"derived_extent_cases\":"<<metadata_cases
             <<",\"timing_sample\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
