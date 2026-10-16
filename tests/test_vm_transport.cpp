// Transport optimization contracts: exact bytes, failure rollback and unchanged
// graph admission. --write-fixtures lets frozen/current builds compare wires.
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <new>
#include <span>
#include <string>
#include <string_view>

#include "../xoffsetdatastructure.hpp"

namespace {
thread_local bool fail_next_allocation = false;
}

// The next save output allocation is the only allocation armed by this test.
void* operator new(std::size_t size) {
    if (std::exchange(fail_next_allocation, false)) throw std::bad_alloc();
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

using namespace XOffsetDatastructure;

struct TransportRoot {
    std::uint64_t marker{};
    bool enabled{};
    XString name;
    XVector<std::uint8_t> bytes;
};
XOFFSET_REGISTER_SCHEMA_NAME(TransportRoot, "test.TransportRoot")

namespace {

XBuffer make_transport(std::size_t length) {
    auto buffer = XBuffer::create<TransportRoot>(length * 2 + 65536,
        XBuffer::maximum_capacity(length * 2 + 262144));
    auto& root = buffer.root<TransportRoot>();
    root.marker = 0x1122334455667788ULL;
    root.enabled = true;
    root.name = "transport copy with embedded data";
    root.bytes.reserve(length);
    for (std::size_t i = 0; i < length; ++i)
        root.bytes.push_back(static_cast<std::uint8_t>((i * 37 + 11) % 251 + 1));
    return buffer;
}

void check_root(const TransportRoot& root, std::size_t length) {
    assert(root.marker == 0x1122334455667788ULL && root.enabled);
    assert(root.name == "transport copy with embedded data");
    assert(root.bytes.size() == length);
    for (std::size_t i = 0; i < length; ++i)
        assert(root.bytes[i] == static_cast<std::uint8_t>((i * 37 + 11) % 251 + 1));
}

template <typename Operation>
void require_rejection(Operation&& operation) {
    bool rejected = false;
    try { operation(); }
    catch (const XException&) { rejected = true; }
    assert(rejected);
}

void check_transport(std::size_t length, const std::filesystem::path& fixture_directory) {
    auto source = make_transport(length);
    auto* root = &source.root<TransportRoot>();
    auto handle = source.handle<TransportRoot>();
    const auto* source_base = source.get_address();
    const auto epoch = source.epoch();
    const auto wire = source.save_verified<TransportRoot>();
    const auto payload = source.copy_bytes();
    assert(wire.size() == sizeof(XWireHeaderV1) + payload.size());
    assert(std::memcmp(wire.data() + sizeof(XWireHeaderV1), payload.data(), payload.size()) == 0);
    assert(source.save_verified<TransportRoot>() == wire);
    assert(source.get_address() == source_base && source.epoch() == epoch && handle.get() == root);

    auto loaded = XBuffer::load_verified<TransportRoot>(wire);
    check_root(loaded.root<TransportRoot>(), length);
    assert(loaded.copy_bytes() == payload && loaded.save_verified<TransportRoot>() == wire);
    auto exact = XBuffer::load_verified<TransportRoot>(wire,
        XBuffer::maximum_capacity(payload.size()));
    assert(exact.maximum_capacity() == payload.size() && !exact.grow(1));
    require_rejection([&] {
        (void)XBuffer::load_verified<TransportRoot>(wire,
            XBuffer::maximum_capacity(payload.size() - 1));
    });

    // A different live reservation is valid input, as is the replacement's own
    // old reservation: complete the copy before assignment releases its source.
    auto direct = XBuffer::load_unverified(source.bytes());
    assert(direct.copy_bytes() == payload);
    require_rejection([&] { (void)direct.root<TransportRoot>(); });
    check_root(direct.unsafe_root<TransportRoot>(), length);
    direct = XBuffer::load_unverified(direct.bytes());
    assert(direct.copy_bytes() == payload);

    auto* loaded_root = &loaded.root<TransportRoot>();
    const auto* loaded_base = loaded.get_address();
    for (unsigned cycle = 0; cycle < 3; ++cycle) {
        const auto before = loaded.get_size();
        assert(loaded.grow(32768 + cycle * 19));
        assert(std::all_of(loaded.data() + before, loaded.data() + loaded.get_size(),
                           [](char value) { return value == 0; }));
        std::memset(loaded.data() + before, 0x5a, loaded.get_size() - before);
        assert(loaded.save_verified<TransportRoot>() == wire);
        assert(loaded.get_address() == loaded_base && &loaded.root<TransportRoot>() == loaded_root);
        check_root(*loaded_root, length);
    }

    if (!fixture_directory.empty()) {
        std::filesystem::create_directories(fixture_directory);
        std::ofstream output(fixture_directory / (std::to_string(length) + ".wire"), std::ios::binary);
        output.write(wire.data(), static_cast<std::streamsize>(wire.size()));
        assert(output.good());
    }
}

void load_failures_preserve_target() {
    auto source = make_transport(65537);
    const auto wire = source.save_verified<TransportRoot>();
    auto target = make_transport(31);
    auto handle = target.handle<TransportRoot>();
    auto* root = handle.get();
    const auto* base = target.get_address();
    const auto before = target.copy_bytes();
    const auto epoch = target.epoch();
    for (const auto failure : {detail::VmOperation::Reserve, detail::VmOperation::Enable,
                               detail::VmOperation::Prepare}) {
        detail::fail_next_vm_operation(failure);
        require_rejection([&] { target = XBuffer::load_verified<TransportRoot>(wire); });
        assert(target.get_address() == base && target.epoch() == epoch && handle.get() == root);
        assert(target.copy_bytes() == before);
        auto retry = XBuffer::load_verified<TransportRoot>(wire);
        check_root(retry.root<TransportRoot>(), 65537);
    }
    std::string corrupt_header = wire;
    corrupt_header[sizeof(XWireHeaderV1)] = '?';
    std::string corrupt_graph = wire;
    XWireHeaderV1 header{};
    std::memcpy(&header, wire.data(), sizeof(header));
    corrupt_graph[sizeof(header) + header.root_offset + offsetof(TransportRoot, enabled)] = 2;
    for (const auto& invalid : {corrupt_header, corrupt_graph}) {
        for (unsigned retry = 0; retry < 4; ++retry)
            require_rejection([&] { target = XBuffer::load_verified<TransportRoot>(invalid); });
        assert(target.get_address() == base && target.epoch() == epoch && handle.get() == root);
        assert(target.copy_bytes() == before);
        check_root(target.root<TransportRoot>(), 31);
    }
    auto alloc = target.allocator<std::uint32_t>();
    auto* value = alloc.allocate(1);
    *value = 42;
    assert(*value == 42);
    alloc.deallocate(value, 1);
    detail::clear_vm_failures();
}

void save_failure_keeps_normalized_source_usable() {
    auto source = make_transport(65537);
    auto handle = source.handle<TransportRoot>();
    auto* root = handle.get();
    const auto* base = source.get_address();
    const auto epoch = source.epoch();
    const auto old_size = source.get_size();
    bool rejected = false;
    fail_next_allocation = true;
    try { (void)source.save_verified<TransportRoot>(); }
    catch (const std::bad_alloc&) { rejected = true; }
    assert(rejected && !fail_next_allocation);
    assert(source.get_size() < old_size);
    assert(source.get_address() == base && source.epoch() == epoch && handle.get() == root);
    check_root(*root, 65537);
    auto saved = source.save_verified<TransportRoot>();
    auto loaded = XBuffer::load_verified<TransportRoot>(saved);
    check_root(loaded.root<TransportRoot>(), 65537);
    saved[sizeof(XWireHeaderV1)] = '?';
    assert(source.data()[0] != '?');
}

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path fixtures;
    if (argc == 3 && std::string_view(argv[1]) == "--write-fixtures") fixtures = argv[2];
    else assert(argc == 1);
    for (const auto length : {std::size_t(0), std::size_t(31), std::size_t(65537)})
        check_transport(length, fixtures);
    load_failures_preserve_target();
    save_failure_keeps_normalized_source_usable();
}
