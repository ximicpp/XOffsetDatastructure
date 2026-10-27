#include "../xoffsetdatastructure.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>

using namespace XOffsetDatastructure;

struct PolicyRoot {
    std::uint32_t marker;
    XVector<std::uint32_t> values;
    XString text;
};
XOFFSET_RECORD_DERIVED(PolicyRoot, (), marker, values, text)
XOFFSET_REGISTER_SCHEMA_NAME(PolicyRoot, "test.storage.policies.v1")

struct NestedPolicyValue { XString first, second; };
struct NestedPolicyRoot {
    NestedPolicyValue source;
    XVector<NestedPolicyValue> rows;
};
XOFFSET_RECORD_DERIVED(NestedPolicyValue, (), first, second)
XOFFSET_RECORD_DERIVED(NestedPolicyRoot, (), source, rows)
XOFFSET_REGISTER_SCHEMA_NAME(NestedPolicyRoot, "test.storage.policies.nested.v1")

namespace {
constexpr std::size_t limit = 256 * 1024;

template <class F> void bad_alloc(F&& operation) {
    bool threw = false;
    try { operation(); } catch (const XBadAlloc&) { threw = true; }
    assert(threw);
}

void automatic_growth_and_limits() {
    const auto automatic = XBuffer::maximum_capacity(limit).with_growth(XGrowPolicy::Automatic);
    auto buffer = TypedXBuffer<PolicyRoot>::create(64, automatic);
    assert(buffer.growth_policy() == XGrowPolicy::Automatic);
    auto* root = &buffer.root();
    root->marker = 0x12345678;
    const auto base = buffer.get_address();
    const auto epoch = buffer.epoch();
    const auto initial = buffer.get_size();
    root->values.push_back(0);
    auto* values = root->values.data();
    for (std::uint32_t i = 1; i < 20'000; ++i) root->values.push_back(i);
    assert(buffer.get_size() > initial && buffer.get_size() <= limit);
    assert(buffer.get_address() == base && &buffer.root() == root);
    assert(root->values.data() == values && buffer.epoch() == epoch);
    for (std::uint32_t i = 0; i < root->values.size(); ++i) assert(root->values[i] == i);

    const auto used = buffer.arena()->used_end;
    const auto logical = buffer.get_size();
    const auto capacity = root->values.capacity();
    bad_alloc([&] { root->values.reserve(limit / sizeof(std::uint32_t) + 1); });
    assert(buffer.arena()->used_end == used && buffer.get_size() == logical);
    assert(root->values.capacity() == capacity && root->values[19'999] == 19'999);

    const auto wire = buffer.save_verified();
    auto loaded = TypedXBuffer<PolicyRoot>::load(wire, automatic);
    loaded.root().text = std::string(60'000, 'z');
    assert(loaded.root().marker == root->marker && loaded.root().values[19'999] == 19'999);
    assert(loaded.root().text.size() == 60'000);
    auto explicit_load = TypedXBuffer<PolicyRoot>::load(wire);
    assert(explicit_load.growth_policy() == XGrowPolicy::Explicit);
    bad_alloc([&] { explicit_load.root().text = std::string(60'000, 'x'); });
    explicit_load.set_growth_policy(XGrowPolicy::Automatic);
    explicit_load.root().text = std::string(60'000, 'x');

    auto raw = XBuffer::load_unverified(buffer.copy_bytes(), automatic);
    assert(raw.growth_policy() == XGrowPolicy::Automatic);
    auto* raw_allocation = raw.arena()->allocate_aligned(32'768, 8);
    assert(raw_allocation);
    bool rejected = false;
    try { (void)raw.root<PolicyRoot>(); } catch (const XException&) { rejected = true; }
    assert(rejected);

    // Choosing Automatic must work before bootstrap/root allocation, including
    // objects too large for the initial logical length.
    auto untyped = XBuffer::create<PolicyRoot>(64, automatic);
    assert(untyped.has_root<PolicyRoot>() && untyped.get_size() > 64);
    XBuffer explicit_buffer(4096, XBuffer::maximum_capacity(limit));
    auto* er = explicit_buffer.make<PolicyRoot>();
    bad_alloc([&] { er->values.reserve(4096); });
    assert(explicit_buffer.get_size() == 4096);
}

void failures_and_ownership() {
    using detail::VmOperation;
    for (const auto fault : {VmOperation::Enable, VmOperation::Prepare}) {
        auto buffer = TypedXBuffer<PolicyRoot>::create(4096,
            XBuffer::maximum_capacity(limit).with_growth(XGrowPolicy::Automatic));
        auto* root = &buffer.root();
        root->values.push_back(71);
        const auto old = buffer.copy_bytes();
        const auto logical = buffer.get_size();
        const auto base = buffer.get_address();
        const auto capacity = root->values.capacity();
        detail::fail_next_vm_operation(fault);
        bad_alloc([&] { root->values.reserve(32'768); });
        assert(buffer.get_size() == logical && buffer.copy_bytes() == old);
        assert(buffer.get_address() == base && root->values.capacity() == capacity);
        root->values.reserve(32'768);
        assert(root->values[0] == 71 && buffer.get_address() == base);
        detail::clear_vm_failures();
    }

    XBuffer left(4096, XBuffer::maximum_capacity(limit).with_growth(XGrowPolicy::Automatic));
    XBuffer right(4096, XBuffer::maximum_capacity(limit));
    left.make<PolicyRoot>()->marker = 3;
    right.make<PolicyRoot>()->marker = 7;
    left.swap(right);
    assert(left.growth_policy() == XGrowPolicy::Explicit);
    assert(right.growth_policy() == XGrowPolicy::Automatic);
    XBuffer moved(std::move(right));
    assert(moved.growth_policy() == XGrowPolicy::Automatic);
    assert(right.growth_policy() == XGrowPolicy::Explicit);
    XBufferCore target;
    target = std::move(static_cast<XBufferCore&>(moved));
    assert(target.growth_policy() == XGrowPolicy::Automatic);
    assert(moved.growth_policy() == XGrowPolicy::Explicit);
    assert(target.find_root_storage<PolicyRoot>()->marker == 3);
}

void automatic_reuses_before_growing() {
    auto buffer = TypedXBuffer<PolicyRoot>::create(4096,
        XBuffer::maximum_capacity(limit).with_growth(XGrowPolicy::Automatic));
    const auto alloc = fixed_detail::allocator_ref_of(buffer.arena());
    auto* left = fixed_detail::allocate_dynamic_bytes(alloc, 2048, 8);
    // V2 retires the exact block (its size is derived by the owner) into the
    // host free table. A smaller request is served by splitting that block
    // before the tail grows; the residual stays an exact free block.
    fixed_detail::retire_dynamic_bytes(alloc, left, 2048);
    buffer.shrink_to_fit();
    const auto logical = buffer.get_size();
    auto* reused = fixed_detail::allocate_dynamic_bytes(alloc, 1800, 8);
    assert(reused == left && buffer.get_size() == logical);
    assert(buffer.free_block_count() == 1 && buffer.free_block_bytes() == 2048 - 1800);
    fixed_detail::retire_dynamic_bytes(alloc, reused, 1800);
    auto* aligned = buffer.arena()->allocate_aligned(32'768, 32'768);
    assert(reinterpret_cast<std::uintptr_t>(aligned) % 32'768 == 0);
    assert(buffer.get_size() > logical && buffer.get_size() <= limit);
}

void nested_mutation_failure_after_successful_growth() {
    constexpr std::size_t hard_limit = 16 * 1024;
    auto buffer = TypedXBuffer<NestedPolicyRoot>::create(4096,
        XBuffer::maximum_capacity(hard_limit).with_growth(XGrowPolicy::Automatic));
    auto& root = buffer.root();
    root.rows.reserve(4);
    auto& existing = root.rows.emplace_back();
    existing.first = "old first";
    existing.second = "old second";
    root.source.first = std::string(2048, 'a');
    root.source.second = std::string(8192, 'b');
    buffer.shrink_to_fit();
    const auto before_logical = buffer.get_size();
    const auto before_used = buffer.arena()->used_end;
    const auto base = buffer.get_address();
    const auto epoch = buffer.epoch();
    // V2 blocks have no header: a string block is round8(size + 1).
    const auto block = [](std::size_t bytes) { return (bytes + 7u) & ~std::size_t(7); };
    const auto first_copy_bytes = block(root.source.first.size() + 1);
    const auto second_copy_bytes = block(root.source.second.size() + 1);
    assert(before_used + first_copy_bytes + 16 < hard_limit);
    assert(before_used + first_copy_bytes + second_copy_bytes > hard_limit);

    // The spare vector slot avoids vector growth. Its first copied string
    // allocates and grows successfully; the second exceeds the hard limit.
    bad_alloc([&] { root.rows.push_back(root.source); });
    assert(buffer.get_size() > before_logical && buffer.get_size() <= hard_limit);
    assert(buffer.arena()->used_end > before_used);
    assert(buffer.get_address() == base && buffer.epoch() == epoch);
    const auto check_original = [](const NestedPolicyRoot& value) {
        assert(value.rows.size() == 1);
        assert(value.rows[0].first == "old first" && value.rows[0].second == "old second");
        assert(value.source.first == std::string(2048, 'a'));
        assert(value.source.second == std::string(8192, 'b'));
    };
    check_original(root);
    auto loaded = TypedXBuffer<NestedPolicyRoot>::load(buffer.save_verified(),
        XBuffer::maximum_capacity(hard_limit * 2).with_growth(XGrowPolicy::Automatic));
    check_original(loaded.root());
    loaded.root().rows.push_back(loaded.root().source);
    assert(loaded.root().rows.size() == 2);
    assert(loaded.root().rows[1].first == std::string(2048, 'a'));
    assert(loaded.root().rows[1].second == std::string(8192, 'b'));
    auto roundtrip = TypedXBuffer<NestedPolicyRoot>::load(loaded.save_verified());
    assert(roundtrip.root().rows.size() == 2 && roundtrip.root().rows[0].first == "old first");
    assert(roundtrip.root().rows[1].first == std::string(2048, 'a'));
    assert(roundtrip.root().rows[1].second == std::string(8192, 'b'));
}

void reclaim_integration() {
    auto buffer = TypedXBuffer<PolicyRoot>::create(limit,
        XBuffer::maximum_capacity(limit).with_growth(XGrowPolicy::Automatic));
    auto* root = &buffer.root();
    root->marker = 99;
    for (std::uint32_t i = 0; i < 100; ++i) root->values.push_back(i);
    const auto base = buffer.get_address();
    const auto epoch = buffer.epoch();
    const auto wire = buffer.save_verified();
    const auto logical = buffer.get_size();
    const auto before = buffer.accessible_bytes();
    const auto reclaimed = buffer.reclaim_tail_pages();
    assert(reclaimed.complete() && reclaimed.inaccessible_bytes > 0);
    assert(buffer.get_size() == logical && buffer.accessible_bytes() < before);
    assert(buffer.get_address() == base && buffer.epoch() == epoch);
    assert(buffer.save_verified() == wire);
    root->values.reserve(20'000);
    assert(root->marker == 99 && root->values[99] == 99);
    assert(buffer.get_address() == base && buffer.epoch() == epoch);
    assert(buffer.reclaim_tail_pages().complete());
    XBuffer empty;
    assert(empty.reclaim_tail_pages().complete());
}

void pooled_verified_lifecycle() {
    XBufferPool pool(XBufferPool::Limits{4, 4 * limit, 4 * limit});
    const auto options = XBuffer::maximum_capacity(limit)
        .with_pool(pool).with_growth(XGrowPolicy::Automatic);
    const void* first = nullptr;
    std::string wire;
    {
        auto buffer = TypedXBuffer<PolicyRoot>::create(4096, options);
        auto* root = &buffer.root();
        root->marker = 123;
        root->text = std::string(50'000, 'a');
        root->values.push_back(42);
        first = buffer.get_address();
        wire = buffer.save_verified();
    }
    assert(pool.stats().cached_blocks == 1);
    {
        auto buffer = TypedXBuffer<PolicyRoot>::load(wire, options);
        assert(buffer.get_address() == first && pool.stats().cached_blocks == 0);
        assert(buffer.root().marker == 123 && buffer.root().values[0] == 42);
        auto moved = std::move(buffer);
        moved.root().values.push_back(43); // owner index follows pooled storage
        assert(moved.root().values[1] == 43);
    }
    assert(pool.stats().cached_blocks == 1 && pool.stats().hits >= 1);
    {
        // Empty construction must clear reused logical bytes and prior graph.
        auto clean = TypedXBuffer<PolicyRoot>::create(4096, options);
        assert(clean.get_address() == first);
        assert(clean.root().marker == 0 && clean.root().text.empty() && clean.root().values.empty());
        const auto offset = clean.arena()->used_end;
        const auto n = clean.get_size();
        for (auto i = offset; i < n; ++i) assert(clean.data()[i] == 0);
    }
    // Registration and graph failures must not leave cached addresses registered.
    detail::fail_next_registry_registration();
    bad_alloc([&] { (void)TypedXBuffer<PolicyRoot>::load(wire, options); });
    assert(pool.stats().cached_blocks == 1);
    detail::clear_registry_failures();
    auto malformed = wire;
    XWireHeaderV2 header;
    std::memcpy(&header, malformed.data(), sizeof(header));
    // Dense wire: payload offset p (>= 48) is wire byte sizeof(header) + p - 48.
    assert(header.extent_count == 0);
    auto* vector_bytes = malformed.data() + sizeof(header) + (header.root_offset - XWIRE_PAYLOAD_BEGIN)
        + offsetof(PolicyRoot, values);
    using Values = XVector<std::uint32_t>;
    std::uint32_t invalid_size;
    std::memcpy(&invalid_size, vector_bytes + offsetof(Values, capacity_), sizeof(invalid_size));
    ++invalid_size;
    std::memcpy(vector_bytes + offsetof(Values, size_), &invalid_size, sizeof(invalid_size));
    bool rejected = false;
    try { (void)TypedXBuffer<PolicyRoot>::load(malformed, options); }
    catch (const XException&) { rejected = true; }
    assert(rejected && pool.stats().cached_blocks == 1);
    auto retry = TypedXBuffer<PolicyRoot>::load(wire, options);
    assert(retry.root().values[0] == 42);
    pool.clear(); // live reservation is unaffected
    retry.root().values.push_back(44);
    assert(retry.root().values.back() == 44 && pool.stats().cached_blocks == 0);
}
} // namespace

int main() {
    automatic_growth_and_limits();
    failures_and_ownership();
    nested_mutation_failure_after_successful_growth();
    reclaim_integration();
    pooled_verified_lifecycle();
    // Last: its 32 KiB-aligned allocation currently trips a V2 library defect
    // (alignment is applied to the payload offset, not the address).
    automatic_reuses_before_growing();
    std::cout << "Storage policies passed\n";
}
