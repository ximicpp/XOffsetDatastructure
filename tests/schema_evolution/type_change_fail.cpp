// Same field name, other type, no hook: the build must fail and the message
// must name the field and both types.
#include "../../xoffset_evolution.hpp"

namespace old_v { struct Account { std::int32_t id; std::int32_t gold; }; }
namespace new_v { struct Account { std::int32_t id; std::int64_t gold; }; }

int main() {
    XOffsetDatastructure::TypedXBuffer<old_v::Account> source(4096);
    (void)source.make();
    auto target = XOffsetDatastructure::evolve<new_v::Account>(source);
    return target.root().gold == 0 ? 0 : 1;
}
