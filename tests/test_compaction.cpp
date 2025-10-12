// ============================================================================
// Test: Memory Compaction
// Purpose: Test memory compaction functionality
// ============================================================================

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

struct CompactTestType {
    template <typename Allocator>
    CompactTestType(Allocator allocator) 
        : data(allocator), strings(allocator) {}
    
    int value;
    XVector<int> data;
    XVector<XString> strings;
    
    // Migration function for compaction
    static void migrate(XBuffer& old_buf, XBuffer& new_buf) {
        auto* old_obj = old_buf.find<CompactTestType>("CompactTest").first;
        if (!old_obj) return;
        
        auto* new_obj = new_buf.construct<CompactTestType>("CompactTest")(new_buf.get_segment_manager());
        new_obj->value = old_obj->value;
        new_obj->data = old_obj->data;
        
        for (const auto& str : old_obj->strings) {
            new_obj->strings.emplace_back(str.c_str(), new_buf.get_segment_manager());
        }
    }
};

bool test_memory_compaction() {
    std::cout << "\n[TEST] Memory Compaction\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Create initial buffer with extra space
    XBuffer xbuf(16384);
    auto* obj = xbuf.construct<CompactTestType>("CompactTest")(xbuf.get_segment_manager());
    
    // Test 1: Fill with data
    std::cout << "Test 1: Fill buffer with data... ";
    obj->value = 999;
    for (int i = 0; i < 100; ++i) {
        obj->data.push_back(i);
    }
    for (int i = 0; i < 20; ++i) {
        std::string str = "TestString_" + std::to_string(i);
        obj->strings.emplace_back(str.c_str(), xbuf.get_segment_manager());
    }
    std::cout << "[OK]\n";
    
    // Test 2: Check memory stats before compaction
    std::cout << "Test 2: Memory stats before compaction... ";
    auto stats_before = XBufferVisualizer::get_memory_stats(xbuf);
    std::cout << "\n  Total size: " << stats_before.total_size << " bytes\n";
    std::cout << "  Used:       " << stats_before.used_size << " bytes\n";
    std::cout << "  Free:       " << stats_before.free_size << " bytes\n";
    std::cout << "  Efficiency: " << stats_before.usage_percent() << "%\n";
    std::cout << "[OK]\n";
    
    // Test 3: Perform compaction
    std::cout << "Test 3: Compact memory... ";
    XBuffer compact_buf = XBufferCompactor::compact_manual<CompactTestType>(xbuf);
    assert(compact_buf.get_size() > 0);
    std::cout << "[OK]\n";
    
    // Test 4: Check memory stats after compaction
    std::cout << "Test 4: Memory stats after compaction... ";
    auto stats_after = XBufferVisualizer::get_memory_stats(compact_buf);
    std::cout << "\n  Total size: " << stats_after.total_size << " bytes\n";
    std::cout << "  Used:       " << stats_after.used_size << " bytes\n";
    std::cout << "  Free:       " << stats_after.free_size << " bytes\n";
    std::cout << "  Efficiency: " << stats_after.usage_percent() << "%\n";
    
    // Verify size reduction
    assert(stats_after.total_size < stats_before.total_size);
    double reduction = (1.0 - (double)stats_after.total_size / stats_before.total_size) * 100.0;
    std::cout << "  Reduction:  " << reduction << "%\n";
    std::cout << "[OK]\n";
    
    // Test 5: Verify data integrity after compaction
    std::cout << "Test 5: Verify data integrity... ";
    auto* compact_obj = compact_buf.find<CompactTestType>("CompactTest").first;
    assert(compact_obj != nullptr);
    assert(compact_obj->value == 999);
    assert(compact_obj->data.size() == 100);
    assert(compact_obj->strings.size() == 20);
    assert(compact_obj->data[50] == 50);
    assert(compact_obj->strings[10] == "TestString_10");
    std::cout << "[OK]\n";
    
    // Test 6: Verify all data elements
    std::cout << "Test 6: Verify all elements... ";
    for (int i = 0; i < 100; ++i) {
        assert(compact_obj->data[i] == i);
    }
    for (int i = 0; i < 20; ++i) {
        std::string expected = "TestString_" + std::to_string(i);
        assert(compact_obj->strings[i] == expected.c_str());
    }
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] All compaction tests passed!\n";
    return true;
}

int main() {
    try {
        return test_memory_compaction() ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}
