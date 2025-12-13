// demo.cpp - Type layout signature usage examples

#include <iostream>
#include <cstdint>
#include "../include/typelayout.hpp"

using namespace typelayout;

// Example types
struct Point { int32_t x, y; };
struct Player {
    uint64_t id;
    char name[32];
    Point pos;
    float health;
};

struct Vec2 { int32_t x, y; };  // Same layout as Point

// Compile-time verification
TYPELAYOUT_ASSERT_MATCH(Point, "struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}");
TYPELAYOUT_ASSERT_COMPATIBLE(Point, Vec2);

int main() {
    std::cout << "=== Layout Signature Demo ===\n\n";
    
    // 1. Get signature
    std::cout << "Point:\n  " << get_layout_signature_cstr<Point>() << "\n\n";
    std::cout << "Player:\n  " << get_layout_signature_cstr<Player>() << "\n\n";
    
    // 2. Compare signatures
    constexpr bool match = signatures_match<Point, Vec2>();
    std::cout << "Point == Vec2: " << (match ? "yes" : "no") << "\n\n";
    
    // 3. Primitives
    std::cout << "int32_t: " << get_layout_signature_cstr<int32_t>() << "\n";
    std::cout << "double:  " << get_layout_signature_cstr<double>() << "\n";
    std::cout << "void*:   " << get_layout_signature_cstr<void*>() << "\n\n";
    
    std::cout << "All static_assert passed.\n";
    return 0;
}