// XOffsetDatastructure2 Hello World Example

#include <iostream>
#include "../xoffsetdatastructure2.hpp"
#include "../generated/player.hpp"

using namespace XOffsetDatastructure2;

int main() {
    // Create buffer
    XBufferExt xbuf(4096);
    
    // Create player
    auto* player = xbuf.make<Player>("Hero");
    player->name = XString("Alice", xbuf.allocator<XString>());
    player->id = 1001;
    player->level = 10;
    player->items.push_back(101);
    player->items.push_back(102);
    player->items.push_back(103);
    
    std::cout << "Created player: " << player->name.c_str() 
              << " (ID " << player->id << ", level " << player->level 
              << ", " << player->items.size() << " items)\n";
    
    // Serialize
    auto data = xbuf.save_to_string();
    std::cout << "Serialized to " << data.size() << " bytes\n";
    
    // Deserialize
    XBufferExt loaded = XBufferExt::load_from_string(data);
    auto [loaded_player, found] = loaded.find_ex<Player>("Hero");
    std::cout << "Loaded player: " << loaded_player->name.c_str()
              << " (level " << loaded_player->level << ")\n";
    
    // Check memory
    auto stats = xbuf.stats();
    std::cout << "Memory: " << stats.used_size << "/" << stats.total_size 
              << " bytes (" << static_cast<int>(stats.usage_percent()) << "%)\n";
    
    return 0;
}
