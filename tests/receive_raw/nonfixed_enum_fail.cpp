// Must NOT compile without reflection: test_receive_raw's enums without a fixed
// underlying type (their value range needs reflection). One target per enum,
// XOFFSET_TEST_ENUM names it; CTest receive_raw_nonfixed_<enum>_fail requires
// the diagnostic.
#include "receive_raw.hpp"
#include <cstdint>

namespace model {
enum Nonfixed { nf_zero=0,nf_seven=7 };
enum Negative { negative_low=-3,negative_high=1 };
enum Zero { only_zero=0 };
enum EmptyEnum {};
enum Wide { wide_min=(-9223372036854775807LL-1),wide_max=9223372036854775807LL };
enum UnsignedWide { unsigned_wide_max=18446744073709551615ULL };
}

int main() {
    return XOffsetDatastructure::detail::raw_enum_representation_valid<model::XOFFSET_TEST_ENUM>(0) ? 0 : 1;
}
