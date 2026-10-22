// Map state demo: headless self-test. OWNER: common-library thread. STUB until implemented.
#include <cstdio>
#include "mapdemo/checks.hpp"
#include "mapdemo/metrics.hpp"
#include "mapdemo/net.hpp"
#include "mapdemo/rules.hpp"
#include "mapdemo/std_model.hpp"

// Compile-only: the rule templates must instantiate on both models.
[[maybe_unused]] static void instantiate_rules(mapdemo::UnitStateBatch& x, mapdemo::stdm::UnitStateBatch& s) {
    mapdemo::rules::init_batch(x, 1, 1);
    mapdemo::rules::init_batch(s, 1, 1);
    mapdemo::rules::step_batch(x, 1);
    mapdemo::rules::step_batch(s, 1);
}

int main() {
    std::fprintf(stderr, "selftest: not implemented yet\n");
    return 2;
}
