#include <iostream>
#include <experimental/meta>

struct S { 
    int x; 
    int y;
};

int main() {
    std::cout << "=== Reflect Syntax Test ===\n\n";
    
    // Test reflecting a type
    constexpr auto type_s = ^^S;
    std::cout << "✅ Type S reflected\n";
    
    // Test reflecting members using splice
    S obj{42, 100};
    int S::*x_ptr = &[:^^S::x:];
    int S::*y_ptr = &[:^^S::y:];
    
    std::cout << "\nMember access:\n";
    std::cout << "  S.x = " << obj.*x_ptr << "\n";
    std::cout << "  S.y = " << obj.*y_ptr << "\n";
    
    std::cout << "\n✅ Reflect syntax test passed!\n";
    
    return 0;
}
