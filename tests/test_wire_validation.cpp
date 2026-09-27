// Purpose: Reject hostile offsets and allocation metadata before dereferencing or mutation.
#include <cassert>
#include <cstring>
#include <iostream>
#include <optional>
#include "../xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

struct CheckedRoot {
    int32_t id{};
    bool enabled{};
    bool bits[2];
    XString name;
    XVector<int32_t> values;
    XMap<int32_t, int32_t> map;
    XVector<XString> labels;
};
XOFFSET_REGISTER_SCHEMA_NAME(CheckedRoot, "test.CheckedRoot")

template <typename T> T read(const std::string& bytes, std::size_t pos) {
    T result{};
    std::memcpy(&result, bytes.data() + pos, sizeof(T));
    return result;
}
template <typename T> void write(std::string& bytes, std::size_t pos, const T& value) {
    std::memcpy(bytes.data() + pos, &value, sizeof(T));
}
void reject(const std::string& bytes) {
    bool rejected = false;
    try { (void)XBuffer::load_verified<CheckedRoot>(bytes); }
    catch (const XException&) { rejected = true; }
    assert(rejected);
}

int main() {
    auto buf = XBuffer::create<CheckedRoot>(8192);
    auto& r = buf.root<CheckedRoot>();
    r.id = 42;
    r.name = "checked";
    r.labels.push_back("array-zero");
    r.labels.push_back("array-one");
    r.values.reserve(4);
    r.values.push_back(10);
    r.values.reserve(16); // Include a retired allocation.
    r.map.try_emplace(1, 11);
    r.map.try_emplace(2, 22);
    const auto original = buf.save_verified<CheckedRoot>();
    const auto h = read<XWireHeaderV1>(original, 0);
    const std::size_t payload = sizeof(h), root = payload + h.root_offset, alloc = payload + h.allocator_offset;
    auto loaded = XBuffer::load_verified<CheckedRoot>(original);
    assert(loaded.root<CheckedRoot>().id == 42);

    for (auto bad : {uint32_t(1), uint32_t(0x7ffffff8), UINT32_MAX}) {
        auto bytes = original;
        write(bytes, alloc + offsetof(XAllocatorStateV1, freelist_head), bad);
        reject(bytes);
    }
    {
        auto bytes = original;
        write(bytes, payload + offsetof(XBufferCore::arena_header, used_end), uint64_t(48));
        reject(bytes);
    }
    for (auto offset : {offsetof(CheckedRoot, enabled), offsetof(CheckedRoot, bits) + 1}) {
        auto bytes = original;
        write(bytes, root + offset, uint8_t(2));
        reject(bytes);
    }
    {
        auto bytes = original;
        write(bytes, root + offsetof(CheckedRoot, labels) + offsetof(XFixedString, arena_delta), INT32_MAX);
        reject(bytes);
    }
    {
        auto bytes = original;
        auto state = read<XAllocatorStateV1>(bytes, alloc);
        for (auto head : state.freelist_head) if (head) {
            write(bytes, alloc + head, head); // Cycle back to this node.
            break;
        }
        reject(bytes);
    }
    {
        auto bytes = original;
        const auto origin = root + offsetof(CheckedRoot, map) + offsetof(XFixedVector<int32_t>, data_delta);
        const auto data = origin + read<int32_t>(bytes, origin);
        write(bytes, data + sizeof(XKeyValue<int32_t, int32_t>), int32_t(1));
        reject(bytes);
    }
    {
        auto bytes = original;
        write(bytes, root - sizeof(XBufferCore::AllocationHeader), uint32_t(1));
        reject(bytes);
    }
    {
        auto bytes = original;
        const auto vec = root + offsetof(CheckedRoot, values);
        const auto origin = vec + offsetof(XFixedVector<int32_t>, data_delta);
        write(bytes, origin, static_cast<int32_t>(root - origin));
        write(bytes, vec + offsetof(XFixedVector<int32_t>, capacity_), uint32_t(1));
        reject(bytes);
    }
    {
        auto bytes = original;
        write(bytes, payload + offsetof(XBufferCore::arena_header, root_offset), UINT64_MAX);
        reject(bytes);
    }
    {
        auto bytes = original;
        auto bad = h;
        bad.reserved_bytes = UINT64_MAX;
        write(bytes, 0, bad);
        reject(bytes);
    }
    // Some mutations affect legitimate scalar values or padding. Accepted
    // images must remain safely mutable, not merely readable once.
    std::size_t rejected = 0, accepted = 0;
    for (std::size_t i = 0; i < original.size(); ++i) {
        for (unsigned char mask : {1u, 128u}) {
            auto bytes = original;
            bytes[i] ^= mask;
            std::optional<XBuffer> candidate;
            try {
                candidate.emplace(XBuffer::load_verified<CheckedRoot>(bytes));
            } catch (const XException&) { ++rejected; continue; }
            assert(candidate->grow(8192));
            auto& value = candidate->root<CheckedRoot>();
            value.name = "still-mutable";
            value.labels.push_back("array-mutated");
            value.values.reserve(200);
            value.values.push_back(100);
            value.map.try_emplace(3, 33);
            auto again = XBuffer::load_verified<CheckedRoot>(candidate->save_verified<CheckedRoot>());
            assert(again.root<CheckedRoot>().values.back() == 100);
            ++accepted;
        }
    }
    assert(rejected > 0 && accepted > 0);
    std::cout << "Wire mutation sweep: " << rejected << " rejected, " << accepted << " mutable\n";
}
