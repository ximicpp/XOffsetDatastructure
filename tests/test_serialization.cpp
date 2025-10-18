// ============================================================================
// Test: Serialization and Deserialization
// Purpose: Test save/load functionality
// ============================================================================

#include <iostream>
#include <cassert>
#include <string>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// Test structures
struct SimpleData {
    template <typename Allocator>
    SimpleData(Allocator allocator) : name(allocator) {}
    
    int id;
    float value;
    XString name;
};

struct ComplexData {
    template <typename Allocator>
    ComplexData(Allocator allocator)
        : title(allocator), items(allocator), tags(allocator), metadata(allocator) {}
    
    XString title;
    XVector<int> items;
    XSet<int> tags;
    XMap<XString, int> metadata;
};

bool test_simple_serialization() {
    std::cout << "\n[TEST] Simple Serialization\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Create and populate
    std::cout << "Test 1: Create and populate... ";
    XBufferExt xbuf(4096);
    auto* data = xbuf.make<SimpleData>("TestData");
    data->id = 42;
    data->value = 3.14f;
    data->name = xbuf.make<XString>("Hello");
    std::cout << "[OK]\n";
    
    // Serialize
    std::cout << "Test 2: Serialize to string... ";
    std::string serialized = xbuf.save_to_string();
    assert(serialized.size() > 0);
    std::cout << "[OK] (Size: " << serialized.size() << " bytes)\n";
    
    // Deserialize
    std::cout << "Test 3: Deserialize from string... ";
    XBufferExt loaded = XBufferExt::load_from_string(serialized);
    auto [loaded_data, found] = loaded.find_ex<SimpleData>("TestData");
    assert(found);
    std::cout << "[OK]\n";
    
    // Verify data
    std::cout << "Test 4: Verify data integrity... ";
    assert(loaded_data->id == 42);
    assert(loaded_data->value == 3.14f);
    assert(loaded_data->name == "Hello");
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Simple serialization tests passed!\n";
    return true;
}

bool test_complex_serialization() {
    std::cout << "\n[TEST] Complex Serialization\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Create and populate complex structure
    std::cout << "Test 1: Create complex structure... ";
    XBufferExt xbuf(8192);
    auto* data = xbuf.make<ComplexData>("Complex");
    data->title = xbuf.make<XString>("Test Title");
    
    for (int i = 0; i < 10; ++i) {
        data->items.push_back(i * 10);
    }
    
    data->tags.insert(1);
    data->tags.insert(5);
    data->tags.insert(10);
    
    data->metadata.emplace(xbuf.make<XString>("version"), 1);
    data->metadata.emplace(xbuf.make<XString>("count"), 100);
    std::cout << "[OK]\n";
    
    // Serialize
    std::cout << "Test 2: Serialize complex structure... ";
    std::string serialized = xbuf.save_to_string();
    std::cout << "[OK] (Size: " << serialized.size() << " bytes)\n";
    
    // Deserialize
    std::cout << "Test 3: Deserialize complex structure... ";
    XBufferExt loaded = XBufferExt::load_from_string(serialized);
    auto [loaded_data, found] = loaded.find_ex<ComplexData>("Complex");
    assert(found);
    std::cout << "[OK]\n";
    
    // Verify all data
    std::cout << "Test 4: Verify complex data... ";
    assert(loaded_data->title == "Test Title");
    assert(loaded_data->items.size() == 10);
    assert(loaded_data->items[0] == 0);
    assert(loaded_data->items[9] == 90);
    assert(loaded_data->tags.size() == 3);
    assert(loaded_data->tags.count(5) == 1);
    assert(loaded_data->metadata.size() == 2);
    assert(loaded_data->metadata[xbuf.make<XString>("version")] == 1);
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Complex serialization tests passed!\n";
    return true;
}

bool test_multiple_objects() {
    std::cout << "\n[TEST] Multiple Objects Serialization\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Create multiple objects
    std::cout << "Test 1: Create multiple objects... ";
    XBufferExt xbuf(8192);
    
    auto* obj1 = xbuf.make<SimpleData>("Object1");
    obj1->id = 1;
    obj1->value = 1.1f;
    obj1->name = xbuf.make<XString>("First");
    
    auto* obj2 = xbuf.make<SimpleData>("Object2");
    obj2->id = 2;
    obj2->value = 2.2f;
    obj2->name = xbuf.make<XString>("Second");
    
    auto* obj3 = xbuf.make<SimpleData>("Object3");
    obj3->id = 3;
    obj3->value = 3.3f;
    obj3->name = xbuf.make<XString>("Third");
    std::cout << "[OK]\n";
    
    // Serialize
    std::cout << "Test 2: Serialize all objects... ";
    std::string serialized = xbuf.save_to_string();
    std::cout << "[OK]\n";
    
    // Deserialize
    std::cout << "Test 3: Deserialize and find all objects... ";
    XBufferExt loaded = XBufferExt::load_from_string(serialized);
    
    auto [loaded1, found1] = loaded.find_ex<SimpleData>("Object1");
    auto [loaded2, found2] = loaded.find_ex<SimpleData>("Object2");
    auto [loaded3, found3] = loaded.find_ex<SimpleData>("Object3");
    
    assert(found1 && found2 && found3);
    std::cout << "[OK]\n";
    
    // Verify all objects
    std::cout << "Test 4: Verify all objects... ";
    assert(loaded1->id == 1 && loaded1->name == "First");
    assert(loaded2->id == 2 && loaded2->name == "Second");
    assert(loaded3->id == 3 && loaded3->name == "Third");
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Multiple objects serialization tests passed!\n";
    return true;
}

bool test_empty_buffer() {
    std::cout << "\n[TEST] Empty Buffer Serialization\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Create empty buffer
    std::cout << "Test 1: Create empty buffer... ";
    XBufferExt xbuf(1024);
    std::cout << "[OK]\n";
    
    // Serialize empty buffer
    std::cout << "Test 2: Serialize empty buffer... ";
    std::string serialized = xbuf.save_to_string();
    assert(serialized.size() == 1024);
    std::cout << "[OK]\n";
    
    // Deserialize
    std::cout << "Test 3: Deserialize empty buffer... ";
    XBufferExt loaded = XBufferExt::load_from_string(serialized);
    assert(loaded.get_size() == 1024);
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Empty buffer serialization tests passed!\n";
    return true;
}

bool test_roundtrip() {
    std::cout << "\n[TEST] Multiple Roundtrip Serialization\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Create initial data
    std::cout << "Test 1: Create initial data... ";
    XBufferExt xbuf1(4096);
    auto* data = xbuf1.make<SimpleData>("Roundtrip");
    data->id = 999;
    data->value = 9.99f;
    data->name = xbuf1.make<XString>("Original");
    std::cout << "[OK]\n";
    
    // First roundtrip
    std::cout << "Test 2: First serialize/deserialize... ";
    std::string s1 = xbuf1.save_to_string();
    XBufferExt xbuf2 = XBufferExt::load_from_string(s1);
    auto [data2, f2] = xbuf2.find_ex<SimpleData>("Roundtrip");
    assert(f2 && data2->id == 999);
    std::cout << "[OK]\n";
    
    // Second roundtrip
    std::cout << "Test 3: Second serialize/deserialize... ";
    std::string s2 = xbuf2.save_to_string();
    XBufferExt xbuf3 = XBufferExt::load_from_string(s2);
    auto [data3, f3] = xbuf3.find_ex<SimpleData>("Roundtrip");
    assert(f3 && data3->id == 999);
    std::cout << "[OK]\n";
    
    // Third roundtrip
    std::cout << "Test 4: Third serialize/deserialize... ";
    std::string s3 = xbuf3.save_to_string();
    XBufferExt xbuf4 = XBufferExt::load_from_string(s3);
    auto [data4, f4] = xbuf4.find_ex<SimpleData>("Roundtrip");
    assert(f4 && data4->id == 999);
    assert(data4->value == 9.99f);
    assert(data4->name == "Original");
    std::cout << "[OK]\n";
    
    // Verify serialized data is identical
    std::cout << "Test 5: Verify serialized data stability... ";
    assert(s1.size() == s2.size());
    assert(s2.size() == s3.size());
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Multiple roundtrip tests passed!\n";
    return true;
}

int main() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  Serialization Tests\n";
    std::cout << "========================================\n";
    
    bool all_passed = true;
    
    all_passed &= test_simple_serialization();
    all_passed &= test_complex_serialization();
    all_passed &= test_multiple_objects();
    all_passed &= test_empty_buffer();
    all_passed &= test_roundtrip();
    
    std::cout << "\n";
    std::cout << "========================================\n";
    if (all_passed) {
        std::cout << "  ALL TESTS PASSED ✓\n";
    } else {
        std::cout << "  SOME TESTS FAILED ✗\n";
    }
    std::cout << "========================================\n\n";
    
    return all_passed ? 0 : 1;
}
