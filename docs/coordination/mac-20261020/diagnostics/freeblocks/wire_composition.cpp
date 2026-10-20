// Diagnostic: byte composition of an XOffset V1 verified wire (W7 candidate).
// Count-only. Loads the wire with the adapter's load (load_verified, full
// structural validation), enumerates live allocations by walking the model
// graph with the public container fields, enumerates allocator free lists from
// XAllocatorStateV1, then linearly scans the AllocationHeader chain of the
// payload so that every payload byte is classified exactly once.
#include "adapter.hpp"
#include "adapters/xoffset/adapter.hpp"
#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <map>
#include <unordered_map>
#include <vector>

using namespace XOffsetDatastructure;
using sc::xo_detail::Attr;
using sc::xo_detail::Record;
using sc::xo_detail::Snapshot;

namespace {
enum Cls : std::uint8_t {
    C_ARENA_HDR, C_LIVE_AHDR, C_ALLOC_STATE, C_ROOT, C_LIVE_DATA, C_LIVE_SLACK,
    C_FREE_AHDR, C_FREE_FBH, C_FREE_BODY, C_FREE_SLACK, C_ORPHAN_AHDR, C_ORPHAN_BODY,
    C_GAP, C_TAIL, C_COUNT
};
const char* cls_name[C_COUNT] = {
    "arena_header(48B XBUFV1)", "live AllocationHeader", "allocator state(88B user)", "root Snapshot(user)",
    "live user data(registered extent)", "live internal slack(user_bytes>registered)",
    "free-list AllocationHeader", "free-list FreeBlockHeader(8B)", "free-list body(block_bytes-8)",
    "free-list slack(user_bytes>block_bytes)", "orphan(Core-free) AllocationHeader", "orphan(Core-free) body",
    "alignment gap/padding", "trailing slack before used_end"};

enum Kind : std::uint8_t { K_STATE, K_ROOT, K_RECORDS, K_NAME, K_SAMPLES, K_PAYLOAD, K_ATTRS, K_KEY, K_VALUE, K_N };
const char* kind_name[K_N] = {"alloc_state", "root", "records[]", "name", "samples", "payload", "attrs[]", "attr.key", "attr.value"};

std::uint64_t r8(std::uint64_t v) { return (v + 7u) & ~std::uint64_t(7u); }

struct Live { Kind kind; std::uint64_t reg; };
struct Free { std::uint32_t block_bytes; std::uint32_t cls; };

void analyze(const char* label, const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path); std::exit(2); }
    const std::size_t n = static_cast<std::size_t>(f.tellg());
    f.seekg(0);
    std::vector<std::uint64_t> storage((n + 7) / 8 + 1);
    auto* bytes = reinterpret_cast<unsigned char*>(storage.data());
    f.read(reinterpret_cast<char*>(bytes), static_cast<std::streamsize>(n));

    static_assert(sizeof(XWireHeaderV1) == 88);
    XWireHeaderV1 wh{};
    std::memcpy(&wh, bytes, sizeof(wh));
    const std::uint64_t P = wh.used_bytes;
    const unsigned char* pl = bytes + sizeof(XWireHeaderV1);
    XBufferCore::arena_header ah{};
    std::memcpy(&ah, pl, sizeof(ah));
    std::printf("==== %s: %s\n", label, path);
    std::printf("file_bytes=%zu  v1_header=%zu  payload(used_bytes)=%" PRIu64 "  arena.used_end=%" PRIu64
                "  arena.header_size=%u  root_off=%" PRIu64 "  alloc_off=%" PRIu64 "\n",
                n, sizeof(XWireHeaderV1), P, ah.used_end, ah.header_size, wh.root_offset, wh.allocator_offset);
    if (P + sizeof(XWireHeaderV1) != n) { std::printf("SIZE MISMATCH\n"); std::exit(3); }

    // Full library validation + typed access.
    auto state = sc::XOffsetAdapter::load(std::span<const std::uint8_t>(bytes, n));
    const auto base = reinterpret_cast<std::uintptr_t>(state->get_address());
    Snapshot& s = state->root();
    std::printf("load_verified: OK  records=%u (capacity %u)  segment_size=%zu\n",
                s.records.size(), s.records.capacity(), state->segment_size());

    std::unordered_map<std::uint64_t, Live> live;
    live.reserve(1 << 20);
    std::uint64_t dup = 0;
    auto add = [&](const void* p, std::uint64_t reg, Kind k) {
        const auto off = reinterpret_cast<std::uintptr_t>(p) - base;
        if (!live.emplace(off, Live{k, reg}).second) ++dup;
    };
    add(reinterpret_cast<const void*>(base + wh.allocator_offset), sizeof(XAllocatorStateV1), K_STATE);
    add(&s, sizeof(Snapshot), K_ROOT);
    if (s.records.capacity_) add(s.records.data(), r8(std::uint64_t(s.records.capacity_) * sizeof(Record)), K_RECORDS);
    for (std::uint32_t i = 0; i < s.records.size(); ++i) {
        const Record& r = s.records.data()[i];
        if (r.name.capacity_) add(r.name.data(), r8(std::uint64_t(r.name.capacity_) + 1), K_NAME);
        if (r.samples.capacity_) add(r.samples.data(), r8(std::uint64_t(r.samples.capacity_) * 4), K_SAMPLES);
        if (r.payload.capacity_) add(r.payload.data(), r8(std::uint64_t(r.payload.capacity_)), K_PAYLOAD);
        if (r.attrs.capacity_) add(r.attrs.data(), r8(std::uint64_t(r.attrs.capacity_) * sizeof(Attr)), K_ATTRS);
        for (std::uint32_t j = 0; j < r.attrs.size(); ++j) {
            const Attr& a = r.attrs.data()[j];
            if (a.key.capacity_) add(a.key.data(), r8(std::uint64_t(a.key.capacity_) + 1), K_KEY);
            if (a.value.capacity_) add(a.value.data(), r8(std::uint64_t(a.value.capacity_)), K_VALUE);
        }
    }

    // Allocator free lists (offsets are relative to the XAllocatorStateV1 object).
    XAllocatorStateV1 st{};
    std::memcpy(&st, pl + wh.allocator_offset, sizeof(st));
    std::unordered_map<std::uint64_t, Free> freel;
    std::uint64_t fl_sum = 0, fl_count_cls[16] = {}, fl_bytes_cls[16] = {};
    for (std::uint32_t c = 0; c < 16; ++c) {
        std::uint32_t off = st.freelist_head[c];
        while (off) {
            fixed_detail::FreeBlockHeader fb{};
            std::memcpy(&fb, pl + wh.allocator_offset + off, sizeof(fb));
            freel.emplace(wh.allocator_offset + off, Free{fb.block_bytes, c});
            fl_sum += fb.block_bytes;
            ++fl_count_cls[c];
            fl_bytes_cls[c] += fb.block_bytes;
            off = fb.next_offset;
        }
    }
    std::printf("allocator_state: arena_begin=%u arena_end=%u free_bytes=%u epoch=%u  freelist blocks=%zu sum(block_bytes)=%" PRIu64 "\n",
                st.arena_begin, st.arena_end, st.free_bytes, st.epoch, freel.size(), fl_sum);
    for (int c = 0; c < 16; ++c)
        if (fl_count_cls[c]) std::printf("  class %2d (<=%6u B): %8" PRIu64 " blocks %10" PRIu64 " B\n", c, 8u << c, fl_count_cls[c], fl_bytes_cls[c]);

    // Linear AllocationHeader chain scan + per-byte classification.
    std::vector<std::uint8_t> cls(P, C_GAP);
    std::uint64_t cnt[C_COUNT] = {};
    auto mark = [&](std::uint64_t b, std::uint64_t e, Cls c) {
        if (e <= b) return;
        std::memset(cls.data() + b, c, e - b);
        cnt[c] += e - b;
    };
    mark(0, sizeof(XBufferCore::arena_header), C_ARENA_HDR);
    std::uint64_t cur = sizeof(XBufferCore::arena_header);
    std::uint64_t nlive = 0, nfree = 0, norphan = 0, gap_events = 0;
    std::uint64_t kind_cnt[K_N] = {}, kind_reg[K_N] = {}, kind_slack[K_N] = {};
    std::uint64_t orphan_big = 0, orphan_big_bytes = 0;
    std::map<std::uint64_t, std::uint64_t> orphan_hist; // log2 bucket -> bytes
    std::uint64_t orphan_cnt_hist[40] = {};
    while (cur < P) {
        XBufferCore::AllocationHeader h{};
        bool ok = cur + 16 <= P;
        if (ok) {
            std::memcpy(&h, pl + cur, 16);
            ok = h.user_offset == cur + 16 && h.total_bytes == std::uint64_t(h.user_bytes) + 16 &&
                 h.alignment && (h.alignment & (h.alignment - 1)) == 0 && h.user_offset % h.alignment == 0 &&
                 std::uint64_t(h.user_offset) + h.user_bytes <= P;
        }
        if (!ok) { mark(cur, cur + 8 > P ? P : cur + 8, C_GAP); ++gap_events; cur += 8; continue; }
        const std::uint64_t u = h.user_offset, ue = u + h.user_bytes;
        if (auto it = live.find(u); it != live.end()) {
            ++nlive;
            mark(cur, u, C_LIVE_AHDR);
            const auto k = it->second.kind;
            const auto reg = std::min<std::uint64_t>(it->second.reg, h.user_bytes);
            Cls dc = k == K_STATE ? C_ALLOC_STATE : (k == K_ROOT ? C_ROOT : C_LIVE_DATA);
            mark(u, u + reg, dc);
            mark(u + reg, ue, C_LIVE_SLACK);
            ++kind_cnt[k]; kind_reg[k] += reg; kind_slack[k] += h.user_bytes - reg;
            if (it->second.reg > h.user_bytes) std::printf("WARN live reg > user_bytes at %" PRIu64 "\n", u);
        } else if (auto jt = freel.find(u); jt != freel.end()) {
            ++nfree;
            mark(cur, u, C_FREE_AHDR);
            const auto bb = std::min<std::uint64_t>(jt->second.block_bytes, h.user_bytes);
            mark(u, u + 8, C_FREE_FBH);
            mark(u + 8, u + bb, C_FREE_BODY);
            mark(u + bb, ue, C_FREE_SLACK);
        } else {
            ++norphan;
            mark(cur, u, C_ORPHAN_AHDR);
            mark(u, ue, C_ORPHAN_BODY);
            int b = 0; while ((1ull << (b + 1)) <= h.user_bytes) ++b;
            orphan_hist[b] += h.total_bytes; ++orphan_cnt_hist[b];
            if (h.user_bytes > fixed_detail::freelist_max_block_bytes) { ++orphan_big; orphan_big_bytes += h.total_bytes; }
        }
        cur = ue;
    }
    std::printf("scan: live blocks=%" PRIu64 " (graph-registered %zu, dup %" PRIu64 ")  freelist blocks=%" PRIu64
                " (listed %zu)  orphan blocks=%" PRIu64 "  gap 8B-steps=%" PRIu64 "\n",
                nlive, live.size(), dup, nfree, freel.size(), norphan, gap_events);
    if (nlive != live.size() || nfree != freel.size()) std::printf("WARN: scan/graph count mismatch\n");

    std::uint64_t tot = 0;
    std::printf("-- byte composition (payload) --\n");
    for (int c = 0; c < C_COUNT; ++c) { tot += cnt[c]; std::printf("  %-46s %12" PRIu64 "\n", cls_name[c], cnt[c]); }
    std::printf("  %-46s %12" PRIu64 "  (+88 header = %" PRIu64 ", file=%zu)\n", "SUM payload", tot, tot + 88, n);
    std::printf("-- live by kind: blocks / registered bytes / internal slack --\n");
    for (int k = 0; k < K_N; ++k)
        std::printf("  %-12s %9" PRIu64 " %12" PRIu64 " %10" PRIu64 "\n", kind_name[k], kind_cnt[k], kind_reg[k], kind_slack[k]);
    std::printf("-- orphan (unreferenced, not in free list; Core-level free_blocks_ are host-only) --\n");
    std::printf("  orphans with user_bytes > %u (freelist max class): %" PRIu64 " blocks, %" PRIu64 " B\n",
                fixed_detail::freelist_max_block_bytes, orphan_big, orphan_big_bytes);
    for (auto& [b, by] : orphan_hist)
        std::printf("  user_bytes in [2^%" PRIu64 ",2^%" PRIu64 "): %6" PRIu64 " blocks %10" PRIu64 " B\n", b, b + 1, orphan_cnt_hist[b], by);

    // 16 KiB pages whose every byte is "dead" under three definitions.
    auto pages = [&](const char* name, std::initializer_list<Cls> allowed_l, std::uint64_t shift) {
        bool allowed[C_COUNT] = {};
        for (auto c : allowed_l) allowed[c] = true;
        const std::uint64_t PG = 16384;
        std::uint64_t full = 0, total_full = 0;
        // shift=0: payload-relative pages; shift=88: wire-file-relative pages.
        for (std::uint64_t fs = 0; fs + PG <= P + shift; fs += PG) {
            if (fs < shift) continue; // page overlapping V1 header
            const std::uint64_t b = fs - shift, e = b + PG;
            ++total_full;
            bool dead = true;
            for (std::uint64_t i = b; i < e; ++i) if (!allowed[cls[i]]) { dead = false; break; }
            if (dead) ++full;
        }
        std::printf("  %-58s %6" PRIu64 " / %6" PRIu64 " pages = %10" PRIu64 " B\n", name, full, total_full, full * PG);
    };
    std::printf("-- 16 KiB pages entirely dead (payload-relative, payload base page-aligned) --\n");
    pages("A: free-list body only (keep AHDR+FreeBlockHeader)", {C_FREE_BODY, C_FREE_SLACK}, 0);
    pages("B: A + orphan blocks (hdr+body) + gaps", {C_FREE_BODY, C_FREE_SLACK, C_ORPHAN_AHDR, C_ORPHAN_BODY, C_GAP}, 0);
    pages("C: B + live internal slack", {C_FREE_BODY, C_FREE_SLACK, C_ORPHAN_AHDR, C_ORPHAN_BODY, C_GAP, C_LIVE_SLACK}, 0);
    pages("D(upper bound): every free-list/orphan/gap byte incl. headers", {C_FREE_AHDR, C_FREE_FBH, C_FREE_BODY, C_FREE_SLACK, C_ORPHAN_AHDR, C_ORPHAN_BODY, C_GAP, C_LIVE_SLACK}, 0);
    {
        std::uint64_t best = 0, run = 0, runs = 0, best_at = 0, max_blk = 0;
        for (std::uint64_t i = 0; i < P; ++i) {
            const bool d = cls[i] == C_FREE_AHDR || cls[i] == C_FREE_FBH || cls[i] == C_FREE_BODY || cls[i] == C_FREE_SLACK;
            if (d) { if (!run) ++runs; ++run; if (run > best) { best = run; best_at = i + 1 - run; } } else run = 0;
        }
        for (auto& [u, fr] : freel) max_blk = std::max<std::uint64_t>(max_blk, fr.block_bytes);
        std::printf("  free-list contiguous runs=%" PRIu64 "  longest run=%" PRIu64 " B at payload off %" PRIu64 "  max block_bytes=%" PRIu64 "\n", runs, best, best_at, max_blk);
    }
    std::printf("-- same, wire-file-relative pages (payload starts at file offset 88) --\n");
    pages("A(file)", {C_FREE_BODY, C_FREE_SLACK}, 88);
    pages("B(file)", {C_FREE_BODY, C_FREE_SLACK, C_ORPHAN_AHDR, C_ORPHAN_BODY, C_GAP}, 88);
    pages("C(file)", {C_FREE_BODY, C_FREE_SLACK, C_ORPHAN_AHDR, C_ORPHAN_BODY, C_GAP, C_LIVE_SLACK}, 88);

    // Are dead bytes zero? (informational)
    std::uint64_t nz_free = 0, nz_orphan = 0;
    for (std::uint64_t i = 0; i < P; ++i) {
        if ((cls[i] == C_FREE_BODY || cls[i] == C_FREE_SLACK) && pl[i]) ++nz_free;
        if (cls[i] == C_ORPHAN_BODY && pl[i]) ++nz_orphan;
    }
    std::printf("nonzero bytes: free-list body+slack=%" PRIu64 "  orphan body=%" PRIu64 "\n\n", nz_free, nz_orphan);
}
} // namespace

int main(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; i += 2) analyze(argv[i], argv[i + 1]);
    return 0;
}
