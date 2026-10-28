// Must NOT compile (except XOFFSET_TEST_CASE=portable): wire schemas whose
// meaning differs between 64-bit little-endian platforms. A set or map key
// that orders by plain char sorts differently where char is signed (x86-64,
// Apple, Windows) and unsigned (Linux and Android arm64); an enum without a
// fixed underlying type is int on MSVC and unsigned int on GCC and Clang.
// One target per case; CTest portable_schema_<case>_fail requires the diagnostic,
// portable_schema_portable_build requires the portable spellings to compile.
#include "xoffsetdatastructure.hpp"
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
#elif defined(XOFFSET_TEST_CASE_portable)
struct Root {
    char letter{};  // plain char outside a key is portable: same bytes, no order
    Fixed e{};
    xo::XSet<std::int8_t> i8;
    xo::XSet<ByteEnum> bytes;
    xo::XMap<xo::XString, char> names;
    xo::XMap<Int8Member, std::uint32_t> records;
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
#elif defined(XOFFSET_TEST_CASE_portable)
XOFFSET_RECORD_DERIVED(ps::Root, (), letter, e, i8, bytes, names, records)
#else
XOFFSET_RECORD_DERIVED(ps::Root, (), keys)
#endif

int main() {
    constexpr auto signature = xo::wire_abi_signature<ps::Root>();
    return std::string_view(signature).empty() ? 1 : 0;
}
