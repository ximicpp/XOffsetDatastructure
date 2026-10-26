// Must NOT compile: bit-fields are not ordinary members.
// CTest ordinary_records_bit_field_fail requires the diagnostic.
#include "xoffsetdatastructure.hpp"
#include <cstdint>

namespace orf {
struct Bits {
    std::uint32_t flags : 3;
    std::uint32_t level : 29;
    XOffsetDatastructure::XString name;
};
} // namespace orf

XOFFSET_ORDINARY_RECORDS(orf::Bits)

int main() { return 0; }
