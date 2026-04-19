// ============================================================================
// Test: Type Safety & is_byte_copy_safe_v
// Validates compile-time admission and runtime safety classification.
// ============================================================================

#include <iostream>
#include <cstdint>
#include <cstddef>
#include <cassert>

#include "../xoffsetdatastructure.hpp"
#include <boost/typelayout/tools/safety_level.hpp>

using namespace XOffsetDatastructure;
using namespace boost::typelayout;

// ============================================================================
// Test types — safe
// ============================================================================

struct SafeRecord { int32_t x; float y; double z; uint16_t w; };
struct NestedSafe { SafeRecord inner; int64_t tag; };
struct HasEnum { enum FixedEnum : uint32_t { A, B }; FixedEnum e; int32_t x; };
struct HasArray { int32_t arr[4]; double val; };

struct ContainerTypes {
    XString name; XVector<int32_t> numbers;
    XSet<int32_t> ids; XMap<int32_t, XString> id_to_name;
    template<typename A> ContainerTypes(A a) : name(a), numbers(a), ids(a), id_to_name(a) {}
};

struct NestedContainers {
    XVector<XVector<int32_t>> matrix; XVector<XString> strings;
    template<typename A> NestedContainers(A a) : matrix(a), strings(a) {}
};

struct Point { float x, y, z; };

struct Player {
    XString name; int32_t level; Point position; XVector<int32_t> inventory;
    template<typename A> Player(A a) : name(a), level(0), position{}, inventory(a) {}
};

// ============================================================================
// Test types — unsafe
// ============================================================================

struct HasPointer    { int32_t* ptr; int32_t val; };
struct Polymorphic   { virtual ~Polymorphic() = default; int32_t x; };
struct WithStdString { std::string name; int32_t id; };
struct WithStdVector { std::vector<int32_t> data; };
struct UnsafeNested  { Point position; Polymorphic bad; };

// ============================================================================
// Test types — platform variant (locally safe)
// ============================================================================

struct HasLong        { long val; int32_t x; };
struct HasUnsignedLong{ unsigned long val; int32_t x; };
struct HasWchar       { wchar_t ch; int32_t x; };
struct HasLongDouble  { long double ld; int32_t x; };

// ============================================================================
// Test types — boundary (Audit P5)
// ============================================================================

struct HasNullptr  { int32_t x; std::nullptr_t n; };
struct HasByte     { std::byte b1; std::byte b2; int32_t x; };

struct Foo { int x; double y; };
struct HasMemberPointer     { int Foo::* mp; int32_t data; };
struct HasMemberFuncPointer { void (Foo::* mfp)(); int32_t data; };

// ============================================================================
// Compile-time static_asserts
// ============================================================================

// Safe
static_assert( is_byte_copy_safe_v<int32_t>);
static_assert( is_byte_copy_safe_v<double>);
static_assert( is_byte_copy_safe_v<SafeRecord>);
static_assert( is_byte_copy_safe_v<NestedSafe>);
static_assert( is_byte_copy_safe_v<HasEnum>);
static_assert( is_byte_copy_safe_v<HasArray>);
static_assert( is_byte_copy_safe_v<ContainerTypes>);
static_assert( is_byte_copy_safe_v<NestedContainers>);
static_assert( is_byte_copy_safe_v<Point>);
static_assert( is_byte_copy_safe_v<Player>);

// Unsafe
static_assert(!is_byte_copy_safe_v<HasPointer>);
static_assert(!is_byte_copy_safe_v<Polymorphic>);
static_assert(!is_byte_copy_safe_v<WithStdString>);
static_assert(!is_byte_copy_safe_v<WithStdVector>);
static_assert(!is_byte_copy_safe_v<UnsafeNested>);
static_assert(!is_byte_copy_safe_v<HasMemberPointer>);
static_assert(!is_byte_copy_safe_v<HasMemberFuncPointer>);

// Platform variant — locally safe
static_assert( is_byte_copy_safe_v<HasLong>);
static_assert( is_byte_copy_safe_v<HasUnsignedLong>);
static_assert( is_byte_copy_safe_v<HasWchar>);
static_assert( is_byte_copy_safe_v<HasLongDouble>);

// Boundary
static_assert( is_byte_copy_safe_v<HasNullptr>);
static_assert( is_byte_copy_safe_v<HasByte>);

// ============================================================================
// Test 1: Signature comparison (strict check)
// ============================================================================
bool test_signature_comparison() {
    std::cout << "\n[TEST] Signature comparison\n";

    constexpr auto gold = get_layout_signature<SafeRecord>();
    static_assert(std::string_view(get_layout_signature<SafeRecord>()) == std::string_view(gold));
    static_assert(!(std::string_view(get_layout_signature<NestedSafe>()) == std::string_view(gold)));
    static_assert(!is_byte_copy_safe_v<HasPointer>);

    std::cout << "  [OK]\n";
    return true;
}

// ============================================================================
// Test 2: classify_signature (runtime safety levels)
// ============================================================================
bool test_classify_levels() {
    using boost::typelayout::compat::detail::SafetyLevel;
    using boost::typelayout::compat::detail::classify_signature;

    std::cout << "\n[TEST] classify_signature\n";

    {
        constexpr auto sig = get_layout_signature<int32_t>();
        assert(classify_signature(std::string_view(sig.value, sig.size)) == SafetyLevel::TrivialSafe);
    }
    {
        constexpr auto sig = get_layout_signature<SafeRecord>();
        auto lvl = classify_signature(std::string_view(sig.value, sig.size));
        assert(lvl == SafetyLevel::TrivialSafe || lvl == SafetyLevel::PaddingRisk);
    }
    {
        constexpr auto sig = get_layout_signature<HasPointer>();
        assert(classify_signature(std::string_view(sig.value, sig.size)) == SafetyLevel::PointerRisk);
    }
    {
        constexpr auto sig = get_layout_signature<HasWchar>();
        assert(classify_signature(std::string_view(sig.value, sig.size)) == SafetyLevel::PlatformVariant);
    }
    {
        constexpr auto sig = get_layout_signature<HasLongDouble>();
        assert(classify_signature(std::string_view(sig.value, sig.size)) == SafetyLevel::PlatformVariant);
    }

    std::cout << "  [OK]\n";
    return true;
}

// ============================================================================
// Test 3: Nested container recursive safety (C2)
// ============================================================================
struct SafeFlat { int32_t x; float y; double z; };

bool test_nested_container_recursion() {
    std::cout << "\n[TEST] Nested container recursive safety\n";

    // Single-level
    static_assert( is_byte_copy_safe_v<XVector<int32_t>>);
    static_assert( is_byte_copy_safe_v<XVector<SafeFlat>>);
    static_assert(!is_byte_copy_safe_v<XVector<HasPointer>>);

    // Double-nested
    static_assert( is_byte_copy_safe_v<XVector<XVector<int32_t>>>);
    static_assert(!is_byte_copy_safe_v<XVector<XVector<HasPointer>>>);

    // Triple-nested
    static_assert( is_byte_copy_safe_v<XVector<XVector<XVector<int32_t>>>>);
    static_assert(!is_byte_copy_safe_v<XVector<XVector<XVector<HasPointer>>>>);

    // Map containers
    static_assert( is_byte_copy_safe_v<XMap<int32_t, SafeFlat>>);
    static_assert(!is_byte_copy_safe_v<XMap<int32_t, HasPointer>>);
    static_assert(!is_byte_copy_safe_v<XMap<HasPointer, int32_t>>);

    // Polymorphic in container
    static_assert(!is_byte_copy_safe_v<XVector<Polymorphic>>);

    std::cout << "  [OK]\n";
    return true;
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "\n=== Type Safety Tests ===\n";

    bool all = true;
    all &= test_signature_comparison();
    all &= test_classify_levels();
    all &= test_nested_container_recursion();

    std::cout << "\n" << (all ? "[PASS] All type safety tests passed" : "[FAIL] Some tests failed") << "\n";
    return all ? 0 : 1;
}
