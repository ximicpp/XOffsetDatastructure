// Must NOT compile: a std::string member. The first error names the member
// (fields<sdf::Named>::field_name) and says to use XString.
// CTest schema_describe_std_string_fail requires the diagnostic.
#include "xoffsetdatastructure.hpp"
#include <cstdint>
#include <string>

namespace sdf { struct Named { std::int32_t id{}; std::string name; }; }
XOFFSET_RECORD(sdf::Named, id, name)

int main() { return 0; }
