struct S { int x; };

int main() {
    constexpr auto m = ^S::x;
    return 0;
}
