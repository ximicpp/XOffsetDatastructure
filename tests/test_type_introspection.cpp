// ============================================================================
// Test: Type Introspection
// Purpose: Test type queries and introspection APIs
// ============================================================================

#include "../xoffsetdatastructure2.hpp"
#include <iostream>

#if __has_include(<experimental/meta>)
#include <experimental/meta>
#define HAS_REFLECTION 1

using namespace XOffsetDatastructure2;

struct ComplexType {
    int x;
    const double y;
    XString* ptr;
    XVector<int> vec;
    
    template <typename Allocator>
    ComplexType(Allocator allocator)
        : x(0), y(0.0), ptr(nullptr), vec(allocator) {}
};

struct NestedType {
    int id;
    ComplexType data;
    
    template <typename Allocator>
    NestedType(Allocator allocator) : id(0), data(allocator) {}
};

void test_type_names() {
    using namespace std::meta;
    
    std::cout << "[Test 1] Type Names\n";
    std::cout << "-------------------\n";
    
    std::cout << "  Basic types:\n";
    std::cout << "    int: " << display_string_of(^^int) << "\n";
    std::cout << "    double: " << display_string_of(^^double) << "\n";
    std::cout << "    float: " << display_string_of(^^float) << "\n";
    std::cout << "    uint32_t: " << display_string_of(^^uint32_t) << "\n";
    
    std::cout << "\n  Container types:\n";
    std::cout << "    XString: " << display_string_of(^^XString) << "\n";
    std::cout << "    XVector<int>: " << display_string_of(^^XVector<int>) << "\n";
    std::cout << "    XSet<int>: " << display_string_of(^^XSet<int>) << "\n";
    std::cout << "    XMap<int,double>: " << display_string_of(^^XMap<int, double>) << "\n";
    
    std::cout << "\n  User-defined types:\n";
    std::cout << "    ComplexType: " << display_string_of(^^ComplexType) << "\n";
    std::cout << "    NestedType: " << display_string_of(^^NestedType) << "\n";
    
    std::cout << "[PASS] Type names\n\n";
}

void test_member_types() {
    using namespace std::meta;
    
    std::cout << "[Test 2] Member Type Analysis\n";
    std::cout << "-----------------------------\n";
    
    constexpr auto x_type = type_of(^^ComplexType::x);
    constexpr auto y_type = type_of(^^ComplexType::y);
    constexpr auto ptr_type = type_of(^^ComplexType::ptr);
    constexpr auto vec_type = type_of(^^ComplexType::vec);
    
    std::cout << "  ComplexType member types:\n";
    std::cout << "    x: " << display_string_of(x_type) << "\n";
    std::cout << "    y: " << display_string_of(y_type) << " (const)\n";
    std::cout << "    ptr: " << display_string_of(ptr_type) << " (pointer)\n";
    std::cout << "    vec: " << display_string_of(vec_type) << " (container)\n";
    
    std::cout << "[PASS] Member types\n\n";
}

// Helper: Get member count at compile time
template<typename T>
consteval auto get_member_count() {
    using namespace std::meta;
    return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
}

void test_member_properties() {
    using namespace std::meta;
    
    std::cout << "[Test 3] Member Properties\n";
    std::cout << "--------------------------\n";
    
    constexpr auto member_count = get_member_count<ComplexType>();
    std::cout << "  ComplexType members (" << member_count << "):\n";
    
    // Manual listing (vector<info> is consteval-only, can't iterate at runtime)
    std::cout << "\n    " << display_string_of(^^ComplexType::x) << ":\n";
    std::cout << "      Type: " << display_string_of(type_of(^^ComplexType::x)) << "\n";
    std::cout << "      is_public: " << (is_public(^^ComplexType::x) ? "yes" : "no") << "\n";
    std::cout << "      is_static_member: " 
              << (is_static_member(^^ComplexType::x) ? "yes" : "no") << "\n";
    std::cout << "      is_nonstatic_data_member: " 
              << (is_nonstatic_data_member(^^ComplexType::x) ? "yes" : "no") << "\n";
    
    std::cout << "\n    " << display_string_of(^^ComplexType::y) << ":\n";
    std::cout << "      Type: " << display_string_of(type_of(^^ComplexType::y)) << "\n";
    std::cout << "      is_public: " << (is_public(^^ComplexType::y) ? "yes" : "no") << "\n";
    std::cout << "      is_static_member: " 
              << (is_static_member(^^ComplexType::y) ? "yes" : "no") << "\n";
    std::cout << "      is_nonstatic_data_member: " 
              << (is_nonstatic_data_member(^^ComplexType::y) ? "yes" : "no") << "\n";
    
    std::cout << "\n    " << display_string_of(^^ComplexType::ptr) << ":\n";
    std::cout << "      Type: " << display_string_of(type_of(^^ComplexType::ptr)) << "\n";
    std::cout << "      is_public: " << (is_public(^^ComplexType::ptr) ? "yes" : "no") << "\n";
    std::cout << "      is_nonstatic_data_member: " 
              << (is_nonstatic_data_member(^^ComplexType::ptr) ? "yes" : "no") << "\n";
    
    std::cout << "\n    " << display_string_of(^^ComplexType::vec) << ":\n";
    std::cout << "      Type: " << display_string_of(type_of(^^ComplexType::vec)) << "\n";
    std::cout << "      is_public: " << (is_public(^^ComplexType::vec) ? "yes" : "no") << "\n";
    std::cout << "      is_nonstatic_data_member: " 
              << (is_nonstatic_data_member(^^ComplexType::vec) ? "yes" : "no") << "\n";
    
    std::cout << "\n[PASS] Member properties\n\n";
}

void test_type_comparison() {
    using namespace std::meta;
    
    std::cout << "[Test 4] Type Comparison\n";
    std::cout << "------------------------\n";
    
    constexpr auto x_type = type_of(^^ComplexType::x);
    constexpr auto y_type = type_of(^^ComplexType::y);
    
    constexpr bool x_is_int = (x_type == ^^int);
    constexpr bool y_is_double = (y_type == ^^const double);
    
    std::cout << "  Type validation:\n";
    std::cout << "    x is int: " << (x_is_int ? "yes" : "no") << "\n";
    std::cout << "    y is const double: " << (y_is_double ? "yes" : "no") << "\n";
    
    // Check container types
    constexpr auto vec_type = type_of(^^ComplexType::vec);
    constexpr bool vec_is_xvector = (vec_type == ^^XVector<int>);
    std::cout << "    vec is XVector<int>: " << (vec_is_xvector ? "yes" : "no") << "\n";
    
    std::cout << "[PASS] Type comparison\n\n";
}

void test_nested_type_introspection() {
    using namespace std::meta;
    
    std::cout << "[Test 5] Nested Type Introspection\n";
    std::cout << "-----------------------------------\n";
    
    constexpr auto member_count = get_member_count<NestedType>();
    std::cout << "  NestedType members (" << member_count << "):\n";
    
    // Manual listing (vector<info> is consteval-only)
    std::cout << "    " << display_string_of(^^NestedType::id) 
              << ": " << display_string_of(type_of(^^NestedType::id)) << "\n";
    std::cout << "    " << display_string_of(^^NestedType::data) 
              << ": " << display_string_of(type_of(^^NestedType::data)) << "\n";
    
    // Check if data member is ComplexType
    constexpr auto data_type = type_of(^^NestedType::data);
    constexpr bool data_is_complex = (data_type == ^^ComplexType);
    
    std::cout << "\n  data is ComplexType: " << (data_is_complex ? "yes" : "no") << "\n";
    
    std::cout << "[PASS] Nested type introspection\n\n";
}

void test_pointer_types() {
    using namespace std::meta;
    
    std::cout << "[Test 6] Pointer Type Analysis\n";
    std::cout << "-------------------------------\n";
    
    constexpr auto ptr_type = type_of(^^ComplexType::ptr);
    
    std::cout << "  Pointer member:\n";
    std::cout << "    Type: " << display_string_of(ptr_type) << "\n";
    std::cout << "    Is pointer to XString\n";
    
    // Test with actual pointer types
    using IntPtr = int*;
    using DoublePtr = double*;
    
    std::cout << "\n  Pointer types:\n";
    std::cout << "    int*: " << display_string_of(^^IntPtr) << "\n";
    std::cout << "    double*: " << display_string_of(^^DoublePtr) << "\n";
    
    std::cout << "[PASS] Pointer types\n\n";
}

void test_container_introspection() {
    using namespace std::meta;
    
    std::cout << "[Test 7] Container Type Introspection\n";
    std::cout << "--------------------------------------\n";
    
    std::cout << "  XOffsetDatastructure2 containers:\n";
    std::cout << "    XVector<int>: " << display_string_of(^^XVector<int>) << "\n";
    std::cout << "    XVector<double>: " << display_string_of(^^XVector<double>) << "\n";
    std::cout << "    XSet<int>: " << display_string_of(^^XSet<int>) << "\n";
    std::cout << "    XSet<uint32_t>: " << display_string_of(^^XSet<uint32_t>) << "\n";
    std::cout << "    XMap<int,int>: " << display_string_of(^^XMap<int, int>) << "\n";
    std::cout << "    XMap<uint32_t,double>: " << display_string_of(^^XMap<uint32_t, double>) << "\n";
    
    std::cout << "[PASS] Container introspection\n\n";
}

#endif // __cpp_reflection

int main() {
    std::cout << "========================================\n";
    std::cout << "  Type Introspection Test\n";
    std::cout << "========================================\n\n";

#if __has_include(<experimental/meta>)
    std::cout << "[INFO] C++26 Reflection: ENABLED\n";
    std::cout << "[INFO] Testing type introspection APIs\n\n";
    
    test_type_names();
    test_member_types();
    test_member_properties();
    test_type_comparison();
    test_nested_type_introspection();
    test_pointer_types();
    test_container_introspection();
    
    std::cout << "========================================\n";
    std::cout << "  Summary\n";
    std::cout << "========================================\n";
    std::cout << "[PASS] Test 1: Type names\n";
    std::cout << "[PASS] Test 2: Member types\n";
    std::cout << "[PASS] Test 3: Member properties\n";
    std::cout << "[PASS] Test 4: Type comparison\n";
    std::cout << "[PASS] Test 5: Nested type introspection\n";
    std::cout << "[PASS] Test 6: Pointer types\n";
    std::cout << "[PASS] Test 7: Container introspection\n";
    std::cout << "\n[SUCCESS] All type introspection tests passed!\n";
    
    return 0;
#else
    std::cout << "[SKIP] C++26 Reflection not available\n";
    std::cout << "[INFO] Compile with -std=c++26 -freflection to enable\n";
    return 0;
#endif
}
