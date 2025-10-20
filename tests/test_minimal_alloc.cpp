// ============================================================================
// Test: Minimal Allocator Test
// Purpose: Find the exact cause of the MSVC allocator issue
// ============================================================================

#include <iostream>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// Test 1: Simple struct with XString initialized with allocator
struct SimpleStruct1 {
    template <typename Allocator>
    SimpleStruct1(Allocator allocator) : name(allocator) {}
    
    XString name;
};

// Test 2: Complex struct like GameData
struct ComplexStruct2 {
    template <typename Allocator>
    ComplexStruct2(Allocator allocator) 
        : name(allocator),
          items(allocator),
          int_set(allocator),
          string_map(allocator) {}
    
    XString name;
    XVector<int> items;
    XSet<int> int_set;
    XMap<XString, int> string_map;
};

int main() {
    std::cout << "\n[TEST] Minimal Allocator Test\n\n";
    
    try {
        XBufferExt xbuf(4096);
        
        std::cout << "1. Creating SimpleStruct1... ";
        auto* s1 = xbuf.make<SimpleStruct1>("s1");
        std::cout << "[OK]\n";
        
        std::cout << "2. Assigning to SimpleStruct1::name... ";
        s1->name = XString("Test", xbuf.allocator<XString>());
        std::cout << "[OK]\n";
        
        std::cout << "3. Creating ComplexStruct2... ";
        auto* s2 = xbuf.make<ComplexStruct2>("s2");
        std::cout << "[OK]\n";
        
        std::cout << "4. Assigning to ComplexStruct2::name... ";
        s2->name = XString("Complex", xbuf.allocator<XString>());
        std::cout << "[OK]\n";
        
        std::cout << "5. Adding to string_map (lvalue)... ";
        XString key("key1", xbuf.allocator<XString>());
        s2->string_map.emplace(key, 100);
        std::cout << "[OK]\n";
        
        std::cout << "6. Adding to string_map (temporary)... ";
        s2->string_map.emplace(XString("key2", xbuf.allocator<XString>()), 200);
        std::cout << "[OK]\n";
        
        std::cout << "\n[PASS] All tests successful!\n\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}
