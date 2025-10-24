#include <iostream>
#include <experimental/meta>

struct Point {
    int x;
    int y;
    double z;
};

struct Vector3D {
    float x, y, z;
};

struct Color {
    unsigned char r, g, b, a;
};

int main() {
    std::cout << "=== Meta Programming Test ===\n\n";
    
    // Test 1: Multiple type reflections
    std::cout << "1. Type Reflection:\n";
    constexpr auto point_type = ^^Point;
    constexpr auto vector_type = ^^Vector3D;
    constexpr auto color_type = ^^Color;
    std::cout << "   ✅ Point reflected\n";
    std::cout << "   ✅ Vector3D reflected\n";
    std::cout << "   ✅ Color reflected\n";
    
    // Test 2: Member access for Point
    std::cout << "\n2. Point Member Access:\n";
    Point p{10, 20, 3.14};
    int Point::*x_ptr = &[:^^Point::x:];
    int Point::*y_ptr = &[:^^Point::y:];
    double Point::*z_ptr = &[:^^Point::z:];
    std::cout << "   x=" << p.*x_ptr << ", y=" << p.*y_ptr << ", z=" << p.*z_ptr << "\n";
    
    // Test 3: Member access for Vector3D
    std::cout << "\n3. Vector3D Member Access:\n";
    Vector3D v{1.0f, 2.0f, 3.0f};
    float Vector3D::*vx_ptr = &[:^^Vector3D::x:];
    float Vector3D::*vy_ptr = &[:^^Vector3D::y:];
    float Vector3D::*vz_ptr = &[:^^Vector3D::z:];
    std::cout << "   x=" << v.*vx_ptr << ", y=" << v.*vy_ptr << ", z=" << v.*vz_ptr << "\n";
    
    // Test 4: Member access for Color
    std::cout << "\n4. Color Member Access:\n";
    Color c{255, 128, 64, 255};
    unsigned char Color::*r_ptr = &[:^^Color::r:];
    unsigned char Color::*g_ptr = &[:^^Color::g:];
    unsigned char Color::*b_ptr = &[:^^Color::b:];
    unsigned char Color::*a_ptr = &[:^^Color::a:];
    std::cout << "   r=" << (int)(c.*r_ptr) << ", g=" << (int)(c.*g_ptr) 
              << ", b=" << (int)(c.*b_ptr) << ", a=" << (int)(c.*a_ptr) << "\n";
    
    std::cout << "\n=== All Meta Tests Passed! ===\n";
    std::cout << "✅ Multiple types reflected\n";
    std::cout << "✅ Different member types (int, double, float, unsigned char)\n";
    std::cout << "✅ All splice operations work\n";
    std::cout << "\n🎉 P2996 Meta Programming functional!\n";
    
    return 0;
}
