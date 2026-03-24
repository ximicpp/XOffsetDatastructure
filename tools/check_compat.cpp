// ============================================================================
// XOffsetDatastructure Cross-Platform Compatibility Check
//
// Compares exported .sig.hpp files across platforms.
// This file can be compiled with any C++26 compiler — P2996 is NOT required.
//
// Usage: ./check_compat
// ============================================================================

#include "sigs/x86_64_linux_clang.sig.hpp"

#include <boost/typelayout/tools/compat_check.hpp>

namespace linux_plat = boost::typelayout::platform::x86_64_linux_clang;

using boost::typelayout::compat::layout_match;

// ============================================================================
// Compile-time self-verification (same platform → must match)
// ============================================================================

static_assert(layout_match(linux_plat::Player_layout, linux_plat::Player_layout),
    "Player: self layout mismatch!");

static_assert(layout_match(linux_plat::Item_layout, linux_plat::Item_layout),
    "Item: self layout mismatch!");

static_assert(layout_match(linux_plat::GameData_layout, linux_plat::GameData_layout),
    "GameData: self layout mismatch!");

// ============================================================================
// Runtime report
// When more platforms are added, include their .sig.hpp and add them below.
// ============================================================================

int main() {
    ::boost::typelayout::compat::CompatReporter reporter;
    reporter.add_platform(linux_plat::get_platform_info());
    reporter.print_report();
    return 0;
}
