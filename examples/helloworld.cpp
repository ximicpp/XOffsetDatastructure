// ============================================================================
// XOffsetDatastructure2 Hello World Demo
// Purpose: Simple introduction to basic usage
// ============================================================================

#include <iostream>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// Simple data structure with 64-bit alignment
struct alignas(BASIC_ALIGNMENT) Player {
    template <typename Allocator>
    Player(Allocator allocator) : name(allocator), items(allocator) {}
    
    int id;
    int level;
    XString name;
    XVector<int> items;
};

int main() {
    std::cout << "\n=== XOffsetDatastructure2 Hello World ===\n\n";
    
    // 1. Create buffer
    std::cout << "1. Creating buffer...\n";
    XBufferExt xbuf(4096);
    
    // 2. Create an object
    std::cout << "2. Creating player...\n";
    auto* player = xbuf.make<Player>("Hero");
    player->id = 1;
    player->level = 10;
    player->name = xbuf.make<XString>("Alice");
    
    // 3. Add some data
    std::cout << "3. Adding items...\n";
    player->items.push_back(101);
    player->items.push_back(102);
    player->items.push_back(103);
    
    // 4. Display data
    std::cout << "4. Player info:\n";
    std::cout << "   Name: " << player->name << "\n";
    std::cout << "   ID: " << player->id << ", Level: " << player->level << "\n";
    std::cout << "   Items: ";
    for (int item : player->items) {
        std::cout << item << " ";
    }
    std::cout << "\n";
    
    // 5. Serialize to string
    std::cout << "\n5. Serializing...\n";
    auto data = xbuf.save_to_string();
    std::cout << "   Serialized size: " << data.size() << " bytes\n";
    
    // 6. Deserialize from string
    std::cout << "6. Deserializing...\n";
    XBufferExt loaded = XBufferExt::load_from_string(data);
    auto [loaded_player, found] = loaded.find_ex<Player>("Hero");
    
    if (found) {
        std::cout << "   Loaded player: " << loaded_player->name 
                  << " (Level " << loaded_player->level << ")\n";
        std::cout << "   Items count: " << loaded_player->items.size() << "\n";
    }
    
    std::cout << "\n=== Done! ===\n\n";
    return 0;
}
