// H1 gate V6: print the compile-time decisions the library uses to pick a code
// path, for every record type of the SLG models (and of the formal acceptance
// model when bench/formal is present). Build it with GCC 16 and with
// clang-p2996 and diff the outputs: every line not starting with '#' must be
// identical. A difference means the two compilers take different paths for the
// same type, and must be fixed before timing (H1 design section 6).
//
//   path_audit            decisions, one "type predicate value" line each
//   lines starting '#'    compiler and feature-macro information (not compared)
#include <cstdio>
#include <string_view>
#include <type_traits>
#include "slg/model_xoffset.hpp"
#include "slg/model_xoffset_optin.hpp"
#if __has_include("xoffset/adapter.hpp")
#include "xoffset/adapter.hpp"  // bench/formal: the formal acceptance model (sc::xo_detail)
#define H1_FORMAL_MODEL 1
#endif

namespace xo = XOffsetDatastructure;
namespace xd = XOffsetDatastructure::detail;
namespace xf = XOffsetDatastructure::fixed_detail;

namespace {

void line(const char* type, const char* what, long long v) { std::printf("%s %s %lld\n", type, what, v); }

// Decisions for a record type (container element types, message roots).
template <class T> void record(const char* name) {
    line(name, "sizeof", sizeof(T));
    line(name, "alignof", alignof(T));
    line(name, "is_trivially_copyable", std::is_trivially_copyable_v<T>);
    line(name, "is_trivially_destructible", std::is_trivially_destructible_v<T>);
    line(name, "has_implicit_padding_v", xo::has_implicit_padding_v<T>);
    line(name, "wire_value_needs_validation_v", xd::wire_value_needs_validation_v<T>);
    line(name, "can_relocate_without_allocation", xf::can_relocate_without_allocation<T>());
    line(name, "stable_erase_supported", xf::stable_erase_supported<T>());
    line(name, "shift_relocate_supported", xd::shift_relocate_supported<T>());
    line(name, "receive_raw_supported", xd::receive_raw_supported<T>());
    line(name, "receive_raw_future_supported", xd::receive_raw::future_supported_v<T>);
    line(name, "raw_needs_validation", xd::raw_needs_validation<T>());
    line(name, "raw_implicit_lifetime", xd::raw_implicit_lifetime<T>());
    line(name, "StableEraseOrdinaryMembers", xo::StableEraseOrdinaryMembers<T>::value);
}

// Decisions for every non-static data member's type (containers, strings, scalars).
template <class T> void members(const char* name) {
    template for (constexpr auto m : std::define_static_array(
                      std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
        using M = typename[:std::meta::type_of(m):];
        std::printf("%s.%s offset %zu\n", name, std::meta::identifier_of(m).data(), std::meta::offset_of(m).bytes);
        std::printf("%s.%s sizeof %zu\n", name, std::meta::identifier_of(m).data(), sizeof(M));
        std::printf("%s.%s has_implicit_padding_v %d\n", name, std::meta::identifier_of(m).data(),
                    int(xo::has_implicit_padding_v<M>));
        std::printf("%s.%s wire_value_needs_validation_v %d\n", name, std::meta::identifier_of(m).data(),
                    int(xd::wire_value_needs_validation_v<M>));
        std::printf("%s.%s is_trivially_copyable %d\n", name, std::meta::identifier_of(m).data(),
                    int(std::is_trivially_copyable_v<M>));
    }
}

template <class T> void audit(const char* name) {
    record<T>(name);
    members<T>(name);
}

#define H1_MODEL(ns)                                                                    \
    audit<ns::Buff>(#ns "::Buff");               audit<ns::Skill>(#ns "::Skill");       \
    audit<ns::Troop>(#ns "::Troop");             audit<ns::Hero>(#ns "::Hero");         \
    audit<ns::Army>(#ns "::Army");               audit<ns::City>(#ns "::City");         \
    audit<ns::PlayerState>(#ns "::PlayerState"); audit<ns::Event>(#ns "::Event");       \
    audit<ns::Report>(#ns "::Report");           audit<ns::BattleMsg>(#ns "::BattleMsg"); \
    audit<ns::Tile>(#ns "::Tile");               audit<ns::March>(#ns "::March");       \
    audit<ns::ShardState>(#ns "::ShardState");

} // namespace

int main() {
#if defined(__clang__)
    std::printf("# compiler clang %s\n", __clang_version__);
#elif defined(__GNUC__)
    std::printf("# compiler gcc %s\n", __VERSION__);
#endif
#if defined(_LIBCPP_VERSION)
    std::printf("# stdlib libc++ %d\n", _LIBCPP_VERSION);
#elif defined(__GLIBCXX__)
    std::printf("# stdlib libstdc++ %d\n", __GLIBCXX__);
#endif
#if defined(__cpp_lib_is_implicit_lifetime)
    std::printf("# __cpp_lib_is_implicit_lifetime %ld\n", long(__cpp_lib_is_implicit_lifetime));
#else
    std::printf("# __cpp_lib_is_implicit_lifetime undefined\n");
#endif
#if defined(_GLIBCXX_ASSERTIONS)
    std::printf("# _GLIBCXX_ASSERTIONS defined\n");
#endif
#if defined(_LIBCPP_HARDENING_MODE)
    std::printf("# _LIBCPP_HARDENING_MODE %d\n", int(_LIBCPP_HARDENING_MODE));
#endif
#if defined(NDEBUG)
    std::printf("# NDEBUG defined\n");
#endif
    H1_MODEL(slg::xm)
    H1_MODEL(slg::xmr)
#if H1_FORMAL_MODEL
    audit<sc::xo_detail::Attr>("sc::xo_detail::Attr");
    audit<sc::xo_detail::Record>("sc::xo_detail::Record");
    audit<sc::xo_detail::Snapshot>("sc::xo_detail::Snapshot");
#else
    std::printf("# formal model not found (bench/formal)\n");
#endif
    return 0;
}
