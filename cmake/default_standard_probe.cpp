// Configure-time probe (cmake/XOffsetDefaultStandard.cmake): does this
// toolchain have C++26 reflection? Standard headers only, so it answers
// before the dependencies are checked.
#include <meta>

struct DefaultStandardProbe {
    int value = 7;
};

int main() {
    int count = 0;
    template for (constexpr auto member : std::define_static_array(
                      std::meta::nonstatic_data_members_of(
                          ^^DefaultStandardProbe, std::meta::access_context::unchecked()))) {
        count += DefaultStandardProbe{}.[:member:];
    }
    return count == 7 ? 0 : 1;
}
