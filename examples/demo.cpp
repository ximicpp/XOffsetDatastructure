// ============================================================================
// XOffsetDatastructure2 Demo
// Purpose: Demonstrate basic usage and API
// ============================================================================

#include <iostream>
#include <fstream>
#include <vector>
#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Data Structures
// ============================================================================

struct alignas(BASIC_ALIGNMENT) Item {
    template <typename Allocator>
    Item(Allocator allocator) 
        : item_id(0), item_type(0), quantity(0), name(allocator) {}
    
    int item_id;
    int item_type;    // 0=weapon, 1=armor, 2=potion, 3=material
    int quantity;
    XString name;
};

struct alignas(BASIC_ALIGNMENT) GameData {
    template <typename Allocator>
    GameData(Allocator allocator)
        : player_name(allocator),
          items(allocator),
          achievements(allocator),
          quest_progress(allocator) {}
    
    int player_id;
    int level;
    float health;
    XString player_name;
    XVector<Item> items;               // Inventory items with type info
    XSet<int> achievements;            // Achievement IDs
    XMap<XString, int> quest_progress; // Quest name -> progress %
};

// ============================================================================
// Example: Game Data with Nested Structures
// ============================================================================
void example_game_data() {
    std::cout << "\n========== Example: Game Data with User-Friendly API ==========\n\n";
    
    // Create buffer with extended API
    std::cout << "1. Creating buffer...\n";
    XBufferExt xbuf(8192);
    std::cout << "   Buffer size: " << xbuf.get_size() << " bytes\n";
    
    // Create and fill game data using unified make<T>() API
    std::cout << "\n2. Creating game data with unified make<T>() API...\n";
    auto* game = xbuf.make<GameData>("Player1");
    game->player_id = 99;
    game->level = 42;
    game->health = 87.5f;
    game->player_name = xbuf.make<XString>("DragonSlayer");
    
    // Add items using unified API
    game->items.emplace_back(xbuf.allocator<Item>());
    game->items.back().item_id = 101;
    game->items.back().item_type = 0;
    game->items.back().quantity = 1;
    game->items.back().name = xbuf.make<XString>("Steel Sword");
    
    game->items.emplace_back(xbuf.allocator<Item>());
    game->items.back().item_id = 201;
    game->items.back().item_type = 1;
    game->items.back().quantity = 1;
    game->items.back().name = xbuf.make<XString>("Iron Shield");
    
    game->items.emplace_back(xbuf.allocator<Item>());
    game->items.back().item_id = 301;
    game->items.back().item_type = 2;
    game->items.back().quantity = 5;
    game->items.back().name = xbuf.make<XString>("Health Potion");
    
    // Add achievements
    game->achievements.insert(1);
    game->achievements.insert(5);
    game->achievements.insert(10);
    
    // Add quest progress
    game->quest_progress.emplace(xbuf.make<XString>("MainQuest"), 75);
    game->quest_progress.emplace(xbuf.make<XString>("SideQuest1"), 100);
    game->quest_progress.emplace(xbuf.make<XString>("SideQuest2"), 50);
    
    // Display summary
    std::cout << "3. Game data summary:\n";
    std::cout << "   Player: " << game->player_name << " (ID: " << game->player_id << ")\n";
    std::cout << "   Level: " << game->level << ", Health: " << game->health << "%\n";
    std::cout << "   Items:\n";
    for (const auto& item : game->items) {
        const char* type_name[] = {"Weapon", "Armor", "Potion", "Material"};
        std::cout << "     - " << item.name << " (x" << item.quantity << ", " 
                  << type_name[item.item_type] << ")\n";
    }
    std::cout << "   Achievements: " << game->achievements.size() << " unlocked\n";
    std::cout << "   Quests: " << game->quest_progress.size() << " in progress\n";
    
    auto stats3 = xbuf.stats();
    std::cout << "   Memory used: " << stats3.used_size << " / " << stats3.total_size 
              << " bytes (" << stats3.usage_percent() << "%)\n";
    
    // Show quest details
    std::cout << "\n4. Quest progress:\n";
    for (const auto& [quest_name, progress] : game->quest_progress) {
        std::cout << "   " << quest_name << ": " << progress << "%\n";
    }
    
    // Grow buffer and add more items
    std::cout << "\n5. Growing buffer and adding more items...\n";
    xbuf.grow(8192);
    game = xbuf.find_or_make<GameData>("Player1");
    
    // Add materials using unified API
    for (int i = 0; i < 10; ++i) {
        game->items.emplace_back(xbuf.allocator<Item>());
        game->items.back().item_id = 400 + i;
        game->items.back().item_type = 3;
        game->items.back().quantity = 10 + i;
        game->items.back().name = xbuf.make<XString>("Material_" + std::to_string(i));
    }
    
    auto stats5 = xbuf.stats();
    std::cout << "   Total items: " << game->items.size() << "\n";
    std::cout << "   Memory used: " << stats5.used_size << " / " << stats5.total_size 
              << " bytes (" << stats5.usage_percent() << "%)\n";
    
    // Modify data and shrink
    std::cout << "\n6. Updating data and shrinking buffer...\n";
    game->level++;
    
    // Add legendary sword using unified API
    game->items.emplace_back(xbuf.allocator<Item>());
    game->items.back().item_id = 999;
    game->items.back().item_type = 0;
    game->items.back().quantity = 1;
    game->items.back().name = xbuf.make<XString>("Legendary Sword");
    
    game->quest_progress[xbuf.make<XString>("MainQuest")] = 90;
    
    xbuf.shrink_to_fit();
    auto stats6 = xbuf.stats();
    std::cout << "   New level: " << game->level << "\n";
    std::cout << "   Total items: " << game->items.size() << "\n";
    std::cout << "   Last item added: " << game->items.back().name 
              << " (ID: " << game->items.back().item_id << ")\n";
    std::cout << "   MainQuest progress: " 
              << game->quest_progress[xbuf.make<XString>("MainQuest")] << "%\n";
    std::cout << "   Memory after shrink: " << stats6.used_size << " / " << stats6.total_size 
              << " bytes (" << stats6.usage_percent() << "%)\n";
    
    // Serialization and Deserialization
    std::cout << "\n7. Serialization and Deserialization...\n";
    
    // Serialize to string
    std::cout << "   Serializing to string...\n";
    auto serialized_data = xbuf.save_to_string();
    std::cout << "   Serialized size: " << serialized_data.size() << " bytes\n";
    
    // Deserialize from string
    std::cout << "   Deserializing from string...\n";
    XBufferExt loaded_xbuf = XBufferExt::load_from_string(serialized_data);
    std::cout << "   [OK] Loaded successfully!\n";
    
    auto [loaded_game, loaded_found] = loaded_xbuf.find_ex<GameData>("Player1");
    if (loaded_found) {
        std::cout << "   Loaded player: " << loaded_game->player_name 
                  << " (Level " << loaded_game->level << ")\n";
        std::cout << "   Loaded items count: " << loaded_game->items.size() << "\n";
        std::cout << "   Loaded first item: " << loaded_game->items[0].name << "\n";
        std::cout << "   Loaded last item: " << loaded_game->items.back().name << "\n";
        
        // Verify data integrity
        bool data_matches = (loaded_game->player_id == game->player_id) &&
                           (loaded_game->level == game->level) &&
                           (loaded_game->items.size() == game->items.size()) &&
                           (loaded_game->achievements.size() == game->achievements.size());
        
        if (data_matches) {
            std::cout << "   [OK] Serialized and deserialized data match!\n";
        } else {
            std::cout << "   [ERROR] Data mismatch after deserialization!\n";
        }
    } else {
        std::cout << "   [ERROR] Player data not found after deserialization!\n";
    }
    
    // Verify original data integrity
    std::cout << "\n8. Verifying original data integrity...\n";
    auto [verify_game, found] = xbuf.find_ex<GameData>("Player1");
    std::cout << "   Found: " << (found ? "YES" : "NO") << "\n";
    std::cout << "   Player: " << verify_game->player_name << "\n";
    std::cout << "   Total items: " << verify_game->items.size() << "\n";
    std::cout << "   First item: " << verify_game->items[0].name 
              << " (ID: " << verify_game->items[0].item_id << ")\n";
    std::cout << "   Last item: " << verify_game->items.back().name 
              << " (ID: " << verify_game->items.back().item_id << ")\n";
    
    std::cout << "\n[OK] Example completed successfully!\n";
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char* argv[]) {
    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "  XOffsetDatastructure2 Demo - Usage Example\n";
    std::cout << "======================================================================\n";
    
    // Run example
    example_game_data();
    
    std::cout << "\n======================================================================\n";
    std::cout << "  Example completed successfully!\n";
    std::cout << "======================================================================\n\n";
    
    return 0;
}
