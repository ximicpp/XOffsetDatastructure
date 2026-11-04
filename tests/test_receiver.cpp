// Trust tiers by link type (xoffset_receiver.hpp), phase P1: XReceiver
// reload / load with XCheck::admit against the full check.
//
// Built with XOFFSET_FORCE_FULL_CHECK=0 so that admit is the real admit path
// even though test assertions undefine NDEBUG (test_receiver_forced covers
// the forced full check). No exception of its own: also built as
// test_receiver_noexc.
#define XOFFSET_FORCE_FULL_CHECK 0
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <vector>
#include "../xoffset_receiver.hpp"
using namespace XOffsetDatastructure;

struct RxItem {
    std::uint32_t id{};
    XString name;
    XVector<std::uint16_t> marks;
};
XOFFSET_RECORD_DERIVED(RxItem, (), id, name, marks)

struct RxRoot {
    std::uint64_t seq{};
    XString title;
    XVector<std::uint32_t> values;
    XVector<RxItem> items;
    XMap<std::uint32_t, XString> names;
    bool flag{};
};
XOFFSET_RECORD_DERIVED(RxRoot, (), seq, title, values, items, names, flag)
XOFFSET_REGISTER_SCHEMA_NAME(RxRoot, "test.ReceiverRoot")

struct RxOther { std::uint32_t x{}; };
XOFFSET_RECORD_DERIVED(RxOther, (), x)
XOFFSET_REGISTER_SCHEMA_NAME(RxOther, "test.ReceiverOther")

static_assert(default_mutable_check<XLink::server_peer> == XCheck::admit);
static_assert(default_mutable_check<XLink::from_server> == XCheck::admit);
static_assert(default_mutable_check<XLink::from_client> == XCheck::full);
static_assert(!detail::force_full_check);

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

// A random root with strings (inline and heap), vectors, nested records and
// a map, plus holes in the free table (erased elements, retired blocks).
XBuffer make_source(std::uint64_t seed, std::size_t scale) {
    Rng rng{seed};
    auto b = XBuffer::create<RxRoot>(4096, cap());
    auto& r = b.root<RxRoot>();
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
        item.name = std::string(rng.below(40), char('A' + rng.below(26))).c_str();
        const auto marks = rng.below(20);
        for (std::uint32_t k = 0; k < marks; ++k) item.marks.push_back(std::uint16_t(rng.next()));
    }
    const auto names = rng.below(std::uint32_t(scale / 16) + 2);
    for (std::uint32_t k = 0; k < names; ++k)
        r.names.emplace(std::uint32_t(rng.next()), std::string(rng.below(50), 'n').c_str());
    if (r.items.size() > 2 && rng.below(2)) r.items.erase(r.items.begin() + 1);
    if (!r.names.empty() && rng.below(2)) r.names.erase(r.names.begin()->first);
    const auto retired = rng.below(6);
    for (std::uint32_t i = 0; i < retired; ++i) {
        const auto bytes = std::uint32_t(8 * (1 + rng.below(800)));
        auto* p = b.arena()->allocate_aligned(bytes, 8);
        std::memset(p, 0xab, bytes);
        b.arena()->deallocate(p, bytes);
    }
    return b;
}

std::size_t used(const XBufferCore& b) { return static_cast<std::size_t>(b.arena()->used_end); }

bool used_bytes_equal(const XBufferCore& a, const XBufferCore& b) {
    return used(a) == used(b) && std::memcmp(a.data(), b.data(), used(a)) == 0;
}

std::vector<std::uint64_t> free_blocks(const XBufferCore& b) {
    std::vector<std::uint64_t> out;
    b.for_each_free_block([&](XBufferCore::WireHole h) { out.push_back(std::uint64_t(h.offset) << 32 | h.bytes); });
    return out;
}

bool same_free_table(const XBufferCore& a, const XBufferCore& b) {
    return a.free_table_complete() == b.free_table_complete() && a.free_block_count() == b.free_block_count() &&
           a.free_block_bytes() == b.free_block_bytes() && free_blocks(a) == free_blocks(b);
}

std::uint64_t checksum(XBuffer& b) {
    auto& r = b.root<RxRoot>();
    std::uint64_t h = r.seq ^ (r.flag ? 1 : 0) ^ r.title.size();
    for (auto v : r.values) h = h * 31 + v;
    for (auto& item : r.items) {
        h = h * 31 + item.id + item.name.size();
        for (auto m : item.marks) h = h * 31 + m;
    }
    for (auto& [k, v] : r.names) h = h * 31 + k + v.size();
    return h;
}

// The same edits on two buffers that must stay byte-identical: the free
// table admit loaded must place allocations as the verified one does.
void edit(XBuffer& b, std::uint32_t salt) {
    auto& r = b.root<RxRoot>();
    r.values.push_back(salt);
    r.items.emplace_back();
    r.items.back().name = std::string(33, 'q').c_str();
    r.items.back().marks.push_back(std::uint16_t(salt));
    r.names.emplace(0xfeed0000u + salt, std::string(70, 'z').c_str());
    r.title = std::string(90, char('m' + salt % 8)).c_str();
}

std::span<const std::byte> bytes_of(const std::string& s) {
    return {reinterpret_cast<const std::byte*>(s.data()), s.size()};
}

// Differential: on every wire the full check accepts, admit gives the same
// payload bytes, free table, typed root, re-save and behaviour under edits.
void admit_matches_full() {
    XReceiver<XLink::server_peer> rx(cap());
    const XWireEmission emissions[] = {XWireEmission::dense, XWireEmission::free_runs, XWireEmission::zero_runs};
    auto admitted = XBuffer::create<RxRoot>(4096, cap());
    auto verified = XBuffer::create<RxRoot>(4096, cap());
    std::size_t cases = 0;
    for (std::uint64_t seed = 1; seed <= 300; ++seed) {
        const std::size_t scale = seed % 7 == 0 ? 20000 : seed % 3 == 0 ? 600 : 24;
        auto src = make_source(seed, scale);
        std::vector<std::string> wires;
        for (auto e : emissions) wires.push_back(src.save_verified<RxRoot>(XSaveOptions{e}));
        wires.push_back(src.save_compact<RxRoot>());
        for (const auto& wire : wires) {
            ++cases;
            need(rx.reload<RxRoot>(admitted, wire).has_value(), "admit reload of a valid wire");
            verified.reload_verified<RxRoot>(wire);
            need(used_bytes_equal(admitted, verified), "admit payload differs from verified");
            need(same_free_table(admitted, verified), "admit free table differs from verified");
            need(checksum(admitted) == checksum(verified), "admit root reads differ");
            need(admitted.save_verified<RxRoot>() == verified.save_verified<RxRoot>(), "admit re-save differs");
            auto loaded = rx.load<RxRoot>(wire);
            need(loaded.has_value(), "admit load of a valid wire");
            if (loaded) {
                need(used_bytes_equal(*loaded, verified), "admit load payload differs");
                need(same_free_table(*loaded, verified), "admit load free table differs");
            }
            if (seed % 5 == 0) {
                edit(admitted, std::uint32_t(seed));
                edit(verified, std::uint32_t(seed));
                need(used_bytes_equal(admitted, verified), "edits after admit diverge");
                need(admitted.save_verified<RxRoot>() == verified.save_verified<RxRoot>(), "re-save after edits");
            }
        }
    }
    need(cases == 1200, "differential case count");
}

// Admit keeps every framing check: the same rejections as the full check,
// and the destination is left empty without a root.
void admit_rejects_framing() {
    XReceiver<XLink::from_server> rx(cap());
    auto src = make_source(7, 200);
    const auto wire = src.save_verified<RxRoot>(XSaveOptions{XWireEmission::dense});
    auto dst = XBuffer::create<RxRoot>(4096, cap());
    auto rejects = [&](const std::string& w, XErrc code, const char* what) {
        auto st = rx.reload<RxRoot>(dst, w);
        need(!st && st.code() == code, what);
        need(!dst.has_root<RxRoot>(), "rejected admit left a root");
        auto ld = rx.load<RxRoot>(w);
        need(!ld && ld.code() == code, what);
        need(rx.reload<RxRoot>(dst, wire).has_value(), "admit after a rejection");
    };
    std::string w = wire;
    w[0] ^= 1;
    rejects(w, XErrc::bad_magic, "bad magic");
    rejects(wire.substr(0, 20), XErrc::truncated, "truncated header");
    rejects(wire.substr(0, wire.size() - 8), XErrc::size_mismatch, "short body");
    rejects(wire + std::string(8, '\0'), XErrc::size_mismatch, "long body");
    // Another root type: fingerprint mismatch (rolling deploys with mixed versions).
    auto other = XBuffer::create<RxOther>(4096, cap());
    other.root<RxOther>().x = 5;
    rejects(other.save_verified<RxOther>(), XErrc::root_type_mismatch, "other root type");

    XWireHeaderV2 header;
    std::memcpy(&header, wire.data(), sizeof(header));
    auto with_header = [&](auto patch) {
        XWireHeaderV2 h = header;
        patch(h);
        std::string out = wire;
        std::memcpy(out.data(), &h, sizeof(h));
        return out;
    };
    rejects(with_header([](XWireHeaderV2& h) { h.schema_hash ^= 1; }), XErrc::schema_mismatch, "schema hash");
    {
        auto st = rx.reload<RxRoot>(dst, with_header([&](XWireHeaderV2& h) { h.root_offset = h.used_bytes + 64; }));
        need(!st, "root offset past used");
        need(!dst.has_root<RxRoot>(), "rejected root offset left a root");
    }
    {
        // The root object must lie inside the used payload.
        auto st = rx.reload<RxRoot>(dst, with_header([&](XWireHeaderV2& h) { h.root_offset = h.used_bytes - 8; }));
        need(!st, "root object past used");
        need(!dst.has_root<RxRoot>(), "rejected root object left a root");
    }
    // A wire larger than the receiver's limit: load refuses before reserving.
    XReceiver<XLink::server_peer> tight(XBuffer::maximum_capacity(4096));
    auto big = make_source(9, 20000);
    auto st = tight.load<RxRoot>(big.save_verified<RxRoot>());
    need(!st && st.code() == XErrc::capacity_exceeded, "load over the receiver's maximum capacity");
    // reload is bounded by the destination's own maximum capacity.
    auto small = XBuffer::create<RxRoot>(4096, XBuffer::maximum_capacity(8192));
    auto st2 = rx.reload<RxRoot>(small, big.save_verified<RxRoot>());
    need(!st2, "reload over the destination's maximum capacity");
}

// Holes go into the free table and allocations write into them, so admit
// checks each hole lies inside the payload.
void admit_rejects_bad_holes() {
    XReceiver<XLink::server_peer> rx(cap());
    auto src = make_source(11, 300);
    for (std::uint32_t bytes : {64u, 136u, 6000u}) {
        auto* p = src.arena()->allocate_aligned(bytes, 8);
        src.arena()->deallocate(p, bytes);
    }
    const auto wire = src.save_verified<RxRoot>(XSaveOptions{XWireEmission::dense});
    XWireHeaderV2 header;
    std::memcpy(&header, wire.data(), sizeof(header));
    need(header.hole_count > 0, "source has holes");
    const std::size_t last = wire.size() - 8;
    auto with_hole = [&](std::uint32_t offset, std::uint32_t bytes) {
        std::string out = wire;
        XBufferCore::WireHole h{offset, bytes};
        std::memcpy(out.data() + last, &h, sizeof(h));
        return out;
    };
    auto dst = XBuffer::create<RxRoot>(4096, cap());
    const std::uint32_t used_bytes = header.used_bytes;
    const struct { std::uint32_t offset, bytes; const char* what; } bad[] = {
        {used_bytes, 8, "hole at used"},
        {used_bytes - 8, 16, "hole crossing used"},
        {0, 8, "hole in the payload header"},
        {64, 0, "empty hole"},
        {68, 8, "unaligned hole offset"},
        {64, 12, "unaligned hole size"},
        {0xfffffff8u, 16, "hole offset overflow"},
    };
    for (const auto& b : bad) {
        auto st = rx.reload<RxRoot>(dst, with_hole(b.offset, b.bytes));
        need(!st && st.code() == XErrc::invalid_payload, b.what);
        need(!dst.has_root<RxRoot>(), "rejected hole left a root");
    }
    need(rx.reload<RxRoot>(dst, wire).has_value(), "admit after hole rejections");
}

// What admit does not check: the object graph. A container head pointing
// outside the payload passes admit and is the sender's bug to own; the full
// check rejects it. (The admitted buffer is not read.)
void admit_skips_graph() {
    auto src = XBuffer::create<RxRoot>(4096, cap());
    auto& r = src.root<RxRoot>();
    for (std::uint32_t i = 0; i < 40; ++i) r.values.push_back(i);
    const auto head = std::size_t(reinterpret_cast<const char*>(&r.values) - reinterpret_cast<const char*>(&r));
    const auto wire = src.save_verified<RxRoot>(XSaveOptions{XWireEmission::dense});
    XWireHeaderV2 header;
    std::memcpy(&header, wire.data(), sizeof(header));
    std::string bad = wire;
    // The vector head's rel32 to its block: point it far outside the payload.
    const std::size_t at = sizeof(XWireHeaderV2) + (header.root_offset - XWIRE_PAYLOAD_BEGIN) + head;
    const std::int32_t far = 0x40000000;
    std::memcpy(bad.data() + at, &far, sizeof(far));

    XReceiver<XLink::server_peer> peer(cap());
    XReceiver<XLink::from_client> client(cap());
    auto dst = XBuffer::create<RxRoot>(4096, cap());
    need(peer.reload<RxRoot>(dst, bad).has_value(), "admit walks the graph");
    need(!peer.reload<RxRoot, XCheck::full>(dst, bad), "explicit full on a trusted link accepted a bad graph");
    need(!client.reload<RxRoot>(dst, bad), "client uplink (full) accepted a bad graph");
    need(!client.load<RxRoot>(bad), "client uplink load (full) accepted a bad graph");
    need(!XBuffer::try_load_verified<RxRoot>(bytes_of(bad), cap()), "load_verified accepted a bad graph");
}

// Typed buffers and typed loads; the client uplink defaults to full.
void typed_and_links() {
    auto src = make_source(21, 500);
    const auto wire = src.save_verified<RxRoot>();
    XReceiver<XLink::from_server> rx(cap());
    auto typed = TypedXBuffer<RxRoot>::create(4096, cap());
    need(rx.reload(typed, wire).has_value(), "typed admit reload");
    need(rx.reload<XCheck::full>(typed, bytes_of(wire)).has_value(), "typed full reload");
    auto expect = XBuffer::load_verified<RxRoot>(wire, cap());
    need(used_bytes_equal(typed, expect), "typed admit payload");
    auto loaded = rx.load_typed<RxRoot>(wire);
    need(loaded.has_value() && checksum(*loaded) == checksum(expect), "load_typed admit");
    // Mutate in place after admit, as after a verified load.
    if (loaded) {
        loaded->root().values.push_back(99);
        auto back = XBuffer::load_verified<RxRoot>(loaded->save_verified(), cap());
        need(back.root<RxRoot>().values.back() == 99, "mutate after admit");
    }
    XReceiver<XLink::from_client> client(cap());
    auto dst = XBuffer::create<RxRoot>(4096, cap());
    need(client.reload<RxRoot>(dst, wire).has_value(), "client uplink full reload");
    need(used_bytes_equal(dst, expect), "client uplink payload");
    need(client.maximum_capacity().value == cap().value, "receiver capacity");
}

// Reloading the same trusted wire into a warm buffer allocates nothing and
// keeps the mapping, like reload_verified.
void reload_reuses_buffer() {
    XReceiver<XLink::server_peer> rx(cap());
    auto src = make_source(31, 3000);
    const auto wire = src.save_verified<RxRoot>(XSaveOptions{XWireEmission::dense});
    auto dst = XBuffer::create<RxRoot>(4096, cap());
    need(rx.reload<RxRoot>(dst, wire).has_value(), "first reload");
    const auto* address = dst.get_address();
    const auto epoch = dst.epoch();
    need(rx.reload<RxRoot>(dst, wire).has_value(), "second reload");
    need(dst.get_address() == address, "admit reload changed the payload address");
    need(dst.epoch() != epoch, "admit reload did not invalidate typed references");
}
} // namespace

int main() {
    admit_matches_full();
    admit_rejects_framing();
    admit_rejects_bad_holes();
    admit_skips_graph();
    typed_and_links();
    reload_reuses_buffer();
    if (failures) {
        std::fprintf(stderr, "test_receiver: %d failure(s)\n", failures);
        return 1;
    }
    std::puts("test_receiver: ok");
    return 0;
}
