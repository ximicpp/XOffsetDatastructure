// ============================================================================
// Test: Basic Types
// Purpose: Test basic POD types (int, float, double, etc.)
// ============================================================================

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure2.hpp"
#include "basic_types.hpp"

using namespace XOffsetDatastructure2;

bool test_basic_types() {
    std::cout << "\n[TEST] Basic Types\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Create buffer
    XBuffer xbuf(1024);
    
    // Create and initialize object
    auto* obj = xbuf.construct<BasicTypes>("BasicTypes")(xbuf.get_segment_manager());
    obj->mInt = 42;
    obj->mFloat = 3.14f;
    obj->mDouble = 2.71828;
    obj->mChar = 'A';
    obj->mBool = true;
    obj->mLongLong = 9223372036854775807LL;
    
    // Verify
    assert(obj->mInt == 42);
    assert(obj->mFloat == 3.14f);
    assert(obj->mDouble == 2.71828);
    assert(obj->mChar == 'A');
    assert(obj->mBool == true);
    assert(obj->mLongLong == 9223372036854775807LL);
    
    // Test persistence
    auto* buffer = xbuf.get_buffer();
    XBuffer loaded_buf(buffer->data(), buffer->size());
    auto* loaded_obj = loaded_buf.find<BasicTypes>("BasicTypes").first;
    
    assert(loaded_obj->mInt == 42);
    assert(loaded_obj->mFloat == 3.14f);
    assert(loaded_obj->mDouble == 2.71828);
    assert(loaded_obj->mChar == 'A');
    assert(loaded_obj->mBool == true);
    assert(loaded_obj->mLongLong == 9223372036854775807LL);
    
    std::cout << "[PASS] All basic type tests passed!\n";
    return true;
}

int main() {
    try {
        return test_basic_types() ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}