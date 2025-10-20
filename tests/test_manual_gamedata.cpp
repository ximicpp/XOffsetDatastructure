// ============================================================================
// Test: Manual GameData Definition
// Purpose: Test if manually defining GameData works
// ============================================================================

#include <iostream>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// Manually define Item (same as generated)
struct alignas(8) ItemManual {
    template <typename Allocator>
    ItemManual(Allocator allocator) : name(allocator) {}

    template <typename Allocator>
    ItemManual(Allocator allocator, int item_id_val, int item_type_val, int quantity_val, const char* name_val)
        : item_id(item_id_val)
        , item_type(item_type_val)
        , quantity(quantity_val)
        , name(name_val, allocator)
    {}

    int item_id{0};
    int item_type{0};
    int quantity{0};
    XString name;
};

// Manually define GameData (same as generated)
struct alignas(8) GameDataManual {
    template <typename Allocator>
    GameDataManual(Allocator allocator) 
        : player_name(allocator), items(allocator), achievements(allocator), quest_progress(allocator) {}

    template <typename Allocator>
    GameDataManual(Allocator allocator, int player_id_val, int level_val, float health_val, const char* player_name_val)
        : player_id(player_id_val)
        , level(level_val)
        , health(health_val)
        , player_name(player_name_val, allocator)
        , items(allocator)
        , achievements(allocator)
        , quest_progress(allocator)
    {}

    int player_id{0};
    int level{0};
    float health{0.0f};
    XString player_name;
    XVector<ItemManual> items;
    XSet<int> achievements;
    XMap<XString, int> quest_progress;
};

int main() {
    std::cout << "\n[TEST] Manual GameData Definition\n\n";
    
    try {
        XBufferExt xbuf(4096);
        
        std::cout << "1. Creating GameDataManual... ";
        auto* game = xbuf.make<GameDataManual>("player_save");
        std::cout << "[OK]\n";
        
        std::cout << "2. Setting player_name... ";
        game->player_name = XString("Hero", xbuf.allocator<XString>());
        std::cout << "[OK]\n";
        
        std::cout << "3. Adding items... ";
        for (int i = 0; i < 3; i++) {
            std::string item_name = "Item" + std::to_string(i);
            game->items.emplace_back(
                xbuf.allocator<ItemManual>(),
                i, i % 3, i * 10,
                item_name.c_str()
            );
        }
        std::cout << "[OK]\n";
        
        std::cout << "4. Adding achievements... ";
        for (int i = 0; i < 5; i++) {
            game->achievements.insert(i);
        }
        std::cout << "[OK]\n";
        
        std::cout << "5. Adding quest progress... ";
        XString quest("Quest1", xbuf.allocator<XString>());
        game->quest_progress.emplace(quest, 75);
        std::cout << "[OK]\n";
        
        std::cout << "\n[PASS] All operations successful!\n\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}
