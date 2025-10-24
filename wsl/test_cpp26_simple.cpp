#include <iostream>
#include <experimental/meta>

struct Point {
    int x;
    int y;
};

int main() {
    std::cout << "=== C++26 Reflection Environment Test ===\n\n";
    
    // Check C++ standard
    std::cout << "C++ Standard: " << __cplusplus << "\n";
    if (__cplusplus >= 202400L) {
        std::cout << "  [OK]C++26 or later\n";
    }
    
    std::cout << "\n--- Testing Reflection Features ---\n\n";
    
    // Test 1: Type reflection
    std::cout << "1. Type reflection (^^):\n";
    constexpr auto point_type = ^^Point;
    std::cout << "   [OK]Type reflection works\n";
    
    // Test 2: Member reflection
    std::cout << "\n2. Member access via reflection:\n";
    Point p{100, 200};
    int Point::*x_ptr = &[:^^Point::x:];
    int Point::*y_ptr = &[:^^Point::y:];
    std::cout << "   Point.x = " << p.*x_ptr << "\n";
    std::cout << "   Point.y = " << p.*y_ptr << "\n";
    std::cout << "   [OK]Member reflection works\n";
    
    std::cout << "\n=== All reflection features working! ===\n";
    
    return 0;
}
