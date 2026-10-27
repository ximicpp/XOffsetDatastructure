// ============================================================================
// Test: Fixed-Schema Wire Header Verification
// Purpose: Validate the verified V2 wire header wrapped around the current
//          raw XBuffer payload (and the hole table that follows it).
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

// V2.1 has no endian tag (little-endian 64-bit only, checked at compile
// time); every flag bit is reserved and rejected.
bool test_flags_rejection() {
    std::cout << "\n[TEST] reserved wire flags rejected\n";

    XBuffer xbuf(4096);
    auto* obj = xbuf.make<EnvelopeData>();
    obj->id = 5;

    std::string data = xbuf.save_verified<EnvelopeData>();
    XWireHeaderV2 header{};
    std::memcpy(&header, data.data(), sizeof(header));
    header.flags ^= 0x1;
    std::memcpy(data.data(), &header, sizeof(header));

    bool threw = false;
    try {
        (void)XBuffer::load_verified<EnvelopeData>(data);
    } catch (const XException& e) {
        threw = std::string(e.what()).find("unsupported wire flags 0x00000001") != std::string::npos;
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
    obj->name = std::string(64, 'n'); // retires the old name block: one hole
    assert(xbuf.free_block_count() == 1);
    return xbuf.save_verified<EnvelopeData>();
}
}

// V2 replacement of the retired allocator_offset check: the hole table's
// envelope (hole_count, used_bytes) must describe the wire exactly. V2.1 has
// no hole_bytes sum: exact coverage accounts for every hole byte.
bool test_hole_envelope_rejection() {
    std::cout << "\n[TEST] inconsistent hole table envelope rejected\n";

    const std::string data = envelope_with_hole();
    XWireHeaderV2 header{};
    std::memcpy(&header, data.data(), sizeof(header));
    assert(header.hole_count == 1);
    assert(data.size() == sizeof(header) + header.used_bytes - XWIRE_PAYLOAD_BEGIN +
                              header.hole_count * sizeof(XBufferCore::WireHole));
    (void)XBuffer::load_verified<EnvelopeData>(data);

    auto mutate = [&](auto&& edit) {
        auto copy = data;
        XWireHeaderV2 h{};
        std::memcpy(&h, copy.data(), sizeof(h));
        edit(h);
        std::memcpy(copy.data(), &h, sizeof(h));
        return copy;
    };
    assert(rejects_with(mutate([](XWireHeaderV2& h) { h.hole_count += 1; }), "used size mismatch"));
    assert(rejects_with(mutate([](XWireHeaderV2& h) { h.hole_count = 0; }), "used size mismatch"));
    assert(rejects_with(mutate([](XWireHeaderV2& h) { h.used_bytes += 8; }), "used size mismatch"));
    // Dropping the table entry together with its envelope leaves the retired
    // block unaccounted for: exact coverage fails.
    auto dropped = mutate([](XWireHeaderV2& h) { h.hole_count = 0; });
    dropped.resize(dropped.size() - sizeof(XBufferCore::WireHole));
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
    assert(rejects_with(v1, "wire format 1 is not supported; this build reads 2.1"));
    auto v20 = data;
    std::memcpy(v20.data(), "XOFFV2\0\0", 8);
    assert(rejects_with(v20, "wire format 2.0 is not supported; this build reads 2.1"));

    XWireHeaderV2 header{};
    std::memcpy(&header, data.data(), sizeof(header));
    header.magic = XWIRE_MAGIC - (1u << 16); // 'X' 'O' 1.1
    auto major1 = data;
    std::memcpy(major1.data(), &header, sizeof(header));
    assert(rejects_with(major1, "wire format 1.1 is not supported"));
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
    XWireHeaderV2 header{};
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
