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
// Main - if this compiles, all static_assert tests pass
//=============================================================================

int main() {
    // All tests are compile-time static_assert
    // If this file compiles successfully, all tests pass
    return 0;
}
