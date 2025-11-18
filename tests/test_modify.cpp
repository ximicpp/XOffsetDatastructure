// Test data modification

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
    std::cout << "\nTesting basic type modification...\n";
    
    XBuffer xbuf(8192);
    auto* data = xbuf.make_root<ModifyTestData>("ModifyTest");
    
    // Initialize values
    std::cout << "  initialize... ";
    data->counter = 100;
    data->ratio = 1.5f;
    data->active = true;
    assert(data->counter == 100);
    assert(data->ratio == 1.5f);
    assert(data->active == true);
    std::cout << "ok\n";
    
    // Modify int
    std::cout << "  modify int... ";
    data->counter += 50;
    assert(data->counter == 150);
    data->counter *= 2;
    assert(data->counter == 300);
    data->counter--;
    assert(data->counter == 299);
    std::cout << "ok\n";
    
    // Modify float
    std::cout << "  modify float... ";
    data->ratio += 0.5f;
    assert(data->ratio > 1.99f && data->ratio < 2.01f);
    data->ratio *= 2.0f;
    assert(data->ratio > 3.99f && data->ratio < 4.01f);
    std::cout << "ok\n";
    
    // Toggle bool
    std::cout << "  toggle bool... ";
    data->active = false;
    assert(data->active == false);
    data->active = !data->active;
    assert(data->active == true);
    std::cout << "ok\n";
    
    // Verify persistence after modification
    std::cout << "  verify persistence... ";
    auto [found_data, found] = xbuf.find_root<ModifyTestData>("ModifyTest");
    assert(found);
    assert(found_data->counter == 299);
    assert(found_data->ratio > 3.99f && found_data->ratio < 4.01f);
    assert(found_data->active == true);
    std::cout << "ok\n";
    
    std::cout << "All tests passed\n";
    return true;
}

bool test_modify_mixed_operations() {
    std::cout << "\nTesting mixed modifications...\n";
    
    XBuffer xbuf(16384);
    auto* data = xbuf.make_root<ModifyTestData>("ModifyTest");
    
    // Initialize all fields
    std::cout << "  initialize... ";
    data->counter = 0;
    data->ratio = 1.0f;
    data->active = true;
    
    for (int i = 0; i < 3; ++i) {
        data->numbers.push_back(i);
    }
    
    data->names.emplace_back("Alice", xbuf.allocator<XString>());
    data->names.emplace_back("Bob", xbuf.allocator<XString>());
    
    data->scores.emplace(XString("Alice", xbuf.allocator<XString>()), 80);
    data->scores.emplace(XString("Bob", xbuf.allocator<XString>()), 85);
    
    data->tags.insert(1);
    data->tags.insert(2);
    std::cout << "ok\n";
    
    // Modify all types simultaneously
    std::cout << "  modify all... ";
    data->counter = 100;
    data->ratio = 2.5f;
    data->active = false;
    
    data->numbers[0] = 999;
    data->names[0] = XString("Alicia", xbuf.allocator<XString>());
    data->scores[XString("Alice", xbuf.allocator<XString>())] = 95;
    data->tags.insert(3);
    
    assert(data->counter == 100);
    assert(data->numbers[0] == 999);
    assert(data->names[0] == "Alicia");
    assert(data->scores[XString("Alice", xbuf.allocator<XString>())] == 95);
    assert(data->tags.find(3) != data->tags.end());
    std::cout << "ok\n";
    
    // Verify via find
    std::cout << "  verify via find... ";
    auto [found_data, found] = xbuf.find_root<ModifyTestData>("ModifyTest");
    assert(found);
    assert(found_data->counter == 100);
    assert(found_data->ratio > 2.49f && found_data->ratio < 2.51f);
    assert(found_data->active == false);
    assert(found_data->numbers[0] == 999);
    assert(found_data->names[0] == "Alicia");
    std::cout << "ok\n";
    
    // Complex batch modifications
    std::cout << "  batch modify... ";
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
    assert(data->scores[XString("Alice", xbuf.allocator<XString>())] == 85);
    assert(data->tags.size() == 8);  // 1,2,3,10,11,12,13,14
    std::cout << "ok\n";
    
    // Serialize and verify
    std::cout << "  serialize... ";
    std::vector<char> buffer(*xbuf.get_buffer());
    
    XBuffer new_xbuf(buffer);
    auto [new_data, new_found] = new_xbuf.find_root<ModifyTestData>("ModifyTest");
    assert(new_found);
    assert(new_data->counter == 100);
    assert(new_data->numbers[0] == 1998);
    assert(new_data->names[0] == "Alicia");
    assert(new_data->scores[XString("Alice", xbuf.allocator<XString>())] == 85);
    assert(new_data->tags.size() == 8);
    std::cout << "ok\n";
    
    std::cout << "All tests passed\n";
    return true;
}

int main() {
    try {
        bool all_passed = true;
        
        all_passed &= test_modify_basic_types();
        all_passed &= test_modify_mixed_operations();
        
        if (all_passed) {
            std::cout << "\nAll modification tests passed\n";
            return 0;
        } else {
            std::cout << "\nSome tests failed\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }
}
