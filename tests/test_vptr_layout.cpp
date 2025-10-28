// Test: Verify vptr in polymorphic class
#include "../xoffsetdatastructure2.hpp"
#include <iostream>

using namespace XOffsetDatastructure2;

// Non-polymorphic class
class NonPolymorphic {
public:
    int32_t data;
};

// Polymorphic class (with virtual function)
class Polymorphic {
public:
    int32_t data;
    virtual void func() {}
    virtual ~Polymorphic() = default;
};

int main() {
    std::cout << "========================================\n";
    std::cout << "Virtual Function Memory Layout Test\n";
    std::cout << "========================================\n\n";
    
    // Test 1: Non-polymorphic class
    std::cout << "1. NonPolymorphic class:\n";
    std::cout << "   sizeof:  " << sizeof(NonPolymorphic) << " bytes\n";
    std::cout << "   alignof: " << alignof(NonPolymorphic) << " bytes\n";
    
    constexpr auto sig_non = XTypeSignature::get_XTypeSignature<NonPolymorphic>();
    std::cout << "   Signature: ";
    sig_non.print();
    std::cout << "\n\n";
    
    // Test 2: Polymorphic class
    std::cout << "2. Polymorphic class:\n";
    std::cout << "   sizeof:  " << sizeof(Polymorphic) << " bytes\n";
    std::cout << "   alignof: " << alignof(Polymorphic) << " bytes\n";
    
    constexpr auto sig_poly = XTypeSignature::get_XTypeSignature<Polymorphic>();
    std::cout << "   Signature: ";
    sig_poly.print();
    std::cout << "\n\n";
    
    // Analysis
    std::cout << "========================================\n";
    std::cout << "Analysis:\n";
    std::cout << "========================================\n";
    std::cout << "Non-polymorphic:\n";
    std::cout << "  - Size: " << sizeof(NonPolymorphic) << " bytes (just int32_t)\n";
    std::cout << "  - Expected signature: struct[s:4,a:4]{@0:i32[s:4,a:4]}\n";
    std::cout << "  - data at offset 0 [OK]\n\n";
    
    std::cout << "Polymorphic:\n";
    std::cout << "  - Size: " << sizeof(Polymorphic) << " bytes (int32_t + vptr)\n";
    std::cout << "  - Memory layout:\n";
    std::cout << "      [0-7]:  vptr  (8 bytes, vtable pointer)\n";
    std::cout << "      [8-11]: data  (4 bytes, int32_t)\n";
    std::cout << "      [12-15]: padding (4 bytes)\n\n";
    
    std::cout << "[Problem]:\n";
    std::cout << "  - Reflection only sees explicit data members (not vptr)\n";
    std::cout << "  - Signature shows: struct[s:16,a:8]{@8:i32[s:4,a:4]}\n";
    std::cout << "  - Missing info: vptr at @0 (8 bytes)\n";
    std::cout << "  - This is MISLEADING - offsets are correct but incomplete!\n\n";
    
    std::cout << "[Impact]:\n";
    std::cout << "  1. Type signature is INCOMPLETE for polymorphic classes\n";
    std::cout << "  2. Missing vptr could break cross-platform checks\n";
    std::cout << "  3. Size/alignment correct, but field list incomplete\n\n";
    
    std::cout << "[Recommendation]:\n";
    std::cout << "  - Document: \"Polymorphic classes include compiler vptr\"\n";
    std::cout << "  - Add metadata: is_polymorphic<T> check\n";
    std::cout << "  - Warning: Don't use polymorphic types in XBuffer!\n\n";
    
    std::cout << "[Note]:\n";
    std::cout << "  Virtual functions are NOT recommended for serialization.\n";
    std::cout << "  vtable pointers are compiler/platform specific.\n";
    std::cout << "  Use plain POD types for cross-platform compatibility.\n";
    
    return 0;
}
