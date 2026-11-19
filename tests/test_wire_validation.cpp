// Purpose: Reject hostile offsets, container heads, free blocks and image
// metadata before dereferencing or mutation (wire format 3.0: a dense wire is
// the image [0, used_bytes), its 40-byte header first and the root at 40; the
// free directory and the free blocks are image bytes; the header, the live
// blocks and the free blocks must tile the image exactly).
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
XOFFSET_RECORD_DERIVED(CheckedRoot, (), id, enabled, bits, name, values, map, labels)
XOFFSET_REGISTER_SCHEMA_NAME(CheckedRoot, "test.CheckedRoot")

namespace {
using Hole = XBufferCore::WireHole;
constexpr const char* kCoverage =
    "wire validation failed: payload is not exactly covered by live blocks and holes";
constexpr const char* kLiveFree = "wire validation detected live/free overlap";
constexpr const char* kLiveLive = "wire validation detected overlapping dynamic ranges";
// The full check walks the free directory and its stacks first.
constexpr const char* kFree = "load_verified<T>(): invalid free blocks";

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

// A dense wire: its header and its image [0, used_bytes), indexed by image
// offset (the image's first 40 bytes are the header, rewritten by build()).
// build() sets used_bytes to the image's size unless asked to keep a
// deliberately bad header.
constexpr std::size_t kBegin = XWIRE_PAYLOAD_BEGIN;
struct Parts {
    XWireHeader header{};
    std::string image;
};
Parts parse(const std::string& wire) {
    Parts p;
    p.header = read<XWireHeader>(wire, 0);
    need(p.header.extent_count == 0, "fixture wire is sparse; Parts handles dense wires only");
    need(wire.size() == p.header.used_bytes, "fixture wire is not the image");
    p.image = wire;
    return p;
}
std::string build(Parts p, bool fix_counts = true) {
    if (fix_counts) p.header.used_bytes = static_cast<std::uint32_t>(p.image.size());
    std::memcpy(p.image.data(), &p.header, sizeof(p.header));
    return p.image;
}

void accept(const std::string& bytes, const char* what) {
    try {
        auto b = XBuffer::load_verified<CheckedRoot>(bytes);
        need(b.root<CheckedRoot>().id == 42, what);
        // Accepted images must stay mutable and re-save as valid wires.
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
    const auto origin = head_offset + offsetof(XString, data_delta);
    return static_cast<std::size_t>(static_cast<std::int64_t>(origin) + read<std::int32_t>(payload, origin));
}
void set_head_target(std::string& payload, std::size_t head_offset, std::size_t target) {
    const auto origin = head_offset + offsetof(XString, data_delta);
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
    r.values.reserve(16); // The retired allocation becomes the one free block.
    r.map.try_emplace(1, 11);
    r.map.try_emplace(2, 22);
    const auto original = buf.save_verified<CheckedRoot>();
    const auto base = parse(original);
    const auto& h = base.header;
    // The free directory {n, cap, cap x {size, head}} names one size, 16,
    // whose stack is the retired block.
    const std::uint32_t dir = h.free_dir;
    need(dir != 0 && h.free_bytes == 16 && read<std::uint32_t>(original, dir) == 1 &&
         read<std::uint32_t>(original, dir + 8) == 16, "fixture must have one 16-byte free block");
    const std::uint32_t dir_cap = read<std::uint32_t>(original, dir + 4);
    const Hole hole{read<std::uint32_t>(original, dir + 12), 16};
    need(read<std::uint64_t>(original, hole.offset) == 0, "fixture block is the only one of its size");
    need(build(base) == original, "fixture split/rebuild is not byte-identical");
    const std::size_t root = kBegin;   // wire offset of the root (a dense wire is the image)
    const std::size_t proot = kBegin;  // root offset inside Parts::image
    auto loaded = XBuffer::load_verified<CheckedRoot>(original);
    assert(loaded.root<CheckedRoot>().id == 42);
    assert(loaded.root<CheckedRoot>().labels[2].view() == "array-two-heap-label");

    // ---- Wire header -------------------------------------------------------
    {   // V1, V2.0 and 2.1 input are rejected by version, not as a bad magic.
        auto bytes = original;
        std::memcpy(bytes.data(), "XOFFV1\0\0", 8);
        reject(bytes, "wire format 1 is not supported; this build reads 3.0");
        // A complete V1-shaped wire (88-byte V1 header + XBUFV1 payload).
        LegacyWireHeaderV1 v1{};
        std::memcpy(v1.magic, "XOFFV1\0\0", sizeof(v1.magic));
        v1.format_major = 1;
        v1.header_size = sizeof(v1);
        v1.used_bytes = base.image.size();
        v1.root_offset = kBegin;
        v1.root_type_id = h.root_type_id;
        v1.schema_hash_lo = h.schema_hash;
        v1.endian_tag = 0x01020304u;
        auto legacy_payload = base.image;
        std::memcpy(legacy_payload.data(), "XBUFV1\0\0", 8);
        const auto legacy = std::string(reinterpret_cast<const char*>(&v1), sizeof(v1)) + legacy_payload;
        reject(legacy, "wire format 1 is not supported; this build reads 3.0");
        // A V2.0 wire: 96-byte header (magic XOFFV2, format 2.0) + the whole payload.
        std::string v20(96, '\0');
        std::memcpy(v20.data(), "XOFFV2\0\0", 8);
        write(v20, 8, std::uint16_t(2));
        write(v20, 12, std::uint32_t(96));
        reject(v20 + base.image, "wire format 2.0 is not supported; this build reads 3.0 "
                                 "(sender and receiver must upgrade together)");
        // A 2.1 wire (4.1 and earlier): 'X' 'O' 2 1.
        auto p = base;
        p.header.magic = 0x4F58u | (2u << 16) | (1u << 24);
        reject(build(p), "wire format 2.1 is not supported; this build reads 3.0 "
                         "(sender and receiver must upgrade together)");
    }
    {   // Bytes 0-1 are 'X' 'O', bytes 2-3 the major and minor version.
        auto p = base;
        p.header.magic = XWIRE_MAGIC + (1u << 24);
        reject(build(p), "wire format 3.1 is not supported; this build reads 3.0");
        p = base;
        p.header.magic = XWIRE_MAGIC + (1u << 16);
        reject(build(p), "wire format 4.0 is not supported; this build reads 3.0");
        p = base;
        p.header.magic ^= 0x20u; // 'x' 'O'
        reject(build(p), "not an XOffset wire (bad magic)");
        reject(std::string(), "not an XOffset wire (bad magic)");
        reject(original.substr(0, 3), "not an XOffset wire (bad magic)");
        reject(original.substr(0, sizeof(XWireHeader) - 1), "truncated wire header");
    }
    for (std::uint32_t size : {0u, 48u, 0x80000028u}) {
        // The header size is 40; any other is a header this build cannot read.
        auto p = base;
        p.header.header_bytes = size;
        const auto expected = size == 0u ? "unsupported header size 0x00000000"
                            : size == 48u ? "unsupported header size 0x00000030"
                                          : "unsupported header size 0x80000028";
        reject(build(p), expected);
    }
    {   // The fingerprint: root type id (type name) and schema hash (name and ABI).
        auto p = base;
        p.header.root_type_id ^= 1;
        reject(build(p), "root type id mismatch");
        p = base;
        p.header.schema_hash ^= 1;
        reject(build(p), "schema hash mismatch");
    }
    {   // A dense wire relabelled as sparse: the image bytes are not an extent table.
        auto p = base;
        p.header.extent_count = 1;
        reject(build(p, false), "invalid extent table");
        p.header.extent_count = UINT32_MAX;
        reject(build(p, false), "used size mismatch");
    }
    {   // used_bytes and the total length.
        auto p = base;
        p.header.used_bytes += 8;
        reject(build(p, false), "used size mismatch");
        p = base;
        p.header.used_bytes -= 4; // not a multiple of 8
        reject(build(p, false), "used size mismatch");
        p = base;
        p.header.used_bytes = 32; // less than the header
        reject(build(p, false), "used size mismatch");
        p = base; // the header alone: no root
        p.image.resize(kBegin);
        reject(build(p), "root offset out of bounds");
        p = base; // the root does not fit
        p.image.resize(kBegin + 8);
        reject(build(p), "root not found");
        auto bytes = original;
        bytes.push_back('\0');
        reject(bytes, "used size mismatch");
    }

    // ---- The image header is the wire header ------------------------------
    need(std::memcmp(loaded.arena(), original.data(), sizeof(XWireHeader)) == 0,
         "the loaded image header differs from the wire header");

    // ---- Free directory and free blocks -----------------------------------
    const std::uint32_t used = h.used_bytes;
    const std::size_t entry = dir + 8, head = dir + 12;
    for (std::uint32_t bad : {0u, 1u, 4u, 8u, 32u, used, used - 8u, 0x7ffffff8u, UINT32_MAX}) {
        auto p = base; // the stack's head: none, unaligned, in the header, past or across used
        write(p.image, head, bad);
        reject(build(p), kFree);
    }
    for (std::uint32_t bad : {0u, 8u, 12u, 24u, UINT32_MAX}) {
        auto p = base; // the size: none, too small or large for free_bytes, unaligned, the unused mark
        write(p.image, entry, bad);
        reject(build(p), kFree);
    }
    for (std::uint32_t bad : {8u, 16u, 32u, dir + 4u, used, 0x7ffffff8u}) {
        auto p = base; // the directory: in the header, unaligned, past used
        p.header.free_dir = bad;
        reject(build(p), kFree);
    }
    {
        auto p = base; // free bytes without a directory
        p.header.free_dir = 0;
        reject(build(p), kFree);
        for (std::uint32_t bad : {0u, 8u, 24u}) {
            p = base;
            p.header.free_bytes = bad;
            reject(build(p), kFree);
        }
        p = base; // no unused entry (n == cap), or none at all (cap 1)
        write(p.image, dir, dir_cap);
        reject(build(p), kFree);
        p = base;
        write(p.image, dir + 4, 1u);
        reject(build(p), kFree);
        p = base; // an unused entry that names a size
        write(p.image, dir + 16, 64u);
        reject(build(p), kFree);
        p = base; // the block's second word, then a link to itself and to the directory
        write(p.image, hole.offset + 4, 1u);
        reject(build(p), kFree);
        p = base;
        write(p.image, hole.offset, hole.offset);
        reject(build(p), kFree);
        p = base;
        write(p.image, hole.offset, dir);
        reject(build(p), kFree);
    }
    {   // The block forgotten: its bytes are covered by nothing.
        auto p = base;
        write(p.image, head, 0u);
        p.header.free_bytes = 0;
        reject(build(p), kCoverage);
    }
    {   // A free block over a live one (the root, a heap string, vector blocks).
        const auto values_block = head_target(base.image, proot + offsetof(CheckedRoot, values));
        auto p = base; // the values block's zero slack named as the free block
        write(p.image, head, std::uint32_t(values_block + 8));
        reject(build(p), kLiveFree);
        p = base; // a second 16-byte block on the stack, at the root
        write(p.image, hole.offset, std::uint32_t(proot));
        p.header.free_bytes = 32;
        reject(build(p));
    }
    {   // An extra region past the old used_end: rejected until a free block
        // in the directory covers it exactly.
        auto p = base;
        p.image.append(16, '\0');
        reject(build(p), kCoverage);
        auto covered = p; // pushed on the 16-byte stack
        write(covered.image, used, hole.offset);
        write(covered.image, head, used);
        covered.header.free_bytes += 16;
        accept(build(covered), "tail region on a stack must be accepted");
        auto other = base; // a size the directory has no entry for yet: a new entry
        other.image.append(24, '\0');
        write(other.image, dir, 2u);
        write(other.image, dir + 16, 24u);
        write(other.image, dir + 20, used);
        other.header.free_bytes += 24;
        accept(build(other), "tail region with a new directory entry must be accepted");
        auto unsorted = other; // the entries' sizes must ascend
        write(unsorted.image, dir + 16, 8u);
        reject(build(unsorted), kFree);
        auto partial = p; // 8 of the 16 bytes on an 8-byte stack
        write(partial.image, dir, 2u);
        write(partial.image, dir + 8, 8u);
        write(partial.image, dir + 12, used);
        write(partial.image, dir + 16, 16u);
        write(partial.image, dir + 20, hole.offset);
        partial.header.free_bytes += 8;
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
        const auto origin = root + offsetof(CheckedRoot, map) + offsetof(XVectorCore<int32_t>, data_delta);
        const auto data = origin + read<int32_t>(bytes, origin);
        write(bytes, data + sizeof(XKeyValue<int32_t, int32_t>), int32_t(1));
        reject(bytes, "wire validation: map keys not strictly ordered");
    }

    // ---- Container heads: data_delta --------------------------------------
    {
        auto bytes = original; // values aliases the root block
        const auto vec = root + offsetof(CheckedRoot, values);
        const auto origin = vec + offsetof(XVectorCore<int32_t>, data_delta);
        write(bytes, origin, static_cast<int32_t>(root - origin));
        write(bytes, vec + offsetof(XVectorCore<int32_t>, capacity_), uint32_t(1));
        reject(bytes, kLiveLive);
    }
    {
        const auto name_head = proot + offsetof(CheckedRoot, name);
        const auto values_head = proot + offsetof(CheckedRoot, values);
        const auto label2_head = head_target(base.image, proot + offsetof(CheckedRoot, labels)) + 2 * sizeof(XString);
        auto p = base; // name now shares the heap label's block
        set_head_target(p.image, name_head, head_target(p.image, label2_head));
        write(p.image, name_head + offsetof(XString, capacity_), uint32_t(20));
        write(p.image, name_head + offsetof(XString, size_), uint32_t(20));
        reject(build(p), kLiveLive);
        p = base; // values moved onto the free block (derived extent: 16 * 4 bytes)
        set_head_target(p.image, values_head, hole.offset);
        reject(build(p), kLiveFree);
        p = base; // values moved by one block inside its own extent
        set_head_target(p.image, values_head, head_target(base.image, values_head) + 8);
        reject(build(p));
        p = base; // values moved to an address that is not 8-aligned
        set_head_target(p.image, values_head, head_target(base.image, values_head) + 4);
        write(p.image, values_head + offsetof(XVectorCore<int32_t>, capacity_), uint32_t(4)); // stays in bounds
        reject(build(p), "wire validation: allocation out of bounds");
        // Relabelling the free block as live is valid only together with the
        // old block becoming the free block (zeroed, on the directory).
        p = base;
        const auto old_values = head_target(base.image, values_head);
        set_head_target(p.image, values_head, hole.offset);
        write(p.image, values_head + offsetof(XVectorCore<int32_t>, capacity_), uint32_t(4));
        std::memcpy(p.image.data() + hole.offset, base.image.data() + old_values, 16);
        reject(build(p)); // the directory still names the old free block
        std::memset(p.image.data() + old_values, 0, 64);
        write(p.image, entry, 64u);
        write(p.image, head, std::uint32_t(old_values));
        p.header.free_bytes = 64;
        accept(build(p), "exchanging a live block and a free block consistently must be valid");
    }

    // ---- Container heads: capacity_ / size_ (derived extents) -------------
    {
        const auto values_head = proot + offsetof(CheckedRoot, values);
        const auto name_head = proot + offsetof(CheckedRoot, name);
        auto p = base; // smaller capacity_: the tail of the real block is unaccounted
        write(p.image, values_head + offsetof(XVectorCore<int32_t>, capacity_), uint32_t(8));
        reject(build(p), kCoverage);
        p = base; // capacity rounding: 15 * 4 = 60 -> 64, the same derived extent
        write(p.image, values_head + offsetof(XVectorCore<int32_t>, capacity_), uint32_t(15));
        accept(build(p), "capacity with an identical derived extent must be valid");
        p = base; // larger capacity_: the derived block runs past its real extent
        write(p.image, values_head + offsetof(XVectorCore<int32_t>, capacity_), uint32_t(18));
        reject(build(p));
        p = base;
        write(p.image, values_head + offsetof(XVectorCore<int32_t>, size_), uint32_t(17));
        reject(build(p), "wire validation failed: XVector size exceeds capacity");
        p = base;
        write(p.image, values_head + offsetof(XVectorCore<int32_t>, capacity_), uint32_t(0));
        write(p.image, values_head + offsetof(XVectorCore<int32_t>, size_), uint32_t(0));
        reject(build(p), "wire validation failed: empty XVector has invalid payload");
        p = base; // string capacity 30 -> 17: block 32 -> 24 bytes, 8 bytes unaccounted
        write(p.image, name_head + offsetof(XString, capacity_), uint32_t(17));
        reject(build(p), kCoverage);
        p = base; // string capacity 30 -> 24: round8(25) == round8(31) == 32
        write(p.image, name_head + offsetof(XString, capacity_), uint32_t(24));
        accept(build(p), "string capacity with an identical derived extent must be valid");
        p = base;
        write(p.image, name_head + offsetof(XString, size_), uint32_t(31));
        reject(build(p), "wire validation failed: XString size exceeds capacity");
        p = base; // terminator required at data[size_]
        write(p.image, name_head + offsetof(XString, size_), uint32_t(16));
        reject(build(p), "wire validation failed: XString missing terminator");
        p = base;
        write(p.image, name_head + offsetof(XString, capacity_), uint32_t(0x7ffffff8));
        reject(build(p), "wire validation failed: XString size exceeds capacity");
        p = base;
        write(p.image, name_head + offsetof(XString, capacity_), uint32_t(0));
        write(p.image, name_head + offsetof(XString, size_), uint32_t(0));
        reject(build(p), "wire validation failed: empty XString has invalid payload");
    }

    // ---- Inline (I1) string bytes -----------------------------------------
    {
        const auto label0 = head_target(base.image, proot + offsetof(CheckedRoot, labels));
        const auto label1 = label0 + sizeof(XString);
        auto p = base; // length byte 11 > inline_max
        p.image[label1 + 11] = static_cast<char>(0x80 | 11);
        reject(build(p), "wire validation failed: inline XString length exceeds 10");
        p = base;
        p.image[label1 + 11] = static_cast<char>(0xff);
        reject(build(p), "wire validation failed: inline XString length exceeds 10");
        p = base; // terminator (data[len]) must be zero: "array-one" has len 9
        p.image[label1 + 9] = 'x';
        reject(build(p), "wire validation failed: inline XString has nonzero padding");
        p = base; // last padding byte (head byte 10)
        p.image[label1 + 10] = 1;
        reject(build(p), "wire validation failed: inline XString has nonzero padding");
        p = base; // a 10-byte inline string still needs data[10] == 0
        p.image[label0 + 10] = 1;
        reject(build(p), "wire validation failed: inline XString has nonzero padding");
        p = base; // shorter length: the cut-off characters become nonzero padding
        p.image[label0 + 11] = static_cast<char>(0x80 | 5);
        reject(build(p), "wire validation failed: inline XString has nonzero padding");
        p = base; // longer length over zero padding is a well-formed (different) value
        p.image[label1 + 11] = static_cast<char>(0x80 | 10);
        {
            auto b = XBuffer::load_verified<CheckedRoot>(build(p));
            need(b.root<CheckedRoot>().labels[1].size() == 10, "inline length change not observed");
        }
        p = base; // clearing the inline flag reinterprets the chars as a heap head
        p.image[label1 + 11] = static_cast<char>(9);
        reject(build(p));
        // Every length 0..10 against every byte position 0..10: a nonzero
        // byte at or past the length is padding (rejected), one before it a
        // character (accepted).
        for (unsigned length = 0; length <= 10; ++length) {
            for (unsigned at = 0; at < 11; ++at) {
                p = base;
                for (unsigned i = 0; i < 11; ++i) p.image[label1 + i] = i < length ? 'a' : 0;
                p.image[label1 + 11] = static_cast<char>(0x80 | length);
                p.image[label1 + at] = 1;
                if (at >= length) {
                    reject(build(p), "wire validation failed: inline XString has nonzero padding");
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
    // extents. Gap bytes load as zero wherever they lie. A gap is never by
    // itself a reason to reject: this is a relaxed encoding rule, not a
    // relaxed payload check (K4). A sparse wire is accepted exactly when the
    // dense wire of the same loaded image is, with the same error.
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
        const auto sh = read<XWireHeader>(dense, 0);
        const auto wh = read<XWireHeader>(wire, 0);
        // The end of the retired block hosts the free directory; the rest is
        // the one free block.
        const std::uint32_t dir_bytes = sbuf.free_directory().bytes, freed = 8192 - dir_bytes;
        need(sh.extent_count == 0 && sh.used_bytes == dense.size() && sh.free_bytes == freed &&
             sh.free_dir == read<std::uint32_t>(dense, sh.free_dir + 12) + freed,
             "dense fixture layout");
        need(wh.extent_count >= 2 && wh.used_bytes == sh.used_bytes && wh.free_bytes == freed &&
             wire.size() + freed < dense.size(), "sparse fixture layout");
        const std::size_t table = sizeof(XWireHeader);
        const std::size_t count = static_cast<std::size_t>(wh.extent_count);
        std::vector<Hole> ext;
        std::size_t stored = 0;
        for (std::size_t i = 0; i < count; ++i) {
            ext.push_back(read<Hole>(wire, table + i * 8));
            stored += ext.back().bytes;
            need(i == 0 ? ext[i].offset == kBegin : ext[i].offset > ext[i - 1].offset + ext[i - 1].bytes,
                 "sparse fixture extents");
        }
        need(wire.size() == table + count * 8 + stored, "sparse fixture size");
        // Inflates a sparse wire with a well-formed extent table to the
        // dense wire of the image it loads (gap bytes zero).
        auto inflate = [&](const std::string& sparse) {
            const auto h = read<XWireHeader>(sparse, 0);
            Parts p;
            p.header = h;
            p.header.extent_count = 0;
            p.image.assign(static_cast<std::size_t>(h.used_bytes), '\0');
            std::size_t at = table + std::size_t(h.extent_count) * 8;
            for (std::size_t i = 0; i < h.extent_count; ++i) {
                const auto e = read<Hole>(sparse, table + i * 8);
                need(e.offset + std::size_t(e.bytes) <= p.image.size() && at + e.bytes <= sparse.size(),
                     "inflate: extent out of range");
                std::memcpy(p.image.data() + e.offset, sparse.data() + at, e.bytes);
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
        need(inflate(wire) == dense, "sparse wire does not load the dense image");
        {   // The default (free-run) emission skips the free block's body: all
            // of it but its link word.
            const auto runs = sbuf.save_verified<CheckedRoot>();
            const auto rh = read<XWireHeader>(runs, 0);
            need(rh.extent_count == 2 && runs.size() == dense.size() - (freed - 8) + 16 && inflate(runs) == dense,
                 "free-run emission did not skip exactly the free block's body");
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
        reject(with_extent(0, {48, ext[0].bytes - 8}), "invalid extent table");           // first extent not at 40
        reject(with_extent(0, {32, ext[0].bytes + 8}), "invalid extent table");           // inside the header
        reject(with_extent(0, {40, ext[0].bytes - 4}), "invalid extent table");           // not a multiple of 8
        reject(with_extent(count - 1, {last.offset, std::uint32_t(sh.used_bytes - last.offset + 8)}),
               "invalid extent table");                                                    // past used_bytes
        reject(with_extent(1, {ext[0].offset + ext[0].bytes - 8, ext[1].bytes}), "invalid extent table"); // overlaps extent 0
        reject(with_extent(1, {ext[1].offset, 0}), "invalid extent table");
        {
            auto bytes = wire;
            auto h2 = wh;
            h2.extent_count = 0; // the extent table is not image bytes
            write(bytes, 0, h2);
            reject(bytes, "used size mismatch");
        }
        // Gaps over live bytes: an extent loses its first or last 8 bytes,
        // or one 8-byte word inside an extent is cut out into a new gap.
        // Each loads those bytes as zero; the outcome must equal the dense
        // wire's. Some of these images are valid and some are not.
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
                throw std::runtime_error("sparse gap outcome differs from its dense image");
            }
            ++gap_cases;
            gap_accepted += got == "accepted";
        };
        for (std::size_t i = 0; i < count; ++i) { shrink(i, true); shrink(i, false); }
        for (std::size_t word = 8; word + 8 < ext[0].bytes; word += 8) {
            // Split extent 0 around one word: {40, word} gap {40 + word + 8, rest}.
            auto bytes = wire;
            std::string t;
            const Hole a{std::uint32_t(kBegin), std::uint32_t(word)},
                b{std::uint32_t(kBegin + word + 8), ext[0].bytes - std::uint32_t(word + 8)};
            t.append(reinterpret_cast<const char*>(&a), 8);
            t.append(reinterpret_cast<const char*>(&b), 8);
            bytes.replace(table, 8, t);
            bytes.erase(table + (count + 1) * 8 + word, 8);
            auto h2 = wh;
            h2.extent_count = static_cast<std::uint32_t>(count + 1);
            write(bytes, 0, h2);
            const auto got = outcome(bytes), want = outcome(inflate(bytes));
            if (got != want) {
                std::cerr << "word " << word << ": sparse '" << got << "' vs dense '" << want << "'\n";
                throw std::runtime_error("sparse gap outcome differs from its dense image");
            }
            ++gap_cases;
            gap_accepted += got == "accepted";
        }
        need(gap_cases >= 7 && gap_accepted > 0 && gap_accepted < gap_cases,
             "sparse gap cases must include accepted and rejected images");
        {   // The free block shrinks by 8 bytes (its size and free_bytes):
            // its last 8 bytes are covered by nothing, sparse or dense.
            auto bytes = wire;
            std::size_t pos = table + count * 8, dir_pos = 0;
            for (const auto& e : ext) {
                if (sh.free_dir >= e.offset && sh.free_dir + 16 <= e.offset + e.bytes)
                    dir_pos = pos + (sh.free_dir - e.offset);
                pos += e.bytes;
            }
            need(dir_pos != 0, "the free directory is on the sparse wire");
            write(bytes, dir_pos + 8, freed - 8);
            auto h2 = wh;
            h2.free_bytes = freed - 8;
            write(bytes, 0, h2);
            reject(bytes, kCoverage);
            reject(inflate(bytes), kCoverage);
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
              << precise_rejections << " targeted rejections\n";
    return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
