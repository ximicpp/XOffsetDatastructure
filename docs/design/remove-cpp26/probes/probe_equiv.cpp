// Probe 1 (P2996 build, real library): descriptor-derived wire signature,
// schema hash and padding flag equal the library's reflection-derived ones.
#include <cstddef>
#include <cstdio>
#include <cstring>
#include "xoffsetdatastructure.hpp"
#include "slg/model_xoffset.hpp"
#include "xo_describe.hpp"

namespace xo = XOffsetDatastructure;

namespace cw {
enum class Kind : std::uint8_t { none = 0, sword = 1, bow = 2, staff = 3 };
enum class Rank : std::uint32_t { r1 = 1, r2, r3, r4, r5, r6, r7, r8 };
inline std::strong_ordering operator<=>(Rank a, Rank b) noexcept { return std::uint32_t(b) <=> std::uint32_t(a); }
struct Vec3 { float x{}, y{}, z{}; };
struct Item { std::uint32_t id{}; bool equipped{}; Kind kind{}; double weight{}; xo::XString name; xo::XVector<std::uint16_t> stats; };
struct Unit { std::int64_t hp{}; Vec3 pos; std::array<std::int16_t, 5> slots{}; xo::XString tag; };
struct Root {
    std::uint64_t epoch{}; xo::XString title; xo::XVector<Item> items; xo::XVector<Unit> units;
    xo::XVector<xo::XString> labels; xo::XSet<std::int32_t> keys; xo::XMap<xo::XString, std::uint64_t> attrs;
    xo::XMap<Rank, xo::XString> ranks; xo::XSet<Rank> rank_set;
};
struct Padded { std::uint8_t a{}; std::uint32_t b{}; std::uint16_t c[3]{}; std::int64_t d{}; };
} // namespace cw
XOFFSET_REGISTER_SCHEMA_NAME(cw::Root, "cross_compiler.v2.root")

// ---- the descriptor lines a C++20 user would write ----------------------------
XO_DESCRIBE(slg::xm::Buff, id, stacks, expire_ms)
XO_DESCRIBE(slg::xm::Skill, id, level)
XO_DESCRIBE(slg::xm::Troop, type, count, wounded)
XO_DESCRIBE(slg::xm::Hero, id, level, star, name, skills, buffs)
XO_DESCRIBE(slg::xm::Army, army_id, owner_id, x, y, heroes, troops, buffs)
XO_DESCRIBE(slg::xm::City, city_id, level, buildings, resources)
XO_DESCRIBE(slg::xm::PlayerState, player_id, name, city, armies, heroes, research, items)
XO_DESCRIBE(slg::xm::Event, round, hero_id, skill_id, target, damage)
XO_DESCRIBE(slg::xm::Report, battle_id, summary, events)
XO_DESCRIBE(slg::xm::BattleMsg, battle_id, seed, winner, attacker, defender, report)
XO_DESCRIBE(slg::xm::Tile, x, y, terrain, level, owner)
XO_DESCRIBE(slg::xm::March, march_id, owner, from_x, from_y, to_x, to_y, eta, troops)
XO_DESCRIBE(slg::xm::ShardState, shard_id, tick, width, tiles, cities, marches)
XO_DESCRIBE(cw::Vec3, x, y, z)
XO_DESCRIBE(cw::Item, id, equipped, kind, weight, name, stats)
XO_DESCRIBE(cw::Unit, hp, pos, slots, tag)
XO_DESCRIBE(cw::Root, epoch, title, items, units, labels, keys, attrs, ranks, rank_set)
XO_DESCRIBE(cw::Padded, a, b, c, d)

// Library-owned descriptors (the library would ship these).
namespace xo_desc {
template <class K, class V> struct describe<xo::XKeyValue<K, V>>
    : record<xo::XKeyValue<K, V>, bases<>, field<&xo::XKeyValue<K, V>::first, __builtin_offsetof(xo::XKeyValue<K, V>, first)>,
                                        field<&xo::XKeyValue<K, V>::second, __builtin_offsetof(xo::XKeyValue<K, V>, second)>> {};
}

namespace probe {
namespace d = xo::detail;
namespace tl = ::boost::typelayout;

template <class T> consteval auto sig();

// std::array<E, N>: the library emits the reflected shape {@0:array[N]<E>} by hand.
template <class T> struct is_std_array : std::false_type {};
template <class E, std::size_t N> struct is_std_array<std::array<E, N>> : std::true_type {};

template <class T, class F, bool First>
consteval auto field_sig() {
    auto one = tl::FixedString{"@"} + tl::to_fixed_string<F::offset>() + tl::FixedString{":"} + sig<typename F::type>();
    if constexpr (First) return one; else return tl::FixedString{","} + one;
}
template <class T, std::size_t... I>
consteval auto fields_sig(std::index_sequence<I...>) {
    if constexpr (sizeof...(I) == 0) return tl::FixedString{""};
    else return (field_sig<T, xo_desc::member_t<T, I>, I == 0>() + ...);
}
template <class U>
consteval auto record_sig() {
    static_assert(xo_desc::described<U>, "record has no XO_DESCRIBE");
    static_assert(xo_desc::base_count<U> == 0);
    return tl::FixedString{"record[s:"} + tl::to_fixed_string<sizeof(U)>() + tl::FixedString{",a:"} +
           tl::to_fixed_string<alignof(U)>() + tl::FixedString{"]{"} +
           fields_sig<U>(std::make_index_sequence<xo_desc::member_count<U>>{}) + tl::FixedString{"}"};
}
template <class N>
consteval auto seq_head(const N& tag, std::size_t, std::size_t) { return tag; }

template <class T>
consteval auto sig() {
    using U = std::remove_cv_t<T>;
    if constexpr (d::fixed_size_scalar<U> || std::is_enum_v<U> || d::is_fixed_string_v<U> ||
                  d::is_fixed_flat_map_v<U> || d::is_fixed_flat_set_v<U> || d::is_fixed_vector_v<U>) {
        // Non-record kinds: the library code for these never touches reflection
        // except through the element type, which we route back here.
        if constexpr (d::fixed_size_scalar<U> || std::is_enum_v<U> || d::is_fixed_string_v<U>)
            return d::wire_abi_signature<U>();
        else if constexpr (d::is_fixed_flat_map_v<U>)
            return d::fixed_sequence_header_signature<U>(tl::FixedString{"xflatmap"}) + tl::FixedString{"<"} +
                   sig<typename U::key_type>() + tl::FixedString{","} + sig<typename U::mapped_type>() + tl::FixedString{">"};
        else if constexpr (d::is_fixed_flat_set_v<U>)
            return d::fixed_sequence_header_signature<U>(tl::FixedString{"xflatset"}) + tl::FixedString{"<"} +
                   sig<typename U::value_type>() + tl::FixedString{">"};
        else if constexpr (std::is_same_v<typename U::value_type, std::byte>)
            return d::fixed_sequence_header_signature<U>(tl::FixedString{"xblob"});
        else
            return d::fixed_sequence_header_signature<U>(tl::FixedString{"xvector"}) + tl::FixedString{"<"} +
                   sig<typename U::value_type>() + tl::FixedString{">"};
    } else if constexpr (std::is_array_v<U>) {
        return tl::FixedString{"array["} + tl::to_fixed_string<std::extent_v<U>>() + tl::FixedString{"]<"} +
               sig<std::remove_extent_t<U>>() + tl::FixedString{">"};
    } else if constexpr (d::is_xkeyvalue_v<U>) {
        return tl::FixedString{"xkv[s:"} + tl::to_fixed_string<sizeof(U)>() + tl::FixedString{",a:"} +
               tl::to_fixed_string<alignof(U)>() + tl::FixedString{"]<"} + sig<typename U::first_type>() +
               tl::FixedString{","} + sig<typename U::second_type>() + tl::FixedString{">"};
    } else if constexpr (is_std_array<U>::value) {
        return tl::FixedString{"record[s:"} + tl::to_fixed_string<sizeof(U)>() + tl::FixedString{",a:"} +
               tl::to_fixed_string<alignof(U)>() + tl::FixedString{"]{@0:"} +
               sig<typename U::value_type[std::tuple_size_v<U>]>() + tl::FixedString{"}"};
    } else {
        return record_sig<U>();
    }
}

template <class T> consteval bool padded();
template <class U, std::size_t... I>
consteval bool record_padded(std::index_sequence<I...>) {
    std::size_t covered = 0; bool p = false;
    auto one = [&]<std::size_t K>() {
        using F = xo_desc::member_t<U, K>;
        if (F::offset != covered) p = true;
        covered = F::offset + sizeof(typename F::type);
        if (padded<typename F::type>()) p = true;
    };
    (one.template operator()<I>(), ...);
    return p || covered != sizeof(U);
}
template <class T> consteval bool padded() {
    using U = std::remove_cv_t<T>;
    if constexpr (d::fixed_size_scalar<U> || std::is_enum_v<U> || d::is_fixed_string_v<U>) return false;
    else if constexpr (std::is_array_v<U>) return padded<std::remove_extent_t<U>>();
    else if constexpr (d::is_fixed_vector_v<U>) return padded<typename U::value_type>();
    else if constexpr (is_std_array<U>::value) return padded<typename U::value_type>();
    else return record_padded<U>(std::make_index_sequence<xo_desc::member_count<U>>{});
}

template <class T> consteval std::uint64_t hash() {
    constexpr auto s = sig<T>();
    return d::fnv1a_append(d::fnv1a(xo::schema_name<T>::value), std::string_view(s));
}

int failures = 0;
template <class T> void check(const char* name) {
    constexpr auto lib = xo::wire_abi_signature<T>();
    constexpr auto mine = sig<T>();
    const bool sig_eq = std::string_view(lib) == std::string_view(mine);
    const bool hash_eq = xo::wire_schema_hash_v<T>() == hash<T>();
    const bool pad_eq = xo::has_implicit_padding_v<T> == padded<T>();
    if (!(sig_eq && hash_eq && pad_eq)) ++failures;
    std::printf("%-22s sig %s hash %s pad %s  len=%zu hash=%016llx\n", name, sig_eq ? "==" : "!=",
                hash_eq ? "==" : "!=", pad_eq ? "==" : "!=", std::string_view(lib).size(),
                static_cast<unsigned long long>(xo::wire_schema_hash_v<T>()));
    if (!sig_eq) std::printf("  lib : %s\n  mine: %s\n", std::string_view(lib).data(), std::string_view(mine).data());
}
} // namespace probe

int main() {
    using namespace probe;
    check<slg::xm::Buff>("slg::Buff");           check<slg::xm::Skill>("slg::Skill");
    check<slg::xm::Troop>("slg::Troop");         check<slg::xm::Hero>("slg::Hero");
    check<slg::xm::Army>("slg::Army");           check<slg::xm::City>("slg::City");
    check<slg::xm::PlayerState>("slg::PlayerState"); check<slg::xm::Event>("slg::Event");
    check<slg::xm::Report>("slg::Report");       check<slg::xm::BattleMsg>("slg::BattleMsg");
    check<slg::xm::Tile>("slg::Tile");           check<slg::xm::March>("slg::March");
    check<slg::xm::ShardState>("slg::ShardState");
    check<cw::Vec3>("cw::Vec3"); check<cw::Item>("cw::Item"); check<cw::Unit>("cw::Unit");
    check<cw::Root>("cw::Root"); check<cw::Padded>("cw::Padded");
    std::printf("%s (%d mismatches)\n", failures ? "FAIL" : "ALL EQUAL", failures);
    return failures != 0;
}
