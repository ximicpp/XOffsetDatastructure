// Must NOT compile: the C array member tail is omitted.
// CTest schema_describe_array_fail requires the diagnostic.
#include "xoffsetdatastructure.hpp"
#include <cstdint>

namespace sdf { struct Neg { std::int32_t a{}; std::uint8_t tail[3]{}; }; }
XOFFSET_RECORD(sdf::Neg, a)

int main() { return 0; }
