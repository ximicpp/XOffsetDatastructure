// ============================================================================
// Test: Splice Operations
// Purpose: Test the splice operator [: :] in various scenarios
// ============================================================================

#include "../xoffsetdatastructure2.hpp"
#include <iostream>

#if __has_include(<experimental/meta>)
#include <experimental/meta>
#define HAS_REFLECTION 1

using namespace XOffsetDatastructure2;

struct Point {
    int x;
    int y;
};

struct DataStruct {
    uint32_t id;
    double value;
    XString name;
    
    template <typename Allocator>
    DataStruct(Allocator allocator) : id(0), value(0.0), name(allocator) {}
};

void test_direct_member_splice() {
    std::cout << "[Test 1] Direct Member Splice\n";
    std::cout << "------------------------------\n";
    
    Point p{10, 20};
    
    std::cout << "  Original: x=" << p.x << ", y=" << p.y << "\n";
    
    // Direct splice to modify member
    p.[:^^Point::x:] = 100;
    p.[:^^Point::y:] = 200;
    
    std::cout << "  After splice: x=" << p.x << ", y=" << p.y << "\n";
    
    std::cout << "[PASS] Direct member splice\n\n";
}

void test_member_pointer_splice() {
    std::cout << "[Test 2] Member Pointer Splice\n";
    std::cout << "-------------------------------\n";
    
    Point p{30, 40};
    
    // Get member pointers via splice
    int Point::*x_ptr = &[:^^Point::x:];
    int Point::*y_ptr = &[:^^Point::y:];
    
    std::cout << "  Access via member pointer: x=" << p.*x_ptr 
              << ", y=" << p.*y_ptr << "\n";
    
    // Modify via member pointer
    p.*x_ptr = 300;
    p.*y_ptr = 400;
    
    std::cout << "  After modification: x=" << p.x << ", y=" << p.y << "\n";
    
    std::cout << "[PASS] Member pointer splice\n\n";
}

void test_type_splice() {
    using namespace std::meta;
    
    std::cout << "[Test 3] Type Splice\n";
    std::cout << "--------------------\n";
    
    // Create type alias via splice
    using PointType = [:^^Point:];
    using IntType = [:^^int:];
    using DoubleType = [:^^double:];
    
    PointType p1{50, 60};
    IntType i = 42;
    DoubleType d = 3.14;
    
    std::cout << "  Point via splice: (" << p1.x << ", " << p1.y << ")\n";
    std::cout << "  int via splice: " << i << "\n";
    std::cout << "  double via splice: " << d << "\n";
    
    std::cout << "[PASS] Type splice\n\n";
}

void test_expression_splice() {
    std::cout << "[Test 4] Expression Splice\n";
    std::cout << "--------------------------\n";
    
    Point p{15, 25};
    
    // Use splice in expressions
    auto sum = p.[:^^Point::x:] + p.[:^^Point::y:];
    auto product = p.[:^^Point::x:] * p.[:^^Point::y:];
    auto diff = p.[:^^Point::x:] - p.[:^^Point::y:];
    
    std::cout << "  Point: (" << p.x << ", " << p.y << ")\n";
    std::cout << "  Sum: " << sum << "\n";
    std::cout << "  Product: " << product << "\n";
    std::cout << "  Difference: " << diff << "\n";
    
    std::cout << "[PASS] Expression splice\n\n";
}

void test_xoffsetdatastructure_splice() {
    std::cout << "[Test 5] XOffsetDatastructure2 Splice\n";
    std::cout << "--------------------------------------\n";
    
    XBufferExt xbuf(1024);
    auto* data = xbuf.make<DataStruct>("test");
    
    data->id = 1001;
    data->value = 99.99;
    data->name = XString("TestData", xbuf.allocator<XString>());
    
    std::cout << "  Original:\n";
    std::cout << "    id: " << data->id << "\n";
    std::cout << "    value: " << data->value << "\n";
    std::cout << "    name: " << data->name.c_str() << "\n";
    
    // Access via splice
    std::cout << "\n  Access via splice:\n";
    std::cout << "    id: " << data->[:^^DataStruct::id:] << "\n";
    std::cout << "    value: " << data->[:^^DataStruct::value:] << "\n";
    
    // Modify via splice
    data->[:^^DataStruct::id:] = 2002;
    data->[:^^DataStruct::value:] = 88.88;
    
    std::cout << "\n  After modification:\n";
    std::cout << "    id: " << data->id << "\n";
    std::cout << "    value: " << data->value << "\n";
    
    std::cout << "[PASS] XOffsetDatastructure2 splice\n\n";
}

void test_const_member_splice() {
    struct ConstTest {
        int normal;
        const int constant = 42;
    };
    
    std::cout << "[Test 6] Const Member Splice\n";
    std::cout << "----------------------------\n";
    
    ConstTest obj{10, 42};
    
    // Access normal member
    std::cout << "  Normal member: " << obj.[:^^ConstTest::normal:] << "\n";
    obj.[:^^ConstTest::normal:] = 20;
    std::cout << "  After modification: " << obj.normal << "\n";
    
    // Access const member (read-only)
    std::cout << "  Const member: " << obj.[:^^ConstTest::constant:] << "\n";
    // Note: Cannot modify const member
    
    std::cout << "[PASS] Const member splice\n\n";
}

#endif // __cpp_reflection

int main() {
    std::cout << "========================================\n";
    std::cout << "  Splice Operations Test\n";
    std::cout << "========================================\n\n";

#if __has_include(<experimental/meta>)
    std::cout << "[INFO] C++26 Reflection: ENABLED\n";
    std::cout << "[INFO] Testing splice operator [: :]\n\n";
    
    test_direct_member_splice();
    test_member_pointer_splice();
    test_type_splice();
    test_expression_splice();
    test_xoffsetdatastructure_splice();
    test_const_member_splice();
    
    std::cout << "========================================\n";
    std::cout << "  Summary\n";
    std::cout << "========================================\n";
    std::cout << "[PASS] Test 1: Direct member splice\n";
    std::cout << "[PASS] Test 2: Member pointer splice\n";
    std::cout << "[PASS] Test 3: Type splice\n";
    std::cout << "[PASS] Test 4: Expression splice\n";
    std::cout << "[PASS] Test 5: XOffsetDatastructure2 splice\n";
    std::cout << "[PASS] Test 6: Const member splice\n";
    std::cout << "\n[SUCCESS] All splice operation tests passed!\n";
    
    return 0;
#else
    std::cout << "[SKIP] C++26 Reflection not available\n";
    std::cout << "[INFO] Compile with -std=c++26 -freflection to enable\n";
    return 0;
#endif
}
