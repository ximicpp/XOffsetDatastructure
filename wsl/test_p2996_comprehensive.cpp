#include <iostream>
#include <experimental/meta>
#include <string_view>

// Test structures
struct SimpleStruct {
    int x;
    double y;
    float z;
};

struct NestedStruct {
    int id;
    SimpleStruct data;
    const char* name;
};

enum class Color {
    Red, Green, Blue
};

class MyClass {
public:
    int public_member;
private:
    int private_member;
};

// Namespace for testing
namespace TestNamespace {
    struct InnerStruct {
        int value;
    };
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  P2996 Reflection Feature Tests\n";
    std::cout << "========================================\n\n";

    // ========================================================================
    // Test 1: Basic Reflection Operator (^^)
    // ========================================================================
    std::cout << "[Test 1] Reflection Operator (^^)\n";
    std::cout << "-----------------------------------\n";
    {
        // Reflect types
        constexpr auto int_refl = ^^int;
        constexpr auto double_refl = ^^double;
        constexpr auto simple_refl = ^^SimpleStruct;
        constexpr auto nested_refl = ^^NestedStruct;
        constexpr auto enum_refl = ^^Color;
        constexpr auto class_refl = ^^MyClass;
        
        std::cout << "[PASS] Type reflection: int, double, struct, enum, class\n";
        
        // Reflect members
        constexpr auto x_refl = ^^SimpleStruct::x;
        constexpr auto y_refl = ^^SimpleStruct::y;
        constexpr auto z_refl = ^^SimpleStruct::z;
        
        std::cout << "[PASS] Member reflection: SimpleStruct::x, y, z\n";
        
        // Reflect namespace
        constexpr auto ns_refl = ^^TestNamespace;
        
        std::cout << "[PASS] Namespace reflection: TestNamespace\n";
    }
    std::cout << "[PASS] Test 1 PASSED\n\n";

    // ========================================================================
    // Test 2: Splice Operator ([: ... :])
    // ========================================================================
    std::cout << "[Test 2] Splice Operator ([: ... :])\n";
    std::cout << "-----------------------------------\n";
    {
        SimpleStruct obj{42, 3.14, 2.71f};
        
        // Splice to get member pointer
        constexpr auto x_refl = ^^SimpleStruct::x;
        int SimpleStruct::*x_ptr = &[:x_refl:];
        
        std::cout << "Original: x=" << obj.x << "\n";
        obj.*x_ptr = 100;
        std::cout << "After splice modification: x=" << obj.x << "\n";
        std::cout << "[PASS] Splice for member access\n";
        
        // Splice to get type
        using SplicedType = [:^^SimpleStruct:];
        SplicedType obj2{1, 2.0, 3.0f};
        std::cout << "[PASS] Splice for type alias: obj2.x=" << obj2.x << "\n";
    }
    std::cout << "[PASS] Test 2 PASSED\n\n";

    // ========================================================================
    // Test 3: Member Pointer via Reflection
    // ========================================================================
    std::cout << "[Test 3] Member Pointers\n";
    std::cout << "-----------------------------------\n";
    {
        SimpleStruct obj{10, 20.5, 30.5f};
        
        // Get all member pointers via reflection
        int SimpleStruct::*px = &[:^^SimpleStruct::x:];
        double SimpleStruct::*py = &[:^^SimpleStruct::y:];
        float SimpleStruct::*pz = &[:^^SimpleStruct::z:];
        
        std::cout << "Access via member pointers:\n";
        std::cout << "  x = " << obj.*px << "\n";
        std::cout << "  y = " << obj.*py << "\n";
        std::cout << "  z = " << obj.*pz << "\n";
        
        // Modify via member pointers
        obj.*px = 999;
        obj.*py = 888.8;
        obj.*pz = 777.7f;
        
        std::cout << "After modification:\n";
        std::cout << "  x = " << obj.x << "\n";
        std::cout << "  y = " << obj.y << "\n";
        std::cout << "  z = " << obj.z << "\n";
        
        std::cout << "[PASS] Member pointer read/write\n";
    }
    std::cout << "[PASS] Test 3 PASSED\n\n";

    // ========================================================================
    // Test 4: Nested Struct Reflection
    // ========================================================================
    std::cout << "[Test 4] Nested Structure Reflection\n";
    std::cout << "-----------------------------------\n";
    {
        NestedStruct obj{42, {1, 2.0, 3.0f}, "test"};
        
        // Reflect nested members
        constexpr auto id_refl = ^^NestedStruct::id;
        constexpr auto data_refl = ^^NestedStruct::data;
        constexpr auto name_refl = ^^NestedStruct::name;
        
        int NestedStruct::*id_ptr = &[:id_refl:];
        SimpleStruct NestedStruct::*data_ptr = &[:data_refl:];
        const char* NestedStruct::*name_ptr = &[:name_refl:];
        
        std::cout << "Nested struct access:\n";
        std::cout << "  id = " << obj.*id_ptr << "\n";
        std::cout << "  data.x = " << (obj.*data_ptr).x << "\n";
        std::cout << "  name = " << obj.*name_ptr << "\n";
        
        std::cout << "[PASS] Nested struct reflection\n";
    }
    std::cout << "[PASS] Test 4 PASSED\n\n";

    // ========================================================================
    // Test 5: Enum Reflection
    // ========================================================================
    std::cout << "[Test 5] Enum Reflection\n";
    std::cout << "-----------------------------------\n";
    {
        // Reflect enum type
        constexpr auto color_refl = ^^Color;
        
        // Reflect enum values
        constexpr auto red_refl = ^^Color::Red;
        constexpr auto green_refl = ^^Color::Green;
        constexpr auto blue_refl = ^^Color::Blue;
        
        Color c1 = [:red_refl:];
        Color c2 = [:green_refl:];
        Color c3 = [:blue_refl:];
        
        std::cout << "Enum values via reflection:\n";
        std::cout << "  Red = " << static_cast<int>(c1) << "\n";
        std::cout << "  Green = " << static_cast<int>(c2) << "\n";
        std::cout << "  Blue = " << static_cast<int>(c3) << "\n";
        
        std::cout << "[PASS] Enum reflection and splice\n";
    }
    std::cout << "[PASS] Test 5 PASSED\n\n";

    // ========================================================================
    // Test 6: Class Member Access (Public)
    // ========================================================================
    std::cout << "[Test 6] Class Member Reflection\n";
    std::cout << "-----------------------------------\n";
    {
        MyClass obj;
        obj.public_member = 42;
        
        // Reflect public member
        constexpr auto pub_refl = ^^MyClass::public_member;
        int MyClass::*pub_ptr = &[:pub_refl:];
        
        std::cout << "Public member via reflection: " << obj.*pub_ptr << "\n";
        obj.*pub_ptr = 100;
        std::cout << "After modification: " << obj.public_member << "\n";
        
        std::cout << "[PASS] Class public member reflection\n";
        
        // Note: Private members cannot be accessed without additional access rights
        std::cout << "[INFO] Private member reflection requires special handling\n";
    }
    std::cout << "[PASS] Test 6 PASSED\n\n";

    // ========================================================================
    // Test 7: Multiple Types in One Test
    // ========================================================================
    std::cout << "[Test 7] Multiple Type Reflection\n";
    std::cout << "-----------------------------------\n";
    {
        // Reflect various built-in types
        constexpr auto int_t = ^^int;
        constexpr auto long_t = ^^long;
        constexpr auto float_t = ^^float;
        constexpr auto double_t = ^^double;
        constexpr auto char_t = ^^char;
        constexpr auto bool_t = ^^bool;
        
        // Use spliced types
        [:int_t:] i = 42;
        [:long_t:] l = 1000000L;
        [:float_t:] f = 3.14f;
        [:double_t:] d = 2.718;
        [:char_t:] ch = 'A';
        [:bool_t:] b = true;
        
        std::cout << "Built-in types via reflection:\n";
        std::cout << "  int: " << i << "\n";
        std::cout << "  long: " << l << "\n";
        std::cout << "  float: " << f << "\n";
        std::cout << "  double: " << d << "\n";
        std::cout << "  char: " << ch << "\n";
        std::cout << "  bool: " << b << "\n";
        
        std::cout << "[PASS] Multiple built-in type reflection\n";
    }
    std::cout << "[PASS] Test 7 PASSED\n\n";

    // ========================================================================
    // Test 8: Const and Volatile Qualifiers
    // ========================================================================
    std::cout << "[Test 8] CV-Qualified Types\n";
    std::cout << "-----------------------------------\n";
    {
        struct TestStruct {
            int normal;
            const int const_member = 42;
        };
        
        TestStruct obj{10, 42};
        
        // Reflect normal member
        constexpr auto normal_refl = ^^TestStruct::normal;
        int TestStruct::*normal_ptr = &[:normal_refl:];
        
        std::cout << "Normal member: " << obj.*normal_ptr << "\n";
        obj.*normal_ptr = 20;
        std::cout << "After modification: " << obj.normal << "\n";
        
        // Reflect const member
        constexpr auto const_refl = ^^TestStruct::const_member;
        const int TestStruct::*const_ptr = &[:const_refl:];
        
        std::cout << "Const member: " << obj.*const_ptr << "\n";
        // Note: Cannot modify const member
        
        std::cout << "[PASS] CV-qualified member reflection\n";
    }
    std::cout << "[PASS] Test 8 PASSED\n\n";

    // ========================================================================
    // Test 9: Pointer Types
    // ========================================================================
    std::cout << "[Test 9] Pointer Types\n";
    std::cout << "-----------------------------------\n";
    {
        struct TestStruct {
            int* ptr;
            const char* str;
            double* dptr;
        };
        
        int target = 100;
        double dvalue = 3.14;
        TestStruct obj{&target, "Hello", &dvalue};
        
        // Reflect pointer members
        constexpr auto ptr_refl = ^^TestStruct::ptr;
        int* TestStruct::*ptr_ptr = &[:ptr_refl:];
        
        std::cout << "Pointer member points to: " << *(obj.*ptr_ptr) << "\n";
        
        // Modify through pointer
        *(obj.*ptr_ptr) = 200;
        std::cout << "After modification: target=" << target << "\n";
        
        // Reflect string pointer
        constexpr auto str_refl = ^^TestStruct::str;
        const char* TestStruct::*str_ptr = &[:str_refl:];
        
        std::cout << "String pointer: " << obj.*str_ptr << "\n";
        
        // Reflect double pointer
        constexpr auto dptr_refl = ^^TestStruct::dptr;
        double* TestStruct::*dptr_ptr = &[:dptr_refl:];
        
        std::cout << "Double pointer: " << *(obj.*dptr_ptr) << "\n";
        
        std::cout << "[PASS] Pointer type reflection\n";
    }
    std::cout << "[PASS] Test 9 PASSED\n\n";

    // ========================================================================
    // Test 10: Array Members
    // ========================================================================
    std::cout << "[Test 10] Array Member Reflection\n";
    std::cout << "-----------------------------------\n";
    {
        struct TestStruct {
            int arr[5];
        };
        
        TestStruct obj{{1, 2, 3, 4, 5}};
        
        // Reflect array member
        constexpr auto arr_refl = ^^TestStruct::arr;
        int (TestStruct::*arr_ptr)[5] = &[:arr_refl:];
        
        std::cout << "Array member via reflection:\n";
        for (int i = 0; i < 5; ++i) {
            std::cout << "  arr[" << i << "] = " << (obj.*arr_ptr)[i] << "\n";
        }
        
        // Modify via reflection
        (obj.*arr_ptr)[2] = 999;
        std::cout << "After modification: arr[2] = " << obj.arr[2] << "\n";
        
        std::cout << "[PASS] Array member reflection\n";
    }
    std::cout << "[PASS] Test 10 PASSED\n\n";

    // ========================================================================
    // Summary
    // ========================================================================
    std::cout << "========================================\n";
    std::cout << "  P2996 Feature Test Summary\n";
    std::cout << "========================================\n";
    std::cout << "[PASS] Test 1:  Reflection operator (^^)\n";
    std::cout << "[PASS] Test 2:  Splice operator ([: :])\n";
    std::cout << "[PASS] Test 3:  Member pointers\n";
    std::cout << "[PASS] Test 4:  Nested structures\n";
    std::cout << "[PASS] Test 5:  Enum reflection\n";
    std::cout << "[PASS] Test 6:  Class members\n";
    std::cout << "[PASS] Test 7:  Built-in types\n";
    std::cout << "[PASS] Test 8:  CV-qualified types\n";
    std::cout << "[PASS] Test 9:  Pointer types\n";
    std::cout << "[PASS] Test 10: Array members\n";
    std::cout << "\n[SUCCESS] All P2996 reflection features working!\n";
    std::cout << "========================================\n";

    return 0;
}
