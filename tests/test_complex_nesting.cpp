// ============================================================================
// Probe: Complex Nesting Stress Test
// Purpose: Verify zero-boilerplate works in deeply nested, complex scenarios
// ============================================================================

#include <cstdio>
#include <cassert>
#include <cstring>
#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// ============================================================================
// Complex Type Hierarchy — ALL pure aggregates, zero boilerplate
// ============================================================================

// Level 0: Leaf with string
struct Tag {
    int32_t id;
    XString label;
};

// Level 1: Has XVector of leaf
struct Skill {
    int32_t skill_id;
    XString name;
    XVector<int32_t> levels;       // primitive vector
    XVector<Tag> tags;             // vector of struct with string
};

// Level 2: Has XVector of Level 1
struct Character {
    int32_t char_id;
    XString name;
    float health;
    XVector<Skill> skills;         // vector of struct containing vectors
    XSet<int32_t> badges;
    XMap<int32_t, int32_t> stats;  // int->int map
};

// Level 3: Has XVector of Level 2 + XMap<XString, ...>
struct Guild {
    int32_t guild_id;
    XString guild_name;
    XVector<Character> members;    // vector of deeply nested structs
    XMap<XString, int32_t> ranks;  // string-keyed map
};

// Level 4: Root with XVector of Level 3
struct World {
    int32_t world_id;
    XString world_name;
    XVector<Guild> guilds;
    XSet<int32_t> active_worlds;
};

// ============================================================================
// Struct with only containers (no POD at all)
// ============================================================================
struct ContainerOnly {
    XString name;
    XVector<int32_t> data;
    XSet<int32_t> unique_ids;
};

// ============================================================================
// Struct with only POD (trivially copyable edge case check)
// ============================================================================
struct PodOnly {
    int32_t a;
    int32_t b;
    float c;
    double d;
    char e;
    bool f;
};

// ============================================================================
// Struct with many members (field count stress)
// ============================================================================
struct ManyFields {
    int32_t f1;
    int32_t f2;
    int32_t f3;
    int32_t f4;
    int32_t f5;
    int32_t f6;
    int32_t f7;
    int32_t f8;
    XString s1;
    XString s2;
    XVector<int32_t> v1;
    XVector<int32_t> v2;
    XSet<int32_t> set1;
    XMap<int32_t, int32_t> map1;
};

// ============================================================================
// Tests
// ============================================================================

bool test_deep_nesting() {
    fprintf(stderr, "\n[TEST] 4-level deep nesting: World > Guild > Character > Skill > Tag\n");

    XBuffer xbuf(1024 * 512);  // 512KB for deep nesting (generous for Linux overhead)
    auto* world = xbuf.make<World>();
    world->world_id = 1;
    world->world_name = "Azeroth";
    world->active_worlds.insert(1);
    world->active_worlds.insert(2);

    // Create 3 guilds
    for (int g = 0; g < 3; g++) {
        world->guilds.emplace_back();
        auto& guild = world->guilds.back();
        guild.guild_id = g + 1;
        std::string gname = "Guild_" + std::to_string(g);
        guild.guild_name = gname.c_str();
        guild.ranks["Leader"] = 1;
        guild.ranks["Officer"] = 2;

        // Each guild has 4 characters
        for (int c = 0; c < 4; c++) {
            guild.members.emplace_back();
            auto& ch = guild.members.back();
            ch.char_id = g * 100 + c;
            std::string cname = "Char_" + std::to_string(g) + "_" + std::to_string(c);
            ch.name = cname.c_str();
            ch.health = 100.0f - c * 5.0f;
            ch.badges.insert(c * 10);
            ch.badges.insert(c * 10 + 1);
            ch.stats[1] = 50 + c;  // strength
            ch.stats[2] = 30 + c;  // agility

            // Each character has 3 skills
            for (int s = 0; s < 3; s++) {
                ch.skills.emplace_back();
                auto& skill = ch.skills.back();
                skill.skill_id = g * 1000 + c * 100 + s;
                std::string sname = "Skill_" + std::to_string(s);
                skill.name = sname.c_str();
                skill.levels.push_back(1);
                skill.levels.push_back(5);
                skill.levels.push_back(10);

                // Each skill has 2 tags
                for (int t = 0; t < 2; t++) {
                    skill.tags.emplace_back();
                    auto& tag = skill.tags.back();
                    tag.id = t;
                    std::string tname = (t == 0) ? "offensive" : "defensive";
                    tag.label = tname.c_str();
                }
            }
        }
    }

    // Verify structure
    assert(world->guilds.size() == 3);
    assert(world->guilds[0].guild_name == "Guild_0");
    assert(world->guilds[1].members.size() == 4);
    assert(world->guilds[2].members[3].char_id == 203);
    assert(world->guilds[0].members[0].skills.size() == 3);
    assert(world->guilds[1].members[2].skills[1].levels.size() == 3);
    assert(world->guilds[0].members[0].skills[0].tags.size() == 2);
    assert(world->guilds[0].members[0].skills[0].tags[1].label == "defensive");
    assert(world->guilds[2].ranks.size() == 2);
    assert(world->active_worlds.size() == 2);

    fprintf(stderr, "  Structure verified (3 guilds × 4 chars × 3 skills × 2 tags) [OK]\n");

    // Persistence test
    auto saved = xbuf.save();
    auto loaded = XBuffer::load(saved);
    auto& w = loaded.root<World>();

    assert(w.world_name == "Azeroth");
    assert(w.guilds.size() == 3);
    assert(w.guilds[1].guild_name == "Guild_1");
    assert(w.guilds[0].members[2].name == "Char_0_2");
    assert(w.guilds[2].members[1].skills[2].name == "Skill_2");
    assert(w.guilds[1].members[3].skills[0].tags[0].label == "offensive");
    assert(w.guilds[0].members[0].health == 100.0f);
    assert(w.guilds[0].members[0].badges.size() == 2);
    assert(w.guilds[0].members[0].stats[1] == 50);

    fprintf(stderr, "  Persistence round-trip verified [OK]\n");
    return true;
}

bool test_vector_realloc_deep() {
    fprintf(stderr, "\n[TEST] Heavy reallocation with nested structs\n");

    XBuffer xbuf(1024 * 512);  // 512KB
    auto* world = xbuf.make<World>();
    world->world_id = 42;
    world->world_name = "StressWorld";

    // Single guild, push many characters to trigger reallocations
    world->guilds.emplace_back();
    auto& guild = world->guilds[0];
    guild.guild_id = 1;
    guild.guild_name = "StressGuild";

    for (int i = 0; i < 50; i++) {
        guild.members.emplace_back();
        auto& ch = guild.members.back();
        ch.char_id = i;
        std::string name = "Warrior_" + std::to_string(i);
        ch.name = name.c_str();
        ch.health = 100.0f;

        // Each character gets skills (triggers nested realloc)
        for (int s = 0; s < 5; s++) {
            ch.skills.emplace_back();
            auto& skill = ch.skills.back();
            skill.skill_id = i * 100 + s;
            std::string sn = "S" + std::to_string(s);
            skill.name = sn.c_str();
            for (int l = 0; l < 3; l++) {
                skill.levels.push_back(l + 1);
            }
            // Add tags
            skill.tags.emplace_back();
            skill.tags.back().id = 0;
            skill.tags.back().label = "tag";
        }
    }

    // Verify ALL 50 characters survived reallocation
    assert(guild.members.size() == 50);
    for (int i = 0; i < 50; i++) {
        std::string expected = "Warrior_" + std::to_string(i);
        assert(guild.members[i].name == expected.c_str());
        assert(guild.members[i].char_id == i);
        assert(guild.members[i].skills.size() == 5);
        assert(guild.members[i].skills[0].levels.size() == 3);
        assert(guild.members[i].skills[0].tags.size() == 1);
        assert(guild.members[i].skills[0].tags[0].label == "tag");
    }

    fprintf(stderr, "  50 characters × 5 skills, all survived realloc [OK]\n");
    return true;
}

bool test_compaction_deep() {
    fprintf(stderr, "\n[TEST] Compaction of deeply nested pure aggregates\n");

    XBuffer xbuf(1024 * 64);
    auto* world = xbuf.make<World>();
    world->world_id = 7;
    world->world_name = "CompactWorld";

    world->guilds.emplace_back();
    auto& guild = world->guilds[0];
    guild.guild_id = 1;
    guild.guild_name = "TestGuild";
    guild.ranks["Leader"] = 1;

    for (int i = 0; i < 10; i++) {
        guild.members.emplace_back();
        guild.members.back().char_id = i;
        std::string n = "C" + std::to_string(i);
        guild.members.back().name = n.c_str();
        guild.members.back().skills.emplace_back();
        guild.members.back().skills[0].skill_id = i;
        guild.members.back().skills[0].name = "BasicAttack";
        guild.members.back().skills[0].levels.push_back(1);
    }

    // Create fragmentation
    guild.members.pop_back();
    guild.members.pop_back();
    guild.members.pop_back();

    auto before = xbuf.stats();

    // Compact
    XBuffer compacted = XCompactor::compact<World>(xbuf);
    auto after = compacted.stats();

    assert(after.total_size <= before.total_size);

    auto& w = compacted.root<World>();
    assert(w.world_name == "CompactWorld");
    assert(w.guilds.size() == 1);
    assert(w.guilds[0].members.size() == 7);
    assert(w.guilds[0].members[0].name == "C0");
    assert(w.guilds[0].members[6].name == "C6");
    assert(w.guilds[0].members[0].skills[0].name == "BasicAttack");
    assert(w.guilds[0].ranks.size() == 1);

    fprintf(stderr, "  Compacted %zu → %zu bytes, data verified [OK]\n",
            before.total_size, after.total_size);
    return true;
}

bool test_container_only_struct() {
    fprintf(stderr, "\n[TEST] Struct with only container members (no POD)\n");

    XBuffer xbuf(8192);
    auto* root = xbuf.make<ContainerOnly>();
    root->name = "ContainerStruct";
    root->data.push_back(1);
    root->data.push_back(2);
    root->data.push_back(3);
    root->unique_ids.insert(100);
    root->unique_ids.insert(200);

    assert(root->name == "ContainerStruct");
    assert(root->data.size() == 3);
    assert(root->unique_ids.size() == 2);

    auto saved = xbuf.save();
    auto loaded = XBuffer::load(saved);
    auto& l = loaded.root<ContainerOnly>();
    assert(l.name == "ContainerStruct");
    assert(l.data[2] == 3);
    assert(l.unique_ids.size() == 2);

    fprintf(stderr, "  ContainerOnly struct + persistence [OK]\n");
    return true;
}

bool test_pod_only_struct() {
    fprintf(stderr, "\n[TEST] Struct with only POD members (trivially copyable)\n");

    XBuffer xbuf(4096);
    auto* p = xbuf.make<PodOnly>();
    p->a = 1; p->b = 2; p->c = 3.14f; p->d = 2.718; p->e = 'X'; p->f = true;

    auto saved = xbuf.save();
    auto loaded = XBuffer::load(saved);
    auto& l = loaded.root<PodOnly>();
    assert(l.a == 1 && l.b == 2);
    assert(l.c == 3.14f && l.d == 2.718);
    assert(l.e == 'X' && l.f == true);

    fprintf(stderr, "  PodOnly struct + persistence [OK]\n");
    return true;
}

// test_many_fields removed: covered by test_field_limit_fix.cpp

bool test_vector_of_container_only() {
    fprintf(stderr, "\n[TEST] XVector<ContainerOnly> — struct with no POD as element\n");

    XBuffer xbuf(32768);

    // Use a root that holds a vector of ContainerOnly
    struct Root {
        XVector<ContainerOnly> items;
    };

    auto* r = xbuf.make<Root>();

    for (int i = 0; i < 20; i++) {
        r->items.emplace_back();
        auto& item = r->items.back();
        std::string n = "Item_" + std::to_string(i);
        item.name = n.c_str();
        for (int j = 0; j < i + 1; j++) {
            item.data.push_back(j * 10);
        }
        item.unique_ids.insert(i);
    }

    assert(r->items.size() == 20);
    for (int i = 0; i < 20; i++) {
        std::string expected = "Item_" + std::to_string(i);
        assert(r->items[i].name == expected.c_str());
        assert(r->items[i].data.size() == static_cast<size_t>(i + 1));
        assert(r->items[i].unique_ids.size() == 1);
    }

    fprintf(stderr, "  20 ContainerOnly elements with realloc [OK]\n");
    return true;
}

bool test_xhandle_deep_nesting() {
    fprintf(stderr, "\n[TEST] XHandle with deep nesting + grow()\n");

    XBuffer xbuf(65536);
    auto h = xbuf.make_handle<World>();
    h->world_id = 99;
    h->world_name = "HandleWorld";

    h->guilds.emplace_back();
    h->guilds[0].guild_id = 1;
    h->guilds[0].guild_name = "HandleGuild";
    h->guilds[0].members.emplace_back();
    h->guilds[0].members[0].char_id = 42;
    h->guilds[0].members[0].name = "HandleHero";
    h->guilds[0].members[0].skills.emplace_back();
    h->guilds[0].members[0].skills[0].name = "Fireball";

    // Grow buffer — all raw pointers invalidated
    xbuf.grow(65536);

    // XHandle should still work
    assert(h->world_name == "HandleWorld");
    assert(h->guilds[0].guild_name == "HandleGuild");
    assert(h->guilds[0].members[0].name == "HandleHero");
    assert(h->guilds[0].members[0].skills[0].name == "Fireball");

    fprintf(stderr, "  XHandle + grow() on 4-level nesting [OK]\n");
    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    fprintf(stderr, "===================================================\n");
    fprintf(stderr, "  Complex Nesting Stress Test\n");
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

    run("deep_nesting", test_deep_nesting);
    run("vector_realloc_deep", test_vector_realloc_deep);
    run("compaction_deep", test_compaction_deep);
    run("container_only", test_container_only_struct);
    run("pod_only", test_pod_only_struct);
    run("vector_of_container_only", test_vector_of_container_only);
    run("xhandle_deep", test_xhandle_deep_nesting);

    fprintf(stderr, "\n===================================================\n");
    if (all_pass) {
        fprintf(stderr, "  ALL COMPLEX NESTING TESTS PASSED\n");
    } else {
        fprintf(stderr, "  SOME TESTS FAILED\n");
    }
    fprintf(stderr, "===================================================\n");

    return all_pass ? 0 : 1;
}
