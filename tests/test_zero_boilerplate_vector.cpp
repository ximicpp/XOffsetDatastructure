// ============================================================================
// Test: Zero-Boilerplate Vector Elements (Layer 2)
// Purpose: Verify that pure aggregate types (no constructors, no macros)
//          work correctly as elements inside XVector, including reallocation,
//          move semantics, and persistence.
//
// This test validates the Layer 2 zero-boilerplate feature:
//   struct Inner { int id; XString name; XVector<int> items; };
//   struct Root  { XVector<Inner> things; };
//   auto* r = xbuf.make<Root>();
//   r->things.emplace_back();  // Just works — no allocator ctor needed!
// ============================================================================

#include <cstdio>
#include <cassert>
#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// ============================================================================
// Pure aggregate types — NO constructors, NO macros, NO typedefs!
// ============================================================================

/// Simple inner type with a container member
struct Item {
    int32_t id;
    int32_t quantity;
    XString name;
};

/// Inner type with multiple container types
struct PlayerEntry {
    int32_t id;
    int32_t level;
    XString name;
    XVector<int32_t> scores;
};

/// Root type that holds a vector of pure aggregates
struct Inventory {
    int32_t owner_id;
    XVector<Item> items;
};

/// Root type with vector of complex aggregates
struct Leaderboard {
    XString title;
    XVector<PlayerEntry> players;
};

/// Deeply nested: root → vector<Outer> where Outer contains XVector
struct TeamMember {
    int32_t id;
    XString role;
    XVector<int32_t> stats;
};

struct Team {
    XString team_name;
    XVector<TeamMember> members;
};

// ============================================================================
// Tests
// ============================================================================

bool test_vector_of_simple_aggregate() {
    fprintf(stderr, "\n[TEST] XVector<Item> — simple aggregate with XString\n");

    XBuffer xbuf(8192);
    auto* inv = xbuf.make<Inventory>();
    inv->owner_id = 42;

    // Default-construct elements via emplace_back (0 args)
    inv->items.emplace_back();
    inv->items[0].id = 1;
    inv->items[0].quantity = 10;
    inv->items[0].name = "Sword";

    inv->items.emplace_back();
    inv->items[1].id = 2;
    inv->items[1].quantity = 5;
    inv->items[1].name = "Shield";

    assert(inv->items.size() == 2);
    assert(inv->items[0].name == "Sword");
    assert(inv->items[1].quantity == 5);

    fprintf(stderr, "  emplace_back + field access [OK]\n");
    return true;
}

bool test_vector_reallocation() {
    fprintf(stderr, "\n[TEST] XVector<Item> reallocation — many elements\n");

    XBuffer xbuf(65536);
    auto* inv = xbuf.make<Inventory>();
    inv->owner_id = 1;

    // Push enough elements to trigger multiple reallocations
    for (int i = 0; i < 50; i++) {
        inv->items.emplace_back();
        inv->items.back().id = i;
        inv->items.back().quantity = i * 10;
        std::string s = "Item_" + std::to_string(i);
        inv->items.back().name = s.c_str();
    }

    assert(inv->items.size() == 50);

    // Verify ALL elements survived reallocation correctly
    for (int i = 0; i < 50; i++) {
        assert(inv->items[i].id == i);
        assert(inv->items[i].quantity == i * 10);
        std::string expected = "Item_" + std::to_string(i);
        assert(inv->items[i].name == expected.c_str());
    }

    fprintf(stderr, "  50 elements with reallocation [OK]\n");
    return true;
}

bool test_vector_of_complex_aggregate() {
    fprintf(stderr, "\n[TEST] XVector<PlayerEntry> — aggregate with XString + XVector\n");

    XBuffer xbuf(65536);
    auto* lb = xbuf.make<Leaderboard>();
    lb->title = "World Championship";

    for (int i = 0; i < 20; i++) {
        lb->players.emplace_back();
        auto& p = lb->players.back();
        p.id = i + 1;
        p.level = (i + 1) * 5;
        std::string name = "Player_" + std::to_string(i);
        p.name = name.c_str();
        for (int j = 0; j < 5; j++) {
            p.scores.push_back((i + 1) * 100 + j);
        }
    }

    assert(lb->players.size() == 20);
    assert(lb->title == "World Championship");

    // Verify nested containers survived reallocation
    for (int i = 0; i < 20; i++) {
        assert(lb->players[i].id == i + 1);
        assert(lb->players[i].scores.size() == 5);
        assert(lb->players[i].scores[0] == (i + 1) * 100);
        std::string expected = "Player_" + std::to_string(i);
        assert(lb->players[i].name == expected.c_str());
    }

    fprintf(stderr, "  20 complex elements with nested XVector [OK]\n");
    return true;
}

bool test_vector_persistence() {
    fprintf(stderr, "\n[TEST] XVector<Item> persistence — save/load\n");

    XBuffer xbuf(8192);
    auto* inv = xbuf.make<Inventory>();
    inv->owner_id = 99;

    inv->items.emplace_back();
    inv->items[0].id = 1;
    inv->items[0].quantity = 42;
    inv->items[0].name = "MagicWand";

    inv->items.emplace_back();
    inv->items[1].id = 2;
    inv->items[1].quantity = 7;
    inv->items[1].name = "HealthPotion";

    // Save and reload
    auto saved = xbuf.save();
    auto loaded = XBuffer::load(saved);
    auto& l = loaded.root<Inventory>();

    assert(l.owner_id == 99);
    assert(l.items.size() == 2);
    assert(l.items[0].id == 1);
    assert(l.items[0].name == "MagicWand");
    assert(l.items[1].quantity == 7);
    assert(l.items[1].name == "HealthPotion");

    fprintf(stderr, "  save/load round-trip [OK]\n");
    return true;
}

bool test_deeply_nested_vector() {
    fprintf(stderr, "\n[TEST] Deeply nested — XVector<TeamMember> inside Team\n");

    XBuffer xbuf(65536);
    auto* team = xbuf.make<Team>();
    team->team_name = "AlphaSquad";

    for (int i = 0; i < 10; i++) {
        team->members.emplace_back();
        auto& m = team->members.back();
        m.id = i;
        std::string role = (i % 2 == 0) ? "attacker" : "defender";
        m.role = role.c_str();
        for (int j = 0; j <= i; j++) {
            m.stats.push_back(j * 10);
        }
    }

    assert(team->members.size() == 10);
    assert(team->team_name == "AlphaSquad");

    // Verify nested vectors
    for (int i = 0; i < 10; i++) {
        assert(team->members[i].id == i);
        assert(team->members[i].stats.size() == static_cast<size_t>(i + 1));
        assert(team->members[i].stats[0] == 0);
    }

    // Persistence test
    auto saved = xbuf.save();
    auto loaded = XBuffer::load(saved);
    auto& lt = loaded.root<Team>();

    assert(lt.team_name == "AlphaSquad");
    assert(lt.members.size() == 10);
    assert(lt.members[5].role == "defender");
    assert(lt.members[9].stats.size() == 10);

    fprintf(stderr, "  deeply nested with persistence [OK]\n");
    return true;
}

bool test_vector_clear_and_reuse() {
    fprintf(stderr, "\n[TEST] XVector<Item> clear and reuse\n");

    XBuffer xbuf(16384);
    auto* inv = xbuf.make<Inventory>();

    // Fill
    for (int i = 0; i < 10; i++) {
        inv->items.emplace_back();
        inv->items.back().id = i;
        inv->items.back().name = "temp";
    }
    assert(inv->items.size() == 10);

    // Clear
    inv->items.clear();
    assert(inv->items.empty());

    // Refill
    for (int i = 0; i < 5; i++) {
        inv->items.emplace_back();
        inv->items.back().id = 100 + i;
        std::string s = "new_" + std::to_string(i);
        inv->items.back().name = s.c_str();
    }

    assert(inv->items.size() == 5);
    assert(inv->items[0].id == 100);
    assert(inv->items[4].name == "new_4");

    fprintf(stderr, "  clear + refill [OK]\n");
    return true;
}

bool test_vector_with_grow() {
    fprintf(stderr, "\n[TEST] XVector<Item> with buffer grow()\n");

    XBuffer xbuf(4096);
    auto h = xbuf.make_handle<Inventory>();
    h->owner_id = 1;

    h->items.emplace_back();
    h->items[0].id = 1;
    h->items[0].name = "BeforeGrow";

    // Grow the buffer (invalidates all raw pointers)
    bool grew = xbuf.grow(8192);
    assert(grew);

    // XHandle re-finds correctly
    assert(h->items.size() == 1);
    assert(h->items[0].id == 1);
    assert(h->items[0].name == "BeforeGrow");

    // Add more after grow
    h->items.emplace_back();
    h->items[1].id = 2;
    h->items[1].name = "AfterGrow";

    assert(h->items.size() == 2);
    assert(h->items[1].name == "AfterGrow");

    fprintf(stderr, "  buffer grow + XHandle [OK]\n");
    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    fprintf(stderr, "===================================================\n");
    fprintf(stderr, "  Zero-Boilerplate Vector Test Suite (Layer 2)\n");
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

    run("simple_aggregate", test_vector_of_simple_aggregate);
    run("reallocation", test_vector_reallocation);
    run("complex_aggregate", test_vector_of_complex_aggregate);
    run("persistence", test_vector_persistence);
    run("deeply_nested", test_deeply_nested_vector);
    run("clear_reuse", test_vector_clear_and_reuse);
    run("grow", test_vector_with_grow);

    fprintf(stderr, "\n===================================================\n");
    if (all_pass) {
        fprintf(stderr, "  ALL LAYER-2 TESTS PASSED\n");
    } else {
        fprintf(stderr, "  SOME TESTS FAILED\n");
    }
    fprintf(stderr, "===================================================\n");

    return all_pass ? 0 : 1;
}
