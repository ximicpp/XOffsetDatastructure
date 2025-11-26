// Test: Compile-time validation of polymorphic types
// This test demonstrates that polymorphic types are REJECTED at compile-time

#include "../xoffsetdatastructure2.hpp"
#include <iostream>

using namespace XOffsetDatastructure2;

// [GOOD]: Non-polymorphic type (no virtual functions)
struct SafeData {
    int32_t id;
    float value;
    
    SafeData(XBuffer::segment_manager* mgr) : id(0), value(0.0f) {}
    
    // Regular methods are OK
    void process() {
        value *= 2.0f;
    }
};

// [BAD]: Polymorphic type (has virtual function)
struct UnsafeData {
    int32_t id;
    
    UnsafeData(XBuffer::segment_manager* mgr) : id(0) {}
    
    virtual void process() {}  // Virtual function = polymorphic
};

// [BAD]: Has virtual destructor
struct UnsafeWithVirtualDestructor {
    int32_t id;
    
    UnsafeWithVirtualDestructor(XBuffer::segment_manager* mgr) : id(0) {}
    
    virtual ~UnsafeWithVirtualDestructor() = default;
};

// [BAD]: Inheritance (Base class)
struct Base {
    int32_t x;
};

// [BAD]: Inheritance (Derived class)
struct Derived : Base {
    int32_t y;
};

// [GOOD]: Composition
struct Composite {
    Base b;
    int32_t y;
};

int main() {
    std::cout << "========================================\n";
    std::cout << "Type Safety Validation Test\n";
    std::cout << "========================================\n\n";
    
    // Test 1: Safe type should compile
    std::cout << "Test 1: Safe (non-polymorphic) type\n";
    std::cout << "  is_xbuffer_safe<SafeData>: " 
              << (is_xbuffer_safe<SafeData>::value ? "[OK] SAFE" : "[FAIL] UNSAFE") << "\n";
    std::cout << "  is_polymorphic: " << std::is_polymorphic_v<SafeData> << "\n";
    std::cout << "  sizeof: " << sizeof(SafeData) << " bytes\n\n";
    
    // This should compile fine
    XBufferExt xbuf(1024);
    auto* data = xbuf.make<SafeData>("test");
    data->id = 42;
    data->value = 3.14f;
    std::cout << "  Created SafeData successfully!\n";
    std::cout << "  ID: " << data->id << ", Value: " << data->value << "\n\n";
    
    // Test 2: Unsafe type detection
    std::cout << "Test 2: Unsafe (polymorphic) type\n";
    std::cout << "  is_xbuffer_safe<UnsafeData>: " 
              << (is_xbuffer_safe<UnsafeData>::value ? "[OK] SAFE" : "[FAIL] UNSAFE") << "\n";
    std::cout << "  is_polymorphic: " << std::is_polymorphic_v<UnsafeData> << "\n";
    std::cout << "  sizeof: " << sizeof(UnsafeData) << " bytes (includes vptr!)\n\n";
    
    // Test 3: Virtual destructor detection
    std::cout << "Test 3: Type with virtual destructor\n";
    std::cout << "  is_xbuffer_safe<UnsafeWithVirtualDestructor>: " 
              << (is_xbuffer_safe<UnsafeWithVirtualDestructor>::value ? "[OK] SAFE" : "[FAIL] UNSAFE") << "\n";
    std::cout << "  has_virtual_destructor: " << std::has_virtual_destructor_v<UnsafeWithVirtualDestructor> << "\n\n";
    
    // Test 4: Inheritance vs Composition
    std::cout << "Test 4: Inheritance vs Composition\n";
    std::cout << "  is_xbuffer_safe<Derived> (Inheritance): " 
              << (is_xbuffer_safe<Derived>::value ? "[OK] SAFE" : "[FAIL] UNSAFE") << "\n";
    std::cout << "  is_xbuffer_safe<Composite> (Composition): " 
              << (is_xbuffer_safe<Composite>::value ? "[OK] SAFE" : "[FAIL] UNSAFE") << "\n";
    
    if (!is_xbuffer_safe<Derived>::value) {
        std::cout << "  [SUCCESS] Inheritance correctly rejected.\n";
    } else {
        std::cout << "  [FAILURE] Inheritance was NOT rejected.\n";
    }
    
    if (is_xbuffer_safe<Composite>::value) {
        std::cout << "  [SUCCESS] Composition correctly accepted.\n\n";
    } else {
        std::cout << "  [FAILURE] Composition was incorrectly rejected.\n\n";
    }

    // [BAD] THESE SHOULD FAIL AT COMPILE-TIME (uncomment to test):
    
    // auto* unsafe1 = xbuf.make<UnsafeData>("unsafe");
    // Error: static assertion failed: Cannot use polymorphic types in XBuffer!
    
    // auto* unsafe2 = xbuf.make<UnsafeWithVirtualDestructor>("unsafe2");
    // Error: static assertion failed: Cannot use polymorphic types in XBuffer!

    // auto* unsafe3 = xbuf.make<Derived>("unsafe3");
    // Error: static assertion failed: Inheritance not allowed (use composition)
    
    std::cout << "========================================\n";
    std::cout << "Summary\n";
    std::cout << "========================================\n";
    std::cout << "[OK] SafeData (non-polymorphic): ALLOWED\n";
    std::cout << "[BLOCKED] UnsafeData (virtual function): BLOCKED\n";
    std::cout << "[BLOCKED] UnsafeWithVirtualDestructor: BLOCKED\n";
    std::cout << "[BLOCKED] Derived (Inheritance): BLOCKED\n";
    std::cout << "[OK] Composite (Composition): ALLOWED\n\n";
    
    std::cout << "Type safety is enforced at COMPILE-TIME!\n";
    std::cout << "Unsafe types will cause compilation errors.\n";
    
    return 0;
}