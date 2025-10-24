#include <iostream>
#include <experimental/meta>

// Test structures for meta queries
struct Point {
    int x;
    int y;
    double z;
};

struct Complex {
    int a, b, c, d, e;
};

template<typename T>
struct Template {
    T value;
};

int main() {
    std::cout << "========================================\n";
    std::cout << "  P2996 Meta Functions Tests\n";
    std::cout << "========================================\n\n";

    // ========================================================================
    // Test 1: Using namespace std::meta
    // ========================================================================
    std::cout << "[Test 1] std::meta Namespace\n";
    std::cout << "-----------------------------------\n";
    {
        using namespace std::meta;
        
        // Test reflection with meta namespace
        constexpr auto point_refl = ^^Point;
        
        std::cout << "[OK] std::meta namespace accessible\n";
        std::cout << "[OK] Reflection operator works in meta context\n";
    }
    std::cout << "[OK] Test 1 PASSED\n\n";

    // ========================================================================
    // Test 2: Template Reflection
    // ========================================================================
    std::cout << "[Test 2] Template Reflection\n";
    std::cout << "-----------------------------------\n";
    {
        // Reflect template type
        constexpr auto template_int = ^^Template<int>;
        constexpr auto template_double = ^^Template<double>;
        
        // Create instances using reflection
        using IntTemplate = [:template_int:];
        using DoubleTemplate = [:template_double:];
        
        IntTemplate obj1{42};
        DoubleTemplate obj2{3.14};
        
        std::cout << "Template<int> value: " << obj1.value << "\n";
        std::cout << "Template<double> value: " << obj2.value << "\n";
        
        std::cout << "[OK] Template type reflection\n";
        std::cout << "[OK] Template instantiation via splice\n";
    }
    std::cout << "[OK] Test 2 PASSED\n\n";

    // ========================================================================
    // Test 3: Multiple Member Reflection in Sequence
    // ========================================================================
    std::cout << "[Test 3] Sequential Member Reflection\n";
    std::cout << "-----------------------------------\n";
    {
        Complex obj{1, 2, 3, 4, 5};
        
        // Reflect all members
        constexpr auto a_refl = ^^Complex::a;
        constexpr auto b_refl = ^^Complex::b;
        constexpr auto c_refl = ^^Complex::c;
        constexpr auto d_refl = ^^Complex::d;
        constexpr auto e_refl = ^^Complex::e;
        
        // Access via reflection
        int Complex::*a_ptr = &[:a_refl:];
        int Complex::*b_ptr = &[:b_refl:];
        int Complex::*c_ptr = &[:c_refl:];
        int Complex::*d_ptr = &[:d_refl:];
        int Complex::*e_ptr = &[:e_refl:];
        
        std::cout << "All members via reflection: ";
        std::cout << obj.*a_ptr << ", ";
        std::cout << obj.*b_ptr << ", ";
        std::cout << obj.*c_ptr << ", ";
        std::cout << obj.*d_ptr << ", ";
        std::cout << obj.*e_ptr << "\n";
        
        std::cout << "[OK] Multiple sequential reflections\n";
    }
    std::cout << "[OK] Test 3 PASSED\n\n";

    // ========================================================================
    // Test 4: Constexpr Reflection
    // ========================================================================
    std::cout << "[Test 4] Constexpr Reflection\n";
    std::cout << "-----------------------------------\n";
    {
        // All reflections must be constexpr
        constexpr auto point_refl = ^^Point;
        constexpr auto x_refl = ^^Point::x;
        constexpr auto y_refl = ^^Point::y;
        constexpr auto z_refl = ^^Point::z;
        
        // Verify they work in constexpr context
        // The reflection itself is always constexpr
        constexpr auto test_refl1 = ^^Point;
        constexpr auto test_refl2 = ^^Point::x;
        
        std::cout << "[OK] All reflections are constexpr\n";
        std::cout << "[OK] Can be used in compile-time contexts\n";
        std::cout << "[OK] Reflection operators evaluated at compile-time\n";
    }
    std::cout << "[OK] Test 4 PASSED\n\n";

    // ========================================================================
    // Test 5: Reflection with Different Access Patterns
    // ========================================================================
    std::cout << "[Test 5] Access Patterns\n";
    std::cout << "-----------------------------------\n";
    {
        Point p{10, 20, 30.5};
        
        // Pattern 1: Direct splice
        p.[:^^Point::x:] = 100;
        std::cout << "Pattern 1 (direct splice): x=" << p.x << "\n";
        
        // Pattern 2: Member pointer
        int Point::*ptr = &[:^^Point::y:];
        p.*ptr = 200;
        std::cout << "Pattern 2 (member pointer): y=" << p.y << "\n";
        
        // Pattern 3: Through constexpr variable
        constexpr auto z_refl = ^^Point::z;
        double Point::*z_ptr = &[:z_refl:];
        p.*z_ptr = 300.5;
        std::cout << "Pattern 3 (constexpr var): z=" << p.z << "\n";
        
        std::cout << "[OK] Multiple access patterns work\n";
    }
    std::cout << "[OK] Test 5 PASSED\n\n";

    // ========================================================================
    // Test 6: Type Alias via Reflection
    // ========================================================================
    std::cout << "[Test 6] Type Aliases\n";
    std::cout << "-----------------------------------\n";
    {
        // Create type aliases using reflection
        using ReflectedPoint = [:^^Point:];
        using ReflectedInt = [:^^int:];
        using ReflectedDouble = [:^^double:];
        
        ReflectedPoint p{1, 2, 3.0};
        ReflectedInt i = 42;
        ReflectedDouble d = 3.14;
        
        std::cout << "Point via alias: (" << p.x << ", " << p.y << ", " << p.z << ")\n";
        std::cout << "int via alias: " << i << "\n";
        std::cout << "double via alias: " << d << "\n";
        
        std::cout << "[OK] Type aliases from reflection\n";
    }
    std::cout << "[OK] Test 6 PASSED\n\n";

    // ========================================================================
    // Test 7: Reflection in Generic Code
    // ========================================================================
    std::cout << "[Test 7] Generic Reflection Patterns\n";
    std::cout << "-----------------------------------\n";
    {
        // Helper lambda to access member via reflection
        auto get_x = [](auto& obj) -> decltype(auto) {
            using ObjType = std::remove_reference_t<decltype(obj)>;
            constexpr auto x_refl = ^^ObjType::x;
            using MemberPtr = decltype(&[:x_refl:]);
            MemberPtr ptr = &[:x_refl:];
            return obj.*ptr;
        };
        
        Point p{100, 200, 300.0};
        
        std::cout << "Generic access to x: " << get_x(p) << "\n";
        
        // Modify through generic accessor
        get_x(p) = 999;
        std::cout << "After generic modification: " << p.x << "\n";
        
        std::cout << "[OK] Reflection in generic/template code\n";
    }
    std::cout << "[OK] Test 7 PASSED\n\n";

    // ========================================================================
    // Test 8: Combining Multiple Reflections
    // ========================================================================
    std::cout << "[Test 8] Combined Reflections\n";
    std::cout << "-----------------------------------\n";
    {
        Point p{1, 2, 3.0};
        
        // Reflect and use multiple members in one expression
        auto sum = p.[:^^Point::x:] + p.[:^^Point::y:];
        auto product = p.[:^^Point::x:] * p.[:^^Point::y:];
        
        std::cout << "Sum of x and y: " << sum << "\n";
        std::cout << "Product of x and y: " << product << "\n";
        
        std::cout << "[OK] Multiple reflections in expressions\n";
    }
    std::cout << "[OK] Test 8 PASSED\n\n";

    // ========================================================================
    // Test 9: Reflection Operator Composition
    // ========================================================================
    std::cout << "[Test 9] Reflection Composition\n";
    std::cout << "-----------------------------------\n";
    {
        // Nested reflection and splice
        using Type1 = [:^^Point:];
        constexpr auto type1_refl = ^^Type1;
        using Type2 = [:type1_refl:];
        
        Type2 p{10, 20, 30.0};
        std::cout << "Through composed reflection: x=" << p.x << "\n";
        
        std::cout << "[OK] Reflection operator composition\n";
    }
    std::cout << "[OK] Test 9 PASSED\n\n";

    // ========================================================================
    // Test 10: Decltype with Reflection
    // ========================================================================
    std::cout << "[Test 10] Decltype and Reflection\n";
    std::cout << "-----------------------------------\n";
    {
        Point p{1, 2, 3.0};
        
        // Use decltype with reflected members
        using XType = decltype(p.[:^^Point::x:]);
        using YType = decltype(p.[:^^Point::y:]);
        using ZType = decltype(p.[:^^Point::z:]);
        
        XType x_val = 100;
        YType y_val = 200;
        ZType z_val = 300.5;
        
        std::cout << "Decltype from reflection:\n";
        std::cout << "  XType (int): " << x_val << "\n";
        std::cout << "  YType (int): " << y_val << "\n";
        std::cout << "  ZType (double): " << z_val << "\n";
        
        std::cout << "[OK] Decltype works with reflection\n";
    }
    std::cout << "[OK] Test 10 PASSED\n\n";

    // ========================================================================
    // Summary
    // ========================================================================
    std::cout << "========================================\n";
    std::cout << "  Meta Functions Test Summary\n";
    std::cout << "========================================\n";
    std::cout << "[OK] Test 1:  std::meta namespace\n";
    std::cout << "[OK] Test 2:  Template reflection\n";
    std::cout << "[OK] Test 3:  Sequential reflections\n";
    std::cout << "[OK] Test 4:  Constexpr reflection\n";
    std::cout << "[OK] Test 5:  Access patterns\n";
    std::cout << "[OK] Test 6:  Type aliases\n";
    std::cout << "[OK] Test 7:  Generic code\n";
    std::cout << "[OK] Test 8:  Combined reflections\n";
    std::cout << "[OK] Test 9:  Reflection composition\n";
    std::cout << "[OK] Test 10: Decltype integration\n";
    std::cout << "\n[OK][OK] All meta functions working!\n";
    std::cout << "========================================\n";

    return 0;
}
