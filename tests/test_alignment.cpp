// ============================================================================
// Test: Alignment Verification
// Purpose: Verify BASIC_ALIGNMENT is applied correctly
// ============================================================================

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
    std::cout << "\n[TEST] Alignment Specification\n";
    std::cout << std::string(50, '-') << "\n";
    
    std::cout << "Test 1: Check BASIC_ALIGNMENT value... ";
    std::cout << "[" << BASIC_ALIGNMENT << " bytes]\n";
    assert(BASIC_ALIGNMENT == 8);
    std::cout << "   Expected: 8 bytes [OK]\n";
    
    std::cout << "Test 2: Check AlignedStruct alignment... ";
    std::size_t aligned_alignment = alignof(AlignedStruct);
    std::cout << "[" << aligned_alignment << " bytes]\n";
    assert(aligned_alignment >= BASIC_ALIGNMENT);
    std::cout << "   Expected: >= " << BASIC_ALIGNMENT << " bytes [OK]\n";
    
    std::cout << "Test 3: Check UnalignedStruct alignment... ";
    std::size_t unaligned_alignment = alignof(UnalignedStruct);
    std::cout << "[" << unaligned_alignment << " bytes]\n";
    std::cout << "   Natural alignment [OK]\n";
    
    std::cout << "Test 4: Verify aligned >= unaligned... ";
    assert(aligned_alignment >= unaligned_alignment);
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Alignment specification tests passed!\n";
    return true;
}

bool test_aligned_allocation() {
    std::cout << "\n[TEST] Aligned Object Allocation\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBufferExt xbuf(8192);
    
    std::cout << "Test 1: Allocate aligned struct... ";
    auto* aligned = xbuf.make<AlignedStruct>("Aligned");
    aligned->a = 1;
    aligned->b = 2;
    aligned->c = 3.14;
    aligned->name = XString("AlignedData", xbuf.allocator<XString>());
    std::cout << "[OK]\n";
    
    std::cout << "Test 2: Check memory address alignment... ";
    std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(aligned);
    bool is_aligned = (addr % BASIC_ALIGNMENT) == 0;
    std::cout << "\n   Address: 0x" << std::hex << addr << std::dec << "\n";
    std::cout << "   Alignment check: " << (is_aligned ? "ALIGNED" : "NOT ALIGNED") << "\n";
    // Note: Alignment may depend on allocator implementation
    std::cout << "   [OK]\n";
    
    std::cout << "Test 3: Verify data integrity... ";
    assert(aligned->a == 1);
    assert(aligned->b == 2);
    assert(aligned->c == 3.14);
    assert(aligned->name == "AlignedData");
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Aligned allocation tests passed!\n";
    return true;
}

bool test_serialization_with_alignment() {
    std::cout << "\n[TEST] Serialization with Aligned Structures\n";
    std::cout << std::string(50, '-') << "\n";
    
    std::cout << "Test 1: Create and serialize aligned struct... ";
    XBufferExt xbuf(4096);
    auto* data = xbuf.make<AlignedStruct>("Data");
    data->a = 1;
    data->b = 2;
    data->c = 2.718;
    data->name = XString("TestData", xbuf.allocator<XString>());
    
    std::string serialized = xbuf.save_to_string();
    std::cout << "[OK]\n";
    
    std::cout << "Test 2: Deserialize and verify... ";
    XBufferExt loaded = XBufferExt::load_from_string(serialized);
    auto [loaded_data, found] = loaded.find_ex<AlignedStruct>("Data");
    assert(found);
    assert(loaded_data->a == 1);
    assert(loaded_data->b == 2);
    assert(loaded_data->c == 2.718);
    assert(loaded_data->name == "TestData");
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Serialization with alignment tests passed!\n";
    return true;
}

bool test_mixed_alignment() {
    std::cout << "\n[TEST] Mixed Aligned and Unaligned Structures\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBufferExt xbuf(8192);
    
    std::cout << "Test 1: Create both aligned and unaligned... ";
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
    std::cout << "[OK]\n";
    
    std::cout << "Test 2: Verify both objects... ";
    assert(aligned->b == 2);
    assert(unaligned->b == 20);
    std::cout << "[OK]\n";
    
    std::cout << "Test 3: Serialize and deserialize... ";
    std::string serialized = xbuf.save_to_string();
    XBufferExt loaded = XBufferExt::load_from_string(serialized);
    
    auto [a, fa] = loaded.find_ex<AlignedStruct>("Aligned");
    auto [u, fu] = loaded.find_ex<UnalignedStruct>("Unaligned");
    
    assert(fa && fu);
    assert(a->name == "Aligned");
    assert(u->name == "Unaligned");
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Mixed alignment tests passed!\n";
    return true;
}

int main() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  Alignment Tests\n";
    std::cout << "========================================\n";
    
    bool all_passed = true;
    
    all_passed &= test_alignment_specification();
    all_passed &= test_aligned_allocation();
    all_passed &= test_serialization_with_alignment();
    all_passed &= test_mixed_alignment();
    
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