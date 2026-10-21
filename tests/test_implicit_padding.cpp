// M6 (threat model T5/P3): has_implicit_padding_v finds bytes no field covers,
// in the type itself or anywhere a wire can reach from it.
#include "xoffsetdatastructure.hpp"

#include <cstdint>
#include <iostream>

namespace xo = XOffsetDatastructure;

namespace pad {
struct Dense { std::uint32_t a{}; std::uint32_t b{}; };
struct Interior { std::uint8_t tag{}; std::uint64_t value{}; };     // 7 bytes after tag
struct Tail { std::uint64_t value{}; std::uint8_t tag{}; };         // 7 bytes at the end
struct Explicit { std::uint8_t tag{}; std::uint8_t reserved[7]{}; std::uint64_t value{}; };
struct HoldsInterior { std::uint64_t id{}; Interior inner; };
struct Strings { xo::XString name; xo::XVector<std::uint32_t> ids; };
enum class Kind : std::uint8_t { a, b };
struct EnumPair { Kind kind{}; Kind other{}; std::uint16_t n{}; };

struct DenseRoot {
    Dense d;
    Explicit e;
    Strings s;
    EnumPair k;
    std::uint16_t arr[6]{};                      // ends at 72, where the 8-aligned map head starts
    xo::XMap<std::uint64_t, Dense> by_id;
    xo::XSet<std::uint32_t> set;
};
struct VectorOfPadded { xo::XVector<Interior> items; };
struct MapWithPaddedPair { xo::XMap<std::uint8_t, std::uint64_t> m; };   // XKeyValue<u8,u64>
struct ArrayOfPadded { Tail items[2]; };
} // namespace pad

static_assert(!xo::has_implicit_padding_v<pad::Dense>);
static_assert(xo::has_implicit_padding_v<pad::Interior>);
static_assert(xo::has_implicit_padding_v<pad::Tail>);
static_assert(!xo::has_implicit_padding_v<pad::Explicit>);
static_assert(xo::has_implicit_padding_v<pad::HoldsInterior>);
static_assert(!xo::has_implicit_padding_v<pad::Strings>);
static_assert(!xo::has_implicit_padding_v<pad::EnumPair>);
static_assert(!xo::has_implicit_padding_v<pad::DenseRoot>);
static_assert(xo::has_implicit_padding_v<pad::VectorOfPadded>);
static_assert(xo::has_implicit_padding_v<pad::MapWithPaddedPair>);
static_assert(xo::has_implicit_padding_v<pad::ArrayOfPadded>);
static_assert(!xo::has_implicit_padding_v<std::uint64_t[3]>);

XOFFSET_REQUIRE_NO_PADDING(pad::DenseRoot);
XOFFSET_REQUIRE_NO_PADDING(pad::Explicit);

int main() {
    std::cout << "implicit padding detection: 12 compile-time checks passed\n";
    return 0;
}
