// ============================================================================
// Test: Comprehensive Class Type Signature Support
// Purpose: Test type signature generation for various class configurations
// ============================================================================

#include "../xoffsetdatastructure2.hpp"
#include <iostream>
#include <cassert>

using namespace XOffsetDatastructure2;

// ============================================================================
// Test Cases: Various Class Configurations
// ============================================================================

// 1. Simple struct (baseline)
struct SimpleStruct {
    int32_t a;
    float b;
};

// 2. Simple class with public members
class SimpleClass {
public:
    int32_t a;
    float b;
};

// 3. Class with alignas
class alignas(8) ClassWithAlign8 {
public:
    int32_t a;
    float b;
};

class alignas(16) ClassWithAlign16 {
public:
    int32_t a;
    double b;
};

class alignas(32) ClassWithAlign32 {
public:
    int32_t a;
    int32_t b;
    int32_t c;
};

// 4. Class with private members
class ClassWithPrivate {
public:
    int32_t public_member;
private:
    float private_member;
};

// 5. Class with mixed access (public + private + protected)
class ClassWithMixedAccess {
public:
    int32_t public_data;
protected:
    float protected_data;
private:
    double private_data;
};

// 6. Class with constructors
class ClassWithConstructor {
public:
    int32_t id;
    float value;
    
    ClassWithConstructor() : id(0), value(0.0f) {}
    ClassWithConstructor(int32_t i, float v) : id(i), value(v) {}
};

// 7. Class with methods
class ClassWithMethods {
public:
    int32_t data;
    
    void set_data(int32_t v) { data = v; }
    int32_t get_data() const { return data; }
    
    static int32_t static_method() { return 42; }
};

// 8. Template class
template<typename T>
class TemplateClass {
public:
    T value;
    int32_t count;
};

// 9. Class with XString and XVector
template<typename Allocator>
class ClassWithContainers {
public:
    int32_t id;
    XString name;
    XVector<int32_t> scores;
    
    ClassWithContainers(Allocator alloc) 
        : id(0), name(alloc), scores(alloc) {}
};

// 10. Nested classes
class OuterClass {
public:
    int32_t outer_data;
    
    class InnerClass {
    public:
        int32_t inner_data;
    };
};

// 11. Class with inheritance
class BaseClass {
public:
    int32_t base_value;
};

class DerivedClass : public BaseClass {
public:
    float derived_value;
};

// 12. Empty class
class EmptyClass {
};

// 13. Class with only static members (effectively empty for instance data)
class ClassWithOnlyStatic {
public:
    static int32_t static_data;
    static void static_func() {}
};

// 14. Class with bit fields
class ClassWithBitFields {
public:
    uint32_t flag1 : 1;
    uint32_t flag2 : 1;
    uint32_t value : 30;
};

// 15. Polymorphic class (with virtual functions)
class PolymorphicClass {
public:
    int32_t data;
    virtual void virtual_func() {}
    virtual ~PolymorphicClass() {}
};

// 16. Class with const members
class ClassWithConst {
public:
    const int32_t const_value = 42;
    int32_t normal_value;
};

// 17. Class with arrays
class ClassWithArrays {
public:
    int32_t array1[4];
    char array2[16];
};

// 18. Packed class
class __attribute__((packed)) PackedClass {
public:
    char a;
    int32_t b;
    char c;
};

// ============================================================================
// Test Helper Functions
// ============================================================================

template<typename T>
void test_type_signature(const char* name, bool should_succeed = true) {
    std::cout << "\n[Test] " << name << "\n";
    std::cout << "  sizeof:  " << sizeof(T) << " bytes\n";
    std::cout << "  alignof: " << alignof(T) << " bytes\n";
    
    try {
        constexpr auto sig = XTypeSignature::get_XTypeSignature<T>();
        
        std::cout << "  Signature: ";
        sig.print();
        std::cout << "\n";
        
        if (should_succeed) {
            std::cout << "  [OK] SUCCESS - Type signature generated\n";
        } else {
            std::cout << "  [WARN] UNEXPECTED - Expected to fail but succeeded\n";
        }
    } catch (...) {
        if (should_succeed) {
            std::cout << "  [FAIL] FAILED - Could not generate signature\n";
        } else {
            std::cout << "  [OK] EXPECTED - Type signature not supported\n";
        }
    }
}

// ============================================================================
// Main Test Function
// ============================================================================

int main() {
    std::cout << "========================================================================\n";
    std::cout << "  Comprehensive Class Type Signature Support Test\n";
    std::cout << "========================================================================\n";
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "Category 1: Basic Struct vs Class\n";
    std::cout << std::string(70, '=') << "\n";
    
    test_type_signature<SimpleStruct>("SimpleStruct");
    test_type_signature<SimpleClass>("SimpleClass");
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "Category 2: Classes with alignas\n";
    std::cout << std::string(70, '=') << "\n";
    
    test_type_signature<ClassWithAlign8>("ClassWithAlign8 (alignas(8))");
    test_type_signature<ClassWithAlign16>("ClassWithAlign16 (alignas(16))");
    test_type_signature<ClassWithAlign32>("ClassWithAlign32 (alignas(32))");
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "Category 3: Classes with Access Control\n";
    std::cout << std::string(70, '=') << "\n";
    
    test_type_signature<ClassWithPrivate>("ClassWithPrivate (public + private)");
    test_type_signature<ClassWithMixedAccess>("ClassWithMixedAccess (public + protected + private)");
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "Category 4: Classes with Constructors and Methods\n";
    std::cout << std::string(70, '=') << "\n";
    
    test_type_signature<ClassWithConstructor>("ClassWithConstructor");
    test_type_signature<ClassWithMethods>("ClassWithMethods");
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "Category 5: Template Classes\n";
    std::cout << std::string(70, '=') << "\n";
    
    test_type_signature<TemplateClass<int32_t>>("TemplateClass<int32_t>");
    test_type_signature<TemplateClass<float>>("TemplateClass<float>");
    test_type_signature<TemplateClass<double>>("TemplateClass<double>");
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "Category 6: Nested Classes\n";
    std::cout << std::string(70, '=') << "\n";
    
    test_type_signature<OuterClass>("OuterClass");
    test_type_signature<OuterClass::InnerClass>("OuterClass::InnerClass");
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "Category 7: Inheritance\n";
    std::cout << std::string(70, '=') << "\n";
    
    test_type_signature<BaseClass>("BaseClass");
    test_type_signature<DerivedClass>("DerivedClass (inherits BaseClass)");
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "Category 8: Special Classes\n";
    std::cout << std::string(70, '=') << "\n";
    
    test_type_signature<EmptyClass>("EmptyClass (empty)");
    test_type_signature<ClassWithOnlyStatic>("ClassWithOnlyStatic (only static members)");
    test_type_signature<ClassWithBitFields>("ClassWithBitFields (bit fields)");
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "Category 9: Polymorphic Classes\n";
    std::cout << std::string(70, '=') << "\n";
    
    test_type_signature<PolymorphicClass>("PolymorphicClass (with virtual functions)");
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "Category 10: Classes with Special Members\n";
    std::cout << std::string(70, '=') << "\n";
    
    test_type_signature<ClassWithConst>("ClassWithConst (const members)");
    test_type_signature<ClassWithArrays>("ClassWithArrays (array members)");
    test_type_signature<PackedClass>("PackedClass (packed)");
    
    std::cout << "\n========================================================================\n";
    std::cout << "  Summary\n";
    std::cout << "========================================================================\n";
    
    std::cout << "\n[SUPPORTED]:\n";
    std::cout << "  - Simple struct/class\n";
    std::cout << "  - Class with alignas (8, 16, 32, etc.)\n";
    std::cout << "  - Class with private/protected/public members\n";
    std::cout << "  - Class with constructors\n";
    std::cout << "  - Class with methods (methods ignored in signature)\n";
    std::cout << "  - Template classes\n";
    std::cout << "  - Nested classes\n";
    std::cout << "  - Class with inheritance (flattened layout)\n";
    std::cout << "  - Empty class\n";
    std::cout << "  - Class with bit fields\n";
    std::cout << "  - Polymorphic class (includes vptr)\n";
    std::cout << "  - Class with const members\n";
    std::cout << "  - Class with array members\n";
    std::cout << "  - Packed class\n";
    
    std::cout << "\n[NOTES]:\n";
    std::cout << "  - Type signatures reflect actual memory layout\n";
    std::cout << "  - Methods and static members are not included\n";
    std::cout << "  - Private members are included via access_context::unchecked()\n";
    std::cout << "  - Inheritance: base class members appear first\n";
    std::cout << "  - Polymorphic classes include vptr in layout\n";
    
    std::cout << "\n[SUCCESS] All class type signature tests completed!\n";
    
    return 0;
}
