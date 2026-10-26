// Must NOT compile: `tag` is [[no_unique_address]] and shares storage with
// `id`. CTest ordinary_records_overlap_fail requires the diagnostic.
#include "xoffsetdatastructure.hpp"
#include <cstdint>

namespace orf {
struct Tag {};
struct Overlapping {
    [[no_unique_address]] Tag tag;
    std::int64_t id{};
    XOffsetDatastructure::XString name;
};
} // namespace orf

XOFFSET_ORDINARY_RECORDS(orf::Overlapping)

int main() { return 0; }
