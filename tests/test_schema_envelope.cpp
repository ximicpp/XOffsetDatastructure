// ============================================================================
// Test: Fixed-Schema Wire Header Verification
// Purpose: Validate the verified wire header (format 3.0: the image's own
//          40-byte header) and the free block envelope it carries.
// ============================================================================

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

struct EnvelopeData {
    int32_t id{0};
    XString name;
    XVector<int32_t> items;
};

struct EnvelopeDataAlias {
    int32_t id{0};
    XString name;
    XVector<int32_t> items;
};
XOFFSET_RECORD_DERIVED(EnvelopeData, (), id, name, items)
XOFFSET_RECORD_DERIVED(EnvelopeDataAlias, (), id, name, items)

XOFFSET_REGISTER_SCHEMA_NAME(EnvelopeData, "test.EnvelopeData")
XOFFSET_REGISTER_SCHEMA_NAME(EnvelopeDataAlias, "test.EnvelopeDataAlias")

static_assert(wire_root_type_id_v<EnvelopeData>() != wire_root_type_id_v<EnvelopeDataAlias>());
static_assert(wire_schema_hash_v<EnvelopeData>() != wire_schema_hash_v<EnvelopeDataAlias>());

bool test_verified_round_trip() {
    std::cout << "\n[TEST] verified round-trip\n";

    XBuffer xbuf(4096);
    auto* obj = xbuf.make<EnvelopeData>();
    obj->id = 7;
    obj->name = "Envelope";
    obj->items.push_back(10);
    obj->items.push_back(20);

    std::string data = xbuf.save_verified<EnvelopeData>();
    XBuffer loaded = XBuffer::load_verified<EnvelopeData>(data);
    auto& root = loaded.root<EnvelopeData>();

    assert(root.id == 7);
    assert(root.name == "Envelope");
    assert(root.items.size() == 2);
    assert(root.items[0] == 10);
    assert(root.items[1] == 20);
    std::cout << "  [OK]\n";
    return true;
}

bool test_magic_rejection() {
    std::cout << "\n[TEST] invalid magic rejected\n";

    XBuffer xbuf(4096);
    auto* obj = xbuf.make<EnvelopeData>();
    obj->id = 1;

    std::string data = xbuf.save_verified<EnvelopeData>();
    data[0] ^= 0x1;

    bool threw = false;
    try {
        (void)XBuffer::load_verified<EnvelopeData>(data);
    } catch (const XException&) {
        threw = true;
    }

    assert(threw);
    std::cout << "  [OK]\n";
    return true;
}

bool test_schema_rejection() {
    std::cout << "\n[TEST] schema mismatch rejected\n";

    XBuffer xbuf(4096);
    auto* obj = xbuf.make<EnvelopeData>();
    obj->id = 99;
    obj->name = "SchemaMismatch";

    std::string data = xbuf.save_verified<EnvelopeData>();

    bool threw = false;
    try {
        (void)XBuffer::load_verified<EnvelopeDataAlias>(data);
    } catch (const XException&) {
        threw = true;
    }

    assert(threw);
    std::cout << "  [OK]\n";
    return true;
}

// Format 3.0 has no endian tag (little-endian 64-bit only, checked at
// compile time) and no flags; a header size other than 40 is rejected.
bool test_flags_rejection() {
    std::cout << "\n[TEST] unsupported header size rejected\n";

    XBuffer xbuf(4096);
    auto* obj = xbuf.make<EnvelopeData>();
    obj->id = 5;

    std::string data = xbuf.save_verified<EnvelopeData>();
    XWireHeader header{};
    std::memcpy(&header, data.data(), sizeof(header));
    header.header_bytes = 48;
    std::memcpy(data.data(), &header, sizeof(header));

    bool threw = false;
    try {
        (void)XBuffer::load_verified<EnvelopeData>(data);
    } catch (const XException& e) {
        threw = std::string(e.what()).find("unsupported header size 0x00000030") != std::string::npos;
    }

    assert(threw);
    std::cout << "  [OK]\n";
    return true;
}

namespace {
bool rejects_with(const std::string& data, const char* expected) {
    try {
        (void)XBuffer::load_verified<EnvelopeData>(data);
    } catch (const XException& e) {
        return expected == nullptr || std::string(e.what()).find(expected) != std::string::npos;
    }
    return false;
}

std::string envelope_with_hole() {
    XBuffer xbuf(4096);
    auto* obj = xbuf.make<EnvelopeData>();
    obj->id = 8;
    obj->name = "envelope-with-a-heap-name";
    obj->items.push_back(1);
    obj->name = std::string(64, 'n'); // retires the old name block: one free block
    assert(xbuf.free_block_count() == 1);
    return xbuf.save_verified<EnvelopeData>();
}
}

// The free block envelope (free_dir, free_bytes in the header) must
// describe the image's free blocks exactly; exact coverage accounts for
// every free byte.
bool test_hole_envelope_rejection() {
    std::cout << "\n[TEST] inconsistent free block envelope rejected\n";

    const std::string data = envelope_with_hole();
    XWireHeader header{};
    std::memcpy(&header, data.data(), sizeof(header));
    // One free block (the free directory is a block of its own).
    const auto loaded = XBuffer::load_verified<EnvelopeData>(data);
    assert(loaded.free_block_count() == 1 && header.free_dir != 0 && header.free_bytes == loaded.free_block_bytes());
    assert(data.size() == header.used_bytes);

    auto mutate = [&](auto&& edit) {
        auto copy = data;
        XWireHeader h{};
        std::memcpy(&h, copy.data(), sizeof(h));
        edit(h);
        std::memcpy(copy.data(), &h, sizeof(h));
        return copy;
    };
    assert(rejects_with(mutate([](XWireHeader& h) { h.free_bytes += 8; }), "invalid free blocks"));
    assert(rejects_with(mutate([](XWireHeader& h) { h.free_bytes = 0; }), "invalid free blocks"));
    assert(rejects_with(mutate([](XWireHeader& h) { h.free_dir = 0; }), "invalid free blocks"));
    assert(rejects_with(mutate([](XWireHeader& h) { h.used_bytes += 8; }), "used size mismatch"));
    // Dropping the block from its stack together with its bytes leaves the
    // retired block unaccounted for: exact coverage fails.
    auto dropped = mutate([](XWireHeader& h) { h.free_bytes = 0; });
    const std::uint32_t empty = 0;
    std::memcpy(dropped.data() + header.free_dir + 12, &empty, sizeof(empty));
    assert(rejects_with(dropped, "payload is not exactly covered by live blocks and holes"));
    std::cout << "  [OK]\n";
    return true;
}

bool test_v1_wire_rejected() {
    std::cout << "\n[TEST] V1 wire format rejected\n";

    XBuffer xbuf(4096);
    auto* obj = xbuf.make<EnvelopeData>();
    obj->id = 11;
    std::string data = xbuf.save_verified<EnvelopeData>();

    auto v1 = data;
    std::memcpy(v1.data(), "XOFFV1\0\0", 8);
    assert(rejects_with(v1, "wire format 1 is not supported; this build reads 3.0"));
    auto v20 = data;
    std::memcpy(v20.data(), "XOFFV2\0\0", 8);
    assert(rejects_with(v20, "wire format 2.0 is not supported; this build reads 3.0"));

    XWireHeader header{};
    std::memcpy(&header, data.data(), sizeof(header));
    header.magic = XWIRE_MAGIC - (1u << 16) + (1u << 24); // 'X' 'O' 2.1
    auto v21 = data;
    std::memcpy(v21.data(), &header, sizeof(header));
    assert(rejects_with(v21, "wire format 2.1 is not supported; this build reads 3.0"));
    std::cout << "  [OK]\n";
    return true;
}

bool test_typed_buffer_verified_api() {
    std::cout << "\n[TEST] TypedXBuffer verified API\n";

    TypedXBuffer<EnvelopeData> xbuf(4096);
    auto* obj = xbuf.make();
    obj->id = 123;
    obj->name = "Typed";
    obj->items.push_back(5);

    std::string data = xbuf.save_verified();
    auto loaded = TypedXBuffer<EnvelopeData>::load_verified(data);
    auto& root = loaded.plain_root();

    assert(root.id == 123);
    assert(root.name == "Typed");
    assert(root.items.size() == 1);
    assert(root.items[0] == 5);
    std::cout << "  [OK]\n";
    return true;
}

bool test_verified_span_api() {
    std::cout << "\n[TEST] verified span API\n";

    TypedXBuffer<EnvelopeData> xbuf(4096);
    auto* obj = xbuf.make();
    obj->id = 321;
    obj->name = "Span";
    obj->items.push_back(9);

    std::string data = xbuf.save_verified();
    auto bytes = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data.data()), data.size());
    auto loaded = TypedXBuffer<EnvelopeData>::load_verified(bytes);
    auto& root = loaded.plain_root();

    assert(root.id == 321);
    assert(root.name == "Span");
    assert(root.items.size() == 1);
    assert(root.items[0] == 9);
    std::cout << "  [OK]\n";
    return true;
}

bool test_typed_load_defaults_to_verified() {
    std::cout << "\n[TEST] TypedXBuffer load() defaults to verified\n";

    TypedXBuffer<EnvelopeData> xbuf(4096);
    auto* obj = xbuf.make();
    obj->id = 456;
    obj->name = "DefaultLoad";
    obj->items.push_back(42);

    std::string data = xbuf.save_verified();
    auto loaded = TypedXBuffer<EnvelopeData>::load(data);
    auto& root = loaded.plain_root();

    assert(root.id == 456);
    assert(root.name == "DefaultLoad");
    assert(root.items.size() == 1);
    assert(root.items[0] == 42);

    bool threw = false;
    try {
        (void)TypedXBuffer<EnvelopeDataAlias>::load(data);
    } catch (const XException&) {
        threw = true;
    }
    assert(threw);

    std::cout << "  [OK]\n";
    return true;
}

bool test_verified_save_is_normalized_transport() {
    std::cout << "\n[TEST] verified save normalizes buffer slack but preserves mutation\n";

    TypedXBuffer<EnvelopeData> xbuf(4096);
    auto* obj = xbuf.make();
    obj->id = 777;
    obj->name = "Normalized";
    obj->items.reserve(8);
    obj->items.push_back(1);
    obj->items.push_back(2);

    auto before = xbuf.stats();
    assert(before.total_size == 4096);
    assert(before.free_size > 0);
    assert(obj->items.capacity() == 8);

    std::string data = xbuf.save_verified();
    XWireHeader header{};
    std::memcpy(&header, data.data(), sizeof(header));

    auto loaded = TypedXBuffer<EnvelopeData>::load_verified(data);
    auto after = loaded.stats();
    auto& root = loaded.plain_root();

    assert(after.total_size == header.used_bytes);
    assert(after.total_size < before.total_size);
    assert(root.id == 777);
    assert(root.name == "Normalized");
    assert(root.items.capacity() == 8);

    while (root.items.size() < root.items.capacity()) {
        root.items.push_back(static_cast<int32_t>(root.items.size() + 1));
    }

    assert(root.items.size() == 8);
    assert(root.items.back() == 8);
    std::cout << "  [OK]\n";
    return true;
}

int main() {
    std::cout << "=== Schema Wire Header Tests ===\n";

    bool all_passed = true;
    all_passed &= test_verified_round_trip();
    all_passed &= test_magic_rejection();
    all_passed &= test_schema_rejection();
    all_passed &= test_flags_rejection();
    all_passed &= test_hole_envelope_rejection();
    all_passed &= test_v1_wire_rejected();
    all_passed &= test_typed_buffer_verified_api();
    all_passed &= test_verified_span_api();
    all_passed &= test_typed_load_defaults_to_verified();
    all_passed &= test_verified_save_is_normalized_transport();

    std::cout << "\n";
    if (all_passed) {
        std::cout << "[PASS] All schema envelope tests passed!\n";
        return 0;
    }
    std::cout << "[FAIL] Some schema envelope tests failed!\n";
    return 1;
}
