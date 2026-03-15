// ============================================================================
// Test: Zero-Boilerplate Type Definitions
// Purpose: Verify that pure aggregate types (no constructors, no macros)
//          work seamlessly with XBuffer via C++26 reflection.
//
// This test validates the core zero-boilerplate feature:
//   struct MyData { int x; XString name; XVector<int> items; };
//   auto* p = xbuf.make<MyData>();  // Just works!
// ============================================================================

#include <cstdio>
#include <cassert>
#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// ============================================================================
// Zero-boilerplate test types — NO constructors, NO macros, NO typedefs!
// ============================================================================

struct PodData {
    int32_t x;
    int32_t y;
    float z;
    double w;
};

struct SimplePlayer {
    int32_t id;
    int32_t level;
    XString name;
    XVector<int32_t> scores;
};

struct GameState {
    int32_t round;
    int32_t turn;
    XString title;
    XVector<int32_t> player_ids;
    XMap<int32_t, int32_t> score_map;
    XSet<int32_t> active_set;
};

// ============================================================================
// Traditional type (with allocator ctor) — verify backward compatibility
// ============================================================================

struct LegacyPlayer {
    template <typename Allocator>
    LegacyPlayer(Allocator allocator) 
        : name(allocator), items(allocator) {}
    
    int32_t id{0};
    int32_t level{0};
    XString name;
    XVector<int32_t> items;
};

// ============================================================================
// Tests
// ============================================================================

bool test_pod_zero_boilerplate() {
    fprintf(stderr, "\n[TEST] POD zero-boilerplate\n");
    
    XBuffer xbuf(4096);
    auto* obj = xbuf.make<PodData>();
    
    // Members should be value-initialized (zero)
    assert(obj->x == 0);
    assert(obj->y == 0);
    assert(obj->z == 0.0f);
    assert(obj->w == 0.0);
    
    // Set values
    obj->x = 42;
    obj->y = -1;
    obj->z = 3.14f;
    obj->w = 2.718;
    
    // Persistence
    auto saved = xbuf.save();
    auto loaded = XBuffer::load(saved);
    auto& l = loaded.root<PodData>();
    assert(l.x == 42);
    assert(l.y == -1);
    assert(l.z == 3.14f);
    assert(l.w == 2.718);
    
    fprintf(stderr, "  [PASS] POD zero-boilerplate works!\n");
    return true;
}

bool test_simple_player_zero_boilerplate() {
    fprintf(stderr, "\n[TEST] SimplePlayer zero-boilerplate (XString + XVector)\n");
    
    XBuffer xbuf(4096);
    auto* p = xbuf.make<SimplePlayer>();
    
    // Check initial state
    assert(p->id == 0);
    assert(p->level == 0);
    assert(p->name.size() == 0);
    assert(p->scores.size() == 0);
    
    // Set values
    p->id = 1;
    p->level = 50;
    p->name = "Alice";
    p->scores.push_back(100);
    p->scores.push_back(200);
    p->scores.push_back(300);
    
    assert(p->name == "Alice");
    assert(p->scores.size() == 3);
    assert(p->scores[2] == 300);
    
    // Persistence
    auto saved = xbuf.save();
    auto loaded = XBuffer::load(saved);
    auto& l = loaded.root<SimplePlayer>();
    assert(l.id == 1);
    assert(l.level == 50);
    assert(l.name == "Alice");
    assert(l.scores.size() == 3);
    assert(l.scores[1] == 200);
    
    fprintf(stderr, "  [PASS] SimplePlayer zero-boilerplate works!\n");
    return true;
}

bool test_game_state_zero_boilerplate() {
    fprintf(stderr, "\n[TEST] GameState zero-boilerplate (XString + XVector + XMap + XSet)\n");
    
    XBuffer xbuf(8192);
    auto* g = xbuf.make<GameState>();
    
    g->round = 5;
    g->turn = 2;
    g->title = "Championship";
    
    for (int i = 0; i < 10; i++) {
        g->player_ids.push_back(i * 100);
    }
    
    g->score_map[1] = 500;
    g->score_map[2] = 300;
    g->score_map[3] = 700;
    
    g->active_set.insert(1);
    g->active_set.insert(2);
    g->active_set.insert(5);
    
    assert(g->title == "Championship");
    assert(g->player_ids.size() == 10);
    assert(g->score_map.size() == 3);
    assert(g->active_set.size() == 3);
    
    // Persistence
    auto saved = xbuf.save();
    auto loaded = XBuffer::load(saved);
    auto& l = loaded.root<GameState>();
    assert(l.round == 5);
    assert(l.title == "Championship");
    assert(l.player_ids.size() == 10);
    assert(l.player_ids[5] == 500);
    assert(l.score_map.size() == 3);
    assert(l.active_set.size() == 3);
    
    fprintf(stderr, "  [PASS] GameState zero-boilerplate works!\n");
    return true;
}

// test_has_root_and_handle, test_make_handle, test_duplicate_make_throws
// removed: covered by test_xhandle.cpp and test_error_paths.cpp

bool test_backward_compat_legacy() {
    fprintf(stderr, "\n[TEST] Backward compatibility with legacy constructor types\n");
    
    XBuffer xbuf(4096);
    auto* obj = xbuf.make<LegacyPlayer>();
    
    obj->id = 7;
    obj->name = "LegacyChar";
    obj->items.push_back(42);
    
    assert(obj->name == "LegacyChar");
    assert(obj->items.size() == 1);
    
    // Persistence
    auto saved = xbuf.save();
    auto loaded = XBuffer::load(saved);
    auto& l = loaded.root<LegacyPlayer>();
    assert(l.id == 7);
    assert(l.name == "LegacyChar");
    assert(l.items[0] == 42);
    
    fprintf(stderr, "  [PASS] Legacy types still work!\n");
    return true;
}

bool test_grow_after_make() {
    fprintf(stderr, "\n[TEST] grow() after make<T>() with zero-boilerplate\n");
    
    XBuffer xbuf(4096);
    auto h = xbuf.make_handle<SimplePlayer>();
    
    h->id = 1;
    h->name = "GrowTest";
    h->scores.push_back(100);
    
    // Grow the buffer
    bool grew = xbuf.grow(4096);
    assert(grew);
    
    // XHandle should re-find correctly
    assert(h->id == 1);
    assert(h->name == "GrowTest");
    assert(h->scores.size() == 1);
    assert(h->scores[0] == 100);
    
    fprintf(stderr, "  [PASS] grow() + XHandle re-find works!\n");
    return true;
}

int main() {
    fprintf(stderr, "===================================================\n");
    fprintf(stderr, "  Zero-Boilerplate Test Suite\n");
    fprintf(stderr, "===================================================\n");
    
    bool all_pass = true;
    
    auto run = [&](const char* name, auto fn) {
        try {
            if (!fn()) { all_pass = false; fprintf(stderr, "  [FAIL] %s\n", name); }
        } catch (const std::exception& e) {
            fprintf(stderr, "  [FAIL] %s: %s\n", name, e.what());
            all_pass = false;
        }
    };
    
    run("POD", test_pod_zero_boilerplate);
    run("SimplePlayer", test_simple_player_zero_boilerplate);
    run("GameState", test_game_state_zero_boilerplate);
    run("backward_compat", test_backward_compat_legacy);
    run("grow_after_make", test_grow_after_make);
    
    fprintf(stderr, "\n===================================================\n");
    if (all_pass) {
        fprintf(stderr, "  ALL TESTS PASSED\n");
    } else {
        fprintf(stderr, "  SOME TESTS FAILED\n");
    }
    fprintf(stderr, "===================================================\n");
    
    return all_pass ? 0 : 1;
}
