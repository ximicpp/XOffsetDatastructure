// A name in xevolve<Old, New>::handles that neither version has must fail the
// build, naming the entry.
#include "../../xoffset_evolution.hpp"

namespace old_v { struct Account { std::int32_t id; std::int32_t gold; }; }
namespace new_v { struct Account { std::int32_t id; std::int32_t gold; std::int32_t gems; }; }

template <> struct XOffsetDatastructure::xevolve<old_v::Account, new_v::Account> {
    static constexpr std::string_view handles[] = {"gold", "glod"};
    static void upgrade(const old_v::Account& from, new_v::Account& to, XEvolveContext&) { to.gold = from.gold; }
    static void downgrade(const new_v::Account& from, old_v::Account& to, XEvolveContext&) { to.gold = from.gold; }
};

int main() {
    XOffsetDatastructure::TypedXBuffer<old_v::Account> source(4096);
    (void)source.make();
    auto target = XOffsetDatastructure::evolve<new_v::Account>(source);
    return target.root().gold == 0 ? 0 : 1;
}
