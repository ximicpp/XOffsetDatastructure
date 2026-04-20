// ============================================================================
// Test: Fixed-Schema Wire Header Verification
// Purpose: Validate the verified v1 wire header wrapped around the current
//          raw XBuffer payload.
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

bool test_endian_tag_rejection() {
    std::cout << "\n[TEST] invalid endian tag rejected\n";

    XBuffer xbuf(4096);
    auto* obj = xbuf.make<EnvelopeData>();
    obj->id = 5;

    std::string data = xbuf.save_verified<EnvelopeData>();
    XWireHeaderV1 header{};
    std::memcpy(&header, data.data(), sizeof(header));
    header.endian_tag ^= 0x1;
    std::memcpy(data.data(), &header, sizeof(header));

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

bool test_allocator_offset_rejection() {
    std::cout << "\n[TEST] invalid allocator offset rejected\n";

    XBuffer xbuf(4096);
    auto* obj = xbuf.make<EnvelopeData>();
    obj->id = 8;

    std::string data = xbuf.save_verified<EnvelopeData>();
    XWireHeaderV1 header{};
    std::memcpy(&header, data.data(), sizeof(header));
    header.allocator_offset += 8;
    std::memcpy(data.data(), &header, sizeof(header));

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

bool test_typed_buffer_verified_api() {
    std::cout << "\n[TEST] TypedXBuffer verified API\n";

    TypedXBuffer<EnvelopeData> xbuf(4096);
    auto* obj = xbuf.make();
    obj->id = 123;
    obj->name = "Typed";
    obj->items.push_back(5);

    std::string data = xbuf.save_verified();
    auto loaded = TypedXBuffer<EnvelopeData>::load_verified(data);
    auto& root = loaded.root();

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
    auto& root = loaded.root();

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
    auto& root = loaded.root();

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
    XWireHeaderV1 header{};
    std::memcpy(&header, data.data(), sizeof(header));

    auto loaded = TypedXBuffer<EnvelopeData>::load_verified(data);
    auto after = loaded.stats();
    auto& root = loaded.root();

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
    all_passed &= test_endian_tag_rejection();
    all_passed &= test_allocator_offset_rejection();
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
