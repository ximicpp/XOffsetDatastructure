// Must NOT compile: the omitted one-byte member looks like padding, so only
// the aggregate initialization count catches it (sizeof and tiling do not).
// CTest schema_describe_padding_fail requires the diagnostic.
#include "xoffsetdatastructure.hpp"
#include <cstdint>

namespace sdf {
struct Neg { std::uint8_t a{}; std::uint8_t hidden{}; std::uint16_t c{}; std::int64_t b{}; };
}
XOFFSET_RECORD(sdf::Neg, a, c, b)

int main() { return 0; }
