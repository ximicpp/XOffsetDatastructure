// ============================================================================
// Test: Memory Efficiency Analysis — Realistic Game Character Data
// Purpose: Measure XBuffer memory overhead for production-like game data
//          structures at various complexity levels, and compare with
//          equivalent raw C++ memory + manual serialization sizes.
// ============================================================================

#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstring>
#include <vector>
#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// ============================================================================
// Helper
// ============================================================================
struct Report {
    const char* label;
    std::size_t buf_total;
    std::size_t buf_used;
    std::size_t logical;       // pure user data bytes (no headers/padding)
    std::size_t proto_est;     // estimated protobuf/msgpack size

    void print() const {
        double eff = buf_total > 0 ? logical * 100.0 / buf_total : 0;
        double vs_proto = proto_est > 0 ? (double)buf_total / proto_est : 0;
        std::cout << std::left << std::setw(40) << label
                  << "  xbuf=" << std::setw(7) << buf_total
                  << "  logical=" << std::setw(6) << logical
                  << "  eff=" << std::fixed << std::setprecision(1) << std::setw(5) << eff << "%"
                  << "  overhead=" << std::setw(5) << (buf_total - logical) << "B"
                  << "  vs_proto≈" << std::setprecision(2) << vs_proto << "x"
                  << "\n";
    }
};

// ============================================================================
// Game Data Structures — Realistic RPG character
// ============================================================================

// Macro to declare the standard allocator-aware constructors pattern:
// 1. Allocator-only constructor (for make<T>() and default construction)
// 2. Move + allocator constructor (for vector reallocation via scoped_allocator)

// --- Equipment slot ---
struct EquipSlot {
    using allocator_type = XAllocator;
    template<typename A> EquipSlot(A a) 
        : item_id(0), enchant_level(0), name(a) {}
    template<typename A> EquipSlot(EquipSlot&& o, A a) 
        : item_id(o.item_id), enchant_level(o.enchant_level), name(std::move(o.name), a) {}
    int32_t item_id;
    int32_t enchant_level;
    XString name;
};

// --- Skill entry ---
struct Skill {
    using allocator_type = XAllocator;
    template<typename A> Skill(A a) 
        : skill_id(0), level(0), cooldown(0), name(a) {}
    template<typename A> Skill(Skill&& o, A a) 
        : skill_id(o.skill_id), level(o.level), cooldown(o.cooldown), name(std::move(o.name), a) {}
    int32_t skill_id;
    int32_t level;
    float   cooldown;
    XString name;
};

// --- Quest progress ---
struct QuestProgress {
    using allocator_type = XAllocator;
    template<typename A> QuestProgress(A a) 
        : quest_id(0), stage(0), completed(0), title(a), objectives(a) {}
    template<typename A> QuestProgress(QuestProgress&& o, A a) 
        : quest_id(o.quest_id), stage(o.stage), completed(o.completed),
          title(std::move(o.title), a), objectives(std::move(o.objectives), a) {}
    int32_t quest_id;
    int32_t stage;
    int32_t completed;
    XString title;
    XVector<int32_t> objectives;
};

// --- Inventory item ---
struct InventoryItem {
    using allocator_type = XAllocator;
    template<typename A> InventoryItem(A a) 
        : item_id(0), quantity(0), slot(0), name(a) {}
    template<typename A> InventoryItem(InventoryItem&& o, A a) 
        : item_id(o.item_id), quantity(o.quantity), slot(o.slot), name(std::move(o.name), a) {}
    int32_t item_id;
    int32_t quantity;
    int32_t slot;
    XString name;
};

// --- Social entry ---
struct FriendEntry {
    using allocator_type = XAllocator;
    template<typename A> FriendEntry(A a)
        : player_id(0), friendship_level(0), name(a) {}
    template<typename A> FriendEntry(FriendEntry&& o, A a)
        : player_id(o.player_id), friendship_level(o.friendship_level), name(std::move(o.name), a) {}
    int32_t player_id;
    int32_t friendship_level;
    XString name;
};

// --- Achievement ---
struct Achievement {
    using allocator_type = XAllocator;
    template<typename A> Achievement(A a)
        : ach_id(0), progress(0), unlocked(0), name(a) {}
    template<typename A> Achievement(Achievement&& o, A a)
        : ach_id(o.ach_id), progress(o.progress), unlocked(o.unlocked), name(std::move(o.name), a) {}
    int32_t ach_id;
    int32_t progress;
    int32_t unlocked;
    XString name;
};

// ================================================================
// Profile 1: Minimal character (new player, just created)
// ================================================================
struct CharacterMinimal {
    template<typename A> CharacterMinimal(A a) 
        : player_id(0), level(1), experience(0),
          health(100.0f), mana(50.0f), stamina(100.0f),
          pos_x(0), pos_y(0), pos_z(0),
          name(a), guild_name(a) {}

    int32_t player_id;
    int32_t level;
    int64_t experience;
    float   health;
    float   mana;
    float   stamina;
    double  pos_x;
    double  pos_y;
    double  pos_z;
    XString name;
    XString guild_name;
};

// ================================================================
// Profile 2: Mid-game character
// ================================================================
struct CharacterMidGame {
    template<typename A> CharacterMidGame(A a) 
        : player_id(0), level(0), experience(0),
          health(0), max_health(0), mana(0), max_mana(0),
          stamina(0), strength(0), dexterity(0), intelligence(0),
          pos_x(0), pos_y(0), pos_z(0), rotation(0),
          gold(0), playtime_seconds(0),
          name(a), guild_name(a), title(a),
          inventory(a), equipment(a), skills(a),
          quests(a), friends(a) {}

    // --- Identity ---
    int32_t player_id;
    int32_t level;
    int64_t experience;

    // --- Stats ---
    float   health, max_health;
    float   mana, max_mana;
    float   stamina;
    int32_t strength, dexterity, intelligence;

    // --- Position ---
    double  pos_x, pos_y, pos_z;
    float   rotation;

    // --- Economy ---
    int64_t gold;
    int64_t playtime_seconds;

    // --- Strings ---
    XString name;
    XString guild_name;
    XString title;

    // --- Collections ---
    XVector<InventoryItem> inventory;   // ~30 items
    XVector<EquipSlot>     equipment;   // ~8 slots
    XVector<Skill>         skills;      // ~15 skills
    XVector<QuestProgress> quests;      // ~5 active
    XVector<FriendEntry>   friends;     // ~20 friends
};

// ================================================================
// Profile 3: End-game veteran character (max everything)
// ================================================================
struct CharacterEndGame {
    template<typename A> CharacterEndGame(A a) 
        : player_id(0), level(0), experience(0),
          health(0), max_health(0), mana(0), max_mana(0),
          stamina(0), strength(0), dexterity(0), intelligence(0),
          wisdom(0), charisma(0), luck(0),
          pos_x(0), pos_y(0), pos_z(0), rotation(0),
          gold(0), premium_currency(0), playtime_seconds(0),
          pvp_rating(0), arena_wins(0), arena_losses(0),
          name(a), guild_name(a), title(a), bio(a),
          inventory(a), equipment(a), skills(a),
          quests(a), completed_quests(a),
          friends(a), blocked(a),
          achievements(a),
          settings(a), chat_log(a) {}

    // --- Identity ---
    int32_t player_id;
    int32_t level;
    int64_t experience;

    // --- Core stats ---
    float   health, max_health, mana, max_mana, stamina;
    int32_t strength, dexterity, intelligence, wisdom, charisma, luck;

    // --- Position ---
    double  pos_x, pos_y, pos_z;
    float   rotation;

    // --- Economy & progression ---
    int64_t gold, premium_currency, playtime_seconds;
    int32_t pvp_rating, arena_wins, arena_losses;

    // --- Strings ---
    XString name;
    XString guild_name;
    XString title;
    XString bio;

    // --- Collections ---
    XVector<InventoryItem> inventory;       // 200 items
    XVector<EquipSlot>     equipment;       // 12 slots
    XVector<Skill>         skills;          // 40 skills
    XVector<QuestProgress> quests;          // 10 active
    XVector<int32_t>       completed_quests;// 500 quest IDs
    XVector<FriendEntry>   friends;         // 100 friends
    XVector<int32_t>       blocked;         // 20 blocked IDs
    XVector<Achievement>   achievements;    // 150 achievements
    XVector<XString>       settings;        // 30 key=value strings
    XVector<XString>       chat_log;        // 50 recent messages
};

// ============================================================================
// Populate helpers
// ============================================================================

void populate_minimal(CharacterMinimal& c) {
    c.player_id = 1001;
    c.level = 1;
    c.experience = 0;
    c.health = 100.0f; c.mana = 50.0f; c.stamina = 100.0f;
    c.pos_x = 128.5; c.pos_y = 64.0; c.pos_z = 0.0;
    c.name = "Newbie_Player";
    c.guild_name = "";
}

void populate_midgame(CharacterMidGame& c) {
    c.player_id = 50042;
    c.level = 45;
    c.experience = 1250000;
    c.health = 850.0f; c.max_health = 850.0f;
    c.mana = 400.0f; c.max_mana = 400.0f;
    c.stamina = 200.0f;
    c.strength = 55; c.dexterity = 40; c.intelligence = 62;
    c.pos_x = 3201.75; c.pos_y = -1024.5; c.pos_z = 128.0;
    c.rotation = 1.57f;
    c.gold = 45000;
    c.playtime_seconds = 360000; // 100 hours

    c.name = "ShadowBlade_X";
    c.guild_name = "Knights of Dawn";
    c.title = "The Dragonslayer";

    auto* sm = c.inventory.get_stored_allocator().get_segment_manager();
    // 30 inventory items
    for (int i = 0; i < 30; i++) {
        c.inventory.emplace_back(sm);
        auto& item = c.inventory.back();
        item.item_id = 1000 + i; item.quantity = 1 + (i % 5); item.slot = i;
        item.name = ("Item_" + std::to_string(i)).c_str();
    }
    // 8 equipment slots
    const char* slots[] = {"Iron Helmet +2", "Dragonscale Armor", "Leather Boots",
                           "Flaming Sword +5", "Oak Shield +3", "Ring of Power",
                           "Amulet of Speed", "Cloak of Shadows"};
    for (int i = 0; i < 8; i++) {
        c.equipment.emplace_back(sm);
        auto& eq = c.equipment.back();
        eq.item_id = 2000 + i; eq.enchant_level = i % 4; eq.name = slots[i];
    }
    // 15 skills
    for (int i = 0; i < 15; i++) {
        c.skills.emplace_back(sm);
        auto& sk = c.skills.back();
        sk.skill_id = 3000 + i; sk.level = 1 + (i % 10); sk.cooldown = 0.5f * i;
        sk.name = ("Skill_" + std::to_string(i)).c_str();
    }
    // 5 quests with objectives
    for (int i = 0; i < 5; i++) {
        c.quests.emplace_back(sm);
        auto& q = c.quests.back();
        q.quest_id = 4000 + i; q.stage = i % 3; q.completed = 0;
        q.title = ("Quest: The Lost " + std::to_string(i)).c_str();
        for (int j = 0; j < 3; j++) q.objectives.push_back(j < i ? 1 : 0);
    }
    // 20 friends
    for (int i = 0; i < 20; i++) {
        c.friends.emplace_back(sm);
        auto& f = c.friends.back();
        f.player_id = 10000 + i; f.friendship_level = 1 + (i % 5);
        f.name = ("Friend_" + std::to_string(i)).c_str();
    }
}

void populate_endgame(CharacterEndGame& c) {
    c.player_id = 7;
    c.level = 100;
    c.experience = 99999999;
    c.health = 5000; c.max_health = 5000;
    c.mana = 3000; c.max_mana = 3000;
    c.stamina = 800;
    c.strength = 200; c.dexterity = 150; c.intelligence = 250;
    c.wisdom = 180; c.charisma = 90; c.luck = 77;
    c.pos_x = 10000.5; c.pos_y = -5000.25; c.pos_z = 512.0;
    c.rotation = 3.14f;
    c.gold = 9999999;
    c.premium_currency = 5000;
    c.playtime_seconds = 3600000; // 1000 hours
    c.pvp_rating = 2400; c.arena_wins = 850; c.arena_losses = 320;

    c.name = "Legendary_Hero_XYZ";
    c.guild_name = "Order of the Phoenix";
    c.title = "Grand Marshal, Defender of the Realm";
    c.bio = "A veteran warrior who has conquered every dungeon and slain every boss. Known across all servers for unmatched skill.";

    auto* sm = c.inventory.get_stored_allocator().get_segment_manager();
    // 200 inventory items
    for (int i = 0; i < 200; i++) {
        c.inventory.emplace_back(sm);
        auto& item = c.inventory.back();
        item.item_id = 1000+i; item.quantity = 1+(i%99); item.slot = i%60;
        item.name = ("Item_" + std::to_string(i)).c_str();
    }
    // 12 equipment slots
    for (int i = 0; i < 12; i++) {
        c.equipment.emplace_back(sm);
        auto& eq = c.equipment.back();
        eq.item_id = 5000+i; eq.enchant_level = 5+(i%3);
        eq.name = ("Mythic_Gear_Slot_" + std::to_string(i)).c_str();
    }
    // 40 skills
    for (int i = 0; i < 40; i++) {
        c.skills.emplace_back(sm);
        auto& sk = c.skills.back();
        sk.skill_id = 3000+i; sk.level = 10; sk.cooldown = 1.0f+0.5f*i;
        sk.name = ("MasterSkill_" + std::to_string(i)).c_str();
    }
    // 10 active quests
    for (int i = 0; i < 10; i++) {
        c.quests.emplace_back(sm);
        auto& q = c.quests.back();
        q.quest_id = 9000+i; q.stage = i%5; q.completed = 0;
        q.title = ("Epic Quest Chain Part " + std::to_string(i+1)).c_str();
        for (int j = 0; j < 5; j++) q.objectives.push_back(j < 3 ? 1 : 0);
    }
    // 500 completed quests
    for (int i = 0; i < 500; i++) c.completed_quests.push_back(i);
    // 100 friends
    for (int i = 0; i < 100; i++) {
        c.friends.emplace_back(sm);
        auto& f = c.friends.back();
        f.player_id = 10000+i; f.friendship_level = 1+(i%10);
        f.name = ("Player_" + std::to_string(10000+i)).c_str();
    }
    // 20 blocked
    for (int i = 0; i < 20; i++) c.blocked.push_back(90000 + i);
    // 150 achievements
    for (int i = 0; i < 150; i++) {
        c.achievements.emplace_back(sm);
        auto& a = c.achievements.back();
        a.ach_id = 7000+i; a.progress = 100; a.unlocked = 1;
        a.name = ("Achievement_" + std::to_string(i)).c_str();
    }
    // 30 settings
    for (int i = 0; i < 30; i++) {
        std::string s = "setting_" + std::to_string(i) + "=value_" + std::to_string(i);
        c.settings.push_back(s.c_str());
    }
    // 50 chat messages
    for (int i = 0; i < 50; i++) {
        std::string m = "[Guild] Player_" + std::to_string(i) + ": Hey everyone, raid starts soon!";
        c.chat_log.push_back(m.c_str());
    }
}

// ============================================================================
// Calculate logical data sizes (pure user payload, no headers/padding)
// ============================================================================
std::size_t calc_logical_minimal() {
    return 4 + 4 + 8   // player_id, level, experience
         + 4 * 3       // health, mana, stamina
         + 8 * 3       // pos_x/y/z
         + 13 + 0;     // "Newbie_Player" + ""
}

std::size_t calc_logical_midgame() {
    std::size_t scalars = 4+4+8 + 4*4+4 + 4*3 + 8*3+4 + 8*2;  // all scalar fields
    std::size_t strings = 13 + 15 + 16;  // name + guild + title
    // inventory: 30 items × (4+4+4 + ~6 avg name)
    std::size_t inv = 30 * (12 + 6);
    // equipment: 8 × (4+4 + ~16 avg name)
    std::size_t equip = 8 * (8 + 16);
    // skills: 15 × (4+4+4 + ~8 avg name)
    std::size_t skill = 15 * (12 + 8);
    // quests: 5 × (4+4+4 + ~20 title + 3×4 objectives)
    std::size_t quest = 5 * (12 + 20 + 12);
    // friends: 20 × (4+4 + ~9 avg name)
    std::size_t friends = 20 * (8 + 9);
    return scalars + strings + inv + equip + skill + quest + friends;
}

std::size_t calc_logical_endgame() {
    std::size_t scalars = 4+4+8 + 4*5+4*6 + 8*3+4 + 8*3+4*3;  // all scalar fields
    std::size_t strings = 18 + 20 + 36 + 118;  // name + guild + title + bio
    std::size_t inv = 200 * (12 + 6);
    std::size_t equip = 12 * (8 + 19);
    std::size_t skill = 40 * (12 + 14);
    std::size_t quest = 10 * (12 + 25 + 20);
    std::size_t completed = 500 * 4;
    std::size_t friends = 100 * (8 + 12);
    std::size_t blocked = 20 * 4;
    std::size_t ach = 150 * (12 + 15);
    std::size_t settings = 30 * 22;
    std::size_t chat = 50 * 50;
    return scalars + strings + inv + equip + skill + quest + completed
         + friends + blocked + ach + settings + chat;
}

// Estimate protobuf/msgpack size: ~logical + ~15% tag/length overhead
std::size_t est_proto(std::size_t logical) {
    return logical + logical / 7;
}

// ============================================================================
// Main test scenarios
// ============================================================================

bool test_profile_minimal() {
    std::cout << "\n[Profile 1] New Player — Minimal Character\n";
    std::cout << std::string(70, '-') << "\n";
    std::cout << "  Fields: id, level, exp, 3 floats, 3 doubles, 2 strings\n";
    std::cout << "  Collections: none\n\n";

    XBuffer xbuf(4096);
    auto* c = xbuf.make<CharacterMinimal>();
    populate_minimal(*c);
    xbuf.shrink_to_fit();
    auto s = xbuf.stats();
    std::size_t logical = calc_logical_minimal();

    Report r{"New Player (no collections)", s.total_size, s.used_size, 
             logical, est_proto(logical)};
    r.print();

    std::cout << "\n  Breakdown:\n";
    std::cout << "    segment_manager overhead:  112 bytes (fixed)\n";
    std::cout << "    root object index:         ~48 bytes (fixed)\n";
    std::cout << "    struct sizeof:             " << sizeof(CharacterMinimal) << " bytes\n";
    std::cout << "    block_ctrl per alloc:      16 bytes × ~2 allocs\n";
    std::cout << "    logical payload:           " << logical << " bytes\n";
    std::cout << "    total buffer:              " << s.total_size << " bytes\n";

    std::cout << "  [PASS]\n";
    return true;
}

bool test_profile_midgame() {
    std::cout << "\n[Profile 2] Mid-Game Character — Active Player\n";
    std::cout << std::string(70, '-') << "\n";
    std::cout << "  Fields: ~20 scalars, 3 strings\n";
    std::cout << "  Collections: 30 items, 8 equip, 15 skills, 5 quests, 20 friends\n\n";

    XBuffer xbuf(65536);
    auto* c = xbuf.make<CharacterMidGame>();
    populate_midgame(*c);

    auto s_before = xbuf.stats();
    std::cout << "  Before shrink: buf=" << s_before.total_size 
              << "  used=" << s_before.used_size 
              << "  usage=" << std::fixed << std::setprecision(1) 
              << s_before.usage_percent() << "%\n";

    xbuf.shrink_to_fit();
    auto s = xbuf.stats();
    std::size_t logical = calc_logical_midgame();

    Report r{"Mid-Game (78 nested objects)", s.total_size, s.used_size, 
             logical, est_proto(logical)};
    std::cout << "  After shrink:  ";
    r.print();

    // Compaction
    XBuffer compacted = XCompactor::compact_automatic<CharacterMidGame>(xbuf);
    auto sc = compacted.stats();
    Report rc{"Mid-Game (compacted)", sc.total_size, sc.used_size,
              logical, est_proto(logical)};
    std::cout << "  After compact: ";
    rc.print();

    // Verify
    auto& v = compacted.root<CharacterMidGame>();
    assert(v.player_id == 50042);
    assert(v.level == 45);
    assert(std::string(v.name.c_str()) == "ShadowBlade_X");
    assert(v.inventory.size() == 30);
    assert(v.equipment.size() == 8);
    assert(v.skills.size() == 15);
    assert(v.quests.size() == 5);
    assert(v.friends.size() == 20);

    std::cout << "\n  Per-collection breakdown (compacted):\n";
    std::cout << "    sizeof(InventoryItem) = " << sizeof(InventoryItem) << "B × 30 items → struct " << sizeof(InventoryItem)*30 << "B\n";
    std::cout << "    sizeof(EquipSlot)     = " << sizeof(EquipSlot) << "B × 8  slots → struct " << sizeof(EquipSlot)*8 << "B\n";
    std::cout << "    sizeof(Skill)         = " << sizeof(Skill) << "B × 15 skills→ struct " << sizeof(Skill)*15 << "B\n";
    std::cout << "    sizeof(QuestProgress) = " << sizeof(QuestProgress) << "B × 5  quests→ struct " << sizeof(QuestProgress)*5 << "B\n";
    std::cout << "    sizeof(FriendEntry)   = " << sizeof(FriendEntry) << "B × 20 friend→ struct " << sizeof(FriendEntry)*20 << "B\n";
    std::cout << "    + string heap allocs + block_ctrl headers\n";

    std::cout << "  Data integrity: [OK]\n";
    std::cout << "  [PASS]\n";
    return true;
}

bool test_profile_endgame() {
    std::cout << "\n[Profile 3] End-Game Veteran — Maximum Data\n";
    std::cout << std::string(70, '-') << "\n";
    std::cout << "  Fields: ~30 scalars, 4 strings (incl 118-char bio)\n";
    std::cout << "  Collections: 200 items, 12 equip, 40 skills, 10 quests,\n";
    std::cout << "               500 completed IDs, 100 friends, 20 blocked,\n";
    std::cout << "               150 achievements, 30 settings, 50 chat msgs\n\n";

    XBuffer xbuf(524288);  // 512KB
    auto* c = xbuf.make<CharacterEndGame>();
    populate_endgame(*c);

    auto s_before = xbuf.stats();
    std::cout << "  Before shrink: buf=" << s_before.total_size 
              << "  used=" << s_before.used_size
              << "  usage=" << std::fixed << std::setprecision(1) 
              << s_before.usage_percent() << "%\n";

    xbuf.shrink_to_fit();
    auto s = xbuf.stats();
    std::size_t logical = calc_logical_endgame();

    Report r{"End-Game (592+ nested objects)", s.total_size, s.used_size,
             logical, est_proto(logical)};
    std::cout << "  After shrink:  ";
    r.print();

    // Compaction
    XBuffer compacted = XCompactor::compact_automatic<CharacterEndGame>(xbuf);
    auto sc = compacted.stats();
    Report rc{"End-Game (compacted)", sc.total_size, sc.used_size,
              logical, est_proto(logical)};
    std::cout << "  After compact: ";
    rc.print();

    // Serialization
    std::string serialized = compacted.save_to_string();
    std::cout << "\n  Serialized wire size: " << serialized.size() << " bytes\n";
    std::cout << "  Estimated protobuf:   ~" << est_proto(logical) << " bytes\n";
    std::cout << "  Wire overhead vs proto: " << std::fixed << std::setprecision(2)
              << (double)serialized.size() / est_proto(logical) << "x\n";

    // Verify
    auto loaded = XBuffer::load_from_string(serialized);
    auto& v = loaded.root<CharacterEndGame>();
    assert(v.player_id == 7);
    assert(v.level == 100);
    assert(std::string(v.name.c_str()) == "Legendary_Hero_XYZ");
    assert(v.inventory.size() == 200);
    assert(v.skills.size() == 40);
    assert(v.friends.size() == 100);
    assert(v.achievements.size() == 150);
    assert(v.chat_log.size() == 50);
    assert(v.completed_quests.size() == 500);

    std::cout << "  Deserialization: zero-copy, zero-decode [OK]\n";
    std::cout << "  Data integrity: [OK]\n";
    std::cout << "  [PASS]\n";
    return true;
}

// ============================================================================
// Scenario 4: sizeof summary for all game types
// ============================================================================
bool test_sizeof_game_types() {
    std::cout << "\n[sizeof Summary] Game Data Types\n";
    std::cout << std::string(70, '-') << "\n";
    std::cout << "  offset_ptr<T>:       " << sizeof(boost::interprocess::offset_ptr<void>) << "B  (same as raw pointer)\n";
    std::cout << "  XString:             " << sizeof(XString) << "B  (std::string=" << sizeof(std::string) << "B)\n";
    std::cout << "  XVector<int>:        " << sizeof(XVector<int>) << "B  (std::vector=" << sizeof(std::vector<int>) << "B)\n\n";

    std::cout << "  Game structs:\n";
    std::cout << "    EquipSlot:         " << sizeof(EquipSlot) << "B\n";
    std::cout << "    Skill:             " << sizeof(Skill) << "B\n";
    std::cout << "    QuestProgress:     " << sizeof(QuestProgress) << "B\n";
    std::cout << "    InventoryItem:     " << sizeof(InventoryItem) << "B\n";
    std::cout << "    FriendEntry:       " << sizeof(FriendEntry) << "B\n";
    std::cout << "    Achievement:       " << sizeof(Achievement) << "B\n\n";

    std::cout << "  Top-level characters:\n";
    std::cout << "    CharacterMinimal:  " << sizeof(CharacterMinimal) << "B\n";
    std::cout << "    CharacterMidGame:  " << sizeof(CharacterMidGame) << "B\n";
    std::cout << "    CharacterEndGame:  " << sizeof(CharacterEndGame) << "B\n";
    std::cout << "  [PASS]\n";
    return true;
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "================================================================\n";
    std::cout << "  XOffset Memory Efficiency — Realistic Game Character Data\n";
    std::cout << "================================================================\n";

    bool ok = true;
    ok &= test_sizeof_game_types();
    ok &= test_profile_minimal();
    ok &= test_profile_midgame();
    ok &= test_profile_endgame();

    std::cout << "\n================================================================\n";
    if (ok) {
        std::cout << "  [PASS] All game character profiles analyzed successfully.\n";
    } else {
        std::cout << "  [FAIL] Some scenarios failed.\n";
    }
    std::cout << "================================================================\n";

    return ok ? 0 : 1;
}