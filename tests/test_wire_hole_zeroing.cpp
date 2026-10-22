// M3 (threat model T5): a retired block is cleared before it becomes a hole, so
// neither the arena nor any emitted wire carries freed contents. For dense and
// sparse wires from save_verified, save_verified_into and freeze_verified:
// every hole byte is zero and every payload byte equals the arena byte. Holes a
// loaded wire brought in keep the sender's bytes (forwarded unchanged); a block
// the receiver retires is cleared. Also checks the M1/M2 limits on declared
// wire sizes, and (W1) that M4 zeroes exactly the vacated elements for every
// byte count around the inline 8..64-byte zeroing bands.
#include "xoffsetdatastructure.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace xo = XOffsetDatastructure;

namespace hz {
struct Entry {
    std::uint8_t flag{};
    std::uint64_t value{};
    xo::XString note;
};
struct Root {
    xo::XString secret;
    xo::XVector<Entry> entries;
    xo::XVector<std::uint64_t> filler;
    xo::XVector<xo::XString> big;
};
struct alignas(8) Wide { std::uint64_t a{}, b{}, c{}; };
struct Bands {
    xo::XVector<std::uint8_t> u8;
    xo::XVector<std::uint32_t> u32;
    xo::XVector<std::uint64_t> u64;
    xo::XVector<Wide> wide;
};
} // namespace hz

XOFFSET_REGISTER_SCHEMA_NAME(hz::Root, "test.wire_hole_zeroing.root")
XOFFSET_REGISTER_SCHEMA_NAME(hz::Bands, "test.wire_hole_zeroing.bands")
namespace XOffsetDatastructure {
template<> struct wire_record_layout<hz::Entry> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<hz::Root> { static constexpr bool ordinary_members = true; };
}

namespace {
using Buffer = xo::TypedXBuffer<hz::Root>;
constexpr std::string_view kSecret = "SECRET-TOKEN-0123456789-abcdefghijklmnop";

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_wire_hole_zeroing FAILED: " << message << "\n";
    std::exit(1);
}

xo::XBufferCore::MaximumCapacity cap() {
    return xo::XBuffer::maximum_capacity(16u << 20).with_growth(xo::XGrowPolicy::Automatic);
}

// Retires blocks that held recognizable contents; `sparse` adds a 64 KiB run.
// Every allocation that could reuse the secret's block happens before it is
// retired, so the secret's block is still a hole when the wire is emitted.
Buffer make(bool sparse) {
    auto b = Buffer::create(65536, cap());
    auto& r = b.root();
    for (int i = 0; i < 8; ++i) {
        auto& e = r.entries.emplace_back();
        e.flag = 1;
        e.value = 2;
        e.note = "short";                       // inline
    }
    if (sparse) {
        r.big.emplace_back(std::string(64 * 1024, 'B'));
        r.entries.emplace_back().note = "after";
        r.big.pop_back();                       // 64 KiB of 'B' becomes a sparse gap
    }
    r.filler.assign(64, 0x4141414141414141ull);
    r.secret = std::string(kSecret);
    r.filler.push_back(1);                      // keeps the secret's block off the tail
    r.secret = std::string(200, 'z');           // retires the secret's block (P1)
    r.filler.clear();                           // the AAAA elements become spare capacity (P2)
    return b;
}

struct Parsed {
    xo::XWireHeaderV2 header{};
    std::vector<std::array<std::uint32_t, 2>> extents;  // payload offset, bytes
    const char* stored = nullptr;
    std::vector<xo::XBufferCore::WireHole> holes;
};

Parsed parse(std::string_view wire) {
    Parsed p;
    std::memcpy(&p.header, wire.data(), sizeof(p.header));
    std::size_t at = sizeof(p.header);
    for (std::uint64_t i = 0; i < p.header.extent_count; ++i, at += 8) {
        std::array<std::uint32_t, 2> e{};
        std::memcpy(e.data(), wire.data() + at, 8);
        p.extents.push_back(e);
    }
    if (p.extents.empty()) p.extents.push_back({0, static_cast<std::uint32_t>(p.header.used_bytes)});
    p.stored = wire.data() + at;
    std::size_t stored = 0;
    for (const auto& e : p.extents) stored += e[1];
    at += stored;
    for (std::uint64_t i = 0; i < p.header.hole_count; ++i, at += 8) {
        xo::XBufferCore::WireHole h{};
        std::memcpy(&h, wire.data() + at, 8);
        p.holes.push_back(h);
    }
    if (at != wire.size()) fail("wire framing does not add up");
    return p;
}

// Every emitted payload byte equals the arena byte, and is zero inside a hole.
void check_emitted(std::string_view wire, const char* arena, const char* what) {
    const auto p = parse(wire);
    std::vector<bool> in_hole(p.header.used_bytes, false);
    std::uint64_t hole_bytes = 0;
    for (const auto& h : p.holes) {
        for (std::uint32_t i = 0; i < h.bytes; ++i) in_hole[h.offset + i] = true;
        hole_bytes += h.bytes;
    }
    if (hole_bytes == 0) fail(std::string(what) + ": fixture has no holes");
    const char* stored = p.stored;
    for (const auto& e : p.extents) {
        for (std::uint32_t i = 0; i < e[1]; ++i) {
            const auto offset = std::size_t(e[0]) + i;
            if (stored[i] != arena[offset])
                fail(std::string(what) + ": byte at payload offset " + std::to_string(offset) +
                     " differs from the arena");
            if (in_hole[offset] && stored[i] != 0)
                fail(std::string(what) + ": hole byte at payload offset " + std::to_string(offset) +
                     " is not zero");
        }
        stored += e[1];
    }
    for (std::uint64_t offset = 0; offset < p.header.used_bytes; ++offset)
        if (in_hole[offset] && arena[offset] != 0)
            fail(std::string(what) + ": arena hole byte at offset " + std::to_string(offset) + " is not zero");
    if (wire.find(kSecret.substr(0, 12)) != std::string_view::npos ||
        wire.find("AAAAAAAA") != std::string_view::npos)
        fail(std::string(what) + ": freed contents reached the wire");
}

void emitted_holes_are_zero() {
    for (bool sparse : {false, true}) {
        auto b = make(sparse);
        const auto wire = b.save_verified();
        if ((parse(wire).header.extent_count != 0) != sparse) fail("unexpected dense/sparse emission");
        const char* arena = static_cast<const char*>(b.get_address());
        if (std::string_view(arena, b.arena()->used_end).find(kSecret.substr(0, 12)) != std::string_view::npos)
            fail("the retired secret is still in the arena");
        check_emitted(wire, arena, sparse ? "save_verified sparse" : "save_verified dense");

        std::string into = "previous contents";
        b.save_verified_into(into);
        if (into != wire) fail("save_verified_into differs from save_verified");

        // Reload installs the hole table; the re-emitted wire is identical.
        auto again = Buffer::load(wire, cap());
        if (again.save_verified() != wire) fail("reload changed the emitted bytes");
    }
}

void frozen_segments_are_zero() {
    auto b = make(false);
    const auto expected = b.save_verified();
    auto snapshot = xo::freeze_verified(std::move(b));
    std::string joined;
    for (const auto& seg : snapshot.segments())
        joined.append(reinterpret_cast<const char*>(seg.data()), seg.size());
    if (joined != expected) fail("frozen segments differ from save_verified bytes");
    if (joined.find(kSecret.substr(0, 12)) != std::string::npos) fail("frozen segments carry freed contents");
}

// A dense wire whose hole holds sender bytes: load accepts it, re-save forwards
// those bytes unchanged (they are the sender's, not the receiver's), and a block
// the receiver itself retires is cleared.
void loaded_holes_keep_sender_bytes() {
    auto b = make(false);
    std::string wire = b.save_verified();
    const auto p = parse(wire);
    if (p.holes.empty() || p.header.extent_count != 0) fail("fixture: need a dense wire with holes");
    const auto hole = p.holes.front();
    const auto at = static_cast<std::size_t>(p.stored - wire.data()) + hole.offset;
    std::memset(wire.data() + at, 0x5a, hole.bytes);
    auto loaded = Buffer::load(wire, cap());
    if (loaded.save_verified() != wire) fail("sender hole bytes were not forwarded unchanged");
    auto& r = loaded.root();
    r.secret = std::string(kSecret);
    r.entries.emplace_back().note = "keeps the secret's block off the tail";
    r.secret = std::string(300, 'y');           // the receiver retires the secret's block
    const auto again = loaded.save_verified();
    if (again.find(kSecret.substr(0, 12)) != std::string::npos) fail("receiver-freed contents reached the wire");
    const char* arena = static_cast<const char*>(loaded.get_address());
    if (std::string_view(arena, loaded.arena()->used_end).find(kSecret.substr(0, 12)) != std::string_view::npos)
        fail("receiver-retired secret is still in the arena");
}

std::string inflate_sparse(const std::string& wire, std::uint64_t used) {
    xo::XWireHeaderV2 h;
    std::memcpy(&h, wire.data(), sizeof(h));
    if (h.extent_count || h.hole_count) fail("inflate needs a dense wire without holes");
    const std::uint64_t u0 = h.used_bytes;
    std::string payload = wire.substr(sizeof(h), u0);
    std::memcpy(payload.data() + 24, &used, 8); // arena header used_end
    h.used_bytes = used;
    h.hole_count = 1;
    h.hole_bytes = used - u0;
    h.extent_count = 1;
    std::string out(reinterpret_cast<const char*>(&h), sizeof(h));
    const std::uint32_t extent[2] = {0, static_cast<std::uint32_t>(u0)};
    out.append(reinterpret_cast<const char*>(extent), 8);
    out += payload;
    const std::uint32_t hole[2] = {static_cast<std::uint32_t>(u0), static_cast<std::uint32_t>(used - u0)};
    out.append(reinterpret_cast<const char*>(hole), 8);
    return out;
}

template <class F>
std::string rejection(F&& f) {
    try { f(); } catch (const xo::XBadAlloc& e) { return e.what(); }
    fail("expected XBadAlloc");
}

void declared_size_limits() {
    auto small = Buffer::create(4096);
    const auto base = small.save_verified();
    // A few hundred bytes that claim 1 GiB (threat model R1).
    const auto claim = inflate_sparse(base, std::uint64_t(1) << 30);
    if (claim.size() > 512) fail("inflated fixture is not small");
    auto what = rejection([&] { (void)Buffer::load(claim); });
    if (what.find("exceeds the default maximum_capacity") == std::string::npos)
        fail("default limit message: " + what);
    what = rejection([&] { (void)Buffer::load(claim, xo::XBuffer::maximum_capacity(1u << 20)); });
    if (what.find("exceeds maximum_capacity 1048576") == std::string::npos)
        fail("explicit limit message: " + what);
    what = rejection([&] { (void)xo::begin_verified_receive<hz::Root>(std::size_t(1) << 30); });
    if (what.find("begin_verified_receive") == std::string::npos) fail("receive limit message: " + what);
    // Within the default limit a claim is still accepted (and stays sparse).
    const auto ok = inflate_sparse(base, 32u << 20);
    auto loaded = Buffer::load(ok);
    if (loaded.save_verified() != ok) fail("a 32 MiB sparse claim did not round-trip");
    // An explicit limit admits larger wires.
    auto large = Buffer::load(inflate_sparse(base, 96u << 20), xo::XBuffer::maximum_capacity(128u << 20));
    if (large.arena()->used_end != (96u << 20)) fail("explicit limit did not admit 96 MiB");
}

// W1: clear/pop_back/erase/resize zero exactly the vacated elements (M4), for
// every vacated byte count from 0 to past the 64-byte inline band, while live
// elements and bytes past the old size are untouched. Also pins the inline
// reserve/emplace_back fast paths to the old outcomes.
template <class T, class Fill>
void check_vector_scrub(xo::XVector<T>& v, const char* what, Fill&& fill) {
    constexpr std::size_t kCap = 24;
    auto spare = [&](std::size_t from) {
        const auto* bytes = reinterpret_cast<const unsigned char*>(v.data());
        for (std::size_t i = from * sizeof(T); i < v.capacity() * sizeof(T); ++i)
            if (bytes[i] != 0) return false;
        return true;
    };
    auto refill = [&](std::size_t n) {
        v.clear();
        v.reserve(kCap);
        for (std::size_t i = 0; i < n; ++i) fill(v.emplace_back(), i);
        if (v.size() != n || v.capacity() != kCap) fail(std::string(what) + ": refill shape");
    };
    auto live_ok = [&](std::size_t n, std::size_t skip) {
        for (std::size_t i = 0, j = 0; i < v.size(); ++i, ++j) {
            if (j == skip) ++j;
            T expect{};
            fill(expect, j);
            if (std::memcmp(&v[static_cast<std::uint32_t>(i)], &expect, sizeof(T)) != 0) return false;
        }
        return v.size() == n;
    };
    for (std::size_t n = 0; n <= kCap; ++n) {
        refill(n);
        if (!spare(n)) fail(std::string(what) + ": spare capacity not zero after refill, n=" + std::to_string(n));
        v.clear();
        if (!spare(0)) fail(std::string(what) + ": clear left bytes, n=" + std::to_string(n));
        for (std::size_t keep = 0; keep <= n; ++keep) {
            refill(n);
            v.resize(keep);
            if (!live_ok(keep, kCap) || !spare(keep))
                fail(std::string(what) + ": resize " + std::to_string(n) + "->" + std::to_string(keep));
        }
        if (n == 0) continue;
        refill(n);
        v.pop_back();
        if (!live_ok(n - 1, kCap) || !spare(n - 1)) fail(std::string(what) + ": pop_back, n=" + std::to_string(n));
        refill(n);
        v.erase(v.begin());
        if (!live_ok(n - 1, 0) || !spare(n - 1)) fail(std::string(what) + ": erase front, n=" + std::to_string(n));
    }
    // reserve below capacity is a no-op; growth keeps contents.
    refill(5);
    const auto* before = v.data();
    v.reserve(3);
    v.reserve(kCap);
    if (v.data() != before || v.capacity() != kCap || !live_ok(5, kCap)) fail(std::string(what) + ": no-op reserve");
    v.reserve(kCap * 3);
    if (v.capacity() != kCap * 3 || !live_ok(5, kCap) || !spare(5)) fail(std::string(what) + ": growing reserve");
    while (v.size() < v.capacity()) fill(v.emplace_back(), v.size());
    fill(v.emplace_back(), v.size());           // full: takes the growth path
    if (v.size() != kCap * 3 + 1 || !live_ok(kCap * 3 + 1, kCap * 3 + 2)) fail(std::string(what) + ": growth append");
}

void spare_capacity_scrub_bands() {
    auto b = xo::TypedXBuffer<hz::Bands>::create(65536, cap());
    auto& r = b.root();
    check_vector_scrub(r.u8, "u8", [](std::uint8_t& e, std::size_t i) { e = std::uint8_t(0xA0 + i); });
    check_vector_scrub(r.u32, "u32", [](std::uint32_t& e, std::size_t i) { e = 0xC1C2C300u + std::uint32_t(i); });
    check_vector_scrub(r.u64, "u64", [](std::uint64_t& e, std::size_t i) { e = 0xD1D2D3D4D5D6D700ull + i; });
    check_vector_scrub(r.wide, "wide", [](hz::Wide& e, std::size_t i) {
        e.a = 0xE1E2E3E4E5E6E700ull + i; e.b = ~e.a; e.c = e.a ^ 0x5555u;
    });
    const auto wire = b.save_verified();
    auto again = xo::TypedXBuffer<hz::Bands>::load(wire, cap());
    if (again.save_verified() != wire) fail("bands fixture did not round-trip");

    // Unbound vectors keep the old errors on the inline paths.
    xo::XVector<std::uint32_t> loose;
    bool threw = false;
    try { loose.reserve(0); } catch (const xo::XException&) { threw = true; }
    if (!threw) fail("reserve(0) on an unbound vector no longer throws");
    threw = false;
    try { loose.emplace_back(); } catch (const xo::XException&) { threw = true; }
    if (!threw) fail("emplace_back on an unbound vector no longer throws");
}
} // namespace

int main() {
    emitted_holes_are_zero();
    frozen_segments_are_zero();
    loaded_holes_keep_sender_bytes();
    declared_size_limits();
    spare_capacity_scrub_bands();
    std::cout << "wire hole zeroing, declared-size limits and spare-capacity bands: 5 groups passed\n";
    return 0;
}
