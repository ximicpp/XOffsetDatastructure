// XSpare<N> (reserved bytes of a later version, docs/SCHEMA_EVOLUTION.md
// section 9): its own wire signature token (so XSpare<8> and
// std::uint8_t[8] are different layouts), size N, alignment 1, zero by
// default, admitted, no implicit padding, an ordinary record; every check
// treats its bytes as opaque: a wire whose spare bytes are not zero (a newer
// peer keeps fields there) passes admit and the full check and keeps those
// bytes through compact, relayout-free resends and edits elsewhere; a shared
// delta carries a spare member as plain bytes. Records use field lists, so
// this runs in every build; no try/catch: also built as test_xspare_noexc.
#define XOFFSET_FORCE_FULL_CHECK 0
#include "../xoffset_receiver.hpp"
#include "../xoffset_shared_delta.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

using namespace XOffsetDatastructure;
namespace xs = XOffsetDatastructure::xsync;

namespace {
int failures = 0;
#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                            \
        }                                                                          \
    } while (0)
} // namespace

namespace sp {
struct Item {
    std::int32_t id{};
    bool on{};
    std::uint8_t kind{};
    XSpare<2> spare{};
};
struct Root {
    std::int64_t tick{};
    std::int32_t level{};
    XSpare<4> pad4{};
    XString name;
    XVector<Item> items;
    XVector<XSpare<16>> later;    // a list kept for a later version
    XSpare<12> spare{};           // to the end: no tail padding
};
struct Bytes { std::int32_t a{}; std::uint8_t raw[8]{}; };
struct Spare { std::int32_t a{}; XSpare<8> raw{}; };
} // namespace sp
XOFFSET_RECORD(sp::Item, id, on, kind, spare)
XOFFSET_RECORD(sp::Root, tick, level, pad4, name, items, later, spare)
XOFFSET_RECORD(sp::Bytes, a, raw)
XOFFSET_RECORD(sp::Spare, a, raw)
XOFFSET_REGISTER_SCHEMA_NAME(sp::Root, "test.xspare.Root")
XOFFSET_REGISTER_SCHEMA_NAME(sp::Bytes, "test.xspare.Same")
XOFFSET_REGISTER_SCHEMA_NAME(sp::Spare, "test.xspare.Same")

// The type itself.
static_assert(sizeof(XSpare<1>) == 1 && sizeof(XSpare<8>) == 8 && sizeof(XSpare<13>) == 13);
static_assert(alignof(XSpare<8>) == 1 && alignof(XSpare<16>) == 1);
static_assert(std::is_trivially_copyable_v<XSpare<8>> && std::is_aggregate_v<XSpare<8>>);
static_assert(std::is_standard_layout_v<XSpare<8>>);
static_assert(detail::is_spare_v<XSpare<8>> && detail::is_spare_v<const XSpare<8>>);
static_assert(!detail::is_spare_v<std::uint8_t[8]> && !detail::is_spare_v<sp::Spare>);
static_assert(is_wire_admitted_v<XSpare<8>> && is_wire_admitted_v<sp::Root> && is_wire_admitted_v<sp::Spare>);
static_assert(!has_implicit_padding_v<XSpare<3>> && !has_implicit_padding_v<sp::Item>);
static_assert(!has_implicit_padding_v<sp::Root>);
static_assert(wire_record_layout<XSpare<8>>::ordinary_members);

// Its own signature token: the same bytes as std::uint8_t[8], another layout.
static_assert(std::string_view(wire_abi_signature<XSpare<8>>()) == "spare[8]");
static_assert(std::string_view(wire_abi_signature<std::uint8_t[8]>()) == "array[8]<u8char>");
static_assert(std::string_view(wire_abi_signature<sp::Spare>()) == "record[s:12,a:4]{@0:i32,@4:spare[8]}");
static_assert(std::string_view(wire_abi_signature<sp::Bytes>()) == "record[s:12,a:4]{@0:i32,@4:array[8]<u8char>}");
static_assert(wire_schema_hash_v<sp::Bytes>() != wire_schema_hash_v<sp::Spare>());
XOFFSET_PIN_SCHEMA(sp::Item, "record[s:8,a:4]{@0:i32,@4:bool,@5:u8char,@6:spare[2]}");

// A shared delta sees a spare member as one plain member.
static_assert(xs::sd_detail::Members<sp::Item>::kind<3> == xs::sd_detail::kMemberPlain);
static_assert(xs::sd_detail::Members<sp::Item>::size<3> == 2);
static_assert(xs::XRecordBody<sp::Item>::whole_body::flat);
static_assert(xs::sd_detail::Info<sp::Root>::plain_bytes == 8 + 4 + 4 + 12);

namespace {

XBuffer::MaximumCapacity cap() { return XBuffer::maximum_capacity(1u << 20); }

template <std::size_t N>
bool zero(const XSpare<N>& s) {
    for (std::uint8_t b : s.bytes)
        if (b) return false;
    return true;
}

template <std::size_t N>
void fill(XSpare<N>& s, std::uint8_t seed) {
    for (std::size_t i = 0; i < N; ++i) s.bytes[i] = std::uint8_t(seed + i * 37 + 1);
}

template <std::size_t N>
bool filled(const XSpare<N>& s, std::uint8_t seed) {
    for (std::size_t i = 0; i < N; ++i)
        if (s.bytes[i] != std::uint8_t(seed + i * 37 + 1)) return false;
    return true;
}

void defaults() {
    XSpare<8> s;
    CHECK(zero(s));
    sp::Item item;
    CHECK(zero(item.spare));
    auto b = TypedXBuffer<sp::Root>::create(4096, cap());
    auto& r = b.plain_root();
    CHECK(zero(r.pad4) && zero(r.spare));
    r.items.emplace_back();
    r.later.emplace_back();
    r.later.resize(3);
    CHECK(zero(r.items[0].spare));
    for (const auto& l : r.later) CHECK(zero(l));
}

// What a newer peer sends: fields in the spare bytes, the list kept for later
// filled with records.
TypedXBuffer<sp::Root> newer_image() {
    auto b = TypedXBuffer<sp::Root>::create(4096, cap());
    auto& r = b.plain_root();
    r.tick = 7;
    r.level = 3;
    fill(r.pad4, 4);
    fill(r.spare, 8);
    r.name.assign("player-with-a-long-name");
    for (std::uint8_t i = 0; i < 5; ++i) {
        r.items.emplace_back();
        r.items.back().id = i;
        fill(r.items.back().spare, std::uint8_t(20 + i));
    }
    r.later.resize(4);
    for (std::uint8_t i = 0; i < 4; ++i) fill(r.later[i], std::uint8_t(60 + i));
    return b;
}

bool newer_bytes_kept(const sp::Root& r) {
    bool ok = filled(r.pad4, 4) && filled(r.spare, 8) && r.items.size() == 5 && r.later.size() == 4;
    for (std::uint8_t i = 0; ok && i < 5; ++i) ok = filled(r.items[i].spare, std::uint8_t(20 + i));
    for (std::uint8_t i = 0; ok && i < 4; ++i) ok = filled(r.later[i], std::uint8_t(60 + i));
    return ok;
}

// Opaque: no tier requires spare bytes to be zero, and a program that edits
// other fields keeps them.
void opaque_on_every_tier() {
    auto src = newer_image();
    const XWireEmission emissions[] = {XWireEmission::dense, XWireEmission::free_runs, XWireEmission::zero_runs};
    XReceiver<XLink::server_peer> peer(cap());
    for (auto e : emissions) {
        const std::string wire = src.save_verified(XSaveOptions{e});
        auto full = TypedXBuffer<sp::Root>::try_load_verified(wire, cap());
        CHECK(full.has_value());
        if (!full) continue;
        CHECK(newer_bytes_kept(full->plain_root()));
        auto admitted = peer.load<sp::Root>(wire);
        CHECK(admitted.has_value());
        if (admitted) CHECK(newer_bytes_kept(admitted->root<sp::Root>()));

        // An edit elsewhere, then compact and dense resends: the bytes stay.
        auto& r = full->plain_root();
        r.level = 99;
        r.items.emplace_back();
        r.name.assign("x");
        r.items.pop_back();
        for (const std::string& again : {full->save_compact(), full->save_verified()}) {
            auto back = TypedXBuffer<sp::Root>::try_load_verified(again, cap());
            CHECK(back.has_value());
            if (back) {
                CHECK(back->plain_root().level == 99 && back->plain_root().name.view() == "x");
                CHECK(newer_bytes_kept(back->plain_root()));
            }
        }
    }
}

// A shared delta: the whole body carries spare bytes, a change body names a
// spare member as one plain member of its sizeof bytes.
void shared_delta_bodies() {
    sp::Item a;
    a.id = 5;
    a.on = true;
    a.kind = 2;
    fill(a.spare, 3);
    char body[64];
    char* end = xs::XRecordBody<sp::Item>::put_whole(body, a);
    CHECK(end - body == 8);
    CHECK(xs::XRecordBody<sp::Item>::check_whole(body, end) == end);
    sp::Item b;
    CHECK(xs::XRecordBody<sp::Item>::apply_whole(body, b) == end);
    CHECK(std::memcmp(&a, &b, sizeof a) == 0);

    sp::Item c;
    const std::uint64_t mask = xs::XRecordBody<sp::Item>::changed(a, c);
    CHECK(mask == 0b1111);
    end = xs::XRecordBody<sp::Item>::put_change(body, a, std::uint64_t(1) << 3);
    CHECK(end - body == 1 + 2);
    CHECK(xs::XRecordBody<sp::Item>::check_change(body, end) == end);
    CHECK(xs::XRecordBody<sp::Item>::apply_change(body, c) == end);
    CHECK(filled(c.spare, 3) && c.id == 0 && !c.on);
}

} // namespace

int main() {
    defaults();
    opaque_on_every_tier();
    shared_delta_bodies();
    if (failures) {
        std::fprintf(stderr, "test_xspare: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("test_xspare: ok\n");
    return 0;
}
