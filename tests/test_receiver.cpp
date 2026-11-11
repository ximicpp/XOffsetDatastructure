// Trust tiers by link type (xoffset_receiver.hpp), phases P1 and P2:
// XReceiver reload / load / apply_delta with XCheck::admit against the full
// check.
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
    const std::int32_t outside = 0x40000000;  // not "far": a macro on Windows
    std::memcpy(bad.data() + at, &outside, sizeof(outside));

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
        loaded->plain_root().values.push_back(99);
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
// ---- P2: deltas -----------------------------------------------------------

std::string varint(std::uint32_t v) {
    std::string out;
    for (; v >= 0x80; v >>= 7) out += char(0x80 | (v & 0x7F));
    return out + char(v);
}

// The payload past the arena header (what a delta carries) and its extent.
bool payload_equal(const XBufferCore& a, const XBufferCore& b) {
    return used(a) == used(b) && a.arena()->root_offset == b.arena()->root_offset &&
           std::memcmp(a.data() + XWIRE_PAYLOAD_BEGIN, b.data() + XWIRE_PAYLOAD_BEGIN,
                       used(a) - XWIRE_PAYLOAD_BEGIN) == 0;
}

// The changes a server makes between deltas: scalars, strings across the
// inline and heap forms, vector growth and erasure, map inserts and erases,
// and now and then a reset that shrinks the payload.
void mutate(XBuffer& b, Rng& rng) {
    auto& r = b.root<RxRoot>();
    switch (rng.below(9)) {
    case 0: r.seq = rng.next(); break;
    case 1: r.flag = !r.flag; break;
    case 2:
        if (!r.values.empty()) r.values[rng.below(std::uint32_t(r.values.size()))] = std::uint32_t(rng.next());
        break;
    case 3: r.title = std::string(rng.below(3) ? rng.below(10) : 20 + rng.below(80), char('a' + rng.below(26))).c_str(); break;
    case 4: edit(b, std::uint32_t(rng.next())); break;
    case 5:
        if (!r.items.empty()) r.items.erase(r.items.begin() + std::ptrdiff_t(rng.below(std::uint32_t(r.items.size()))));
        break;
    case 6:
        if (!r.names.empty()) r.names.erase(r.names.begin()->first);
        break;
    case 7:
        if (!r.items.empty()) {
            auto& item = r.items[rng.below(std::uint32_t(r.items.size()))];
            if (rng.below(2) || item.marks.empty()) item.marks.push_back(std::uint16_t(rng.next()));
            else item.marks.pop_back();
            item.name = std::string(rng.below(30), 'k').c_str();
        }
        break;
    default:
        if (rng.below(6) == 0) {
            b.reset<RxRoot>();
            b.root<RxRoot>().seq = rng.next();
        } else if (!r.values.empty()) {
            r.values.pop_back();
        }
        break;
    }
}

// Differential: on every delta of random streams, admit leaves the same
// payload, free table, root and cursor as apply_delta_verified, with and
// without a digest, for key deltas, late joins and a full apply now and then
// on the admitted replica (which then verifies the whole payload).
void delta_admit_matches_full() {
    XReceiver<XLink::from_server> rx(cap());
    std::size_t deltas = 0, keys = 0, fulls = 0, joins = 0;
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
        Rng rng{seed * 7919};
        auto src = make_source(seed, seed % 4 == 0 ? 3000 : 200);
        XDeltaBaseline base(seed * 2 + 1);
        auto admitted = XBuffer::create<RxRoot>(4096, cap());
        auto verified = XBuffer::create<RxRoot>(4096, cap());
        auto late = XBuffer::create<RxRoot>(4096, cap());
        XDeltaCursor ca, cv, cl;
        bool joined = false;
        for (int step = 0; step < 60; ++step) {
            if (step > 0) {
                if (rng.below(25) == 0) base.reset();  // the receiver asked for a resync
                const auto changes = 1 + rng.below(3);
                for (std::uint32_t k = 0; k < changes; ++k) mutate(src, rng);
            }
            XDeltaOptions options;
            options.digest = rng.below(2) != 0;
            options.whole_blocks = rng.below(4) == 0;
            std::string d;
            const auto stats = src.save_delta_into<RxRoot>(d, base, options);
            if (stats.empty) continue;
            ++deltas;
            keys += stats.key;
            const bool full = !stats.key && step % 9 == 4;
            const XStatus a = full ? rx.apply_delta<RxRoot, XCheck::full>(admitted, d, ca)
                                   : rx.apply_delta<RxRoot>(admitted, d, ca);
            need(a.has_value(), "admit rejected a valid delta");
            need(verified.try_apply_delta_verified<RxRoot>(d, cv).has_value(), "verified rejected a valid delta");
            if (full) {
                // The first full apply after admits verifies the whole payload.
                need(ca.last_check() == XDeltaCheck::full, "full after admit did not verify in full");
                ++fulls;
            } else {
                need(ca.last_check() == XDeltaCheck::admit, "admit not reported as admit");
            }
            need(used_bytes_equal(admitted, verified), "admitted payload differs");
            need(payload_equal(admitted, src), "admitted payload differs from the sender");
            need(same_free_table(admitted, verified), "admitted free table differs");
            need(checksum(admitted) == checksum(verified), "admitted root reads differ");
            need(ca.version() == cv.version() && ca.stream() == cv.stream(), "admit cursor does not follow");
            if (joined) {
                need(rx.apply_delta<RxRoot>(late, d, cl).has_value(), "late admit rejected a shared delta");
                need(payload_equal(late, src), "late admitted payload differs");
            } else if (rng.below(6) == 0) {
                need(rx.apply_delta<RxRoot>(late, base.key_delta(options), cl).has_value(), "late key admit");
                need(payload_equal(late, src) && cl.last_check() == XDeltaCheck::admit, "late join");
                joined = true;
                ++joins;
            }
        }
        // The admitted replica is a buffer like any other: it re-saves and verifies.
        need(admitted.save_verified<RxRoot>() == verified.save_verified<RxRoot>(), "admitted re-save differs");
        need(XBuffer::try_load_verified<RxRoot>(bytes_of(admitted.save_verified<RxRoot>()), cap()).has_value(),
             "admitted replica does not verify");
    }
    need(deltas > 1500 && keys > 40 && fulls > 100 && joins > 10, "delta differential coverage");
}

// Admit keeps the delta's framing checks; a rejected delta leaves the replica
// and the cursor as they were (a rejected key delta empties the replica).
void delta_admit_rejects() {
    XReceiver<XLink::server_peer> rx(cap());
    auto src = make_source(41, 400);
    XDeltaBaseline base(77);
    const std::string key = src.save_delta<RxRoot>(base);
    src.root<RxRoot>().seq ^= 1;
    const std::string d1 = src.save_delta<RxRoot>(base);
    src.root<RxRoot>().values.push_back(5);
    const std::string d2 = src.save_delta<RxRoot>(base);
    auto dst = XBuffer::create<RxRoot>(4096, cap());
    XDeltaCursor cur;

    auto st = rx.apply_delta<RxRoot>(dst, d1, cur);
    need(!st && st.code() == XErrc::delta_base_mismatch, "admit applied a delta before its key delta");
    auto other = XBuffer::create<RxOther>(4096, cap());
    other.root<RxOther>().x = 5;
    XDeltaBaseline other_base(78);
    st = rx.apply_delta<RxRoot>(dst, other.save_delta<RxOther>(other_base), cur);
    need(!st && st.code() == XErrc::root_type_mismatch, "admit took another root type's key delta");
    need(rx.apply_delta<RxRoot>(dst, key, cur).has_value(), "admit key delta");

    const std::vector<std::byte> held(reinterpret_cast<const std::byte*>(dst.data()),
                                      reinterpret_cast<const std::byte*>(dst.data()) + used(dst));
    auto unchanged = [&](const char* what) {
        need(used(dst) == held.size() && std::memcmp(dst.data(), held.data(), held.size()) == 0, what);
        need(cur.version() == 1, "a rejected delta moved the cursor");
    };
    std::string bad = d1;
    bad[0] ^= 1;
    st = rx.apply_delta<RxRoot>(dst, bad, cur);
    need(!st && st.code() == XErrc::bad_magic, "bad magic");
    unchanged("bad magic changed the replica");
    st = rx.apply_delta<RxRoot>(dst, d1.substr(0, 10), cur);
    need(!st && st.code() == XErrc::truncated, "truncated delta");
    st = rx.apply_delta<RxRoot>(dst, d2, cur);
    need(!st && st.code() == XErrc::delta_base_mismatch, "admit skipped a version");
    unchanged("a skipped version changed the replica");
    st = rx.apply_delta<RxRoot>(dst, d1 + std::string(1, '\0'), cur);
    need(!st && st.code() == XErrc::size_mismatch, "trailing bytes");

    // Hand-made deltas on the replica's stream and version: a block past the
    // used payload, and a root object that would end past it.
    const auto header = [&](std::uint32_t flags) {
        std::string h = {'X', 'D', char(0x10), char(flags)};
        const std::uint64_t stream = cur.stream();
        h.append(reinterpret_cast<const char*>(&stream), 8);
        return h + varint(cur.version() + 1);
    };
    st = rx.apply_delta<RxRoot>(dst, header(0) + varint(1) + varint(std::uint32_t(used(dst) / 64 + 4)) +
                                         std::string(1, '\0') + std::string(64, 'x'), cur);
    need(!st && st.code() == XErrc::invalid_extent_table, "block past used");
    unchanged("a block past used changed the replica");
    const auto words = std::uint32_t((used(dst) - XWIRE_PAYLOAD_BEGIN) / 8);
    st = rx.apply_delta<RxRoot>(dst, header(8) + varint(words) + varint(words * 8 - 8) + varint(0), cur);
    need(!st && st.code() == XErrc::root_mismatch, "root object past used");
    unchanged("a root past used changed the replica");

    // A digest that does not match rolls the patch back (a replica changed
    // locally: the delta's result differs from the sender's).
    XDeltaOptions digest;
    digest.digest = true;
    XDeltaBaseline fresh(79);
    auto twin = XBuffer::create<RxRoot>(4096, cap());
    XDeltaCursor tc;
    need(rx.apply_delta<RxRoot>(twin, src.save_delta<RxRoot>(fresh), tc).has_value(), "twin key");
    twin.root<RxRoot>().title = "changed here";
    src.root<RxRoot>().flag = !src.root<RxRoot>().flag;
    st = rx.apply_delta<RxRoot>(twin, src.save_delta<RxRoot>(fresh, digest), tc);
    need(!st && st.code() == XErrc::digest_mismatch, "admit missed a digest mismatch");
    need(std::string(twin.root<RxRoot>().title.c_str()) == "changed here", "digest mismatch was not rolled back");
    need(tc.version() == 1, "digest mismatch moved the cursor");

    // After the rejections the next delta still applies.
    need(rx.apply_delta<RxRoot>(dst, d1, cur).has_value() && rx.apply_delta<RxRoot>(dst, d2, cur).has_value(),
         "admit after rejections");
}

// What admit does not check: what the bytes mean. A delta that points a
// vector head outside the payload passes admit; the full check of a later
// delta on the same cursor verifies the whole payload and rejects it.
void delta_admit_skips_graph() {
    auto src = XBuffer::create<RxRoot>(4096, cap());
    auto& r = src.root<RxRoot>();
    for (std::uint32_t i = 0; i < 40; ++i) r.values.push_back(i);
    XDeltaBaseline base(91);
    XReceiver<XLink::server_peer> peer(cap());
    auto dst = XBuffer::create<RxRoot>(4096, cap());
    XDeltaCursor cur;
    need(peer.apply_delta<RxRoot>(dst, src.save_delta<RxRoot>(base), cur).has_value(), "key");
    // One block that rewrites the vector head's rel32 (bytes inside the root).
    const auto head = std::size_t(reinterpret_cast<const char*>(&src.root<RxRoot>().values) - src.data());
    const std::size_t block = head / 64, at = head % 64;
    std::uint64_t mask = std::uint64_t(0xF) << at;
    std::string d = {'X', 'D', char(0x10), char(0)};
    const std::uint64_t stream = cur.stream();
    d.append(reinterpret_cast<const char*>(&stream), 8);
    d += varint(cur.version() + 1) + varint(1) + varint(std::uint32_t(block));
    unsigned sel = 0;
    std::string masks;
    for (unsigned k = 0; k < 8; ++k)
        if (const auto byte = unsigned(mask >> (8 * k)) & 0xFF) {
            sel |= 1u << k;
            masks += char(byte);
        }
    d += char(sel) + masks;
    const std::int32_t outside = 0x40000000;
    d.append(reinterpret_cast<const char*>(&outside), 4);
    need(peer.apply_delta<RxRoot>(dst, d, cur).has_value(), "admit checked what the bytes mean");
    need(cur.last_check() == XDeltaCheck::admit && cur.version() == 2, "bad delta admitted");
    // The next delta of the stream writes nothing; checked in full it verifies
    // the whole payload, which no longer verifies.
    std::string next = {'X', 'D', char(0x10), char(0)};
    next.append(reinterpret_cast<const char*>(&stream), 8);
    next += varint(cur.version() + 1) + varint(0);
    XReceiver<XLink::from_client> client(cap());
    auto st = client.apply_delta<RxRoot>(dst, next, cur);
    need(!st && st.code() != XErrc::delta_base_mismatch, "client uplink (full) accepted the payload");
    st = peer.apply_delta<RxRoot, XCheck::full>(dst, next, cur);
    need(!st && st.code() != XErrc::delta_base_mismatch, "full after a bad admitted delta accepted the payload");
    need(cur.version() == 2, "a rejected full apply moved the cursor");
}
} // namespace

int main() {
    admit_matches_full();
    admit_rejects_framing();
    admit_rejects_bad_holes();
    admit_skips_graph();
    typed_and_links();
    reload_reuses_buffer();
    delta_admit_matches_full();
    delta_admit_rejects();
    delta_admit_skips_graph();
    if (failures) {
        std::fprintf(stderr, "test_receiver: %d failure(s)\n", failures);
        return 1;
    }
    std::puts("test_receiver: ok");
    return 0;
}
