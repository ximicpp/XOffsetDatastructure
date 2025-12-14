// test_all_types.cpp - Comprehensive type coverage tests
// Compile success = all tests pass (static_assert based)

#include <cstdint>
#include "../include/typelayout.hpp"

using namespace typelayout;

//=============================================================================
// 1. Primitive Types
//=============================================================================

// Integers
static_assert(get_layout_signature<int8_t>() == "i8[s:1,a:1]");
static_assert(get_layout_signature<uint8_t>() == "u8[s:1,a:1]");
static_assert(get_layout_signature<int16_t>() == "i16[s:2,a:2]");
static_assert(get_layout_signature<uint16_t>() == "u16[s:2,a:2]");
static_assert(get_layout_signature<int32_t>() == "i32[s:4,a:4]");
static_assert(get_layout_signature<uint32_t>() == "u32[s:4,a:4]");
static_assert(get_layout_signature<int64_t>() == "i64[s:8,a:8]");
static_assert(get_layout_signature<uint64_t>() == "u64[s:8,a:8]");

// Floating point
static_assert(get_layout_signature<float>() == "f32[s:4,a:4]");
static_assert(get_layout_signature<double>() == "f64[s:8,a:8]");

// Character types
static_assert(get_layout_signature<char>() == "char[s:1,a:1]");
static_assert(get_layout_signature<char8_t>() == "char8[s:1,a:1]");
static_assert(get_layout_signature<char16_t>() == "char16[s:2,a:2]");
static_assert(get_layout_signature<char32_t>() == "char32[s:4,a:4]");

// Boolean
static_assert(get_layout_signature<bool>() == "bool[s:1,a:1]");

// Pointers (all 8 bytes on 64-bit)
static_assert(get_layout_signature<void*>() == "ptr[s:8,a:8]");
static_assert(get_layout_signature<int*>() == "ptr[s:8,a:8]");
static_assert(get_layout_signature<const char*>() == "ptr[s:8,a:8]");
static_assert(get_layout_signature<void**>() == "ptr[s:8,a:8]");

// References
static_assert(get_layout_signature<int&>() == "ref[s:8,a:8]");
static_assert(get_layout_signature<const double&>() == "ref[s:8,a:8]");
static_assert(get_layout_signature<int&&>() == "rref[s:8,a:8]");

// nullptr
static_assert(get_layout_signature<std::nullptr_t>() == "nullptr[s:8,a:8]");

//=============================================================================
// 2. Array Types
//=============================================================================

// char arrays -> bytes
static_assert(get_layout_signature<char[16]>() == "bytes[s:16,a:1]");
static_assert(get_layout_signature<char[64]>() == "bytes[s:64,a:1]");
static_assert(get_layout_signature<char[1]>() == "bytes[s:1,a:1]");

// Regular arrays
static_assert(get_layout_signature<int32_t[4]>() == "array[s:16,a:4]<i32[s:4,a:4],4>");
static_assert(get_layout_signature<double[3]>() == "array[s:24,a:8]<f64[s:8,a:8],3>");
static_assert(get_layout_signature<uint8_t[10]>() == "array[s:10,a:1]<u8[s:1,a:1],10>");

// Multi-dimensional arrays
static_assert(get_layout_signature<int32_t[2][3]>() == "array[s:24,a:4]<array[s:12,a:4]<i32[s:4,a:4],3>,2>");

//=============================================================================
// 3. Struct Types
//=============================================================================

// Simple struct
struct SimpleStruct {
    int32_t a;
    int32_t b;
};
static_assert(get_layout_signature<SimpleStruct>() == 
    "struct[s:8,a:4]{@0[a]:i32[s:4,a:4],@4[b]:i32[s:4,a:4]}");

// Struct with padding
struct PaddedStruct {
    int8_t x;     // offset 0
    // 3 bytes padding
    int32_t y;    // offset 4
    int8_t z;     // offset 8
    // 7 bytes padding to align to 8
};
static_assert(sizeof(PaddedStruct) == 12 || sizeof(PaddedStruct) == 16);

// Nested struct
struct Inner {
    uint16_t val;
};
struct Outer {
    Inner inner;
    uint32_t extra;
};
static_assert(get_layout_signature<Outer>() == 
    "struct[s:8,a:4]{@0[inner]:struct[s:2,a:2]{@0[val]:u16[s:2,a:2]},@4[extra]:u32[s:4,a:4]}");

// Empty struct
struct EmptyStruct {};
static_assert(sizeof(EmptyStruct) == 1);

//=============================================================================
// 4. Inheritance
//=============================================================================

// Single inheritance
struct Base1 {
    uint64_t id;
};
struct Derived1 : Base1 {
    uint32_t value;
};
static_assert(get_layout_signature<Derived1>() == 
    "class[s:16,a:8,inherited]{@0[base]:struct[s:8,a:8]{@0[id]:u64[s:8,a:8]},@8[value]:u32[s:4,a:4]}");

// Multiple inheritance
struct MixinA {
    uint32_t a_val;
};
struct MixinB {
    uint32_t b_val;
};
struct MultiDerived : MixinA, MixinB {
    uint32_t own_val;
};

// Virtual inheritance
struct VBase {
    uint64_t vb_id;
};
struct VDerived1 : virtual VBase {
    float v1;
};
struct VDerived2 : virtual VBase {
    double v2;
};
struct Diamond : VDerived1, VDerived2 {
    int32_t d_val;
};
// Virtual base should show [vbase] marker

//=============================================================================
// 5. Polymorphic Classes
//=============================================================================

// Simple polymorphic
class Polymorphic {
public:
    virtual ~Polymorphic() = default;
private:
    int32_t data;
};
static_assert(sizeof(Polymorphic) == 16); // vptr + data + padding

// Pure virtual
class Interface {
public:
    virtual ~Interface() = default;
    virtual void method() = 0;
protected:
    uint64_t iface_data;
};

// Polymorphic with inheritance
class PolyDerived : public Base1 {
public:
    virtual ~PolyDerived() = default;
    virtual void tick() = 0;
private:
    float velocity;
};

//=============================================================================
// 6. Bit-fields
//=============================================================================

// Single byte bit-field
struct BitField1 {
    uint8_t a : 1;
    uint8_t b : 2;
    uint8_t c : 3;
    uint8_t d : 2;
};
static_assert(sizeof(BitField1) == 1);

// Multi-byte bit-field
struct BitField2 {
    uint32_t x : 10;
    uint32_t y : 10;
    uint32_t z : 12;
};
static_assert(sizeof(BitField2) == 4);

// Mixed fields and bit-fields
struct MixedBF {
    uint32_t normal;
    uint8_t bf_a : 3;
    uint8_t bf_b : 5;
    char name[4];
};

// Bit-field crossing storage unit (compiler dependent)
struct CrossBF {
    uint16_t a : 4;
    uint16_t b : 8;
    uint16_t c : 4;
};
static_assert(sizeof(CrossBF) == 2);

//=============================================================================
// 7. Enum and Union
//=============================================================================

// C-style enum (underlying type is implementation-defined, usually int or unsigned int)
enum CEnum { CE_A, CE_B, CE_C };
// Note: underlying type may be int or unsigned int depending on compiler
static_assert(sizeof(CEnum) == 4 && alignof(CEnum) == 4);

// Scoped enum with explicit underlying type
enum class ScopedU8 : uint8_t { A, B };
static_assert(get_layout_signature<ScopedU8>() == "enum[s:1,a:1]<u8[s:1,a:1]>");

enum class ScopedI32 : int32_t { X = -1, Y = 0, Z = 1 };
static_assert(get_layout_signature<ScopedI32>() == "enum[s:4,a:4]<i32[s:4,a:4]>");

enum class ScopedU64 : uint64_t { Big = 0xFFFFFFFFFFFFFFFF };
static_assert(get_layout_signature<ScopedU64>() == "enum[s:8,a:8]<u64[s:8,a:8]>");

// Union
union TestUnion {
    int32_t i;
    float f;
    char bytes[4];
};
static_assert(get_layout_signature<TestUnion>() == "union[s:4,a:4]");

union BigUnion {
    double d;
    uint64_t u;
    char buf[16];
};
static_assert(get_layout_signature<BigUnion>() == "union[s:16,a:8]");

//=============================================================================
// 8. Special Layout
//=============================================================================

// alignas
struct alignas(16) Aligned16 {
    int32_t x;
    int32_t y;
};
static_assert(alignof(Aligned16) == 16);
static_assert(sizeof(Aligned16) == 16);

struct alignas(32) Aligned32 {
    double val;
};
static_assert(alignof(Aligned32) == 32);

// Empty base optimization
struct EmptyBase {};
struct EBODerived : EmptyBase {
    int32_t value;
};
static_assert(sizeof(EBODerived) == 4); // EBO applied

// Multiple empty bases
struct Empty1 {};
struct Empty2 {};
struct MultiEmpty : Empty1, Empty2 {
    int32_t data;
};
static_assert(sizeof(MultiEmpty) == 4);

//=============================================================================
// 9. Member Pointers
//=============================================================================

struct TestClass {
    int32_t member;
    void method() {}
    virtual void vmethod() {}
};

// Data member pointer
using DataMemberPtr = int32_t TestClass::*;
static_assert(sizeof(DataMemberPtr) == 8);

// Function member pointer (may be larger for virtual)
using FuncMemberPtr = void (TestClass::*)();

//=============================================================================
// 10. CV Qualifiers and Templates
//=============================================================================

// const/volatile should be stripped
static_assert(get_layout_signature<const int32_t>() == "i32[s:4,a:4]");
static_assert(get_layout_signature<volatile int32_t>() == "i32[s:4,a:4]");
static_assert(get_layout_signature<const volatile int32_t>() == "i32[s:4,a:4]");
static_assert(get_layout_signature<const int32_t*>() == "ptr[s:8,a:8]");

// Template instantiation
template<typename T>
struct Container {
    T value;
    uint32_t size;
};

using IntContainer = Container<int32_t>;
using DoubleContainer = Container<double>;

static_assert(sizeof(IntContainer) == 8);
static_assert(sizeof(DoubleContainer) == 16);

// Verify template instances have correct signatures
static_assert(get_layout_signature<IntContainer>() == 
    "struct[s:8,a:4]{@0[value]:i32[s:4,a:4],@4[size]:u32[s:4,a:4]}");

//=============================================================================
// 11. Struct with pointer/reference members
//=============================================================================

struct WithPointers {
    int32_t* ptr;
    const char* str;
    void* data;
};
static_assert(get_layout_signature<WithPointers>() ==
    "struct[s:24,a:8]{@0[ptr]:ptr[s:8,a:8],@8[str]:ptr[s:8,a:8],@16[data]:ptr[s:8,a:8]}");

//=============================================================================
// 12. Struct containing arrays
//=============================================================================

struct WithArrays {
    int32_t values[4];
    char name[16];
};
static_assert(get_layout_signature<WithArrays>() ==
    "struct[s:32,a:4]{@0[values]:array[s:16,a:4]<i32[s:4,a:4],4>,@16[name]:bytes[s:16,a:1]}");

//=============================================================================
// 13. wchar_t (platform-dependent size)
//=============================================================================

// wchar_t size: 2 bytes on Windows, 4 bytes on Linux
static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4);

//=============================================================================
// 14. long double (platform-dependent)
//=============================================================================

// long double: 8/12/16 bytes depending on platform
static_assert(sizeof(long double) >= 8);

//=============================================================================
// 15. std::byte (C++17)
//=============================================================================

static_assert(get_layout_signature<std::byte>() == "byte[s:1,a:1]");

// std::byte array
static_assert(get_layout_signature<std::byte[8]>() == "array[s:8,a:1]<byte[s:1,a:1],8>");

// Struct with std::byte
struct WithByte {
    std::byte b;
    int32_t val;
};
static_assert(sizeof(WithByte) == 8);

//=============================================================================
// 16. Function Pointers
//=============================================================================

// Simple function pointer
using VoidFn = void(*)();
static_assert(get_layout_signature<VoidFn>() == "fnptr[s:8,a:8]");

// Function pointer with args and return
using IntFn = int(*)(int, int);
static_assert(get_layout_signature<IntFn>() == "fnptr[s:8,a:8]");

// Noexcept function pointer
using NoexceptFn = void(*)() noexcept;
static_assert(get_layout_signature<NoexceptFn>() == "fnptr[s:8,a:8]");

// Variadic function pointer
using VarFn = int(*)(const char*, ...);
static_assert(get_layout_signature<VarFn>() == "fnptr[s:8,a:8]");

// Complex function pointer
using ComplexFn = double(*)(int, float, const char*);
static_assert(get_layout_signature<ComplexFn>() == "fnptr[s:8,a:8]");

// Struct with function pointer
struct WithFnPtr {
    void (*callback)(int);
    void* user_data;
};
static_assert(get_layout_signature<WithFnPtr>() ==
    "struct[s:16,a:8]{@0[callback]:fnptr[s:8,a:8],@8[user_data]:ptr[s:8,a:8]}");

//=============================================================================
// Cross-type compatibility checks
//=============================================================================

struct TypeA { int32_t x; int32_t y; };
struct TypeB { int32_t x; int32_t y; };  // Same field names = same signature
static_assert(signatures_match<TypeA, TypeB>()); // Identical layout and names

struct TypeC { int32_t a; int32_t b; };  // Different field names
static_assert(!signatures_match<TypeA, TypeC>()); // Field names differ

struct TypeD { int32_t x; int64_t y; };
static_assert(!signatures_match<TypeA, TypeD>()); // Different layout

//=============================================================================
// 17. Platform-dependent Type Detection (Portability Warnings)
//=============================================================================

// Platform-dependent primitive types should be detected
// NOTE: wchar_t and long double are ALWAYS platform-dependent
static_assert(is_platform_dependent_v<wchar_t> == true);
static_assert(is_platform_dependent_v<long double> == true);

// Windows: long is 4 bytes, different from int64_t
#if defined(_WIN32) || defined(_WIN64)
static_assert(is_platform_dependent_v<long> == true);
static_assert(is_platform_dependent_v<unsigned long> == true);
static_assert(is_platform_dependent_v<const long> == true);
static_assert(is_platform_dependent_v<long[4]> == true);
#else
// On Linux, long = int64_t, so long is NOT flagged as platform-dependent
static_assert(is_platform_dependent_v<long> == false);
static_assert(is_platform_dependent_v<unsigned long> == false);
#endif

// CV-qualified variants for wchar_t and long double
static_assert(is_platform_dependent_v<volatile wchar_t> == true);
static_assert(is_platform_dependent_v<const volatile long double> == true);

// Arrays of platform-dependent types
static_assert(is_platform_dependent_v<wchar_t[16]> == true);
static_assert(is_platform_dependent_v<long double[4]> == true);

// Portable types should NOT be detected as platform-dependent
static_assert(is_platform_dependent_v<int> == false);
static_assert(is_platform_dependent_v<int32_t> == false);
static_assert(is_platform_dependent_v<int64_t> == false);
static_assert(is_platform_dependent_v<double> == false);
static_assert(is_platform_dependent_v<char> == false);
static_assert(is_platform_dependent_v<char16_t> == false);
static_assert(is_platform_dependent_v<char32_t> == false);

// Fixed-width integer type detection
static_assert(is_fixed_width_integer_v<int8_t> == true);
static_assert(is_fixed_width_integer_v<uint8_t> == true);
static_assert(is_fixed_width_integer_v<int16_t> == true);
static_assert(is_fixed_width_integer_v<uint16_t> == true);
static_assert(is_fixed_width_integer_v<int32_t> == true);
static_assert(is_fixed_width_integer_v<uint32_t> == true);
static_assert(is_fixed_width_integer_v<int64_t> == true);
static_assert(is_fixed_width_integer_v<uint64_t> == true);
// NOTE: On Linux, int = int32_t (same type via typedef), so int IS a fixed-width integer
// This is expected behavior - we detect actual types, not names
static_assert(is_fixed_width_integer_v<float> == false);

//=============================================================================
// 18. Struct Portability Checking
//=============================================================================

// Portable struct - uses only fixed-width types
struct PortableStruct {
    int32_t x;
    int64_t y;
    double z;
    char name[16];
};
static_assert(is_portable<PortableStruct>() == true);
TYPELAYOUT_ASSERT_PORTABLE(PortableStruct);  // Should compile successfully

// Non-portable struct - contains wchar_t (always platform-dependent)
struct NonPortableWithWchar {
    int32_t id;
    wchar_t name[16];  // Platform-dependent: 2 bytes on Windows, 4 bytes on Linux
};
static_assert(is_portable<NonPortableWithWchar>() == false);

// Non-portable struct - contains long double (always platform-dependent)
struct NonPortableWithLongDouble {
    double normal;
    long double extended;  // Platform-dependent: 8/12/16 bytes
};
static_assert(is_portable<NonPortableWithLongDouble>() == false);

// On Windows, long is platform-dependent
#if defined(_WIN32) || defined(_WIN64)
struct NonPortableWithLong {
    int32_t x;
    long y;  // Platform-dependent on Windows: 4 bytes (vs 8 bytes on Linux)
};
static_assert(is_portable<NonPortableWithLong>() == false);
#endif

// Empty struct is portable
static_assert(is_portable<EmptyStruct>() == true);

// Nested struct portability
struct NestedPortable {
    PortableStruct inner;
    int32_t extra;
};
static_assert(is_portable<NestedPortable>() == true);

// Nested non-portable struct
struct NestedNonPortable {
    NonPortableWithWchar inner;
    int32_t extra;
};
static_assert(is_portable<NestedNonPortable>() == false);

// Primitives are portable (except platform-dependent ones)
static_assert(is_portable<int32_t>() == true);
static_assert(is_portable<double>() == true);
static_assert(is_portable<wchar_t>() == false);
static_assert(is_portable<long double>() == false);

//=============================================================================
// 19. Inheritance and Portability
//=============================================================================

// Base class with platform-dependent type
struct NonPortableBase {
    wchar_t name[8];  // Platform-dependent
};

// Derived class should inherit non-portability from base
struct DerivedFromNonPortable : NonPortableBase {
    int32_t id;  // This field is portable, but base is not
};
static_assert(is_portable<DerivedFromNonPortable>() == false);

// Portable base class
struct PortableBase {
    int32_t value;
};

// Derived from portable base should be portable
struct DerivedFromPortable : PortableBase {
    int64_t extra;
};
static_assert(is_portable<DerivedFromPortable>() == true);
TYPELAYOUT_ASSERT_PORTABLE(DerivedFromPortable);

// Multiple inheritance - one non-portable base
struct MultiBaseNonPortable : PortableBase, NonPortableBase {
    double data;
};
static_assert(is_portable<MultiBaseNonPortable>() == false);

// Deep inheritance chain
struct Level1 { int32_t a; };
struct Level2 : Level1 { int64_t b; };
struct Level3 : Level2 { double c; };
static_assert(is_portable<Level3>() == true);

// Deep inheritance with non-portable at root
struct BadRoot { wchar_t w; };
struct Level2Bad : BadRoot { int32_t x; };
struct Level3Bad : Level2Bad { double y; };
static_assert(is_portable<Level3Bad>() == false);

//=============================================================================
// 20. Union Portability Tests
//=============================================================================

// Union with all portable members
union PortableUnion {
    int32_t i;
    float f;
    char c[8];
};
static_assert(is_portable<PortableUnion>() == true);
TYPELAYOUT_ASSERT_PORTABLE(PortableUnion);

// Union with a non-portable member (wchar_t)
union NonPortableUnion1 {
    int32_t i;
    wchar_t w;  // Platform-dependent (2 bytes on Windows, 4 bytes on Linux)
};
static_assert(is_portable<NonPortableUnion1>() == false);

// Union containing non-portable struct
struct StructWithWchar {
    wchar_t name[16];
};

union NonPortableUnion2 {
    int32_t id;
    StructWithWchar s;  // Contains wchar_t
};
static_assert(is_portable<NonPortableUnion2>() == false);

// Empty union (edge case)
union EmptyUnion {};
static_assert(is_portable<EmptyUnion>() == true);

// Union with portable nested union
union InnerPortableUnion {
    int32_t a;
    double b;
};

union OuterPortableUnion {
    InnerPortableUnion inner;
    int64_t value;
};
static_assert(is_portable<OuterPortableUnion>() == true);

// Union with non-portable nested union
union InnerNonPortableUnion {
    wchar_t w;
    int32_t i;
};

union OuterNonPortableUnion {
    InnerNonPortableUnion inner;  // Contains wchar_t
    int64_t value;
};
static_assert(is_portable<OuterNonPortableUnion>() == false);

//=============================================================================
// 21. Concept Tests
//=============================================================================

// Portable concept
static_assert(Portable<int32_t>);
static_assert(Portable<PortableStruct>);
static_assert(Portable<PortableUnion>);
static_assert(!Portable<wchar_t>);
static_assert(!Portable<NonPortableWithWchar>);

// PlatformIndependent concept
static_assert(PlatformIndependent<int32_t>);
static_assert(PlatformIndependent<double>);
static_assert(!PlatformIndependent<wchar_t>);
static_assert(!PlatformIndependent<long double>);

// LayoutCompatible concept
static_assert(LayoutCompatible<TypeA, TypeB>);
static_assert(!LayoutCompatible<TypeA, TypeC>);
static_assert(!LayoutCompatible<TypeA, TypeD>);

// Template constraint usage example
template<Portable T>
constexpr bool accepts_portable() { return true; }

static_assert(accepts_portable<PortableStruct>());
static_assert(accepts_portable<int32_t>());

//=============================================================================
// Main - if this compiles, all static_assert tests pass
//=============================================================================

int main() {
    // All tests are compile-time static_assert
    // If this file compiles successfully, all tests pass
    return 0;
}