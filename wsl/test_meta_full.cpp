#include <meta>
#include <iostream>

struct Point {
    int x;
    int y;
    double z;
};

int main() {
    using namespace std::meta;
    
    // 获取 Point 的反射信息
    constexpr auto point_refl = ^^Point;
    
    // 获取成员数量（编译时）
    constexpr auto members = nonstatic_data_members_of(point_refl, access_context::unchecked());
    constexpr size_t member_count = members.size();
    
    std::cout << "Point has " << member_count << " members\n";
    
#ifdef __cpp_reflection
    std::cout << "__cpp_reflection = " << __cpp_reflection << "\n";
#else
    std::cout << "__cpp_reflection is NOT defined\n";
#endif
    
    return 0;
}