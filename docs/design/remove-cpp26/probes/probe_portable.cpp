// Probe 2 (no XOffset header, any C++20 compiler): the descriptor layer and
// the wire-signature algorithm compile on GCC 13 / Clang 18 in -std=c++20 and
// give the same signatures and hashes as P2996 reflection does for the same
// structs (reference values printed by probe_equiv under clang-p2996).
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>
// TypeLayout's config.hpp uses __has_feature inside #if; GCC < 14 has no
// __has_feature, so the header does not preprocess there (finding F-4).
#ifndef __has_feature
#define __has_feature(x) 0
#endif
#include <boost/typelayout/fixed_string.hpp>
#include "xo_describe.hpp"

namespace slg::xm {
struct alignas(8) Buff  { int32_t id{}; int32_t stacks{}; int64_t expire_ms{}; };
struct alignas(8) Skill { int32_t id{}; int32_t level{}; };
struct alignas(8) Troop { int32_t type{}; int32_t count{}; int32_t wounded{}; };
struct alignas(8) Event { int32_t round{}; int32_t hero_id{}; int32_t skill_id{}; int32_t target{}; int32_t damage{}; };
struct alignas(8) Tile { int32_t x{}; int32_t y{}; int32_t terrain{}; int32_t level{}; int64_t owner{}; };
}
namespace cw {
struct Vec3 { float x{}, y{}, z{}; };
struct Padded { std::uint8_t a{}; std::uint32_t b{}; std::uint16_t c[3]{}; std::int64_t d{}; };
enum class Kind : std::uint8_t { none, sword };
struct Mixed { Kind kind{}; bool on{}; Vec3 pos; std::array<std::int16_t, 5> slots{}; double w[2][3]{}; };
struct Base1 { int32_t id{}; };
struct Derived : Base1 { int32_t level{}; float hp{}; };   // in-arena only; v1 wire rejects bases
}

XO_DESCRIBE(slg::xm::Buff, id, stacks, expire_ms)
XO_DESCRIBE(slg::xm::Skill, id, level)
XO_DESCRIBE(slg::xm::Troop, type, count, wounded)
XO_DESCRIBE(slg::xm::Event, round, hero_id, skill_id, target, damage)
XO_DESCRIBE(slg::xm::Tile, x, y, terrain, level, owner)
XO_DESCRIBE(cw::Vec3, x, y, z)
XO_DESCRIBE(cw::Padded, a, b, c, d)
XO_DESCRIBE(cw::Mixed, kind, on, pos, slots, w)
XO_DESCRIBE(cw::Base1, id)
XO_DESCRIBE_DERIVED(cw::Derived, (cw::Base1), level, hp)

#if defined(NEG) && NEG == 1
struct Neg1 { int32_t a{}; int32_t b{}; int32_t c{}; };
XO_DESCRIBE(Neg1, a, c)            // omitted member b -> must not compile
#elif defined(NEG) && NEG == 2
struct Neg2 { int32_t a{}; int32_t b{}; };
XO_DESCRIBE(Neg2, b, a)            // wrong order -> must not compile
#elif defined(NEG) && NEG == 3
struct Neg3 { int32_t a{}; int32_t b{}; int32_t c{}; };
XO_DESCRIBE(Neg3, a, a, c)         // duplicate hides an omission -> must not compile
#elif defined(NEG) && NEG == 4
struct Neg4 { int32_t a{}; std::uint8_t tail[3]{}; };
XO_DESCRIBE(Neg4, a)               // omitted array member -> must not compile
#elif defined(NEG) && NEG == 5
struct Neg5 { std::uint8_t a{}; std::uint8_t hidden{}; std::uint16_t pad_looking{}; std::int64_t b{}; };
XO_DESCRIBE(Neg5, a, pad_looking, b)  // omission that a sizeof/tiling check would miss
#endif

namespace tl = ::boost::typelayout;
template <class T> consteval auto sig();

template <class U> consteval auto scalar_sig() {
    if constexpr (std::is_same_v<U, bool>) return tl::FixedString{"bool"};
    else if constexpr (std::is_same_v<U, char>) return tl::FixedString{"char"};
    else if constexpr (std::is_same_v<U, signed char>) return tl::FixedString{"i8char"};
    else if constexpr (std::is_same_v<U, unsigned char>) return tl::FixedString{"u8char"};
    else if constexpr (std::is_same_v<U, int8_t>) return tl::FixedString{"i8"};
    else if constexpr (std::is_same_v<U, uint8_t>) return tl::FixedString{"u8"};
    else if constexpr (std::is_same_v<U, int16_t>) return tl::FixedString{"i16"};
    else if constexpr (std::is_same_v<U, uint16_t>) return tl::FixedString{"u16"};
    else if constexpr (std::is_same_v<U, int32_t>) return tl::FixedString{"i32"};
    else if constexpr (std::is_same_v<U, uint32_t>) return tl::FixedString{"u32"};
    else if constexpr (std::is_same_v<U, int64_t>) return tl::FixedString{"i64"};
    else if constexpr (std::is_same_v<U, uint64_t>) return tl::FixedString{"u64"};
    else if constexpr (std::is_same_v<U, float>) return tl::FixedString{"f32"};
    else if constexpr (std::is_same_v<U, double>) return tl::FixedString{"f64"};
    else static_assert(sizeof(U) == 0);
}
template <class T> struct is_std_array : std::false_type {};
template <class E, std::size_t N> struct is_std_array<std::array<E, N>> : std::true_type {};

template <class T, class F, bool First> consteval auto field_sig() {
    auto one = tl::FixedString{"@"} + tl::to_fixed_string<F::offset>() + tl::FixedString{":"} + sig<typename F::type>();
    if constexpr (First) return one; else return tl::FixedString{","} + one;
}
template <class T, std::size_t... I> consteval auto fields_sig(std::index_sequence<I...>) {
    if constexpr (sizeof...(I) == 0) return tl::FixedString{""};
    else return (field_sig<T, xo_desc::member_t<T, I>, I == 0>() + ...);
}
template <class T> consteval auto sig() {
    using U = std::remove_cv_t<T>;
    if constexpr (std::is_arithmetic_v<U>) return scalar_sig<U>();
    else if constexpr (std::is_enum_v<U>) return tl::FixedString{"enum<"} + sig<std::underlying_type_t<U>>() + tl::FixedString{">"};
    else if constexpr (std::is_array_v<U>)
        return tl::FixedString{"array["} + tl::to_fixed_string<std::extent_v<U>>() + tl::FixedString{"]<"} +
               sig<std::remove_extent_t<U>>() + tl::FixedString{">"};
    else if constexpr (is_std_array<U>::value)
        return tl::FixedString{"record[s:"} + tl::to_fixed_string<sizeof(U)>() + tl::FixedString{",a:"} +
               tl::to_fixed_string<alignof(U)>() + tl::FixedString{"]{@0:"} +
               sig<typename U::value_type[std::tuple_size_v<U>]>() + tl::FixedString{"}"};
    else {
        static_assert(xo_desc::base_count<U> == 0, "v1 wire records have no bases");
        return tl::FixedString{"record[s:"} + tl::to_fixed_string<sizeof(U)>() + tl::FixedString{",a:"} +
               tl::to_fixed_string<alignof(U)>() + tl::FixedString{"]{"} +
               fields_sig<U>(std::make_index_sequence<xo_desc::member_count<U>>{}) + tl::FixedString{"}"};
    }
}
consteval std::uint64_t fnv1a_append(std::uint64_t seed, std::string_view sv) {
    for (char ch : sv) { seed ^= static_cast<unsigned char>(ch); seed *= 1099511628211ull; }
    return seed;
}
template <class T> consteval std::uint64_t hash() {
    constexpr auto s = sig<T>();
    return fnv1a_append(fnv1a_append(14695981039346656037ull, ""), std::string_view(s));
}
template <class T> void show(const char* name, std::uint64_t reference) {
    constexpr auto s = sig<T>();
    constexpr auto h = hash<T>();
    std::printf("%-14s hash=%016llx %s  %s\n", name, (unsigned long long)h,
                reference == 0 ? "(no ref)" : (reference == h ? "== P2996 reflection" : "!= P2996 reflection"),
                std::string_view(s).data());
}
// Field access through the descriptor: sums int32 members of a Tile.
int32_t sum_i32(const slg::xm::Tile& t) {
    int32_t s = 0;
    xo_desc::static_for<xo_desc::member_count<slg::xm::Tile>>([&](auto i) {
        using F = xo_desc::member_t<slg::xm::Tile, decltype(i)::value>;
        if constexpr (std::is_same_v<typename F::type, int32_t>) s += F::get(t);
    });
    return s;
}
int main() {
    show<slg::xm::Buff>("slg::Buff", 0xefe32f961166fcc0ull);
    show<slg::xm::Skill>("slg::Skill", 0xbefd3d66ddaa32a6ull);
    show<slg::xm::Troop>("slg::Troop", 0x0ab2ae9620d2959dull);
    show<slg::xm::Event>("slg::Event", 0x66f910559a549a66ull);
    show<slg::xm::Tile>("slg::Tile", 0x3bbe3f5581de48fbull);
    show<cw::Vec3>("cw::Vec3", 0xb46ff3f5c09c0eb0ull);
    show<cw::Padded>("cw::Padded", 0x1ef9750b4caebbdbull);
    show<cw::Mixed>("cw::Mixed", 0);
    static_assert(xo_desc::base_count<cw::Derived> == 1 && xo_desc::member_count<cw::Derived> == 2);
    slg::xm::Tile t{1, 2, 3, 4, 5};
    std::printf("sum_i32(Tile{1,2,3,4,5}) = %d\n", sum_i32(t));
}
