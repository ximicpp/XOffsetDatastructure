// Device mesh demo: the six types (types.hpp).
// STUB until the types thread fills it in (INTERFACES.md section 7): every
// call fails with "not implemented", so the programs build and link.
#include "xmesh/types.hpp"

namespace xmesh {
namespace {
Error not_implemented(const char* what) { return Error{std::string(what) + ": not implemented yet"}; }
} // namespace

std::uint32_t data_limit(TypeId, Scale) noexcept { return 0; }
std::uint32_t receive_cap(TypeId, Scale) noexcept { return 0; }

Result<Object> make_object(TypeId, std::uint64_t, Scale, std::string_view) { return not_implemented("make_object"); }
Result<Object> receive_object(TypeId, Scale, std::string_view) { return not_implemented("receive_object"); }
std::uint64_t content_digest(Object&) { return 0; }
Status mutate(Object&, std::uint64_t, const Step&) { return not_implemented("mutate"); }
Status check_against_model(Object&, std::uint64_t, Scale, std::span<const Step>, std::uint32_t) {
    return not_implemented("check_against_model");
}
Result<std::string> save(Object&, SaveMode) { return not_implemented("save"); }
std::string summary(Object&) { return {}; }
Result<std::vector<HopExpect>> replay(TypeId, std::uint64_t, Scale, SaveMode, std::span<const Step>) {
    return not_implemented("replay");
}
Result<EvilPart> make_evil(TypeId, std::uint64_t, Scale, SaveMode, std::string_view, EvilCase) {
    return not_implemented("make_evil");
}

} // namespace xmesh
