// Must NOT compile: a buffer of a record with a pointer member.
// CTest schema_describe_pointer_fail requires the diagnostic.
#include "xoffsetdatastructure.hpp"
#include <cstdint>

namespace sdf { struct Linked { std::int64_t id{}; Linked* next{}; }; }
XOFFSET_RECORD(sdf::Linked, id, next)

int main() {
    auto buffer = XOffsetDatastructure::TypedXBuffer<sdf::Linked>::create(1024);
    return buffer.ok() ? 0 : 1;
}
