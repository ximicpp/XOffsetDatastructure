#include "xoffsetdatastructure.hpp"
#include <meta>

struct ReflectionProbe {
    int value = 7;
};
// Also described, so the probe builds with XOFFSET_SCHEMA_BACKEND=describe.
XOFFSET_RECORD(ReflectionProbe, value)

static_assert(std::meta::nonstatic_data_members_of(
                  ^^ReflectionProbe, std::meta::access_context::unchecked()).size() == 1);

int main() {
    auto buffer = XOffsetDatastructure::XBuffer::create<ReflectionProbe>();
    int value = 0;
    template for (constexpr auto member : std::define_static_array(
                      std::meta::nonstatic_data_members_of(
                          ^^ReflectionProbe, std::meta::access_context::unchecked()))) {
        value += buffer.root<ReflectionProbe>().[:member:];
    }
    return value == 7 ? 0 : 1;
}
