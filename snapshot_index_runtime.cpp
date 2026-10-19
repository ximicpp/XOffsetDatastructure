#ifndef XOFFSET_SNAPSHOT_INDEX_RUNTIME_BUILD
#define XOFFSET_SNAPSHOT_INDEX_RUNTIME_BUILD 1
#endif
#include "xoffset_snapshot_index_budget.hpp"

namespace XOffsetDatastructure::snapshot_index_detail {
// Process-wide within ONE loaded shared runtime. Consumers must use this same
// exported accessor, ABI/runtime and library instance, including hidden DSOs.
extern "C" XOFFSET_SNAPSHOT_INDEX_EXPORT BudgetRegistry*
xoffset_snapshot_index_shared_budget_v1() noexcept {
    static BudgetRegistry state;
    return &state;
}
}
