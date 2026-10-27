// Configure-time probe of the C++20/C++23 builds: the full library compiles and
// links with records described by XOFFSET_RECORD, without reflection or TypeLayout.
#include "xoffsetdatastructure.hpp"

#if defined(__cpp_impl_reflection) || defined(BOOST_TYPELAYOUT_HPP_INCLUDED)
#error "the C++20/C++23 build must not use P2996 reflection or TypeLayout"
#endif

struct FieldListProbe {
    int value = 7;
};
XOFFSET_RECORD(FieldListProbe, value)

int main() {
    auto buffer = XOffsetDatastructure::XBuffer::create<FieldListProbe>();
    return buffer.root<FieldListProbe>().value == 7 ? 0 : 1;
}
