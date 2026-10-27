// Must NOT compile under C++26: XOFFSET_RECORD promises an ordinary record, and
// reflection sees a user-provided destructor.
// CTest schema_describe_promise_fail requires the diagnostic.
#include "xoffsetdatastructure.hpp"
#include <cstdint>

namespace sdf { struct Neg { std::int32_t a{}; ~Neg() {} }; }
XOFFSET_RECORD(sdf::Neg, a)

int main() { return 0; }
