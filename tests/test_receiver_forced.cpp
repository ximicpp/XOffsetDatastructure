// XOFFSET_FORCE_FULL_CHECK=1 (the default without NDEBUG and under ASan or
// TSan): every admit runs the full check, so a sender's bad graph is
// rejected instead of used. No exception of its own: also built as
// test_receiver_forced_noexc.
#define XOFFSET_FORCE_FULL_CHECK 1
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include "../xoffset_receiver.hpp"
using namespace XOffsetDatastructure;

struct ForcedRoot {
    std::uint64_t id{};
    XVector<std::uint32_t> values;
};
XOFFSET_RECORD_DERIVED(ForcedRoot, (), id, values)
XOFFSET_REGISTER_SCHEMA_NAME(ForcedRoot, "test.ReceiverForcedRoot")

static_assert(detail::force_full_check);

int main() {
    auto cap = XBuffer::maximum_capacity(std::size_t(1) << 22).with_growth(XGrowPolicy::Automatic);
    auto src = XBuffer::create<ForcedRoot>(4096, cap);
    auto& r = src.root<ForcedRoot>();
    r.id = 3;
    for (std::uint32_t i = 0; i < 40; ++i) r.values.push_back(i);
    const auto head = std::size_t(reinterpret_cast<const char*>(&r.values) - reinterpret_cast<const char*>(&r));
    const auto wire = src.save_verified<ForcedRoot>(XSaveOptions{XWireEmission::dense});
    XWireHeaderV2 header;
    std::memcpy(&header, wire.data(), sizeof(header));
    std::string bad = wire;
    const std::int32_t far = 0x40000000;
    std::memcpy(bad.data() + sizeof(XWireHeaderV2) + (header.root_offset - XWIRE_PAYLOAD_BEGIN) + head, &far,
                sizeof(far));

    int failures = 0;
    XReceiver<XLink::server_peer> rx(cap);
    auto dst = XBuffer::create<ForcedRoot>(4096, cap);
    if (!rx.reload<ForcedRoot>(dst, wire)) { std::fputs("FAIL: forced admit of a valid wire\n", stderr); ++failures; }
    if (rx.reload<ForcedRoot>(dst, bad)) { std::fputs("FAIL: forced admit accepted a bad graph\n", stderr); ++failures; }
    if (rx.load<ForcedRoot>(bad)) { std::fputs("FAIL: forced admit load accepted a bad graph\n", stderr); ++failures; }
    if (failures) return 1;
    std::puts("test_receiver_forced: ok");
    return 0;
}
