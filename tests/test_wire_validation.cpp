// Purpose: Reject hostile offsets, container heads, hole tables and payload
// metadata before dereferencing or mutation (V2 wire: header + payload + hole
// table; live blocks plus holes must tile [0, used_end) exactly).
#include <cassert>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "../xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

struct CheckedRoot {
    int32_t id{};
    bool enabled{};
    bool bits[2];
    XString name;
    XVector<int32_t> values;
    XMap<int32_t, int32_t> map;
    XVector<XString> labels;
};
XOFFSET_REGISTER_SCHEMA_NAME(CheckedRoot, "test.CheckedRoot")

namespace {
using Hole = XBufferCore::WireHole;
constexpr const char* kCoverage =
    "wire validation failed: payload is not exactly covered by live blocks and holes";
constexpr const char* kHoleEntry = "wire validation failed: invalid hole table entry";
constexpr const char* kLiveFree = "wire validation detected live/free overlap";
constexpr const char* kLiveLive = "wire validation detected overlapping dynamic ranges";
constexpr const char* kFreeFree = "wire validation detected overlapping free-list ranges";
constexpr const char* kBinding = "wire validation: invalid allocator binding";
constexpr const char* kRelative = "wire validation: relative pointer out of bounds";

std::size_t precise_rejections = 0;

void need(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(what);
}
template <typename T> T read(const std::string& bytes, std::size_t pos) {
    need(pos <= bytes.size() && sizeof(T) <= bytes.size() - pos, "fixture read out of bounds");
    T result{};
    std::memcpy(&result, bytes.data() + pos, sizeof(T));
    return result;
}
template <typename T> void write(std::string& bytes, std::size_t pos, const T& value) {
    need(pos <= bytes.size() && sizeof(T) <= bytes.size() - pos, "fixture write out of bounds");
    std::memcpy(bytes.data() + pos, &value, sizeof(T));
}

// A V2 wire split into its three segments. build() recomputes used_bytes,
// hole_count and hole_bytes unless asked to keep a deliberately bad header.
struct Parts {
    XWireHeaderV2 header{};
    std::string payload;
    std::vector<Hole> holes;
};
Parts parse(const std::string& wire) {
    Parts p;
    p.header = read<XWireHeaderV2>(wire, 0);
    need(p.header.extent_count == 0, "fixture wire is sparse; Parts handles dense wires only");
    need(wire.size() == sizeof(XWireHeaderV2) + p.header.used_bytes + p.header.hole_count * sizeof(Hole),
         "fixture wire is not header + payload + hole table");
    p.payload = wire.substr(sizeof(XWireHeaderV2), p.header.used_bytes);
    for (std::size_t i = 0; i < p.header.hole_count; ++i)
        p.holes.push_back(read<Hole>(wire, sizeof(XWireHeaderV2) + p.header.used_bytes + i * sizeof(Hole)));
    return p;
}
std::string build(Parts p, bool fix_counts = true) {
    if (fix_counts) {
        p.header.used_bytes = p.payload.size();
        p.header.hole_count = p.holes.size();
        std::uint64_t sum = 0;
        for (auto h : p.holes) sum += h.bytes;
        p.header.hole_bytes = sum;
    }
    std::string out(reinterpret_cast<const char*>(&p.header), sizeof(p.header));
    out += p.payload;
    for (auto h : p.holes) out.append(reinterpret_cast<const char*>(&h), sizeof(h));
    return out;
}

void accept(const std::string& bytes, const char* what) {
    try {
        auto b = XBuffer::load_verified<CheckedRoot>(bytes);
        need(b.root<CheckedRoot>().id == 42, what);
        // Accepted images must stay mutable and re-save as valid V2 wires.
        need(b.grow(8192), "accepted image cannot grow");
        auto& value = b.root<CheckedRoot>();
        value.values.push_back(7);
        value.labels.push_back("accepted-and-mutated");
        auto again = XBuffer::load_verified<CheckedRoot>(b.save_verified<CheckedRoot>());
        need(again.root<CheckedRoot>().values.back() == 7, what);
    } catch (const XException& e) {
        std::cerr << "unexpected rejection: " << e.what() << '\n';
        throw std::runtime_error(what);
    }
}
// expected == nullptr: any XException. Otherwise e.what() must contain it.
void reject(const std::string& bytes, const char* expected = nullptr) {
    bool rejected = false;
    std::string message;
    try { (void)XBuffer::load_verified<CheckedRoot>(bytes); }
    catch (const XException& e) { rejected = true; message = e.what(); }
    if (!rejected) {
        std::cerr << "accepted hostile wire; expected: " << (expected ? expected : "any rejection") << '\n';
        throw std::runtime_error("hostile wire accepted");
    }
    if (expected) {
        if (message.find(expected) == std::string::npos) {
            std::cerr << "expected '" << expected << "' got '" << message << "'\n";
            throw std::runtime_error("wrong rejection category");
        }
        ++precise_rejections;
    }
}

// Payload offset of the block a head's data_delta resolves to.
std::size_t head_target(const std::string& payload, std::size_t head_offset) {
    const auto origin = head_offset + offsetof(XFixedString, data_delta);
    return static_cast<std::size_t>(static_cast<std::int64_t>(origin) + read<std::int32_t>(payload, origin));
}
void set_head_target(std::string& payload, std::size_t head_offset, std::size_t target) {
    const auto origin = head_offset + offsetof(XFixedString, data_delta);
    write(payload, origin, static_cast<std::int32_t>(static_cast<std::int64_t>(target) - std::int64_t(origin)));
}

// V1 rejection: an 88-byte V1 header (layout of the retired XWireHeaderV1).
struct LegacyWireHeaderV1 {
    char magic[8];
    std::uint16_t format_major, format_minor;
    std::uint32_t header_size;
    std::uint64_t used_bytes, reserved_bytes, root_offset, allocator_offset;
    std::uint64_t root_type_id, schema_hash_lo, schema_hash_hi;
    std::uint32_t flags, endian_tag, crc32c, reserved0;
};
static_assert(sizeof(LegacyWireHeaderV1) == 88);
} // namespace

int main() {
    try {
    auto buf = XBuffer::create<CheckedRoot>(8192);
    auto& r = buf.root<CheckedRoot>();
    r.id = 42;
    // A heap string whose capacity exceeds its size (assign within capacity
    // keeps the 30-byte buffer), so capacity_ alone determines its extent.
    r.name = "checked-heap-name-capacity-030";
    r.name = "checked-heap-name";
    need(!r.name.is_inline() && r.name.capacity() == 30 && r.name.size() == 17, "fixture name layout");
    r.labels.push_back("array-zero");            // 10 bytes: inline (boundary)
    r.labels.push_back("array-one");             // inline
    r.labels.push_back("array-two-heap-label");  // out of line
    need(r.labels[0].is_inline() && r.labels[1].is_inline() && !r.labels[2].is_inline(), "fixture label layout");
    r.values.reserve(4);
    r.values.push_back(10);
    r.map.reserve(4); // Keep values non-tail so reserve must retire its old block.
    r.values.reserve(16); // The retired allocation becomes the one wire hole.
    r.map.try_emplace(1, 11);
    r.map.try_emplace(2, 22);
    const auto original = buf.save_verified<CheckedRoot>();
    const auto base = parse(original);
    const auto& h = base.header;
    need(h.hole_count == 1 && base.holes[0].bytes == 16 && h.hole_bytes == 16, "fixture must have one 16-byte hole");
    need(build(base) == original, "fixture split/rebuild is not byte-identical");
    const std::size_t payload = sizeof(h), root = payload + h.root_offset;
    const std::size_t proot = h.root_offset; // root offset inside Parts::payload
    const Hole hole = base.holes[0];
    auto loaded = XBuffer::load_verified<CheckedRoot>(original);
    assert(loaded.root<CheckedRoot>().id == 42);
    assert(loaded.root<CheckedRoot>().labels[2].view() == "array-two-heap-label");

    // ---- Wire header -------------------------------------------------------
    {   // V1 input (XOFFV1 magic) is rejected explicitly, not as a bad magic.
        auto bytes = original;
        std::memcpy(bytes.data(), XWIRE_MAGIC_V1, sizeof(XWIRE_MAGIC_V1));
        reject(bytes, "unsupported wire format version 1");
        // A complete V1-shaped wire (88-byte V1 header + XBUFV1 payload).
        LegacyWireHeaderV1 v1{};
        std::memcpy(v1.magic, XWIRE_MAGIC_V1, sizeof(v1.magic));
        v1.format_major = 1;
        v1.header_size = sizeof(v1);
        v1.used_bytes = base.payload.size();
        v1.root_offset = h.root_offset;
        v1.root_type_id = h.root_type_id;
        v1.schema_hash_lo = h.schema_hash_lo;
        v1.endian_tag = XWIRE_ENDIAN_TAG_V2;
        std::string legacy(reinterpret_cast<const char*>(&v1), sizeof(v1));
        auto legacy_payload = base.payload;
        legacy_payload[5] = '1'; // XBUFV1
        legacy += legacy_payload;
        reject(legacy, "unsupported wire format version 1");
    }
    {
        auto p = base;
        p.header.format_major = 1; // V2 magic with a V1 major
        reject(build(p), "unsupported format version");
        p = base;
        p.header.format_minor = 1;
        reject(build(p), "unsupported format version");
        p = base;
        p.header.magic[5] = '3';
        reject(build(p), "bad magic");
        p = base;
        p.header.header_size = 88;
        reject(build(p), "header size mismatch");
    }
    for (int field = 0; field < 2; ++field) {
        auto p = base;
        if (field == 0) p.header.crc32c = 1;
        if (field == 1) p.header.reserved0 = 1;
        reject(build(p), "unsupported reserved header fields");
    }
    {   // A dense wire relabelled as sparse: the payload bytes are not an extent table.
        auto p = base;
        p.header.extent_count = 1;
        reject(build(p, false), "invalid extent table");
        p.header.extent_count = UINT64_MAX;
        reject(build(p, false), "used size mismatch");
    }
    {
        auto p = base;
        p.header.flags = 1;
        reject(build(p), "unsupported flags");
    }
    {   // hole_bytes must equal the sum of the table entries.
        auto p = base;
        p.header.hole_bytes += 8;
        reject(build(p, false), "hole byte count mismatch");
        p = base;
        p.header.hole_count = 0; // table bytes become unexplained trailing bytes
        reject(build(p, false), "used size mismatch");
        p = base;
        p.header.used_bytes += 8;
        reject(build(p, false), "used size mismatch");
        p = base;
        p.header.root_offset += 8;
        reject(build(p), "root offset mismatch");
        auto bytes = original;
        bytes.push_back('\0');
        reject(bytes, "used size mismatch");
    }

    // ---- Payload header ----------------------------------------------------
    {
        auto bytes = original;
        write(bytes, payload + offsetof(XBufferCore::arena_header, used_end), uint64_t(48));
        reject(bytes, "non-normalized payload extent");
        bytes = original;
        write(bytes, payload + offsetof(XBufferCore::arena_header, used_end), uint64_t(h.used_bytes - 8));
        reject(bytes, "non-normalized payload extent");
        bytes = original;
        write(bytes, payload + offsetof(XBufferCore::arena_header, used_end), uint64_t(h.used_bytes + 8));
        reject(bytes, "used_end out of range");
    }
    {
        auto bytes = original;
        write(bytes, payload + offsetof(XBufferCore::arena_header, root_offset), UINT64_MAX);
        reject(bytes, "root not found");
        bytes = original;
        bytes[payload + 5] = '1'; // XBUFV1 payload magic
        reject(bytes, "payload header mismatch");
        bytes = original;
        write(bytes, payload + offsetof(XBufferCore::arena_header, header_size), uint32_t(64));
        reject(bytes, "payload header mismatch");
    }
    // Replaces the V1 "root block header" mutation: the bytes immediately
    // before the root are now the payload header's own fields.
    for (std::size_t field : {offsetof(XBufferCore::arena_header, flags),
                              offsetof(XBufferCore::arena_header, reserved0),
                              offsetof(XBufferCore::arena_header, reserved1)}) {
        auto bytes = original;
        write(bytes, payload + field, uint32_t(1));
        reject(bytes, "wire validation: unsupported arena header flags");
    }

    // ---- Hole table entries (replace V1 free-list head/cycle mutations) ----
    for (auto bad : {uint32_t(1), uint32_t(4), uint32_t(0), uint32_t(40),
                     uint32_t(h.used_bytes), uint32_t(0x7ffffff8), UINT32_MAX}) {
        auto p = base;
        p.holes[0].offset = bad; // unaligned, inside the payload header, or past used_end
        reject(build(p), kHoleEntry);
    }
    for (auto bad : {uint32_t(0), uint32_t(4), uint32_t(12), uint32_t(h.used_bytes), UINT32_MAX - 7}) {
        auto p = base;
        p.holes[0].bytes = bad; // empty, not a multiple of 8, or past used_end
        reject(build(p), kHoleEntry);
    }
    {   // An entry ending one block past used_end.
        auto p = base;
        p.holes[0].offset = static_cast<uint32_t>(h.used_bytes - 8);
        p.holes[0].bytes = 16;
        reject(build(p), kHoleEntry);
    }
    {   // Duplicate entry and partially overlapping entries.
        auto p = base;
        p.holes.push_back(hole);
        reject(build(p), kFreeFree);
        p = base;
        p.holes[0].bytes = 8;
        p.holes.push_back({hole.offset, 16});
        reject(build(p), kFreeFree);
    }
    {   // A hole removed: its bytes become an unreachable region.
        auto p = base;
        p.holes.clear();
        reject(build(p), kCoverage);
        p = base;
        p.holes[0].bytes = 8; // half of the retired block left unaccounted
        reject(build(p), kCoverage);
    }
    {   // Splitting the hole into two exact entries (any order) is equivalent.
        auto p = base;
        p.holes = {{hole.offset + 8, 8}, {hole.offset, 8}};
        accept(build(p), "split hole table must remain valid");
    }
    {   // A hole overlapping a live block: the root, a heap string, vector blocks.
        const auto name_block = head_target(base.payload, proot + offsetof(CheckedRoot, name));
        const auto labels_block = head_target(base.payload, proot + offsetof(CheckedRoot, labels));
        const auto values_block = head_target(base.payload, proot + offsetof(CheckedRoot, values));
        for (auto live : {std::size_t(proot), name_block, labels_block, values_block, values_block + 56}) {
            auto p = base;
            p.holes.push_back({static_cast<uint32_t>(live), 8});
            reject(build(p), kLiveFree);
        }
        auto p = base; // moving the only hole onto a live block also uncovers its own bytes
        p.holes[0].offset = static_cast<uint32_t>(name_block);
        reject(build(p), kLiveFree);
    }
    {   // An extra region past the old used_end: rejected without a hole,
        // accepted once the hole table covers it exactly.
        auto p = base;
        p.payload.append(16, '\0');
        write(p.payload, offsetof(XBufferCore::arena_header, used_end), uint64_t(p.payload.size()));
        reject(build(p), kCoverage);
        auto covered = p;
        covered.holes.push_back({static_cast<uint32_t>(h.used_bytes), 16});
        accept(build(covered), "tail region described by a hole must be accepted");
        auto partial = p;
        partial.holes.push_back({static_cast<uint32_t>(h.used_bytes), 8});
        reject(build(partial), kCoverage);
    }

    // ---- Scalars and ordering ---------------------------------------------
    for (auto offset : {offsetof(CheckedRoot, enabled), offsetof(CheckedRoot, bits) + 1}) {
        auto bytes = original;
        write(bytes, root + offset, uint8_t(2));
        reject(bytes, "wire validation: invalid bool representation");
    }
    {
        auto bytes = original;
        const auto origin = root + offsetof(CheckedRoot, map) + offsetof(XFixedVector<int32_t>, data_delta);
        const auto data = origin + read<int32_t>(bytes, origin);
        write(bytes, data + sizeof(XKeyValue<int32_t, int32_t>), int32_t(1));
        reject(bytes, "wire validation: map keys not strictly ordered");
    }

    // ---- Container heads: arena_delta -------------------------------------
    {
        auto bytes = original;
        write(bytes, root + offsetof(CheckedRoot, labels) + offsetof(XFixedString, arena_delta), INT32_MAX);
        reject(bytes, kRelative);
        const auto at = proot + offsetof(CheckedRoot, values) + offsetof(XFixedVector<int32_t>, arena_delta);
        for (std::int32_t delta : {0, 8, 48, -8}) {
            auto p = base;
            write(p.payload, at, delta == 0 ? 0 : read<std::int32_t>(p.payload, at) + delta);
            // Must resolve exactly to the payload base; below the base is out of bounds.
            reject(build(p), delta < 0 ? kRelative : kBinding);
        }
        auto p = base; // an inline string's binding is checked too
        const auto label0 = head_target(base.payload, proot + offsetof(CheckedRoot, labels));
        const auto label_at = label0 + offsetof(XFixedString, arena_delta);
        write(p.payload, label_at, read<std::int32_t>(p.payload, label_at) + 16);
        reject(build(p), kBinding);
    }

    // ---- Container heads: data_delta --------------------------------------
    {
        auto bytes = original; // values aliases the root block
        const auto vec = root + offsetof(CheckedRoot, values);
        const auto origin = vec + offsetof(XFixedVector<int32_t>, data_delta);
        write(bytes, origin, static_cast<int32_t>(root - origin));
        write(bytes, vec + offsetof(XFixedVector<int32_t>, capacity_), uint32_t(1));
        reject(bytes, kLiveLive);
    }
    {
        const auto name_head = proot + offsetof(CheckedRoot, name);
        const auto values_head = proot + offsetof(CheckedRoot, values);
        const auto label2_head = head_target(base.payload, proot + offsetof(CheckedRoot, labels)) + 2 * sizeof(XString);
        auto p = base; // name now shares the heap label's block
        set_head_target(p.payload, name_head, head_target(p.payload, label2_head));
        write(p.payload, name_head + offsetof(XFixedString, capacity_), uint32_t(20));
        write(p.payload, name_head + offsetof(XFixedString, size_), uint32_t(20));
        reject(build(p), kLiveLive);
        p = base; // values moved onto the hole (derived extent: 16 * 4 bytes)
        set_head_target(p.payload, values_head, hole.offset);
        reject(build(p), kLiveFree);
        p = base; // values moved by one block inside its own extent
        set_head_target(p.payload, values_head, head_target(base.payload, values_head) + 8);
        reject(build(p));
        p = base; // values moved to an address that is not 8-aligned
        set_head_target(p.payload, values_head, head_target(base.payload, values_head) + 4);
        write(p.payload, values_head + offsetof(XFixedVector<int32_t>, capacity_), uint32_t(4)); // stays in bounds
        reject(build(p), "wire validation: allocation out of bounds");
        // Relabelling the hole as live is valid only together with the old
        // block becoming the hole.
        p = base;
        const auto old_values = head_target(base.payload, values_head);
        set_head_target(p.payload, values_head, hole.offset);
        write(p.payload, values_head + offsetof(XFixedVector<int32_t>, capacity_), uint32_t(4));
        std::memcpy(p.payload.data() + hole.offset, base.payload.data() + old_values, 16);
        reject(build(p), kLiveFree);
        p.holes = {{static_cast<uint32_t>(old_values), 64}};
        accept(build(p), "exchanging a live block and a hole consistently must be valid");
    }

    // ---- Container heads: capacity_ / size_ (derived extents) -------------
    {
        const auto values_head = proot + offsetof(CheckedRoot, values);
        const auto name_head = proot + offsetof(CheckedRoot, name);
        auto p = base; // smaller capacity_: the tail of the real block is unaccounted
        write(p.payload, values_head + offsetof(XFixedVector<int32_t>, capacity_), uint32_t(8));
        reject(build(p), kCoverage);
        p = base; // capacity rounding: 15 * 4 = 60 -> 64, the same derived extent
        write(p.payload, values_head + offsetof(XFixedVector<int32_t>, capacity_), uint32_t(15));
        accept(build(p), "capacity with an identical derived extent must be valid");
        p = base; // larger capacity_: the derived block runs past its real extent
        write(p.payload, values_head + offsetof(XFixedVector<int32_t>, capacity_), uint32_t(18));
        reject(build(p));
        p = base;
        write(p.payload, values_head + offsetof(XFixedVector<int32_t>, size_), uint32_t(17));
        reject(build(p), "wire validation failed: XFixedVector size exceeds capacity");
        p = base;
        write(p.payload, values_head + offsetof(XFixedVector<int32_t>, capacity_), uint32_t(0));
        write(p.payload, values_head + offsetof(XFixedVector<int32_t>, size_), uint32_t(0));
        reject(build(p), "wire validation failed: empty XFixedVector has invalid payload");
        p = base; // string capacity 30 -> 17: block 32 -> 24 bytes, 8 bytes unaccounted
        write(p.payload, name_head + offsetof(XFixedString, capacity_), uint32_t(17));
        reject(build(p), kCoverage);
        p = base; // string capacity 30 -> 24: round8(25) == round8(31) == 32
        write(p.payload, name_head + offsetof(XFixedString, capacity_), uint32_t(24));
        accept(build(p), "string capacity with an identical derived extent must be valid");
        p = base;
        write(p.payload, name_head + offsetof(XFixedString, size_), uint32_t(31));
        reject(build(p), "wire validation failed: XFixedString size exceeds capacity");
        p = base; // terminator required at data[size_]
        write(p.payload, name_head + offsetof(XFixedString, size_), uint32_t(16));
        reject(build(p), "wire validation failed: XFixedString missing terminator");
        p = base;
        write(p.payload, name_head + offsetof(XFixedString, capacity_), uint32_t(0x7ffffff8));
        reject(build(p), "wire validation failed: XFixedString size exceeds capacity");
        p = base;
        write(p.payload, name_head + offsetof(XFixedString, capacity_), uint32_t(0));
        write(p.payload, name_head + offsetof(XFixedString, size_), uint32_t(0));
        reject(build(p), "wire validation failed: empty XFixedString has invalid payload");
    }

    // ---- Inline (I1) string bytes -----------------------------------------
    {
        const auto label0 = head_target(base.payload, proot + offsetof(CheckedRoot, labels));
        const auto label1 = label0 + sizeof(XString);
        auto p = base; // length byte 11 > inline_max
        p.payload[label1 + 15] = static_cast<char>(0x80 | 11);
        reject(build(p), "wire validation failed: inline XFixedString length exceeds 10");
        p = base;
        p.payload[label1 + 15] = static_cast<char>(0xff);
        reject(build(p), "wire validation failed: inline XFixedString length exceeds 10");
        p = base; // terminator (data[len]) must be zero: "array-one" has len 9
        p.payload[label1 + 4 + 9] = 'x';
        reject(build(p), "wire validation failed: inline XFixedString has nonzero padding");
        p = base; // last padding byte (head byte 14)
        p.payload[label1 + 14] = 1;
        reject(build(p), "wire validation failed: inline XFixedString has nonzero padding");
        p = base; // a 10-byte inline string still needs data[10] == 0
        p.payload[label0 + 14] = 1;
        reject(build(p), "wire validation failed: inline XFixedString has nonzero padding");
        p = base; // shorter length: the cut-off characters become nonzero padding
        p.payload[label0 + 15] = static_cast<char>(0x80 | 5);
        reject(build(p), "wire validation failed: inline XFixedString has nonzero padding");
        p = base; // longer length over zero padding is a well-formed (different) value
        p.payload[label1 + 15] = static_cast<char>(0x80 | 10);
        {
            auto b = XBuffer::load_verified<CheckedRoot>(build(p));
            need(b.root<CheckedRoot>().labels[1].size() == 10, "inline length change not observed");
        }
        p = base; // clearing the inline flag reinterprets the chars as a heap head
        p.payload[label1 + 15] = static_cast<char>(9);
        reject(build(p));
        // Every length 0..10 against every byte position 0..10: a nonzero
        // byte at or past the length is padding (rejected), one before it a
        // character (accepted).
        for (unsigned length = 0; length <= 10; ++length) {
            for (unsigned at = 0; at < 11; ++at) {
                p = base;
                for (unsigned i = 0; i < 11; ++i) p.payload[label1 + 4 + i] = i < length ? 'a' : 0;
                p.payload[label1 + 15] = static_cast<char>(0x80 | length);
                p.payload[label1 + 4 + at] = 1;
                if (at >= length) {
                    reject(build(p), "wire validation failed: inline XFixedString has nonzero padding");
                } else {
                    auto b = XBuffer::load_verified<CheckedRoot>(build(p));
                    const auto& label = b.root<CheckedRoot>().labels[1];
                    need(label.size() == length && label.c_str()[at] == 1, "inline character position rejected");
                }
            }
        }
    }

    // ---- Sparse wire (extent_count != 0) -----------------------------------
    // Zero-run elision skips zero runs (here the retired 8 KiB block and the
    // slack of the values block): header | extent table | concatenated
    // extents | hole table. Gap bytes load as zero wherever they lie. A gap
    // is never by itself a reason to reject: this is a relaxed encoding rule,
    // not a relaxed payload check (K4). A sparse wire is accepted exactly
    // when the dense wire of the same loaded payload is, with the same error.
    {
        auto sbuf = XBuffer::create<CheckedRoot>(65536);
        auto& s = sbuf.root<CheckedRoot>();
        s.id = 42;
        s.values.reserve(2048);  // 8 KiB block
        s.values.push_back(5);
        s.map.reserve(4);        // barrier directly after it
        s.map.try_emplace(1, 11);
        s.map.try_emplace(2, 22);
        s.values.reserve(4096);  // retires the 8 KiB block
        const XSaveOptions elide{.emission = XWireEmission::zero_runs};
        const XSaveOptions dense_only{.emission = XWireEmission::dense};
        const auto dense = sbuf.save_verified<CheckedRoot>(dense_only);
        const auto wire = sbuf.save_verified<CheckedRoot>(elide);
        const auto sh = read<XWireHeaderV2>(dense, 0);
        const auto wh = read<XWireHeaderV2>(wire, 0);
        need(sh.extent_count == 0 && sh.hole_count == 1 && sh.hole_bytes == 8192, "dense fixture layout");
        need(wh.extent_count >= 2 && wh.used_bytes == sh.used_bytes && wh.hole_count == 1 &&
             wh.hole_bytes == 8192 && wire.size() + 8192 < dense.size(), "sparse fixture layout");
        const std::size_t table = sizeof(XWireHeaderV2);
        const std::size_t count = static_cast<std::size_t>(wh.extent_count);
        std::vector<Hole> ext;
        std::size_t stored = 0;
        for (std::size_t i = 0; i < count; ++i) {
            ext.push_back(read<Hole>(wire, table + i * 8));
            stored += ext.back().bytes;
            need(i == 0 ? ext[i].offset == 0 : ext[i].offset > ext[i - 1].offset + ext[i - 1].bytes,
                 "sparse fixture extents");
        }
        need(wire.size() == table + count * 8 + stored + sizeof(Hole), "sparse fixture size");
        // Inflates a sparse wire with a well-formed extent table to the
        // dense wire of the payload it loads (gap bytes zero).
        auto inflate = [&](const std::string& sparse) {
            const auto h = read<XWireHeaderV2>(sparse, 0);
            Parts p;
            p.header = h;
            p.header.extent_count = 0;
            p.payload.assign(static_cast<std::size_t>(h.used_bytes), '\0');
            std::size_t at = table + std::size_t(h.extent_count) * 8;
            for (std::size_t i = 0; i < h.extent_count; ++i) {
                const auto e = read<Hole>(sparse, table + i * 8);
                need(e.offset + std::size_t(e.bytes) <= p.payload.size() && at + e.bytes <= sparse.size(),
                     "inflate: extent out of range");
                std::memcpy(p.payload.data() + e.offset, sparse.data() + at, e.bytes);
                at += e.bytes;
            }
            std::string out = build(p, false);
            out.append(sparse, at, std::string::npos);
            return out;
        };
        auto outcome = [](const std::string& bytes) -> std::string {
            try { (void)XBuffer::load_verified<CheckedRoot>(bytes); return "accepted"; }
            catch (const XException& e) { return e.what(); }
        };
        need(inflate(wire) == dense, "sparse wire does not load the dense payload");
        {   // The default (free-run) emission skips exactly the 8 KiB hole.
            const auto runs = sbuf.save_verified<CheckedRoot>();
            const auto rh = read<XWireHeaderV2>(runs, 0);
            need(rh.extent_count == 2 && runs.size() == dense.size() - 8192 + 16 && inflate(runs) == dense,
                 "free-run emission did not skip exactly the 8 KiB hole");
            accept(runs, "free-run wire must load");
        }
        accept(wire, "sparse wire must load");
        {
            auto b = XBuffer::load_verified<CheckedRoot>(wire);
            const auto& v = b.root<CheckedRoot>();
            need(v.values.size() == 1 && v.values[0] == 5 && v.map.size() == 2 && v.map.find(2)->second == 22,
                 "sparse wire values differ");
            need(b.save_verified<CheckedRoot>(elide) == wire, "sparse write-read-write is not byte-stable");
            need(b.save_verified<CheckedRoot>(dense_only) == dense, "sparse load -> dense save differs from the dense wire");
            std::string into;
            b.save_verified_into<CheckedRoot>(into, elide);
            need(into == wire, "save_verified_into differs from save_verified for a sparse wire");
        }
        auto with_extent = [&](std::size_t index, Hole e) {
            auto bytes = wire;
            write(bytes, table + index * 8, e);
            return bytes;
        };
        const auto last = ext.back();
        reject(with_extent(0, {8, ext[0].bytes - 8}), "invalid extent table");            // first extent not at 0
        reject(with_extent(0, {0, ext[0].bytes - 4}), "invalid extent table");            // not a multiple of 8
        reject(with_extent(count - 1, {last.offset, std::uint32_t(sh.used_bytes - last.offset + 8)}),
               "invalid extent table");                                                    // past used_bytes
        reject(with_extent(1, {ext[0].bytes - 8, ext[1].bytes}), "invalid extent table");  // overlaps extent 0
        reject(with_extent(1, {ext[1].offset, 0}), "invalid extent table");
        {
            auto bytes = wire;
            auto h2 = wh;
            h2.extent_count = 0; // the extent table is not payload
            write(bytes, 0, h2);
            reject(bytes, "used size mismatch");
        }
        // Gaps over live bytes: an extent loses its first or last 8 bytes,
        // or one 8-byte word inside an extent is cut out into a new gap.
        // Each loads those bytes as zero; the outcome must equal the dense
        // wire's. Some of these payloads are valid and some are not.
        std::size_t gap_cases = 0, gap_accepted = 0;
        auto shrink = [&](std::size_t index, bool front) {
            auto e = ext[index];
            if (e.bytes < 16 || (front && index == 0)) return;
            auto bytes = wire;
            const std::size_t data = table + count * 8;
            std::size_t at = data;
            for (std::size_t i = 0; i < index; ++i) at += ext[i].bytes;
            bytes.erase(front ? at : at + e.bytes - 8, 8);
            write(bytes, table + index * 8, Hole{front ? e.offset + 8 : e.offset, e.bytes - 8});
            const auto got = outcome(bytes), want = outcome(inflate(bytes));
            if (got != want) {
                std::cerr << "sparse '" << got << "' vs dense '" << want << "'\n";
                throw std::runtime_error("sparse gap outcome differs from its dense payload");
            }
            ++gap_cases;
            gap_accepted += got == "accepted";
        };
        for (std::size_t i = 0; i < count; ++i) { shrink(i, true); shrink(i, false); }
        for (std::size_t word = 8; word + 8 < ext[0].bytes; word += 8) {
            // Split extent 0 around one word: {0, word} gap {word + 8, rest}.
            auto bytes = wire;
            std::string t;
            const Hole a{0, std::uint32_t(word)}, b{std::uint32_t(word + 8), ext[0].bytes - std::uint32_t(word + 8)};
            t.append(reinterpret_cast<const char*>(&a), 8);
            t.append(reinterpret_cast<const char*>(&b), 8);
            bytes.replace(table, 8, t);
            bytes.erase(table + (count + 1) * 8 + word, 8);
            auto h2 = wh;
            h2.extent_count = count + 1;
            write(bytes, 0, h2);
            const auto got = outcome(bytes), want = outcome(inflate(bytes));
            if (got != want) {
                std::cerr << "word " << word << ": sparse '" << got << "' vs dense '" << want << "'\n";
                throw std::runtime_error("sparse gap outcome differs from its dense payload");
            }
            ++gap_cases;
            gap_accepted += got == "accepted";
        }
        need(gap_cases > 8 && gap_accepted > 0 && gap_accepted < gap_cases,
             "sparse gap cases must include accepted and rejected payloads");
        {   // The hole shrinks: coverage fails first, sparse or dense.
            auto bytes = wire;
            const auto shole = read<Hole>(wire, wire.size() - sizeof(Hole));
            write(bytes, wire.size() - sizeof(Hole), Hole{shole.offset + 8, shole.bytes - 8});
            auto h2 = wh;
            h2.hole_bytes -= 8;
            write(bytes, 0, h2);
            reject(bytes, kCoverage);
        }
    }

    // Some mutations affect legitimate scalar values or padding. Accepted
    // images must remain safely mutable, not merely readable once.
    std::size_t rejected = 0, accepted = 0;
    for (std::size_t i = 0; i < original.size(); ++i) {
        for (unsigned char mask : {1u, 128u}) {
            auto bytes = original;
            bytes[i] ^= mask;
            std::optional<XBuffer> candidate;
            try {
                candidate.emplace(XBuffer::load_verified<CheckedRoot>(bytes));
            } catch (const XException&) { ++rejected; continue; }
            assert(candidate->grow(8192));
            auto& value = candidate->root<CheckedRoot>();
            value.name = "still-mutable";
            value.labels.push_back("array-mutated");
            value.values.reserve(200);
            value.values.push_back(100);
            value.map.try_emplace(3, 33);
            auto again = XBuffer::load_verified<CheckedRoot>(candidate->save_verified<CheckedRoot>());
            assert(again.root<CheckedRoot>().values.back() == 100);
            ++accepted;
        }
    }
    assert(rejected > 0 && accepted > 0);
    std::cout << "Wire mutation sweep: " << rejected << " rejected, " << accepted << " mutable; "
              << precise_rejections << " targeted V2 rejections\n";
    return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
