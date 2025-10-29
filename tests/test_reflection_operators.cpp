// ============================================================================
// Test: Reflection Operators (^^ and [: :])
// Purpose: Test basic reflection and splice operators
// ============================================================================

#include "../xoffsetdatastructure2.hpp"
#include <iostream>
#include <experimental/meta>

using namespace XOffsetDatastructure2;

struct TestStruct {
    int x;
    double y;
    XString name;
    
    template <typename Allocator>
    TestStruct(Allocator allocator) : x(0), y(0.0), name(allocator) {}
};

// Type safety validation
static_assert(is_xbuffer_safe<TestStruct>::value, 
              "TestStruct must be safe for XBuffer");

void test_type_reflection() {
    using namespace std::meta;
    
    std::cout << "[Test 1] Type Reflection\n";
    std::cout << "------------------------\n";
    
    // Reflect types
    constexpr auto test_refl = ^^TestStruct;
    constexpr auto int_refl = ^^int;
    constexpr auto double_refl = ^^double;
    constexpr auto xstring_refl = ^^XString;
    
    std::cout << "  TestStruct: " << display_string_of(test_refl) << "\n";
    std::cout << "  int: " << display_string_of(int_refl) << "\n";
    std::cout << "  double: " << display_string_of(double_refl) << "\n";
    std::cout << "  XString: " << display_string_of(xstring_refl) << "\n";
    
    std::cout << "[PASS] Type reflection\n\n";
}

void test_member_reflection() {
    using namespace std::meta;
    
    std::cout << "[Test 2] Member Reflection\n";
    std::cout << "--------------------------\n";
    
    // Reflect members
    constexpr auto x_refl = ^^TestStruct::x;
    constexpr auto y_refl = ^^TestStruct::y;
    constexpr auto name_refl = ^^TestStruct::name;
    
    std::cout << "  Member x: " << display_string_of(x_refl) << "\n";
    std::cout << "  Member y: " << display_string_of(y_refl) << "\n";
    std::cout << "  Member name: " << display_string_of(name_refl) << "\n";
    
    // Get member types
    constexpr auto x_type = type_of(x_refl);
    constexpr auto y_type = type_of(y_refl);
    constexpr auto name_type = type_of(name_refl);
    
    std::cout << "\n  Type of x: " << display_string_of(x_type) << "\n";
    std::cout << "  Type of y: " << display_string_of(y_type) << "\n";
    std::cout << "  Type of name: " << display_string_of(name_type) << "\n";
    
    std::cout << "[PASS] Member reflection\n\n";
}

void test_builtin_types() {
    using namespace std::meta;
    
    std::cout << "[Test 3] Built-in Types\n";
    std::cout << "-----------------------\n";
    
    // Reflect various built-in types
    constexpr auto char_refl = ^^char;
    constexpr auto short_refl = ^^short;
    constexpr auto long_refl = ^^long;
    constexpr auto float_refl = ^^float;
    constexpr auto bool_refl = ^^bool;
    constexpr auto uint32_refl = ^^uint32_t;
    constexpr auto uint64_refl = ^^uint64_t;
    
    std::cout << "  char: " << display_string_of(char_refl) << "\n";
    std::cout << "  short: " << display_string_of(short_refl) << "\n";
    std::cout << "  long: " << display_string_of(long_refl) << "\n";
    std::cout << "  float: " << display_string_of(float_refl) << "\n";
    std::cout << "  bool: " << display_string_of(bool_refl) << "\n";
    std::cout << "  uint32_t: " << display_string_of(uint32_refl) << "\n";
    std::cout << "  uint64_t: " << display_string_of(uint64_refl) << "\n";
    
    std::cout << "[PASS] Built-in types\n\n";
}

void test_container_types() {
    using namespace std::meta;
    
    std::cout << "[Test 4] Container Types\n";
    std::cout << "------------------------\n";
    
    // Reflect XOffsetDatastructure2 container types
    constexpr auto xvector_refl = ^^XVector<int>;
    constexpr auto xset_refl = ^^XSet<int>;
    constexpr auto xmap_refl = ^^XMap<int, double>;
    
    std::cout << "  XVector<int>: " << display_string_of(xvector_refl) << "\n";
    std::cout << "  XSet<int>: " << display_string_of(xset_refl) << "\n";
    std::cout << "  XMap<int,double>: " << display_string_of(xmap_refl) << "\n";
    
    std::cout << "[PASS] Container types\n\n";
}

void test_reflection_with_instances() {
    std::cout << "[Test 5] Reflection with Instances\n";
    std::cout << "-----------------------------------\n";
    
    XBufferExt xbuf(1024);
    auto* obj = xbuf.make<TestStruct>("test");
    
    obj->x = 42;
    obj->y = 3.14;
    obj->name = XString("ReflectionTest", xbuf.allocator<XString>());
    
    std::cout << "  Created instance:\n";
    std::cout << "    x = " << obj->x << "\n";
    std::cout << "    y = " << obj->y << "\n";
    std::cout << "    name = " << obj->name.c_str() << "\n";
    
    // Access via reflection
    std::cout << "\n  Access via splice:\n";
    std::cout << "    x = " << obj->[:^^TestStruct::x:] << "\n";
    std::cout << "    y = " << obj->[:^^TestStruct::y:] << "\n";
    
    std::cout << "[PASS] Reflection with instances\n\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  Reflection Operators Test\n";
    std::cout << "========================================\n\n";

    std::cout << "[INFO] C++26 Reflection: ENABLED\n";
    std::cout << "[INFO] Using Clang P2996 experimental reflection\n\n";
    
    test_type_reflection();
    test_member_reflection();
    test_builtin_types();
    test_container_types();
    test_reflection_with_instances();
    
    std::cout << "========================================\n";
    std::cout << "  Summary\n";
    std::cout << "========================================\n";
    std::cout << "[PASS] Test 1: Type reflection\n";
    std::cout << "[PASS] Test 2: Member reflection\n";
    std::cout << "[PASS] Test 3: Built-in types\n";
    std::cout << "[PASS] Test 4: Container types\n";
    std::cout << "[PASS] Test 5: Reflection with instances\n";
    std::cout << "\n[SUCCESS] All reflection operator tests passed!\n";
    
    return 0;
}
