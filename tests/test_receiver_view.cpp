// Client uplinks (3.5 W4, trust tier P3 and P4): XReceiver::view with
// XCheck::bounds, the read-only in-place view of a wire.
//
//   - differential: on every wire the full check accepts, the bounds view
//     (and the admit view of a trusted link) reads exactly what reload_verified
//     loads;
//   - framing: the same rejections and codes as the full check, plus a sparse
//     wire and a misaligned wire;
//   - payload: every out-of-range block, size, string length and bool the walk
//     must catch, and the work bound for overlapping blocks;
//   - inputs the full check rejects but the bounds view accepts (overlap, key
//     order, padding) read in bounds: they are copied into exact-size heap
//     blocks, so AddressSanitizer catches a read past the wire.
//
// Built with XOFFSET_FORCE_FULL_CHECK=0 so that the walk itself is tested
// (test_receiver_forced covers the forced full check). No exception of its
// own: also built as test_receiver_view_noexc.
#define XOFFSET_FORCE_FULL_CHECK 0
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <vector>
#include "../xoffset_receiver.hpp"
using namespace XOffsetDatastructure;

struct VwItem {
    std::uint32_t id{};
    bool ready{};
    XString name;
    XVector<std::uint16_t> marks;
};
XOFFSET_RECORD_DERIVED(VwItem, (), id, ready, name, marks)

struct VwRoot {
    std::uint64_t seq{};
    XString title;
    XVector<std::uint32_t> values;
    XVector<VwItem> items;
    XMap<std::uint32_t, XString> names;
    XSet<std::uint32_t> keys;
    bool flag{};
};
XOFFSET_RECORD_DERIVED(VwRoot, (), seq, title, values, items, names, keys, flag)
XOFFSET_REGISTER_SCHEMA_NAME(VwRoot, "test.ReceiverViewRoot")

// Groups of items: an outer vector whose elements are vectors that need the walk.
struct VwGroups {
    XVector<XVector<VwItem>> groups;
};
XOFFSET_RECORD_DERIVED(VwGroups, (), groups)
XOFFSET_REGISTER_SCHEMA_NAME(VwGroups, "test.ReceiverViewGroups")

static_assert(default_view_check<XLink::server_peer> == XCheck::admit);
static_assert(default_view_check<XLink::from_server> == XCheck::admit);
static_assert(default_view_check<XLink::from_client> == XCheck::bounds);
static_assert(!detail::force_full_check);
// A view gives only const access.
static_assert(std::is_same_v<decltype(std::declval<XView<VwRoot>&>().root()), const VwRoot&>);
static_assert(std::is_same_v<decltype(std::declval<XView<VwRoot>&>().operator->()), const VwRoot*>);

namespace {
int failures = 0;
void need(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

constexpr std::size_t kMax = std::size_t(1) << 24;
auto cap() { return XBuffer::maximum_capacity(kMax).with_growth(XGrowPolicy::Automatic); }

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

XBuffer make_source(std::uint64_t seed, std::size_t scale) {
    Rng rng{seed};
    auto b = XBuffer::create<VwRoot>(4096, cap());
    auto& r = b.root<VwRoot>();
    r.seq = rng.next();
    r.title = std::string(rng.below(60), char('a' + rng.below(26))).c_str();
    r.flag = rng.below(2) != 0;
    const auto values = rng.below(std::uint32_t(scale) + 1);
    for (std::uint32_t i = 0; i < values; ++i) r.values.push_back(std::uint32_t(rng.next()));
    const auto items = rng.below(std::uint32_t(scale / 8) + 2);
    for (std::uint32_t i = 0; i < items; ++i) {
        r.items.emplace_back();
        auto& item = r.items.back();
        item.id = i;
        item.ready = rng.below(2) != 0;
        item.name = std::string(rng.below(40), char('A' + rng.below(26))).c_str();
        const auto marks = rng.below(20);
        for (std::uint32_t k = 0; k < marks; ++k) item.marks.push_back(std::uint16_t(rng.next()));
    }
    const auto names = rng.below(std::uint32_t(scale / 16) + 2);
    for (std::uint32_t k = 0; k < names; ++k)
        r.names.emplace(std::uint32_t(rng.next()), std::string(rng.below(50), 'n').c_str());
    const auto keys = rng.below(30);
    for (std::uint32_t k = 0; k < keys; ++k) r.keys.insert(std::uint32_t(rng.next()));
    if (r.items.size() > 2 && rng.below(2)) r.items.erase(r.items.begin() + 1);
    if (!r.names.empty() && rng.below(2)) r.names.erase(r.names.begin()->first);
    return b;
}

// Every field the root reaches, read through const access (what a server does
// with a client's message); the lookups exercise the set's and map's find.
std::string dump(const VwRoot& r) {
    std::string s = std::to_string(r.seq) + (r.flag ? "T" : "F") + std::string(r.title.view()) + "|" + r.title.c_str();
    for (auto v : r.values) s += std::to_string(v) + ",";
    for (const auto& item : r.items) {
        s += std::to_string(item.id) + (item.ready ? "R" : "-") + std::string(item.name.view()) + item.name.c_str() + "[";
        for (auto m : item.marks) s += std::to_string(m) + ",";
        s += "]";
    }
    for (const auto& kv : r.names) {
        s += std::to_string(kv.first) + "=" + std::string(kv.second.view()) + ";";
        s += r.names.find(kv.first) != r.names.end() ? "f" : "m";
    }
    for (auto k : r.keys) s += std::to_string(k) + (r.keys.contains(k) ? "+" : "?");
    return s;
}

std::string dump(const VwGroups& g) {
    std::string s;
    for (const auto& group : g.groups) {
        s += "{";
        for (const auto& item : group) s += std::to_string(item.id) + std::string(item.name.view()) + ",";
        s += "}";
    }
    return s;
}

// The wire in a heap block of exactly its size (8-aligned), so that a read
// past it is an AddressSanitizer error.
struct Exact {
    std::unique_ptr<std::byte[]> bytes;
    std::size_t size;
    explicit Exact(const std::string& w) : bytes(new std::byte[w.size()]), size(w.size()) {
        std::memcpy(bytes.get(), w.data(), w.size());
    }
    std::span<const std::byte> span() const { return {bytes.get(), size}; }
};

XWireHeaderV2 header_of(const std::string& w) {
    XWireHeaderV2 h;
    std::memcpy(&h, w.data(), sizeof(h));
    return h;
}
// Wire position of payload offset `off` (dense wire).
std::size_t at(std::uint64_t off) { return sizeof(XWireHeaderV2) + std::size_t(off - XWIRE_PAYLOAD_BEGIN); }
template <class V> void put(std::string& w, std::size_t pos, V v) { std::memcpy(w.data() + pos, &v, sizeof(v)); }
template <class V> V get(const std::string& w, std::size_t pos) {
    V v;
    std::memcpy(&v, w.data() + pos, sizeof(v));
    return v;
}
// Payload offset of `field` in buffer `b`'s root.
template <class B, class F> std::uint64_t offset_in(const B& b, const F& field) {
    return std::uint64_t(reinterpret_cast<const char*>(&field) - reinterpret_cast<const char*>(b.data()));
}

void view_matches_full() {
    XReceiver<XLink::from_client> client(cap());
    XReceiver<XLink::server_peer> peer(cap());
    auto verified = XBuffer::create<VwRoot>(4096, cap());
    std::size_t cases = 0;
    for (std::uint64_t seed = 1; seed <= 300; ++seed) {
        const std::size_t scale = seed % 7 == 0 ? 20000 : seed % 3 == 0 ? 600 : 24;
        auto src = make_source(seed, scale);
        for (const auto& wire : {src.save_verified<VwRoot>(XSaveOptions{XWireEmission::dense}),
                                 src.save_compact<VwRoot>()}) {
            ++cases;
            verified.reload_verified<VwRoot>(wire);
            const auto expect = dump(verified.root<VwRoot>());
            const Exact exact(wire);
            auto v = client.view<VwRoot>(exact.span());
            need(v.has_value(), "bounds view of a valid wire");
            if (v) {
                need(v->check() == XCheck::bounds, "bounds view reports its check");
                need(dump(v->root()) == expect, "bounds view reads differ from reload_verified");
                need(v->wire().data() == exact.span().data(), "the view reads the wire in place");
            }
            auto a = peer.view<VwRoot>(exact.span());
            need(a.has_value() && a->check() == XCheck::admit, "admit view of a valid wire");
            if (a) need(dump(**a) == expect, "admit view reads differ from reload_verified");
            auto b = peer.view<VwRoot, XCheck::bounds>(wire);
            need(b.has_value(), "bounds view on a trusted link, from a std::string");
        }
    }
    need(cases == 600, "differential case count");
}

void rejects_framing() {
    XReceiver<XLink::from_client> rx(cap());
    auto src = make_source(7, 200);
    const auto wire = src.save_verified<VwRoot>(XSaveOptions{XWireEmission::dense});
    auto rejects = [&](const std::string& w, XErrc code, const char* what) {
        const Exact exact(w);
        auto v = rx.view<VwRoot>(exact.span());
        need(!v && v.code() == code, what);
        // The full check gives the same code where it rejects at the header.
        auto full = XBuffer::try_load_verified<VwRoot>(exact.span(), cap());
        if (code != XErrc::invalid_argument) need(!full && full.code() == code, "the full check agrees");
    };
    rejects(wire.substr(0, 20), XErrc::truncated, "truncated header");
    std::string bad = wire;
    bad[0] = 'Y';
    rejects(bad, XErrc::bad_magic, "bad magic");
    bad = wire;
    bad[3] = char(bad[3] + 1);
    rejects(bad, XErrc::unsupported_version, "other version");
    bad = wire;
    put<std::uint32_t>(bad, 4, 1);
    rejects(bad, XErrc::unsupported_flags, "flags");
    bad = wire;
    put<std::uint64_t>(bad, 24, get<std::uint64_t>(wire, 24) ^ 1);
    rejects(bad, XErrc::schema_mismatch, "schema hash");
    bad = wire;
    put<std::uint64_t>(bad, 32, get<std::uint64_t>(wire, 32) ^ 1);
    rejects(bad, XErrc::root_type_mismatch, "root type");
    bad = wire;
    put<std::uint32_t>(bad, 12, header_of(wire).used_bytes);
    rejects(bad, XErrc::root_offset_out_of_bounds, "root offset at used");
    bad = wire;
    bad.pop_back();
    rejects(bad, XErrc::size_mismatch, "short body");
    bad = wire + std::string(8, '\0');
    rejects(bad, XErrc::size_mismatch, "long body");
    // Over the receiver's maximum capacity (the full check with the same limit agrees).
    {
        XReceiver<XLink::from_client> small(XBuffer::maximum_capacity(256));
        const Exact exact(wire);
        auto v = small.view<VwRoot>(exact.span());
        need(!v && v.code() == XErrc::capacity_exceeded, "used_bytes over the maximum capacity");
    }
    // A sparse wire: valid, but a view needs the payload in one piece.
    {
        auto big = make_source(3, 200);
        auto& r = big.root<VwRoot>();
        for (int i = 0; i < 4000; ++i) r.values.push_back(std::uint32_t(i));
        r.values.clear();
        const auto sparse = big.save_verified<VwRoot>(XSaveOptions{XWireEmission::zero_runs});
        need(header_of(sparse).extent_count != 0, "test wire is sparse");
        const Exact exact(sparse);
        auto v = rx.view<VwRoot>(exact.span());
        need(!v && v.code() == XErrc::invalid_argument, "sparse wire refused");
        auto dense = rx.view<VwRoot>(big.save_verified<VwRoot>(XSaveOptions{XWireEmission::dense}));
        need(dense.has_value(), "its dense form is viewed");
    }
    // Misaligned bytes.
    {
        std::vector<std::uint64_t> words(wire.size() / 8 + 2);
        auto* p = reinterpret_cast<std::byte*>(words.data()) + 4;
        std::memcpy(p, wire.data(), wire.size());
        auto v = rx.view<VwRoot>(std::span<const std::byte>(p, wire.size()));
        need(!v && v.code() == XErrc::invalid_argument, "misaligned wire refused");
    }
}

// Each payload defect the walk must catch: the bounds view rejects with
// invalid_payload, and so does the full check.
void rejects_payload() {
    XReceiver<XLink::from_client> rx(cap());
    auto src = XBuffer::create<VwRoot>(4096, cap());
    auto& r = src.root<VwRoot>();
    r.title = std::string(40, 't').c_str();                 // heap string
    for (std::uint32_t i = 0; i < 16; ++i) r.values.push_back(i);
    r.items.emplace_back();
    r.items.back().name = "short";                            // inline string
    r.items.back().marks.push_back(7);
    r.items.emplace_back();
    r.items.back().name = std::string(30, 'h').c_str();
    const auto wire = src.save_verified<VwRoot>(XSaveOptions{XWireEmission::dense});
    const auto used = header_of(wire).used_bytes;
    const auto title = at(offset_in(src, r.title));
    const auto values = at(offset_in(src, r.values));
    const auto item0 = at(offset_in(src, r.items[0]));
    const auto name0 = at(offset_in(src, r.items[0].name));
    const auto flag = at(offset_in(src, r.flag));
    const auto ready0 = at(offset_in(src, r.items[0].ready));
    (void)item0;

    auto rejects = [&](const std::string& w, const char* what) {
        const Exact exact(w);
        auto v = rx.view<VwRoot>(exact.span());
        need(!v && v.code() == XErrc::invalid_payload, what);
        need(!XBuffer::try_load_verified<VwRoot>(exact.span(), cap()), "the full check rejects it too");
    };
    std::string bad;
    // A vector's block past the payload, before it, in the payload header, misaligned.
    const auto values_delta = get<std::int32_t>(wire, values);
    bad = wire; put<std::int32_t>(bad, values, std::int32_t(used));       rejects(bad, "vector block past the payload");
    bad = wire; put<std::int32_t>(bad, values, -0x40000000);               rejects(bad, "vector block before the payload");
    bad = wire;
    put<std::int32_t>(bad, values, std::int32_t(8 - std::int64_t(offset_in(src, r.values))));
    rejects(bad, "vector block in the payload header");
    bad = wire; put<std::int32_t>(bad, values, values_delta + 4);         rejects(bad, "misaligned vector block");
    bad = wire; put<std::uint32_t>(bad, values + 4, 17);                   rejects(bad, "vector size over capacity");
    bad = wire;
    put<std::uint32_t>(bad, values + 8, 0x10000000u);
    put<std::uint32_t>(bad, values + 4, 0x10000000u);
    rejects(bad, "vector capacity past the payload");
    bad = wire; put<std::int32_t>(bad, values, 0);                         rejects(bad, "vector with a size and no block");
    // Strings: a heap string's block, size and terminator; an inline length.
    bad = wire; put<std::int32_t>(bad, title, std::int32_t(used));        rejects(bad, "string block past the payload");
    bad = wire; put<std::uint32_t>(bad, title + 4, 41);                    rejects(bad, "string size over capacity");
    {
        bad = wire;
        const auto block = std::int64_t(offset_in(src, r.title)) + get<std::int32_t>(wire, title);
        bad[at(std::uint64_t(block)) + 40] = 'x';  // the terminator after 40 characters
        rejects(bad, "string missing terminator");
    }
    bad = wire;
    bad[name0 + 11] = char(0x80 | 11);
    rejects(bad, "inline string of 11 bytes");
    bad = wire;
    bad[name0 + 11] = char(0xff);
    rejects(bad, "inline string of 127 bytes");
    // Bools anywhere the walk reaches.
    bad = wire; bad[flag] = 2;   rejects(bad, "root bool 2");
    bad = wire; bad[ready0] = char(0x80); rejects(bad, "element bool 0x80");
    // The untouched wire is viewed.
    const Exact exact(wire);
    need(rx.view<VwRoot>(exact.span()).has_value(), "the untouched wire");
}

// Overlapping blocks: the bounds view does not look for overlap, but the
// work is bounded (at most the payload's bytes walked), and a small overlap
// reads in bounds.
void overlap_bounded() {
    XReceiver<XLink::from_client> rx(cap());
    constexpr std::uint32_t kGroups = 256, kItems = 128;
    auto src = XBuffer::create<VwGroups>(4096, cap());
    auto& g = src.root<VwGroups>();
    for (std::uint32_t i = 0; i < kGroups; ++i) g.groups.emplace_back();
    for (std::uint32_t k = 0; k < kItems; ++k) {
        g.groups[0].emplace_back();
        g.groups[0].back().id = k;
        g.groups[0].back().name = std::string(k % 20, 'g').c_str();
    }
    const auto wire = src.save_verified<VwGroups>(XSaveOptions{XWireEmission::dense});
    const auto head0 = offset_in(src, g.groups[0]);
    const auto block0 = std::int64_t(head0) + get<std::int32_t>(wire, at(head0));
    // Every group names group 0's items.
    auto aim = [&](std::string& w, std::uint32_t count, std::uint32_t items) {
        for (std::uint32_t i = 1; i < count; ++i) {
            const auto head = offset_in(src, g.groups[i]);
            put<std::int32_t>(w, at(head), std::int32_t(block0 - std::int64_t(head)));
            put<std::uint32_t>(w, at(head) + 4, items);
            put<std::uint32_t>(w, at(head) + 8, get<std::uint32_t>(wire, at(head0) + 8));
        }
    };
    std::string all = wire;
    aim(all, kGroups, kItems);
    const Exact amplified(all);
    need(std::uint64_t(kGroups) * kItems * sizeof(VwItem) > header_of(all).used_bytes, "the walk would exceed the payload");
    auto v = rx.view<VwGroups>(amplified.span());
    need(!v && v.code() == XErrc::invalid_payload, "amplification refused");
    std::string two = wire;
    aim(two, 2, 8);  // a second group naming the first 8 items
    const Exact shared(two);
    need(!XBuffer::try_load_verified<VwGroups>(shared.span(), cap()), "the full check refuses the overlap");
    auto w = rx.view<VwGroups>(shared.span());
    need(w.has_value(), "a small overlap is viewed (read-only, in bounds)");
    if (w) need(dump(**w).size() > 0, "reading the overlap");
}

// Inputs the full check refuses and the bounds view may accept are read in
// bounds (AddressSanitizer): random byte, word and delta mutations of valid wires.
void mutations_read_in_bounds() {
    XReceiver<XLink::from_client> rx(cap());
    std::size_t viewed = 0, refused = 0, full_refused_viewed = 0;
    for (std::uint64_t seed = 1; seed <= 120; ++seed) {
        auto src = make_source(seed, seed % 4 == 0 ? 400 : 24);
        const auto wire = src.save_verified<VwRoot>(XSaveOptions{XWireEmission::dense});
        Rng rng{seed * 977};
        for (int m = 0; m < 60; ++m) {
            std::string w = wire;
            const auto body = w.size() - sizeof(XWireHeaderV2);
            const int edits = 1 + int(rng.below(3));
            for (int e = 0; e < edits; ++e) {
                const auto pos = sizeof(XWireHeaderV2) + (body ? rng.below(std::uint32_t(body)) : 0);
                switch (rng.below(4)) {
                case 0: w[pos] = char(rng.next()); break;
                case 1: if (pos + 4 <= w.size()) put<std::uint32_t>(w, pos & ~std::size_t(3), std::uint32_t(rng.next())); break;
                case 2: if (pos + 4 <= w.size()) put<std::int32_t>(w, pos & ~std::size_t(3), std::int32_t(rng.below(4096)) - 2048); break;
                default: if (pos + 4 <= w.size()) put<std::uint32_t>(w, pos & ~std::size_t(3), rng.below(64)); break;
                }
            }
            const Exact exact(w);
            auto v = rx.view<VwRoot>(exact.span());
            if (!v) { ++refused; continue; }
            ++viewed;
            (void)dump(v->root());  // every const access, in bounds
            if (!XBuffer::try_load_verified<VwRoot>(exact.span(), cap())) ++full_refused_viewed;
        }
    }
    need(viewed > 0 && refused > 0, "mutations both viewed and refused");
    std::printf("mutations: %zu viewed (%zu of them refused by the full check), %zu refused\n", viewed,
                full_refused_viewed, refused);
}
} // namespace

int main() {
    view_matches_full();
    rejects_framing();
    rejects_payload();
    overlap_bounded();
    mutations_read_in_bounds();
    if (failures) {
        std::fprintf(stderr, "test_receiver_view: %d failures\n", failures);
        return 1;
    }
    std::puts("test_receiver_view: ok");
    return 0;
}
