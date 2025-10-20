// ============================================================================
// Test: Nested Struct Allocator Test
// Purpose: Test nested structures with XString
// ============================================================================

#include <iostream>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// Nested struct with XString
struct NestedItem {
    template <typename Allocator>
    NestedItem(Allocator allocator) : name(allocator) {}
    
    template <typename Allocator>
    NestedItem(Allocator allocator, int id_val, const char* name_val)
        : id(id_val), name(name_val, allocator) {}
    
    int id{0};
    XString name;
};

// Outer struct containing vector of nested structs
struct OuterStruct {
    template <typename Allocator>
    OuterStruct(Allocator allocator)
        : label(allocator),
          nested_items(allocator) {}
    
    XString label;
    XVector<NestedItem> nested_items;
};

int main() {
    std::cout << "\n[TEST] Nested Struct Allocator Test\n\n";
    
    try {
        XBufferExt xbuf(4096);
        
        std::cout << "1. Creating OuterStruct... ";
        auto* outer = xbuf.make<OuterStruct>("outer");
        std::cout << "[OK]\n";
        
        std::cout << "2. Setting label... ";
        outer->label = XString("TestLabel", xbuf.allocator<XString>());
        std::cout << "[OK]\n";
        
        std::cout << "3. Adding nested items... ";
        for (int i = 0; i < 3; i++) {
            std::string item_name = "Item" + std::to_string(i);
            outer->nested_items.emplace_back(
                xbuf.allocator<NestedItem>(),
                i,
                item_name.c_str()
            );
        }
        std::cout << "[OK]\n";
        
        std::cout << "4. Verifying nested items... ";
        assert(outer->nested_items.size() == 3);
        assert(outer->nested_items[0].name == "Item0");
        std::cout << "[OK]\n";
        
        std::cout << "\n[PASS] All tests successful!\n\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}
