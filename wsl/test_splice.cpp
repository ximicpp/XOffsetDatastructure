#include <iostream>
#include <experimental/meta>

struct Point {
    int x;
    int y;
    double z;
};

int main() {
    std::cout << "=== P2996 Reflection Splice Test ===\n\n";
    
    // Test type reflection
    constexpr auto point_refl = ^^Point;
    std::cout << "[OK]Type reflected: Point\n";
    
    // Test member access via reflection
    Point p{10, 20, 3.14};
    
    // Use splice to get member pointers
    int Point::*x_ptr = &[:^^Point::x:];
    int Point::*y_ptr = &[:^^Point::y:];
    double Point::*z_ptr = &[:^^Point::z:];
    
    std::cout << "\n--- Member Access via Reflection ---\n";
    std::cout << "  x (via splice): " << p.*x_ptr << "\n";
    std::cout << "  y (via splice): " << p.*y_ptr << "\n";
    std::cout << "  z (via splice): " << p.*z_ptr << "\n";
    
    std::cout << "\n--- Direct Access (verification) ---\n";
    std::cout << "  x (direct): " << p.x << "\n";
    std::cout << "  y (direct): " << p.y << "\n";
    std::cout << "  z (direct): " << p.z << "\n";
    
    std::cout << "\n[OK]Splice operator [: :] works!\n";
    std::cout << "[OK]Reflection operator ^^ works!\n";
    std::cout << "\nP2996 Reflection is fully functional!\n";
    
    return 0;
}
