// Must NOT compile: a proxy of a record with library containers but no field
// list (field lists forced). The first error asks for XOFFSET_RECORD.
// CTest schema_describe_proxy_undescribed_fail requires the diagnostic.
#define XOFFSET_SCHEMA_BACKEND_DESCRIBE 1
#include "xoffsetdatastructure.hpp"
#include <cstdint>

namespace sdf { struct Bag { std::int32_t id{}; XOffsetDatastructure::XString name; }; }

int main() {
    auto buffer = XOffsetDatastructure::TypedXBuffer<sdf::Bag>::create(1024);
    return static_cast<int>(buffer.root().size());
}
