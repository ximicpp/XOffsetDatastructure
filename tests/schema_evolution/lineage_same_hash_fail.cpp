// Two versions of a lineage with the same layout (fields swapped) have the
// same schema_hash: the build must fail, naming both versions.
#include "../../xoffset_evolution.hpp"

namespace v1 { struct Stats { std::int32_t hp; std::int32_t mp; }; }
namespace v2 { struct Stats { std::int32_t mp; std::int32_t hp; }; }
XOFFSET_REGISTER_SCHEMA_NAME(v1::Stats, "test.evolution.Stats")
XOFFSET_REGISTER_SCHEMA_NAME(v2::Stats, "test.evolution.Stats")
XOFFSET_SCHEMA_LINEAGE(v2::Stats, v1::Stats)

int main() { return 0; }
