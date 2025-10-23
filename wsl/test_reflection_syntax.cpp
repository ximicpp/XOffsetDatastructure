// 测试 P2996 反射语法（使用 ^^ 操作符）
struct Point {
    int x;
    int y;
};

int main() {
    // 使用 ^^ 反射操作符
    using info = decltype(^^int);
    constexpr auto refl = ^^Point;
    return 0;
}