// ============================================================================
// Test: Data Modification
// Purpose: Test in-place data modification operations
// ============================================================================

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// Test data structure
struct alignas(BASIC_ALIGNMENT) ModifyTestData {
    template <typename Allocator>
    ModifyTestData(Allocator allocator) 
        : numbers(allocator), names(allocator), scores(allocator), tags(allocator) {}
    
    // Basic types
    int counter;
    float ratio;
    bool active;
    
    // Containers
    XVector<int> numbers;
    XVector<XString> names;
    XMap<XString, int> scores;
    XSet<int> tags;
};

bool test_modify_basic_types() {
    std::cout << "\n[TEST] Modify Basic Types\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBuffer xbuf(8192);
    auto* data = xbuf.construct<ModifyTestData>("ModifyTest")(xbuf.get_segment_manager());
    
    // Test 1: Initialize values
    std::cout << "Test 1: Initialize basic values... ";
    data->counter = 100;
    data->ratio = 1.5f;
    data->active = true;
    assert(data->counter == 100);
    assert(data->ratio == 1.5f);
    assert(data->active == true);
    std::cout << "[OK]\n";
    
    // Test 2: Modify int
    std::cout << "Test 2: Modify int value... ";
    data->counter += 50;
    assert(data->counter == 150);
    data->counter *= 2;
    assert(data->counter == 300);
    data->counter--;
    assert(data->counter == 299);
    std::cout << "[OK]\n";
    
    // Test 3: Modify float
    std::cout << "Test 3: Modify float value... ";
    data->ratio += 0.5f;
    assert(data->ratio > 1.99f && data->ratio < 2.01f);
    data->ratio *= 2.0f;
    assert(data->ratio > 3.99f && data->ratio < 4.01f);
    std::cout << "[OK]\n";
    
    // Test 4: Toggle bool
    std::cout << "Test 4: Toggle bool value... ";
    data->active = false;
    assert(data->active == false);
    data->active = !data->active;
    assert(data->active == true);
    std::cout << "[OK]\n";
    
    // Test 5: Verify persistence after modification
    std::cout << "Test 5: Verify persistence... ";
    auto [found_data, found] = xbuf.find<ModifyTestData>("ModifyTest");
    assert(found);
    assert(found_data->counter == 299);
    assert(found_data->ratio > 3.99f && found_data->ratio < 4.01f);
    assert(found_data->active == true);
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] All basic type modification tests passed!\n";
    return true;
}

bool test_modify_vector() {
    std::cout << "\n[TEST] Modify Vector Contents\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBuffer xbuf(8192);
    auto* data = xbuf.construct<ModifyTestData>("ModifyTest")(xbuf.get_segment_manager());
    
    // Test 1: Add elements
    std::cout << "Test 1: Add elements to vector... ";
    for (int i = 0; i < 5; ++i) {
        data->numbers.push_back(i * 10);
    }
    assert(data->numbers.size() == 5);
    assert(data->numbers[0] == 0);
    assert(data->numbers[4] == 40);
    std::cout << "[OK]\n";
    
    // Test 2: Modify existing elements by index
    std::cout << "Test 2: Modify elements by index... ";
    data->numbers[0] = 999;
    data->numbers[2] = 777;
    data->numbers[4] = 555;
    assert(data->numbers[0] == 999);
    assert(data->numbers[2] == 777);
    assert(data->numbers[4] == 555);
    std::cout << "[OK]\n";
    
    // Test 3: Modify via iterator
    std::cout << "Test 3: Modify via iterator... ";
    for (auto& num : data->numbers) {
        num += 1;
    }
    assert(data->numbers[0] == 1000);
    assert(data->numbers[1] == 11);
    assert(data->numbers[2] == 778);
    std::cout << "[OK]\n";
    
    // Test 4: Remove elements
    std::cout << "Test 4: Remove elements... ";
    data->numbers.pop_back();
    assert(data->numbers.size() == 4);
    data->numbers.erase(data->numbers.begin());
    assert(data->numbers.size() == 3);
    assert(data->numbers[0] == 11);  // Was numbers[1]
    std::cout << "[OK]\n";
    
    // Test 5: Clear and refill
    std::cout << "Test 5: Clear and refill... ";
    data->numbers.clear();
    assert(data->numbers.empty());
    data->numbers.push_back(123);
    data->numbers.push_back(456);
    assert(data->numbers.size() == 2);
    assert(data->numbers[0] == 123);
    assert(data->numbers[1] == 456);
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] All vector modification tests passed!\n";
    return true;
}

bool test_modify_vector_string() {
    std::cout << "\n[TEST] Modify Vector of Strings\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBuffer xbuf(8192);
    auto* data = xbuf.construct<ModifyTestData>("ModifyTest")(xbuf.get_segment_manager());
    
    // Test 1: Add string elements
    std::cout << "Test 1: Add string elements... ";
    data->names.emplace_back("Alice", xbuf.get_segment_manager());
    data->names.emplace_back("Bob", xbuf.get_segment_manager());
    data->names.emplace_back("Carol", xbuf.get_segment_manager());
    assert(data->names.size() == 3);
    assert(data->names[0] == "Alice");
    std::cout << "[OK]\n";
    
    // Test 2: Modify string element
    std::cout << "Test 2: Modify string element... ";
    data->names[1] = XString("Bobby", xbuf.get_segment_manager());
    assert(data->names[1] == "Bobby");
    std::cout << "[OK]\n";
    
    // Test 3: Modify via iterator
    std::cout << "Test 3: Append to strings via iterator... ";
    for (auto& name : data->names) {
        XString suffix("_Modified", xbuf.get_segment_manager());
        name = XString((std::string(name.c_str()) + std::string(suffix.c_str())).c_str(), 
                       xbuf.get_segment_manager());
    }
    assert(data->names[0] == "Alice_Modified");
    assert(data->names[1] == "Bobby_Modified");
    assert(data->names[2] == "Carol_Modified");
    std::cout << "[OK]\n";
    
    // Test 4: Insert new element
    std::cout << "Test 4: Insert new element... ";
    auto it = data->names.begin() + 1;
    data->names.insert(it, XString("David", xbuf.get_segment_manager()));
    assert(data->names.size() == 4);
    assert(data->names[1] == "David");
    assert(data->names[2] == "Bobby_Modified");
    std::cout << "[OK]\n";
    
    // Test 5: Erase element
    std::cout << "Test 5: Erase element... ";
    data->names.erase(data->names.begin() + 1);
    assert(data->names.size() == 3);
    assert(data->names[1] == "Bobby_Modified");
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] All string vector modification tests passed!\n";
    return true;
}

bool test_modify_map() {
    std::cout << "\n[TEST] Modify Map Contents\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBuffer xbuf(8192);
    auto* data = xbuf.construct<ModifyTestData>("ModifyTest")(xbuf.get_segment_manager());
    
    // Test 1: Add key-value pairs
    std::cout << "Test 1: Add key-value pairs... ";
    data->scores.emplace(XString("Alice", xbuf.get_segment_manager()), 85);
    data->scores.emplace(XString("Bob", xbuf.get_segment_manager()), 90);
    data->scores.emplace(XString("Carol", xbuf.get_segment_manager()), 78);
    assert(data->scores.size() == 3);
    std::cout << "[OK]\n";
    
    // Test 2: Modify existing value by key
    std::cout << "Test 2: Modify value by key... ";
    auto it = data->scores.find(XString("Alice", xbuf.get_segment_manager()));
    assert(it != data->scores.end());
    it->second = 95;
    assert(data->scores.find(XString("Alice", xbuf.get_segment_manager()))->second == 95);
    std::cout << "[OK]\n";
    
    // Test 3: Modify via operator[]
    std::cout << "Test 3: Modify via operator[]... ";
    data->scores[XString("Bob", xbuf.get_segment_manager())] = 92;
    assert(data->scores[XString("Bob", xbuf.get_segment_manager())] == 92);
    std::cout << "[OK]\n";
    
    // Test 4: Add new entry via operator[]
    std::cout << "Test 4: Add via operator[]... ";
    data->scores[XString("David", xbuf.get_segment_manager())] = 88;
    assert(data->scores.size() == 4);
    assert(data->scores[XString("David", xbuf.get_segment_manager())] == 88);
    std::cout << "[OK]\n";
    
    // Test 5: Modify all values via iterator
    std::cout << "Test 5: Increment all values... ";
    for (auto& pair : data->scores) {
        pair.second += 5;
    }
    assert(data->scores[XString("Alice", xbuf.get_segment_manager())] == 100);
    assert(data->scores[XString("Bob", xbuf.get_segment_manager())] == 97);
    assert(data->scores[XString("Carol", xbuf.get_segment_manager())] == 83);
    assert(data->scores[XString("David", xbuf.get_segment_manager())] == 93);
    std::cout << "[OK]\n";
    
    // Test 6: Erase entry
    std::cout << "Test 6: Erase entry... ";
    data->scores.erase(XString("Carol", xbuf.get_segment_manager()));
    assert(data->scores.size() == 3);
    assert(data->scores.find(XString("Carol", xbuf.get_segment_manager())) == data->scores.end());
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] All map modification tests passed!\n";
    return true;
}

bool test_modify_set() {
    std::cout << "\n[TEST] Modify Set Contents\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBuffer xbuf(8192);
    auto* data = xbuf.construct<ModifyTestData>("ModifyTest")(xbuf.get_segment_manager());
    
    // Test 1: Add elements
    std::cout << "Test 1: Add elements to set... ";
    data->tags.insert(10);
    data->tags.insert(20);
    data->tags.insert(30);
    data->tags.insert(20);  // Duplicate, should be ignored
    assert(data->tags.size() == 3);
    std::cout << "[OK]\n";
    
    // Test 2: Check existence
    std::cout << "Test 2: Check element existence... ";
    assert(data->tags.find(20) != data->tags.end());
    assert(data->tags.find(99) == data->tags.end());
    std::cout << "[OK]\n";
    
    // Test 3: Remove element
    std::cout << "Test 3: Remove element... ";
    data->tags.erase(20);
    assert(data->tags.size() == 2);
    assert(data->tags.find(20) == data->tags.end());
    std::cout << "[OK]\n";
    
    // Test 4: Add multiple elements
    std::cout << "Test 4: Add multiple elements... ";
    for (int i = 40; i <= 60; i += 10) {
        data->tags.insert(i);
    }
    assert(data->tags.size() == 5);
    assert(data->tags.find(50) != data->tags.end());
    std::cout << "[OK]\n";
    
    // Test 5: Clear and refill
    std::cout << "Test 5: Clear and refill... ";
    data->tags.clear();
    assert(data->tags.empty());
    data->tags.insert(100);
    data->tags.insert(200);
    assert(data->tags.size() == 2);
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] All set modification tests passed!\n";
    return true;
}

bool test_modify_mixed_operations() {
    std::cout << "\n[TEST] Mixed Modification Operations\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBuffer xbuf(16384);
    auto* data = xbuf.construct<ModifyTestData>("ModifyTest")(xbuf.get_segment_manager());
    
    // Test 1: Initialize all fields
    std::cout << "Test 1: Initialize all fields... ";
    data->counter = 0;
    data->ratio = 1.0f;
    data->active = true;
    
    for (int i = 0; i < 3; ++i) {
        data->numbers.push_back(i);
    }
    
    data->names.emplace_back("Alice", xbuf.get_segment_manager());
    data->names.emplace_back("Bob", xbuf.get_segment_manager());
    
    data->scores.emplace(XString("Alice", xbuf.get_segment_manager()), 80);
    data->scores.emplace(XString("Bob", xbuf.get_segment_manager()), 85);
    
    data->tags.insert(1);
    data->tags.insert(2);
    std::cout << "[OK]\n";
    
    // Test 2: Modify all types simultaneously
    std::cout << "Test 2: Modify all types... ";
    data->counter = 100;
    data->ratio = 2.5f;
    data->active = false;
    
    data->numbers[0] = 999;
    data->names[0] = XString("Alicia", xbuf.get_segment_manager());
    data->scores[XString("Alice", xbuf.get_segment_manager())] = 95;
    data->tags.insert(3);
    
    assert(data->counter == 100);
    assert(data->numbers[0] == 999);
    assert(data->names[0] == "Alicia");
    assert(data->scores[XString("Alice", xbuf.get_segment_manager())] == 95);
    assert(data->tags.find(3) != data->tags.end());
    std::cout << "[OK]\n";
    
    // Test 3: Verify via find
    std::cout << "Test 3: Verify via find... ";
    auto [found_data, found] = xbuf.find<ModifyTestData>("ModifyTest");
    assert(found);
    assert(found_data->counter == 100);
    assert(found_data->ratio > 2.49f && found_data->ratio < 2.51f);
    assert(found_data->active == false);
    assert(found_data->numbers[0] == 999);
    assert(found_data->names[0] == "Alicia");
    std::cout << "[OK]\n";
    
    // Test 4: Complex batch modifications
    std::cout << "Test 4: Complex batch modifications... ";
    // Modify vector
    for (auto& num : data->numbers) {
        num *= 2;
    }
    // Modify map
    for (auto& pair : data->scores) {
        pair.second -= 10;
    }
    // Add to set
    for (int i = 10; i < 15; ++i) {
        data->tags.insert(i);
    }
    
    assert(data->numbers[0] == 1998);
    assert(data->scores[XString("Alice", xbuf.get_segment_manager())] == 85);
    assert(data->tags.size() == 8);  // 1,2,3,10,11,12,13,14
    std::cout << "[OK]\n";
    
    // Test 5: Serialize and verify
    std::cout << "Test 5: Serialize to memory... ";
    std::vector<char> buffer(*xbuf.get_buffer());
    
    XBuffer new_xbuf(buffer);
    auto [new_data, new_found] = new_xbuf.find<ModifyTestData>("ModifyTest");
    assert(new_found);
    assert(new_data->counter == 100);
    assert(new_data->numbers[0] == 1998);
    assert(new_data->names[0] == "Alicia");
    assert(new_data->scores[XString("Alice", xbuf.get_segment_manager())] == 85);
    assert(new_data->tags.size() == 8);
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] All mixed modification tests passed!\n";
    return true;
}

int main() {
    try {
        bool all_passed = true;
        
        all_passed &= test_modify_basic_types();
        all_passed &= test_modify_vector();
        all_passed &= test_modify_vector_string();
        all_passed &= test_modify_map();
        all_passed &= test_modify_set();
        all_passed &= test_modify_mixed_operations();
        
        if (all_passed) {
            std::cout << "\n[SUCCESS] All modification tests passed!\n";
            return 0;
        } else {
            std::cout << "\n[FAILURE] Some tests failed!\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}
