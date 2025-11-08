#include "../xoffsetdatastructure2.hpp"
#include <iostream>
#include <cassert>

using namespace XTypeSignature;

// Test struct with const fields
struct ConstFieldsExample {
    const int32_t id;        // const field
    float value;             // non-const field
    const double ratio;      // const field
};

int main() {
    std::cout << "=== Testing const T Support ===" << std::endl;
    std::cout << std::endl;
    
    // Test 1: const int32_t should have same signature as int32_t
    std::cout << "[TEST 1] const int32_t" << std::endl;
    auto sig_int32 = get_XTypeSignature<int32_t>();
    auto sig_const_int32 = get_XTypeSignature<const int32_t>();
    
    std::cout << "int32_t signature:       ";
    sig_int32.print();
    std::cout << std::endl;
    
    std::cout << "const int32_t signature: ";
    sig_const_int32.print();
    std::cout << std::endl;
    
    assert(sig_int32 == sig_const_int32);
    std::cout << "[PASS] const int32_t has same signature as int32_t" << std::endl;
    std::cout << std::endl;
    
    // Test 2: Struct with const fields should be reflectable
    std::cout << "[TEST 2] Struct with const fields" << std::endl;
    auto sig_const_fields = get_XTypeSignature<ConstFieldsExample>();
    
    std::cout << "ConstFieldsExample signature: ";
    sig_const_fields.print();
    std::cout << std::endl;
    
    // Expected: struct[s:16,a:8]{@0:i32[s:4,a:4],@4:f32[s:4,a:4],@8:f64[s:8,a:8]}
    std::cout << "[PASS] Struct with const fields is reflectable" << std::endl;
    std::cout << std::endl;
    
    // Verify sizes
    static_assert(sizeof(ConstFieldsExample) == 16);
    static_assert(alignof(ConstFieldsExample) == 8);
    std::cout << "[PASS] Size and alignment verification" << std::endl;
    std::cout << std::endl;
    
    std::cout << "[SUCCESS] All const T support tests passed!" << std::endl;
    
    return 0;
}
