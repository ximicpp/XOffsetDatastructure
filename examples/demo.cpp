// ============================================================================
// XOffsetDatastructure2 - Comprehensive Demo
// Purpose: Showcase the main features and capabilities
// ============================================================================

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include "../xoffsetdatastructure2.hpp"
#include "../generated/game_data.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Demo Utilities
// ============================================================================

void print_section(const std::string& title) {
    std::cout << "\n";
    std::cout << "+"; 
    for (int i = 0; i < 68; i++) std::cout << "=";
    std::cout << "+\n";
    std::cout << "| " << std::left << std::setw(66) << title << " |\n";
    std::cout << "+";
    for (int i = 0; i < 68; i++) std::cout << "=";
    std::cout << "+\n";
}

void print_subsection(const std::string& title) {
    std::cout << "\n+- " << title << "\n";
}

void print_check(const std::string& msg) {
    std::cout << "  [OK] " << msg << "\n";
}

void print_info(const std::string& key, const std::string& value) {
    std::cout << "  " << std::left << std::setw(20) << key << ": " << value << "\n";
}

// ============================================================================
// Demo 1: Basic Usage
// ============================================================================

void demo_basic_usage() {
    print_section("1. Basic Usage - Creating and Accessing Data");
    
    // Create buffer
    print_subsection("Creating XBufferExt with 4KB");
    XBufferExt xbuf(4096);
    print_check("Buffer created");
    
    // Create game data
    print_subsection("Creating Game Data");
    auto* game = xbuf.make<GameData>("player_save");
    
    // Set player data
    game->player_name = xbuf.make<XString>("Hero");
    game->player_id = 12345;
    game->level = 42;
    game->health = 100.0f;
    
    print_check("Player created: " + std::string(game->player_name.c_str()));
    print_info("Player ID", std::to_string(game->player_id));
    print_info("Level", std::to_string(game->level));
    print_info("Health", std::to_string(game->health));
    
    // Add items
    print_subsection("Adding Items to Inventory");
    for (int i = 0; i < 5; i++) {
        Item item(xbuf.allocator<Item>());
        item.item_id = i + 1;
        item.item_type = (i % 3);  // 0=Potion, 1=Weapon, 2=Armor
        std::string item_name = "Potion " + std::to_string(i+1);
        item.name = xbuf.make<XString>(item_name);
        item.quantity = (i + 1) * 10;
        game->items.push_back(item);
    }
    print_check("Added " + std::to_string(game->items.size()) + " items");
    
    // Add achievements (using integers)
    print_subsection("Unlocking Achievements");
    std::vector<int> achievements = {1, 5, 10, 25, 50, 100};  // Achievement IDs
    for (int ach_id : achievements) {
        game->achievements.insert(ach_id);
    }
    print_check("Unlocked " + std::to_string(game->achievements.size()) + " achievements");
    
    // Add quest progress
    print_subsection("Quest Progress");
    game->quest_progress[xbuf.make<XString>("Main Quest")] = 75;
    game->quest_progress[xbuf.make<XString>("Side Quest A")] = 100;
    game->quest_progress[xbuf.make<XString>("Side Quest B")] = 50;
    print_check("Tracking " + std::to_string(game->quest_progress.size()) + " quests");
    
    // Display inventory
    print_subsection("Inventory Details");
    const char* item_types[] = {"Potion", "Weapon", "Armor"};
    for (size_t i = 0; i < game->items.size(); i++) {
        const auto& item = game->items[i];
        std::cout << "  [" << item.item_id << "] " 
                  << std::left << std::setw(15) << item.name.c_str()
                  << " x" << std::setw(3) << item.quantity
                  << " (Type: " << item_types[item.item_type] << ")\n";
    }
}

// ============================================================================
// Demo 2: Memory Management
// ============================================================================

void demo_memory_management() {
    print_section("2. Memory Management - Buffer Operations");
    
    print_subsection("Initial Buffer");
    XBufferExt xbuf(1024);
    auto stats = xbuf.stats();
    print_info("Total Size", std::to_string(stats.total_size) + " bytes");
    print_info("Free Size", std::to_string(stats.free_size) + " bytes");
    print_info("Usage", std::to_string(static_cast<int>(stats.usage_percent())) + "%");
    
    print_subsection("Adding Data");
    auto* game = xbuf.make<GameData>("game");
    game->player_name = xbuf.make<XString>("TestPlayer");
    for (int i = 0; i < 100; i++) {
        game->achievements.insert(i);  // Add achievement IDs
    }
    
    stats = xbuf.stats();
    print_info("After Adding Data", std::to_string(stats.used_size) + " bytes used");
    print_info("Usage", std::to_string(static_cast<int>(stats.usage_percent())) + "%");
    
    print_subsection("Growing Buffer");
    xbuf.grow(4096);
    stats = xbuf.stats();
    print_info("New Total Size", std::to_string(stats.total_size) + " bytes");
    print_info("Usage", std::to_string(static_cast<int>(stats.usage_percent())) + "%");
    print_check("Buffer grown successfully");
    
    print_subsection("Shrinking to Fit");
    xbuf.shrink_to_fit();
    stats = xbuf.stats();
    print_info("After Shrink", std::to_string(stats.total_size) + " bytes");
    print_info("Usage", std::to_string(static_cast<int>(stats.usage_percent())) + "%");
    print_check("Memory optimized");
}

// ============================================================================
// Demo 3: Serialization
// ============================================================================

void demo_serialization() {
    print_section("3. Serialization - Save and Load");
    
    print_subsection("Creating Source Data");
    XBufferExt src_buf(2048);
    auto* src_game = src_buf.make<GameData>("save");
    src_game->player_name = src_buf.make<XString>("SavedHero");
    src_game->player_id = 99999;
    src_game->level = 99;
    src_game->health = 100.0f;
    
    print_check("Created game state");
    print_info("Player", std::string(src_game->player_name.c_str()));
    print_info("Player ID", std::to_string(src_game->player_id));
    print_info("Level", std::to_string(src_game->level));
    
    print_subsection("Serializing to Binary");
    std::string binary_data = src_buf.save_to_string();
    print_check("Serialized to " + std::to_string(binary_data.size()) + " bytes");
    
    print_subsection("Deserializing from Binary");
    XBufferExt dst_buf = XBufferExt::load_from_string(binary_data);
    auto* dst_game = dst_buf.find<GameData>("save").first;
    
    if (dst_game) {
        print_check("Deserialization successful!");
        print_info("Player", std::string(dst_game->player_name.c_str()));
        print_info("Player ID", std::to_string(dst_game->player_id));
        print_info("Level", std::to_string(dst_game->level));
        
        // Verify integrity
        bool integrity_ok = (
            std::string(dst_game->player_name.c_str()) == "SavedHero" &&
            dst_game->player_id == 99999 &&
            dst_game->level == 99
        );
        
        if (integrity_ok) {
            print_check("Data integrity verified");
        }
    } else {
        std::cout << "  [FAIL] Deserialization failed\n";
    }
}

// ============================================================================
// Demo 4: Type Signature Verification
// ============================================================================

void demo_type_signatures() {
    print_section("4. Type Signature System - Compile-Time Safety");
    
    print_subsection("Type Signature Information");
    
    constexpr auto item_sig = XTypeSignature::get_XTypeSignature<ItemReflectionHint>();
    constexpr auto game_sig = XTypeSignature::get_XTypeSignature<GameDataReflectionHint>();
    
    print_info("Item Signature", std::string(item_sig.value));
    std::cout << "\n";
    print_info("GameData Signature", std::string(game_sig.value));
    
    print_subsection("Benefits");
    print_check("Binary compatibility across compilations");
    print_check("Automatic verification at compile-time");
    print_check("Prevents data corruption from layout changes");
    print_check("Type-safe offset calculations");
}

// ============================================================================
// Demo 5: Performance Insights
// ============================================================================

void demo_performance() {
    print_section("5. Performance Characteristics");
    
    print_subsection("Container Growth Strategy");
    print_info("Growth Factor", "1.1x (10% incremental)");
    print_info("Benefit", "Reduced memory overhead");
    print_info("Trade-off", "More frequent reallocations");
    
    print_subsection("Memory Layout");
    print_info("Allocator", "Sequential-fit (fast)");
    print_info("Alignment", "8-byte (BASIC_ALIGNMENT)");
    print_info("Pointers", "Offset-based (relocatable)");
    
    print_subsection("Platform Requirements");
    print_info("Architecture", "64-bit only");
    print_info("Byte Order", "Little-endian");
    print_info("Compatibility", "x86-64, ARM64");
    
    print_subsection("Benchmarking Example");
    XBufferExt xbuf(65536);  // 64KB buffer for 1000 items
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Insert 1000 items
    auto* game = xbuf.make<GameData>("perf_test");
    for (int i = 0; i < 1000; i++) {
        Item item(xbuf.allocator<Item>());
        item.item_id = i;
        item.item_type = i % 3;
        std::string item_name = "Item_" + std::to_string(i);
        item.name = xbuf.make<XString>(item_name);
        item.quantity = i;
        game->items.push_back(item);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    print_info("Operations", "1000 item insertions");
    print_info("Time", std::to_string(duration.count()) + " us");
    print_info("Avg per Item", std::to_string(duration.count() / 1000.0) + " us");
}

// ============================================================================
// Demo 6: Advanced Features
// ============================================================================

void demo_advanced_features() {
    print_section("6. Advanced Features");
    
    print_subsection("Container Types");
    print_check("XVector<T> - Dynamic array (like std::vector)");
    print_check("XSet<T> - Unique elements (flat_set implementation)");
    print_check("XMap<K,V> - Key-value pairs (flat_map implementation)");
    print_check("XString - Offset-aware string container");
    
    print_subsection("Type System");
    print_check("Compile-time type signature calculation");
    print_check("Automatic size and offset computation");
    print_check("Cross-compilation compatibility checks");
    print_check("Boost.PFR reflection support");
    
    print_subsection("Memory Features");
    print_check("Dynamic buffer growth");
    print_check("Shrink-to-fit optimization");
    print_check("Memory usage statistics");
    print_check("Zero-copy deserialization");
    
    print_subsection("Future Enhancements (Planned)");
    std::cout << "  - C++26 reflection-based compaction\n";
    std::cout << "  - Schema versioning and migration\n";
    std::cout << "  - JSON export/import\n";
    std::cout << "  - Performance benchmarking suite\n";
}

// ============================================================================
// Main Demo Entry Point
// ============================================================================

int main() {
    std::cout << R"(
+======================================================================+
|                                                                      |
|           XOffsetDatastructure2 - Comprehensive Demo                |
|                                                                      |
|     Offset-Based Data Structures with Type Signature System         |
|                                                                      |
+======================================================================+
)";

    try {
        // Run all demos
        demo_basic_usage();
        demo_memory_management();
        demo_serialization();
        demo_type_signatures();
        demo_performance();
        demo_advanced_features();
        
        // Summary
        print_section("Demo Complete");
        std::cout << "\n";
        std::cout << "  * All demonstrations completed successfully!\n";
        std::cout << "\n";
        std::cout << "  For more information:\n";
        std::cout << "     - Documentation: docs/\n";
        std::cout << "     - Examples: examples/\n";
        std::cout << "     - Tests: tests/\n";
        std::cout << "\n";
        std::cout << "  Key Takeaways:\n";
        std::cout << "     - Type-safe offset-based containers\n";
        std::cout << "     - Binary serialization with zero-copy\n";
        std::cout << "     - Compile-time type signature verification\n";
        std::cout << "     - Memory-efficient growth strategy\n";
        std::cout << "\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] " << e.what() << "\n";
        return 1;
    }
}
