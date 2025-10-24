// Test P2996 reflection???Using ^^ operator?
struct Point {
    int x;
    int y;
};

int main() {
    // Using ^^ reflectionoperator
    using info = decltype(^^int);
    constexpr auto refl = ^^Point;
    return 0;
}