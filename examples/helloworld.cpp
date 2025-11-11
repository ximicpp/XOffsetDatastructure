// ============================================================================
// XOffsetDatastructure2 - Hello World
// Purpose: Simple introduction to basic usage with Player
// ============================================================================

#include <iostream>
#include "../xoffsetdatastructure2.hpp"
#include "../generated/player.hpp"

using namespace XOffsetDatastructure2;

int main() {
    // Create buffer
    XBufferExt xbuf(4096);
    
    // Create a player
    auto* player = xbuf.make<Player>("Hero");
    if (!player) {
        std::cerr << "Player creation failed\n";
        return 1;
    }
    
    // Set player information
    player->name = XString("Alice", xbuf.allocator<XString>());
    player->id = 1;
    player->level = 10;
    
    // Add items
    player->items.push_back(101);
    player->items.push_back(102);
    player->items.push_back(103);
    
    std::cout << "Player: " << player->name.c_str() 
              << " (ID: " << player->id 
              << ", Level: " << player->level << ")\n";
    std::cout << "Items: " << player->items.size() << "\n";
    
    // Serialize
    auto data = xbuf.save_to_string();
    std::cout << "Serialized: " << data.size() << " bytes\n";
    
    // Deserialize
    XBufferExt loaded = XBufferExt::load_from_string(data);
    auto [loaded_player, found] = loaded.find_ex<Player>("Hero");
    
    if (found) {
        std::cout << "Deserialized: " << loaded_player->name.c_str()
                  << " (Level " << loaded_player->level 
                  << ", Items: " << loaded_player->items.size() << ")\n";
    }
    
    // Memory stats
    auto stats = xbuf.stats();
    std::cout << "Memory: " << stats.used_size << "/" << stats.total_size 
              << " bytes (" << static_cast<int>(stats.usage_percent()) << "%)\n";
    
    return 0;
}
