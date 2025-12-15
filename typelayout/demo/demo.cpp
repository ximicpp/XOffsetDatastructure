// demo.cpp - typelayout usage examples

#include <iostream>
#include <cstdint>
#include "../include/typelayout.hpp"

using namespace typelayout;

// Data structures
struct Point { int32_t x, y; };
struct Player {
    uint64_t id;
    char name[32];
    Point pos;
    float health;
};

// Bind types to "golden" layout signatures (compilation fails if layout changes)
TYPELAYOUT_BIND(Point, "struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}");
TYPELAYOUT_BIND(Player, "struct[s:56,a:8]{@0[id]:u64[s:8,a:8],@8[name]:bytes[s:32,a:1],@40[pos]:struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]},@48[health]:f32[s:4,a:4]}");

// Layout compatibility check
struct Vec2 { int32_t x, y; };
static_assert(signatures_match<Point, Vec2>(), "Point and Vec2 must have same layout");

// Portability checks
static_assert(is_portable<Point>(), "Point must be portable");
static_assert(is_portable<Player>(), "Player must be portable");

// Template constraint using layout signature
template<typename T>
    requires LayoutMatch<T, "struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}">
void send_point(const T& p) {
    std::cout << "Sending point-like data...\n";
    (void)p;
}

// Compile-time hash values
constexpr uint64_t POINT_LAYOUT_HASH = get_layout_hash<Point>();
constexpr uint64_t PLAYER_LAYOUT_HASH = get_layout_hash<Player>();
static_assert(hashes_match<Point, Vec2>(), "Point and Vec2 must have same layout hash");

// Template constraint using hash
template<typename T>
    requires LayoutHashMatch<T, POINT_LAYOUT_HASH>
void process_point_data(const T& p) {
    std::cout << "Processing point data (hash validated)...\n";
    (void)p;
}

int main() {
    std::cout << "=== typelayout Demo ===\n\n";
    
    // Layout signatures
    std::cout << "Point:  " << get_layout_signature_cstr<Point>() << "\n";
    std::cout << "Player: " << get_layout_signature_cstr<Player>() << "\n\n";
    
    // Compatibility
    std::cout << "Point == Vec2: " << (signatures_match<Point, Vec2>() ? "yes" : "no") << "\n\n";
    
    // Primitives
    std::cout << "int32_t: " << get_layout_signature_cstr<int32_t>() << "\n";
    std::cout << "double:  " << get_layout_signature_cstr<double>() << "\n";
    std::cout << "void*:   " << get_layout_signature_cstr<void*>() << "\n\n";
    
    // Template with layout constraint
    Point p{10, 20};
    Vec2 v{30, 40};
    send_point(p);
    send_point(v);
    
    // Layout hashes
    std::cout << "\nPoint hash:  0x" << std::hex << POINT_LAYOUT_HASH << "\n";
    std::cout << "Vec2 hash:   0x" << get_layout_hash<Vec2>() << "\n";
    std::cout << "Player hash: 0x" << PLAYER_LAYOUT_HASH << std::dec << "\n";
    
    // Template with hash constraint
    process_point_data(p);
    process_point_data(v);
    
    std::cout << "\nAll compile-time checks passed!\n";
    return 0;
}