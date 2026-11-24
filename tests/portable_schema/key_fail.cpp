// Must NOT compile (except XOFFSET_TEST_CASE=portable): wire schemas whose
// meaning or layout differs between 64-bit little-endian platforms. A set or
// map key that orders by plain char sorts differently where char is signed
// (x86-64, Apple, Windows) and unsigned (Linux and Android arm64); an enum
// without a fixed underlying type is int on MSVC and unsigned int on GCC and
// Clang; long double is 8 bytes on MSVC and 16 on GCC and Clang, wchar_t 2 and
// 4; #pragma pack, alignas on a member and an unnamed bit-field move members
// away from the natural layout, which compilers and flags disagree about;
// std::array<E, 0> is empty in libstdc++ and libc++ and one E in MSVC's
// library.
// One target per case; CTest portable_schema_<case>_fail requires the diagnostic,
// portable_schema_portable_build requires the portable spellings to compile.
#include "xoffsetdatastructure.hpp"
#include <array>
#include <compare>
#include <cstdint>
#include <string_view>

namespace xo = XOffsetDatastructure;

namespace ps {
enum class CharEnum : char { a, b };
enum class ByteEnum : std::uint8_t { a, b };
enum Unfixed { unfixed_zero, unfixed_one };
enum Fixed : std::uint32_t { fixed_zero, fixed_one };
struct CharMember {
    char c{};
    std::uint32_t n{};
    auto operator<=>(const CharMember&) const = default;
};
struct CharArray {
    char tag[4]{};
    auto operator<=>(const CharArray&) const = default;
};
struct Nested {
    CharArray inner;
    auto operator<=>(const Nested&) const = default;
};
struct Int8Member {
    std::int8_t c{};
    std::uint8_t tag[4]{};
    auto operator<=>(const Int8Member&) const = default;
};

#if defined(XOFFSET_TEST_CASE_set_char)
struct Root { xo::XSet<char> keys; };
#elif defined(XOFFSET_TEST_CASE_map_char)
struct Root { xo::XMap<char, std::uint32_t> keys; };
#elif defined(XOFFSET_TEST_CASE_char_enum)
struct Root { xo::XSet<CharEnum> keys; };
#elif defined(XOFFSET_TEST_CASE_record_member)
struct Root { xo::XMap<CharMember, std::uint32_t> keys; };
#elif defined(XOFFSET_TEST_CASE_array_member)
struct Root { xo::XSet<Nested> keys; };
#elif defined(XOFFSET_TEST_CASE_unfixed_enum)
struct Root { Unfixed e{}; std::uint32_t n{}; };
#elif defined(XOFFSET_TEST_CASE_long_double)
struct Root { std::uint32_t n{}; long double x{}; };
#elif defined(XOFFSET_TEST_CASE_wchar)
struct Root { std::uint32_t n{}; wchar_t w{}; };
#elif defined(XOFFSET_TEST_CASE_pragma_pack)
#pragma pack(push, 1)
struct Root { std::uint8_t a{}; std::uint32_t b{}; };
#pragma pack(pop)
#elif defined(XOFFSET_TEST_CASE_member_alignas)
struct Root { std::uint8_t a{}; alignas(8) std::uint32_t b{}; };
#elif defined(XOFFSET_TEST_CASE_unnamed_bit_field)
struct Root { std::uint8_t a{}; std::uint8_t : 8; std::uint8_t b{}; };
#elif defined(XOFFSET_TEST_CASE_empty_std_array)
struct Root { std::array<std::uint32_t, 0> none; std::uint32_t n{}; };
#elif defined(XOFFSET_TEST_CASE_portable)
struct alignas(8) Aligned {  // alignas on the record itself is standard and kept
    std::uint32_t id{};
    std::uint16_t level{};
};
struct Root {
    char letter{};  // plain char outside a key is portable: same bytes, no order
    Fixed e{};
    xo::XSet<std::int8_t> i8;
    xo::XSet<ByteEnum> bytes;
    xo::XMap<xo::XString, char> names;
    xo::XMap<Int8Member, std::uint32_t> records;
    Aligned aligned;
    std::array<std::uint16_t, 3> triple{};
    xo::XMap<std::uint8_t, std::uint64_t> wide;  // natural XKeyValue: second at 8
};
#else
#error "XOFFSET_TEST_CASE_<case> is not set"
#endif
} // namespace ps

XOFFSET_RECORD_DERIVED(ps::CharMember, (), c, n)
XOFFSET_RECORD_DERIVED(ps::CharArray, (), tag)
XOFFSET_RECORD_DERIVED(ps::Nested, (), inner)
XOFFSET_RECORD_DERIVED(ps::Int8Member, (), c, tag)
#if defined(XOFFSET_TEST_CASE_unfixed_enum)
XOFFSET_RECORD_DERIVED(ps::Root, (), e, n)
#elif defined(XOFFSET_TEST_CASE_long_double)
XOFFSET_RECORD_DERIVED(ps::Root, (), n, x)
#elif defined(XOFFSET_TEST_CASE_wchar)
XOFFSET_RECORD_DERIVED(ps::Root, (), n, w)
#elif defined(XOFFSET_TEST_CASE_pragma_pack) || defined(XOFFSET_TEST_CASE_member_alignas)
XOFFSET_RECORD_DERIVED(ps::Root, (), a, b)
#elif defined(XOFFSET_TEST_CASE_unnamed_bit_field)
// Under reflection the record needs no field list (and reflection does not
// list the unnamed bit-field, which is not a member).
#if !XOFFSET_SCHEMA_REFLECTION
XOFFSET_RECORD_DERIVED(ps::Root, (), a, b)
#endif
#elif defined(XOFFSET_TEST_CASE_empty_std_array)
XOFFSET_RECORD_DERIVED(ps::Root, (), none, n)
#elif defined(XOFFSET_TEST_CASE_portable)
XOFFSET_RECORD_DERIVED(ps::Aligned, (), id, level)
XOFFSET_RECORD_DERIVED(ps::Root, (), letter, e, i8, bytes, names, records, aligned, triple, wide)
#else
XOFFSET_RECORD_DERIVED(ps::Root, (), keys)
#endif

int main() {
    constexpr auto signature = xo::wire_abi_signature<ps::Root>();
    return std::string_view(signature).empty() ? 1 : 0;
}
