// ============================================================================
// Test: Type Signature Verification
// Purpose: Verify compile-time type signature generation and validation
// ============================================================================

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// Simple test structure
struct alignas(XTypeSignature::BASIC_ALIGNMENT) SimpleStruct {
    int32_t value;
    float ratio;
};

// Structure with containers (runtime version)
struct alignas(XTypeSignature::BASIC_ALIGNMENT) ContainerStruct {
    template <typename Allocator>
    ContainerStruct(Allocator allocator) : data(allocator), name(allocator) {}
    
    int32_t id;
    XVector<int32_t> data;
    XString name;
};

// Reflection hint for ContainerStruct
struct alignas(XTypeSignature::BASIC_ALIGNMENT) ContainerStructReflectionHint {
    int32_t id;
    XVector<int32_t> data;
    XString name;
};

bool test_simple_signature() {
    std::cout << "\n[TEST] Simple Type Signature\n";
    std::cout << std::string(50, '-') << "\n";
    
    constexpr auto sig = XTypeSignature::get_XTypeSignature<SimpleStruct>();
    
    std::cout << "SimpleStruct signature: ";
    sig.print();
    std::cout << "\n";
    
    // Verify expected signature
    // All compilers now use unified Boost.PFR implementation
    constexpr auto expected = XTypeSignature::CompileString{"struct[s:8,a:8]{@0:i32[s:4,a:4],@4:f32[s:4,a:4]}"};
    static_assert(sig == expected, "SimpleStruct signature mismatch");
    
    std::cout << "[PASS] Signature matches expected value\n";
    return true;
}

bool test_container_signature() {
    std::cout << "\n[TEST] Container Type Signature\n";
    std::cout << std::string(50, '-') << "\n";
    
    constexpr auto sig = XTypeSignature::get_XTypeSignature<ContainerStructReflectionHint>();
    
    std::cout << "ContainerStructReflectionHint signature: ";
    sig.print();
    std::cout << "\n";
    
    // Verify layout match
    static_assert(sizeof(ContainerStruct) == sizeof(ContainerStructReflectionHint),
                  "Size mismatch between runtime and reflection types");
    static_assert(alignof(ContainerStruct) == alignof(ContainerStructReflectionHint),
                  "Alignment mismatch between runtime and reflection types");
    
    std::cout << "Runtime size:    " << sizeof(ContainerStruct) << " bytes\n";
    std::cout << "Reflection size: " << sizeof(ContainerStructReflectionHint) << " bytes\n";
    std::cout << "[PASS] Layout validation successful\n";
    
    return true;
}

bool test_nested_signature() {
    std::cout << "\n[TEST] Nested Structure Signature\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Nested structure
    struct Inner {
        int32_t x;
        float y;
    };
    
    struct Outer {
        Inner inner;
        int32_t z;
    };
    
    constexpr auto inner_sig = XTypeSignature::get_XTypeSignature<Inner>();
    constexpr auto outer_sig = XTypeSignature::get_XTypeSignature<Outer>();
    
    std::cout << "Inner signature: ";
    inner_sig.print();
    std::cout << "\n";
    
    std::cout << "Outer signature: ";
    outer_sig.print();
    std::cout << "\n";
    
    std::cout << "[PASS] Nested structure signatures generated\n";
    return true;
}

int main() {
    std::cout << "\n=== Type Signature Verification Tests ===\n";
    
    try {
        bool all_passed = true;
        
        all_passed &= test_simple_signature();
        all_passed &= test_container_signature();
        all_passed &= test_nested_signature();
        
        if (all_passed) {
            std::cout << "\n[SUCCESS] All type signature tests passed!\n\n";
            return 0;
        } else {
            std::cout << "\n[FAILURE] Some tests failed!\n\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception: " << e.what() << "\n";
        return 1;
    }
}
