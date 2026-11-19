// Threat model T5: freed contents on the wire. In every build: a retired block
// is cleared before it becomes a free block (M3), so neither the arena nor any
// emitted wire carries freed contents; each emitted image byte equals the
// arena byte and every free block byte past its link word is zero (dense and
// sparse wires from save_verified and save_verified_into); free blocks a
// loaded wire brought in keep the sender's bytes (forwarded unchanged); and
// save_compact carries nothing that was freed or vacated. With the M4 scrub switches
// (XOFFSET_SCRUB_ERASED, _POPPED; this program is built again as *_hardened
// with XOFFSET_HARDENED=1) vacated elements are zeroed too, so no emitted wire
// carries anything deleted. Also checks the M1/M2 limits on declared wire
// sizes.
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
} // namespace hz
XOFFSET_RECORD_DERIVED(hz::Entry, (), flag, value, note)
XOFFSET_RECORD_DERIVED(hz::Root, (), secret, entries, filler, big)

XOFFSET_REGISTER_SCHEMA_NAME(hz::Root, "test.wire_hole_zeroing.root")
namespace XOffsetDatastructure {
template<> struct wire_record_layout<hz::Entry> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<hz::Root> { static constexpr bool ordinary_members = true; };
}

namespace {
using Buffer = xo::TypedXBuffer<hz::Root>;
constexpr std::string_view kSecret = "SECRET-TOKEN-0123456789-abcdefghijklmnop";
// Whether this build zeroes what containers vacate (M4); freed blocks always are (M3).
constexpr bool kScrubbed = XOFFSET_SCRUB_ERASED && XOFFSET_SCRUB_POPPED;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_wire_hole_zeroing FAILED: " << message << "\n";
    std::exit(1);
}

xo::XBufferCore::MaximumCapacity cap() {
    return xo::XBuffer::maximum_capacity(16u << 20).with_growth(xo::XGrowPolicy::Automatic);
}

// Retires blocks that held recognizable contents; `sparse` adds a 64 KiB run
// (skipped by the default and the zero-run emission).
// Every allocation that could reuse the secret's block happens before it is
// retired, so the secret's block is still a hole when the wire is emitted.
Buffer make(bool sparse) {
    auto b = Buffer::create(65536, cap());
    auto& r = b.plain_root();
    for (int i = 0; i < 8; ++i) {
        auto& e = r.entries.emplace_back();
        e.flag = 1;
        e.value = 2;
        e.note = "short";                       // inline
    }
    if (sparse) {
        r.big.emplace_back(std::string(64 * 1024, 'B'));
        r.entries.emplace_back().note = "after";
        r.big.pop_back();                       // 64 KiB of 'B' becomes a zeroed hole
    }
    r.filler.assign(64, 0x4141414141414141ull);
    r.secret = std::string(kSecret);
    r.filler.push_back(1);                      // keeps the secret's block off the tail
    r.secret = std::string(200, 'z');           // retires the secret's block (P1)
    r.filler.clear();                           // the AAAA elements become spare capacity (P2)
    return b;
}

struct Parsed {
    xo::XWireHeader header{};
    std::vector<std::array<std::uint32_t, 2>> extents;  // image offset, bytes
    const char* stored = nullptr;
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
    if (p.extents.empty())
        p.extents.push_back({std::uint32_t(xo::XWIRE_PAYLOAD_BEGIN),
                             static_cast<std::uint32_t>(p.header.used_bytes - xo::XWIRE_PAYLOAD_BEGIN)});
    p.stored = wire.data() + at;
    std::size_t stored = 0;
    for (const auto& e : p.extents) stored += e[1];
    at += stored;
    if (at != wire.size()) fail("wire framing does not add up");
    return p;
}

std::uint32_t word_at(const char* image, std::size_t at) {
    std::uint32_t v;
    std::memcpy(&v, image + at, sizeof(v));
    return v;
}
// The free blocks of an image (its directory's stacks; the directory block
// itself is not one).
std::vector<xo::XBufferCore::WireHole> free_blocks(const char* image) {
    xo::XWireHeader h;
    std::memcpy(&h, image, sizeof(h));
    std::vector<xo::XBufferCore::WireHole> out;
    if (h.free_dir == 0) return out;
    const std::uint32_t n = word_at(image, h.free_dir);
    for (std::uint32_t k = 0; k < n; ++k) {
        const std::uint32_t size = word_at(image, h.free_dir + 8 + 8 * k);
        for (std::uint32_t at = word_at(image, h.free_dir + 12 + 8 * k); at != 0; at = word_at(image, at))
            out.push_back({at, size});
    }
    return out;
}

// Every emitted image byte equals the arena byte, and is zero inside a free
// block past its link word.
void check_emitted(std::string_view wire, const char* arena, const char* what) {
    const auto p = parse(wire);
    std::vector<bool> in_hole(p.header.used_bytes, false);
    std::uint64_t hole_bytes = 0;
    constexpr std::uint32_t header = xo::XBufferCore::free_block_header_bytes;
    for (const auto& h : free_blocks(arena)) {
        hole_bytes += h.bytes;
        for (std::uint32_t i = header; i < h.bytes; ++i) in_hole[h.offset + i] = true;
    }
    if (hole_bytes == 0 || hole_bytes != p.header.free_bytes) fail(std::string(what) + ": fixture has no free blocks");
    xo::XWireHeader image_header;
    std::memcpy(&image_header, arena, sizeof(image_header));
    image_header.extent_count = p.header.extent_count;
    if (std::memcmp(&image_header, wire.data(), sizeof(image_header)) != 0)
        fail(std::string(what) + ": the wire header is not the image header");
    const char* stored = p.stored;
    for (const auto& e : p.extents) {
        for (std::uint32_t i = 0; i < e[1]; ++i) {
            const auto offset = std::size_t(e[0]) + i;
            if (stored[i] != arena[offset])
                fail(std::string(what) + ": byte at image offset " + std::to_string(offset) +
                     " differs from the arena");
            if (in_hole[offset] && stored[i] != 0)
                fail(std::string(what) + ": free byte at image offset " + std::to_string(offset) +
                     " is not zero");
        }
        stored += e[1];
    }
    for (std::uint64_t offset = 0; offset < p.header.used_bytes; ++offset)
        if (in_hole[offset] && arena[offset] != 0)
            fail(std::string(what) + ": arena free byte at offset " + std::to_string(offset) + " is not zero");
    if (wire.find(kSecret.substr(0, 12)) != std::string_view::npos)
        fail(std::string(what) + ": freed contents reached the wire");
    if (kScrubbed && wire.find("AAAAAAAA") != std::string_view::npos)
        fail(std::string(what) + ": cleared elements reached the wire");
}

void emitted_holes_are_zero() {
    for (bool sparse : {false, true})
    for (auto emission : {xo::XWireEmission::free_runs, xo::XWireEmission::dense, xo::XWireEmission::zero_runs}) {
        auto b = make(sparse);
        const xo::XSaveOptions options{.emission = emission};
        const auto wire = b.save_verified(options);
        if ((parse(wire).header.extent_count != 0) != (sparse && emission != xo::XWireEmission::dense))
            fail("unexpected dense/sparse emission");
        const char* arena = static_cast<const char*>(b.get_address());
        if (std::string_view(arena, b.arena()->used_end).find(kSecret.substr(0, 12)) != std::string_view::npos)
            fail("the retired secret is still in the arena");
        check_emitted(wire, arena, sparse ? "save_verified sparse" : "save_verified dense");

        std::string into = "previous contents";
        b.save_verified_into(into, options);
        if (into != wire) fail("save_verified_into differs from save_verified");

        // The free blocks travel in the image; the re-emitted wire is identical.
        auto again = Buffer::load(wire, cap());
        if (again.save_verified(options) != wire) fail("reload changed the emitted bytes");
    }
}

// A dense wire whose free block holds sender bytes: load accepts it, re-save
// forwards those bytes unchanged (they are the sender's, not the receiver's),
// and a block the receiver itself retires is cleared.
void loaded_holes_keep_sender_bytes() {
    constexpr xo::XSaveOptions image{.emission = xo::XWireEmission::free_runs};
    auto b = make(false);
    std::string wire = b.save_verified(image);
    const auto p = parse(wire);
    const auto blocks = free_blocks(wire.data());
    if (blocks.empty() || p.header.extent_count != 0) fail("fixture: need a dense wire with free blocks");
    // A free block's bytes past its link word (a dense wire is the image).
    const auto hole = blocks.back();
    constexpr std::uint32_t header = xo::XBufferCore::free_block_header_bytes;
    std::memset(wire.data() + hole.offset + header, 0x5a, hole.bytes - header);
    auto loaded = Buffer::load(wire, cap());
    if (loaded.save_verified(image) != wire) fail("sender hole bytes were not forwarded unchanged");
    auto& r = loaded.plain_root();
    r.secret = std::string(kSecret);
    r.entries.emplace_back().note = "keeps the secret's block off the tail";
    r.secret = std::string(300, 'y');           // the receiver retires the secret's block
    const auto again = loaded.save_verified(image);
    if (again.find(kSecret.substr(0, 12)) != std::string::npos) fail("receiver-freed contents reached the wire");
    const char* arena = static_cast<const char*>(loaded.get_address());
    if (std::string_view(arena, loaded.arena()->used_end).find(kSecret.substr(0, 12)) != std::string_view::npos)
        fail("receiver-retired secret is still in the arena");
}

// save_compact rebuilds the wire from the live graph: no hole, spare capacity
// and string tails written as zero. It carries nothing freed or vacated in
// any build (the way to send from a default build to a reader that must not
// see deleted data), and loads as the same content.
void compact_carries_no_freed_contents() {
    for (bool sparse : {false, true}) {
        auto b = make(sparse);
        const auto wire = b.save_compact();
        if (wire.find(kSecret.substr(0, 12)) != std::string::npos || wire.find("AAAAAAAA") != std::string::npos ||
            wire.find("BBBBBBBB") != std::string::npos)
            fail("save_compact carries freed contents");
        if (parse(wire).header.free_dir != 0) fail("save_compact wrote a free block");
        auto loaded = Buffer::load(wire, cap());
        const auto& r = loaded.plain_root();
        if (r.secret.view() != std::string(200, 'z') || !r.filler.empty() ||
            r.entries.size() != (sparse ? 9u : 8u) || r.entries[0].note.view() != "short")
            fail("save_compact changed the live content");
    }
}

// A small sparse wire that claims `used` bytes: the dense image of `wire`,
// then a free directory of 8 entries naming one free block that fills the
// rest, which is a gap (it loads as zero, as a free block is).
std::string inflate_sparse(const std::string& wire, std::uint64_t used) {
    xo::XWireHeader h;
    std::memcpy(&h, wire.data(), sizeof(h));
    if (h.extent_count || h.free_dir) fail("inflate needs a dense wire without free blocks");
    const std::uint32_t u0 = h.used_bytes, dir_bytes = 72, block = u0 + dir_bytes;
    std::uint32_t dir[18] = {1, 8, static_cast<std::uint32_t>(used - block), block};
    for (int k = 1; k < 8; ++k) {
        dir[2 + 2 * k] = 0xFFFFFFFFu;
        dir[3 + 2 * k] = 0;
    }
    h.used_bytes = static_cast<std::uint32_t>(used);
    h.extent_count = 1;
    h.free_dir = u0;
    h.free_bytes = static_cast<std::uint32_t>(used - block);
    std::string out(reinterpret_cast<const char*>(&h), sizeof(h));
    const std::uint32_t extent[2] = {std::uint32_t(xo::XWIRE_PAYLOAD_BEGIN),
                                     static_cast<std::uint32_t>(block - xo::XWIRE_PAYLOAD_BEGIN)};
    out.append(reinterpret_cast<const char*>(extent), 8);
    out.append(wire, xo::XWIRE_PAYLOAD_BEGIN, u0 - xo::XWIRE_PAYLOAD_BEGIN);
    out.append(reinterpret_cast<const char*>(dir), dir_bytes);
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
    // Within the default limit a claim is still accepted, and free-run
    // emission skips the claimed free run again.
    const auto ok = inflate_sparse(base, 32u << 20);
    auto loaded = Buffer::load(ok);
    const auto resent = loaded.save_verified(xo::XSaveOptions{.emission = xo::XWireEmission::free_runs});
    // The resent wire also carries the free block's link word.
    if (resent.size() > ok.size() + 64 || Buffer::load(resent).arena()->used_end != (32u << 20))
        fail("a 32 MiB sparse claim did not round-trip sparsely");
    // The default (automatic) emission sends it compact: no free block at all.
    const auto automatic = loaded.save_verified();
    if (automatic.size() > base.size() + 64 || parse(automatic).header.free_dir != 0)
        fail("the default emission of a 32 MiB free claim is not compact");
    // An explicit limit admits larger wires.
    auto large = Buffer::load(inflate_sparse(base, 96u << 20), xo::XBuffer::maximum_capacity(128u << 20));
    if (large.arena()->used_end != (96u << 20)) fail("explicit limit did not admit 96 MiB");
}
} // namespace

int main() {
    emitted_holes_are_zero();
    loaded_holes_keep_sender_bytes();
    compact_carries_no_freed_contents();
    declared_size_limits();
    std::cout << "wire hole zeroing (M4 scrub switches " << (kScrubbed ? "on" : "off")
              << ") and declared-size limits: 4 groups passed\n";
    return 0;
}
