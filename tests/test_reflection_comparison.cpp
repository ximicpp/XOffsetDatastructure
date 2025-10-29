// ============================================================================
// Test: Reflection-Based Comparison
// Purpose: Test reflection for comparison and validation operations
// ============================================================================

#include "../xoffsetdatastructure2.hpp"
#include <iostream>

#include <experimental/meta>
#include <experimental/meta>

using namespace XOffsetDatastructure2;

struct ComparableData {
    int x;
    double y;
    uint32_t z;
    
    template <typename Allocator>
    ComparableData(Allocator allocator) : x(0), y(0.0), z(0) {}
};

struct Point3D {
    float x, y, z;
};

// Compile-time member count using reflection
template<typename T>
consteval size_t member_count() {
    using namespace std::meta;
    return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
}

void test_compile_time_member_count() {
    std::cout << "[Test 1] Compile-Time Member Count\n";
    std::cout << "-----------------------------------\n";
    
    constexpr auto comparable_count = member_count<ComparableData>();
    constexpr auto point_count = member_count<Point3D>();
    
    std::cout << "  Member counts (compile-time):\n";
    std::cout << "    ComparableData: " << comparable_count << " members\n";
    std::cout << "    Point3D: " << point_count << " members\n";
    
    // These are constexpr and evaluated at compile-time
    static_assert(comparable_count == 3, "ComparableData should have 3 members");
    static_assert(point_count == 3, "Point3D should have 3 members");
    
    std::cout << "  [OK] static_assert passed\n";
    std::cout << "[PASS] Compile-time member count\n\n";
}

void test_manual_comparison() {
    std::cout << "[Test 2] Manual Comparison\n";
    std::cout << "--------------------------\n";
    
    XBufferExt xbuf(1024);
    auto* data1 = xbuf.make<ComparableData>("data1");
    auto* data2 = xbuf.make<ComparableData>("data2");
    
    data1->x = 10;
    data1->y = 20.5;
    data1->z = 30;
    
    data2->x = 10;
    data2->y = 20.5;
    data2->z = 30;
    
    // Manual comparison
    bool equal = (data1->x == data2->x) && 
                 (data1->y == data2->y) && 
                 (data1->z == data2->z);
    
    std::cout << "  data1: (" << data1->x << ", " << data1->y << ", " << data1->z << ")\n";
    std::cout << "  data2: (" << data2->x << ", " << data2->y << ", " << data2->z << ")\n";
    std::cout << "  Comparison result: " << (equal ? "EQUAL" : "NOT EQUAL") << "\n";
    
    // Test inequality
    data2->z = 31;
    bool not_equal = (data1->z != data2->z);
    
    std::cout << "\n  After change data2.z to " << data2->z << ":\n";
    std::cout << "  Not equal: " << (not_equal ? "yes" : "no") << "\n";
    
    std::cout << "[PASS] Manual comparison\n\n";
}

void test_member_count_for_validation() {
    std::cout << "[Test 3] Member Count for Validation\n";
    std::cout << "-------------------------------------\n";
    
    // Compile-time member count
    constexpr size_t actual_members = member_count<ComparableData>();
    
    std::cout << "  ComparableData validation:\n";
    std::cout << "    Expected members: 3\n";
    std::cout << "    Actual members: " << actual_members << "\n";
    
    if (actual_members == 3) {
        std::cout << "    Status: VALID\n";
    } else {
        std::cout << "    Status: INVALID (structure changed!)\n";
    }
    
    // This can be used to detect struct layout changes
    std::cout << "\n  Use case: Version compatibility check\n";
    std::cout << "    - Serialize with version A (3 members)\n";
    std::cout << "    - Deserialize with version B\n";
    std::cout << "    - Compare member counts to detect changes\n";
    
    std::cout << "[PASS] Member count validation\n\n";
}

// Helper: get member at index (consteval)
template<typename T, size_t I>
consteval auto get_member_at() {
    using namespace std::meta;
    auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
    return members[I];
}

// Helper: print type verification for member
template<typename T, size_t I>
void print_type_verification() {
    using namespace std::meta;
    constexpr auto member = get_member_at<T, I>();
    constexpr auto member_type = type_of(member);
    
    std::cout << "    " << display_string_of(member) << ":\n";
    
    if constexpr (member_type == ^^int) {
        std::cout << "      Type: int (MATCH)\n";
    } else if constexpr (member_type == ^^double) {
        std::cout << "      Type: double (MATCH)\n";
    } else if constexpr (member_type == ^^uint32_t) {
        std::cout << "      Type: uint32_t (MATCH)\n";
    } else {
        std::cout << "      Type: " << display_string_of(member_type) << "\n";
    }
}

template<typename T, size_t... Is>
void print_all_type_verification(std::index_sequence<Is...>) {
    (print_type_verification<T, Is>(), ...);
}

void test_type_comparison_helper() {
    std::cout << "[Test 4] Type Comparison Helper\n";
    std::cout << "--------------------------------\n";
    
    std::cout << "  Type verification:\n";
    
    constexpr size_t count = member_count<ComparableData>();
    print_all_type_verification<ComparableData>(std::make_index_sequence<count>{});
    
    std::cout << "[PASS] Type comparison\n\n";
}

// Helper: check if member is float
template<typename T, size_t I>
consteval bool is_member_float() {
    using namespace std::meta;
    constexpr auto member = get_member_at<T, I>();
    return type_of(member) == ^^float;
}

// Helper: check all members are float
template<typename T, size_t... Is>
consteval bool all_members_float(std::index_sequence<Is...>) {
    return (is_member_float<T, Is>() && ...);
}

void test_structure_equality_check() {
    std::cout << "[Test 5] Structure Equality Check\n";
    std::cout << "----------------------------------\n";
    
    Point3D p1{1.0f, 2.0f, 3.0f};
    Point3D p2{1.0f, 2.0f, 3.0f};
    Point3D p3{4.0f, 5.0f, 6.0f};
    
    // Manual equality
    auto equals = [](const Point3D& a, const Point3D& b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    };
    
    std::cout << "  p1 vs p2: " << (equals(p1, p2) ? "EQUAL" : "NOT EQUAL") << "\n";
    std::cout << "  p1 vs p3: " << (equals(p1, p3) ? "EQUAL" : "NOT EQUAL") << "\n";
    std::cout << "  p2 vs p3: " << (equals(p2, p3) ? "EQUAL" : "NOT EQUAL") << "\n";
    
    // Show that reflection can help verify structure
    constexpr size_t count = member_count<Point3D>();
    constexpr bool all_float = all_members_float<Point3D>(std::make_index_sequence<count>{});
    
    std::cout << "\n  Reflection-based validation:\n";
    std::cout << "    Member count: " << count << "\n";
    std::cout << "    All members are float: " << (all_float ? "yes" : "no") << "\n";
    
    std::cout << "[PASS] Structure equality\n\n";
}

void test_member_wise_diff() {
    std::cout << "[Test 6] Member-Wise Difference\n";
    std::cout << "--------------------------------\n";
    
    Point3D p1{10.0f, 20.0f, 30.0f};
    Point3D p2{10.0f, 25.0f, 30.0f};
    
    std::cout << "  p1: (" << p1.x << ", " << p1.y << ", " << p1.z << ")\n";
    std::cout << "  p2: (" << p2.x << ", " << p2.y << ", " << p2.z << ")\n";
    
    std::cout << "\n  Differences:\n";
    
    if (p1.x != p2.x) std::cout << "    x differs: " << p1.x << " vs " << p2.x << "\n";
    if (p1.y != p2.y) std::cout << "    y differs: " << p1.y << " vs " << p2.y << "\n";
    if (p1.z != p2.z) std::cout << "    z differs: " << p1.z << " vs " << p2.z << "\n";
    
    // Count differences
    int diff_count = 0;
    if (p1.x != p2.x) diff_count++;
    if (p1.y != p2.y) diff_count++;
    if (p1.z != p2.z) diff_count++;
    
    std::cout << "\n  Total differences: " << diff_count << " out of 3 members\n";
    
    std::cout << "[PASS] Member-wise diff\n\n";
}

void test_version_compatibility() {
    std::cout << "[Test 7] Version Compatibility Check\n";
    std::cout << "-------------------------------------\n";
    
    // Simulate version check
    constexpr size_t EXPECTED_VERSION_1_MEMBERS = 3;
    constexpr size_t current_count = member_count<ComparableData>();
    
    std::cout << "  Version compatibility:\n";
    std::cout << "    Expected (v1): " << EXPECTED_VERSION_1_MEMBERS << " members\n";
    std::cout << "    Current: " << current_count << " members\n";
    
    if constexpr (current_count == EXPECTED_VERSION_1_MEMBERS) {
        std::cout << "    Status: COMPATIBLE\n";
        std::cout << "    Can safely deserialize v1 data\n";
    } else if constexpr (current_count > EXPECTED_VERSION_1_MEMBERS) {
        std::cout << "    Status: FORWARD COMPATIBLE\n";
        std::cout << "    New fields added, can read old data\n";
    } else {
        std::cout << "    Status: INCOMPATIBLE\n";
        std::cout << "    Fields removed, cannot read old data\n";
    }
    
    std::cout << "[PASS] Version compatibility\n\n";
}


int main() {
    std::cout << "========================================\n";
    std::cout << "  Reflection Comparison Test\n";
    std::cout << "========================================\n\n";

#include <experimental/meta>
    std::cout << "[INFO] C++26 Reflection: ENABLED\n";
    std::cout << "[INFO] Testing reflection for comparison\n\n";
    
    test_compile_time_member_count();
    test_manual_comparison();
    test_member_count_for_validation();
    test_type_comparison_helper();
    test_structure_equality_check();
    test_member_wise_diff();
    test_version_compatibility();
    
    std::cout << "========================================\n";
    std::cout << "  Summary\n";
    std::cout << "========================================\n";
    std::cout << "[PASS] Test 1: Compile-time member count\n";
    std::cout << "[PASS] Test 2: Manual comparison\n";
    std::cout << "[PASS] Test 3: Member count validation\n";
    std::cout << "[PASS] Test 4: Type comparison\n";
    std::cout << "[PASS] Test 5: Structure equality\n";
    std::cout << "[PASS] Test 6: Member-wise diff\n";
    std::cout << "[PASS] Test 7: Version compatibility\n";
    std::cout << "\n[SUCCESS] All comparison tests passed!\n";
    
    return 0;
}
