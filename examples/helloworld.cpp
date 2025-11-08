// ============================================================================
// XOffsetDatastructure2 - Hello World
// Purpose: Introduction to basic usage with visual enhancements
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
    
    // Check buffer creation
    if (xbuf.empty()) {
        std::cerr << "    ✗ Buffer creation failed\n";
        return 1;
    }
    
    std::cout << "    ✓ Buffer initialized\n";
    
    // Get buffer statistics
    BufferStats stats = xbuf.get_stats();
    std::cout << "    Total: " << stats.total_size << " bytes\n";
    std::cout << "    Free:  " << stats.free_size << " bytes\n";
    
    // 2. Create an object
    std::cout << "\n[2] Creating game data\n";
    auto game = xbuf.make<GameData>("Game1");
    
    // Check object creation
    if (!game) {
        std::cerr << "    ✗ GameData creation failed\n";
        return 1;
    }
    
    std::cout << "    ✓ GameData created\n";
    
    // 3. Set player information
    std::cout << "\n[3] Setting player info\n";
    game->player_name = XString("Alice", xbuf.allocator<XString>());
    game->player_id = 1;
    game->level = 10;
    game->health = 100;
    
    std::cout << "    Name:   " << game->player_name.c_str() << "\n";
    std::cout << "    ID:     " << game->player_id << "\n";
    std::cout << "    Level:  " << game->level << "\n";
    std::cout << "    Health: " << game->health << "\n";
    
    // 4. Add items to inventory
    std::cout << "\n[4] Adding items\n";
    game->items.push_back(Item{"Sword", 1, 10});
    game->items.push_back(Item{"Shield", 1, 20});
    game->items.push_back(Item{"Potion", 2, 30});
    
    std::cout << "    ✓ Added " << game->items.size() << " items\n";
    
    // 5. Display inventory
    std::cout << "\n[5] Inventory\n";
    for (size_t i = 0; i < game->items.size(); i++) {
        std::cout << "    [" << i+1 << "] " 
                  << game->items[i].name.c_str()
                  << " x" << game->items[i].quantity << "\n";
    }
    
    // 6. Show memory usage
    std::cout << "\n[6] Memory usage\n";
    stats = xbuf.get_stats();
    std::cout << "    Used:  " << stats.used_size << " bytes\n";
    std::cout << "    Free:  " << stats.free_size << " bytes\n";
    std::cout << "    Total: " << stats.total_size << " bytes\n";
    std::cout << "    Usage: " << static_cast<int>(stats.usage_percent()) << "%\n";
    
    // 7. Show type signature
    std::cout << "\n[7] Type signature\n";
    std::cout << "    ✓ Computed at compile-time\n";
    std::cout << "    ✓ Ensures binary compatibility\n";
    std::cout << "    GameData: struct[s:" << sizeof(GameDataReflectionHint)
              << ",a:" << alignof(GameDataReflectionHint) << "]\n";
    
    // Final summary
    std::cout << "\n+==================================================================+\n";
    std::cout << "|                     ✓ Example Complete                         |\n";
    std::cout << "+==================================================================+\n";
    std::cout << "\nKey Takeaways:\n";
    std::cout << "  • XBufferExt manages offset-based memory\n";
    std::cout << "  • make<T>() creates named objects\n";
    std::cout << "  • Containers work seamlessly with allocators\n";
    std::cout << "  • Type signatures ensure compile-time safety\n";
    std::cout << "  • Zero-copy serialization is supported\n\n";
    
    return 0;
}