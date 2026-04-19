// ============================================================================
// Test: Zero-Boilerplate Type Definitions
// Verifies that pure aggregate types (no constructors, no macros) work with
// XBuffer via C++26 reflection — both as root objects and vector elements.
// ============================================================================

#include <cstdio>
#include <cassert>
#include <string>
#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// ============================================================================
// Pure aggregate types — NO constructors, NO macros
// ============================================================================

struct PodData { int32_t x; int32_t y; float z; double w; };

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

// Traditional type — backward compatibility
struct LegacyPlayer {
    template <typename Allocator>
    LegacyPlayer(Allocator a) : name(a), items(a) {}
    int32_t id{0};
    int32_t level{0};
    XString name;
    XVector<int32_t> items;
};

// Vector element types — also NO constructors
struct Item { int32_t id; int32_t quantity; XString name; };

struct PlayerEntry {
    int32_t id;
    int32_t level;
    XString name;
    XVector<int32_t> scores;
};

struct Inventory { int32_t owner_id; XVector<Item> items; };
struct Leaderboard { XString title; XVector<PlayerEntry> players; };

struct TeamMember { int32_t id; XString role; XVector<int32_t> stats; };
struct Team { XString team_name; XVector<TeamMember> members; };

// ============================================================================
// Root-level tests
// ============================================================================

bool test_pod() {
    fprintf(stderr, "\n[TEST] POD zero-boilerplate\n");
    XBuffer xbuf(4096);
    auto* obj = xbuf.make<PodData>();
    assert(obj->x == 0 && obj->y == 0);
    obj->x = 42; obj->y = -1; obj->z = 3.14f; obj->w = 2.718;

    auto loaded = XBuffer::load_unverified(xbuf.save());
    auto& l = loaded.unsafe_root<PodData>();
    assert(l.x == 42 && l.z == 3.14f && l.w == 2.718);
    fprintf(stderr, "  [OK]\n");
    return true;
}

bool test_simple_player() {
    fprintf(stderr, "\n[TEST] SimplePlayer (XString + XVector)\n");
    XBuffer xbuf(4096);
    auto* p = xbuf.make<SimplePlayer>();
    p->id = 1; p->level = 50; p->name = "Alice";
    p->scores.push_back(100); p->scores.push_back(200); p->scores.push_back(300);
    assert(p->name == "Alice" && p->scores.size() == 3);

    auto loaded = XBuffer::load_unverified(xbuf.save());
    auto& l = loaded.unsafe_root<SimplePlayer>();
    assert(l.id == 1 && l.name == "Alice" && l.scores[1] == 200);
    fprintf(stderr, "  [OK]\n");
    return true;
}

bool test_game_state() {
    fprintf(stderr, "\n[TEST] GameState (all container types)\n");
    XBuffer xbuf(8192);
    auto* g = xbuf.make<GameState>();
    g->round = 5; g->turn = 2; g->title = "Championship";
    for (int i = 0; i < 10; i++) g->player_ids.push_back(i * 100);
    g->score_map[1] = 500; g->score_map[2] = 300;
    g->active_set.insert(1); g->active_set.insert(5);

    auto loaded = XBuffer::load_unverified(xbuf.save());
    auto& l = loaded.unsafe_root<GameState>();
    assert(l.round == 5 && l.title == "Championship");
    assert(l.player_ids.size() == 10 && l.score_map.size() == 2 && l.active_set.size() == 2);
    fprintf(stderr, "  [OK]\n");
    return true;
}

bool test_legacy_compat() {
    fprintf(stderr, "\n[TEST] Legacy constructor backward compat\n");
    XBuffer xbuf(4096);
    auto* obj = xbuf.make<LegacyPlayer>();
    obj->id = 7; obj->name = "Legacy"; obj->items.push_back(42);

    auto loaded = XBuffer::load_unverified(xbuf.save());
    auto& l = loaded.unsafe_root<LegacyPlayer>();
    assert(l.id == 7 && l.name == "Legacy" && l.items[0] == 42);
    fprintf(stderr, "  [OK]\n");
    return true;
}

bool test_grow() {
    fprintf(stderr, "\n[TEST] grow() with zero-boilerplate\n");
    XBuffer xbuf(4096);
    auto h = xbuf.make_handle<SimplePlayer>();
    h->id = 1; h->name = "GrowTest"; h->scores.push_back(100);
    assert(xbuf.grow(4096));
    assert(h->id == 1 && h->name == "GrowTest" && h->scores[0] == 100);
    fprintf(stderr, "  [OK]\n");
    return true;
}

// ============================================================================
// Vector element tests (Layer 2)
// ============================================================================

bool test_vector_simple() {
    fprintf(stderr, "\n[TEST] XVector<Item> — simple aggregate\n");
    XBuffer xbuf(8192);
    auto* inv = xbuf.make<Inventory>();
    inv->owner_id = 42;
    inv->items.emplace_back(); inv->items[0].id = 1; inv->items[0].quantity = 10; inv->items[0].name = "Sword";
    inv->items.emplace_back(); inv->items[1].id = 2; inv->items[1].quantity = 5;  inv->items[1].name = "Shield";
    assert(inv->items.size() == 2 && inv->items[0].name == "Sword");
    fprintf(stderr, "  [OK]\n");
    return true;
}

bool test_vector_reallocation() {
    fprintf(stderr, "\n[TEST] XVector<Item> reallocation — 50 elements\n");
    XBuffer xbuf(65536);
    auto* inv = xbuf.make<Inventory>();
    for (int i = 0; i < 50; i++) {
        inv->items.emplace_back();
        inv->items.back().id = i;
        inv->items.back().quantity = i * 10;
        std::string s = "Item_" + std::to_string(i);
        inv->items.back().name = s.c_str();
    }
    assert(inv->items.size() == 50);
    for (int i = 0; i < 50; i++) {
        assert(inv->items[i].id == i);
        std::string expected = "Item_" + std::to_string(i);
        assert(inv->items[i].name == expected.c_str());
    }
    fprintf(stderr, "  [OK]\n");
    return true;
}

bool test_vector_complex() {
    fprintf(stderr, "\n[TEST] XVector<PlayerEntry> — nested containers\n");
    XBuffer xbuf(65536);
    auto* lb = xbuf.make<Leaderboard>();
    lb->title = "World Championship";
    for (int i = 0; i < 20; i++) {
        lb->players.emplace_back();
        auto& p = lb->players.back();
        p.id = i + 1; p.level = (i + 1) * 5;
        std::string name = "Player_" + std::to_string(i);
        p.name = name.c_str();
        for (int j = 0; j < 5; j++) p.scores.push_back((i + 1) * 100 + j);
    }
    assert(lb->players.size() == 20);
    for (int i = 0; i < 20; i++) {
        assert(lb->players[i].scores.size() == 5);
        assert(lb->players[i].scores[0] == (i + 1) * 100);
    }
    fprintf(stderr, "  [OK]\n");
    return true;
}

bool test_vector_persistence() {
    fprintf(stderr, "\n[TEST] XVector<Item> persistence\n");
    XBuffer xbuf(8192);
    auto* inv = xbuf.make<Inventory>();
    inv->owner_id = 99;
    inv->items.emplace_back(); inv->items[0].id = 1; inv->items[0].name = "MagicWand";
    inv->items.emplace_back(); inv->items[1].id = 2; inv->items[1].name = "HealthPotion";

    auto loaded = XBuffer::load_unverified(xbuf.save());
    auto& l = loaded.unsafe_root<Inventory>();
    assert(l.owner_id == 99 && l.items.size() == 2);
    assert(l.items[0].name == "MagicWand" && l.items[1].name == "HealthPotion");
    fprintf(stderr, "  [OK]\n");
    return true;
}

bool test_deeply_nested() {
    fprintf(stderr, "\n[TEST] Deeply nested — Team > TeamMember > stats\n");
    XBuffer xbuf(65536);
    auto* team = xbuf.make<Team>();
    team->team_name = "AlphaSquad";
    for (int i = 0; i < 10; i++) {
        team->members.emplace_back();
        auto& m = team->members.back();
        m.id = i;
        m.role = (i % 2 == 0) ? "attacker" : "defender";
        for (int j = 0; j <= i; j++) m.stats.push_back(j * 10);
    }
    assert(team->members.size() == 10);

    auto loaded = XBuffer::load_unverified(xbuf.save());
    auto& lt = loaded.unsafe_root<Team>();
    assert(lt.team_name == "AlphaSquad" && lt.members.size() == 10);
    assert(lt.members[5].role == "defender" && lt.members[9].stats.size() == 10);
    fprintf(stderr, "  [OK]\n");
    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    fprintf(stderr, "=== Zero-Boilerplate Test Suite ===\n");
    bool all = true;
    auto run = [&](const char* name, auto fn) {
        try { if (!fn()) { all = false; fprintf(stderr, "  [FAIL] %s\n", name); } }
        catch (const std::exception& e) { fprintf(stderr, "  [FAIL] %s: %s\n", name, e.what()); all = false; }
    };

    run("pod", test_pod);
    run("simple_player", test_simple_player);
    run("game_state", test_game_state);
    run("legacy_compat", test_legacy_compat);
    run("grow", test_grow);
    run("vector_simple", test_vector_simple);
    run("vector_reallocation", test_vector_reallocation);
    run("vector_complex", test_vector_complex);
    run("vector_persistence", test_vector_persistence);
    run("deeply_nested", test_deeply_nested);

    fprintf(stderr, "\n%s\n", all ? "[PASS] All zero-boilerplate tests passed" : "[FAIL] Some tests failed");
    return all ? 0 : 1;
}
