#include <iostream>
#include <experimental/meta>

struct Point { int x; int y; };

int main() {
    using namespace std::meta;
    
    constexpr auto point_refl = ^^Point;
    std::cout << "Point type: " << display_string_of(point_refl) << std::endl;
    
    std::cout << "Reflection is working!" << std::endl;
    
    return 0;
}
