// ============================================================================
// Test: Memory Statistics
// Purpose: Test memory visualization and statistics
// ============================================================================

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

struct alignas(BASIC_ALIGNMENT) MemoryTestType {
    template <typename Allocator>
    MemoryTestType(Allocator allocator) 
        : data(allocator), strings(allocator) {}
    
    int value;
    XVector<int> data;
    XVector<XString> strings;
};

bool test_memory_stats() {
    std::cout << "\n[TEST] Memory Statistics\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Test 1: Initial buffer stats
    std::cout << "Test 1: Initial buffer stats... ";
    XBufferExt xbuf(4096);
    auto stats1 = XBufferVisualizer::get_memory_stats(xbuf);
    assert(stats1.total_size == 4096);
    assert(stats1.free_size > 0);
    assert(stats1.used_size > 0);  // Has management overhead
    assert(stats1.usage_percent() < 10.0);  // Very low usage
    std::cout << "[OK]\n";
    std::cout << "  Total: " << stats1.total_size << " bytes\n";
    std::cout << "  Used:  " << stats1.used_size << " bytes (" 
              << stats1.usage_percent() << "%)\n";
    
    // Test 2: After object creation
    std::cout << "Test 2: After object creation... ";
    auto* obj = xbuf.make<MemoryTestType>("MemTest");
    obj->value = 42;
    auto stats2 = XBufferVisualizer::get_memory_stats(xbuf);
    assert(stats2.used_size > stats1.used_size);
    assert(stats2.free_size < stats1.free_size);
    std::cout << "[OK]\n";
    std::cout << "  Used:  " << stats2.used_size << " bytes (" 
              << stats2.usage_percent() << "%)\n";
    
    // Test 3: After adding data
    std::cout << "Test 3: After adding data... ";
    for (int i = 0; i < 100; ++i) {
        obj->data.push_back(i);
    }
    auto stats3 = XBufferVisualizer::get_memory_stats(xbuf);
    assert(stats3.used_size > stats2.used_size);
    std::cout << "[OK]\n";
    std::cout << "  Used:  " << stats3.used_size << " bytes (" 
              << stats3.usage_percent() << "%)\n";
    
    // Test 4: After adding strings
    std::cout << "Test 4: After adding strings... ";
    for (int i = 0; i < 20; ++i) {
        std::string str = "String_" + std::to_string(i);
        obj->strings.emplace_back(XString(str.c_str(), xbuf.allocator<XString>()));
    }
    auto stats4 = XBufferVisualizer::get_memory_stats(xbuf);
    assert(stats4.used_size > stats3.used_size);
    std::cout << "[OK]\n";
    std::cout << "  Used:  " << stats4.used_size << " bytes (" 
              << stats4.usage_percent() << "%)\n";
    
    // Test 5: Buffer growth
    std::cout << "Test 5: Buffer growth... ";
    std::size_t old_size = xbuf.get_size();
    xbuf.grow(4096);
    assert(xbuf.get_size() == old_size + 4096);
    auto stats5 = XBufferVisualizer::get_memory_stats(xbuf);
    assert(stats5.total_size == old_size + 4096);
    assert(stats5.free_size > stats4.free_size);
    assert(stats5.usage_percent() < stats4.usage_percent());
    std::cout << "[OK]\n";
    std::cout << "  Total: " << stats5.total_size << " bytes\n";
    std::cout << "  Used:  " << stats5.used_size << " bytes (" 
              << stats5.usage_percent() << "%)\n";
    
    // Test 6: Shrink to fit
    std::cout << "Test 6: Shrink to fit... ";
    xbuf.shrink_to_fit();
    auto stats6 = XBufferVisualizer::get_memory_stats(xbuf);
    assert(stats6.total_size < stats5.total_size);
    assert(stats6.usage_percent() > stats5.usage_percent());
    std::cout << "[OK]\n";
    std::cout << "  Total: " << stats6.total_size << " bytes\n";
    std::cout << "  Used:  " << stats6.used_size << " bytes (" 
              << stats6.usage_percent() << "%)\n";
    
    // Test 7: Verify data integrity after shrink
    std::cout << "Test 7: Verify data after shrink... ";
    auto [found_obj, found] = xbuf.find<MemoryTestType>("MemTest");
    assert(found);
    assert(found_obj->value == 42);
    assert(found_obj->data.size() == 100);
    assert(found_obj->strings.size() == 20);
    assert(found_obj->data[50] == 50);
    assert(found_obj->strings[10] == "String_10");
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] All memory statistics tests passed!\n";
    return true;
}

int main() {
    try {
        return test_memory_stats() ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}