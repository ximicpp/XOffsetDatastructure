// V2 replacement for the retired V1 FreelistIndex test. V1's in-arena
// segregated free lists, skip cursors, host runtime cache, XAllocatorStateV1
// and release_cached_blocks no longer exist (and their diagnostics are gone),
// so the V1 visit-count gates have no equivalent. V2 keeps an exact-size binned
// free table on the host (XBufferCore). This test checks the documented V2
// reuse policy against an independent oracle, on the same workloads:
//  - exact-size bins up to 4096 B reuse LIFO (O(1), no scan);
//  - otherwise the smallest larger small bin, then the smallest large block
//    (earliest inserted among equal sizes) is split; the remainder stays free;
//  - a block is never handed out larger than requested;
//  - otherwise the tail grows (alignment padding > 8 becomes a free block);
//  - retire never lowers used_end; live + free blocks tile [48, used_end);
//  - the free table follows the storage on move/swap and reloads in
//    canonical order (identical subsequent allocation decisions).
#include "xoffsetdatastructure.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace xo = XOffsetDatastructure;
namespace fd = xo::fixed_detail;
namespace {
using Hole = xo::XBufferCore::WireHole;
constexpr std::uint32_t kSmallMax = 4096;
constexpr std::uint32_t kHeader = 48;

void need(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::uint32_t block_of(std::uint32_t n) { return n == 0 ? 8u : (n + 7u) & ~7u; }

struct Oracle {
    struct Free { std::uint32_t offset, bytes; std::uint64_t seq; };
    std::vector<Free> free;
    std::map<std::uint32_t, std::uint32_t> live; // offset -> block bytes
    std::uint64_t used_end = kHeader, seq = 0;
    std::size_t reused = 0, bumped = 0, splits = 0;

    void push(std::uint32_t offset, std::uint32_t bytes) { free.push_back({offset, bytes, seq++}); }
    std::uint64_t free_bytes() const {
        std::uint64_t total = 0;
        for (const auto& f : free) total += f.bytes;
        return total;
    }
    // Index of the free block the V2 policy selects, or npos.
    std::size_t select(std::uint32_t bytes) const {
        constexpr auto npos = static_cast<std::size_t>(-1);
        std::size_t best = npos;
        if (bytes <= kSmallMax) {
            for (std::size_t i = 0; i < free.size(); ++i)   // exact size, latest first
                if (free[i].bytes == bytes && (best == npos || free[i].seq > free[best].seq)) best = i;
            if (best != npos) return best;
            for (std::size_t i = 0; i < free.size(); ++i) { // smallest larger small size, latest first
                const auto b = free[i].bytes;
                if (b <= bytes || b > kSmallMax) continue;
                if (best == npos || b < free[best].bytes || (b == free[best].bytes && free[i].seq > free[best].seq))
                    best = i;
            }
            if (best != npos) return best;
        }
        for (std::size_t i = 0; i < free.size(); ++i) {     // large: smallest fit, earliest first
            const auto b = free[i].bytes;
            if (b <= kSmallMax || b < bytes) continue;
            if (best == npos || b < free[best].bytes || (b == free[best].bytes && free[i].seq < free[best].seq))
                best = i;
        }
        return best;
    }
    std::uint32_t allocate(std::uint32_t bytes, std::size_t alignment) {
        if (alignment <= 8) {
            const auto i = select(bytes);
            if (i != static_cast<std::size_t>(-1)) {
                const auto f = free[i];
                free.erase(free.begin() + static_cast<std::ptrdiff_t>(i));
                if (f.bytes > bytes) { push(f.offset + bytes, f.bytes - bytes); ++splits; }
                live[f.offset] = bytes;
                ++reused;
                return f.offset;
            }
        }
        const auto a = std::max<std::uint64_t>(alignment, 8);
        const auto start = (used_end + a - 1) & ~(a - 1);
        if (start != used_end) push(static_cast<std::uint32_t>(used_end), static_cast<std::uint32_t>(start - used_end));
        used_end = start + bytes;
        live[static_cast<std::uint32_t>(start)] = bytes;
        ++bumped;
        return static_cast<std::uint32_t>(start);
    }
    void retire(std::uint32_t offset, std::uint32_t bytes) {
        auto it = live.find(offset);
        need(it != live.end() && it->second == bytes, "oracle retire of a block that is not live");
        live.erase(it);
        push(offset, bytes);
    }
    // Canonical emission order: ascending size, insertion order within a size.
    std::vector<Hole> canonical() const {
        auto sorted = free;
        std::sort(sorted.begin(), sorted.end(), [](const Free& a, const Free& b) {
            return a.bytes != b.bytes ? a.bytes < b.bytes : a.seq < b.seq;
        });
        std::vector<Hole> out;
        for (const auto& f : sorted) out.push_back({f.offset, f.bytes});
        return out;
    }
};

std::vector<Hole> holes_of(const xo::XBufferCore& core) {
    std::vector<Hole> out;
    core.for_each_free_block([&](Hole h) { out.push_back(h); });
    return out;
}
bool same_holes(const std::vector<Hole>& a, const std::vector<Hole>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](Hole x, Hole y) {
        return x.offset == y.offset && x.bytes == y.bytes;
    });
}

struct Arena {
    xo::XBufferCore core;
    Oracle oracle;
    Arena() : core(16u * 1024u * 1024u, xo::XBufferCore::maximum_capacity(32u * 1024u * 1024u)) {}
    fd::allocator_ref ref() { return {core.arena()}; }
    std::uint32_t offset(const void* p) const {
        return static_cast<std::uint32_t>(static_cast<const char*>(p) - core.data());
    }
    void agree(const char* what) const {
        need(core.free_block_count() == oracle.free.size() && core.free_block_bytes() == oracle.free_bytes(), what);
        need(core.arena()->used_end == oracle.used_end && core.free_table_complete(), what);
    }
    void* alloc(std::uint32_t n, std::size_t alignment = 1) {
        const auto expected = oracle.allocate(block_of(n), alignment);
        auto* actual = fd::allocate_dynamic_bytes(ref(), n, alignment);
        need(offset(actual) == expected, "V2 free-table pointer differs from the policy oracle");
        need(reinterpret_cast<std::uintptr_t>(actual) % std::max<std::size_t>(alignment, 8) == 0,
             "allocation is not aligned to max(alignment, 8)");
        agree("free table / used_end after allocation differs");
        return actual;
    }
    void retire(void* p, std::uint32_t n) {
        oracle.retire(offset(p), block_of(n));
        fd::retire_dynamic_bytes(ref(), p, n);
        agree("free table / used_end after retirement differs");
    }
    // Live blocks plus free blocks tile [48, used_end) exactly, and the table
    // is emitted in canonical order.
    void check_tiling() const {
        const auto holes = holes_of(core);
        need(same_holes(holes, oracle.canonical()), "free table emission order differs from canonical order");
        std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
        for (auto [o, b] : oracle.live) ranges.emplace_back(o, o + b);
        for (auto h : holes) ranges.emplace_back(h.offset, std::uint64_t(h.offset) + h.bytes);
        std::sort(ranges.begin(), ranges.end());
        std::uint64_t at = kHeader;
        for (auto [b, e] : ranges) {
            need(b == at && e > b && b % 8 == 0 && e % 8 == 0, "live and free blocks do not tile the payload");
            at = e;
        }
        need(at == core.arena()->used_end, "live and free blocks do not reach used_end exactly");
    }
};

void emit(const char* name, std::size_t n, const Oracle& o) {
    std::cout << "{\"case\":\"" << name << "\",\"n\":" << n << ",\"reused\":" << o.reused
              << ",\"tail_allocations\":" << o.bumped << ",\"splits\":" << o.splits
              << ",\"free_blocks\":" << o.free.size() << ",\"used_end\":" << o.used_end << "}\n";
}

void mixed(std::size_t n, bool same_bucket_retire) {
    Arena a; std::vector<void*> small, fit, extra;
    for (std::size_t i = 0; i < n; ++i) { small.push_back(a.alloc(24)); fit.push_back(a.alloc(32)); }
    if (same_bucket_retire) for (std::size_t i = 0; i < n; ++i) extra.push_back(a.alloc(24));
    for (std::size_t i = 0; i < n; ++i) { a.retire(small[i], 24); a.retire(fit[i], 32); }
    const auto tail = a.core.arena()->used_end;
    for (std::size_t i = 0; i < n; ++i) {
        if (same_bucket_retire) a.retire(extra[i], 24);
        // Exact 32-byte bin, LIFO: independent of every 24-byte block.
        need(a.alloc(32) == fit[n - 1 - i], "exact-size bin is not LIFO");
    }
    need(a.core.arena()->used_end == tail, "exact-size reuse grew the tail");
    need(a.core.free_block_count() == n * (same_bucket_retire ? 2 : 1), "24-byte blocks were not all kept free");
    a.check_tiling();
    emit(same_bucket_retire ? "mixed-same-bucket-retire" : "mixed-static", n, a.oracle);
}

#include "workload_prefix.inc"
void actual_M_prefix() {
    Arena a; std::vector<void*> names, samples, payloads, values;
    for (auto len : workload_old_name_lengths) names.push_back(a.alloc(len + 1));
    for (std::size_t i = 0; i < 4096; ++i) samples.push_back(a.alloc(128));
    for (std::size_t i = 0; i < 4096; ++i) payloads.push_back(a.alloc(128));
    for (std::size_t i = 0; i < 4096; ++i) values.push_back(a.alloc(16));
    for (std::size_t i = 0; i < 4096; ++i) { (void)a.alloc(129); a.retire(names[i], workload_old_name_lengths[i] + 1); }
    for (std::size_t i = 0; i < 4096; ++i) { (void)a.alloc(256); a.retire(samples[i], 128); }
    for (std::size_t i = 0; i < 4096; ++i) { (void)a.alloc(384); a.retire(payloads[i], 128); }
    a.check_tiling();
    const auto tail = a.core.arena()->used_end;
    for (std::size_t i = 0; i < 4096; ++i) { (void)a.alloc(32); a.retire(values[i], 16); }
    // Every 32-byte request fits a retired block: no tail growth in stage 5.
    need(a.core.arena()->used_end == tail, "fixed M stage-5 replay grew the tail despite free blocks");
    a.check_tiling();
    emit("fixed-M-stages2-through5-size-replay", 4096, a.oracle);
}

void query_changes_and_randomized_oracle() {
    Arena a; std::mt19937_64 rng(0x4f32505245464958ull);
    struct Live { void* p; std::uint32_t n; }; std::vector<Live> live;
    const std::uint32_t sizes[] = {8, 16, 24, 32, 40, 48, 56, 64, 72, 128, 4096, 4104, 5000, 9000};
    const std::size_t alignments[] = {1, 2, 4, 8, 16, 32};
    for (unsigned i = 0; i < 3000; ++i) {
        if (!live.empty() && rng() % 3 == 0) {
            auto at = rng() % live.size(); auto b = live[at];
            a.retire(b.p, b.n); live.erase(live.begin() + static_cast<std::ptrdiff_t>(at));
        } else {
            auto n = sizes[rng() % 14]; auto alignment = alignments[rng() % 6];
            live.push_back({a.alloc(n, alignment), n});
        }
        if (i % 97 == 0) a.check_tiling();
    }
    a.check_tiling();
    need(a.oracle.reused > 0 && a.oracle.splits > 0, "randomized workload did not exercise reuse and splitting");
    emit("seeded-size-alignment-switching", 3000, a.oracle);
}

// Replaces the V1 "middle removal with unchanged head" cursor case: a larger
// free block is split, never handed out whole, and the remainder is reused.
void split_and_remainder_reuse() {
    Arena a;
    auto* p64 = a.alloc(64);
    (void)a.alloc(8); // barrier: p64 is not the tail
    a.retire(p64, 64);
    auto* q24 = a.alloc(24);
    need(q24 == p64, "smallest larger block was not split at its start");
    need(a.core.free_block_count() == 1 && a.core.free_block_bytes() == 40, "split remainder not kept free");
    auto* r40 = a.alloc(40);
    need(r40 == static_cast<char*>(p64) + 24, "split remainder was not reused exactly");
    // Smallest larger small bin wins over a larger one; equal sizes are LIFO.
    auto* b80 = a.alloc(80); (void)a.alloc(8);
    auto* b48 = a.alloc(48); (void)a.alloc(8);
    auto* c48 = a.alloc(48); (void)a.alloc(8);
    a.retire(b80, 80); a.retire(b48, 48); a.retire(c48, 48);
    need(a.alloc(40) == c48, "split did not choose the smallest larger (latest) bin");
    need(a.alloc(48) == b48, "exact bin did not win over the split remainder");
    // Large blocks: best fit, the remainder may fall into a small bin.
    auto* l9000 = a.alloc(9000); (void)a.alloc(8);
    auto* l12000 = a.alloc(12000); (void)a.alloc(8);
    a.retire(l12000, 12000); a.retire(l9000, 9000);
    need(a.alloc(5000) == l9000, "large reuse was not best fit");
    need(a.alloc(4000) == static_cast<char*>(l9000) + 5000, "large split remainder (small bin) was not reused");
    a.check_tiling();
}

void tail_alignment_and_expansion() {
    Arena a;
    auto* t = a.alloc(32);
    const auto end = a.core.arena()->used_end;
    a.retire(t, 32);
    need(a.core.arena()->used_end == end, "retire of the tail block lowered used_end");
    need(a.alloc(32) == t, "retired tail block was not reused");
    // Alignment above 8 never reuses and leaves its padding as a free block.
    (void)a.alloc(8);
    const auto before = a.core.free_block_count();
    auto* aligned = a.alloc(16, 64);
    need(reinterpret_cast<std::uintptr_t>(aligned) % 64 == 0, "64-byte alignment lost");
    need(a.core.free_block_count() == before + 1, "alignment padding is not an exact free block");
    a.check_tiling();
    // In-place expansion only for the tail block, never past it.
    auto* tail = a.alloc(48); // larger than the 40-byte padding: a tail block
    need(a.offset(tail) + 48u == a.core.arena()->used_end, "fixture block is not the tail");
    need(fd::try_expand_dynamic_bytes(a.ref(), tail, 48, 96, 1), "tail block did not expand in place");
    a.oracle.live[a.offset(tail)] = 96; a.oracle.used_end += 48;
    a.agree("in-place expansion accounting differs");
    need(!fd::try_expand_dynamic_bytes(a.ref(), aligned, 16, 64, 1), "non-tail block expanded");
    need(fd::try_expand_dynamic_bytes(a.ref(), tail, 96, 64, 1), "shrinking request on the tail must succeed");
    a.agree("no-op expansion changed accounting");
    a.check_tiling();
}

void lifecycle_move_swap_reload() {
    Arena a;
    auto* s0 = a.alloc(24); auto* s1 = a.alloc(24); auto* f0 = a.alloc(32); auto* f1 = a.alloc(32);
    a.retire(f1, 32); a.retire(f0, 32); a.retire(s1, 24); a.retire(s0, 24);
    const auto before = a.core.copy_bytes(); auto* old_arena = a.core.arena();
    xo::XBufferCore moved(std::move(a.core));
    need(moved.copy_bytes() == before && moved.arena() == old_arena, "Core move changed payload or storage");
    need(moved.free_block_count() == 4 && a.core.free_block_count() == 0, "free table did not move with the storage");
    // Allocation through the old arena pointer reaches the new owner (host prefix page).
    auto expected = a.oracle.allocate(32, 1);
    auto* actual = fd::allocate_dynamic_bytes({old_arena}, 32, 1);
    need(static_cast<char*>(actual) - moved.data() == expected && actual == f0, "moved owner lost its free table");
    Arena other; auto* other_first = other.alloc(24); other.retire(other_first, 24);
    moved.swap(other.core);
    need(other.core.arena() == old_arena && moved.free_block_count() == 1 && other.core.free_block_count() == 4 - 1,
         "swap did not exchange free tables with their storage");
    auto* more = fd::allocate_dynamic_bytes({old_arena}, 32, 1);
    need(more == f1, "swapped free table not used through its arena");
    fd::retire_dynamic_bytes({old_arena}, more, 32);
    auto* borrowed = fd::allocate_dynamic_bytes({moved.arena()}, 24, 1);
    need(borrowed == other_first, "other storage's free table lost after swap");
    fd::retire_dynamic_bytes({moved.arena()}, borrowed, 24);
    xo::XBufferCore destination; destination = std::move(other.core);
    fd::retire_dynamic_bytes({destination.arena()}, actual, 32);
    need(destination.free_block_count() == 4, "move assignment lost the free table");

    // Unverified reload: holes are unknown, the table is incomplete and empty,
    // and allocation grows the tail instead of guessing.
    const auto wire = destination.copy_bytes();
    const auto cap = xo::XBufferCore::maximum_capacity(32u * 1024u * 1024u);
    xo::XBufferCore unverified(wire.data(), wire.size(), cap);
    need(!unverified.free_table_complete() && unverified.free_block_count() == 0, "unverified load invented a free table");
    const auto tail = unverified.arena()->used_end;
    auto* grown = unverified.allocate_block(32, 8);
    need(static_cast<char*>(grown) - unverified.data() == static_cast<std::ptrdiff_t>(tail), "unverified reload reused unknown bytes");

    // Loading the canonical hole table reproduces every later decision.
    const auto holes = holes_of(destination);
    xo::XBufferCore reloaded(wire.data(), wire.size(), cap);
    reloaded.replace_free_blocks(holes);
    need(reloaded.free_table_complete() && same_holes(holes_of(reloaded), holes), "reloaded table not canonical");
    std::string t1(holes.size() * sizeof(Hole), '\0'), t2(holes.size() * sizeof(Hole), '\0');
    destination.write_free_blocks(t1.data()); reloaded.write_free_blocks(t2.data());
    need(t1 == t2, "write-read-write hole table bytes differ");
    for (std::uint32_t n : {24u, 32u, 16u, 32u, 24u, 8u, 64u}) {
        auto* x = destination.allocate_block(n, 8);
        auto* y = reloaded.allocate_block(n, 8);
        need(static_cast<char*>(x) - destination.data() == static_cast<char*>(y) - reloaded.data(),
             "reloaded free table makes a different allocation decision");
    }
    need(sizeof(xo::XBufferCore::arena_header) == 48 && sizeof(Hole) == 8, "V2 layout changed");
}

void existing_owner_and_invalid_requests() {
    Arena a; auto* p = a.alloc(32); a.retire(p, 32);
    xo::detail::fail_next_registry_registration();
    try {
        // V2 allocation/retirement never touch the registry (host prefix page).
        need(a.alloc(32) == p, "free-table hit while registration failure is armed");
        a.retire(p, 32);
        bool bad = false; try { xo::XBufferCore b(1024); } catch (const xo::XBadAlloc&) { bad = true; }
        need(bad, "existing-owner lookup consumed registry injection");
    } catch (...) { xo::detail::clear_registry_failures(); throw; }
    xo::detail::clear_registry_failures();
    const auto before = a.core.copy_bytes(); const auto holes = holes_of(a.core);
    auto unchanged = [&](const char* what) {
        need(a.core.copy_bytes() == before && same_holes(holes_of(a.core), holes), what);
    };
    for (auto alignment : {std::size_t{0}, std::size_t{3}}) {
        bool bad = false; try { (void)fd::allocate_dynamic_bytes(a.ref(), 32, alignment); } catch (const xo::XBadAlloc&) { bad = true; }
        need(bad, "invalid alignment accepted"); unchanged("invalid request changed arena or free table");
    }
    for (std::uint32_t bytes : {0u, 12u, 0x7ffffffcu}) {
        bool bad = false; try { (void)a.core.allocate_block(bytes, 8); } catch (const xo::XBadAlloc&) { bad = true; }
        need(bad, "allocate_block accepted a size that is not a nonzero multiple of 8");
        unchanged("invalid block size changed arena or free table");
    }
    {   // Explicit growth policy: a request past the extent fails without change.
        bool bad = false; try { (void)a.core.allocate_block(32u * 1024u * 1024u - 8u, 8); } catch (const xo::XBadAlloc&) { bad = true; }
        need(bad, "allocation beyond the explicit extent succeeded"); unchanged("failed growth changed state");
    }
}
} // namespace

int main() {
    try {
        for (auto n : {512u, 1024u, 2048u, 4096u}) { mixed(n, false); mixed(n, true); }
        actual_M_prefix(); query_changes_and_randomized_oracle(); split_and_remainder_reuse();
        tail_alignment_and_expansion(); lifecycle_move_swap_reload(); existing_owner_and_invalid_requests();
        std::cout << "{\"status\":\"passed\",\"performance_measurement\":false}\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
