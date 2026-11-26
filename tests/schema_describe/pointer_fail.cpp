// Must NOT compile: a pointer member.
// CTest schema_describe_pointer_fail requires the diagnostic.
#include "xoffsetdatastructure.hpp"
#include <cstdint>

namespace sdf { struct Linked { std::int64_t id{}; Linked* next{}; }; }
XOFFSET_RECORD(sdf::Linked, id, next)

int main() { return 0; }
