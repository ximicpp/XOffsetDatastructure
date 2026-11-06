// Delta format 1.0 is frozen (docs/DELTA_SYNC.md section 2, "Stability").
//
// A root of plain fields has one layout on every 64-bit little-endian target
// and no allocation placement to depend on, so its deltas are pinned byte for
// byte, both ways: the sender must write exactly these bytes, and the
// receiver must apply them (with and without the full check). The bytes are
// written out field by field after the table in section 2, so a change to the
// framing (header, varints, flags, selector and mask bytes, holes, digest)
// fails here on every compiler and schema backend. Update only together with
// a new format version. No exception of its own: also built as
// test_delta_format_noexc.
#define XOFFSET_FORCE_FULL_CHECK 0
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include "../xoffset_receiver.hpp"
using namespace XOffsetDatastructure;

struct FrozenRoot {
    std::uint64_t a{};
    std::uint32_t b{};
    std::uint32_t c{};
};
XOFFSET_RECORD(FrozenRoot, a, b, c)
XOFFSET_REGISTER_SCHEMA_NAME(FrozenRoot, "test.DeltaFormatRoot")

namespace {
int failures = 0;
void need(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

std::string hex(const std::string& s) {
    static const char* d = "0123456789abcdef";
    std::string out;
    for (unsigned char ch : s) {
        out += d[ch >> 4];
        out += d[ch & 15];
    }
    return out;
}

template <typename V>
std::string le(V v) {
    std::string out(sizeof(V), '\0');
    std::memcpy(out.data(), &v, sizeof(V));
    return out;
}

constexpr std::uint64_t kStream = 0x0123456789abcdefull;

// The payload: arena header [0, 48), the root at 48 (16 bytes), used_end 64.
// Key delta, version 1: KEY | HOLES | EXTENT, used (64 - 48) / 8 = 2, root
// 48 - 48 = 0, one block; the fingerprint; block 0 (skip 0) with mask bytes
// for words 6 and 7 (selector 0xc0): a's eight bytes (0xff), and of word 7
// only the nonzero bytes, b's first and c's first (0x11), since a key delta
// applies to zeros; then those ten bytes; no holes.
std::string key_delta() {
    std::string d = {'X', 'D', char(0x10), char(0x0b)};
    d += le(kStream);
    d += char(1) + std::string(1, char(2)) + char(0) + char(1);
    d += le(detail::wire_schema_hash<FrozenRoot>()) + le(detail::wire_root_type_id<FrozenRoot>());
    d += std::string{char(0), char(0xc0), char(0xff), char(0x11)};
    d += le(std::uint64_t(0x1122334455667788ull)) + char(7) + char(9);
    d += char(0);  // hole count
    return d;
}

// Version 2: b = 8. No flags (used_end and root stay): 14-byte header, one
// block, selector 0x80 (word 7), mask 0x01 (its first byte), the byte.
std::string scalar_delta() {
    std::string d = {'X', 'D', char(0x10), char(0)};
    d += le(kStream);
    d += char(2) + std::string(1, char(1));
    d += std::string{char(0), char(0x80), char(0x01), char(8)};
    return d;
}

// Version 3: a's low four bytes, with DIGEST: xxHash64 of [48, 64) after it.
std::string digest_delta(std::uint64_t digest) {
    std::string d = {'X', 'D', char(0x10), char(0x04)};
    d += le(kStream);
    d += char(3) + std::string(1, char(1));
    d += le(digest);
    d += std::string{char(0), char(0x40), char(0x0f)};
    d += le(std::uint32_t(0xa0b0c0d0u));
    return d;
}

std::uint64_t digest_after() {
    std::string payload = le(std::uint64_t(0x11223344a0b0c0d0ull)) + le(std::uint32_t(8)) + le(std::uint32_t(9));
    return detail::xxh64(payload.data(), payload.size());
}

void sender_writes_frozen_bytes() {
    auto src = XBuffer::create<FrozenRoot>(4096, XBuffer::maximum_capacity(1 << 20));
    auto& r = src.root<FrozenRoot>();
    need(reinterpret_cast<const char*>(&r) - src.data() == 48, "root not at offset 48");
    r.a = 0x1122334455667788ull;
    r.b = 7;
    r.c = 9;
    XDeltaBaseline base(kStream);
    std::string d;
    src.save_delta_into<FrozenRoot>(d, base);
    if (d != key_delta()) std::fprintf(stderr, "key delta written: %s\n           frozen: %s\n", hex(d).c_str(),
                                       hex(key_delta()).c_str());
    need(d == key_delta(), "sender: key delta bytes changed");
    r.b = 8;
    src.save_delta_into<FrozenRoot>(d, base);
    if (d != scalar_delta()) std::fprintf(stderr, "delta written: %s\n       frozen: %s\n", hex(d).c_str(),
                                          hex(scalar_delta()).c_str());
    need(d == scalar_delta(), "sender: delta bytes changed");
    need(d.size() == 18, "sender: a scalar delta is a 14-byte header and a 4-byte block");
    r.a = 0x11223344a0b0c0d0ull;
    XDeltaOptions digest;
    digest.digest = true;
    src.save_delta_into<FrozenRoot>(d, base, digest);
    need(d == digest_delta(digest_after()), "sender: digest delta bytes changed");
}

template <typename Apply>
void receiver_applies_frozen_bytes(Apply apply, const char* who) {
    std::string what = std::string(who) + ": ";
    auto dst = XBuffer::create<FrozenRoot>(4096, XBuffer::maximum_capacity(1 << 20));
    XDeltaCursor cur;
    need(apply(dst, key_delta(), cur), (what + "frozen key delta rejected").c_str());
    const auto& r = dst.root<FrozenRoot>();
    need(r.a == 0x1122334455667788ull && r.b == 7 && r.c == 9, (what + "key delta content").c_str());
    need(cur.stream() == kStream && cur.version() == 1, (what + "key delta cursor").c_str());
    need(apply(dst, scalar_delta(), cur) && dst.root<FrozenRoot>().b == 8, (what + "frozen delta").c_str());
    need(apply(dst, digest_delta(digest_after()), cur) && dst.root<FrozenRoot>().a == 0x11223344a0b0c0d0ull,
         (what + "frozen digest delta").c_str());
    need(cur.version() == 3, (what + "cursor after the stream").c_str());
    // Another format byte is refused, whatever follows it.
    std::string other = key_delta();
    other[2] = char(0x11);
    XDeltaCursor fresh;
    need(!apply(dst, other, fresh), (what + "format 1.1 accepted").c_str());
}
} // namespace

int main() {
    sender_writes_frozen_bytes();
    receiver_applies_frozen_bytes(
        [](XBuffer& b, const std::string& d, XDeltaCursor& c) {
            return b.try_apply_delta_verified<FrozenRoot>(d, c).has_value();
        },
        "apply_delta_verified");
    receiver_applies_frozen_bytes(
        [](XBuffer& b, const std::string& d, XDeltaCursor& c) {
            static XReceiver<XLink::from_server> rx(XBuffer::maximum_capacity(1 << 20));
            return rx.apply_delta<FrozenRoot>(b, d, c).has_value();
        },
        "XReceiver admit");
    if (failures) {
        std::fprintf(stderr, "test_delta_format: %d failure(s)\n", failures);
        return 1;
    }
    std::puts("test_delta_format: ok");
    return 0;
}
