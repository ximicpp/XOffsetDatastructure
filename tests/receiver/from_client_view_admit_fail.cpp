// Must not compile: a client uplink receiver never views unchecked data.
#include "../../xoffset_receiver.hpp"
using namespace XOffsetDatastructure;

struct ClientViewRoot { std::uint32_t x{}; };
XOFFSET_RECORD_DERIVED(ClientViewRoot, (), x)
XOFFSET_REGISTER_SCHEMA_NAME(ClientViewRoot, "test.ClientViewRoot")

int main() {
    XReceiver<XLink::from_client> rx(XBuffer::maximum_capacity(4096));
    std::string wire;
    return rx.view<ClientViewRoot, XCheck::admit>(wire) ? 0 : 1;
}
