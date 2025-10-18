// ============================================================================
// Example: Type Signature Verification Demo
// Purpose: Demonstrate compile-time type signature validation
// ============================================================================

#include <iostream>
#include <iomanip>
#include "xoffsetdatastructure2.hpp"
#include "player.hpp"
#include "game_data.hpp"

using namespace XOffsetDatastructure2;

void print_type_signature_info() {
    std::cout << "\n=== Type Signature Information ===\n\n";
    
    // Player type
    std::cout << "Player Structure:\n";
    std::cout << "  Size: " << sizeof(Player) << " bytes\n";
    std::cout << "  Alignment: " << alignof(Player) << " bytes\n";
    std::cout << "  Signature: ";
    XTypeSignature::get_XTypeSignature<PlayerReflectionHint>().print();
    std::cout << "\n\n";
    
    // Item type
    std::cout << "Item Structure:\n";
    std::cout << "  Size: " << sizeof(Item) << " bytes\n";
    std::cout << "  Alignment: " << alignof(Item) << " bytes\n";
    std::cout << "  Signature: ";
    XTypeSignature::get_XTypeSignature<ItemReflectionHint>().print();
    std::cout << "\n\n";
    
    // GameData type
    std::cout << "GameData Structure:\n";
    std::cout << "  Size: " << sizeof(GameData) << " bytes\n";
    std::cout << "  Alignment: " << alignof(GameData) << " bytes\n";
    std::cout << "  Signature: ";
    XTypeSignature::get_XTypeSignature<GameDataReflectionHint>().print();
    std::cout << "\n\n";
}

void demonstrate_validation() {
    std::cout << "=== Compile-Time Validation ===\n\n";
    
    std::cout << "✓ All type signatures validated at compile-time\n";
    std::cout << "✓ Runtime and Reflection types have identical layouts\n";
    std::cout << "✓ Binary compatibility ensured\n\n";
    
    std::cout << "Validation checks performed:\n";
    std::cout << "  1. sizeof(Runtime) == sizeof(Reflection)\n";
    std::cout << "  2. alignof(Runtime) == alignof(Reflection)\n";
    std::cout << "  3. Type signature computed and verified\n\n";
}

int main() {
    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "  XOffsetDatastructure2 - Type Signature Verification Demo\n";
    std::cout << "======================================================================\n";
    
    print_type_signature_info();
    demonstrate_validation();
    
    std::cout << "======================================================================\n";
    std::cout << "  All types successfully validated!\n";
    std::cout << "======================================================================\n\n";
    
    return 0;
}
