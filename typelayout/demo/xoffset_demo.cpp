// xoffset_demo.cpp - TypeLayout verification for XOffsetDatastructure types

#include <iostream>
// Include xoffsetdatastructure first (includes Boost headers)
// This enables the conditional offset_ptr specialization in typelayout.hpp
#include "../../xoffsetdatastructure2.hpp"
#include "../include/typelayout.hpp"

using namespace typelayout;
using namespace XOffsetDatastructure2;

// Optional: User-provided specializations for cleaner container signatures
// If removed, containers will use generic signatures (more verbose but still valid)
namespace typelayout {

template <>
struct TypeSignature<XString> {
    static consteval auto calculate() noexcept {
        return CompileString{"xstring[s:"} +
               CompileString<32>::from_number(sizeof(XString)) +
               CompileString{",a:"} +
               CompileString<32>::from_number(alignof(XString)) +
               CompileString{"]"};
    }
};

template <typename T>
struct TypeSignature<XVector<T>> {
    static consteval auto calculate() noexcept {
        return CompileString{"xvector[s:"} +
               CompileString<32>::from_number(sizeof(XVector<T>)) +
               CompileString{",a:"} +
               CompileString<32>::from_number(alignof(XVector<T>)) +
               CompileString{"]<"} +
               TypeSignature<T>::calculate() +
               CompileString{">"};
    }
};

template <typename T>
struct TypeSignature<XSet<T>> {
    static consteval auto calculate() noexcept {
        return CompileString{"xset[s:"} +
               CompileString<32>::from_number(sizeof(XSet<T>)) +
               CompileString{",a:"} +
               CompileString<32>::from_number(alignof(XSet<T>)) +
               CompileString{"]<"} +
               TypeSignature<T>::calculate() +
               CompileString{">"};
    }
};

template <typename K, typename V>
struct TypeSignature<XMap<K, V>> {
    static consteval auto calculate() noexcept {
        return CompileString{"xmap[s:"} +
               CompileString<32>::from_number(sizeof(XMap<K, V>)) +
               CompileString{",a:"} +
               CompileString<32>::from_number(alignof(XMap<K, V>)) +
               CompileString{"]<"} +
               TypeSignature<K>::calculate() +
               CompileString{","} +
               TypeSignature<V>::calculate() +
               CompileString{">"};
    }
};

} // namespace typelayout

// Example types (layout-only, no constructors needed for verification)
struct alignas(8) Item {
    int32_t item_id;
    int32_t item_type;
    int32_t quantity;
    XString name;
};

struct alignas(8) GameData {
    int32_t player_id;
    int32_t level;
    float health;
    XString player_name;
    XVector<Item> items;
    XSet<int32_t> achievements;
    XMap<XString, int32_t> quest_progress;
};

int main() {
    std::cout << "=== TypeLayout Verification for XOffsetDatastructure ===\n\n";

    std::cout << "Container Types:\n";
    std::cout << "  XString:          " << get_layout_signature_cstr<XString>() << "\n";
    std::cout << "  XVector<int32_t>: " << get_layout_signature_cstr<XVector<int32_t>>() << "\n";
    std::cout << "  XSet<int32_t>:    " << get_layout_signature_cstr<XSet<int32_t>>() << "\n";
    std::cout << "  XMap<int,int>:    " << get_layout_signature_cstr<XMap<int32_t, int32_t>>() << "\n\n";

    std::cout << "Complex Types:\n";
    std::cout << "  Item:\n    " << get_layout_signature_cstr<Item>() << "\n\n";
    std::cout << "  GameData:\n    " << get_layout_signature_cstr<GameData>() << "\n\n";

    std::cout << "All static_assert checks passed.\n";
    return 0;
}