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

void header_metadata_error_order() {
    using Header=xo::XBufferCore::AllocationHeader;
    alignas(64) std::array<char,4096> arena{};
    auto one=[&](std::size_t offset,Header header,std::size_t request,std::size_t alignment,
                 bool incoming_free,const char* expected,bool collide=false) {
        wd::WireValidationContext ctx{.base=arena.data(),.end=arena.data()+arena.size()};
        if (collide) wd::register_owned_range(arena.data()+offset-sizeof(Header),64,ctx,"fixture");
        if (offset>=sizeof(Header)) std::memcpy(arena.data()+offset-sizeof(Header),&header,sizeof(header));
        const auto owned=snapshot(ctx.owned_ranges),free=snapshot(ctx.free_ranges);
        bool caught=false;
        try {wd::register_allocation(arena.data()+offset,request,alignment,ctx,incoming_free);}
        catch (const xo::XException& e) {caught=true;need(std::string(e.what())==expected,"metadata first-error category differs");}
        need(caught==(expected[0]!='\0'),"metadata accept/reject differs");
        if (caught) need(snapshot(ctx.owned_ranges)==owned && snapshot(ctx.free_ranges)==free,
                         "metadata rejection changed index envelope/state");
        ++metadata_cases;
    };
    const Header valid{48,32,128,8};
    one(128,valid,16,8,false,"");one(128,valid,16,8,true,"");
    for (bool incoming_free:{false,true}) {
        for (std::uint32_t alignment:{0u,1u,4u,7u,24u,0x7ffffff9u,0x80000000u}) {
            auto header=valid;header.alignment=alignment;
            one(128,header,16,8,incoming_free,"wire validation: invalid allocation metadata",true);
        }
        auto header=valid;header.user_offset=127;
        one(128,header,16,8,incoming_free,"wire validation: invalid allocation metadata",true);
        header=valid;header.user_bytes=8;
        one(128,header,16,8,incoming_free,"wire validation: invalid allocation metadata",true);
        header=valid;header.total_bytes=49;
        one(128,header,16,8,incoming_free,"wire validation: invalid allocation metadata",true);
        header={48,32,144,64}; // requested8 alignment passes, header64 does not
        one(144,header,16,8,incoming_free,"wire validation: invalid allocation metadata",true);
        header={3016,3000,2048,8};
        one(2048,header,8,8,incoming_free,incoming_free?"wire validation: invalid free allocation":
            "wire validation: invalid live allocation");
        one(8,{},8,8,incoming_free,"wire validation: missing allocation header");
        one(17,{},1,1,incoming_free,"wire validation: invalid allocation header address");
        one(arena.size(),{},1,1,incoming_free,"wire validation: allocation out of bounds");
    }
}
}
int main(){try{
    compare_to_independent_intervals();prepare_failure_does_not_publish_bbox();header_metadata_error_order();
    std::cout<<"{\"groups\":3,\"independent_interval_operations\":"<<oracle_operations
             <<",\"actual_resource_failure_points\":"<<resource_failures<<",\"metadata_cases\":"<<metadata_cases
             <<",\"timing_sample\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
