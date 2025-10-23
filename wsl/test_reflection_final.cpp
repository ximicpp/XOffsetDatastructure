#include <meta>
#include <iostream>

int main() {
#if __cpp_reflection >= 202306L
    std::cout << "✅ Reflection ENABLED: " << __cpp_reflection << "\n";
#else
    std::cout << "❌ Reflection NOT enabled\n";
#endif
    return 0;
}
