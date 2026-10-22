// Map state demo: check bodies. OWNER: common-library thread. STUB.
#include "mapdemo/checks.hpp"

namespace mapdemo::checks {
static Result not_implemented() { return Result{false, "checks: not implemented"}; }
Result batch_invariants(const UnitStateBatch&, std::uint32_t, std::uint64_t) { return not_implemented(); }
Result compare_with_reference(const UnitStateBatch&, const stdm::UnitStateBatch&) { return not_implemented(); }
void Coverage::observe(const UnitStateBatch&, const UnitStateBatch&) {}
bool Coverage::complete() const noexcept { return false; }
Result compare_snapshot(const ViewSnapshot&, const ViewRequest&, const stdm::UnitStateBatch&) { return not_implemented(); }
Result snapshot_sanity(const ViewSnapshot&, std::uint64_t, const Rect&) { return not_implemented(); }
std::uint64_t snapshot_digest(const ViewSnapshot&) { return 0; }
bool in_buffer(const XBuffer&, const void*, std::size_t) noexcept { return false; }
} // namespace mapdemo::checks
