// ============================================================================
// Test: Demo Reproduction
// Purpose: Reproduce the exact scenario from demo.cpp that fails
// ============================================================================

#include <iostream>
#include "../xoffsetdatastructure2.hpp"
#include "../generated/game_data.hpp"

using namespace XOffsetDatastructure2;

int main() {
    std::cout << "\n[TEST] Demo Scenario Reproduction\n\n";
    
    try {
        XBufferExt xbuf(4096);
        
        std::cout << "1. Creating GameData... ";
        auto* game = xbuf.make<GameData>("player_save");
        std::cout << "[OK]\n";
        
        std::cout << "2. Setting player_name... ";
        game->player_name = XString("Hero", xbuf.allocator<XString>());
        std::cout << "[OK]\n";
        
        std::cout << "3. Setting other fields... ";
        game->player_id = 12345;
        game->level = 42;
        game->health = 100.0f;
        std::cout << "[OK]\n";
        
        std::cout << "4. Adding items... ";
        for (int i = 0; i < 3; i++) {
            std::string item_name = "Item" + std::to_string(i);
            game->items.emplace_back(
                xbuf.allocator<Item>(),
                i, i % 3, i * 10,
                item_name.c_str()
            );
        }
        std::cout << "[OK]\n";
        
        std::cout << "5. Adding achievements... ";
        for (int i = 0; i < 5; i++) {
            game->achievements.insert(i);
        }
        std::cout << "[OK]\n";
        
        std::cout << "6. Adding quest progress (lvalue)... ";
        XString quest1("Quest1", xbuf.allocator<XString>());
        game->quest_progress.emplace(quest1, 75);
        std::cout << "[OK]\n";
        
        std::cout << "7. Adding quest progress (rvalue)... ";
        XString quest2("Quest2", xbuf.allocator<XString>());
        game->quest_progress.emplace(std::move(quest2), 100);
        std::cout << "[OK]\n";
        
        std::cout << "8. Adding quest progress (temporary)... ";
        game->quest_progress.emplace(XString("Quest3", xbuf.allocator<XString>()), 50);
        std::cout << "[OK]\n";
        
        std::cout << "\n[PASS] All operations successful!\n\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}
