// XOFFSET_RECORD and the field-list backend. Built under every backend:
//   - C++26 (reflection): every XOFFSET_RECORD line below is checked against
//     reflection by the macro itself, and the library's own field lists
//     (containers, XKeyValue, std::array) are compared with reflection here;
//   - field lists (C++20/C++23, or XOFFSET_SCHEMA_BACKEND_DESCRIBE=1): the
//     same records give the same wire signatures, pinned with reflection's text,
//     and round-trip through save_verified/load_verified.

#include "xoffsetdatastructure.hpp"
#include "schema_describe/dump_cases.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

namespace xo = XOffsetDatastructure;

namespace sd {
    enum class Kind : std::uint8_t { none, sword, shield };
    enum Legacy : std::uint16_t { legacy_a, legacy_b };

    struct Vec3 { float x{}, y{}, z{}; };
    struct alignas(8) Item {
        std::int32_t id{};
        Kind kind{};
        Legacy legacy{};
        std::array<std::int16_t, 5> slots{};
        double grid[2][3]{};
        Vec3 pos;
        xo::XString name;
    };
    struct alignas(8) Bag {
        std::uint64_t owner{};
        xo::XVector<Item> items;
        xo::XMap<std::int32_t, xo::XString> labels;
        xo::XSet<std::int32_t> tags;
    };

    // In-arena only (the v1 wire has no bases).
    struct Entity { std::int64_t id{}; };
    struct Player : Entity { std::int32_t level{}; xo::XVector<std::int32_t> items; };

    // Not an ordinary record: a user-declared copy assignment.
    struct Counted {
        std::int32_t value{};
        Counted& operator=(const Counted&) = default;
    };
} // namespace sd

XOFFSET_RECORD(sd::Vec3, x, y, z)
XOFFSET_RECORD(sd::Item, id, kind, legacy, slots, grid, pos, name)
XOFFSET_RECORD(sd::Bag, owner, items, labels, tags)
XOFFSET_RECORD(sd::Entity, id)
XOFFSET_RECORD_DERIVED(sd::Player, (sd::Entity), level, items)
XOFFSET_RECORD_DERIVED(sd::Counted, (), value)

// The wire signatures reflection computes (P2996 and GCC 16); the field lists
// must give the same text.
XOFFSET_PIN_SCHEMA(sd::Vec3, "record[s:12,a:4]{@0:f32,@4:f32,@8:f32}");
XOFFSET_PIN_SCHEMA(sd::Item,
    "record[s:104,a:8]{@0:i32,@4:enum<u8char>,@6:enum<u16>,@8:record[s:10,a:2]{@0:array[5]<i16>},"
    "@24:array[2]<array[3]<f64>>,@72:record[s:12,a:4]{@0:f32,@4:f32,@8:f32},"
    "@88:xstring[s:16,a:8]{arena:rel32,data:rel32,size:u32,capacity:u32}{inline:10}}");
XOFFSET_PIN_SCHEMA(sd::Bag,
    "record[s:56,a:8]{@0:u64,@8:xvector[s:16,a:8]{arena:rel32,data:rel32,size:u32,capacity:u32}"
    "<record[s:104,a:8]{@0:i32,@4:enum<u8char>,@6:enum<u16>,@8:record[s:10,a:2]{@0:array[5]<i16>},"
    "@24:array[2]<array[3]<f64>>,@72:record[s:12,a:4]{@0:f32,@4:f32,@8:f32},"
    "@88:xstring[s:16,a:8]{arena:rel32,data:rel32,size:u32,capacity:u32}{inline:10}}>,"
    "@24:xflatmap[s:16,a:8]{arena:rel32,data:rel32,size:u32,capacity:u32}<i32,"
    "xstring[s:16,a:8]{arena:rel32,data:rel32,size:u32,capacity:u32}{inline:10}>,"
    "@40:xflatset[s:16,a:8]{arena:rel32,data:rel32,size:u32,capacity:u32}<i32>}");
XOFFSET_REGISTER_SCHEMA_NAME(sd::Bag, "test.schema-describe.bag.v1")

namespace schema = xo::schema;

// Interface answers, whichever backend gives them.
static_assert(schema::member_count_v<sd::Item> == 7);
static_assert(schema::member_t<sd::Item, 3>::offset == offsetof(sd::Item, slots));
static_assert(std::is_same_v<schema::member_type_t<sd::Item, 4>, double[2][3]>);
static_assert(schema::base_count_v<sd::Player> == 1);
static_assert(std::is_same_v<schema::base_t<sd::Player, 0>, sd::Entity>);
static_assert(schema::member_count_v<sd::Player> == 2);
static_assert(schema::base_count_v<xo::XVector<int>> == 1);
static_assert(schema::member_count_v<xo::XFixedVector<int>> == 4);
static_assert(xo::wire_record_layout<sd::Item>::ordinary_members);
static_assert(xo::StableEraseOrdinaryMembers<sd::Bag>::value);
static_assert(!xo::wire_record_layout<sd::Player>::ordinary_members);
static_assert(!xo::wire_record_layout<sd::Counted>::ordinary_members);
static_assert(xo::is_byte_copy_safe_v<sd::Bag>);
static_assert(xo::is_byte_copy_safe_v<sd::Player>);

// Types no field list can describe (a union or a non-aggregate class) are
// opaque without reflection: not admitted, never walked. Reflection sees the
// same answer through their members.
namespace sd {
    struct Polymorphic { virtual ~Polymorphic() = default; std::int32_t x{}; };
    union Bits { std::int32_t i; float f; };
    struct UserDestructor { std::int32_t value{}; ~UserDestructor() {} };
    struct UserAssignment {
        std::int32_t value{};
        UserAssignment& operator=(const UserAssignment& other) { value = other.value; return *this; }
    };
} // namespace sd
XOFFSET_RECORD_DERIVED(sd::UserDestructor, (), value)
XOFFSET_RECORD_DERIVED(sd::UserAssignment, (), value)

template <class T>
concept StableErasable = requires(xo::XVector<T>& v) { v.stable_erase_if([](const T&) { return false; }); };
static_assert(!xo::is_byte_copy_safe_v<std::string>);
static_assert(!xo::is_byte_copy_safe_v<sd::Polymorphic>);
static_assert(!xo::is_byte_copy_safe_v<sdc::DerivedNamed>);
static_assert(!xo::is_byte_copy_safe_v<sdc::CaseRoot>);
static_assert(!StableErasable<std::string>);
static_assert(!xo::detail::receive_raw_supported<std::string>());
static_assert(XOFFSET_SCHEMA_REFLECTION || xo::schema::is_undescribable_v<sd::Bits>);
static_assert(!xo::schema::is_undescribable_v<xo::XVector<std::int32_t>>);

// Without reflection a user-provided destructor or assignment is caught when
// every member is trivial for it (with reflection, always).
static_assert(!schema::implicit_lifetime_aggregate<sd::UserDestructor>());
static_assert(schema::implicit_lifetime_aggregate<sd::UserAssignment>());
static_assert(!schema::only_implicit_special_members<sd::UserAssignment>());
static_assert(!xo::detail::receive_raw_supported<sd::UserDestructor>());
static_assert(!xo::detail::receive_raw_supported<sd::UserAssignment>());

#if XOFFSET_SCHEMA_REFLECTION
// The library's own field lists, against reflection.
namespace {
    // Without reflection both promises are "an aggregate with a field list";
    // the library asks them only of aggregates.
    template <class T>
    consteval bool same_as_reflection() {
        return schema::reflect_detail::description_bases_match<T>() &&
               schema::reflect_detail::description_members_match<T>() &&
               (!std::is_aggregate_v<T> || (schema::implicit_lifetime_aggregate<T>() &&
                                            schema::only_implicit_special_members<T>()));
    }
} // namespace
static_assert(same_as_reflection<xo::XFixedString>());
static_assert(same_as_reflection<xo::XString>());
static_assert(same_as_reflection<xo::XFixedVector<sd::Item>>());
static_assert(same_as_reflection<xo::XVector<sd::Item>>());
static_assert(same_as_reflection<xo::XFixedFlatSet<std::int32_t>>());
static_assert(same_as_reflection<xo::XSet<std::int32_t>>());
static_assert(same_as_reflection<xo::XFixedFlatMap<std::int32_t, xo::XString>>());
static_assert(same_as_reflection<xo::XMap<std::int32_t, xo::XString>>());
static_assert(same_as_reflection<xo::XKeyValue<std::int32_t, std::int64_t>>());
static_assert(same_as_reflection<xo::XKeyValue<xo::XString, sd::Vec3>>());
static_assert(same_as_reflection<std::array<std::int32_t, 3>>());
static_assert(same_as_reflection<std::array<sd::Vec3, 2>>());
static_assert(same_as_reflection<std::array<std::int32_t, 0>>());
#endif

namespace {
    int failures = 0;
    void check(bool ok, const char* what) {
        if (!ok) {
            std::cout << "  FAILED: " << what << "\n";
            ++failures;
        }
    }
} // namespace

int main() {
    auto buffer = xo::XBuffer::create<sd::Bag>(4096);
    auto& bag = buffer.root<sd::Bag>();
    bag.owner = 42;
    sd::Item item;
    item.id = 7;
    item.kind = sd::Kind::shield;
    item.legacy = sd::legacy_b;
    item.slots = {1, 2, 3, 4, 5};
    item.grid[1][2] = 2.5;
    item.pos = {1.0f, 2.0f, 3.0f};
    bag.items.push_back(item);
    bag.items.back().name = std::string_view("shield of seven");
    bag.labels.emplace(3, std::string_view("three"));
    bag.tags.insert(9);

    const auto wire = buffer.save_verified<sd::Bag>();
    auto loaded = xo::XBuffer::load_verified<sd::Bag>(wire);
    const auto& back = loaded.root<sd::Bag>();
    check(back.owner == 42, "owner");
    check(back.items.size() == 1 && back.items[0].id == 7 && back.items[0].kind == sd::Kind::shield &&
              back.items[0].legacy == sd::legacy_b && back.items[0].slots[4] == 5 &&
              back.items[0].grid[1][2] == 2.5 && back.items[0].pos.z == 3.0f,
          "item fields");
    check(std::string_view(back.items[0].name) == "shield of seven", "item name");
    check(back.labels.size() == 1 && back.tags.size() == 1, "map and set");

    auto arena = xo::XBuffer::create<sd::Player>(1024);
    auto& player = arena.root<sd::Player>();
    player.id = 5;
    player.level = 3;
    player.items.push_back(11);
    check(player.id == 5 && player.level == 3 && player.items[0] == 11, "derived record");

    std::cout << "test_schema_describe (" << (XOFFSET_SCHEMA_REFLECTION ? "reflection" : "field lists")
              << "): " << (failures ? "FAILED" : "passed") << "\n";
    return failures ? 1 : 0;
}
