// Must not compile: a client uplink receiver never admits unchecked data.
#include "../../xoffset_receiver.hpp"
using namespace XOffsetDatastructure;

struct ClientRoot { std::uint32_t x{}; };
XOFFSET_RECORD_DERIVED(ClientRoot, (), x)

int main() {
    XReceiver<XLink::from_client> rx(XBuffer::maximum_capacity(4096));
    auto dst = XBuffer::create<ClientRoot>(4096, XBuffer::maximum_capacity(4096));
    std::string wire;
    return rx.reload<ClientRoot, XCheck::admit>(dst, wire) ? 0 : 1;
}
