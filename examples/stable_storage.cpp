#include "../xoffsetdatastructure.hpp"
#include <cassert>
#include <iostream>

using namespace XOffsetDatastructure;

struct StableExample {
    std::int64_t sequence{};
    XString name;
    XVector<std::int32_t> values;
};
XOFFSET_RECORD_DERIVED(StableExample, (), sequence, name, values)

XOFFSET_REGISTER_SCHEMA_NAME(StableExample, "example.StableStorage")

int main() {
    constexpr std::size_t limit = 512 * 1024;
    // Pinned: a VM reservation whose address never changes. Without it this
    // buffer would start on a movable slot, where grow() may move it.
    const auto pinned = XBuffer::maximum_capacity(limit).with_storage(XStorage::Pinned);
    auto buffer = TypedXBuffer<StableExample>::create(4096, pinned);
    // A Pinned buffer never moves, so a plain reference (plain_root()) stays
    // valid across grow(); on other storage use root() (a proxy) or a pin
    // scope (edit / read).
    auto* root = &buffer.plain_root();
    root->sequence = 42;
    root->name = "stable arena";
    root->values.push_back(7);
    const auto* base = buffer.get_address();
    if (!buffer.grow(64 * 1024)) return 1;
    assert(buffer.get_address() == base && &buffer.plain_root() == root);
    auto snapshot = buffer.save_verified();
    assert(buffer.get_address() == base && &buffer.plain_root() == root);

    auto restored = TypedXBuffer<StableExample>::load(snapshot, pinned);
    const auto* restored_base = restored.get_address();
    if (!restored.grow(64 * 1024)) return 1;
    assert(restored.get_address() == restored_base);
    assert(restored.root().sequence().get() == 42 && restored.root().values().get(0) == 7);
    if (buffer.grow(buffer.maximum_capacity() - buffer.get_size() + 1)) return 1;
    std::cout << "Base preserved; logical=" << buffer.get_size()
              << ", accessible=" << buffer.accessible_bytes()
              << ", reserved=" << buffer.reserved_bytes()
              << "; snapshot copied " << snapshot.size() << " bytes\n";

    XBufferPool pool({2, 2 * limit, 2 * limit});
    const auto options = XBuffer::maximum_capacity(limit)
        .with_growth(XGrowPolicy::Automatic).with_pool(pool);
    {
        auto automatic = TypedXBuffer<StableExample>::create(256, options);
        const auto* address = automatic.get_address();
        auto values = automatic.root().values();
        for (int i = 0; i < 20'000; ++i) values.push_back(i);
        assert(automatic.get_address() == address);
        const auto wire = automatic.save_verified();
        const auto reclaimed = automatic.reclaim_tail_pages();
        assert(reclaimed.complete());
        auto copy = TypedXBuffer<StableExample>::load(wire, options);
        assert(copy.root().values().back().get() == 19'999);
    }
    assert(pool.stats().cached_blocks == 2);
    pool.clear();
    assert(pool.stats().cached_blocks == 0);
}
