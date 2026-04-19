// ============================================================================
// Test: Phase-2 Fixed-Layout Container Skeletons
// Purpose: Validate the frozen-layout XFixedString / XFixedVector<T> skeletons
//          alongside the existing runtime.
// ============================================================================

#include <cassert>
#include <cstdint>
#include <iostream>

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

struct FixedRoot {
    int32_t id{0};
    XFixedString name;
    XFixedVector<int32_t> values;
};

struct FixedItem {
    int32_t code{0};
    XFixedString label;
};

struct FixedNestedRoot {
    XFixedString owner;
    XFixedVector<FixedItem> items;
};

struct FixedAssocRoot {
    XFixedFlatSet<int32_t> ids;
    XFixedFlatSet<XFixedString> names;
    XFixedFlatMap<int32_t, XFixedString> by_id;
    XFixedFlatMap<XFixedString, int32_t> by_name;
};

XOFFSET_REGISTER_SCHEMA_NAME(FixedRoot, "test.FixedRoot")
XOFFSET_REGISTER_SCHEMA_NAME(FixedNestedRoot, "test.FixedNestedRoot")
XOFFSET_REGISTER_SCHEMA_NAME(FixedAssocRoot, "test.FixedAssocRoot")

static_assert(sizeof(XFixedString) == 16);
static_assert(sizeof(XFixedVector<int32_t>) == 16);
static_assert(sizeof(XFixedFlatSet<int32_t>) == 16);
static_assert(sizeof(XFixedFlatMap<int32_t, int32_t>) == 16);
static_assert(is_byte_copy_safe_v<FixedRoot>);
static_assert(is_byte_copy_safe_v<FixedItem>);
static_assert(is_byte_copy_safe_v<FixedNestedRoot>);
static_assert(is_byte_copy_safe_v<FixedAssocRoot>);

template <typename Member>
std::size_t payload_offset(const XBuffer& xbuf, const Member* member) {
    auto* base = static_cast<const char*>(xbuf.get_address());
    auto* ptr = reinterpret_cast<const char*>(member);
    return static_cast<std::size_t>(ptr - base);
}

bool test_basic_mutation() {
    std::cout << "\n[TEST] fixed containers basic mutation\n";

    XBuffer xbuf(4096);
    auto* root = xbuf.make<FixedRoot>();
    root->id = 42;
    root->name = "FixedAlpha";
    root->values.push_back(10);
    root->values.push_back(20);
    root->values.push_back(30);

    assert(root->id == 42);
    assert(root->name == "FixedAlpha");
    assert(root->values.size() == 3);
    assert(root->values[0] == 10);
    assert(root->values[2] == 30);
    std::cout << "  [OK]\n";
    return true;
}

bool test_round_trip() {
    std::cout << "\n[TEST] fixed containers raw round-trip\n";

    XBuffer xbuf(4096);
    auto* root = xbuf.make<FixedRoot>();
    root->id = 7;
    root->name = "RoundTrip";
    for (int i = 0; i < 5; ++i) root->values.push_back(i * 11);

    std::string data = xbuf.save();
    XBuffer loaded = XBuffer::load_unverified(data);
    auto& restored = loaded.unsafe_root<FixedRoot>();

    assert(restored.id == 7);
    assert(restored.name == "RoundTrip");
    assert(restored.values.size() == 5);
    assert(restored.values[4] == 44);
    std::cout << "  [OK]\n";
    return true;
}

bool test_verified_round_trip() {
    std::cout << "\n[TEST] fixed containers verified round-trip\n";

    XBuffer xbuf(4096);
    auto* root = xbuf.make<FixedRoot>();
    root->id = 99;
    root->name = "Verified";
    root->values.push_back(1);
    root->values.push_back(2);

    std::string data = xbuf.save_verified<FixedRoot>();
    auto loaded = TypedXBuffer<FixedRoot>::load_verified(data);
    auto& restored = loaded.root();

    assert(restored.id == 99);
    assert(restored.name == "Verified");
    assert(restored.values.size() == 2);
    assert(restored.values[1] == 2);
    std::cout << "  [OK]\n";
    return true;
}

bool test_grow_survival() {
    std::cout << "\n[TEST] fixed containers survive grow()\n";

    XBuffer xbuf(1024);
    auto h = xbuf.make_handle<FixedRoot>();
    h->id = 500;
    h->name = "Grow";
    for (int i = 0; i < 16; ++i) h->values.push_back(i);

    bool grew = xbuf.grow(8192);
    assert(grew);

    assert(h->id == 500);
    assert(h->name == "Grow");
    assert(h->values.size() == 16);
    assert(h->values[15] == 15);
    std::cout << "  [OK]\n";
    return true;
}

bool test_nested_record_vector_growth() {
    std::cout << "\n[TEST] fixed vector supports reflected record elements\n";

    XBuffer xbuf(4096);
    auto* root = xbuf.make<FixedNestedRoot>();
    root->owner = "NestedOwner";

    for (int i = 0; i < 20; ++i) {
        auto& item = root->items.emplace_back();
        item.code = 100 + i;
        item.label = ("Item_" + std::to_string(i)).c_str();
    }

    assert(root->owner == "NestedOwner");
    assert(root->items.size() == 20);
    assert(root->items[0].code == 100);
    assert(root->items[0].label == "Item_0");
    assert(root->items[19].code == 119);
    assert(root->items[19].label == "Item_19");
    std::cout << "  [OK]\n";
    return true;
}

bool test_nested_record_round_trip() {
    std::cout << "\n[TEST] fixed nested records survive save/load\n";

    XBuffer xbuf(4096);
    auto* root = xbuf.make<FixedNestedRoot>();
    root->owner = "RoundTripNested";

    for (int i = 0; i < 6; ++i) {
        auto& item = root->items.emplace_back();
        item.code = i * 3;
        item.label = ("Nested_" + std::to_string(i)).c_str();
    }

    std::string data = xbuf.save_verified<FixedNestedRoot>();
    auto loaded = TypedXBuffer<FixedNestedRoot>::load_verified(data);
    auto& restored = loaded.root();

    assert(restored.owner == "RoundTripNested");
    assert(restored.items.size() == 6);
    assert(restored.items[3].code == 9);
    assert(restored.items[3].label == "Nested_3");
    std::cout << "  [OK]\n";
    return true;
}

bool test_nested_record_compaction() {
    std::cout << "\n[TEST] fixed nested records survive compaction\n";

    XBuffer xbuf(8192);
    auto* root = xbuf.make<FixedNestedRoot>();
    root->owner = "CompactedNested";

    for (int i = 0; i < 12; ++i) {
        auto& item = root->items.emplace_back();
        item.code = 1000 + i;
        item.label = ("Compact_" + std::to_string(i)).c_str();
    }

    auto before = xbuf.stats();
    XBuffer compacted = XCompactor::compact<FixedNestedRoot>(xbuf);
    auto after = compacted.stats();
    auto& restored = compacted.root<FixedNestedRoot>();

    assert(restored.owner == "CompactedNested");
    assert(restored.items.size() == 12);
    assert(restored.items[0].label == "Compact_0");
    assert(restored.items[11].code == 1011);
    assert(restored.items[11].label == "Compact_11");
    assert(after.used_size <= before.used_size);
    std::cout << "  [OK]\n";
    return true;
}

bool test_verified_rejects_corrupt_fixed_string() {
    std::cout << "\n[TEST] verified load rejects corrupt fixed string\n";

    XBuffer xbuf(4096);
    auto* root = xbuf.make<FixedRoot>();
    root->id = 12;
    root->name = "BrokenName";
    root->values.push_back(1);

    std::string data = xbuf.save_verified<FixedRoot>();
    const std::size_t size_offset = sizeof(XWireHeaderV1) +
        payload_offset(xbuf, &root->name.size_);
    std::uint32_t bad_size = root->name.capacity_ + 1;
    std::memcpy(data.data() + size_offset, &bad_size, sizeof(bad_size));

    bool threw = false;
    try {
        (void)TypedXBuffer<FixedRoot>::load_verified(data);
    } catch (const XException&) {
        threw = true;
    }

    assert(threw);
    std::cout << "  [OK]\n";
    return true;
}

bool test_verified_rejects_corrupt_nested_record() {
    std::cout << "\n[TEST] verified load rejects corrupt nested fixed record\n";

    XBuffer xbuf(4096);
    auto* root = xbuf.make<FixedNestedRoot>();
    root->owner = "NestedBroken";
    for (int i = 0; i < 4; ++i) {
        auto& item = root->items.emplace_back();
        item.code = i;
        item.label = ("Nested_" + std::to_string(i)).c_str();
    }

    std::string data = xbuf.save_verified<FixedNestedRoot>();
    auto* nested_size = &root->items[2].label.size_;
    const std::size_t size_offset =
        sizeof(XWireHeaderV1) + payload_offset(xbuf, nested_size);
    std::uint32_t bad_size = root->items[2].label.capacity_ + 5;
    std::memcpy(data.data() + size_offset, &bad_size, sizeof(bad_size));

    bool threw = false;
    try {
        (void)TypedXBuffer<FixedNestedRoot>::load_verified(data);
    } catch (const XException&) {
        threw = true;
    }

    assert(threw);
    std::cout << "  [OK]\n";
    return true;
}

bool test_fixed_flat_set_map_basic() {
    std::cout << "\n[TEST] fixed flat set/map basic operations\n";

    XBuffer xbuf(8192);
    auto* root = xbuf.make<FixedAssocRoot>();

    root->ids.insert(4);
    root->ids.insert(2);
    root->ids.insert(9);
    root->ids.insert(2);

    root->names.emplace("charlie");
    root->names.emplace("alpha");
    root->names.emplace("bravo");
    root->names.emplace("alpha");

    root->by_id.emplace(7, "seven");
    root->by_id.emplace(3, "three");
    root->by_id.emplace(5, "five");
    root->by_id.insert_or_assign(5, "FIVE");

    root->by_name["delta"] = 4;
    root->by_name["beta"] = 2;
    root->by_name["alpha"] = 1;
    root->by_name.insert_or_assign("beta", 22);

    assert(root->ids.size() == 3);
    assert(root->ids.begin()[0] == 2);
    assert(root->ids.begin()[1] == 4);
    assert(root->ids.begin()[2] == 9);

    assert(root->names.size() == 3);
    assert(root->names.begin()[0] == "alpha");
    assert(root->names.begin()[1] == "bravo");
    assert(root->names.begin()[2] == "charlie");
    assert(root->names.find("bravo") != root->names.end());
    assert(root->names.find("zzz") == root->names.end());

    assert(root->by_id.size() == 3);
    assert(root->by_id.begin()[0].first == 3);
    assert(root->by_id.begin()[0].second == "three");
    assert(root->by_id.find(5)->second == "FIVE");

    assert(root->by_name.size() == 3);
    assert(root->by_name.begin()[0].first == "alpha");
    assert(root->by_name.begin()[1].first == "beta");
    assert(root->by_name.find("beta")->second == 22);
    std::cout << "  [OK]\n";
    return true;
}

bool test_fixed_flat_set_map_round_trip() {
    std::cout << "\n[TEST] fixed flat set/map round-trip and compaction\n";

    XBuffer xbuf(8192);
    auto* root = xbuf.make<FixedAssocRoot>();

    for (int i = 5; i >= 0; --i) {
        root->ids.insert(i);
        root->by_id.emplace(i, ("v" + std::to_string(i)).c_str());
    }
    root->names.emplace("lima");
    root->names.emplace("echo");
    root->by_name["lima"] = 12;
    root->by_name["echo"] = 5;

    std::string data = xbuf.save_verified<FixedAssocRoot>();
    auto loaded = TypedXBuffer<FixedAssocRoot>::load_verified(data);
    auto& restored = loaded.root();

    assert(restored.ids.size() == 6);
    assert(restored.ids.begin()[0] == 0);
    assert(restored.by_id.find(4)->second == "v4");
    assert(restored.names.begin()[0] == "echo");
    assert(restored.by_name.find("lima")->second == 12);

    auto before = xbuf.stats();
    auto compacted = XCompactor::compact<FixedAssocRoot>(xbuf);
    auto after = compacted.stats();
    auto& compacted_root = compacted.root<FixedAssocRoot>();

    assert(compacted_root.by_id.find(2)->second == "v2");
    assert(compacted_root.by_name.find("echo")->second == 5);
    assert(after.used_size <= before.used_size);
    std::cout << "  [OK]\n";
    return true;
}

bool test_verified_rejects_corrupt_fixed_flat_map() {
    std::cout << "\n[TEST] verified load rejects corrupt fixed flat map\n";

    XBuffer xbuf(8192);
    auto* root = xbuf.make<FixedAssocRoot>();
    root->by_id.emplace(1, "one");
    root->by_id.emplace(2, "two");
    root->by_id.emplace(3, "three");

    std::string data = xbuf.save_verified<FixedAssocRoot>();
    auto* bad_size = &root->by_id.begin()[1].second.size_;
    const std::size_t size_offset =
        sizeof(XWireHeaderV1) + payload_offset(xbuf, bad_size);
    std::uint32_t corrupted = root->by_id.begin()[1].second.capacity_ + 9;
    std::memcpy(data.data() + size_offset, &corrupted, sizeof(corrupted));

    bool threw = false;
    try {
        (void)TypedXBuffer<FixedAssocRoot>::load_verified(data);
    } catch (const XException&) {
        threw = true;
    }

    assert(threw);
    std::cout << "  [OK]\n";
    return true;
}

int main() {
    std::cout << "=== Fixed Layout Container Tests ===\n";

    bool all_passed = true;
    all_passed &= test_basic_mutation();
    all_passed &= test_round_trip();
    all_passed &= test_verified_round_trip();
    all_passed &= test_grow_survival();
    all_passed &= test_nested_record_vector_growth();
    all_passed &= test_nested_record_round_trip();
    all_passed &= test_nested_record_compaction();
    all_passed &= test_verified_rejects_corrupt_fixed_string();
    all_passed &= test_verified_rejects_corrupt_nested_record();
    all_passed &= test_fixed_flat_set_map_basic();
    all_passed &= test_fixed_flat_set_map_round_trip();
    all_passed &= test_verified_rejects_corrupt_fixed_flat_map();

    std::cout << "\n";
    if (all_passed) {
        std::cout << "[PASS] All fixed layout container tests passed!\n";
        return 0;
    }

    std::cout << "[FAIL] Some fixed layout container tests failed!\n";
    return 1;
}
