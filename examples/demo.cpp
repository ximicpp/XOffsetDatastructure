// XOffsetDatastructure2 - Comprehensive Feature Demonstration

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
// Conditional chrono include to avoid Clang-14 + libstdc++-14 conflicts
#if !defined(__clang__) || __clang_major__ >= 15
#include <chrono>
#define HAS_CHRONO 1
#else
#define HAS_CHRONO 0
#endif
#include "../xoffsetdatastructure2.hpp"
#include "../generated/game_data.hpp"

using namespace XOffsetDatastructure2;

// Demo Utilities

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

// Demo 1: Basic Usage

void demo_basic_usage() {
    print_section("1. Basic Usage");
    
    print_subsection("Creating XBufferExt (4KB)");
    XBufferExt xbuf(4096);
    print_check("Buffer initialized");
    
    print_subsection("Creating Game Data");
    auto* game = xbuf.make<GameData>("player_save");
    
    game->player_name = XString("Hero", xbuf.allocator<XString>());
    game->player_id = 12345;
    game->level = 42;
    game->health = 100.0f;
    
    print_check("Player: " + std::string(game->player_name.c_str()));
    print_info("ID", std::to_string(game->player_id));
    print_info("Level", std::to_string(game->level));
    print_info("Health", std::to_string(game->health));
    
    print_subsection("Inventory Management");
    for (int i = 0; i < 5; i++) {
        std::string item_name = "Potion " + std::to_string(i+1);
        game->items.emplace_back(
            xbuf.allocator<Item>(),
            i + 1,              // item_id
            i % 3,              // item_type (0=Potion, 1=Weapon, 2=Armor)
            (i + 1) * 10,       // quantity
            item_name.c_str()   // name
        );
    }
    print_check(std::to_string(game->items.size()) + " items added");
    
    print_subsection("Achievements System");
    std::vector<int> achievements = {1, 5, 10, 25, 50, 100};  // Achievement IDs
    for (int ach_id : achievements) {
        game->achievements.insert(ach_id);
    }
    print_check(std::to_string(game->achievements.size()) + " achievements unlocked");
    
    print_subsection("Quest System");
    game->quest_progress[XString("Main Quest", xbuf.allocator<XString>())] = 75;
    game->quest_progress[XString("Side Quest A", xbuf.allocator<XString>())] = 100;
    game->quest_progress[XString("Side Quest B", xbuf.allocator<XString>())] = 50;
    print_check(std::to_string(game->quest_progress.size()) + " quests tracked");
    
    print_subsection("Inventory Listing");
    const char* item_types[] = {"Potion", "Weapon", "Armor"};
    for (size_t i = 0; i < game->items.size(); i++) {
        const auto& item = game->items[i];
        std::cout << "  [" << item.item_id << "] " 
                  << std::left << std::setw(15) << item.name.c_str()
                  << " x" << std::setw(3) << item.quantity
                  << " (Type: " << item_types[item.item_type] << ")\n";
    }
}

// Demo 2: Memory Management

void demo_memory_management() {
    print_section("2. Memory Management");
    
    print_subsection("Initial State");
    XBufferExt xbuf(1024);
    auto stats = xbuf.stats();
    print_info("Total Size", std::to_string(stats.total_size) + " bytes");
    print_info("Free Size", std::to_string(stats.free_size) + " bytes");
    print_info("Usage", std::to_string(static_cast<int>(stats.usage_percent())) + "%");
    
    print_subsection("Adding Data");
    auto* game = xbuf.make<GameData>("game");
    game->player_name = XString("TestPlayer", xbuf.allocator<XString>());
    for (int i = 0; i < 100; i++) {
        game->achievements.insert(i);  // Add achievement IDs
    }
    
    stats = xbuf.stats();
    print_info("After Adding Data", std::to_string(stats.used_size) + " bytes used");
    print_info("Usage", std::to_string(static_cast<int>(stats.usage_percent())) + "%");
    
    print_subsection("Dynamic Growth");
    xbuf.grow(4096);
    stats = xbuf.stats();
    print_info("New Total Size", std::to_string(stats.total_size) + " bytes");
    print_info("Usage", std::to_string(static_cast<int>(stats.usage_percent())) + "%");
    print_check("Buffer grown successfully");
    
    print_subsection("Memory Optimization");
    xbuf.shrink_to_fit();
    stats = xbuf.stats();
    print_info("After Shrink", std::to_string(stats.total_size) + " bytes");
    print_info("Usage", std::to_string(static_cast<int>(stats.usage_percent())) + "%");
    print_check("Memory optimized");
}

// Demo 3: Serialization

void demo_serialization() {
    print_section("3. Serialization");
    
    print_subsection("Creating Source Data");
    XBufferExt src_buf(2048);
    auto* src_game = src_buf.make<GameData>("save");
    src_game->player_name = XString("SavedHero", src_buf.allocator<XString>());
    src_game->player_id = 99999;
    src_game->level = 99;
    src_game->health = 100.0f;
    
    print_check("Created game state");
    print_info("Player", std::string(src_game->player_name.c_str()));
    print_info("Player ID", std::to_string(src_game->player_id));
    print_info("Level", std::to_string(src_game->level));
    
    print_subsection("Binary Serialization");
    std::string binary_data = src_buf.save_to_string();
    print_check("Serialized to " + std::to_string(binary_data.size()) + " bytes");
    
    print_subsection("Binary Deserialization");
    XBufferExt dst_buf = XBufferExt::load_from_string(binary_data);
    auto* dst_game = dst_buf.find<GameData>("save").first;
    
    if (dst_game) {
        print_check("Deserialization successful!");
        print_info("Player", std::string(dst_game->player_name.c_str()));
        print_info("Player ID", std::to_string(dst_game->player_id));
        print_info("Level", std::to_string(dst_game->level));
        
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

// Demo 4: Type Signature System

void demo_type_signatures() {
    print_section("4. Type Signature System");
    
    print_subsection("Type Signature Information");
    
#ifndef _MSC_VER
    constexpr auto item_sig = XTypeSignature::get_XTypeSignature<ItemReflectionHint>();
    constexpr auto game_sig = XTypeSignature::get_XTypeSignature<GameDataReflectionHint>();
    
    print_info("Item Signature", std::string(item_sig.value));
    std::cout << "\n";
    print_info("GameData Signature", std::string(game_sig.value));
#else
    std::cout << "  Note: Type signature display disabled on MSVC\n";
    std::cout << "  Size/alignment validation still enforced at compile-time\n";
#endif
    std::cout << "\n";
    
    print_subsection("Key Features");
    print_check("Binary compatibility guarantee");
    print_check("Compile-time size/alignment validation");
    print_check("Protection against layout corruption");
    print_check("Type-safe offset computation");
}

// Demo 5: Performance Characteristics

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
    
    print_subsection("Performance Benchmark");
    XBufferExt xbuf(65536);  // 64KB
    
#if HAS_CHRONO
    auto start = std::chrono::high_resolution_clock::now();
#endif
    
    auto* game = xbuf.make<GameData>("perf_test");
    for (int i = 0; i < 1000; i++) {
        std::string item_name = "Item_" + std::to_string(i);
        game->items.emplace_back(
            xbuf.allocator<Item>(),
            i,                  // item_id
            i % 3,              // item_type
            i,                  // quantity
            item_name.c_str()   // name
        );
    }
    
#if HAS_CHRONO
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    print_info("Items Inserted", "1000");
    print_info("Total Time", std::to_string(duration.count()) + " μs");
    print_info("Avg/Item", std::to_string(duration.count() / 1000.0) + " μs");
#else
    print_info("Items Inserted", "1000");
    print_info("Note", "Timing unavailable (chrono not supported)");
#endif
}

// Demo 6: Advanced Features

void demo_advanced_features() {
    print_section("6. Advanced Features");
    
    print_subsection("Container Types");
    print_check("XVector<T> - Dynamic array");
    print_check("XSet<T> - Flat set (unique elements)");
    print_check("XMap<K,V> - Flat map (key-value pairs)");
    print_check("XString - Offset-aware string");
    
    print_subsection("Type System");
    print_check("Compile-time type signatures");
    print_check("Automatic size/offset computation");
    print_check("Cross-compilation compatibility");
    print_check("Boost.PFR-based reflection");
    
    print_subsection("Memory Features");
    print_check("Dynamic growth/shrinking");
    print_check("Memory usage tracking");
    print_check("Zero-copy serialization");
    
    print_subsection("Roadmap");
    std::cout << "  - C++26 reflection support\n";
    std::cout << "  - Schema versioning\n";
    std::cout << "  - JSON serialization\n";
    std::cout << "  - Benchmark suite\n";
}

// Main Entry Point

int main() {
    std::cout << R"(
+====================================================================+
|         XOffsetDatastructure2 - Feature Demonstration            |
|    Offset-Based Data Structures with Type Safety System          |
+====================================================================+
)";

    try {
        demo_basic_usage();
        demo_memory_management();
        demo_serialization();
        demo_type_signatures();
        demo_performance();
        demo_advanced_features();
        
        print_section("Summary");
        std::cout << "\n  ✓ All demonstrations completed successfully!\n\n";
        std::cout << "  Resources:\n";
        std::cout << "    - Documentation: docs/\n";
        std::cout << "    - Examples: examples/\n";
        std::cout << "    - Tests: tests/\n\n";
        std::cout << "  Key Features:\n";
        std::cout << "    • Type-safe offset-based containers\n";
        std::cout << "    • Zero-copy binary serialization\n";
        std::cout << "    • Compile-time type verification\n";
        std::cout << "    • Memory-efficient growth (1.1x)\n\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] " << e.what() << "\n";
        return 1;
    }
}
