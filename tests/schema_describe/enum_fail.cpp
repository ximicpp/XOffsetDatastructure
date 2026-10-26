// Must NOT compile: without reflection (field lists forced) a raw-received
// enum needs a fixed underlying type.
// CTest schema_describe_enum_fail requires the diagnostic.
#define XOFFSET_SCHEMA_BACKEND_DESCRIBE 1
#include "receive_raw.hpp"
#include <cstdint>

namespace sdf { enum Unfixed { unfixed_zero = 0, unfixed_seven = 7 }; }

int main() {
    return XOffsetDatastructure::detail::raw_enum_representation_valid<sdf::Unfixed>(0) ? 0 : 1;
}
