#include <iostream>

int main() {
    std::cout << "C++ Standard: " << __cplusplus << "\n";
#ifdef __cpp_reflection
    std::cout << "Reflection: " << __cpp_reflection << "\n";
#else
    std::cout << "Reflection: NOT DEFINED\n";
#endif
    return 0;
}
