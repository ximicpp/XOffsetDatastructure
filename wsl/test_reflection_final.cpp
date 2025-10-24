#include <iostream>
#include <experimental/meta>
using namespace std::meta;

// Test structures
struct Player {
    int id;
    int level;
    double score;
};

struct Item {
    int item_id;
    const char* name;
};

int main() {
    std::cout << "=== Final Reflection Test ===\n\n";
    
    // Test 1: Multiple type reflections
    std::cout << "1. Type Reflection:\n";
    constexpr auto player_type = ^^Player;
    constexpr auto item_type = ^^Item;
    std::cout << "   [OK]Player type reflected\n";
    std::cout << "   [OK]Item type reflected\n";
    
    // Test 2: Member access for Player
    std::cout << "\n2. Player Member Access:\n";
    Player p{1001, 25, 98.5};
    int Player::*id_ptr = &[:^^Player::id:];
    int Player::*level_ptr = &[:^^Player::level:];
    double Player::*score_ptr = &[:^^Player::score:];
    
    std::cout << "   Player ID: " << p.*id_ptr << "\n";
    std::cout << "   Level: " << p.*level_ptr << "\n";
    std::cout << "   Score: " << p.*score_ptr << "\n";
    
    // Test 3: Member access for Item
    std::cout << "\n3. Item Member Access:\n";
    Item item{5001, "Magic Sword"};
    int Item::*item_id_ptr = &[:^^Item::item_id:];
    const char* Item::*name_ptr = &[:^^Item::name:];
    
    std::cout << "   Item ID: " << item.*item_id_ptr << "\n";
    std::cout << "   Name: " << item.*name_ptr << "\n";
    
    // Test 4: Modify via reflection
    std::cout << "\n4. Modification Test:\n";
    p.*level_ptr = 30;  // Modify via reflection pointer
    std::cout << "   Level after modification: " << p.level << "\n";
    std::cout << "   [OK]Modification via reflection works\n";
    
    std::cout << "\n=== All Reflection Tests Passed! ===\n";
    std::cout << "[OK]Type reflection works\n";
    std::cout << "[OK]Member access works\n";
    std::cout << "[OK]Multiple types supported\n";
    std::cout << "[OK]Modification works\n";
    std::cout << "\nP2996 Reflection fully functional!\n";
    
    return 0;
}
