// Native discovery tool for the frozen benchmark fixture, not a platform verdict.
#include "bench/slg/include/slg/model_xoffset.hpp"
#include <boost/typelayout/signature.hpp>
#include <iostream>
#include <string_view>
template<class T> void emit(const char* name) {
    constexpr auto wire = XOffsetDatastructure::wire_abi_signature<T>();
    constexpr auto type = boost::typelayout::get_layout_signature<T>();
    std::cout << name << "\t" << std::string_view(wire) << "\t"
              << std::string_view(type.value, type.size) << "\n";
}
int main() {
    emit<slg::xm::PlayerState>("slg::xm::PlayerState");
    emit<slg::xm::BattleMsg>("slg::xm::BattleMsg");
    emit<slg::xm::ShardState>("slg::xm::ShardState");
    emit<slg::xm::Report>("slg::xm::Report");
}
