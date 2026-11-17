// Hole table differential (4.2.0): a buffer whose received hole table is
// loaded lazily must behave exactly like one whose table is built at once.
//
// XOFFSET_TEST_EAGER_HOLE_TABLE turns on XBufferCore::test_eager_hole_table(),
// a per-thread switch that makes every hole table load on this thread take
// the eager path (the bin index built during the load, as every load did
// before the lazy table). Each case loads the same wire twice, once with the
// switch off (lazy) and once with it on (eager), then runs one random
// operation sequence on both and compares after every step: the offset each
// allocation returns, free_block_count, free_block_bytes, the hole table in
// canonical order, the bytes write_free_blocks emits, the free runs a
// free_runs save skips, and the saved wire.
//
// Two levels:
//  - the allocator alone (XBufferCore): allocations of every bin size, large
//    blocks, aligned requests, retirements, in-place hole table reloads,
//    reloads into a table that still has entries, and fresh owners loaded
//    from both buffers' bytes;
//  - typed roots received through XReceiver (admit) and reload_verified:
//    container and string edits, saves (dense and free_runs), clone, a
//    reload into the same buffer, and reset.
#define XOFFSET_FORCE_FULL_CHECK 0
#define XOFFSET_TEST_EAGER_HOLE_TABLE 1
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <vector>
#include "../xoffset_receiver.hpp"
using namespace XOffsetDatastructure;
namespace fd = XOffsetDatastructure::fixed_detail;

struct HtItem {
    std::uint32_t id{};
    XString name;
    XVector<std::uint16_t> marks;
};
XOFFSET_RECORD_DERIVED(HtItem, (), id, name, marks)

struct HtRoot {
    std::uint64_t seq{};
    XString title;
    XVector<std::uint32_t> values;
    XVector<HtItem> items;
    XMap<std::uint32_t, XString> names;
};
XOFFSET_RECORD_DERIVED(HtRoot, (), seq, title, values, items, names)
XOFFSET_REGISTER_SCHEMA_NAME(HtRoot, "test.HoleTableRoot")

namespace {
int failures = 0;
std::size_t steps = 0;
void need(bool ok, const char* what) {
    if (!ok && failures++ < 20) std::fprintf(stderr, "FAIL: %s\n", what);
}

using Hole = XBufferCore::WireHole;

struct Rng {
    std::uint64_t s;
    std::uint64_t next() {
        s += 0x9e3779b97f4a7c15ull;
        std::uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
    std::uint32_t below(std::uint32_t n) { return n ? std::uint32_t(next() % n) : 0; }
};

// Loads with the eager switch set as asked, and restores it.
template <typename Fn>
void with_eager(bool eager, Fn&& fn) {
    XBufferCore::test_eager_hole_table() = eager;
    fn();
    XBufferCore::test_eager_hole_table() = false;
}

std::vector<Hole> holes_of(const XBufferCore& core) {
    std::vector<Hole> out;
    core.for_each_free_block([&](Hole h) { out.push_back(h); });
    return out;
}

std::vector<Hole> holes_at_least(const XBufferCore& core, std::uint32_t min_bytes) {
    std::vector<Hole> out;
    core.for_each_free_block_at_least(min_bytes, [&](Hole h) { out.push_back(h); });
    std::sort(out.begin(), out.end(), [](Hole a, Hole b) { return a.offset < b.offset; });
    return out;
}

bool same(const std::vector<Hole>& a, const std::vector<Hole>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](Hole x, Hole y) {
        return x.offset == y.offset && x.bytes == y.bytes;
    });
}

std::string table_bytes(const XBufferCore& core) {
    std::string out(core.free_block_count() * sizeof(Hole), '\0');
    core.write_free_blocks(out.data());
    return out;
}

std::size_t used(const XBufferCore& b) { return static_cast<std::size_t>(b.arena()->used_end); }

// Everything a later decision or a later save can depend on.
void agree(const XBufferCore& lazy, const XBufferCore& eager, const char* what) {
    ++steps;
    need(lazy.free_table_complete() == eager.free_table_complete(), what);
    need(lazy.free_block_count() == eager.free_block_count(), what);
    need(lazy.free_block_bytes() == eager.free_block_bytes(), what);
    need(used(lazy) == used(eager), what);
    need(same(holes_of(lazy), holes_of(eager)), what);
    need(table_bytes(lazy) == table_bytes(eager), what);
    for (std::uint32_t min_bytes : {8u, 256u, 4096u, 4104u})
        need(same(holes_at_least(lazy, min_bytes), holes_at_least(eager, min_bytes)), what);
}

// ---- The allocator alone --------------------------------------------------

constexpr std::size_t kCapacity = 32u * 1024u * 1024u;
auto core_cap() { return XBufferCore::maximum_capacity(kCapacity).with_growth(XGrowPolicy::Automatic); }

struct Block { std::uint32_t offset, bytes; };

// A source arena with holes of many sizes (runs of one size, single holes,
// large blocks of equal sizes) left by a random alloc/retire history.
struct Source {
    XBufferCore core{std::size_t(1) << 20, core_cap()};
    std::vector<Block> live;
};

std::uint32_t pick_size(Rng& rng) {
    static const std::uint32_t sizes[] = {8, 16, 24, 32, 40, 48, 64, 72, 96, 128, 136, 256, 264, 512,
                                          1024, 2048, 4088, 4096, 4104, 5000 & ~7u, 8192, 12288};
    if (rng.below(5) == 0) return 8u * (1u + rng.below(600));
    return sizes[rng.below(sizeof(sizes) / sizeof(sizes[0]))];
}

Source make_core_source(Rng& rng, unsigned ops) {
    Source s;
    fd::allocator_ref ref{s.core.arena()};
    for (unsigned i = 0; i < ops; ++i) {
        if (!s.live.empty() && rng.below(5) < 2) {
            const auto at = rng.below(std::uint32_t(s.live.size()));
            const auto b = s.live[at];
            fd::retire_dynamic_bytes(ref, s.core.data() + b.offset, b.bytes);
            s.live.erase(s.live.begin() + at);
        } else {
            const auto bytes = pick_size(rng);
            auto* p = fd::allocate_dynamic_bytes(ref, bytes, 8);
            s.live.push_back({std::uint32_t(static_cast<char*>(p) - s.core.data()), bytes});
        }
    }
    return s;
}

// Two owners of the same bytes and holes: one loaded lazily, one eagerly.
struct Pair {
    XBufferCore lazy, eager;
    std::vector<Block> live;
};

void load_pair(Pair& p, const std::string& wire, const std::vector<Hole>& holes) {
    p.lazy = XBufferCore(wire.data(), wire.size(), core_cap());
    p.eager = XBufferCore(wire.data(), wire.size(), core_cap());
    with_eager(false, [&] { p.lazy.replace_free_blocks(holes); });
    with_eager(true, [&] { p.eager.replace_free_blocks(holes); });
}

void core_case(std::uint64_t seed) {
    Rng rng{seed};
    auto src = make_core_source(rng, 200 + rng.below(1500));
    Pair p;
    load_pair(p, src.core.copy_bytes(), holes_of(src.core));
    p.live = src.live;
    agree(p.lazy, p.eager, "core: after load");
    const unsigned ops = 50 + rng.below(400);
    for (unsigned i = 0; i < ops; ++i) {
        const auto op = rng.below(100);
        if (op < 45) {
            const auto bytes = pick_size(rng);
            const std::size_t align = rng.below(10) == 0 ? (std::size_t(16) << rng.below(2)) : 8;
            auto* a = p.lazy.allocate_block(bytes, align);
            auto* b = p.eager.allocate_block(bytes, align);
            const auto oa = std::uint32_t(static_cast<char*>(a) - p.lazy.data());
            const auto ob = std::uint32_t(static_cast<char*>(b) - p.eager.data());
            need(oa == ob, "core: allocation placement differs");
            p.live.push_back({oa, bytes});
        } else if (op < 80 && !p.live.empty()) {
            const auto at = rng.below(std::uint32_t(p.live.size()));
            const auto b = p.live[at];
            p.lazy.retire_block(p.lazy.data() + b.offset, b.bytes);
            p.eager.retire_block(p.eager.data() + b.offset, b.bytes);
            p.live.erase(p.live.begin() + at);
        } else if (op < 88) {
            // Save and receive the bytes into fresh owners (both sides keep
            // their own mode for the new load).
            const auto wire = p.lazy.copy_bytes();
            need(wire == p.eager.copy_bytes(), "core: payload bytes differ");
            const auto holes = holes_of(p.lazy);
            auto live = p.live;
            load_pair(p, wire, holes);
            p.live = std::move(live);
        } else if (op < 94) {
            // An exact recomputation into a table that still has entries.
            const auto holes = holes_of(p.eager);
            with_eager(false, [&] { p.lazy.replace_free_blocks(holes); });
            with_eager(true, [&] { p.eager.replace_free_blocks(holes); });
        } else {
            // Hole table queries only (the const readers).
            (void)table_bytes(p.lazy);
            (void)holes_at_least(p.lazy, 256);
        }
        agree(p.lazy, p.eager, "core: after an operation");
    }
}

// ---- Typed roots received through XReceiver --------------------------------

constexpr std::size_t kMax = std::size_t(1) << 24;
auto cap() { return XBuffer::maximum_capacity(kMax).with_growth(XGrowPolicy::Automatic); }

std::span<const std::byte> bytes_of(const std::string& s) {
    return {reinterpret_cast<const std::byte*>(s.data()), s.size()};
}

std::string text(Rng& rng, std::uint32_t max_len, char c) { return std::string(rng.below(max_len), c); }

// One random edit, applied identically to both roots (the same rng draws).
void edit(XBuffer& b, std::uint64_t seed) {
    Rng rng{seed};
    auto& r = b.root<HtRoot>();
    switch (rng.below(9)) {
    case 0:
        for (auto n = rng.below(40); n; --n) r.values.push_back(std::uint32_t(rng.next()));
        break;
    case 1:
        r.title = text(rng, 120, char('a' + rng.below(26))).c_str();
        break;
    case 2:
        r.items.emplace_back();
        r.items.back().id = std::uint32_t(rng.next());
        r.items.back().name = text(rng, 80, 'q').c_str();
        for (auto n = rng.below(30); n; --n) r.items.back().marks.push_back(std::uint16_t(rng.next()));
        break;
    case 3:
        if (!r.items.empty()) r.items.erase(r.items.begin() + rng.below(std::uint32_t(r.items.size())));
        break;
    case 4:
        if (!r.items.empty()) r.items[rng.below(std::uint32_t(r.items.size()))].name = text(rng, 200, 'w').c_str();
        break;
    case 5:
        r.names.emplace(std::uint32_t(rng.next()), text(rng, 90, 'n').c_str());
        break;
    case 6:
        if (!r.names.empty()) r.names.erase(r.names.begin()->first);
        break;
    case 7:
        if (!r.values.empty()) r.values.resize(rng.below(std::uint32_t(r.values.size())));
        break;
    default:
        if (!r.items.empty()) {
            auto& item = r.items[rng.below(std::uint32_t(r.items.size()))];
            for (auto n = rng.below(200); n; --n) item.marks.push_back(std::uint16_t(n));
        }
        break;
    }
}

void agree_typed(XBuffer& lazy, XBuffer& eager, const char* what) {
    agree(lazy, eager, what);
    need(std::memcmp(lazy.data(), eager.data(), used(lazy)) == 0, what);
    need(lazy.save_verified<HtRoot>() == eager.save_verified<HtRoot>(), what);
    need(lazy.save_verified<HtRoot>(XSaveOptions{XWireEmission::free_runs}) ==
             eager.save_verified<HtRoot>(XSaveOptions{XWireEmission::free_runs}), what);
}

XBuffer make_typed_source(Rng& rng) {
    auto b = XBuffer::create<HtRoot>(4096, cap());
    b.root<HtRoot>().seq = rng.next();
    const unsigned edits = 20 + rng.below(rng.below(4) == 0 ? 3000 : 300);
    for (unsigned i = 0; i < edits; ++i) edit(b, rng.next());
    return b;
}

void typed_case(std::uint64_t seed) {
    Rng rng{seed};
    auto src = make_typed_source(rng);
    XReceiver<XLink::server_peer> rx(cap());
    const XWireEmission emissions[] = {XWireEmission::dense, XWireEmission::free_runs};
    const auto wire = src.save_verified<HtRoot>(XSaveOptions{emissions[rng.below(2)]});
    auto lazy = XBuffer::create<HtRoot>(4096, cap());
    auto eager = XBuffer::create<HtRoot>(4096, cap());
    const bool verified = rng.below(4) == 0;
    auto receive = [&](XBuffer& dst, const std::string& w, bool mode) {
        with_eager(mode, [&] {
            if (verified) dst.reload_verified<HtRoot>(w);
            else need(rx.reload<HtRoot>(dst, bytes_of(w)).has_value(), "typed: admit reload");
        });
    };
    receive(lazy, wire, false);
    receive(eager, wire, true);
    agree_typed(lazy, eager, "typed: after receive");
    const unsigned ops = 10 + rng.below(80);
    for (unsigned i = 0; i < ops; ++i) {
        const auto op = rng.below(100);
        if (op < 75) {
            const auto s = rng.next();
            edit(lazy, s);
            edit(eager, s);
        } else if (op < 85) {
            // Receive the current state again into the same buffers (the
            // kept table is emptied and refilled).
            const auto w = lazy.save_verified<HtRoot>(XSaveOptions{emissions[rng.below(2)]});
            receive(lazy, w, false);
            receive(eager, w, true);
        } else if (op < 93) {
            auto lc = lazy.clone();
            auto ec = eager.clone();
            agree_typed(lc, ec, "typed: clones differ");
            const auto s = rng.next();
            edit(lc, s);
            edit(ec, s);
            agree_typed(lc, ec, "typed: clones differ after an edit");
        } else {
            lazy.reset<HtRoot>();
            eager.reset<HtRoot>();
            receive(lazy, wire, false);
            receive(eager, wire, true);
        }
        agree_typed(lazy, eager, "typed: after an operation");
    }
}

}  // namespace

int main() {
    for (std::uint64_t seed = 1; seed <= 400; ++seed) core_case(seed);
    for (std::uint64_t seed = 1; seed <= 300; ++seed) typed_case(0x5eed0000u + seed);
    std::printf("hole table differential: %zu comparisons, %d failures\n", steps, failures);
    return failures == 0 ? 0 : 1;
}
