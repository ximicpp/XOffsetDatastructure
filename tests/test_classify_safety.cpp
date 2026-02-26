// Test for classify_safety<T> from TypeLayout
//
// Validates the compile-time safety classifier against known types.

#include <iostream>
#include <cstdint>
#include <cassert>

#include <boost/typelayout.hpp>
#include <boost/typelayout/tools/classify_safety.hpp>

using namespace boost::typelayout::compat;

// ============================================================================
// Test types
// ============================================================================

struct SafeStruct {
    int32_t a;
    float   b;
    double  c;
    char    d;
};

struct NestedSafe {
    SafeStruct inner;
    uint64_t   x;
};

struct HasLong {
    int32_t ok;
    long    bad;  // platform-dependent
};

struct HasPointer {
    int32_t* ptr;
};

struct Polymorphic {
    virtual ~Polymorphic() = default;
    int32_t x;
};

union SimpleUnion {
    int32_t i;
    float   f;
};

struct HasUnion {
    SimpleUnion u;
    int32_t     x;
};

enum FixedEnum : uint32_t { A = 0, B = 1 };
enum class ScopedEnum : int16_t { X = 0, Y = 1 };

struct HasArray {
    int32_t arr[10];
    double  vals[3];
};

struct HasBitField {
    int32_t x : 3;
    int32_t y : 5;
};

struct DerivedSafe : SafeStruct {
    uint16_t extra;
};

struct VirtualBase : virtual SafeStruct {
    int32_t z;
};

// ============================================================================
// Static assertions — these validate at compile time
// ============================================================================

// Safe types
static_assert(classify_safety<int32_t>()    == SafetyLevel::Safe,   "int32_t should be Safe");
static_assert(classify_safety<uint64_t>()   == SafetyLevel::Safe,   "uint64_t should be Safe");
static_assert(classify_safety<float>()      == SafetyLevel::Safe,   "float should be Safe");
static_assert(classify_safety<double>()     == SafetyLevel::Safe,   "double should be Safe");
static_assert(classify_safety<bool>()       == SafetyLevel::Safe,   "bool should be Safe");
static_assert(classify_safety<char>()       == SafetyLevel::Safe,   "char should be Safe");
static_assert(classify_safety<SafeStruct>() == SafetyLevel::Safe,   "SafeStruct should be Safe");
static_assert(classify_safety<NestedSafe>() == SafetyLevel::Safe,   "NestedSafe should be Safe");
static_assert(classify_safety<HasArray>()   == SafetyLevel::Safe,   "HasArray should be Safe");
static_assert(classify_safety<DerivedSafe>()== SafetyLevel::Safe,   "DerivedSafe should be Safe");
static_assert(classify_safety<FixedEnum>()  == SafetyLevel::Safe,   "FixedEnum should be Safe");
static_assert(classify_safety<ScopedEnum>() == SafetyLevel::Safe,   "ScopedEnum should be Safe");

// Platform-dependent types: long / unsigned long
// On LP64 (Linux), int64_t == long, so `long` passes the fixed-width whitelist → Safe.
// On macOS (int64_t == long long) and LLP64, `long` is NOT a fixed-width alias → Risk.
static_assert(classify_safety<long>() ==
    (std::is_same_v<long, int64_t> ? SafetyLevel::Safe : SafetyLevel::Risk),
    "long: Safe if it IS int64_t, Risk otherwise");
static_assert(classify_safety<unsigned long>() ==
    (std::is_same_v<unsigned long, uint64_t> ? SafetyLevel::Safe : SafetyLevel::Risk),
    "unsigned long: Safe if it IS uint64_t, Risk otherwise");
static_assert(classify_safety<long long>()  == SafetyLevel::Safe,   "long long should be Safe (always 8 bytes)");
static_assert(classify_safety<wchar_t>()    == SafetyLevel::Risk,   "wchar_t should be Risk");
static_assert(classify_safety<long double>()== SafetyLevel::Risk,   "long double should be Risk");
static_assert(classify_safety<HasLong>() ==
    (std::is_same_v<long, int64_t> ? SafetyLevel::Safe : SafetyLevel::Risk),
    "HasLong: Safe if long IS int64_t, Risk otherwise");
static_assert(classify_safety<HasPointer>() == SafetyLevel::Risk,   "HasPointer should be Risk");
static_assert(classify_safety<int32_t*>()   == SafetyLevel::Risk,   "pointer should be Risk");
static_assert(classify_safety<HasBitField>()== SafetyLevel::Risk,   "HasBitField should be Risk");

// Warning types
static_assert(classify_safety<Polymorphic>()   == SafetyLevel::Warning, "Polymorphic should be Warning");
static_assert(classify_safety<SimpleUnion>()   == SafetyLevel::Warning, "SimpleUnion should be Warning");
static_assert(classify_safety<HasUnion>()      == SafetyLevel::Warning, "HasUnion should be Warning");
static_assert(classify_safety<VirtualBase>()   == SafetyLevel::Warning, "VirtualBase should be Warning");

// Convenience API
static_assert( is_layout_safe<int32_t>(),    "int32_t should be safe");
static_assert( is_layout_safe<SafeStruct>(), "SafeStruct should be safe");
// On LP64 Linux, long == int64_t → safe; on macOS/LLP64, long ≠ int64_t → not safe.
static_assert( is_layout_safe<long>() == std::is_same_v<long, int64_t>,
    "long: safe iff it IS int64_t");
static_assert(!is_layout_safe<HasPointer>(),  "HasPointer should NOT be safe");

// Bounded array of safe type
static_assert(classify_safety<int32_t[5]>() == SafetyLevel::Safe,  "int32_t[5] should be Safe");
static_assert(classify_safety<long[3]>() ==
    (std::is_same_v<long, int64_t> ? SafetyLevel::Safe : SafetyLevel::Risk),
    "long[3]: Safe if long IS int64_t, Risk otherwise");

// CV-qualified types
static_assert(classify_safety<const int32_t>()    == SafetyLevel::Safe, "const int32_t should be Safe");
static_assert(classify_safety<volatile double>()  == SafetyLevel::Safe, "volatile double should be Safe");
static_assert(classify_safety<const long>() ==
    (std::is_same_v<long, int64_t> ? SafetyLevel::Safe : SafetyLevel::Risk),
    "const long: Safe if long IS int64_t, Risk otherwise");

// ============================================================================
// Runtime report (just to confirm compilation + print summary)
// ============================================================================

int main() {
    std::cout << "\n[TEST] classify_safety\n";
    std::cout << std::string(50, '-') << "\n";

    std::cout << "  int32_t     : " << safety_label(classify_safety<int32_t>()) << "\n";
    std::cout << "  long        : " << safety_label(classify_safety<long>()) << "\n";
    std::cout << "  SafeStruct  : " << safety_label(classify_safety<SafeStruct>()) << "\n";
    std::cout << "  NestedSafe  : " << safety_label(classify_safety<NestedSafe>()) << "\n";
    std::cout << "  HasLong     : " << safety_label(classify_safety<HasLong>()) << "\n";
    std::cout << "  HasPointer  : " << safety_label(classify_safety<HasPointer>()) << "\n";
    std::cout << "  Polymorphic : " << safety_label(classify_safety<Polymorphic>()) << "\n";
    std::cout << "  SimpleUnion : " << safety_label(classify_safety<SimpleUnion>()) << "\n";
    std::cout << "  HasUnion    : " << safety_label(classify_safety<HasUnion>()) << "\n";
    std::cout << "  HasBitField : " << safety_label(classify_safety<HasBitField>()) << "\n";
    std::cout << "  VirtualBase : " << safety_label(classify_safety<VirtualBase>()) << "\n";
    std::cout << "  FixedEnum   : " << safety_label(classify_safety<FixedEnum>()) << "\n";
    std::cout << "  int32_t[5]  : " << safety_label(classify_safety<int32_t[5]>()) << "\n";
    std::cout << "  long[3]     : " << safety_label(classify_safety<long[3]>()) << "\n";
    std::cout << "  DerivedSafe : " << safety_label(classify_safety<DerivedSafe>()) << "\n";

    std::cout << std::string(50, '-') << "\n";
    std::cout << "  All static_assert checks passed at compile time!\n";
    std::cout << "[PASS] classify_safety tests passed!\n\n";

    return 0;
}
