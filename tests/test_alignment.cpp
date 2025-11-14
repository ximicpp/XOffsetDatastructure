// Test alignment verification

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// Test structures with and without alignment
struct alignas(BASIC_ALIGNMENT) AlignedStruct {
    template <typename Allocator>
    AlignedStruct(Allocator allocator) : name(allocator) {}
    
    char a;      // 1 byte
    int b;       // 4 bytes
    double c;    // 8 bytes
    XString name;
};

struct UnalignedStruct {
    template <typename Allocator>
    UnalignedStruct(Allocator allocator) : name(allocator) {}
    
    char a;      // 1 byte
    int b;       // 4 bytes
    double c;    // 8 bytes
    float value; // 4 bytes
    XString name;
};

bool test_alignment_specification() {
    std::cout << "\nTesting alignment specification...\n";
    
    std::cout << "  BASIC_ALIGNMENT: " << BASIC_ALIGNMENT << " bytes... ";
    assert(BASIC_ALIGNMENT == 8);
    std::cout << "ok\n";
    
    std::cout << "  AlignedStruct alignment... ";
    std::size_t aligned_alignment = alignof(AlignedStruct);
    assert(aligned_alignment >= BASIC_ALIGNMENT);
    std::cout << "ok (" << aligned_alignment << " bytes)\n";
    
    std::cout << "  UnalignedStruct alignment... ";
    std::size_t unaligned_alignment = alignof(UnalignedStruct);
    std::cout << "ok (" << unaligned_alignment << " bytes)\n";
    
    std::cout << "  verify aligned >= unaligned... ";
    assert(aligned_alignment >= unaligned_alignment);
    std::cout << "ok\n";
    
    std::cout << "All tests passed\n";
    return true;
}

bool test_aligned_allocation() {
    std::cout << "\nTesting aligned allocation...\n";
    
    XBuffer xbuf(8192);
    
    std::cout << "  allocate... ";
    auto* aligned = xbuf.make<AlignedStruct>("Aligned");
    aligned->a = 1;
    aligned->b = 2;
    aligned->c = 3.14;
    aligned->name = XString("AlignedData", xbuf.allocator<XString>());
    std::cout << "ok\n";
    
    std::cout << "  check address... ";
    std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(aligned);
    bool is_aligned = (addr % BASIC_ALIGNMENT) == 0;
    std::cout << "ok (0x" << std::hex << addr << std::dec << ", " << (is_aligned ? "aligned" : "unaligned") << ")\n";
    
    std::cout << "  verify integrity... ";
    assert(aligned->a == 1);
    assert(aligned->b == 2);
    assert(aligned->c == 3.14);
    assert(aligned->name == "AlignedData");
    std::cout << "ok\n";
    
    std::cout << "All tests passed\n";
    return true;
}

bool test_serialization_with_alignment() {
    std::cout << "\nTesting serialization with alignment...\n";
    
    std::cout << "  create and serialize... ";
    XBuffer xbuf(4096);
    auto* data = xbuf.make<AlignedStruct>("Data");
    data->a = 1;
    data->b = 2;
    data->c = 2.718;
    data->name = XString("TestData", xbuf.allocator<XString>());
    
    std::string serialized = xbuf.save_to_string();
    std::cout << "ok\n";
    
    std::cout << "  deserialize and verify... ";
    XBuffer loaded = XBuffer::load_from_string(serialized);
    auto [loaded_data, found] = loaded.find<AlignedStruct>("Data");
    assert(found);
    assert(loaded_data->a == 1);
    assert(loaded_data->b == 2);
    assert(loaded_data->c == 2.718);
    assert(loaded_data->name == "TestData");
    std::cout << "ok\n";
    
    std::cout << "All tests passed\n";
    return true;
}

bool test_mixed_alignment() {
    std::cout << "\nTesting mixed alignment...\n";
    
    XBuffer xbuf(8192);
    
    std::cout << "  create objects... ";
    auto* aligned = xbuf.make<AlignedStruct>("Aligned");
    aligned->a = 1;
    aligned->b = 2;
    aligned->c = 1.0;
    aligned->name = XString("Aligned", xbuf.allocator<XString>());
    
    auto* unaligned = xbuf.make<UnalignedStruct>("Unaligned");
    unaligned->a = 10;
    unaligned->b = 20;
    unaligned->c = 2.0;
    unaligned->value = 3.14f;
    unaligned->name = XString("Unaligned", xbuf.allocator<XString>());
    std::cout << "ok\n";
    
    std::cout << "  verify... ";
    assert(aligned->b == 2);
    assert(unaligned->b == 20);
    std::cout << "ok\n";
    
    std::cout << "  serialize/deserialize... ";
    std::string serialized = xbuf.save_to_string();
    XBuffer loaded = XBuffer::load_from_string(serialized);
    
    auto [a, fa] = loaded.find<AlignedStruct>("Aligned");
    auto [u, fu] = loaded.find<UnalignedStruct>("Unaligned");
    
    assert(fa && fu);
    assert(a->name == "Aligned");
    assert(u->name == "Unaligned");
    std::cout << "ok\n";
    
    std::cout << "All tests passed\n";
    return true;
}

int main() {
    bool all_passed = true;
    
    all_passed &= test_alignment_specification();
    all_passed &= test_aligned_allocation();
    all_passed &= test_serialization_with_alignment();
    all_passed &= test_mixed_alignment();
    
    if (all_passed) {
        std::cout << "\nAll alignment tests passed\n";
    } else {
        std::cout << "\nSome tests failed\n";
    }
    
    return all_passed ? 0 : 1;
}
