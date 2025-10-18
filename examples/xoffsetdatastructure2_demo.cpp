#include <iostream>
#include <fstream>
#include <vector>
#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// Data Structures for demo

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

void example_game_data() {
    std::cout << "\n========== Example 1: Game Data with Capacity Planning ==========\n\n";
    
    // Plan capacity first
    std::cout << "1. Planning buffer capacity...\n";
    std::cout << "   sizeof(GameData) = " << sizeof(GameData) << " bytes\n";
    std::cout << "   sizeof(Item) = " << sizeof(Item) << " bytes\n";
    std::cout << "   sizeof(XString) = " << sizeof(XString) << " bytes\n";
    
    auto capacity_plan = XBufferPlanner::Estimator()
        .add_objects<GameData>(1)
        .add_vector<Item>(13)
        .add_strings(13, 20)
        .add_set<int>(3)
        .add_map<XString, int>(3)
        .add_strings(3, 15)
        .add_margin(0.3);
    
    std::cout << "   Estimated capacity: " << capacity_plan.total() << " bytes\n";
    
    // Create buffer with planned capacity
    std::cout << "\n2. Creating buffer with planned capacity...\n";
    XBufferWithPlanner xbuf(capacity_plan.total());
    std::cout << "   Buffer size: " << xbuf.size() << " bytes\n";
    
    size_t before_used = xbuf.stats().used_size;
    
    // Create and fill game data
    std::cout << "\n3. Creating and filling game data...\n";
    auto* game = xbuf.construct<GameData>("Player1")(xbuf.get_segment_manager());
    game->player_id = 99;
    game->level = 42;
    game->health = 87.5f;
    game->player_name = XString("DragonSlayer", xbuf.get_segment_manager());
    
    // Add items
    game->items.emplace_back(xbuf.get_segment_manager());
    game->items.back().item_id = 101;
    game->items.back().item_type = 0;
    game->items.back().quantity = 1;
    game->items.back().name = XString("Steel Sword", xbuf.get_segment_manager());
    
    game->items.emplace_back(xbuf.get_segment_manager());
    game->items.back().item_id = 201;
    game->items.back().item_type = 1;
    game->items.back().quantity = 1;
    game->items.back().name = XString("Iron Shield", xbuf.get_segment_manager());
    
    game->items.emplace_back(xbuf.get_segment_manager());
    game->items.back().item_id = 301;
    game->items.back().item_type = 2;
    game->items.back().quantity = 5;
    game->items.back().name = XString("Health Potion", xbuf.get_segment_manager());
    
    // Add achievements
    game->achievements.insert(1);
    game->achievements.insert(5);
    game->achievements.insert(10);
    
    // Add quest progress
    game->quest_progress.emplace(
        XString("MainQuest", xbuf.get_segment_manager()), 75);
    game->quest_progress.emplace(
        XString("SideQuest1", xbuf.get_segment_manager()), 100);
    game->quest_progress.emplace(
        XString("SideQuest2", xbuf.get_segment_manager()), 50);
    
    // Display summary
    std::cout << "\n4. Game data summary:\n";
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
    
    // Validate capacity estimation
    size_t after_used = xbuf.stats().used_size;
    size_t actual_used = after_used - before_used;
    
    std::cout << "\n5. Capacity estimation accuracy:\n";
    std::cout << "   Estimated: " << capacity_plan.total() << " bytes\n";
    std::cout << "   Actual used: " << actual_used << " bytes\n";
    double accuracy = (double)actual_used / (double)capacity_plan.total() * 100.0;
    std::cout << "   Usage rate: " << std::fixed << std::setprecision(1) 
              << accuracy << "%\n";
    std::cout << "   Safety margin: " << (capacity_plan.total() - actual_used) 
              << " bytes remaining (" << (100.0 - accuracy) << "%)\n";
    
    if (actual_used <= capacity_plan.total()) {
        std::cout << "   [OK] Estimation was sufficient - no out-of-memory errors!\n";
    } else {
        std::cout << "   [ERROR] Estimation was insufficient\n";
    }
    
    std::cout << "\n6. Current buffer state:\n";
    xbuf.print_stats();
    
    // Show quest details
    std::cout << "\n7. Quest progress:\n";
    for (const auto& [quest_name, progress] : game->quest_progress) {
        std::cout << "   " << quest_name << ": " << progress << "%\n";
    }
    
    // Modify existing data
    std::cout << "\n8. Modifying existing data...\n";
    game->level++;
    game->quest_progress[XString("MainQuest", xbuf.get_segment_manager())] = 90;
    
    // Add a legendary item
    game->items.emplace_back(xbuf.get_segment_manager());
    game->items.back().item_id = 999;
    game->items.back().item_type = 0;
    game->items.back().quantity = 1;
    game->items.back().name = XString("Legendary Sword", xbuf.get_segment_manager());
    
    std::cout << "   New level: " << game->level << "\n";
    std::cout << "   Total items: " << game->items.size() << "\n";
    std::cout << "   Last item added: " << game->items.back().name 
              << " (ID: " << game->items.back().item_id << ")\n";
    std::cout << "   MainQuest progress: " 
              << game->quest_progress[XString("MainQuest", xbuf.get_segment_manager())] << "%\n";
    
    std::cout << "\n9. Final buffer state:\n";
    xbuf.print_stats();
    
    // Verify data persistence
    std::cout << "\n10. Verifying data persistence...\n";
    auto [verify_game, found] = xbuf.find<GameData>("Player1");
    if (found) {
        std::cout << "   [OK] Data found in buffer\n";
        std::cout << "   Player: " << verify_game->player_name << "\n";
        std::cout << "   Level: " << verify_game->level << "\n";
        std::cout << "   Total items: " << verify_game->items.size() << "\n";
        std::cout << "   First item: " << verify_game->items[0].name 
                  << " (ID: " << verify_game->items[0].item_id << ")\n";
        std::cout << "   Last item: " << verify_game->items.back().name 
                  << " (ID: " << verify_game->items.back().item_id << ")\n";
    } else {
        std::cout << "   [ERROR] Data not found!\n";
    }
    
    std::cout << "\n[OK] Example completed successfully!\n";
}

int main(int argc, char* argv[]) {
    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "  XOffsetDatastructure2 Demo - Usage Example\n";
    std::cout << "======================================================================\n";
    
    try {
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
