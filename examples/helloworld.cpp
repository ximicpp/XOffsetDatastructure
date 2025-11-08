// ============================================================================
// XOffsetDatastructure2 - Hello World
// Purpose: Simple introduction to basic usage with Player
// ============================================================================

#include <iostream>
#include "../xoffsetdatastructure2.hpp"
#include "../generated/player.hpp"

using namespace XOffsetDatastructure2;

int main() {
    std::cout << R"(
+==================================================================+
|         XOffsetDatastructure2 - Hello World Example           |
+==================================================================+
)" << std::endl;
    
    // 1. Create buffer
    std::cout << "\n[1] Creating XBufferExt (4KB)\n";
    XBufferExt xbuf(4096);
    std::cout << "    ✓ Buffer initialized\n";
    
    // Get buffer statistics
    auto stats = xbuf.stats();
    std::cout << "    Total: " << stats.total_size << " bytes\n";
    std::cout << "    Free:  " << stats.free_size << " bytes\n";
    
    // 2. Create a player
    std::cout << "\n[2] Creating player\n";
    auto* player = xbuf.make<Player>("Hero");
    
    // Check object creation
    if (!player) {
        std::cerr << "    ✗ Player creation failed\n";
        return 1;
    }
    
    std::cout << "    ✓ Player created\n";
    
    // 3. Set player information
    std::cout << "\n[3] Setting player info\n";
    player->name = XString("Alice", xbuf.allocator<XString>());
    player->id = 1;
    player->level = 10;
    
    std::cout << "    Name:   " << player->name.c_str() << "\n";
    std::cout << "    ID:     " << player->id << "\n";
    std::cout << "    Level:  " << player->level << "\n";
    
    // 4. Add items (item IDs)
    std::cout << "\n[4] Adding items\n";
    player->items.push_back(101);
    player->items.push_back(102);
    player->items.push_back(103);
    
    std::cout << "    ✓ Added " << player->items.size() << " items\n";
    
    // 5. Display items
    std::cout << "\n[5] Item IDs\n";
    for (size_t i = 0; i < player->items.size(); i++) {
        std::cout << "    [" << i+1 << "] Item ID: " 
                  << player->items[i] << "\n";
    }
    
    // 6. Serialize to string
    std::cout << "\n[6] Serialization\n";
    auto data = xbuf.save_to_string();
    std::cout << "    Serialized size: " << data.size() << " bytes\n";
    
    // 7. Deserialize from string
    std::cout << "\n[7] Deserialization\n";
    XBufferExt loaded = XBufferExt::load_from_string(data);
    auto [loaded_player, found] = loaded.find_ex<Player>("Hero");
    
    if (found) {
        std::cout << "    ✓ Loaded player: " << loaded_player->name.c_str()
                  << " (Level " << loaded_player->level << ")\n";
        std::cout << "    ✓ Items count: " << loaded_player->items.size() << "\n";
    } else {
        std::cerr << "    ✗ Failed to find player\n";
    }
    
    // 8. Show memory usage
    std::cout << "\n[8] Memory usage\n";
    stats = xbuf.stats();
    std::cout << "    Used:  " << stats.used_size << " bytes\n";
    std::cout << "    Free:  " << stats.free_size << " bytes\n";
    std::cout << "    Total: " << stats.total_size << " bytes\n";
    std::cout << "    Usage: " << static_cast<int>(stats.usage_percent()) << "%\n";
    
    // 9. Show type signature
    std::cout << "\n[9] Type signature\n";
    std::cout << "    ✓ Computed at compile-time\n";
    std::cout << "    ✓ Ensures binary compatibility\n";
    std::cout << "    Player: struct[s:" << sizeof(PlayerReflectionHint)
              << ",a:" << alignof(PlayerReflectionHint) << "]\n";
    
    // Final summary
    std::cout << "\n+==================================================================+\n";
    std::cout << "|                     ✓ Example Complete                         |\n";
    std::cout << "+==================================================================+\n";
    std::cout << "\nKey Takeaways:\n";
    std::cout << "  • XBufferExt manages offset-based memory\n";
    std::cout << "  • make<T>() creates named objects\n";
    std::cout << "  • Containers (XVector, XString) work seamlessly\n";
    std::cout << "  • Type signatures ensure compile-time safety\n";
    std::cout << "  • Zero-copy serialization is supported\n\n";
    
    return 0;
}
