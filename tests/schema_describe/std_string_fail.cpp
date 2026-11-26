// Must NOT compile: a buffer of a record with a std::string member (a field
// list alone may describe it, for tools/describe_dump). The first error names the member
// (fields<sdf::Named>::field_name) and says to use XString.
// CTest schema_describe_std_string_fail requires the diagnostic.
#include "xoffsetdatastructure.hpp"
#include <cstdint>
#include <string>

namespace sdf { struct Named { std::int32_t id{}; std::string name; }; }
XOFFSET_RECORD(sdf::Named, id, name)

int main() {
    auto buffer = XOffsetDatastructure::TypedXBuffer<sdf::Named>::create(1024);
    return buffer.ok() ? 0 : 1;
}
