// Must not compile: shared deltas are not read from a client's uplink.
#include "../../xoffset_receiver.hpp"
using namespace XOffsetDatastructure;

struct ClientRec { std::uint32_t x{}; };
XOFFSET_RECORD_DERIVED(ClientRec, (), x)

struct ClientTarget {
    using tables = std::tuple<ClientRec>;
    template <unsigned K> static constexpr bool keyed = false;
    ClientRec rec;
    template <unsigned K> std::int64_t find(std::uint32_t id) { return id == 0 ? 0 : -1; }
    template <unsigned K> ClientRec& record(std::int64_t) { return rec; }
};

int main() {
    XReceiver<XLink::from_client> rx(XBuffer::maximum_capacity(4096));
    xsync::XSyncCursor cursor;
    ClientTarget target;
    return rx.apply_sync<XCheck::full>(std::string_view("XS"), cursor, target) ? 0 : 1;
}
