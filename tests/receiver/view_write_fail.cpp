// Must not compile: a view gives only const access. GCC and Clang quote the
// line below, which carries the marker the test looks for; MSVC names its
// const errors (C2662, C3490, C3892).
#include "../../xoffset_receiver.hpp"
using namespace XOffsetDatastructure;

struct ViewWriteRoot {
    std::uint32_t x{};
    XVector<std::uint32_t> values;
};
XOFFSET_RECORD_DERIVED(ViewWriteRoot, (), x, values)
XOFFSET_REGISTER_SCHEMA_NAME(ViewWriteRoot, "test.ViewWriteRoot")

int main() {
    XReceiver<XLink::from_client> rx(XBuffer::maximum_capacity(4096));
    std::string wire;
    auto v = rx.view<ViewWriteRoot>(wire);
    if (!v) return 1;
    (*v)->values.push_back(1); (*v)->x = 2; /* VIEW_WRITE_MARKER */
    return 0;
}
