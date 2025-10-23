#include <iostream>

struct Point {
    int x;
    int y;
    double z;
};

int main() {
    // 测试基本的反射 splice 操作
    constexpr auto point_refl = ^^Point;
    
    std::cout << "Reflection test:\n";
    std::cout << "  Type reflected: Point\n";
    
    // 测试成员访问
    Point p{10, 20, 3.14};
    
    // 使用 splice 获取成员指针
    int Point::*x_ptr = &[:^^Point::x:];  // 成员指针
    
    std::cout << "  Member x exists\n";
    std::cout << "  Value via pointer: " << p.*x_ptr << "\n";
    std::cout << "  Value direct: " << p.x << "\n";
    
#ifdef __cpp_reflection
    std::cout << "\n✅ __cpp_reflection = " << __cpp_reflection << "\n";
#else
    std::cout << "\n❌ __cpp_reflection is NOT defined\n";
#endif
    
    std::cout << "\n🎉 P2996 Reflection is working!\n";
    
    return 0;
}