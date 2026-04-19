// ============================================================================
// Test: Frozen Main Containers
// Purpose: Validate that the public container family runs on the frozen
//          fixed-layout ABI, including verified load and compaction paths.
// ============================================================================

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

struct BridgeItem {
    int32_t id{0};
    XString label;

    template <typename Alloc>
    BridgeItem(Alloc alloc) : label(alloc) {}
};

struct BridgeRoot {
    XString owner;
    XVector<int32_t> scores;
    XVector<XString> tags;
    XVector<BridgeItem> items;

    template <typename Alloc>
    BridgeRoot(Alloc alloc)
        : owner(alloc), scores(alloc), tags(alloc), items(alloc) {}
};

struct BridgeAssocRoot {
    XSet<int32_t> ids;
    XSet<XString> names;
    XMap<int32_t, XString> by_id;
    XMap<XString, int32_t> by_name;

    template <typename Alloc>
    BridgeAssocRoot(Alloc alloc)
        : ids(alloc), names(alloc), by_id(alloc), by_name(alloc) {}
};

XOFFSET_REGISTER_SCHEMA_NAME(BridgeRoot, "test.BridgeRoot")
XOFFSET_REGISTER_SCHEMA_NAME(BridgeAssocRoot, "test.BridgeAssocRoot")

static_assert(sizeof(XString) == 16);
static_assert(sizeof(XVector<int32_t>) == 16);
static_assert(sizeof(XSet<int32_t>) == 16);
static_assert(sizeof(XMap<int32_t, int32_t>) == 16);
static_assert(is_byte_copy_safe_v<BridgeItem>);
static_assert(is_byte_copy_safe_v<BridgeRoot>);
static_assert(is_byte_copy_safe_v<BridgeAssocRoot>);

template <typename Member>
static std::size_t payload_offset(const XBuffer& xbuf, const Member* member) {
    auto* base = static_cast<const char*>(xbuf.get_address());
    auto* ptr = reinterpret_cast<const char*>(member);
    return static_cast<std::size_t>(ptr - base);
}

static void populate_root(BridgeRoot& root) {
    root.owner = "BridgeOwner";
    root.scores.push_back(10);
    root.scores.push_back(20);
    root.scores.resize(4, 99);

    root.tags.push_back("alpha");
    root.tags.push_back(XString("gamma", root.tags.get_stored_allocator()));
    root.tags.insert(root.tags.begin() + 1, "beta");
    root.tags.erase(root.tags.begin());

    auto& first = root.items.emplace_back();
    first.id = 1;
    first.label = "sword";

    auto& second = root.items.emplace_back();
    second.id = 2;
    second.label = "shield";
}

static void assert_base_shape(const BridgeRoot& root, std::size_t expected_items = 2) {
    assert(root.owner == "BridgeOwner");
    assert(root.scores.size() == 4);
    assert(root.scores[0] == 10);
    assert(root.scores[1] == 20);
    assert(root.scores[2] == 99);
    assert(root.scores[3] == 99);

    assert(root.tags.size() == 2);
    assert(root.tags[0] == "beta");
    assert(root.tags[1] == "gamma");

    assert(root.items.size() == expected_items);
    assert(root.items[0].id == 1);
    assert(root.items[0].label == "sword");
    assert(root.items[1].id == 2);
    assert(root.items[1].label == "shield");
}

static void populate_assoc_root(BridgeAssocRoot& root) {
    root.ids.insert(4);
    root.ids.insert(2);
    root.ids.insert(9);
    root.ids.insert(2);

    root.names.emplace("charlie");
    root.names.emplace("alpha");
    root.names.emplace("bravo");
    root.names.emplace("alpha");

    root.by_id.emplace(7, "seven");
    root.by_id.emplace(3, "three");
    root.by_id[5] = "five";
    root.by_id.insert_or_assign(5, "FIVE");

    root.by_name["delta"] = 4;
    root.by_name["beta"] = 2;
    root.by_name["alpha"] = 1;
    root.by_name.insert_or_assign("beta", 22);
}

static void assert_assoc_shape(const BridgeAssocRoot& root) {
    assert(root.ids.size() == 3);
    assert(root.ids.begin()[0] == 2);
    assert(root.ids.begin()[1] == 4);
    assert(root.ids.begin()[2] == 9);

    assert(root.names.size() == 3);
    assert(root.names.begin()[0] == "alpha");
    assert(root.names.begin()[1] == "bravo");
    assert(root.names.begin()[2] == "charlie");
    assert(root.names.contains("alpha"));
    assert(!root.names.contains("zzz"));

    assert(root.by_id.size() == 3);
    assert(root.by_id.begin()[0].first == 3);
    assert(root.by_id.begin()[0].second == "three");
    assert(root.by_id.find(5)->second == "FIVE");
    assert(root.by_id.contains(7));

    assert(root.by_name.size() == 3);
    assert(root.by_name.begin()[0].first == "alpha");
    assert(root.by_name.begin()[1].first == "beta");
    assert(root.by_name.find("beta")->second == 22);
    assert(!root.by_name.contains("omega"));
}

bool test_main_containers_basic() {
    std::cout << "\n[TEST] main containers basic mutation\n";

    XBuffer xbuf(8192);
    auto* root = xbuf.make<BridgeRoot>();
    populate_root(*root);
    assert_base_shape(*root);

    constexpr auto sig = boost::typelayout::get_layout_signature<BridgeRoot>();
    std::string sig_text = sig.value;
    assert(sig_text.find("string[") != std::string::npos);
    assert(sig_text.find("vector[") != std::string::npos);

    std::cout << "  [OK]\n";
    return true;
}

bool test_main_containers_round_trip() {
    std::cout << "\n[TEST] main containers save/load + verified load\n";

    XBuffer xbuf(8192);
    auto* root = xbuf.make<BridgeRoot>();
    populate_root(*root);

    XBuffer raw_loaded = XBuffer::load_unverified(xbuf.save());
    assert_base_shape(raw_loaded.unsafe_root<BridgeRoot>());

    auto verified_loaded = TypedXBuffer<BridgeRoot>::load_verified(
        xbuf.save_verified<BridgeRoot>());
    assert_base_shape(verified_loaded.root());

    std::cout << "  [OK]\n";
    return true;
}

bool test_main_containers_compaction() {
    std::cout << "\n[TEST] main containers compaction\n";

    XBuffer xbuf(16384);
    auto* root = xbuf.make<BridgeRoot>();
    populate_root(*root);

    for (int i = 0; i < 24; ++i) {
        auto& item = root->items.emplace_back();
        item.id = 100 + i;
        item.label = ("extra_" + std::to_string(i)).c_str();
    }

    XBuffer compacted = XCompactor::compact<BridgeRoot>(xbuf);
    auto& restored = compacted.root<BridgeRoot>();

    assert_base_shape(restored, 26);
    assert(restored.items.back().id == 123);
    assert(restored.items.back().label == "extra_23");

    std::cout << "  [OK]\n";
    return true;
}

bool test_main_containers_assoc_basic() {
    std::cout << "\n[TEST] main containers map/set basic mutation\n";

    XBuffer xbuf(8192);
    auto* root = xbuf.make<BridgeAssocRoot>();
    populate_assoc_root(*root);
    assert_assoc_shape(*root);

    constexpr auto sig = boost::typelayout::get_layout_signature<BridgeAssocRoot>();
    std::string sig_text = sig.value;
    assert(sig_text.find("set[") != std::string::npos);
    assert(sig_text.find("map[") != std::string::npos);

    std::cout << "  [OK]\n";
    return true;
}

bool test_main_containers_assoc_round_trip_and_compaction() {
    std::cout << "\n[TEST] main containers map/set verified load + compaction\n";

    XBuffer xbuf(8192);
    auto* root = xbuf.make<BridgeAssocRoot>();
    populate_assoc_root(*root);

    auto verified_loaded = TypedXBuffer<BridgeAssocRoot>::load_verified(
        xbuf.save_verified<BridgeAssocRoot>());
    assert_assoc_shape(verified_loaded.root());

    auto compacted = XCompactor::compact<BridgeAssocRoot>(xbuf);
    assert_assoc_shape(compacted.root<BridgeAssocRoot>());

    std::cout << "  [OK]\n";
    return true;
}

bool test_main_containers_assoc_rejects_corrupt_map() {
    std::cout << "\n[TEST] main containers verified load rejects corrupt map payload\n";

    XBuffer xbuf(8192);
    auto* root = xbuf.make<BridgeAssocRoot>();
    populate_assoc_root(*root);

    std::string data = xbuf.save_verified<BridgeAssocRoot>();
    auto* bad_size = &root->by_id.begin()[1].second.size_;
    const std::size_t size_offset =
        sizeof(XWireHeaderV1) + payload_offset(xbuf, bad_size);
    std::uint32_t corrupted = root->by_id.begin()[1].second.capacity_ + 9;
    std::memcpy(data.data() + size_offset, &corrupted, sizeof(corrupted));

    bool threw = false;
    try {
        (void)TypedXBuffer<BridgeAssocRoot>::load_verified(data);
    } catch (const XException&) {
        threw = true;
    }

    assert(threw);
    std::cout << "  [OK]\n";
    return true;
}

int main() {
    bool all_passed = true;
    all_passed &= test_main_containers_basic();
    all_passed &= test_main_containers_round_trip();
    all_passed &= test_main_containers_compaction();
    all_passed &= test_main_containers_assoc_basic();
    all_passed &= test_main_containers_assoc_round_trip_and_compaction();
    all_passed &= test_main_containers_assoc_rejects_corrupt_map();

    if (!all_passed) return 1;

    std::cout << "\n[PASS] Frozen main container tests passed\n";
    return 0;
}
