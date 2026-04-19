// ============================================================================
// Test: V1 Wire Admission & Schema-Aware Buffer APIs
// Purpose: Validate the stricter fixed-schema v1 admission trait separately
//          from TypeLayout's broader byte-copy safety predicate.
// ============================================================================

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

struct WireRoot {
    template <typename Alloc>
    WireRoot(Alloc alloc)
        : title(alloc), blob(alloc), values(alloc), ids(alloc), names(alloc) {}

    int32_t id{0};
    XString title;
    XBlob blob;
    XVector<uint32_t> values;
    XSet<int32_t> ids;
    XMap<int32_t, XString> names;
};

struct WireOther {
    template <typename Alloc>
    WireOther(Alloc alloc) : label(alloc) {}

    XString label;
};

struct HasLong { long value; };
struct HasWchar { wchar_t value; };
struct HasLongDouble { long double value; };
struct HasPointer { int32_t* ptr; };
struct BaseRecord { int32_t base; };
struct DerivedRecord : BaseRecord { int32_t extra; };

XOFFSET_REGISTER_SCHEMA_NAME(WireRoot, "test.WireRoot")
XOFFSET_REGISTER_SCHEMA_NAME(WireOther, "test.WireOther")

static_assert(is_v1_wire_admitted_v<XBlob>);
static_assert(std::is_same_v<XFlatSet<int32_t>, XSet<int32_t>>);
static_assert(std::is_same_v<XFlatMap<int32_t, XString>, XMap<int32_t, XString>>);
static_assert(is_v1_wire_admitted_v<WireRoot>);
static_assert(is_v1_wire_admitted_v<WireOther>);
static_assert(!is_v1_wire_admitted_v<HasLong>);
static_assert(!is_v1_wire_admitted_v<HasWchar>);
static_assert(!is_v1_wire_admitted_v<HasLongDouble>);
static_assert(!is_v1_wire_admitted_v<HasPointer>);
static_assert(!is_v1_wire_admitted_v<DerivedRecord>);

bool test_v1_admission_round_trip() {
    std::cout << "\n[TEST] v1 admission round-trip\n";

    TypedXBuffer<WireRoot> xbuf(4096);
    auto* root = xbuf.make();
    root->id = 7;
    root->title = "wire-root";
    root->blob.push_back(std::byte{0x10});
    root->blob.push_back(std::byte{0x20});
    root->values.push_back(11);
    root->values.push_back(22);
    root->ids.insert(5);
    root->ids.insert(3);
    root->names.emplace(1, "one");
    root->names.emplace(2, "two");

    assert(xbuf.matches_schema());
    assert(xbuf.bytes().size() == xbuf.segment_size());

    std::string data = xbuf.save_verified();
    XBuffer loaded = XBuffer::load_verified<WireRoot>(data);

    assert(loaded.matches_schema<WireRoot>());
    assert(!loaded.matches_schema<WireOther>());

    auto& restored = loaded.root<WireRoot>();
    assert(restored.id == 7);
    assert(restored.title == "wire-root");
    assert(restored.blob.size() == 2);
    assert(std::to_integer<int>(restored.blob[0]) == 0x10);
    assert(std::to_integer<int>(restored.blob[1]) == 0x20);
    assert(restored.values.size() == 2);
    assert(restored.values[1] == 22);
    assert(restored.ids.contains(3));
    assert(restored.names.find(2) != restored.names.end());
    assert(restored.names.find(2)->second == "two");

    bool threw = false;
    try {
        (void)loaded.root<WireOther>();
    } catch (const XException&) {
        threw = true;
    }
    assert(threw);

    XBuffer compacted = XCompactor::compact<WireRoot>(xbuf);
    assert(compacted.root<WireRoot>().blob.size() == 2);

    std::cout << "  [OK]\n";
    return true;
}

bool test_raw_load_requires_unsafe_root() {
    std::cout << "\n[TEST] raw load requires unsafe_root()\n";

    TypedXBuffer<WireRoot> xbuf(4096);
    auto* root = xbuf.make();
    root->id = 99;
    root->title = "raw";

    XBuffer loaded = XBuffer::load_unverified(xbuf.save());
    assert(!loaded.matches_schema<WireRoot>());

    bool threw = false;
    try {
        (void)loaded.root<WireRoot>();
    } catch (const XException&) {
        threw = true;
    }
    assert(threw);
    assert(loaded.unsafe_root<WireRoot>().id == 99);

    std::cout << "  [OK]\n";
    return true;
}

bool test_raw_span_load_requires_unsafe_root() {
    std::cout << "\n[TEST] raw span load requires unsafe_root()\n";

    TypedXBuffer<WireRoot> xbuf(4096);
    auto* root = xbuf.make();
    root->id = 1234;
    root->title = "span-raw";

    std::string data = xbuf.save();
    auto bytes = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data.data()), data.size());
    XBuffer loaded = XBuffer::load_unverified(bytes);

    bool threw = false;
    try {
        (void)loaded.root<WireRoot>();
    } catch (const XException&) {
        threw = true;
    }
    assert(threw);
    assert(loaded.unsafe_root<WireRoot>().id == 1234);
    assert(loaded.unsafe_root<WireRoot>().title == "span-raw");

    std::cout << "  [OK]\n";
    return true;
}

int main() {
    std::cout << "\n=== V1 Wire Admission Tests ===\n";

    bool all = true;
    all &= test_v1_admission_round_trip();
    all &= test_raw_load_requires_unsafe_root();
    all &= test_raw_span_load_requires_unsafe_root();

    std::cout << "\n" << (all ? "[PASS] All v1 wire admission tests passed"
                               : "[FAIL] Some v1 wire admission tests failed")
              << "\n";
    return all ? 0 : 1;
}
