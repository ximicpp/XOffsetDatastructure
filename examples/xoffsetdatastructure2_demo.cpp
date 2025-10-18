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

// Example 1: Game Data
struct GameData {
    template <typename Allocator>
    GameData(Allocator allocator)
        : player_name(allocator),
          inventory(allocator),
          achievements(allocator),
          quest_progress(allocator) {}
    
    int player_id;
    int level;
    float health;
    XString player_name;
    XVector<int> inventory;           // Item IDs
    XSet<int> achievements;            // Achievement IDs
    XMap<XString, int> quest_progress; // Quest name -> progress %
};

// Example 2: Data Container
struct DataContainer {
    template <typename Allocator>
    DataContainer(Allocator allocator) : items(allocator) {}
    XVector<int> items;
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
    
    // Add inventory items
    game->inventory = {101, 102, 103, 201, 202, 305};
    
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
    
    // Step 3: Display summary
    std::cout << "3. Game data summary:\n";
    std::cout << "   Player: " << game->player_name << " (ID: " << game->player_id << ")\n";
    std::cout << "   Level: " << game->level << ", Health: " << game->health << "%\n";
    std::cout << "   Inventory: " << game->inventory.size() << " items\n";
    std::cout << "   Achievements: " << game->achievements.size() << " unlocked\n";
    std::cout << "   Quests: " << game->quest_progress.size() << " in progress\n";
    
    // Step 4: Show quest details
    std::cout << "\n4. Quest progress:\n";
    for (const auto& [quest_name, progress] : game->quest_progress) {
        std::cout << "   " << quest_name << ": " << progress << "%\n";
    }
    
    // Step 5: Modify data
    std::cout << "\n5. Updating data (level up, add item)...\n";
    game->level++;
    game->inventory.push_back(999); // Legendary sword!
    game->quest_progress[XString("MainQuest", xbuf.get_segment_manager())] = 90;
    
    std::cout << "   New level: " << game->level << "\n";
    std::cout << "   Inventory size: " << game->inventory.size() << "\n";
    std::cout << "   MainQuest progress: " 
              << game->quest_progress[XString("MainQuest", xbuf.get_segment_manager())] << "%\n";
    
    std::cout << "\n[OK] Example 1 completed successfully!\n";
}

// ============================================================================
// Example 2: Memory Operations
// ============================================================================
void example_memory_operations() {
    std::cout << "\n========== Example 2: Memory Operations ==========\n\n";
    
    // Step 1: Create small buffer
    std::cout << "1. Creating 1KB buffer...\n";
    XBuffer xbuf(1024);
    auto stats1 = XBufferVisualizer::get_memory_stats(xbuf);
    std::cout << "   Total: " << stats1.total_size << " bytes\n";
    std::cout << "   Used: " << stats1.used_size << " bytes (" 
              << stats1.usage_percent() << "%)\n";
    
    // Step 2: Add data
    std::cout << "\n2. Adding data...\n";
    auto* data = xbuf.construct<DataContainer>("Data")(xbuf.get_segment_manager());
    for (int i = 0; i < 50; ++i) {
        data->items.push_back(i);
    }
    auto stats2 = XBufferVisualizer::get_memory_stats(xbuf);
    std::cout << "   Used: " << stats2.used_size << " bytes (" 
              << stats2.usage_percent() << "%)\n";
    
    // Step 3: Grow buffer
    std::cout << "\n3. Growing buffer by 2KB...\n";
    xbuf.grow(2048);
    auto stats3 = XBufferVisualizer::get_memory_stats(xbuf);
    std::cout << "   Total: " << stats3.total_size << " bytes\n";
    std::cout << "   Used: " << stats3.used_size << " bytes (" 
              << stats3.usage_percent() << "%)\n";
    
    // Step 4: Add more data
    std::cout << "\n4. Adding more data...\n";
    data = xbuf.find_or_construct<DataContainer>("Data")(xbuf.get_segment_manager());
    for (int i = 50; i < 200; ++i) {
        data->items.push_back(i);
    }
    auto stats4 = XBufferVisualizer::get_memory_stats(xbuf);
    std::cout << "   Items: " << data->items.size() << "\n";
    std::cout << "   Used: " << stats4.used_size << " bytes (" 
              << stats4.usage_percent() << "%)\n";
    
    // Step 5: Shrink to fit
    std::cout << "\n5. Shrinking to fit...\n";
    xbuf.shrink_to_fit();
    auto stats5 = XBufferVisualizer::get_memory_stats(xbuf);
    std::cout << "   Total: " << stats5.total_size << " bytes (reduced from " 
              << stats4.total_size << ")\n";
    std::cout << "   Used: " << stats5.used_size << " bytes (" 
              << stats5.usage_percent() << "%)\n";
    
    // Step 6: Verify data integrity
    auto [verify_data, found] = xbuf.find<DataContainer>("Data");
    std::cout << "\n6. Verifying data integrity...\n";
    std::cout << "   Items count: " << verify_data->items.size() << "\n";
    std::cout << "   First item: " << verify_data->items[0] << "\n";
    std::cout << "   Last item: " << verify_data->items.back() << "\n";
    
    std::cout << "\n[OK] Example 2 completed successfully!\n";
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char* argv[]) {
    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "  XOffsetDatastructure2 Demo - Usage Examples\n";
    std::cout << "======================================================================\n";
    
    try {
        // Run examples
        example_game_data();
        example_memory_operations();
        
        std::cout << "\n======================================================================\n";
        std::cout << "  All examples completed successfully!\n";
        std::cout << "======================================================================\n\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] Exception: " << e.what() << "\n";
        return 1;
    }
}
