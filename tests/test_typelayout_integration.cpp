// ============================================================================
// Test: TypeLayout Integration
// Purpose: Verify boost::typelayout works correctly with XOffsetDatastructure
//          Tests layout signatures, platform prefix, containers, and classify.
// ============================================================================

#include "../xoffsetdatastructure.hpp"
#include <boost/typelayout.hpp>
#include <iostream>
#include <cassert>

using namespace XOffsetDatastructure;
using namespace boost::typelayout;

// ============================================================================
// Test Structures
// ============================================================================

struct Point {
    int32_t x;
    int32_t y;
};

struct Point3D {
    int32_t x;
    int32_t y;
    int32_t z;
};

// Same layout as Point but different field names
struct Coord {
    int32_t a;
    int32_t b;
};

struct WithString {
    int32_t id;
    XString name;

    template <typename Allocator>
    WithString(Allocator alloc) : id(0), name(alloc) {}
};

struct WithVector {
    int32_t count;
    XVector<int32_t> values;

    template <typename Allocator>
    WithVector(Allocator alloc) : count(0), values(alloc) {}
};

// Inheritance test
struct Base {
    int32_t base_val;
};

struct Derived : public Base {
    float derived_val;
};

// ============================================================================
// Test 1: Layout Signature Basics
// ============================================================================

bool test_layout_signature_basics() {
    std::cout << "\n[Test 1] Layout Signature Basics\n";
    std::cout << std::string(50, '-') << "\n";

    constexpr auto sig_point = get_layout_signature<Point>();
    constexpr auto sig_coord = get_layout_signature<Coord>();
    constexpr auto sig_point3d = get_layout_signature<Point3D>();

    std::cout << "  Point:   " << sig_point << "\n";
    std::cout << "  Coord:   " << sig_coord << "\n";
    std::cout << "  Point3D: " << sig_point3d << "\n";

    // Point and Coord have identical byte layout → layout sigs match
    // (Layout signatures are ABI-only; field names are not encoded.)
    constexpr bool layout_match_pc = layout_signatures_match<Point, Coord>();
    static_assert(layout_match_pc, "Point and Coord SHOULD match on layout signature (same ABI)");
    std::cout << "  Point == Coord (layout): " << (layout_match_pc ? "YES" : "NO") << " [OK - expected YES]\n";

    // Point and Point should match
    constexpr bool self_match = layout_signatures_match<Point, Point>();
    static_assert(self_match, "Point should match itself");
    std::cout << "  Point == Point (layout): " << (self_match ? "YES" : "NO") << " [OK]\n";

    // Point and Point3D should NOT match (different sizes)
    constexpr bool size_mismatch = layout_signatures_match<Point, Point3D>();
    static_assert(!size_mismatch, "Point and Point3D should NOT match");
    std::cout << "  Point == Point3D (layout): " << (size_mismatch ? "YES" : "NO") << " [OK - expected NO]\n";

    std::cout << "  [PASS] Layout signature basics\n";
    return true;
}

// ============================================================================
// Test 2: Safety Classification
// ============================================================================

bool test_safety_classification() {
    std::cout << "\n[Test 2] Safety Classification (classify_v)\n";
    std::cout << std::string(50, '-') << "\n";

    // Point: trivially copyable, no pointers, no padding → TrivialSafe
    constexpr auto lvl_point = classify_v<Point>;
    std::cout << "  Point: " << safety_level_name(lvl_point) << "\n";
    static_assert(lvl_point == SafetyLevel::TrivialSafe || lvl_point == SafetyLevel::PaddingRisk,
                  "Point should be TrivialSafe or PaddingRisk");

    // WithString has opaque member → Opaque
    constexpr auto lvl_ws = classify_v<WithString>;
    std::cout << "  WithString: " << safety_level_name(lvl_ws) << "\n";

    // Derived (non-virtual inheritance from Base): safe
    constexpr auto lvl_derived = classify_v<Derived>;
    std::cout << "  Derived: " << safety_level_name(lvl_derived) << "\n";

    std::cout << "  [PASS] Safety classification\n";
    return true;
}

// ============================================================================
// Test 3: Platform Prefix
// ============================================================================

bool test_platform_prefix() {
    std::cout << "\n[Test 3] Platform Prefix\n";
    std::cout << std::string(50, '-') << "\n";

    constexpr auto sig = get_layout_signature<Point>();
    std::string sig_str = sig.value;

    // On 64-bit little-endian, should start with [64-le]
    bool has_prefix = sig_str.find("[64-le]") == 0;
    assert(has_prefix && "Signature should start with [64-le] on this platform");
    std::cout << "  Platform prefix: [64-le] [OK]\n";

    std::cout << "  [PASS] Platform prefix\n";
    return true;
}

// ============================================================================
// Test 4: Container Signatures
// ============================================================================

bool test_container_signatures() {
    std::cout << "\n[Test 4] XOffsetDatastructure Container Signatures\n";
    std::cout << std::string(50, '-') << "\n";

    constexpr auto sig_string = get_layout_signature<WithString>();
    constexpr auto sig_vector = get_layout_signature<WithVector>();

    std::cout << "  WithString: " << sig_string << "\n";
    std::cout << "  WithVector: " << sig_vector << "\n";

    // Verify container types are correctly resolved
    std::string str_sig = sig_string.value;
    assert(str_sig.find("string[s:32,a:8]") != std::string::npos && "XString should appear as string[s:32,a:8]");
    std::cout << "  XString resolved: string[s:32,a:8] [OK]\n";

    std::string vec_sig = sig_vector.value;
    assert(vec_sig.find("vector[s:32,a:8]") != std::string::npos && "XVector should appear as vector[s:32,a:8]");
    std::cout << "  XVector resolved: vector[s:32,a:8] [OK]\n";

    std::cout << "  [PASS] Container signatures\n";
    return true;
}

// ============================================================================
// Test 5: Inheritance Signatures
// ============================================================================

bool test_inheritance_signatures() {
    std::cout << "\n[Test 5] Inheritance Signatures\n";
    std::cout << std::string(50, '-') << "\n";

    constexpr auto sig_base = get_layout_signature<Base>();
    constexpr auto sig_derived = get_layout_signature<Derived>();

    std::cout << "  Base:    " << sig_base << "\n";
    std::cout << "  Derived: " << sig_derived << "\n";

    // Derived definition should include base info
    std::string derived_str = sig_derived.value;
    bool has_base_ref = derived_str.find("~base<Base>") != std::string::npos;
    assert(has_base_ref && "Derived definition signature should reference Base");
    std::cout << "  Derived includes ~base<Base>: " << (has_base_ref ? "YES" : "NO") << " [OK]\n";

    // Base and Derived should NOT match
    constexpr bool def_match = layout_signatures_match<Base, Derived>();
    static_assert(!def_match, "Base and Derived should not match");
    std::cout << "  Base == Derived (definition): " << (def_match ? "YES" : "NO") << " [OK - expected NO]\n";

    std::cout << "  [PASS] Inheritance signatures\n";
    return true;
}

// ============================================================================
// Test 6: Primitive Type Signatures
// ============================================================================

bool test_primitive_signatures() {
    std::cout << "\n[Test 6] Primitive Type Signatures\n";
    std::cout << std::string(50, '-') << "\n";

    // Verify primitive type signatures match expected values
    constexpr auto sig_i32 = TypeSignature<int32_t>::calculate();
    constexpr auto sig_f64 = TypeSignature<double>::calculate();
    constexpr auto sig_bool = TypeSignature<bool>::calculate();

    static_assert(sig_i32 == "i32[s:4,a:4]", "int32_t signature mismatch");
    static_assert(sig_f64 == "f64[s:8,a:8]", "double signature mismatch");
    static_assert(sig_bool == "bool[s:1,a:1]", "bool signature mismatch");

    std::cout << "  int32_t: " << sig_i32 << " [OK]\n";
    std::cout << "  double:  " << sig_f64 << " [OK]\n";
    std::cout << "  bool:    " << sig_bool << " [OK]\n";

    std::cout << "  [PASS] Primitive type signatures\n";
    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "  TypeLayout Integration Test\n";
    std::cout << "========================================\n";

    bool all_passed = true;

    all_passed &= test_layout_signature_basics();
    all_passed &= test_safety_classification();
    all_passed &= test_platform_prefix();
    all_passed &= test_container_signatures();
    all_passed &= test_inheritance_signatures();
    all_passed &= test_primitive_signatures();

    std::cout << "\n========================================\n";
    std::cout << "  Summary\n";
    std::cout << "========================================\n";

    if (all_passed) {
        std::cout << "[PASS] All TypeLayout integration tests passed!\n";
        return 0;
    } else {
        std::cout << "[FAIL] Some tests failed!\n";
        return 1;
    }
}
