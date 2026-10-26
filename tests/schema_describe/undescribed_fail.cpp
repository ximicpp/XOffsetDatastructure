// Must NOT compile: without reflection (field lists forced) a record needs an
// XOFFSET_RECORD line before it is used.
// CTest schema_describe_undescribed_fail requires the diagnostic.
#define XOFFSET_SCHEMA_BACKEND_DESCRIBE 1
#include "xoffsetdatastructure.hpp"
#include <cstdint>

namespace sdf { struct Plain { std::int32_t a{}; }; }

int main() {
    auto buffer = XOffsetDatastructure::XBuffer::create<sdf::Plain>();
    return buffer.root<sdf::Plain>().a;
}
