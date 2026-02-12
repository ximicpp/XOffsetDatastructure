// ============================================================================
// Test: Type-Erased Container Detection
// Purpose: Verify that std::function, std::any, std::shared_ptr, etc.
//          are correctly rejected by is_xbuffer_safe
// ============================================================================

#include <iostream>
#include <cassert>
#include <functional>
#include <memory>
#include <any>
#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// ============================================================================
// Test Data Structures
// ============================================================================

// Safe struct - should pass
struct SafeData {
    template <typename Allocator>
    SafeData(Allocator) {}
    
    int32_t value;
    float score;
};

// Unsafe: contains std::function
struct UnsafeWithFunction {
    template <typename Allocator>
    UnsafeWithFunction(Allocator) {}
    
    int32_t id;
    std::function<void()> callback;  // UNSAFE: type-erased
};

// Unsafe: contains std::any
struct UnsafeWithAny {
    template <typename Allocator>
    UnsafeWithAny(Allocator) {}
    
    int32_t id;
    std::any data;  // UNSAFE: type-erased
};

// Unsafe: contains std::shared_ptr
struct UnsafeWithSharedPtr {
    template <typename Allocator>
    UnsafeWithSharedPtr(Allocator) {}
    
    int32_t id;
    std::shared_ptr<int> ptr;  // UNSAFE: smart pointer
};

// Unsafe: contains std::unique_ptr
struct UnsafeWithUniquePtr {
    template <typename Allocator>
    UnsafeWithUniquePtr(Allocator) {}
    
    int32_t id;
    std::unique_ptr<int> ptr;  // UNSAFE: smart pointer
};

// Unsafe: contains std::weak_ptr
struct UnsafeWithWeakPtr {
    template <typename Allocator>
    UnsafeWithWeakPtr(Allocator) {}
    
    int32_t id;
    std::weak_ptr<int> ptr;  // UNSAFE: smart pointer
};

// ============================================================================
// Tests
// ============================================================================

bool test_safe_types() {
    std::cout << "\n[TEST] Safe Types Detection\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Basic types should be safe
    static_assert(is_xbuffer_safe<int32_t>::value, "int32_t should be safe");
    static_assert(is_xbuffer_safe<float>::value, "float should be safe");
    static_assert(is_xbuffer_safe<double>::value, "double should be safe");
    static_assert(is_xbuffer_safe<bool>::value, "bool should be safe");
    std::cout << "  Basic types: PASS\n";
    
    // SafeData struct should be safe
    static_assert(is_xbuffer_safe<SafeData>::value, "SafeData should be safe");
    std::cout << "  SafeData struct: PASS\n";
    
    return true;
}

bool test_type_erased_detection() {
    std::cout << "\n[TEST] Type-Erased Container Detection\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Direct type checks
    static_assert(!is_xbuffer_safe<std::function<void()>>::value, 
                  "std::function should be unsafe");
    std::cout << "  std::function<void()>: correctly rejected\n";
    
    static_assert(!is_xbuffer_safe<std::any>::value, 
                  "std::any should be unsafe");
    std::cout << "  std::any: correctly rejected\n";
    
    static_assert(!is_xbuffer_safe<std::shared_ptr<int>>::value, 
                  "std::shared_ptr should be unsafe");
    std::cout << "  std::shared_ptr<int>: correctly rejected\n";
    
    static_assert(!is_xbuffer_safe<std::unique_ptr<int>>::value, 
                  "std::unique_ptr should be unsafe");
    std::cout << "  std::unique_ptr<int>: correctly rejected\n";
    
    static_assert(!is_xbuffer_safe<std::weak_ptr<int>>::value, 
                  "std::weak_ptr should be unsafe");
    std::cout << "  std::weak_ptr<int>: correctly rejected\n";
    
    return true;
}

bool test_struct_with_type_erased_members() {
    std::cout << "\n[TEST] Structs with Type-Erased Members\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Structs containing type-erased members should be unsafe
    static_assert(!is_xbuffer_safe<UnsafeWithFunction>::value, 
                  "Struct with std::function should be unsafe");
    std::cout << "  UnsafeWithFunction: correctly rejected\n";
    
    static_assert(!is_xbuffer_safe<UnsafeWithAny>::value, 
                  "Struct with std::any should be unsafe");
    std::cout << "  UnsafeWithAny: correctly rejected\n";
    
    static_assert(!is_xbuffer_safe<UnsafeWithSharedPtr>::value, 
                  "Struct with std::shared_ptr should be unsafe");
    std::cout << "  UnsafeWithSharedPtr: correctly rejected\n";
    
    static_assert(!is_xbuffer_safe<UnsafeWithUniquePtr>::value, 
                  "Struct with std::unique_ptr should be unsafe");
    std::cout << "  UnsafeWithUniquePtr: correctly rejected\n";
    
    static_assert(!is_xbuffer_safe<UnsafeWithWeakPtr>::value, 
                  "Struct with std::weak_ptr should be unsafe");
    std::cout << "  UnsafeWithWeakPtr: correctly rejected\n";
    
    return true;
}

bool test_reason_messages() {
    std::cout << "\n[TEST] Safety Reason Messages\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Check that reason() returns meaningful messages
    std::cout << "  SafeData: " << is_xbuffer_safe<SafeData>::reason() << "\n";
    std::cout << "  std::function: " << is_xbuffer_safe<std::function<void()>>::reason() << "\n";
    std::cout << "  std::any: " << is_xbuffer_safe<std::any>::reason() << "\n";
    std::cout << "  std::shared_ptr: " << is_xbuffer_safe<std::shared_ptr<int>>::reason() << "\n";
    
    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "  Type-Erased Container Detection Test\n";
    std::cout << "========================================\n";
    
    bool all_passed = true;
    
    all_passed &= test_safe_types();
    all_passed &= test_type_erased_detection();
    all_passed &= test_struct_with_type_erased_members();
    all_passed &= test_reason_messages();
    
    std::cout << "\n========================================\n";
    if (all_passed) {
        std::cout << "[PASS] All type-erased detection tests passed!\n";
        return 0;
    } else {
        std::cout << "[FAIL] Some tests failed!\n";
        return 1;
    }
}
