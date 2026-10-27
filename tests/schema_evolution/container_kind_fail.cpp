// A field that changes container kind (vector to set) needs a hook: without
// one the build fails naming the field.
#include "../../xoffset_evolution.hpp"

using namespace XOffsetDatastructure;
namespace old_v { struct Bag { XVector<std::int32_t> items; }; }
namespace new_v { struct Bag { XSet<std::int32_t> items; }; }

int main() {
    TypedXBuffer<old_v::Bag> source(4096);
    (void)source.make();
    auto target = evolve<new_v::Bag>(source);
    return target.root().items.empty() ? 0 : 1;
}
