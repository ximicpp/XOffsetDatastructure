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

struct Item {
    template <typename Allocator>
    Item(Allocator allocator) 
        : item_id(0), item_type(0), quantity(0), name(allocator) {}
    
    int item_id;
    int item_type;    // 0=weapon, 1=armor, 2=potion, 3=material
    int quantity;
    XString name;
};

struct GameData {
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
// Example 1: Game Data with Nested Structures
// ============================================================================
void example_game_data() {
    std::cout << "\n========== Example 1: Game Data ==========\n\n";
    
    // Step 1: Create game data
    std::cout << "1. Creating game data...\n";
    XBuffer xbuf(8192);
    auto* game = xbuf.construct<GameData>("Player1")(xbuf.get_segment_manager());
    
    // Step 2: Fill data
    std::cout << "2. Filling player data...\n";
    game->player_id = 99;
    game->level = 42;
    game->health = 87.5f;
    game->player_name = XString("DragonSlayer", xbuf.get_segment_manager());
    
    // Add items with type information
    game->items.emplace_back(xbuf.get_segment_manager());
    game->items.back().item_id = 101;
    game->items.back().item_type = 0; // weapon
    game->items.back().quantity = 1;
    game->items.back().name = XString("Steel Sword", xbuf.get_segment_manager());
    
    game->items.emplace_back(xbuf.get_segment_manager());
    game->items.back().item_id = 201;
    game->items.back().item_type = 1; // armor
    game->items.back().quantity = 1;
    game->items.back().name = XString("Iron Shield", xbuf.get_segment_manager());
    
    game->items.emplace_back(xbuf.get_segment_manager());
    game->items.back().item_id = 301;
    game->items.back().item_type = 2; // potion
    game->items.back().quantity = 5;
    game->items.back().name = XString("Health Potion", xbuf.get_segment_manager());
    
    // Add achievements
    game->achievements.insert(1);  // First Blood
    game->achievements.insert(5);  // Level 10
    game->achievements.insert(10); // Dragon Slayer
    
    // Add quest progress
    game->quest_progress.emplace(
        XString("MainQuest", xbuf.get_segment_manager()), 75);
    game->quest_progress.emplace(
        XString("SideQuest1", xbuf.get_segment_manager()), 100);
    game->quest_progress.emplace(
        XString("SideQuest2", xbuf.get_segment_manager()), 50);
    
    // Step 3: Display summary and check memory
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
    
    auto stats3 = XBufferVisualizer::get_memory_stats(xbuf);
    std::cout << "   Memory used: " << stats3.used_size << " / " << stats3.total_size 
              << " bytes (" << stats3.usage_percent() << "%)\n";
    
    // Step 4: Show quest details
    std::cout << "\n4. Quest progress:\n";
    for (const auto& [quest_name, progress] : game->quest_progress) {
        std::cout << "   " << quest_name << ": " << progress << "%\n";
    }
    
    // Step 5: Grow buffer and add more items
    std::cout << "\n5. Growing buffer and adding more items...\n";
    xbuf.grow(8192);
    game = xbuf.find_or_construct<GameData>("Player1")(xbuf.get_segment_manager());
    
    // Add materials
    for (int i = 0; i < 10; ++i) {
        game->items.emplace_back(xbuf.get_segment_manager());
        game->items.back().item_id = 400 + i;
        game->items.back().item_type = 3; // material
        game->items.back().quantity = 10 + i;
        game->items.back().name = XString(("Material_" + std::to_string(i)).c_str(), 
                                          xbuf.get_segment_manager());
    }
    
    auto stats5 = XBufferVisualizer::get_memory_stats(xbuf);
    std::cout << "   Total items: " << game->items.size() << "\n";
    std::cout << "   Memory used: " << stats5.used_size << " / " << stats5.total_size 
              << " bytes (" << stats5.usage_percent() << "%)\n";
    
    // Step 6: Modify data and shrink
    std::cout << "\n6. Updating data and shrinking buffer...\n";
    game->level++;
    
    // Add legendary sword
    game->items.emplace_back(xbuf.get_segment_manager());
    game->items.back().item_id = 999;
    game->items.back().item_type = 0; // weapon
    game->items.back().quantity = 1;
    game->items.back().name = XString("Legendary Sword", xbuf.get_segment_manager());
    
    game->quest_progress[XString("MainQuest", xbuf.get_segment_manager())] = 90;
    
    xbuf.shrink_to_fit();
    auto stats6 = XBufferVisualizer::get_memory_stats(xbuf);
    std::cout << "   New level: " << game->level << "\n";
    std::cout << "   Total items: " << game->items.size() << "\n";
    std::cout << "   Last item added: " << game->items.back().name 
              << " (ID: " << game->items.back().item_id << ")\n";
    std::cout << "   MainQuest progress: " 
              << game->quest_progress[XString("MainQuest", xbuf.get_segment_manager())] << "%\n";
    std::cout << "   Memory after shrink: " << stats6.used_size << " / " << stats6.total_size 
              << " bytes (" << stats6.usage_percent() << "%)\n";
    
    // Step 7: Verify data integrity
    std::cout << "\n7. Verifying data integrity...\n";
    auto [verify_game, found] = xbuf.find<GameData>("Player1");
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
    
    try {
        // Run example
        example_game_data();
        
        std::cout << "\n======================================================================\n";
        std::cout << "  Example completed successfully!\n";
        std::cout << "======================================================================\n\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] Exception: " << e.what() << "\n";
        return 1;
    }
}
