// Probe 4 (P2996): reflection writes the C++20 description lines. Migration
// helper: one call per root prints XO_DESCRIBE for every record reachable from
// it (members, nested records, container elements), in dependency order.
#include <cstdio>
#include <set>
#include <string>
#include <vector>
#include "xoffsetdatastructure.hpp"
#include "slg/model_xoffset.hpp"

namespace xo = XOffsetDatastructure;
namespace d = xo::detail;

consteval std::string qualified(std::meta::info r) {
    std::string s(std::meta::identifier_of(r));
    for (auto p = std::meta::parent_of(r); p != ^^::; p = std::meta::parent_of(p))
        s = std::string(std::meta::identifier_of(p)) + "::" + s;
    return s;
}

struct Dumper {
    std::set<std::string> done;
    std::vector<std::string> lines;
    template <class T> void visit() {
        using U = std::remove_cv_t<T>;
        if constexpr (std::is_array_v<U>) visit<std::remove_extent_t<U>>();
        else if constexpr (d::fixed_size_scalar<U> || std::is_enum_v<U> || d::is_fixed_string_v<U>) {}
        else if constexpr (d::is_fixed_flat_map_v<U>) { visit<typename U::key_type>(); visit<typename U::mapped_type>(); }
        else if constexpr (d::is_fixed_flat_set_v<U> || d::is_fixed_vector_v<U>) visit<typename U::value_type>();
        else if constexpr (std::is_class_v<U>) record<U>();
    }
    template <class T> void record() {
        constexpr auto name = std::define_static_string(qualified(^^T));
        if (!done.insert(name).second) return;
        constexpr auto members = std::define_static_array(
            std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()));
        std::string line = std::string("XO_DESCRIBE(") + name;
        template for (constexpr auto m : members) {
            visit<typename [:std::meta::type_of(m):]>();
            line += std::string(", ") + std::string(std::meta::identifier_of(m));
        }
        lines.push_back(line + ")");
    }
};

int main() {
    Dumper dumper;
    dumper.visit<slg::xm::PlayerState>();
    dumper.visit<slg::xm::BattleMsg>();
    dumper.visit<slg::xm::ShardState>();
    for (auto& l : dumper.lines) std::printf("%s\n", l.c_str());
}
