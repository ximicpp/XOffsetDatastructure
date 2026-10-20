// B5 diagnostic (count only, NOT shippable): instrumented replay of the M
// input + trace through the real W9a library (hooked copy in b5/src) and the
// real serialization_comparison XOffset adapter. The free-list-layer request
// stream (allocate_dynamic_bytes / retire_dynamic_bytes / try_expand) is
// logged, translated to logical block ids, and replayed through a shadow
// allocator under several placement policies. P0 must reproduce the real run
// offset-for-offset.
#include "adapter.hpp"
#include "adapters/xoffset/adapter.hpp"
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <unordered_map>
#include <vector>

using namespace XOffsetDatastructure;

namespace {
sc::Bytes read_file(const char* p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) { std::fprintf(stderr, "cannot open %s\n", p); std::exit(2); }
    sc::Bytes b(static_cast<std::size_t>(f.tellg()));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(b.data()), static_cast<std::streamsize>(b.size()));
    return b;
}
void need(bool b, const char* m) { if (!b) { std::fprintf(stderr, "FAIL: %s\n", m); std::exit(3); } }

constexpr std::uint32_t HDR = 16;
constexpr std::uint32_t NCLS = 16;
constexpr std::uint32_t MAXBB = 8u << 15; // 262144
std::uint32_t cls_of(std::uint32_t b) { // == fixed_detail::freelist_size_class
    std::uint32_t block = 8, i = 0;
    while (block < b && i + 1 < NCLS) { block <<= 1; ++i; }
    return block < b ? NCLS : i;
}
std::uint32_t r8(std::uint32_t v) { return (v + 7u) & ~7u; }

struct IdEv { char op; std::uint32_t id; std::uint32_t bytes; std::uint32_t align; std::int64_t fb; bool ok; };
struct Init { std::uint32_t user, ub, align; };

struct Policy {
    const char* name;
    bool split = false;        // P1: split remainder >= HDR+8 back into free lists
    bool coalesce = false;     // P2: merge address-adjacent free blocks on retire
    bool phys_retire = false;  // free block_bytes = physical user_bytes (needed by coalescing)
    bool tail_trim = false;    // variant: a free block ending at used_end returns to the tail
    std::uint32_t cap = MAXBB; // merged block_bytes limit (current reader: 262144)
};

struct Result {
    std::uint64_t used_end = 0, live_blocks = 0, live_req = 0, live_slack = 0, free_blocks = 0, free_bb = 0,
                  free_phys = 0, max_free = 0, over_cap_free = 0, core_free_blocks = 0, core_free_bytes = 0,
                  splits = 0, merges = 0, merges_capped = 0, trims = 0, trimmed_bytes = 0, expand_div_a = 0,
                  expand_div_b = 0, fl_hits = 0, bumps = 0, rcb = 0, offset_mismatch = 0, gaps = 0;
};

struct Shadow {
    struct Blk { std::uint32_t hdr, user, ub, align, bb; std::int32_t prev = -1, next = -1; std::uint8_t st = 0; std::uint8_t cls = 0; };
    // st: 0 live, 1 free-list, 2 core-free, 3 dead(merged/trimmed)
    Policy P;
    std::vector<Blk> b;
    std::map<std::uint32_t, std::int32_t> by_hdr;
    std::int32_t head[NCLS + 1];
    std::uint64_t used_end = 0, buf = 0, bufmax = 0;
    std::uint64_t free_bytes = 0;
    std::vector<std::int32_t> core_free; // Core free_blocks_ order
    std::vector<std::int32_t> id2b;
    std::vector<std::uint32_t> req;      // logical capacity bytes (aligned) per id
    std::vector<std::uint8_t> aliased;   // id retired implicitly (expand replaced fallback)
    Result R;
    bool stats = false;
    struct St { std::uint64_t bumps = 0, bump_bytes = 0, fl_hits = 0, retires = 0, retire_bytes = 0, used_end = 0, free_b = 0,
                maxfree = 0, maxfree_at_bump = 0, min_req_bump = ~0ull, max_req_bump = 0, splits = 0, merges = 0; };
    std::vector<St> st{1};
    std::uint64_t cur_maxfree() const {
        std::uint64_t m = 0;
        for (std::uint32_t c = 0; c <= NCLS; ++c) for (auto i = head[c]; i >= 0; i = b[i].next) m = std::max<std::uint64_t>(m, b[i].bb);
        return m;
    }

    void push_fl(std::int32_t i) {
        auto& x = b[i];
        std::uint32_t c = cls_of(x.bb);
        if (c >= NCLS && P.cap > MAXBB) c = NCLS; // hypothetical 17th list (uncapped variant)
        x.cls = static_cast<std::uint8_t>(c); x.st = 1; x.prev = -1; x.next = head[c];
        if (head[c] >= 0) b[head[c]].prev = i;
        head[c] = i;
        free_bytes += x.bb;
    }
    void unlink(std::int32_t i) {
        auto& x = b[i];
        if (x.prev >= 0) b[x.prev].next = x.next; else head[x.cls] = x.next;
        if (x.next >= 0) b[x.next].prev = x.prev;
        x.prev = x.next = -1;
        free_bytes -= x.bb;
    }
    std::int32_t make(std::uint32_t hdr, std::uint32_t user, std::uint32_t ub, std::uint32_t align) {
        b.push_back(Blk{hdr, user, ub, align, 0});
        auto i = static_cast<std::int32_t>(b.size() - 1);
        by_hdr[hdr] = i;
        return i;
    }
    void init(const std::vector<Init>& in, std::uint64_t ue, std::uint64_t buf0, std::uint64_t bmax, std::size_t nid) {
        for (auto& h : head) h = -1;
        b.reserve(in.size() + nid + 1024);
        id2b.assign(nid, -1); req.assign(nid, 0); aliased.assign(nid, 0);
        for (std::size_t k = 0; k < in.size(); ++k) {
            id2b[k] = make(in[k].user - HDR, in[k].user, in[k].ub, in[k].align);
            req[k] = in[k].ub;
        }
        used_end = ue; buf = buf0; bufmax = bmax;
    }
    void maybe_split(std::int32_t i, std::uint32_t A) {
        if (!P.split) return;
        auto ub = b[i].ub;
        if (ub < A || ub - A < HDR + 8) return;
        const std::uint32_t nh = b[i].user + A;
        b[i].ub = A;
        auto j = make(nh, nh + HDR, ub - A - HDR, 8);
        b[j].bb = b[j].ub;
        ++R.splits;
        push_fl(j);
    }
    std::int32_t core_alloc(std::uint32_t A, std::uint32_t align, bool growth) {
        const std::uint32_t al = std::max<std::uint32_t>(align, 8);
        for (std::size_t k = 0; k < core_free.size(); ++k) {
            auto i = core_free[k];
            if (b[i].ub >= A && b[i].align >= al && b[i].user % al == 0) {
                core_free.erase(core_free.begin() + static_cast<std::ptrdiff_t>(k));
                b[i].st = 0;
                return i;
            }
        }
        std::uint64_t user = ((used_end + HDR) + al - 1) & ~std::uint64_t(al - 1);
        std::uint64_t end = user + A;
        if (end > buf) {
            if (!growth) return -1;
            const auto geo = buf > bufmax / 2 ? bufmax : buf * 2;
            buf = std::max<std::uint64_t>(end, geo);
        }
        if (user - HDR != used_end) ++R.gaps;
        if (stats) { auto& t = st.back(); ++t.bumps; t.bump_bytes += end - used_end; t.maxfree_at_bump = std::max(t.maxfree_at_bump, cur_maxfree());
                     t.min_req_bump = std::min<std::uint64_t>(t.min_req_bump, A); t.max_req_bump = std::max<std::uint64_t>(t.max_req_bump, A); }
        auto i = make(static_cast<std::uint32_t>(user - HDR), static_cast<std::uint32_t>(user), A, al);
        used_end = end;
        ++R.bumps;
        return i;
    }
    void release_cached() {
        ++R.rcb;
        for (std::uint32_t c = 0; c <= NCLS; ++c) {
            auto i = head[c];
            head[c] = -1;
            while (i >= 0) { auto n = b[i].next; b[i].st = 2; b[i].prev = b[i].next = -1; core_free.push_back(i); i = n; }
        }
        free_bytes = 0;
    }
    std::int32_t alloc(std::uint32_t A, std::uint32_t align) {
        const auto c0 = cls_of(A);
        if (c0 < NCLS) {
            for (std::uint32_t c = c0; c <= NCLS; ++c) {
                for (auto i = head[c]; i >= 0; i = b[i].next) {
                    if (b[i].bb >= A && b[i].user % align == 0) {
                        unlink(i);
                        b[i].st = 0;
                        ++R.fl_hits; if (stats) ++st.back().fl_hits;
                        maybe_split(i, A);
                        return i;
                    }
                }
            }
        }
        auto i = core_alloc(A, align, false);
        if (i < 0) {
            if (free_bytes != 0) release_cached();
            i = core_alloc(A, align, true);
        }
        return i;
    }
    void trim_tail() {
        while (!by_hdr.empty()) {
            auto it = std::prev(by_hdr.end());
            auto i = it->second;
            if (b[i].st != 1 || std::uint64_t(b[i].user) + b[i].ub != used_end) return;
            unlink(i);
            b[i].st = 3;
            used_end = b[i].hdr;
            ++R.trims; R.trimmed_bytes += HDR + b[i].ub;
            by_hdr.erase(it);
        }
    }
    void retire(std::int32_t i, std::uint32_t bytes) {
        if (stats) { ++st.back().retires; st.back().retire_bytes += bytes; }
        const auto c = cls_of(bytes);
        if (c >= NCLS) { b[i].st = 2; core_free.push_back(i); return; }
        b[i].bb = P.phys_retire ? std::max(bytes, b[i].ub) : bytes;
        if (P.coalesce) {
            // next neighbour
            auto it = by_hdr.find(b[i].hdr);
            auto nx = std::next(it);
            if (nx != by_hdr.end()) {
                auto j = nx->second;
                if (b[j].st == 1 && b[j].hdr == b[i].user + b[i].ub) {
                    const std::uint64_t m = std::uint64_t(b[i].ub) + HDR + b[j].ub;
                    if (m <= P.cap) {
                        unlink(j); b[j].st = 3; by_hdr.erase(nx);
                        b[i].ub = static_cast<std::uint32_t>(m); ++R.merges;
                    } else ++R.merges_capped;
                }
            }
            it = by_hdr.find(b[i].hdr);
            if (it != by_hdr.begin()) {
                auto pv = std::prev(it);
                auto j = pv->second;
                if (b[j].st == 1 && b[j].user + b[j].ub == b[i].hdr) {
                    const std::uint64_t m = std::uint64_t(b[j].ub) + HDR + b[i].ub;
                    if (m <= P.cap) {
                        unlink(j);
                        b[j].ub = static_cast<std::uint32_t>(m);
                        b[i].st = 3; by_hdr.erase(it);
                        i = j; ++R.merges;
                    } else ++R.merges_capped;
                }
            }
            b[i].bb = b[i].ub;
        }
        push_fl(i);
        if (P.tail_trim) trim_tail();
    }
    bool at_tail(std::int32_t i) const { return std::uint64_t(b[i].user) + b[i].ub == used_end; }

    void close_stage() {
        auto& t = st.back(); t.used_end = used_end; t.free_b = free_bytes; t.maxfree = cur_maxfree();
        t.splits = R.splits; t.merges = R.merges; st.emplace_back();
    }
    void run(const std::vector<IdEv>& ev, const std::vector<std::uint32_t>* real_off, const std::vector<std::size_t>* se = nullptr) {
        std::int64_t pending_alias_old = -1; // shadow expanded where real fell back
        std::size_t sk = 0;
        for (std::size_t k = 0; k < ev.size(); ++k) {
            while (stats && se && sk < se->size() && (*se)[sk] == k) { close_stage(); ++sk; }
            const auto& e = ev[k];
            if (e.op == 'A') {
                if (e.fb >= 0 && e.fb == pending_alias_old) {
                    // real: fallback allocation after failed expand; shadow expanded in place
                    id2b[e.id] = id2b[e.fb];
                    req[e.id] = e.bytes;
                    aliased[e.fb] = 1;
                    pending_alias_old = -1;
                    continue;
                }
                pending_alias_old = -1;
                auto i = alloc(e.bytes, e.align);
                id2b[e.id] = i; req[e.id] = e.bytes;
                if (real_off && b[i].user != (*real_off)[k]) ++R.offset_mismatch;
            } else if (e.op == 'R') {
                if (aliased[e.id]) { aliased[e.id] = 0; id2b[e.id] = -1; continue; }
                retire(id2b[e.id], e.bytes);
                id2b[e.id] = -1;
            } else if (e.op == 'E') {
                auto i = id2b[e.id];
                const bool sh = at_tail(i);
                if (sh) {
                    if (e.bytes > b[i].ub) {
                        const std::uint64_t end = std::uint64_t(b[i].user) + e.bytes;
                        if (end > buf) buf = std::max<std::uint64_t>(end, buf > bufmax / 2 ? bufmax : buf * 2);
                        b[i].ub = e.bytes; used_end = end;
                    }
                }
                if (e.ok && sh) { req[e.id] = e.bytes; }
                else if (!e.ok && sh) { ++R.expand_div_a; pending_alias_old = e.id; }
                else if (e.ok && !sh) {
                    // real expanded in place; shadow must relocate: allocate new, retire old capacity
                    ++R.expand_div_b;
                    auto ni = alloc(e.bytes, e.align);
                    retire(i, req[e.id]);
                    id2b[e.id] = ni; req[e.id] = e.bytes;
                }
            }
        }
    }
    void finish(std::vector<std::uint8_t>& live_id_mask) {
        while (stats && st.size() <= 9) close_stage();
        R.used_end = used_end;
        for (std::size_t id = 0; id < id2b.size(); ++id) {
            if (id2b[id] < 0) continue;
            auto i = id2b[id];
            ++R.live_blocks; R.live_req += req[id];
            R.live_slack += b[i].ub > req[id] ? b[i].ub - req[id] : 0;
            live_id_mask[id] = 1;
        }
        for (std::uint32_t c = 0; c <= NCLS; ++c)
            for (auto i = head[c]; i >= 0; i = b[i].next) {
                ++R.free_blocks; R.free_bb += b[i].bb; R.free_phys += b[i].ub;
                R.max_free = std::max<std::uint64_t>(R.max_free, b[i].bb);
                if (b[i].bb > MAXBB) ++R.over_cap_free;
            }
        for (auto i : core_free) { ++R.core_free_blocks; R.core_free_bytes += HDR + b[i].ub; }
    }
};
} // namespace

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: replay input.wire trace.bin [final.wire.out]\n"); return 2; }
    auto input = read_file(argv[1]);
    auto trace = sc::decode_trace(read_file(argv[2]));
    auto state = sc::XOffsetAdapter::load(input);
    const char* base = static_cast<const char*>(state->get_address());
    auto arena_used_end = [&]() { XBufferCore::arena_header ah{}; std::memcpy(&ah, state->get_address(), sizeof(ah)); return ah.used_end; };
    auto arena_alloc_off = [&]() { XBufferCore::arena_header ah{}; std::memcpy(&ah, state->get_address(), sizeof(ah)); return ah.allocator_offset; };
    const std::uint64_t ue0 = arena_used_end();
    const std::uint64_t buf0 = state->segment_size();
    std::printf("input: file=%zu used_end=%" PRIu64 " segment_size(after load)=%" PRIu64 "\n", input.size(), ue0, buf0);

    // Initial header chain (input wire has no gaps / free blocks; verify).
    std::vector<Init> init;
    for (std::uint64_t cur = sizeof(XBufferCore::arena_header); cur < ue0;) {
        XBufferCore::AllocationHeader h{};
        std::memcpy(&h, base + cur, sizeof(h));
        need(h.user_offset == cur + HDR && h.total_bytes == h.user_bytes + HDR, "initial header chain");
        init.push_back({h.user_offset, h.user_bytes, h.alignment});
        cur = std::uint64_t(h.user_offset) + h.user_bytes;
    }
    std::printf("initial blocks=%zu\n", init.size());

    std::vector<b5diag::Ev> log;
    log.reserve(1 << 20);
    b5diag::log = &log;
    b5diag::enabled = true;
    std::vector<std::size_t> log_stage_end;
    for (std::size_t next = 0, si = 0; si < trace.stages.size(); ++si) {
        auto start = next;
        while (next < trace.mutations.size() && trace.mutations[next].stage == trace.stages[si].stage) ++next;
        sc::XOffsetAdapter::mutate(state, std::span(trace.mutations).subspan(start, next - start));
        log_stage_end.push_back(log.size());
    }
    b5diag::enabled = false;
    std::printf("events=%zu  stray_core_alloc=%" PRIu64 " core_dealloc=%" PRIu64 " core_free_reuse=%" PRIu64
                " release_cached_blocks=%" PRIu64 " growths=%" PRIu64 " segment_size(after mutate)=%zu\n",
                log.size(), b5diag::stray_core_alloc, b5diag::core_dealloc, b5diag::core_free_reuse, b5diag::rcb,
                b5diag::growths, state->segment_size());
    need(state->get_address() == base, "payload base moved");
    {
        auto canonical = sc::encode_canonical(sc::XOffsetAdapter::export_state(state));
        need(sc::sha256(canonical) == trace.stages.back().canonical_digest, "final canonical digest");
        std::printf("final canonical digest matches trace\n");
    }
    const std::uint64_t ue_real = arena_used_end();
    auto wire = sc::XOffsetAdapter::emit(state);
    std::printf("real final: used_end=%" PRIu64 " emitted wire=%zu sha256=%s\n", ue_real, wire.size(),
                sc::sha256_hex(sc::wire_view(wire)).c_str());
    if (argc > 3) { std::ofstream o(argv[3], std::ios::binary); o.write(wire.data(), static_cast<std::streamsize>(wire.size())); }
    // Real final allocator free lists.
    std::uint64_t real_fl_n = 0, real_fl_b = 0;
    {
        auto* st = reinterpret_cast<const XAllocatorStateV1*>(base + arena_alloc_off());
        for (std::uint32_t c = 0; c < NCLS; ++c)
            for (auto off = st->freelist_head[c]; off;) {
                fixed_detail::FreeBlockHeader fb{};
                std::memcpy(&fb, reinterpret_cast<const char*>(st) + off, sizeof(fb));
                ++real_fl_n; real_fl_b += fb.block_bytes; off = fb.next_offset;
            }
        std::printf("real final free lists: blocks=%" PRIu64 " sum(block_bytes)=%" PRIu64 " free_bytes=%u\n", real_fl_n, real_fl_b, st->free_bytes);
    }

    // Translate offsets -> logical ids.
    std::unordered_map<std::uint32_t, std::uint32_t> live;
    live.reserve(init.size() * 2);
    std::uint32_t next_id = 0;
    for (auto& x : init) live[x.user] = next_id++;
    std::vector<std::uint32_t> reqv(init.size());
    for (std::size_t k = 0; k < init.size(); ++k) reqv[k] = init[k].ub;
    std::vector<IdEv> ev;
    std::vector<std::uint32_t> real_off;
    ev.reserve(log.size()); real_off.reserve(log.size());
    std::uint64_t nA = 0, nAfl = 0, nR = 0, nE = 0, nEok = 0, nG = 0, nK = 0, nFB = 0, retire_req_mismatch = 0,
                  between_fb = 0, max_align = 0;
    std::int64_t open_fb_old = -1; std::uint64_t open_fb_events = 0;
    std::vector<std::size_t> ev_stage_end;
    for (std::size_t li = 0; li <= log.size(); ++li) {
        while (ev_stage_end.size() < log_stage_end.size() && log_stage_end[ev_stage_end.size()] == li) ev_stage_end.push_back(ev.size());
        if (li == log.size()) break;
        auto& e = log[li];
        if (e.op == 'A') {
            ++nA; if (e.src == 1) ++nAfl;
            max_align = std::max<std::uint64_t>(max_align, e.align);
            std::int64_t fb = -1;
            if (e.aux) { auto it = live.find(e.aux); need(it != live.end(), "fallback old id"); fb = it->second; ++nFB; }
            need(live.find(e.off) == live.end(), "alloc over live block");
            const auto id = next_id++;
            live[e.off] = id;
            reqv.push_back(e.bytes);
            ev.push_back({'A', id, e.bytes, e.align, fb, false});
            real_off.push_back(e.off);
            if (open_fb_old >= 0) between_fb += 1;
            if (fb >= 0) { open_fb_old = fb; open_fb_events = 0; }
        } else if (e.op == 'R') {
            ++nR;
            auto it = live.find(e.off);
            need(it != live.end(), "retire of unknown block");
            const auto id = it->second;
            if (r8(reqv[id]) != e.bytes) ++retire_req_mismatch;
            live.erase(it);
            ev.push_back({'R', id, e.bytes, 8, -1, false});
            real_off.push_back(e.off);
            if (open_fb_old >= 0 && static_cast<std::int64_t>(id) == open_fb_old) open_fb_old = -1;
            else if (open_fb_old >= 0) ++between_fb;
        } else if (e.op == 'E') {
            ++nE; if (e.src) ++nEok;
            auto it = live.find(e.off);
            need(it != live.end(), "expand of unknown block");
            if (e.src) reqv[it->second] = e.bytes;
            ev.push_back({'E', it->second, e.bytes, e.align, -1, e.src != 0});
            real_off.push_back(e.off);
        } else if (e.op == 'G') ++nG;
        else if (e.op == 'K') ++nK;
    }
    std::printf("stream: alloc=%" PRIu64 " (free-list %" PRIu64 ", core %" PRIu64 ")  retire=%" PRIu64 "  expand=%" PRIu64
                " (ok %" PRIu64 ")  fallback-allocs=%" PRIu64 "  no-growth-probe-fail=%" PRIu64 "  rcb=%" PRIu64
                "  retire/capacity mismatch=%" PRIu64 "  events between fallback alloc and old retire=%" PRIu64 "  max align=%" PRIu64 "\n",
                nA, nAfl, nA - nAfl, nR, nE, nEok, nFB, nG, nK, retire_req_mismatch, between_fb, max_align);

    const std::uint64_t P4 = 88 + sizeof(XBufferCore::arena_header);
    Policy pols[] = {
        {"P0 current", false, false, false, false},
        {"P1 split-on-reuse", true, false, false, false},
        {"P1' split+phys-retire", true, false, true, false},
        {"P2 coalesce(cap 256K)", false, true, true, false},
        {"P3 split+coalesce", true, true, true, false},
        {"P2u coalesce uncapped", false, true, true, false, 0xffffffffu},
        {"P3u split+coal uncapped", true, true, true, false, 0xffffffffu},
        {"P2T coalesce+tail-trim", false, true, true, true},
        {"P3T split+coal+trim", true, true, true, true},
    };
    std::uint64_t p0_wire = 0;
    std::vector<std::pair<const char*, std::vector<Shadow::St>>> stage_tables;
    std::printf("\n%-24s %12s %10s %7s | %10s %8s %10s %7s %9s %7s %7s %7s %6s %5s %5s\n", "policy", "wire", "saved", "saved%",
                "live_slack", "freeblk", "free_bytes", "maxfree", "splits", "merges", "capped", "fl_hit", "bumps", "divA", "divB");
    for (auto& p : pols) {
        Shadow s; s.P = p;
        if (p.coalesce) s.P.phys_retire = true;
        s.init(init, ue0, buf0, sc::native_arena_maximum, next_id);
        s.stats = true;
        s.run(ev, p.split || p.coalesce || p.tail_trim ? nullptr : &real_off, &ev_stage_end);
        std::vector<std::uint8_t> mask(next_id, 0);
        s.finish(mask);
        stage_tables.push_back({p.name, s.st});
        const auto& r = s.R;
        const std::uint64_t w = r.used_end + 88;
        if (!p0_wire) {
            p0_wire = w;
            std::printf("[P0 check] shadow used_end=%" PRIu64 " real=%" PRIu64 " offset mismatches=%" PRIu64
                        " free blocks=%" PRIu64 "/%" PRIu64 " free bytes=%" PRIu64 "/%" PRIu64 " gaps=%" PRIu64 " rcb=%" PRIu64 "\n",
                        r.used_end, ue_real, r.offset_mismatch, r.free_blocks, real_fl_n, r.free_bb, real_fl_b, r.gaps, r.rcb);
            // P4 lower bound from P0 live set: fixed headers + live AllocationHeaders + live capacity bytes.
            std::uint64_t lb = P4 + r.live_blocks * HDR + r.live_req;
            std::printf("[P4] live blocks=%" PRIu64 " live_req=%" PRIu64 " -> lower-bound wire=%" PRIu64 "\n", r.live_blocks, r.live_req, lb);
        }
        const double saved = double(p0_wire) - double(w);
        std::printf("%-24s %12" PRIu64 " %10.0f %6.2f%% | %10" PRIu64 " %8" PRIu64 " %10" PRIu64 " %7" PRIu64 " %9" PRIu64
                    " %7" PRIu64 " %7" PRIu64 " %7" PRIu64 " %6" PRIu64 " %5" PRIu64 " %5" PRIu64 "  rcb=%" PRIu64 " corefree=%" PRIu64 "/%" PRIu64
                    " trims=%" PRIu64 " overcap_free=%" PRIu64 " gaps=%" PRIu64 "\n",
                    p.name, w, saved, 100.0 * saved / double(p0_wire), r.live_slack, r.free_blocks, r.free_bb, r.max_free, r.splits,
                    r.merges, r.merges_capped, r.fl_hits, r.bumps, r.expand_div_a, r.expand_div_b, r.rcb, r.core_free_blocks,
                    r.core_free_bytes, r.trims, r.over_cap_free, r.gaps);
    }
    const char* want[] = {"P0 current", "P3u split+coal uncapped", "P2 coalesce(cap 256K)"};
    for (auto& [nm, v] : stage_tables) {
        bool w = false; for (auto x : want) if (!std::strcmp(x, nm)) w = true;
        if (!w) continue;
        std::printf("\n-- per stage: %s --\n stg %6s %9s %7s %7s %7s %9s %10s %9s %7s %8s %7s %7s\n", nm, "bumps", "bump_B", "req_min", "req_max",
                    "fl_hit", "retires", "used_end", "free_B", "maxfree", "mxf@bump", "splits", "merges");
        for (std::size_t i = 0; i < 9 && i < v.size(); ++i) {
            auto& t = v[i];
            std::printf(" %3zu %6" PRIu64 " %9" PRIu64 " %7" PRIu64 " %7" PRIu64 " %7" PRIu64 " %9" PRIu64 " %10" PRIu64 " %9" PRIu64 " %7" PRIu64 " %8" PRIu64 " %7" PRIu64 " %7" PRIu64 "\n",
                        i + 1, t.bumps, t.bump_bytes, t.bumps ? t.min_req_bump : 0, t.max_req_bump, t.fl_hits, t.retires, t.used_end, t.free_b,
                        t.maxfree, t.maxfree_at_bump, t.splits, t.merges);
        }
    }
    return 0;
}
