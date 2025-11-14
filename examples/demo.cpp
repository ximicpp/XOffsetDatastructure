// XOffsetDatastructure2 Demo

#include <iostream>
#include <string>
#include "../xoffsetdatastructure2.hpp"
#include "../generated/game_data.hpp"

using namespace XOffsetDatastructure2;

void demo_basic_usage() {
    std::cout << "\n--- Basic Container Operations ---\n\n";
    
    XBuffer xbuf(4096);
    auto* game = xbuf.make<GameData>("player_save");
    
    game->player_name = XString("Hero", xbuf.allocator<XString>());
    game->player_id = 12345;
    game->level = 42;
    game->health = 100.0f;
    
    std::cout << "Player: " << game->player_name.c_str() 
              << " (ID " << game->player_id << ", level " << game->level 
              << ", HP " << game->health << ")\n\n";
    
    // Add items (XVector)
    const char* types[] = {"Potion", "Weapon", "Armor"};
    for (int i = 0; i < 5; i++) {
        game->items.emplace_back(xbuf.allocator<Item>(),
            i + 1, i % 3, (i + 1) * 10, ("Item_" + std::to_string(i+1)).c_str());
    }
    std::cout << "Inventory (" << game->items.size() << " items):\n";
    for (const auto& item : game->items) {
        std::cout << "  " << item.name.c_str() << " x" << item.quantity 
                  << " (" << types[item.item_type] << ")\n";
    }
    
    // Track achievements (XSet)
    for (int id : {1, 5, 10, 25, 50})
        game->achievements.insert(id);
    std::cout << "\nAchievements (" << game->achievements.size() << " unlocked): ";
    for (int id : game->achievements)
        std::cout << id << " ";
    std::cout << "\n";
    
    // Quest progress (XMap)
    game->quest_progress[XString("Main Quest", xbuf.allocator<XString>())] = 75;
    game->quest_progress[XString("Side Quest", xbuf.allocator<XString>())] = 100;
    std::cout << "\nQuests:\n";
    for (const auto& [quest, progress] : game->quest_progress)
        std::cout << "  " << quest.c_str() << ": " << progress << "%\n";
}

void demo_memory_management() {
    std::cout << "\n--- Memory Management ---\n\n";
    
    XBuffer xbuf(1024);
    auto stats = xbuf.stats();
    std::cout << "Initial: " << stats.used_size << "/" << stats.total_size 
              << " bytes (" << static_cast<int>(stats.usage_percent()) << "%)\n";
    
    auto* game = xbuf.make<GameData>("game");
    game->player_name = XString("Player1", xbuf.allocator<XString>());
    for (int i = 0; i < 100; i++)
        game->achievements.insert(i);
    
    stats = xbuf.stats();
    std::cout << "After adding data: " << stats.used_size << "/" << stats.total_size 
              << " bytes (" << static_cast<int>(stats.usage_percent()) << "%)\n";
    
    xbuf.grow(4096);
    stats = xbuf.stats();
    std::cout << "After grow: " << stats.used_size << "/" << stats.total_size 
              << " bytes (" << static_cast<int>(stats.usage_percent()) << "%)\n";
    
    xbuf.shrink_to_fit();
    stats = xbuf.stats();
    std::cout << "After shrink: " << stats.used_size << "/" << stats.total_size 
              << " bytes (" << static_cast<int>(stats.usage_percent()) << "%)\n";
}

void demo_serialization() {
    std::cout << "\n--- Serialization (zero-encoding/decoding) ---\n\n";
    
    XBuffer src_buf(2048);
    auto* src = src_buf.make<GameData>("save");
    src->player_name = XString("SavedHero", src_buf.allocator<XString>());
    src->player_id = 99999;
    src->level = 99;
    src->health = 100.0f;
    
    for (int i = 0; i < 3; i++) {
        src->items.emplace_back(src_buf.allocator<Item>(),
            i, 0, 1, ("Item_" + std::to_string(i)).c_str());
    }
    
    std::cout << "Original: " << src->player_name.c_str() 
              << " (level " << src->level << ", " << src->items.size() << " items)\n";
    
    std::string data = src_buf.save_to_string();
    std::cout << "Saved to " << data.size() << " bytes\n";
    
    XBuffer dst_buf = XBuffer::load_from_string(data);
    auto* dst = dst_buf.find<GameData>("save").first;
    
    std::cout << "Loaded: " << dst->player_name.c_str() 
              << " (level " << dst->level << ", " << dst->items.size() << " items)\n";
    
    bool ok = (std::string(dst->player_name.c_str()) == "SavedHero" &&
               dst->player_id == 99999 && dst->level == 99 && dst->items.size() == 3);
    std::cout << "Data integrity: " << (ok ? "OK" : "FAIL") << "\n";
}

int main() {
    try {
        demo_basic_usage();
        demo_memory_management();
        demo_serialization();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
