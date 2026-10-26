// Must NOT compile: a record with a base class is not an ordinary record.
// CTest ordinary_records_base_fail requires the diagnostic.
#include "xoffsetdatastructure.hpp"
#include <cstdint>

namespace orf {
struct Base { std::int64_t id{}; };
struct Derived : Base {
    XOffsetDatastructure::XString name;
};
} // namespace orf

XOFFSET_ORDINARY_RECORDS(orf::Derived)

int main() { return 0; }
