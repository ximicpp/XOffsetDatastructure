// ============================================================================
// XOffsetDatastructure2 - Feature Demonstration
// Purpose: Demonstrate offset-based containers with zero-encoding/decoding
// ============================================================================

#include <iostream>
#include <string>
#include "../xoffsetdatastructure2.hpp"
#include "../generated/game_data.hpp"

using namespace XOffsetDatastructure2;

// Demo 1: Basic Container Operations
void demo_basic_usage() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Demo 1: Basic Container Operations\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    // Create buffer and game data
    XBufferExt xbuf(4096);
    auto* game = xbuf.make<GameData>("player_save");
    
    // Set player info
    game->player_name = XString("Hero", xbuf.allocator<XString>());
    game->player_id = 12345;
    game->level = 42;
    game->health = 100.0f;
    
    std::cout << "[Player Info]\n";
    std::cout << "  Name: " << game->player_name.c_str() << "\n";
    std::cout << "  ID: " << game->player_id << " | Level: " << game->level 
              << " | Health: " << game->health << "\n\n";
    
    // XVector: Add items to inventory
    std::cout << "[XVector: Inventory]\n";
    const char* item_types[] = {"Potion", "Weapon", "Armor"};
    for (int i = 0; i < 5; i++) {
        std::string item_name = "Item_" + std::to_string(i+1);
        game->items.emplace_back(
            xbuf.allocator<Item>(),
            i + 1,                    // item_id
            i % 3,                    // item_type
            (i + 1) * 10,             // quantity
            item_name.c_str()
        );
    }
    
    for (const auto& item : game->items) {
        std::cout << "  [" << item.item_id << "] " << item.name.c_str()
                  << " x" << item.quantity 
                  << " (" << item_types[item.item_type] << ")\n";
    }
    
    // XSet: Track achievements
    std::cout << "\n[XSet: Achievements]\n";
    for (int id : {1, 5, 10, 25, 50}) {
        game->achievements.insert(id);
    }
    std::cout << "  Unlocked: " << game->achievements.size() << " achievements\n";
    std::cout << "  IDs: ";
    for (int id : game->achievements) {
        std::cout << id << " ";
    }
    std::cout << "\n";
    
    // XMap: Quest progress
    std::cout << "\n[XMap: Quest Progress]\n";
    game->quest_progress[XString("Main Quest", xbuf.allocator<XString>())] = 75;
    game->quest_progress[XString("Side Quest", xbuf.allocator<XString>())] = 100;
    
    for (const auto& [quest, progress] : game->quest_progress) {
        std::cout << "  " << quest.c_str() << ": " << progress << "%\n";
    }
}

// Demo 2: Memory Management
void demo_memory_management() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Demo 2: Memory Management\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    // Start with small buffer
    std::cout << "[Initial State]\n";
    XBufferExt xbuf(1024);
    auto stats = xbuf.stats();
    std::cout << "  Total: " << stats.total_size << " bytes\n";
    std::cout << "  Used: " << stats.used_size << " bytes (" 
              << static_cast<int>(stats.usage_percent()) << "%)\n\n";
    
    // Add data
    std::cout << "[Adding Data]\n";
    auto* game = xbuf.make<GameData>("game");
    game->player_name = XString("Player1", xbuf.allocator<XString>());
    for (int i = 0; i < 100; i++) {
        game->achievements.insert(i);
    }
    
    stats = xbuf.stats();
    std::cout << "  Used: " << stats.used_size << " bytes (" 
              << static_cast<int>(stats.usage_percent()) << "%)\n\n";
    
    // Grow buffer
    std::cout << "[Dynamic Growth]\n";
    xbuf.grow(4096);
    stats = xbuf.stats();
    std::cout << "  New Total: " << stats.total_size << " bytes\n";
    std::cout << "  Usage: " << static_cast<int>(stats.usage_percent()) << "%\n\n";
    
    // Shrink to fit
    std::cout << "[Memory Optimization]\n";
    xbuf.shrink_to_fit();
    stats = xbuf.stats();
    std::cout << "  After Shrink: " << stats.total_size << " bytes\n";
    std::cout << "  Usage: " << static_cast<int>(stats.usage_percent()) << "%\n";
}

// Demo 3: Zero-Encoding/Decoding Serialization
void demo_serialization() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Demo 3: Zero-Encoding/Decoding Serialization\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    // Create and populate game data
    std::cout << "[Create Game State]\n";
    XBufferExt src_buf(2048);
    auto* src_game = src_buf.make<GameData>("save");
    
    src_game->player_name = XString("SavedHero", src_buf.allocator<XString>());
    src_game->player_id = 99999;
    src_game->level = 99;
    src_game->health = 100.0f;
    
    // Add some items
    for (int i = 0; i < 3; i++) {
        std::string name = "Item_" + std::to_string(i);
        src_game->items.emplace_back(
            src_buf.allocator<Item>(),
            i, 0, 1, name.c_str()
        );
    }
    
    std::cout << "  Player: " << src_game->player_name.c_str() << "\n";
    std::cout << "  ID: " << src_game->player_id << " | Level: " << src_game->level << "\n";
    std::cout << "  Items: " << src_game->items.size() << "\n\n";
    
    // Serialize to binary (zero-encoding: no encoding/decoding overhead)
    std::cout << "[Serialize to Binary]\n";
    std::string binary_data = src_buf.save_to_string();
    std::cout << "  Binary size: " << binary_data.size() << " bytes\n\n";
    
    // Deserialize from binary
    std::cout << "[Deserialize from Binary]\n";
    XBufferExt dst_buf = XBufferExt::load_from_string(binary_data);
    auto* dst_game = dst_buf.find<GameData>("save").first;
    
    std::cout << "  Player: " << dst_game->player_name.c_str() << "\n";
    std::cout << "  ID: " << dst_game->player_id << " | Level: " << dst_game->level << "\n";
    std::cout << "  Items: " << dst_game->items.size() << "\n\n";
    
    // Verify data integrity
    bool integrity_ok = (
        std::string(dst_game->player_name.c_str()) == "SavedHero" &&
        dst_game->player_id == 99999 &&
        dst_game->level == 99 &&
        dst_game->items.size() == 3
    );
    
    std::cout << "[Data Integrity]\n";
    std::cout << "  Status: " << (integrity_ok ? "✓ PASS" : "✗ FAIL") << "\n";
}

// Main Entry Point
int main() {
    std::cout << R"(
+============================================================+
|     XOffsetDatastructure2 - Feature Demonstration         |
| Offset-Based Containers with Zero-Encoding/Decoding       |
+============================================================+
)";

    try {
        demo_basic_usage();
        demo_memory_management();
        demo_serialization();
        
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "Summary\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "\n✓ All demonstrations completed!\n\n";
        std::cout << "Key Features:\n";
        std::cout << "  • XVector, XSet, XMap - Offset-based containers\n";
        std::cout << "  • Zero-encoding/decoding (no encode/decode overhead)\n";
        std::cout << "  • Dynamic memory management (grow/shrink)\n";
        std::cout << "  • Type-safe with compile-time verification\n\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] " << e.what() << "\n";
        return 1;
    }
}
