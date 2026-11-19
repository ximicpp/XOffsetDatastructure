// The allocator's reuse policy against an independent oracle (XBufferCore,
// "In-band allocator"):
//  - one LIFO stack per block size; the smallest size with a block serves a
//    request (split at its front), the remainder stays free;
//  - a block is never handed out larger than requested;
//  - otherwise the tail grows (alignment padding > 8 becomes a free block);
//  - retire never lowers used_end; live + free blocks tile [40, used_end);
//  - the free directory is a block of the payload (made in the first retired
//    block of at least 40 bytes, at its end, else at the end of the payload;
//    when full, compacted or moved to a block twice its size);
//  - the free state follows the storage on move/swap and reloads (identical
//    subsequent allocation decisions).
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
constexpr std::uint32_t kHeader = 40; // the image header; the first block (the root, when there is one) follows

void need(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::uint32_t block_of(std::uint32_t n) { return n == 0 ? 8u : (n + 7u) & ~7u; }

struct Oracle {
    // Entries in use, ascending size; a stack's head is its back. An emptied
    // stack keeps its entry.
    struct Entry { std::uint32_t size; std::vector<std::uint32_t> stack; };
    std::vector<Entry> dir;
    std::uint32_t dir_at = 0, cap = 0;
    std::map<std::uint32_t, std::uint32_t> live; // offset -> block bytes
    std::uint64_t used_end = kHeader, dir_bumped = 0;
    std::size_t reused = 0, bumped = 0, splits = 0;

    static std::uint32_t dir_bytes(std::uint32_t c) { return 8u + 8u * c; }
    std::size_t free_count() const {
        std::size_t n = 0;
        for (const auto& e : dir) n += e.stack.size();
        return n;
    }
    std::uint64_t free_bytes() const {
        std::uint64_t total = 0;
        for (const auto& e : dir) total += std::uint64_t(e.size) * e.stack.size();
        return total;
    }
    std::size_t find(std::uint32_t bytes) const { // the first entry of at least `bytes`
        std::size_t i = 0;
        while (i < dir.size() && dir[i].size < bytes) ++i;
        return i;
    }
    std::uint32_t bump(std::uint32_t bytes) {
        const auto at = static_cast<std::uint32_t>(used_end);
        used_end += bytes;
        dir_bumped += bytes;
        return at;
    }
    void push(std::uint32_t offset, std::uint32_t bytes) {
        if (dir_at != 0)
            if (const auto i = find(bytes); i < dir.size() && dir[i].size == bytes) return dir[i].stack.push_back(offset);
        if (dir_at == 0) {
            if (bytes >= dir_bytes(4)) { // the block's end holds the directory
                cap = std::min<std::uint32_t>((bytes - 8u) / 8u, 8u);
                bytes -= dir_bytes(cap);
                dir_at = offset + bytes;
            } else {
                cap = 8;
                dir_at = bump(dir_bytes(cap));
            }
            dir.clear();
            if (bytes == 0) return;
        }
        if (dir.size() + 1 == cap) {
            std::size_t nonempty = 0;
            for (const auto& e : dir) nonempty += !e.stack.empty();
            if (nonempty <= dir.size() / 2) {
                std::erase_if(dir, [](const Entry& e) { return e.stack.empty(); });
            } else { // twice the entries, in this block's end or at the end; the old block is freed
                const auto grown = cap * 2u, size = dir_bytes(grown);
                std::uint32_t at;
                if (bytes >= size) { bytes -= size; at = offset + bytes; }
                else at = bump(size);
                const auto old = dir_at, old_bytes = dir_bytes(cap);
                dir_at = at;
                cap = grown;
                push(old, old_bytes);
                if (bytes == 0) return;
            }
        }
        const auto i = find(bytes);
        if (i < dir.size() && dir[i].size == bytes) dir[i].stack.push_back(offset);
        else dir.insert(dir.begin() + static_cast<std::ptrdiff_t>(i), Entry{bytes, {offset}});
    }
    std::uint32_t allocate(std::uint32_t bytes, std::size_t alignment) {
        if (alignment <= 8 && dir_at != 0) {
            auto i = find(bytes);
            while (i < dir.size() && dir[i].stack.empty()) ++i;
            if (i < dir.size()) {
                const auto size = dir[i].size, offset = dir[i].stack.back();
                dir[i].stack.pop_back();
                if (size > bytes) { push(offset + bytes, size - bytes); ++splits; }
                live[offset] = bytes;
                ++reused;
                return offset;
            }
        }
        const auto a = std::max<std::uint64_t>(alignment, 8);
        const auto end = used_end, start = (end + a - 1) & ~(a - 1);
        used_end = start + bytes;
        if (start != end) push(static_cast<std::uint32_t>(end), static_cast<std::uint32_t>(start - end));
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
    // Emission order: the directory's sizes ascending, each stack from its head.
    std::vector<Hole> canonical() const {
        std::vector<Hole> out;
        for (const auto& e : dir)
            for (auto it = e.stack.rbegin(); it != e.stack.rend(); ++it) out.push_back({*it, e.size});
        return out;
    }
};
constexpr bool kInband = xo::XBufferCore::free_block_header_bytes != 0;

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
    std::uint32_t directory_at() const {
        return oracle.dir_at;
    }
    // In-band: makes the free directory first (all of a 40-byte block), so
    // that the next retired blocks stay whole free blocks.
    void make_directory() {
        if constexpr (kInband) {
            retire(alloc(40), 40);
            need(core.free_directory().bytes == 40 && core.free_block_count() == 0, "free directory fixture differs");
        }
    }
    std::uint32_t offset(const void* p) const {
        return static_cast<std::uint32_t>(static_cast<const char*>(p) - core.data());
    }
    void agree(const char* what) const {
        need(core.free_block_count() == oracle.free_count() && core.free_block_bytes() == oracle.free_bytes(), what);
        need(core.free_directory().offset == directory_at(), what);
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
    // Live blocks plus free blocks tile [40, used_end) exactly, and the table
    // is emitted in canonical order.
    void check_tiling() const {
        const auto holes = holes_of(core);
        need(same_holes(holes, oracle.canonical()), "free table emission order differs from canonical order");
        std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
        for (auto [o, b] : oracle.live) ranges.emplace_back(o, o + b);
        for (auto h : holes) ranges.emplace_back(h.offset, std::uint64_t(h.offset) + h.bytes);
        if (const auto d = core.free_directory(); d.bytes != 0) ranges.emplace_back(d.offset, std::uint64_t(d.offset) + d.bytes);
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
              << ",\"free_blocks\":" << o.free_count() << ",\"used_end\":" << o.used_end << "}\n";
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
    const auto dir_bumped = a.oracle.dir_bumped;
    for (std::size_t i = 0; i < 4096; ++i) { (void)a.alloc(32); a.retire(values[i], 16); }
    // Every 32-byte request fits a retired block: no tail growth in stage 5
    // (in-band, but for a free directory placed at the end).
    need(a.core.arena()->used_end == tail + (a.oracle.dir_bumped - dir_bumped),
         "fixed M stage-5 replay grew the tail despite free blocks");
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

// D3: allocate_block and retire_block decide the exact-bin hit, the unpadded
// bump and the retire into a bin with spare capacity inline; every other
// request takes the general path. A long seeded sequence that switches
// between both (repeated small sizes, bins that fill and grow, splits, large
// blocks, alignment above 8) must match the policy oracle after every
// operation, and every retired block (inline or general) reads as zero.
void inline_and_general_paths_interleaved() {
    Arena a; std::mt19937_64 rng(0x44335f696e6c696eull);
    struct Live { void* p; std::uint32_t n; }; std::vector<Live> live;
    const std::uint32_t sizes[] = {8, 16, 16, 24, 32, 32, 48, 64, 64, 72, 96, 128, 4096, 4104, 6000};
    std::size_t aligned = 0;
    for (unsigned i = 0; i < 20000; ++i) {
        const auto pick = rng() % 8;
        if (!live.empty() && (pick < 3 || live.size() > 600)) {
            const auto at = rng() % live.size(); const auto b = live[at];
            a.retire(b.p, b.n);
            // Past its header (the allocator's), but for a free directory placed in it.
            const auto* bytes = static_cast<const unsigned char*>(b.p);
            const auto d = a.core.free_directory();
            const auto* d_at = reinterpret_cast<const unsigned char*>(a.core.data()) + d.offset;
            for (std::uint32_t k = xo::XBufferCore::free_block_header_bytes; k < block_of(b.n); ++k)
                need(bytes[k] == 0 || (bytes + k >= d_at && bytes + k < d_at + d.bytes), "retired block was not zeroed");
            live[at] = live.back(); live.pop_back();
        } else {
            const auto n = sizes[rng() % 15];
            const std::size_t alignment = rng() % 16 == 0 ? (++aligned, std::size_t{16} << (rng() % 3))
                                                          : std::size_t{1} << (rng() % 4);
            auto* p = a.alloc(n, alignment);
            std::memset(p, 0xa5, block_of(n));
            live.push_back({p, n});
        }
        if (i % 101 == 0) a.check_tiling();
    }
    for (const auto& b : live) a.retire(b.p, b.n);
    a.check_tiling();
    need(a.oracle.reused > 5000 && a.oracle.splits > 0 && a.oracle.bumped > 500 && aligned > 0,
         "interleaved workload did not exercise both paths");
    emit("inline-and-general-paths-interleaved", 20000, a.oracle);
}

// Replaces the V1 "middle removal with unchanged head" cursor case: a larger
// free block is split, never handed out whole, and the remainder is reused.
void split_and_remainder_reuse() {
    Arena a;
    a.make_directory();
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
    // (In-band, a full free directory moves to the end of the remainder.)
    const auto d = a.core.free_directory();
    const std::uint32_t rest = 4000 - (d.offset + d.bytes == a.offset(l9000) + 9000 ? d.bytes : 0);
    need(a.alloc(rest) == static_cast<char*>(l9000) + 5000, "large split remainder (small bin) was not reused");
    a.check_tiling();
}

void tail_alignment_and_expansion() {
    Arena a;
    a.make_directory();
    if constexpr (kInband) (void)a.alloc(24); // the offsets below stay as without it, modulo 64
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
    auto* tail = a.alloc(64); // larger than any alignment padding (below 64): a tail block
    need(a.offset(tail) + 64u == a.core.arena()->used_end, "fixture block is not the tail");
    need(fd::try_expand_dynamic_bytes(a.ref(), tail, 64, 112, 1), "tail block did not expand in place");
    a.oracle.live[a.offset(tail)] = 112; a.oracle.used_end += 48;
    a.agree("in-place expansion accounting differs");
    need(!fd::try_expand_dynamic_bytes(a.ref(), aligned, 16, 64, 1), "non-tail block expanded");
    need(fd::try_expand_dynamic_bytes(a.ref(), tail, 112, 64, 1), "shrinking request on the tail must succeed");
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

    // Adopting the buffer's own free regions (the free directory first) on a
    // copy of its bytes reproduces the same image and every later decision.
    const auto holes = holes_of(destination);
    std::vector<Hole> regions;
    destination.for_each_free_region([&](Hole h) { regions.push_back(h); });
    xo::XBufferCore reloaded(wire.data(), wire.size(), cap);
    reloaded.replace_free_blocks(regions);
    need(reloaded.free_table_complete() && same_holes(holes_of(reloaded), holes), "reloaded table not canonical");
    need(std::memcmp(reloaded.data(), destination.data(), destination.arena()->used_end) == 0,
         "write-read-write image bytes differ");
    for (std::uint32_t n : {24u, 32u, 16u, 32u, 24u, 8u, 64u}) {
        auto* x = destination.allocate_block(n, 8);
        auto* y = reloaded.allocate_block(n, 8);
        need(static_cast<char*>(x) - destination.data() == static_cast<char*>(y) - reloaded.data(),
             "reloaded free table makes a different allocation decision");
    }
    need(sizeof(xo::XBufferCore::arena_header) == 40 && sizeof(Hole) == 8, "image header layout changed");
}

// prefetch_for_update() is a hint: no payload byte, hole, counter or later
// allocation decision may change, with or without a free table, and on a
// buffer whose storage has moved away.
void prefetch_hint_changes_nothing() {
    xo::XBufferCore empty;
    empty.prefetch_for_update();
    Arena a;
    a.core.prefetch_for_update();          // no free table yet
    std::vector<std::pair<void*, std::uint32_t>> live;
    for (std::uint32_t n : {8u, 24u, 40u, 24u, 256u, 248u, 512u, 4096u, 5000u, 16u})
        live.emplace_back(a.alloc(n), n);
    for (std::size_t i = 0; i < live.size(); i += 2) a.retire(live[i].first, live[i].second);
    const auto bytes = a.core.copy_bytes();
    const auto holes = holes_of(a.core);
    const std::size_t count = a.core.free_block_count();
    const std::uint64_t freed = a.core.free_block_bytes();
    for (int k = 0; k < 3; ++k) a.core.prefetch_for_update();
    need(a.core.copy_bytes() == bytes && same_holes(holes_of(a.core), holes) &&
             a.core.free_block_count() == count && a.core.free_block_bytes() == freed,
         "prefetch_for_update changed the buffer or its free table");
    a.agree("prefetch_for_update changed the free table");
    for (std::uint32_t n : {24u, 8u, 512u, 64u, 4096u}) {   // decisions still follow the oracle
        a.core.prefetch_for_update();
        a.alloc(n);
    }
    a.check_tiling();
    xo::XBufferCore moved(std::move(a.core));
    a.core.prefetch_for_update();
    moved.prefetch_for_update();
    need(moved.free_block_count() == a.oracle.free_count(), "prefetch_for_update after move changed the table");
}

void existing_owner_and_invalid_requests() {
    Arena a; auto* p = a.alloc(32); a.retire(p, 32);
    xo::detail::fail_next_registry_registration();
    try {
        // V2 allocation/retirement never touch the registry (host prefix page).
        need(a.alloc(32) == p, "free-table hit while registration failure is armed");
        a.retire(p, 32);
        // A record is made on an owner's first identity request.
        bool bad = false; try { xo::XBufferCore b(1024); (void)b.owner_identity(); } catch (const xo::XBadAlloc&) { bad = true; }
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
        actual_M_prefix(); query_changes_and_randomized_oracle(); inline_and_general_paths_interleaved();
        split_and_remainder_reuse();
        tail_alignment_and_expansion(); lifecycle_move_swap_reload(); prefetch_hint_changes_nothing();
        existing_owner_and_invalid_requests();
        std::cout << "{\"status\":\"passed\",\"performance_measurement\":false}\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
