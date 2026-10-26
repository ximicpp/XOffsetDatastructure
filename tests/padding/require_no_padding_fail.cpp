// Must not compile: XOFFSET_REQUIRE_NO_PADDING on a type with interior padding.
#include "xoffsetdatastructure.hpp"

#include <cstdint>

namespace pad_fail {
struct Padded { std::uint8_t tag{}; std::uint64_t value{}; };
}

XOFFSET_RECORD_DERIVED(pad_fail::Padded, (), tag, value)

XOFFSET_REQUIRE_NO_PADDING(pad_fail::Padded);

int main() { return 0; }
