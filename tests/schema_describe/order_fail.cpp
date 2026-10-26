// Must NOT compile: the field list is out of declaration order.
// CTest schema_describe_order_fail requires the diagnostic.
#include "xoffsetdatastructure.hpp"
#include <cstdint>

namespace sdf { struct Neg { std::int32_t a{}; std::int32_t b{}; }; }
XOFFSET_RECORD(sdf::Neg, b, a)

int main() { return 0; }
