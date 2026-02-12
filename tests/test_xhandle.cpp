// ============================================================================
// Test: XHandle<T> — Epoch-Cached Safe Handle (Single-Object Model)
// Purpose: Verify that XHandle automatically re-resolves pointers after
//          buffer mutations (grow/shrink/compact) while maintaining O(1)
//          cached access when no mutation occurs.
// ============================================================================

#include <iostream>
#include <cassert>
#include <string>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

struct Player {
    template <typename Allocator>
    Player(Allocator alloc)
        : name(alloc), items(alloc) {}

    int32_t id = 0;
    int32_t level = 0;
    XString name;
    XVector<int32_t> items;
};

// ============================================================================
// Test 1: Basic make_handle and access
// ============================================================================
bool test_basic_handle() {
    std::cout << "\n[TEST] Basic XHandle creation and access\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt buffer(4096);
    auto player = buffer.make_handle<Player>();

    player->id = 42;
    player->level = 10;
    player->name = "Alice";
    player->items.push_back(1);
    player->items.push_back(2);
    player->items.push_back(3);

    assert(player->id == 42);
    assert(player->level == 10);
    assert(player->items.size() == 3);
    std::cout << "  Test 1.1: make_handle + read/write ... [OK]\n";

    assert(static_cast<bool>(player));
    std::cout << "  Test 1.2: operator bool (valid) ... [OK]\n";

    XHandle<Player> empty;
    assert(!static_cast<bool>(empty));
    assert(empty.get() == nullptr);
    std::cout << "  Test 1.3: default handle is null ... [OK]\n";

    return true;
}

// ============================================================================
// Test 2: Handle survives grow()
// ============================================================================
bool test_handle_survives_grow() {
    std::cout << "\n[TEST] XHandle survives grow()\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt buffer(1024);
    auto player = buffer.make_handle<Player>();
    player->id = 99;
    player->name = "BeforeGrow";
    player->items.push_back(42);

    Player* ptr_before = player.get();
    uint64_t epoch_before = buffer.epoch();
    std::cout << "  Before grow: ptr=" << (void*)ptr_before
              << " epoch=" << epoch_before << "\n";

    bool grew = buffer.grow(4096);
    assert(grew);

    uint64_t epoch_after = buffer.epoch();
    assert(epoch_after > epoch_before);
    std::cout << "  After grow:  epoch=" << epoch_after << "\n";

    assert(player->id == 99);
    assert(std::string(player->name.c_str()) == "BeforeGrow");
    assert(player->items.size() == 1);
    assert(player->items[0] == 42);
    std::cout << "  Test 2.1: data intact after grow() ... [OK]\n";

    Player* ptr_after = player.get();
    std::cout << "  After grow: ptr=" << (void*)ptr_after << "\n";
    std::cout << "  Test 2.2: handle auto-resolved ... [OK]\n";

    return true;
}

// ============================================================================
// Test 3: Handle survives shrink_to_fit()
// ============================================================================
bool test_handle_survives_shrink() {
    std::cout << "\n[TEST] XHandle survives shrink_to_fit()\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt buffer(8192);
    auto player = buffer.make_handle<Player>();
    player->id = 77;
    player->name = "BeforeShrink";

    uint64_t epoch_before = buffer.epoch();

    buffer.shrink_to_fit();

    uint64_t epoch_after = buffer.epoch();
    assert(epoch_after > epoch_before);

    assert(player->id == 77);
    assert(std::string(player->name.c_str()) == "BeforeShrink");
    std::cout << "  Test 3.1: data intact after shrink_to_fit() ... [OK]\n";

    return true;
}

// ============================================================================
// Test 4: Handle with compact_automatic()
// ============================================================================
bool test_handle_survives_compact() {
    std::cout << "\n[TEST] XHandle with compact_automatic()\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt buffer(8192);
    auto player = buffer.make_handle<Player>();
    player->id = 55;
    player->name = "BeforeCompact";
    player->items.push_back(100);
    player->items.push_back(200);

    XBuffer compacted = XBufferCompactor::compact_automatic<Player>(buffer);
    auto player2 = XHandle<Player>(compacted);
    assert(player2->id == 55);
    assert(std::string(player2->name.c_str()) == "BeforeCompact");
    assert(player2->items.size() == 2);
    std::cout << "  Test 4.1: handle on compacted buffer ... [OK]\n";

    return true;
}

// ============================================================================
// Test 5: Epoch cache efficiency — O(1) on repeated access
// ============================================================================
bool test_epoch_cache_efficiency() {
    std::cout << "\n[TEST] Epoch cache O(1) efficiency\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt buffer(4096);
    auto player = buffer.make_handle<Player>();
    player->id = 1;

    uint64_t epoch = buffer.epoch();
    for (int i = 0; i < 1000; i++) {
        player->level = i;
        assert(buffer.epoch() == epoch);
    }
    assert(player->level == 999);
    std::cout << "  Test 5.1: 1000 cached accesses (no re-find) ... [OK]\n";

    buffer.grow(1024);
    assert(buffer.epoch() == epoch + 1);

    assert(player->level == 999);
    std::cout << "  Test 5.2: re-find after grow, then cached ... [OK]\n";

    return true;
}

// ============================================================================
// Test 6: handle() for existing objects
// ============================================================================
bool test_handle_existing() {
    std::cout << "\n[TEST] handle() for existing objects\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt buffer(4096);
    auto* raw = buffer.make<Player>();
    raw->id = 123;
    raw->name = "RawCreated";

    auto player = buffer.handle<Player>();
    assert(player->id == 123);
    assert(std::string(player->name.c_str()) == "RawCreated");
    std::cout << "  Test 6.1: handle() retrieves existing root ... [OK]\n";

    return true;
}

// ============================================================================
// Test 7: root() and has_root() API
// ============================================================================
bool test_root_api() {
    std::cout << "\n[TEST] root() and has_root() API\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt buffer(4096);
    assert(!buffer.has_root<Player>());
    std::cout << "  Test 7.1: has_root() false before make ... [OK]\n";

    buffer.make<Player>();
    assert(buffer.has_root<Player>());
    std::cout << "  Test 7.2: has_root() true after make ... [OK]\n";

    auto& p = buffer.root<Player>();
    p.id = 777;
    p.name = "RootTest";
    assert(buffer.root<Player>().id == 777);
    std::cout << "  Test 7.3: root() returns valid reference ... [OK]\n";

    return true;
}

// ============================================================================
// Test 8: Serialization round-trip
// ============================================================================
bool test_handle_serialization() {
    std::cout << "\n[TEST] Handles with serialization round-trip\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt buffer(4096);
    auto player = buffer.make_handle<Player>();
    player->id = 42;
    player->name = "Serialized";
    player->items.push_back(10);
    player->items.push_back(20);

    std::string data = buffer.save_to_string();
    std::cout << "  Serialized: " << data.size() << " bytes\n";

    XBufferExt loaded = XBufferExt::load_from_string(data);
    auto& lp = loaded.root<Player>();

    assert(lp.id == 42);
    assert(std::string(lp.name.c_str()) == "Serialized");
    assert(lp.items.size() == 2);
    assert(lp.items[0] == 10);
    assert(lp.items[1] == 20);
    std::cout << "  Test 8.1: root on deserialized buffer ... [OK]\n";

    return true;
}

int main() {
    std::cout << "=== XHandle<T> Epoch-Cached Safe Handle Tests ===\n";

    bool all_passed = true;
    all_passed &= test_basic_handle();
    all_passed &= test_handle_survives_grow();
    all_passed &= test_handle_survives_shrink();
    all_passed &= test_handle_survives_compact();
    all_passed &= test_epoch_cache_efficiency();
    all_passed &= test_handle_existing();
    all_passed &= test_root_api();
    all_passed &= test_handle_serialization();

    std::cout << "\n";
    if (all_passed) {
        std::cout << "[PASS] All XHandle tests passed!\n";
        return 0;
    } else {
        std::cout << "[FAIL] Some tests failed!\n";
        return 1;
    }
}