// Verifies that every baseline under tools/sigs remains transfer-safe.

#include "check_compat_baselines.hpp"
#include "signature_type_names.hpp"

#include <iostream>

int main() {
    ::boost::typelayout::compat::CompatReporter reporter;
    xoffset::signature::add_baseline_platforms(reporter);

    if constexpr (xoffset::signature::baseline_platform_count == 0) {
        std::cerr << "No signature baselines found under tools/sigs.\n";
        return 1;
    }

    const auto type_names = xoffset::signature::exported_type_names();
    const auto platform_names = xoffset::signature::baseline_platform_names();

    if (!reporter.are_transfer_safe(type_names, platform_names)) {
        reporter.print_diff_report(std::cerr);
        return 1;
    }

    reporter.print_report();
    return 0;
}
