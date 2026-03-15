// ============================================================================
// Test: Type Signatures — TypeLayout Signature Generation
// Purpose: Tests boost::typelayout signature generation for various type
//          configurations: class/struct variants, alignment, access control,
//          templates, composition, and reflection-based member verification.
//
// Consolidated from:
//   - test_reflection_type_signature.cpp (reflection + TypeLayout integration)
//   - test_class_type_signatures.cpp     (comprehensive class variants)
// ============================================================================

#include "../xoffsetdatastructure.hpp"
#include <iostream>
#include <cassert>
#include <experimental/meta>

using namespace XOffsetDatastructure;
using namespace boost::typelayout;

// ---------------------------------------------------------------------------
// Shared test structures
// ---------------------------------------------------------------------------

struct SimpleStruct { int32_t a; float b; };

class SimpleClass { public: int32_t a; float b; };

class alignas(16) AlignedClass { public: int32_t a; double b; };

class ClassWithPrivate {
public:  int32_t pub;
private: float   priv;
};

class ClassWithMethods {
public:
    int32_t data;
    void set(int32_t v) { data = v; }
    int32_t get() const { return data; }
    static int32_t answer() { return 42; }
};

template<typename T>
class TemplateClass { public: T value; int32_t count; };

class OuterClass {
public:
    int32_t outer;
    class InnerClass { public: int32_t inner; };
};

class BaseClass { public: int32_t base_value; };
class DerivedClass : public BaseClass { public: float derived_value; };

class EmptyClass {};

class ClassWithBitFields {
public:
    uint32_t f1 : 1;
    uint32_t f2 : 1;
    uint32_t val : 30;
};

class PolymorphicClass {
public:
    int32_t data;
    virtual void vfunc() {}
    virtual ~PolymorphicClass() {}
};

class ClassWithConst { public: const int32_t cv = 42; int32_t nv; };

class ClassWithArrays { public: int32_t arr1[4]; char arr2[16]; };

class __attribute__((packed)) PackedClass { public: char a; int32_t b; char c; };

struct Component { int32_t id; float value; };
struct CompositeStruct { Component comp; int32_t extra; };

struct TypeSigTest {
    int a; double b; XString c; XVector<int> d;
    template <typename Allocator>
    TypeSigTest(Allocator alloc) : a(0), b(0.0), c(alloc), d(alloc) {}
};

// ---------------------------------------------------------------------------
// Signature print helper
// ---------------------------------------------------------------------------

template<typename T>
void print_sig(const char* label) {
    constexpr auto sig = get_layout_signature<T>();
    std::cout << "  " << label << ": " << sig
              << "  (size=" << sizeof(T) << ", align=" << alignof(T) << ")\n";
}

// ---------------------------------------------------------------------------
// Test 1: Basic struct/class signatures
// ---------------------------------------------------------------------------

void test_basic_signatures() {
    std::cout << "[Test 1] Basic Struct / Class Signatures\n";
    std::cout << std::string(50, '-') << "\n";

    print_sig<SimpleStruct>("SimpleStruct");
    print_sig<SimpleClass>("SimpleClass");
    print_sig<AlignedClass>("AlignedClass(16)");
    print_sig<ClassWithPrivate>("ClassWithPrivate");
    print_sig<ClassWithMethods>("ClassWithMethods");

    // struct and class with same layout should match
    constexpr bool match = layout_signatures_match<SimpleStruct, SimpleClass>();
    static_assert(match, "SimpleStruct and SimpleClass have identical ABI");
    std::cout << "  struct==class layout: " << match << " [OK]\n";
    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// Test 2: Template, nested, inheritance signatures
// ---------------------------------------------------------------------------

void test_advanced_signatures() {
    std::cout << "[Test 2] Template / Nested / Inheritance Signatures\n";
    std::cout << std::string(50, '-') << "\n";

    print_sig<TemplateClass<int32_t>>("Template<i32>");
    print_sig<TemplateClass<float>>("Template<f32>");
    print_sig<TemplateClass<double>>("Template<f64>");
    print_sig<OuterClass>("OuterClass");
    print_sig<OuterClass::InnerClass>("InnerClass");
    print_sig<BaseClass>("BaseClass");
    print_sig<DerivedClass>("DerivedClass");

    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// Test 3: Special types (empty, bitfields, polymorphic, packed, arrays)
// ---------------------------------------------------------------------------

void test_special_signatures() {
    std::cout << "[Test 3] Special Types\n";
    std::cout << std::string(50, '-') << "\n";

    print_sig<EmptyClass>("EmptyClass");
    print_sig<ClassWithBitFields>("BitFields");
    print_sig<PolymorphicClass>("Polymorphic");
    print_sig<ClassWithConst>("WithConst");
    print_sig<ClassWithArrays>("WithArrays");
    print_sig<PackedClass>("Packed");

    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// Test 4: Composition signature (inner struct embedded)
// ---------------------------------------------------------------------------

void test_composition_signature() {
    std::cout << "[Test 4] Composition Signature\n";
    std::cout << std::string(50, '-') << "\n";

    print_sig<Component>("Component");
    print_sig<CompositeStruct>("CompositeStruct");

    constexpr auto sig = get_layout_signature<CompositeStruct>();
    std::string s = sig.value;
    // Composite should contain inner struct details
    bool has_inner = s.find("i32") != std::string::npos && s.find("f32") != std::string::npos;
    assert(has_inner);
    std::cout << "  Contains inner struct fields: " << has_inner << " [OK]\n";
    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// Test 5: Reflection + Signature — member type consistency
// ---------------------------------------------------------------------------

void test_reflection_signature_consistency() {
    using namespace std::meta;

    std::cout << "[Test 5] Reflection + Signature Consistency\n";
    std::cout << std::string(50, '-') << "\n";

    print_sig<TypeSigTest>("TypeSigTest");

    // Verify member types via reflection (dealias to resolve typedefs)
    constexpr bool a_ok = (dealias(type_of(^^TypeSigTest::a)) == dealias(^^int));
    constexpr bool b_ok = (dealias(type_of(^^TypeSigTest::b)) == dealias(^^double));
    constexpr bool c_ok = (dealias(type_of(^^TypeSigTest::c)) == dealias(^^XString));
    constexpr bool d_ok = (dealias(type_of(^^TypeSigTest::d)) == dealias(^^XVector<int>));
    static_assert(a_ok && b_ok && c_ok && d_ok);

    std::cout << "  Reflection type checks: a=" << a_ok << " b=" << b_ok
              << " c=" << c_ok << " d=" << d_ok << " [OK]\n";

    // Serialize round-trip
    XBuffer xbuf(2048);
    auto* obj = xbuf.make<TypeSigTest>();
    obj->a = 42; obj->b = 3.14; obj->c = "Test"; obj->d.push_back(1);

    std::string bin = xbuf.save();
    XBuffer xbuf2 = XBuffer::load(bin);
    auto& loaded = xbuf2.root<TypeSigTest>();
    assert(loaded.a == 42 && loaded.b == 3.14);
    assert(loaded.d.size() == 1 && loaded.d[0] == 1);
    std::cout << "  Serialize round-trip: a=" << loaded.a
              << " d.size=" << loaded.d.size() << " [OK]\n";
    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    std::cout << "========================================\n";
    std::cout << "  Type Signatures Test\n";
    std::cout << "========================================\n\n";

    test_basic_signatures();
    test_advanced_signatures();
    test_special_signatures();
    test_composition_signature();
    test_reflection_signature_consistency();

    std::cout << "========================================\n";
    std::cout << "[SUCCESS] All type signature tests passed!\n";
    return 0;
}
