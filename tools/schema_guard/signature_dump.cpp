// Native candidate discovery. Output is NOT an approved baseline.
#include "xoffsetdatastructure.hpp"
#include <boost/typelayout/signature.hpp>
#include XSG_CATALOG
#include <iostream>
#include <string_view>
#define XSG_PRINT(T, PIN, TYPE_PIN) { \
    constexpr auto w = ::XOffsetDatastructure::wire_abi_signature<T>(); \
    constexpr auto t = ::boost::typelayout::get_layout_signature<T>(); \
    std::cout << #T << "\t" << std::string_view(w) << "\t" << std::string_view(t.value, t.size) << "\n"; }
int main() { XSG_SCHEMA_ROOTS(XSG_PRINT) }
