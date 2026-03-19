// ============================================================================
// Test: Inheritance Support
// Purpose: Validate that non-virtual inheritance (single, multiple, multi-level)
//          works correctly with XBuffer make/read/compact cycles.
//          Also validates that virtual inheritance and virtual functions are
//          correctly rejected at compile time.
// ============================================================================

#include <iostream>
#include <cassert>
#include <cstring>
#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// ============================================================================
// Test Data Structures
// ============================================================================

// --- Single Inheritance ---
struct Entity {
    int32_t id{0};
    XString name;
};

struct Player : Entity {
    int32_t level{0};
    XVector<int32_t> items;
};

// --- Multi-level Inheritance (3 levels) ---
struct Character : Entity {
    int32_t hp{0};
    int32_t mp{0};
};

struct Warrior : Character {
    int32_t strength{0};
    XVector<XString> skills;
};

// --- Multiple Inheritance (mixin pattern) ---
struct HasId {
    int32_t id{0};
};

struct HasName {
    XString name;
};

struct HasLevel {
    int32_t level{0};
};

struct MixinPlayer : HasId, HasName, HasLevel {
    XVector<int32_t> inventory;
};

// --- Mixed: inheritance + composition ---
struct Stats {
    int32_t attack{0};
    int32_t defense{0};
};

struct FullCharacter : Entity {
    Stats stats;                    // composition
    XVector<XString> abilities;     // container member
};

// ============================================================================
// Test Functions
// ============================================================================

bool test_single_inheritance() {
    std::cout << "\n[TEST] Single Inheritance (Entity -> Player)\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(4096);
    auto* p = xbuf.make<Player>();

    // Write via inherited members
    p->id = 42;
    p->name = "Alice";
    // Write via own members
    p->level = 10;
    p->items.push_back(101);
    p->items.push_back(202);
    p->items.push_back(303);

    // Verify
    assert(p->id == 42);
    assert(std::string(p->name.c_str()) == "Alice");
    assert(p->level == 10);
    assert(p->items.size() == 3);
    assert(p->items[0] == 101);
    assert(p->items[1] == 202);
    assert(p->items[2] == 303);

    std::cout << "  Player id=" << p->id
              << " name=" << p->name.c_str()
              << " level=" << p->level
              << " items=" << p->items.size() << "\n";
    std::cout << "  Test: Single inheritance... [OK]\n";
    return true;
}

bool test_multi_level_inheritance() {
    std::cout << "\n[TEST] Multi-level Inheritance (Entity -> Character -> Warrior)\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(8192);
    auto* w = xbuf.make<Warrior>();

    // Entity level
    w->id = 7;
    w->name = "Conan";
    // Character level
    w->hp = 500;
    w->mp = 50;
    // Warrior level
    w->strength = 99;
    w->skills.push_back(XString("Cleave", xbuf.get_segment_manager()));
    w->skills.push_back(XString("Shield Bash", xbuf.get_segment_manager()));

    // Verify all levels
    assert(w->id == 7);
    assert(std::string(w->name.c_str()) == "Conan");
    assert(w->hp == 500);
    assert(w->mp == 50);
    assert(w->strength == 99);
    assert(w->skills.size() == 2);
    assert(std::string(w->skills[0].c_str()) == "Cleave");
    assert(std::string(w->skills[1].c_str()) == "Shield Bash");

    std::cout << "  Warrior id=" << w->id
              << " name=" << w->name.c_str()
              << " hp=" << w->hp << " mp=" << w->mp
              << " str=" << w->strength
              << " skills=" << w->skills.size() << "\n";
    std::cout << "  Test: Multi-level inheritance... [OK]\n";
    return true;
}

bool test_multiple_inheritance() {
    std::cout << "\n[TEST] Multiple Inheritance (HasId + HasName + HasLevel -> MixinPlayer)\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(4096);
    auto* p = xbuf.make<MixinPlayer>();

    p->id = 100;
    p->name = "MixinBob";
    p->level = 55;
    p->inventory.push_back(1);
    p->inventory.push_back(2);
    p->inventory.push_back(3);

    assert(p->id == 100);
    assert(std::string(p->name.c_str()) == "MixinBob");
    assert(p->level == 55);
    assert(p->inventory.size() == 3);

    std::cout << "  MixinPlayer id=" << p->id
              << " name=" << p->name.c_str()
              << " level=" << p->level
              << " inventory=" << p->inventory.size() << "\n";
    std::cout << "  Test: Multiple inheritance... [OK]\n";
    return true;
}

bool test_mixed_composition_inheritance() {
    std::cout << "\n[TEST] Mixed Composition + Inheritance (Entity -> FullCharacter with Stats)\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(8192);
    auto* fc = xbuf.make<FullCharacter>();

    // Inherited from Entity
    fc->id = 999;
    fc->name = "HybridHero";
    // Composed Stats
    fc->stats.attack = 80;
    fc->stats.defense = 60;
    // Container member
    fc->abilities.push_back(XString("Fireball", xbuf.get_segment_manager()));
    fc->abilities.push_back(XString("Heal", xbuf.get_segment_manager()));

    assert(fc->id == 999);
    assert(std::string(fc->name.c_str()) == "HybridHero");
    assert(fc->stats.attack == 80);
    assert(fc->stats.defense == 60);
    assert(fc->abilities.size() == 2);

    std::cout << "  FullCharacter id=" << fc->id
              << " name=" << fc->name.c_str()
              << " atk=" << fc->stats.attack
              << " def=" << fc->stats.defense
              << " abilities=" << fc->abilities.size() << "\n";
    std::cout << "  Test: Mixed composition + inheritance... [OK]\n";
    return true;
}

bool test_inheritance_compaction() {
    std::cout << "\n[TEST] Inheritance + Compaction\n";
    std::cout << std::string(50, '-') << "\n";

    // Create with extra space
    XBuffer xbuf(16384);
    auto* w = xbuf.make<Warrior>();

    w->id = 42;
    w->name = "CompactWarrior";
    w->hp = 300;
    w->mp = 100;
    w->strength = 75;
    for (int i = 0; i < 5; i++) {
        auto skill_name = std::string("Skill_") + std::to_string(i);
        w->skills.push_back(XString(skill_name.c_str(), xbuf.get_segment_manager()));
    }

    auto stats_before = memory_stats(xbuf);
    std::cout << "  Before compact: used=" << stats_before.used_size
              << " total=" << stats_before.total_size << "\n";

    // Compact
    auto compacted = XCompactor::compact<Warrior>(xbuf);
    auto& w2 = compacted.root<Warrior>();

    auto stats_after = memory_stats(compacted);
    std::cout << "  After compact:  used=" << stats_after.used_size
              << " total=" << stats_after.total_size << "\n";

    // Verify all data survived compaction
    assert(w2.id == 42);
    assert(std::string(w2.name.c_str()) == "CompactWarrior");
    assert(w2.hp == 300);
    assert(w2.mp == 100);
    assert(w2.strength == 75);
    assert(w2.skills.size() == 5);
    for (int i = 0; i < 5; i++) {
        auto expected = std::string("Skill_") + std::to_string(i);
        assert(std::string(w2.skills[i].c_str()) == expected);
    }

    assert(stats_after.total_size <= stats_before.total_size);

    std::cout << "  Test: Compaction preserved all inherited data... [OK]\n";
    return true;
}

bool test_inheritance_in_vector() {
    std::cout << "\n[TEST] Inherited types inside XVector\n";
    std::cout << std::string(50, '-') << "\n";

    // Root struct that holds a vector of inherited types
    struct Team {
        XString team_name;
        XVector<Player> members;
    };

    XBuffer xbuf(16384);
    auto* team = xbuf.make<Team>();
    team->team_name = "AlphaSquad";

    // Add players via emplace_back
    for (int i = 0; i < 5; i++) {
        team->members.emplace_back();
        auto& p = team->members.back();
        p.id = i + 1;
        auto name_str = std::string("Player_") + std::to_string(i);
        p.name = name_str.c_str();
        p.level = (i + 1) * 10;
        p.items.push_back(i * 100);
        p.items.push_back(i * 100 + 1);
    }

    // Verify
    assert(team->members.size() == 5);
    for (int i = 0; i < 5; i++) {
        auto& p = team->members[i];
        assert(p.id == i + 1);
        auto expected_name = std::string("Player_") + std::to_string(i);
        assert(std::string(p.name.c_str()) == expected_name);
        assert(p.level == (i + 1) * 10);
        assert(p.items.size() == 2);
        assert(p.items[0] == i * 100);
        assert(p.items[1] == i * 100 + 1);
    }

    std::cout << "  Team: " << team->team_name.c_str()
              << " with " << team->members.size() << " players\n";
    std::cout << "  Test: Inherited types in XVector... [OK]\n";
    return true;
}

bool test_multiple_inheritance_compaction() {
    std::cout << "\n[TEST] Multiple Inheritance + Compaction\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(8192);
    auto* p = xbuf.make<MixinPlayer>();

    p->id = 77;
    p->name = "CompactMixin";
    p->level = 30;
    for (int i = 0; i < 10; i++) {
        p->inventory.push_back(i * 10);
    }

    auto compacted = XCompactor::compact<MixinPlayer>(xbuf);
    auto& p2 = compacted.root<MixinPlayer>();

    assert(p2.id == 77);
    assert(std::string(p2.name.c_str()) == "CompactMixin");
    assert(p2.level == 30);
    assert(p2.inventory.size() == 10);
    for (int i = 0; i < 10; i++) {
        assert(p2.inventory[i] == i * 10);
    }

    std::cout << "  Test: Multiple inheritance compaction... [OK]\n";
    return true;
}

bool test_safety_validation() {
    std::cout << "\n[TEST] Safety Validation (compile-time checks)\n";
    std::cout << std::string(50, '-') << "\n";

    // These should all be SAFE
    static_assert(is_xbuffer_safe<Entity>::value,      "Entity should be safe");
    static_assert(is_xbuffer_safe<Player>::value,       "Player (single inherit) should be safe");
    static_assert(is_xbuffer_safe<Character>::value,    "Character should be safe");
    static_assert(is_xbuffer_safe<Warrior>::value,      "Warrior (multi-level) should be safe");
    static_assert(is_xbuffer_safe<MixinPlayer>::value,  "MixinPlayer (multiple inherit) should be safe");
    static_assert(is_xbuffer_safe<FullCharacter>::value, "FullCharacter (mixed) should be safe");
    static_assert(is_xbuffer_safe<Stats>::value,        "Stats (plain struct) should be safe");

    // Virtual functions should be UNSAFE
    struct HasVirtual { virtual void foo() {} int x; };
    static_assert(!is_xbuffer_safe<HasVirtual>::value,
                  "HasVirtual should be unsafe");

    // Types inheriting from polymorphic should be UNSAFE
    struct DerivedVirtual : HasVirtual { int y; };
    static_assert(!is_xbuffer_safe<DerivedVirtual>::value,
                  "DerivedVirtual should be unsafe");

    // Struct with unsafe members in base should be UNSAFE
    struct BadBase { std::string name; };
    struct DerivedBad : BadBase { int x; };
    static_assert(!is_xbuffer_safe<DerivedBad>::value,
                  "DerivedBad should be unsafe (base has std::string)");

    std::cout << "  static_assert: Entity safe... [OK]\n";
    std::cout << "  static_assert: Player safe... [OK]\n";
    std::cout << "  static_assert: Warrior safe... [OK]\n";
    std::cout << "  static_assert: MixinPlayer safe... [OK]\n";
    std::cout << "  static_assert: FullCharacter safe... [OK]\n";
    std::cout << "  static_assert: HasVirtual unsafe... [OK]\n";
    std::cout << "  static_assert: DerivedVirtual unsafe... [OK]\n";
    std::cout << "  static_assert: DerivedBad unsafe... [OK]\n";
    std::cout << "  Test: Safety validation... [OK]\n";
    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "============================================\n";
    std::cout << "  XOffsetDatastructure — Inheritance Tests  \n";
    std::cout << "============================================\n";

    bool all_pass = true;
    all_pass &= test_single_inheritance();
    all_pass &= test_multi_level_inheritance();
    all_pass &= test_multiple_inheritance();
    all_pass &= test_mixed_composition_inheritance();
    all_pass &= test_inheritance_compaction();
    all_pass &= test_inheritance_in_vector();
    all_pass &= test_multiple_inheritance_compaction();
    all_pass &= test_safety_validation();

    std::cout << "\n============================================\n";
    if (all_pass) {
        std::cout << "[PASS] All inheritance tests passed!\n";
    } else {
        std::cout << "[FAIL] Some tests failed!\n";
    }
    std::cout << "============================================\n";

    return all_pass ? 0 : 1;
}
