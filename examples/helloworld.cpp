// ============================================================================
// XOffsetDatastructure2 - Hello World
// Purpose: Quick start guide showing basic usage in 5 steps
// ============================================================================

#include <iostream>
#include "../xoffsetdatastructure2.hpp"
#include "../generated/player.hpp"

using namespace XOffsetDatastructure2;

int main() {
    std::cout << "=== XOffsetDatastructure2 Quick Start ===\n\n";
    
    // Step 1: Create a memory buffer
    std::cout << "[Step 1] Create buffer (4KB)\n";
    XBufferExt xbuf(4096);
    
    // Step 2: Create and initialize a Player object
    std::cout << "[Step 2] Create player object\n";
    auto* player = xbuf.make<Player>("Hero");
    
    player->name = XString("Alice", xbuf.allocator<XString>());
    player->id = 1001;
    player->level = 10;
    player->items.push_back(101);  // Sword
    player->items.push_back(102);  // Shield
    player->items.push_back(103);  // Potion
    
    std::cout << "  Player: " << player->name.c_str() 
              << " | ID: " << player->id 
              << " | Level: " << player->level 
              << " | Items: " << player->items.size() << "\n\n";
    
    // Step 3: Serialize to binary (zero-encoding/decoding)
    std::cout << "[Step 3] Serialize to binary (zero-encoding/decoding)\n";
    auto data = xbuf.save_to_string();
    std::cout << "  Binary size: " << data.size() << " bytes\n\n";
    
    // Step 4: Deserialize from binary
    std::cout << "[Step 4] Deserialize from binary\n";
    XBufferExt loaded = XBufferExt::load_from_string(data);
    auto [loaded_player, found] = loaded.find_ex<Player>("Hero");
    
    std::cout << "  Loaded: " << loaded_player->name.c_str()
              << " | Level: " << loaded_player->level 
              << " | Items: " << loaded_player->items.size() << "\n\n";
    
    // Step 5: Check memory usage
    std::cout << "[Step 5] Memory statistics\n";
    auto stats = xbuf.stats();
    std::cout << "  Used: " << stats.used_size << " / " << stats.total_size 
              << " bytes (" << static_cast<int>(stats.usage_percent()) << "%)\n\n";
    
    std::cout << "=== Quick Start Complete! ===\n";
    return 0;
}
