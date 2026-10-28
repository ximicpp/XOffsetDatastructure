// The granule validation index (one bit per 8-byte payload granule, live and
// free bitmaps) against an independent byte-interval oracle: error category
// and order, no state change on rejection, sparse gaps, recomputed holes,
// storage (inline, upstream block, retained reload words), real global new
// failures in typed load, and typed hostile wires through the single-check
// head path. No timer.
#include "validation_global_new_failure.hpp"
#include <xoffsetdatastructure.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace xo = XOffsetDatastructure;
namespace wd = xo::detail;

struct GranuleScalar { std::uint32_t a{}; std::int64_t b{}; std::uint16_t table[3]{}; };
struct GranuleRoot {
    GranuleScalar no_checks[16]{};
    bool flags[3]{};
    xo::XString name;
    xo::XVector<GranuleScalar> plain;
    xo::XVector<bool> booleans;
    xo::XMap<std::uint32_t, bool> sorted;
    xo::XVector<std::uint32_t> words;
};
XOFFSET_RECORD_DERIVED(GranuleScalar, (), a, b, table)
XOFFSET_RECORD_DERIVED(GranuleRoot, (), no_checks, flags, name, plain, booleans, sorted, words)
XOFFSET_REGISTER_SCHEMA_NAME(GranuleRoot, "tests.validation.granules")
struct PairRoot {
    xo::XVector<std::uint8_t> left;
    xo::XVector<std::uint8_t> right;
};
XOFFSET_RECORD_DERIVED(PairRoot, (), left, right)
XOFFSET_REGISTER_SCHEMA_NAME(PairRoot, "tests.validation.granules.pair")
struct BigRoot {
    xo::XVector<std::uint64_t> values;
    xo::XString tail;
};
XOFFSET_RECORD_DERIVED(BigRoot, (), values, tail)
XOFFSET_REGISTER_SCHEMA_NAME(BigRoot, "tests.validation.granules.big")

namespace {
void need(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::size_t oracle_operations{}, priority_cases{}, derived_cases{}, storage_cases{},
    typed_rejections{}, failpoints{}, cursor_operations{};

struct Resource final : std::pmr::memory_resource {
    std::size_t allocations{}, deallocations{}, requested{}, live{};
    bool fail_next = false;
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (fail_next) { fail_next = false; throw std::bad_alloc(); }
        auto* p = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        ++allocations; requested += bytes; live += bytes;
        return p;
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
        ++deallocations; live -= bytes;
    }
    bool do_is_equal(const std::pmr::memory_resource& x) const noexcept override { return this == &x; }
    void balanced() const { need(live == 0 && allocations == deallocations, "scratch resource not balanced"); }
};

struct State {
    std::uint64_t owned{}, free{};
    std::vector<char> live, dead;
    bool operator==(const State&) const = default;
};
// Granule membership as the validator sees it: the cursor's live range
// [0, cursor) while the cursor path runs, the bitmaps after it.
bool live_at(const wd::WireValidationContext& c, std::size_t g) {
    if (c.cursor != wd::WireValidationContext::no_cursor && g * 8 < c.cursor) return true;
    return c.granules.prepared() && c.granules.member(false, g);
}
bool dead_at(const wd::WireValidationContext& c, std::size_t g) {
    return c.granules.prepared() && c.granules.member(true, g);
}
State save(const wd::WireValidationContext& c) {
    State s{c.owned_bytes, c.free_bytes, {}, {}};
    for (std::size_t g = 0; g < c.extent() / 8; ++g) {
        s.live.push_back(char(live_at(c, g)));
        s.dead.push_back(char(dead_at(c, g)));
    }
    return s;
}

using Ranges = std::vector<std::pair<std::size_t, std::size_t>>;
bool owned_is_prefix(const Ranges& owned, std::size_t prefix) {
    std::size_t total = 0;
    for (auto [l, h] : owned) { if (h > prefix) return false; total += h - l; }
    return total == prefix;
}
bool hits(const Ranges& ranges, std::size_t b, std::size_t e) {
    for (auto [l, h] : ranges) if (l < e && b < h) return true;
    return false;
}
void reg(bool free, const char* p, std::size_t bytes, wd::WireValidationContext& ctx) {
    if (free) wd::register_free_range(p, bytes, ctx, "granule test: bounds");
    else wd::register_owned_range(p, bytes, ctx, "granule test: bounds");
}
constexpr const char* live_live = "wire validation detected overlapping dynamic ranges";
constexpr const char* live_free = "wire validation detected live/free overlap";
constexpr const char* free_live = "wire validation detected free-list overlap with live ranges";
constexpr const char* free_free = "wire validation detected overlapping free-list ranges";

// Random registrations (aligned, unaligned, out of bounds, multiword) against
// unsorted byte intervals. Payload extents straddle the inline limit. With the
// cursor path on, the first registrations mostly continue the live prefix, so
// the cursor runs for a while and then hands over to the bitmaps mid-stream.
void pairwise_oracle() {
    for (bool cursor : {false, true})
    for (std::size_t extent : {std::size_t{48}, std::size_t{4100}, wd::WireGranuleIndex::inline_extent,
                               wd::WireGranuleIndex::inline_extent + 8, std::size_t{70001}}) {
        std::vector<std::uint64_t> storage(extent / 8 + 2);
        const char* base = reinterpret_cast<const char*>(storage.data());
        Resource upstream;
        {
            wd::WireValidationContext ctx{.base = base, .end = base + extent, .upstream = &upstream,
                                          .cursor = cursor ? 0 : wd::WireValidationContext::no_cursor};
            Ranges owned, free;
            std::size_t prefix = 0; // end of the contiguous live prefix while only it was registered
            std::uint64_t seed = 0x9e3779b97f4a7c15ULL ^ extent;
            for (std::size_t i = 0; i < 3000; ++i) {
                seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
                const bool extend = cursor && i < 400 && (seed >> 52) % 64 != 0;
                const bool f = !extend && (seed & 1);
                std::size_t b = std::size_t((seed >> 8) % (extent + 64));
                std::size_t n = std::size_t((seed >> 32) % 9 == 0 ? (seed >> 40) % 2048 : (seed >> 40) % 64);
                if ((seed >> 20) % 8) { b &= ~std::size_t(7); n &= ~std::size_t(7); }
                if (extend) b = prefix;
                const char* expected = nullptr;
                if (n) {
                    if (b > extent || n > extent - b) expected = "granule test: bounds";
                    else if ((b | n) & 7) expected = "granule test: bounds";
                    else if (hits(owned, b, b + n)) expected = f ? free_live : live_live;
                    else if (hits(free, b, b + n)) expected = f ? free_free : live_free;
                }
                const auto before = save(ctx);
                bool rejected = false;
                try { reg(f, base + b, n, ctx); }
                catch (const xo::XException& e) {
                    need(expected && std::string_view(e.what()) == expected, "oracle error category or order differs");
                    rejected = true;
                }
                need(rejected == (expected != nullptr), "oracle acceptance differs");
                if (rejected) need(save(ctx) == before, "rejection changed index state");
                else if (n) (f ? free : owned).emplace_back(b, b + n);
                if (!rejected && n && !f && b == prefix && free.empty()) prefix += n;
                if (cursor && ctx.cursor != wd::WireValidationContext::no_cursor)
                    need(ctx.cursor == prefix && free.empty() && owned_is_prefix(owned, prefix), "cursor is not the live prefix");
                if (cursor && ctx.cursor != wd::WireValidationContext::no_cursor) ++cursor_operations;
                std::uint64_t ob = 0, fb = 0;
                for (auto [l, h] : owned) ob += h - l;
                for (auto [l, h] : free) fb += h - l;
                need(ctx.owned_bytes == ob && ctx.free_bytes == fb, "coverage byte counts differ");
                if (i % 97 == 0 || (cursor && i < 450))
                    for (std::size_t g = 0; g < extent / 8; ++g) {
                        need(live_at(ctx, g) == hits(owned, g * 8, g * 8 + 8), "live bitmap differs");
                        need(dead_at(ctx, g) == hits(free, g * 8, g * 8 + 8), "free bitmap differs");
                    }
                ++oracle_operations;
            }
            need(upstream.allocations == (extent > wd::WireGranuleIndex::inline_extent + 7 ? 1u : 0u),
                 "bitmap storage is not inline up to 32 KiB and one upstream block above");
        }
        upstream.balanced();
    }
}

// A live hit anywhere in the range wins over an earlier free hit.
void global_priority() {
    for (bool incoming_free : {false, true}) {
        std::vector<std::uint64_t> storage(16384 / 8);
        const char* base = reinterpret_cast<const char*>(storage.data());
        wd::WireValidationContext ctx{.base = base, .end = base + 16384};
        reg(true, base + 8, 8, ctx);
        reg(false, base + 12288, 16, ctx);
        const auto before = save(ctx);
        bool caught = false;
        try { reg(incoming_free, base, 12296, ctx); }
        catch (const xo::XException& e) { caught = std::string_view(e.what()) == (incoming_free ? free_live : live_live); }
        need(caught && save(ctx) == before, "live priority lost or rejection mutated state");
        ++priority_cases;
    }
}

// register_allocation: the derived block (8-aligned, round8(request)) is the
// registered and bounds-checked range; bounds precede overlap.
void derived_extent_error_order() {
    // The 64-byte alignment cases below are relative to base: keep it 64-aligned.
    alignas(64) static const std::uint64_t storage[512] = {};
    const char* base = reinterpret_cast<const char*>(storage);
    constexpr const char* bounds = "wire validation: allocation out of bounds";
    auto one = [&](const char* ptr, std::size_t request, std::size_t alignment, bool incoming_free,
                   const char* expected, std::size_t fixture_offset = 0, std::size_t fixture_bytes = 0,
                   bool fixture_free = false) {
        wd::WireValidationContext ctx{.base = base, .end = base + 4096};
        if (fixture_bytes) {
            if (fixture_free) wd::register_free_range(base + fixture_offset, fixture_bytes, ctx, "fixture");
            else wd::register_owned_range(base + fixture_offset, fixture_bytes, ctx, "fixture");
        }
        const auto before = save(ctx);
        bool caught = false;
        try { wd::register_allocation(ptr, request, alignment, ctx, incoming_free); }
        catch (const xo::XException& e) { caught = true; need(std::string(e.what()) == expected, "derived-extent first error differs"); }
        need(caught == (expected[0] != '\0'), "derived-extent accept/reject differs");
        if (caught) {
            need(save(ctx) == before, "derived-extent rejection changed state");
        } else {
            const auto begin = static_cast<std::size_t>(ptr - base);
            const auto block = (request + 7u) & ~std::size_t(7u);
            for (std::size_t g = 0; g < 512; ++g) {
                const bool inside = g * 8 >= begin && g * 8 < begin + block;
                const bool was = incoming_free ? before.dead[g] : before.live[g];
                need((incoming_free ? dead_at(ctx, g) : live_at(ctx, g)) == (inside || was), "derived extent is not round8(request)");
            }
            need((incoming_free ? ctx.free_bytes - before.free : ctx.owned_bytes - before.owned) == block,
                 "coverage byte count is not the derived extent");
        }
        ++derived_cases;
    };
    for (bool incoming_free : {false, true}) {
        const char* live_hit = incoming_free ? free_live : live_live;
        const char* free_hit = incoming_free ? free_free : live_free;
        for (std::size_t request : {std::size_t{1}, std::size_t{7}, std::size_t{8}, std::size_t{13}, std::size_t{16}, std::size_t{4095 - 127}})
            one(base + 128, request, 8, incoming_free, "");
        one(base + 128, 16, 1, incoming_free, "");
        one(base + 128, 16, 64, incoming_free, "");
        one(base + 4088, 8, 8, incoming_free, "");
        for (std::size_t offset : {std::size_t{1}, std::size_t{4}, std::size_t{17}, std::size_t{124}, std::size_t{132}})
            for (std::size_t alignment : {std::size_t{1}, std::size_t{4}, std::size_t{8}})
                one(base + offset, 8, alignment, incoming_free, bounds);
        one(base + 136, 16, 64, incoming_free, bounds);
        one(base + 136, 16, 16, incoming_free, bounds);
        one(base + 4088, 9, 8, incoming_free, bounds);
        one(base + 4080, 17, 8, incoming_free, bounds);
        one(base + 4096, 1, 1, incoming_free, bounds);
        one(nullptr, 8, 8, incoming_free, bounds);
        one(base + 128, 17, 8, incoming_free, live_hit, 144, 8, false);
        one(base + 128, 17, 8, incoming_free, free_hit, 144, 8, true);
        one(base + 128, 16, 8, incoming_free, "", 144, 8, false);
        one(base + 128, 16, 8, incoming_free, "", 144, 8, true);
        one(base + 136, 8, 8, incoming_free, live_hit, 128, 16, false);
    }
}

// Recomputed holes against byte truth, with a payload whose
// extent is not a multiple of 8 (its trailing bytes are never free or live).
void recomputed_runs() {
    for (std::size_t extent : {std::size_t{4096}, std::size_t{4100}, std::size_t{40003}}) {
        std::vector<std::uint64_t> storage(extent / 8 + 2);
        const char* base = reinterpret_cast<const char*>(storage.data());
        std::uint64_t seed = 0xabcdef12345ULL ^ extent;
        for (int round = 0; round < 40; ++round) {
            wd::WireValidationContext ctx{.base = base, .end = base + extent};
            std::vector<char> live(extent, 0), dead(extent, 0);
            for (int i = 0; i < 60; ++i) {
                seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
                const bool f = seed & 1;
                const auto b = std::size_t((seed >> 8) % (extent / 8)) * 8;
                const auto n = std::min<std::size_t>(std::size_t((seed >> 32) % 64 + 1) * 8, extent / 8 * 8 - b);
                if (!n) continue;
                try { reg(f, base + b, n, ctx); } catch (const xo::XException&) { continue; }
                std::fill_n((f ? dead : live).begin() + std::ptrdiff_t(b), n, char(1));
            }
            Ranges runs, truth;
            wd::for_each_unowned_run(ctx, [&](std::size_t b, std::size_t e) { runs.emplace_back(b, e); });
            for (std::size_t x = 0; x < extent;) {
                if (live[x]) { ++x; continue; }
                const auto start = x;
                while (x < extent && !live[x]) ++x;
                truth.emplace_back(start, x);
            }
            need(runs == truth, "recomputed unowned runs differ from byte truth");
        }
    }
}

// Storage: inline up to 32 KiB, one upstream block above, retained words for
// a reload buffer, no default-resource use, and upstream failure commits nothing.
void storage() {
    struct DefaultGuard {
        std::pmr::memory_resource* old = std::pmr::set_default_resource(std::pmr::null_memory_resource());
        ~DefaultGuard() { std::pmr::set_default_resource(old); }
    } guard;
    static_assert(std::is_aggregate_v<wd::WireValidationContext>);
    static_assert(!std::is_copy_constructible_v<wd::WireValidationContext>);
    static_assert(!std::is_move_constructible_v<wd::WireValidationContext>);
    std::vector<std::uint64_t> storage(70000 / 8 + 1);
    const char* base = reinterpret_cast<const char*>(storage.data());
    {
        wd::WireValidationContext ctx{.base = base, .end = base + 256};
        need(ctx.upstream == std::pmr::new_delete_resource(), "context captured the default PMR resource");
        reg(false, base + 8, 8, ctx); reg(true, base + 32, 8, ctx);
        need(ctx.granules.upstream_words() == 0, "small payload took host words");
    }
    for (bool fail : {false, true}) {
        Resource upstream;
        {
            wd::WireValidationContext ctx{.base = base, .end = base + 70000, .upstream = &upstream};
            upstream.fail_next = fail;
            bool caught = false;
            try { reg(false, base + 64, 8, ctx); } catch (const std::bad_alloc&) { caught = true; }
            need(caught == fail, "upstream failure not propagated as std::bad_alloc");
            if (fail) {
                need(!ctx.granules.prepared() && ctx.owned_bytes == 0, "upstream failure committed partial state");
                reg(false, base + 64, 8, ctx);
            }
            const auto words = (70000 / 8 + 63) / 64;
            need(upstream.allocations == 1 && upstream.requested == 2 * words * 8 &&
                 ctx.granules.upstream_words() == 2 * words, "upstream block is not two bitmaps of the payload");
            need(ctx.granules.member(false, 8) && !ctx.granules.member(false, 9), "registration after failure");
        }
        upstream.balanced();
        ++storage_cases;
    }
    {
        wd::WireValidationScratch retained;
        Resource upstream;
        for (std::size_t extent : {std::size_t{40000}, std::size_t{70000}, std::size_t{50000}}) {
            const auto before = fail_new::live();
            wd::WireValidationContext ctx{.base = base, .end = base + extent, .upstream = &upstream, .retained = &retained};
            // Stale bits from an earlier, larger payload must not survive.
            reg(false, base + 48, extent / 8 * 8 - 48, ctx);
            need(ctx.owned_bytes == extent / 8 * 8 - 48, "retained words were not cleared");
            need(upstream.allocations == 0 && ctx.granules.upstream_words() == 0, "retained words used the upstream");
            if (extent == 50000) need(fail_new::live() == before, "retained words grew for a smaller payload");
        }
        need(retained.capacity() == 2 * ((70000 / 8 + 63) / 64), "retained words did not grow to the largest payload");
        ++storage_cases;
    }
}

template <class T> T read_at(const std::string& s, std::size_t p) { T x; std::memcpy(&x, s.data() + p, sizeof(x)); return x; }
template <class T> void write_at(std::string& s, std::size_t p, T x) { std::memcpy(s.data() + p, &x, sizeof(x)); }
template <class Root>
void reject(const std::string& wire, std::string_view expected) {
    const auto before = wire;
    bool caught = false;
    std::string what;
    try { auto b = xo::XBuffer::load_verified<Root>(wire); }
    catch (const xo::XException& e) { what = e.what(); caught = what == expected; }
    if (!caught) std::cerr << "expected '" << expected << "', got '" << what << "'\n";
    need(caught && wire == before, "typed rejection or its first error differs");
    auto reloader = xo::XBuffer::create<Root>(4096);
    caught = false;
    try { reloader.template reload_verified<Root>(wire); }
    catch (const xo::XException& e) { caught = std::string_view(e.what()) == expected; }
    need(caught, "typed reload rejection or its first error differs");
    ++typed_rejections;
}

// Hostile heads through the single-check path: each falls back to the full
// check sequence, whose first error is reported.
void typed_head_checks() {
    auto buffer = xo::XBuffer::create<GranuleRoot>(32768);
    auto& root = buffer.root<GranuleRoot>();
    root.name = "verified-and-out-of-line";
    root.plain.resize(8); root.booleans.resize(3); root.booleans[1] = true;
    root.sorted.try_emplace(1, true); root.sorted.try_emplace(2, false);
    for (std::uint32_t i = 0; i < 5; ++i) root.words.push_back(i);
    root.no_checks[15].a = 123;
    const auto original = buffer.save_verified<GranuleRoot>();
    const auto h = read_at<xo::XWireHeaderV2>(original, 0);
    need(h.extent_count == 0 && h.hole_count == 0, "fixture wire must be dense without holes");
    // Dense V2.1: payload offset p is file position payload_at + p.
    const std::int64_t payload_at = std::int64_t(sizeof(h)) - std::int64_t(xo::XWIRE_PAYLOAD_BEGIN);
    const std::size_t rp = std::size_t(payload_at + h.root_offset);
    const std::size_t used = h.used_bytes;
    (void)xo::XBuffer::load_verified<GranuleRoot>(original);
    auto field = [&](std::size_t member, std::size_t head_field) { return rp + member + head_field; };
    const auto words = offsetof(GranuleRoot, words);
    const auto name = offsetof(GranuleRoot, name);
    const auto data_of = [&](const std::string& w, std::size_t member) {
        const auto origin = field(member, offsetof(xo::XFixedVector<std::uint32_t>, data_delta));
        return std::size_t(std::int64_t(origin) + read_at<std::int32_t>(w, origin));
    };
    // Points a head's data_delta at payload offset `to` (file position payload_at + to).
    auto point = [&](std::string& w, std::size_t member, std::int64_t to) {
        const auto origin = field(member, offsetof(xo::XFixedVector<std::uint32_t>, data_delta));
        write_at(w, origin, std::int32_t(payload_at + to - std::int64_t(origin)));
    };
    {   // bool check precedes a later bad pointer.
        auto bad = original;
        write_at(bad, rp + offsetof(GranuleRoot, flags) + 2, std::uint8_t{2});
        write_at(bad, field(words, offsetof(xo::XFixedVector<std::uint32_t>, data_delta)), INT32_MAX);
        reject<GranuleRoot>(bad, "wire validation: invalid bool representation");
    }
    {   // Data address outside the payload, at its end, payload past the end,
        // only the rounded block past the end, element and block misalignment.
        auto bad = original; point(bad, words, -8);
        reject<GranuleRoot>(bad, "wire validation: relative pointer out of bounds");
        point(bad, words, std::int64_t(used));
        reject<GranuleRoot>(bad, "wire validation: relative pointer out of bounds");
        point(bad, words, std::int64_t(used) - 16); // 5 x 4 bytes = 20 > 16
        reject<GranuleRoot>(bad, "wire validation failed: XFixedVector payload out of bounds");
        auto odd = original;
        write_at(odd, field(words, offsetof(xo::XFixedVector<std::uint32_t>, capacity_)), std::uint32_t{5});
        point(odd, words, std::int64_t(used) - 20); // payload fits, round8 block does not, and 4-aligned only
        reject<GranuleRoot>(odd, "wire validation: allocation out of bounds");
        auto mis = original; point(mis, words, std::int64_t(data_of(original, words)) - payload_at + 1);
        reject<GranuleRoot>(mis, "wire validation failed: XFixedVector payload out of bounds");
        auto mis4 = original; point(mis4, words, std::int64_t(data_of(original, words)) - payload_at - 4);
        reject<GranuleRoot>(mis4, "wire validation: allocation out of bounds");
    }
    {   // String: data outside, payload past the end, misaligned block.
        auto bad = original; point(bad, name, std::int64_t(used) + 8);
        reject<GranuleRoot>(bad, "wire validation: relative pointer out of bounds");
        point(bad, name, std::int64_t(used) - 8); // capacity + 1 > 8
        reject<GranuleRoot>(bad, "wire validation failed: XFixedString payload out of bounds");
        auto mis = original;
        const auto origin = field(name, offsetof(xo::XFixedString, data_delta));
        write_at(mis, origin, read_at<std::int32_t>(original, origin) + 4);
        reject<GranuleRoot>(mis, "wire validation: allocation out of bounds");
    }
    {   // Live block onto another live block, and a map order error.
        auto bad = original; point(bad, words, std::int64_t(data_of(original, offsetof(GranuleRoot, name))) - payload_at);
        reject<GranuleRoot>(bad, live_live);
        using Pair = xo::XKeyValue<std::uint32_t, bool>;
        auto order = original;
        const auto data = data_of(original, offsetof(GranuleRoot, sorted));
        write_at(order, data + sizeof(Pair) + offsetof(Pair, first), std::uint32_t{1});
        reject<GranuleRoot>(order, "wire validation: map keys not strictly ordered");
    }
    {   // Live block onto a hole.
        auto retired = xo::XBuffer::create<PairRoot>(32768);
        auto& r = retired.root<PairRoot>();
        r.left.resize(16); r.right.resize(16);
        r.left.reserve(32);
        const auto valid = retired.save_verified<PairRoot>();
        const auto header = read_at<xo::XWireHeaderV2>(valid, 0);
        const auto at = sizeof(header) - xo::XWIRE_PAYLOAD_BEGIN; // + payload offset: file position
        const auto hole = read_at<xo::XBufferCore::WireHole>(valid, at + header.used_bytes);
        need(header.hole_count == 1 && hole.bytes == 16, "fixture must hold one retired block");
        auto bad = valid;
        const auto origin = at + header.root_offset + offsetof(PairRoot, right) +
                            offsetof(xo::XFixedVector<std::uint8_t>, data_delta);
        write_at(bad, origin, std::int32_t(std::int64_t(at + hole.offset) - std::int64_t(origin)));
        reject<PairRoot>(bad, live_free);
        (void)xo::XBuffer::load_verified<PairRoot>(valid);
    }
}

// Payloads above 32 KiB. Preorder: values, then tail, right after the root
// (the cursor path validates it). Holes: a retired block before the tail.
// Reversed: no holes, tail before values (the bitmaps validate it).
enum class Big { preorder, holes, reversed };
std::string big_wire(Big kind) {
    auto buffer = xo::XBuffer::create<BigRoot>(1u << 16,
        xo::XBuffer::maximum_capacity(1u << 22).with_growth(xo::XGrowPolicy::Automatic));
    auto& r = buffer.root<BigRoot>();
    if (kind == Big::holes) r.values.reserve(4);
    if (kind != Big::preorder) r.tail = std::string(100, 'z');
    r.values.reserve(6000); // with holes: the 4-element block retires before the tail
    for (std::uint64_t i = 0; i < 6000; ++i) r.values.push_back(i * 3);
    if (kind == Big::preorder) r.tail = std::string(100, 'z');
    return buffer.save_verified<BigRoot>();
}

// Every global new failpoint of typed load (above 32 KiB: the upstream block
// is one of them): std::bad_alloc or XBadAlloc, nothing leaked, wire intact,
// next load recovers. A reload loop over a hole-free large wire allocates
// nothing after its first reload.
void typed_new_failures() {
    for (Big kind : {Big::preorder, Big::holes, Big::reversed}) {
        const auto wire = big_wire(kind), copy = wire;
        need((read_at<xo::XWireHeaderV2>(wire, 0).hole_count != 0) == (kind == Big::holes), "big fixture hole table");
        const auto outside = fail_new::live();
        { fail_new::Arm arm(-1); auto loaded = xo::XBuffer::load_verified<BigRoot>(wire); }
        const auto points = fail_new::attempts;
        need(points > 0 && !fail_new::overflow, "typed load allocation inventory empty");
        for (std::size_t p = 0; p < points; ++p) {
            const auto before = fail_new::live();
            bool caught = false;
            try { fail_new::Arm arm{long(p)}; auto loaded = xo::XBuffer::load_verified<BigRoot>(wire); }
            catch (const std::bad_alloc&) { caught = true; }
            catch (const xo::XBadAlloc&) { caught = true; }
            need(caught && fail_new::hits == 1, "typed-load failpoint not reached exactly once");
            need(fail_new::live() == before && wire == copy, "failed typed load leaked or changed the wire");
            auto again = xo::XBuffer::load_verified<BigRoot>(wire);
            need(again.save_verified<BigRoot>() == copy, "recovery load differs");
            ++failpoints;
        }
        need(fail_new::live() == outside, "typed-load sweep retained allocations");
        if (kind == Big::preorder) {
            // The cursor path needs no bitmap words, even with none retained.
            auto reloader = xo::XBuffer::create<BigRoot>(4096, xo::XBuffer::maximum_capacity(1u << 22));
            reloader.reload_verified<BigRoot>(wire);
            reloader.release_reload_scratch();
            { fail_new::Arm arm(-1); reloader.reload_verified<BigRoot>(wire); }
            need(fail_new::attempts == 0, "cursor path reload of a large preorder wire allocated host memory");
            need(reloader.save_verified<BigRoot>() == copy, "cursor path reload differs");
        }
        if (kind == Big::reversed) {
            auto reloader = xo::XBuffer::create<BigRoot>(4096, xo::XBuffer::maximum_capacity(1u << 22));
            reloader.reload_verified<BigRoot>(wire);
            { fail_new::Arm arm(-1); reloader.reload_verified<BigRoot>(wire); reloader.reload_verified<BigRoot>(wire); }
            need(fail_new::attempts == 0, "reload of a large hole-free wire allocated host memory");
            reloader.release_reload_scratch();
            { fail_new::Arm arm(0); bool caught = false;
              try { reloader.reload_verified<BigRoot>(wire); } catch (const std::bad_alloc&) { caught = true; }
              need(caught, "released reload scratch was not reallocated"); }
            reloader.reload_verified<BigRoot>(wire);
            need(reloader.save_verified<BigRoot>() == copy, "reload after scratch failure differs");
        }
    }
}

void alignment_fallback() {
    alignas(64) std::array<char, 256> a{};
    wd::WireValidationContext ctx{.base = a.data(), .end = a.data() + a.size()};
    for (std::size_t alignment : {1, 3, 6, 8, 16, 64})
        for (std::size_t offset = 0; offset < 64; ++offset) {
            bool caught = false;
            try { wd::validate_storage_range(a.data() + offset, 8, alignment, ctx, "alignment oracle"); }
            catch (const xo::XException& e) { caught = std::string_view(e.what()) == "alignment oracle"; }
            need(caught == ((reinterpret_cast<std::uintptr_t>(a.data() + offset) % alignment) != 0), "generic alignment check changed");
        }
    wd::validate_storage_range(nullptr, 0, 3, ctx, "zero");
}
} // namespace

int main() {
    try {
        pairwise_oracle(); global_priority(); derived_extent_error_order(); recomputed_runs();
        storage(); typed_head_checks(); typed_new_failures(); alignment_fallback();
        std::cout << "{\"status\":\"passed\",\"oracle_operations\":" << oracle_operations
                  << ",\"cursor_operations\":" << cursor_operations
                  << ",\"priority_cases\":" << priority_cases << ",\"derived_extent_cases\":" << derived_cases
                  << ",\"storage_cases\":" << storage_cases
                  << ",\"typed_rejections\":" << typed_rejections << ",\"typed_new_failpoints\":" << failpoints
                  << ",\"performance_samples\":0}\n";
        return 0;
    } catch (const std::exception& e) {
        fail_new::watching = false; fail_new::fail_after = -1;
        std::cerr << "test_validation_granules failed: " << e.what() << '\n';
        return 1;
    }
}
