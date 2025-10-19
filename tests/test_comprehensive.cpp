// ============================================================================
// Test: Comprehensive Integration Test
// Purpose: Full integration test covering all features with realistic data
// ============================================================================

#include <iostream>
#include <cassert>
#include <vector>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// Comprehensive test data structure
struct alignas(BASIC_ALIGNMENT) ComprehensiveTestType {
    template <typename Allocator>
    ComprehensiveTestType(Allocator allocator) 
        : mVector(allocator), 
          mStringVector(allocator), 
          mComplexMap(allocator), 
          mStringSet(allocator), 
          mSet(allocator), 
          mString(allocator), 
          TestTypeInnerObj(allocator), 
          mXXTypeVector(allocator) {}
    
    int mInt{ 0 };
    float mFloat{ 0.f };
    XVector<int> mVector;
    XVector<XString> mStringVector;
    
    class TestTypeInner {
    public:
        template <typename Allocator>
        TestTypeInner(Allocator allocator) : mVector(allocator) {}
        int mInt{ 0 };
        XVector<int> mVector;
    } TestTypeInnerObj;
    
    XVector<TestTypeInner> mXXTypeVector;
    XMap<XString, TestTypeInner> mComplexMap;
    XSet<XString> mStringSet;
    XSet<int> mSet;
    XString mString;
};

// ============================================================================
// Stage 1: Fill Basic Fields
// ============================================================================
void fillBasicFields(ComprehensiveTestType* obj, XBufferExt& xbuf) {
    obj->mInt = 123;
    obj->mFloat = 3.14f;
    obj->mString = xbuf.make<XString>("abcdefghijklmnopqrstuvwxyz");
    
    obj->mVector.push_back(1);
    obj->mVector.push_back(3);
    obj->mSet.insert(3);
    obj->mSet.insert(4);
    
    obj->TestTypeInnerObj.mInt = 246;
    obj->TestTypeInnerObj.mVector.push_back(23);
    obj->TestTypeInnerObj.mVector.push_back(21);
}

// ============================================================================
// Stage 2: Fill Small Containers
// ============================================================================
void fillSmallContainers(ComprehensiveTestType* obj, XBufferExt& xbuf) {
    for (auto i = 0; i < 1; ++i) {
        obj->mVector.push_back(i);
        obj->TestTypeInnerObj.mVector.push_back(64 + i);
    }

    for (auto i = 0; i < 6; ++i) {
        obj->mVector.push_back(1024 + i);
        obj->mSet.insert(1024 - i);
    }

    for (auto i = 0; i < 10; ++i) {
        obj->mStringVector.emplace_back(xbuf.make<XString>("abcdefghijklmnopqrstuvwxyz"));
    }

    for (auto i = 0; i < 13; ++i) {
        obj->mStringSet.emplace(xbuf.make<XString>("stringinset"));
        obj->mSet.insert(i);
    }
}

// ============================================================================
// Stage 3: Fill Large Nested Data
// ============================================================================
void fillLargeNestedData(ComprehensiveTestType* obj, XBufferExt& xbuf) {
    // Complex map with nested vectors
    for (auto i = 0; i < 7; ++i) {
        std::string key = "stringinset" + std::to_string(i);
        XString xkey = xbuf.make<XString>(key);
        ComprehensiveTestType::TestTypeInner xvalue(xbuf.allocator<ComprehensiveTestType::TestTypeInner>());
        obj->mComplexMap.emplace(xkey, xvalue);
        auto& vec = obj->mComplexMap.find(xkey)->second.mVector;
        for (int j = 0; j < 100; ++j) {
            vec.insert(vec.end(), {7, 8, 9, 10, 11, 12});
        }
    }

    // Vector of objects
    for (auto i = 0; i < 6; ++i) {
        obj->mXXTypeVector.emplace_back(xbuf.allocator<ComprehensiveTestType::TestTypeInner>());
        obj->mXXTypeVector.back().mInt = i;
        auto& vec = obj->mXXTypeVector.back().mVector;
        vec = {1, 2, 3, 4, 5, 6};
        for (auto j = 0; j < 12; ++j) {
            vec.push_back(j);
        }
    }
}

// ============================================================================
// Validation Functions
// ============================================================================
bool validateBasicFields(ComprehensiveTestType* obj) {
    if (obj->mInt != 123) {
        std::cerr << "[X] mInt validation failed\n";
        return false;
    }
    if (std::abs(obj->mFloat - 3.14f) > 0.01f) {
        std::cerr << "[X] mFloat validation failed\n";
        return false;
    }
    if (obj->mString != "abcdefghijklmnopqrstuvwxyz") {
        std::cerr << "[X] mString validation failed\n";
        return false;
    }
    if (obj->TestTypeInnerObj.mInt != 246) {
        std::cerr << "[X] TestTypeInnerObj.mInt validation failed\n";
        return false;
    }
    return true;
}

bool validateContainerSizes(ComprehensiveTestType* obj) {
    // Vector should have: 1,3,0,1024,1025,1026,1027,1028,1029
    if (obj->mVector.size() != 9) {
        std::cerr << "[X] mVector size validation failed: " << obj->mVector.size() << "\n";
        return false;
    }
    if (obj->mStringVector.size() != 10) {
        std::cerr << "[X] mStringVector size validation failed\n";
        return false;
    }
    if (obj->mComplexMap.size() != 7) {
        std::cerr << "[X] mComplexMap size validation failed\n";
        return false;
    }
    if (obj->mXXTypeVector.size() != 6) {
        std::cerr << "[X] mXXTypeVector size validation failed\n";
        return false;
    }
    // Set should have unique values
    if (obj->mSet.size() < 13) {
        std::cerr << "[X] mSet size validation failed\n";
        return false;
    }
    return true;
}

bool validateNestedData(ComprehensiveTestType* obj) {
    // Check complex map entries
    for (int i = 0; i < 7; ++i) {
        std::string key = "stringinset" + std::to_string(i);
        auto it = obj->mComplexMap.find(XString(key.c_str(), obj->mComplexMap.get_allocator()));
        if (it == obj->mComplexMap.end()) {
            std::cerr << "[X] mComplexMap key not found: " << key << "\n";
            return false;
        }
        // Each entry should have 100 * 6 = 600 elements
        if (it->second.mVector.size() != 600) {
            std::cerr << "[X] mComplexMap[" << key << "].mVector size validation failed\n";
            return false;
        }
    }
    
    // Check vector of objects
    for (size_t i = 0; i < obj->mXXTypeVector.size(); ++i) {
        if (obj->mXXTypeVector[i].mInt != (int)i) {
            std::cerr << "[X] mXXTypeVector[" << i << "].mInt validation failed\n";
            return false;
        }
        // Each should have 6 + 12 = 18 elements
        if (obj->mXXTypeVector[i].mVector.size() != 18) {
            std::cerr << "[X] mXXTypeVector[" << i << "].mVector size validation failed\n";
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// Test Functions
// ============================================================================
bool test_comprehensive_creation() {
    std::cout << "\n[TEST] Comprehensive Data Creation\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Test 1: Create buffer and object
    std::cout << "Test 1: Create initial buffer... ";
    XBufferExt xbuf(4096);
    auto* obj = xbuf.make<ComprehensiveTestType>("CompTest");
    assert(obj != nullptr);
    std::cout << "[OK]\n";
    
    // Test 2: Fill basic fields
    std::cout << "Test 2: Fill basic fields... ";
    fillBasicFields(obj, xbuf);
    assert(validateBasicFields(obj));
    std::cout << "[OK]\n";
    
    // Test 3: Fill small containers
    std::cout << "Test 3: Fill small containers... ";
    fillSmallContainers(obj, xbuf);
    std::cout << "[OK]\n";
    
    // Test 4: Grow buffer
    std::cout << "Test 4: Grow buffer for large data... ";
    xbuf.grow(1024 * 32);
    obj = xbuf.find_or_make<ComprehensiveTestType>("CompTest");
    assert(obj != nullptr);
    std::cout << "[OK]\n";
    
    // Test 5: Fill large nested data
    std::cout << "Test 5: Fill large nested data... ";
    fillLargeNestedData(obj, xbuf);
    assert(validateContainerSizes(obj));
    assert(validateNestedData(obj));
    std::cout << "[OK]\n";
    
    // Test 6: Memory stats
    std::cout << "Test 6: Verify memory usage... ";
    auto stats = XBufferVisualizer::get_memory_stats(xbuf);
    std::cout << "\n  Total: " << stats.total_size << " bytes\n";
    std::cout << "  Used:  " << stats.used_size << " bytes (" 
              << stats.usage_percent() << "%)\n";
    assert(stats.used_size > 0);
    assert(stats.used_size < stats.total_size);
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Comprehensive data creation test passed!\n";
    return true;
}

bool test_comprehensive_serialization() {
    std::cout << "\n[TEST] Comprehensive Serialization\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Test 1: Create and fill data
    std::cout << "Test 1: Create comprehensive data... ";
    XBufferExt xbuf(4096);
    auto* obj = xbuf.make<ComprehensiveTestType>("CompTest");
    fillBasicFields(obj, xbuf);
    fillSmallContainers(obj, xbuf);
    xbuf.grow(1024 * 32);
    obj = xbuf.find_or_make<ComprehensiveTestType>("CompTest");
    fillLargeNestedData(obj, xbuf);
    std::cout << "[OK]\n";
    
    // Test 2: Serialize to memory
    std::cout << "Test 2: Serialize to memory... ";
    std::vector<char> buffer(*xbuf.get_buffer());
    assert(buffer.size() > 0);
    std::cout << "[OK] (" << buffer.size() << " bytes)\n";
    
    // Test 3: Deserialize from memory
    std::cout << "Test 3: Deserialize from memory... ";
    XBufferExt new_xbuf(buffer);
    auto [new_obj, found] = new_xbuf.find<ComprehensiveTestType>("CompTest");
    assert(found);
    assert(new_obj != nullptr);
    std::cout << "[OK]\n";
    
    // Test 4: Validate deserialized basic fields
    std::cout << "Test 4: Validate basic fields... ";
    assert(validateBasicFields(new_obj));
    std::cout << "[OK]\n";
    
    // Test 5: Validate deserialized container sizes
    std::cout << "Test 5: Validate container sizes... ";
    assert(validateContainerSizes(new_obj));
    std::cout << "[OK]\n";
    
    // Test 6: Validate deserialized nested data
    std::cout << "Test 6: Validate nested data... ";
    assert(validateNestedData(new_obj));
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Comprehensive serialization test passed!\n";
    return true;
}

bool test_comprehensive_modification() {
    std::cout << "\n[TEST] Comprehensive Modification\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Test 1: Create initial data
    std::cout << "Test 1: Create initial data... ";
    XBufferExt xbuf(4096);
    auto* obj = xbuf.make<ComprehensiveTestType>("CompTest");
    fillBasicFields(obj, xbuf);
    fillSmallContainers(obj, xbuf);
    std::cout << "[OK]\n";
    
    // Test 2: Modify basic fields
    std::cout << "Test 2: Modify basic fields... ";
    obj->mInt = 999;
    obj->mFloat = 2.718f;
    assert(obj->mInt == 999);
    assert(std::abs(obj->mFloat - 2.718f) < 0.01f);
    std::cout << "[OK]\n";
    
    // Test 3: Modify vector
    std::cout << "Test 3: Modify vector elements... ";
    obj->mVector[0] = 777;
    assert(obj->mVector[0] == 777);
    obj->mVector.push_back(9999);
    assert(obj->mVector.back() == 9999);
    std::cout << "[OK]\n";
    
    // Test 4: Modify nested object
    std::cout << "Test 4: Modify nested object... ";
    obj->TestTypeInnerObj.mInt = 555;
    assert(obj->TestTypeInnerObj.mInt == 555);
    std::cout << "[OK]\n";
    
    // Test 5: Serialize and verify modifications
    std::cout << "Test 5: Serialize and verify... ";
    std::vector<char> buffer(*xbuf.get_buffer());
    XBufferExt new_xbuf(buffer);
    auto [new_obj, found] = new_xbuf.find<ComprehensiveTestType>("CompTest");
    assert(found);
    assert(new_obj->mInt == 999);
    assert(new_obj->mVector[0] == 777);
    assert(new_obj->TestTypeInnerObj.mInt == 555);
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Comprehensive modification test passed!\n";
    return true;
}

bool test_comprehensive_memory_operations() {
    std::cout << "\n[TEST] Comprehensive Memory Operations\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Test 1: Initial buffer
    std::cout << "Test 1: Create initial buffer... ";
    XBufferExt xbuf(4096);
    auto stats1 = XBufferVisualizer::get_memory_stats(xbuf);
    std::cout << "[OK]\n  Initial: " << stats1.total_size << " bytes\n";
    
    // Test 2: Add data
    std::cout << "Test 2: Add comprehensive data... ";
    auto* obj = xbuf.make<ComprehensiveTestType>("CompTest");
    fillBasicFields(obj, xbuf);
    fillSmallContainers(obj, xbuf);
    auto stats2 = XBufferVisualizer::get_memory_stats(xbuf);
    assert(stats2.used_size > stats1.used_size);
    std::cout << "[OK]\n  Used: " << stats2.used_size << " bytes (" 
              << stats2.usage_percent() << "%)\n";
    
    // Test 3: Grow buffer
    std::cout << "Test 3: Grow buffer... ";
    std::size_t old_size = xbuf.get_size();
    xbuf.grow(1024 * 32);
    assert(xbuf.get_size() == old_size + 1024 * 32);
    auto stats3 = XBufferVisualizer::get_memory_stats(xbuf);
    std::cout << "[OK]\n  After grow: " << stats3.total_size << " bytes (" 
              << stats3.usage_percent() << "%)\n";
    
    // Test 4: Add more data
    std::cout << "Test 4: Add large nested data... ";
    obj = xbuf.find_or_make<ComprehensiveTestType>("CompTest");
    fillLargeNestedData(obj, xbuf);
    auto stats4 = XBufferVisualizer::get_memory_stats(xbuf);
    std::cout << "[OK]\n  Final used: " << stats4.used_size << " bytes (" 
              << stats4.usage_percent() << "%)\n";
    
    // Test 5: Shrink to fit
    std::cout << "Test 5: Shrink to fit... ";
    xbuf.shrink_to_fit();
    auto stats5 = XBufferVisualizer::get_memory_stats(xbuf);
    assert(stats5.total_size < stats4.total_size);
    std::cout << "[OK]\n  After shrink: " << stats5.total_size << " bytes (" 
              << stats5.usage_percent() << "%)\n";
    
    // Test 6: Verify data after shrink
    std::cout << "Test 6: Verify data integrity... ";
    auto [found_obj, found] = xbuf.find<ComprehensiveTestType>("CompTest");
    assert(found);
    assert(validateBasicFields(found_obj));
    assert(validateContainerSizes(found_obj));
    assert(validateNestedData(found_obj));
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Comprehensive memory operations test passed!\n";
    return true;
}

// ============================================================================
// Main
// ============================================================================
int main() {
    try {
        bool all_passed = true;
        
        all_passed &= test_comprehensive_creation();
        all_passed &= test_comprehensive_serialization();
        all_passed &= test_comprehensive_modification();
        all_passed &= test_comprehensive_memory_operations();
        
        if (all_passed) {
            std::cout << "\n[SUCCESS] All comprehensive tests passed!\n";
            return 0;
        } else {
            std::cout << "\n[FAILURE] Some comprehensive tests failed!\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}
