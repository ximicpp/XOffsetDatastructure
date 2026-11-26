// Must NOT compile: XOFFSET_RECORD inside a namespace block. The first error
// says so and shows the qualified spelling at global scope.
// CTest schema_describe_namespace_fail requires the diagnostic.
#include "xoffsetdatastructure.hpp"
#include <cstdint>

namespace sdf {
struct Unit { std::int32_t a{}; std::int32_t b{}; };
XOFFSET_RECORD(Unit, a, b)
}

int main() { return 0; }
