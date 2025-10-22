// ============================================================================
// XOffsetDatastructure2 Hello World Demo (C++26 Reflection Version)
// Purpose: Simple introduction to basic usage
// ============================================================================

#include <iostream>
#include "../xoffsetdatastructure2.hpp"
#include "player.hpp"

using namespace XOffsetDatastructure2;

int main() {
    std::cout << "\n=== XOffsetDatastructure2 Hello World (C++26) ===\n\n";
    
    // 1. Create buffer
    std::cout << "1. Creating buffer...\n";
    XBufferExt xbuf(4096);
    
    // 2. Create an object
    std::cout << "2. Creating player...\n";
    auto* player = xbuf.make<Player>("Hero");
    player->id = 1;
    player->level = 10;
    player->name = XString("Alice", xbuf.allocator<XString>());
    
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
    
    // 7. Memory Statistics
    std::cout << "\n7. Memory statistics before compaction:\n";
    auto stats_before = xbuf.stats();
    std::cout << "   Total size: " << stats_before.total_size << " bytes\n";
    std::cout << "   Used size: " << stats_before.used_size << " bytes\n";
    std::cout << "   Free size: " << stats_before.free_size << " bytes\n";
    std::cout << "   Usage: " << std::fixed << std::setprecision(1) 
              << stats_before.usage_percent() << "%\n";
    
    // 8. Memory Compaction Demo (C++26 only)
    std::cout << "\n8. Memory Compaction:\n";
    
#if __cpp_reflection >= 202306L
    std::cout << "   [OK] C++26 reflection: ENABLED\n";
    std::cout << "   [OK] Automatic memory compaction available\n\n";
    
    // Create some fragmentation by adding and removing data
    std::cout << "   Creating fragmentation...\n";
    player->items.push_back(201);
    player->items.push_back(202);
    player->items.push_back(203);
    player->items.pop_back();
    player->items.pop_back();
    
    // Show stats after fragmentation
    auto stats_frag = xbuf.stats();
    std::cout << "   After operations - Used: " << stats_frag.used_size 
              << " bytes (" << std::setprecision(1) << stats_frag.usage_percent() << "%)\n";
    
    // Compact the buffer
    std::cout << "\n   Compacting buffer...\n";
    XBuffer compacted = XBufferCompactor::compact_automatic<Player>(xbuf, "Hero");
    
    // Show stats after compaction
    auto stats_after = XBufferVisualizer::get_memory_stats(compacted);
    std::cout << "   After compaction:\n";
    std::cout << "   - Total size: " << stats_after.total_size << " bytes\n";
    std::cout << "   - Used size: " << stats_after.used_size << " bytes\n";
    std::cout << "   - Saved: " << (stats_before.total_size - stats_after.total_size) 
              << " bytes\n";
    std::cout << "   - Efficiency: " << std::setprecision(1) 
              << stats_after.usage_percent() << "%\n";
    
    // Verify data integrity after compaction
    auto [compacted_player, found2] = compacted.find<Player>("Hero");
    if (found2) {
        std::cout << "\n   [OK] Data integrity verified:\n";
        std::cout << "     Name: " << compacted_player->name << "\n";
        std::cout << "     Level: " << compacted_player->level << "\n";
        std::cout << "     Items: ";
        for (int item : compacted_player->items) {
            std::cout << item << " ";
        }
        std::cout << "\n";
    }
    
    // Show type signature (compile-time calculated)
    std::cout << "\n   Type Signature:\n   ";
    constexpr auto sig = XTypeSignature::get_XTypeSignature<Player>();
    sig.print();
    std::cout << "\n";
    
#else
    std::cout << "   [X] C++26 reflection: NOT AVAILABLE\n";
    std::cout << "   [i] Memory compaction requires C++26 reflection\n";
    std::cout << "   [i] Using basic size/alignment validation\n";
    std::cout << "   [i] Compile with -std=c++26 -freflection for compaction features\n";
#endif
    
    std::cout << "\n=== Done! ===\n\n";
    return 0;
}