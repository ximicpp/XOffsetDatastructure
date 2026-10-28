// Must NOT compile: a duplicate entry hides the omitted member b.
// CTest schema_describe_duplicate_fail requires the diagnostic.
#include "xoffsetdatastructure.hpp"
#include <cstdint>

namespace sdf { struct Neg { std::int32_t a{}; std::int32_t b{}; std::int32_t c{}; }; }
XOFFSET_RECORD(sdf::Neg, a, a, c)

int main() { return 0; }
