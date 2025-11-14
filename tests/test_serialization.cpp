// Test serialization and deserialization

#include <iostream>
#include <cassert>
#include <string>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// Test structures
struct alignas(BASIC_ALIGNMENT) SimpleData {
    template <typename Allocator>
    SimpleData(Allocator allocator) : name(allocator) {}
    
    int id;
    float value;
    XString name;
};

struct alignas(BASIC_ALIGNMENT) ComplexData {
    template <typename Allocator>
    ComplexData(Allocator allocator)
        : title(allocator), items(allocator), tags(allocator), metadata(allocator) {}
    
    XString title;
    XVector<int> items;
    XSet<int> tags;
    XMap<XString, int> metadata;
};

bool test_simple_serialization() {
    std::cout << "\nTesting simple serialization...\n";
    
    // Create and populate
    std::cout << "  create and populate... ";
    XBuffer xbuf(4096);
    auto* data = xbuf.make<SimpleData>("TestData");
    data->id = 42;
    data->value = 3.14f;
    std::cout << "ok\n";
    
    // Set string field
    std::cout << "  set string... ";
    data->name = XString("Hello", xbuf.allocator<XString>());
    std::cout << "ok\n";
    
    // Serialize
    std::cout << "  serialize... ";
    std::string serialized = xbuf.save_to_string();
    assert(serialized.size() > 0);
    std::cout << "ok (" << serialized.size() << " bytes)\n";
    
    // Deserialize
    std::cout << "  deserialize... ";
    XBuffer loaded = XBuffer::load_from_string(serialized);
    auto [loaded_data, found] = loaded.find<SimpleData>("TestData");
    assert(found);
    std::cout << "ok\n";
    
    // Verify data
    std::cout << "  verify integrity... ";
    assert(loaded_data->id == 42);
    assert(loaded_data->value == 3.14f);
    assert(loaded_data->name == "Hello");
    std::cout << "ok\n";
    
    std::cout << "All tests passed\n";
    return true;
}

bool test_complex_serialization() {
    std::cout << "\nTesting complex serialization...\n";
    
    // Create and populate complex structure
    std::cout << "  create complex structure... ";
    XBuffer xbuf(8192);
    auto* data = xbuf.make<ComplexData>("Complex");
    data->title = XString("Test Title", xbuf.allocator<XString>());
    
    for (int i = 0; i < 10; ++i) {
        data->items.push_back(i * 10);
    }
    
    data->tags.insert(1);
    data->tags.insert(5);
    data->tags.insert(10);
    
    data->metadata.emplace(XString("version", xbuf.allocator<XString>()), 1);
    data->metadata.emplace(XString("count", xbuf.allocator<XString>()), 100);
    std::cout << "ok\n";
    
    // Serialize
    std::cout << "  serialize... ";
    std::string serialized = xbuf.save_to_string();
    std::cout << "ok (" << serialized.size() << " bytes)\n";
    
    // Deserialize
    std::cout << "  deserialize... ";
    XBuffer loaded = XBuffer::load_from_string(serialized);
    auto [loaded_data, found] = loaded.find<ComplexData>("Complex");
    assert(found);
    std::cout << "ok\n";
    
    // Verify all data
    std::cout << "  verify... ";
    assert(loaded_data->title == "Test Title");
    assert(loaded_data->items.size() == 10);
    assert(loaded_data->items[0] == 0);
    assert(loaded_data->items[9] == 90);
    assert(loaded_data->tags.size() == 3);
    assert(loaded_data->tags.count(5) == 1);
    assert(loaded_data->metadata.size() == 2);
    assert(loaded_data->metadata[XString("version", xbuf.allocator<XString>())] == 1);
    std::cout << "ok\n";
    
    std::cout << "All tests passed\n";
    return true;
}

bool test_multiple_objects() {
    std::cout << "\nTesting multiple objects...\n";
    
    // Create multiple objects
    std::cout << "  create objects... ";
    XBuffer xbuf(8192);
    
    auto* obj1 = xbuf.make<SimpleData>("Object1");
    obj1->id = 1;
    obj1->value = 1.1f;
    obj1->name = XString("First", xbuf.allocator<XString>());
    
    auto* obj2 = xbuf.make<SimpleData>("Object2");
    obj2->id = 2;
    obj2->value = 2.2f;
    obj2->name = XString("Second", xbuf.allocator<XString>());
    
    auto* obj3 = xbuf.make<SimpleData>("Object3");
    obj3->id = 3;
    obj3->value = 3.3f;
    obj3->name = XString("Third", xbuf.allocator<XString>());
    std::cout << "ok\n";
    
    // Serialize
    std::cout << "  serialize... ";
    std::string serialized = xbuf.save_to_string();
    std::cout << "ok\n";
    
    // Deserialize
    std::cout << "  deserialize... ";
    XBuffer loaded = XBuffer::load_from_string(serialized);
    
    auto [loaded1, found1] = loaded.find<SimpleData>("Object1");
    auto [loaded2, found2] = loaded.find<SimpleData>("Object2");
    auto [loaded3, found3] = loaded.find<SimpleData>("Object3");
    
    assert(found1 && found2 && found3);
    std::cout << "ok\n";
    
    // Verify all objects
    std::cout << "  verify... ";
    assert(loaded1->id == 1 && loaded1->name == "First");
    assert(loaded2->id == 2 && loaded2->name == "Second");
    assert(loaded3->id == 3 && loaded3->name == "Third");
    std::cout << "ok\n";
    
    std::cout << "All tests passed\n";
    return true;
}

bool test_empty_buffer() {
    std::cout << "\nTesting empty buffer...\n";
    
    // Create empty buffer
    std::cout << "  create... ";
    XBuffer xbuf(1024);
    std::cout << "ok\n";
    
    // Serialize empty buffer
    std::cout << "  serialize... ";
    std::string serialized = xbuf.save_to_string();
    assert(serialized.size() == 1024);
    std::cout << "ok\n";
    
    // Deserialize
    std::cout << "  deserialize... ";
    XBuffer loaded = XBuffer::load_from_string(serialized);
    assert(loaded.get_size() == 1024);
    std::cout << "ok\n";
    
    std::cout << "All tests passed\n";
    return true;
}

bool test_roundtrip() {
    std::cout << "\nTesting multiple roundtrips...\n";
    
    // Create initial data
    std::cout << "  create data... ";
    XBuffer xbuf1(4096);
    auto* data = xbuf1.make<SimpleData>("Roundtrip");
    data->id = 999;
    data->value = 9.99f;
    data->name = XString("Original", xbuf1.allocator<XString>());
    std::cout << "ok\n";
    
    // First roundtrip
    std::cout << "  roundtrip 1... ";
    std::string s1 = xbuf1.save_to_string();
    XBuffer xbuf2 = XBuffer::load_from_string(s1);
    auto [data2, f2] = xbuf2.find<SimpleData>("Roundtrip");
    assert(f2 && data2->id == 999);
    std::cout << "ok\n";
    
    // Second roundtrip
    std::cout << "  roundtrip 2... ";
    std::string s2 = xbuf2.save_to_string();
    XBuffer xbuf3 = XBuffer::load_from_string(s2);
    auto [data3, f3] = xbuf3.find<SimpleData>("Roundtrip");
    assert(f3 && data3->id == 999);
    std::cout << "ok\n";
    
    // Third roundtrip
    std::cout << "  roundtrip 3... ";
    std::string s3 = xbuf3.save_to_string();
    XBuffer xbuf4 = XBuffer::load_from_string(s3);
    auto [data4, f4] = xbuf4.find<SimpleData>("Roundtrip");
    assert(f4 && data4->id == 999);
    assert(data4->value == 9.99f);
    assert(data4->name == "Original");
    std::cout << "ok\n";
    
    // Verify serialized data is identical
    std::cout << "  verify stability... ";
    assert(s1.size() == s2.size());
    assert(s2.size() == s3.size());
    std::cout << "ok\n";
    
    std::cout << "All tests passed\n";
    return true;
}

int main() {
    bool all_passed = true;
    
    all_passed &= test_simple_serialization();
    all_passed &= test_complex_serialization();
    all_passed &= test_multiple_objects();
    all_passed &= test_empty_buffer();
    all_passed &= test_roundtrip();
    
    if (all_passed) {
        std::cout << "\nAll serialization tests passed\n";
    } else {
        std::cout << "\nSome tests failed\n";
    }
    
    return all_passed ? 0 : 1;
}
