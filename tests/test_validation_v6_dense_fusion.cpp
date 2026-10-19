// Actual register paths versus an independent interval/byte oracle. No timer.
#include "xoffsetdatastructure.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <memory_resource>
#include <stdexcept>
#include <string>
#include <vector>

namespace xo = XOffsetDatastructure;
namespace wd = xo::detail;
using Index = wd::WireValidationByteIndex;
namespace {
void need(bool b, const char* what) { if (!b) throw std::runtime_error(what); }
struct Range { std::size_t begin, end; };
struct Image {
    std::size_t count, extent, dense, directory;
    std::pair<std::size_t, std::size_t> bounds;
    std::vector<std::uint64_t> words;
    bool operator==(const Image&) const = default;
};
Image image(const Index& x) {
    Image out{x.size(), x.extent(), x.dense_prefix_bytes(), x.directory_entries(), x.coverage_bounds(), {}};
    for (std::size_t w=0; w<x.extent()/64+(x.extent()%64!=0); ++w) out.words.push_back(x.word_at(w));
    return out;
}
bool hits(const std::vector<Range>& ranges, std::size_t b, std::size_t e) {
    for (const auto& r:ranges) if (b<r.end && r.begin<e) return true;
    return false;
}
void truth(const Index& index, const std::vector<Range>& ranges) {
    std::vector<unsigned char> bytes(index.extent());
    for (const auto& r:ranges) for (auto p=r.begin; p<r.end; ++p) bytes[p]=1;
    for (std::size_t w=0; w<index.extent()/64+(index.extent()%64!=0); ++w) {
        std::uint64_t expected{};
        for (unsigned bit=0; bit<64 && w*64+bit<bytes.size(); ++bit)
            if (bytes[w*64+bit]) expected |= std::uint64_t{1}<<bit;
        need(index.word_at(w)==expected,"exact byte occupancy differs from oracle");
    }
    need(index.size()==ranges.size(),"registration count differs from oracle");
}
std::size_t operations{}, successful_prepared{}, actual_upstream_failures{};

void real_registration_oracle() {
    constexpr std::size_t extent=16385;
    // All four original representation combinations must remain correct; only
    // prepared dense targets can take the new path.
    for (unsigned mode=0; mode<4; ++mode) {
        alignas(64) std::array<char, extent> arena{};
        wd::WireValidationContext ctx{.base=arena.data(), .end=arena.data()+arena.size(),
            .owned_ranges=Index{std::pmr::new_delete_resource(), bool(mode&1)},
            .free_ranges=Index{std::pmr::new_delete_resource(), bool(mode&2)}};
        std::vector<Range> owned, free;
        auto step = [&](bool is_free, std::size_t begin, std::size_t count) {
            need(begin<=extent,"bad test address");
            const auto before_o=image(ctx.owned_ranges), before_f=image(ctx.free_ranges);
            std::string expected;
            if (count && count>extent-begin) expected="bounds-first";
            else if (count && hits(owned,begin,begin+count)) expected=is_free?
                "wire validation detected free-list overlap with live ranges":
                "wire validation detected overlapping dynamic ranges";
            else if (count && hits(free,begin,begin+count)) expected=is_free?
                "wire validation detected overlapping free-list ranges":
                "wire validation detected live/free overlap";
            bool caught=false;
            try {
                if (is_free) wd::register_free_range(arena.data()+begin,count,ctx,"bounds-first");
                else wd::register_owned_range(arena.data()+begin,count,ctx,"bounds-first");
            } catch (const xo::XException& e) { caught=true;need(e.what()==expected,"first-error category differs"); }
            need(caught==!expected.empty(),"accept/reject differs from independent pairwise oracle");
            if (caught) need(image(ctx.owned_ranges)==before_o && image(ctx.free_ranges)==before_f,
                             "rejection changed state");
            else if (count) (is_free?free:owned).push_back({begin,begin+count});
            truth(ctx.owned_ranges,owned);truth(ctx.free_ranges,free);++operations;
        };
        // Prime both indexes; neighboring disjoint bytes share boundary words.
        step(false,8192,1);step(true,0,1);
        step(false,1,1);step(true,2,1);step(false,63,1);step(true,64,1);step(false,65,1);
        step(false,0,8193);step(true,0,8193); // early free cannot hide later owned
        step(false,4095,1);step(true,4096,1);step(false,4097,1);
        step(false,1000,64);step(false,1000,64);step(true,1001,2);
        step(false,extent,1);step(true,extent-1,2); // backing bounds precede overlap
        step(false,extent,0);step(true,0,0);
        // Fill a word using disjoint masks, inducing partial -> full, then
        // query that full word through small and whole-word registrations.
        for (std::size_t b=128;b<192;++b) step(false,b,1);
        step(true,128,64);step(false,159,1);
        // Whole words and full pages retain their exact summary behavior.
        step(true,12288,4096);step(false,12300,1);
        std::uint64_t seed=0x66332299aabbccddULL;
        constexpr std::array<std::size_t,11> sizes{0,1,2,7,8,31,63,64,65,4095,4097};
        for (unsigned i=0;i<512;++i) {
            seed=seed*6364136223846793005ULL+1442695040888963407ULL;
            step(bool(seed&1),std::size_t((seed>>17)%(extent+1)),sizes[(seed>>45)%sizes.size()]);
        }
        const auto before=image(ctx.owned_ranges);
        wd::register_owned_range(nullptr,999,ctx,"null remains ignored");
        need(image(ctx.owned_ranges)==before,"null registration changed behavior");
    }
}

struct Upstream final : std::pmr::memory_resource {
    std::size_t attempts{}, allocations{}, frees{}, live{};
    std::size_t fail_at=(std::numeric_limits<std::size_t>::max)();
    void* do_allocate(std::size_t bytes,std::size_t alignment) override {
        if (attempts++==fail_at) throw std::bad_alloc();
        auto* p=std::pmr::new_delete_resource()->allocate(bytes,alignment);
        ++allocations;live+=bytes;return p;
    }
    void do_deallocate(void* p,std::size_t bytes,std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(p,bytes,alignment);++frees;live-=bytes;
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this==&other; }
};

void prepared_path_and_eligibility() {
    Upstream resource;
    {
        Index owned(&resource),free(&resource);
        owned.insert_disjoint(100,101,8192);free.insert_disjoint(5000,5001,8192);
        resource.fail_at=resource.attempts;
        auto invoke=[&](bool incoming_free,std::size_t begin,std::size_t end,unsigned expected) {
            const auto before_o=image(owned),before_f=image(free);
            const auto requests=resource.attempts;
            unsigned overlap=99;
            need(Index::try_insert_dense_single_page(owned,free,incoming_free,begin,end,8192,overlap),
                 "prepared single-page dense range was not handled");
            need(overlap==expected && resource.attempts==requests,"handled dense path allocated or misclassified");
            if (expected) need(image(owned)==before_o && image(free)==before_f,"dense reject modified state");
            else ++successful_prepared;
        };
        invoke(false,120,121,0); // same boundary word as byte100, bbox disjoint
        need(owned.word_at(1)==((std::uint64_t{1}<<36)|(std::uint64_t{1}<<56)),"bbox fast path lost neighboring bits");
        invoke(true,121,122,0);
        invoke(false,119,123,1); // owned priority over an earlier/later free byte
        invoke(true,119,123,1);
        invoke(false,121,122,2);
        invoke(true,121,122,2);
        unsigned unchanged=77;
        const auto before_o=image(owned),before_f=image(free);
        for (const auto& range:std::array<Range,4>{{{1,1},{4095,4097},{8192,8193},{1,(std::numeric_limits<std::size_t>::max)()}}}) {
            need(!Index::try_insert_dense_single_page(owned,free,false,range.begin,range.end,8192,unchanged),
                 "invalid/cross-page range incorrectly handled");
            need(unchanged==77,"unhandled path altered result output");
        }
        need(!Index::try_insert_dense_single_page(owned,free,false,130,131,12288,unchanged),
             "extent-growth incorrectly bypassed general preparation");
        need(image(owned)==before_o && image(free)==before_f,"unhandled path changed state");
    }
    need(resource.live==0 && resource.allocations==resource.frees,"prepared path resource leak");
}

void real_fallback_failure_guarantees() {
    // Test genuine upstream failures, not a guessed map-node size. The real
    // context registers must retain their existing cold/growth/sparse behavior.
    for (unsigned scenario=0;scenario<4;++scenario) {
        const unsigned points=scenario==3?3:1;
        for (unsigned point=0;point<points;++point) {
            Upstream resource;
            {
                alignas(64) std::array<char,16384> arena{};
                wd::WireValidationContext ctx{.base=arena.data(), .end=arena.data()+4096,
                    .owned_ranges=Index{&resource,scenario!=3},
                    .free_ranges=Index{&resource,scenario!=3}};
                if (scenario==1 || scenario==2)
                    wd::register_owned_range(arena.data()+100,1,ctx,"prime");
                if (scenario==2) ctx.end=arena.data()+8192;
                if (scenario==3) ctx.end=arena.data()+arena.size();
                const auto before_o=image(ctx.owned_ranges), before_f=image(ctx.free_ranges);
                const auto live=resource.live;
                resource.fail_at=resource.attempts+point;
                bool caught=false;
                try {
                    if (scenario==1) wd::register_free_range(arena.data()+200,1,ctx,"free cold");
                    else if (scenario==3) wd::register_owned_range(arena.data()+4095,4098,ctx,"two partial pages");
                    else wd::register_owned_range(arena.data()+120,1,ctx,"cold or extent growth");
                } catch (const std::bad_alloc&) { caught=true; }
                need(caught,"actual upstream allocation was not failed");
                need(image(ctx.owned_ranges)==before_o && image(ctx.free_ranges)==before_f && resource.live==live,
                     "failed fallback published state or leaked scratch");
                ++actual_upstream_failures;
            }
            need(resource.live==0 && resource.allocations==resource.frees,"fallback failure resource leak");
        }
    }
}
}

int main() { try {
    real_registration_oracle();prepared_path_and_eligibility();real_fallback_failure_guarantees();
    std::cout<<"{\"groups\":3,\"independent_operations\":"<<operations
             <<",\"successful_prepared\":"<<successful_prepared
             <<",\"actual_upstream_failures\":"<<actual_upstream_failures
             <<",\"timing_sample\":false}\n";
    return 0;
} catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; } }
