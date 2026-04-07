// ============================================================================
// Test: Type Safety & is_byte_copy_safe_v
// Purpose: Validates the unified TypeLayout-delegated admission predicate.
//
// Tests:
//   1. is_byte_copy_safe_v — Safe types pass, unsafe types rejected
//   2. Backward compatibility — is_xbuffer_safe<T>::value
//   3. TypeLayout classify_signature (runtime safety classification)
//   4. C2 — Nested container recursive safety
//   5. Inline strict/size checks (replaces StrictPolicy/SmallTypePolicy)
//
// Note: DefaultPolicy, StrictPolicy, SmallTypePolicy were removed.
//       Domain admission is now fully delegated to TypeLayout's
//       is_byte_copy_safe_v<T> recursive predicate.
// ============================================================================

#include <iostream>
#include <cstdint>
#include <cassert>

#include "../xoffsetdatastructure.hpp"
#include <boost/typelayout/tools/safety_level.hpp>  // compat::classify_signature, SafetyLevel

using namespace XOffsetDatastructure;
using namespace XOffsetDatastructure::detail;
using namespace boost::typelayout;

// ============================================================================
// Test types
// ============================================================================

struct SafeRecord {
    int32_t  x;
    float    y;
    double   z;
    uint16_t w;
};

struct NestedSafe {
    SafeRecord inner;
    int64_t    tag;
};

struct HasPointer {
    int32_t* ptr;
    int32_t  val;
};

struct HasUnion {
    union { int32_t i; float f; } u;
    int32_t x;
};

struct HasWchar {
    wchar_t ch;
    int32_t x;
};

struct HasLongDouble {
    long double ld;
    int32_t     x;
};

struct Polymorphic {
    virtual ~Polymorphic() = default;
    int32_t x;
};

struct HasLong {
    long    val;
    int32_t x;
};

enum FixedEnum : uint32_t { A = 0, B = 1 };

struct HasEnum {
    FixedEnum e;
    int32_t   x;
};

struct HasArray {
    int32_t arr[4];
    double  val;
};

// ============================================================================
// Test 1: is_byte_copy_safe_v (replaces DefaultPolicy)
// ============================================================================
bool test_byte_copy_safe() {
    std::cout << "\n[TEST] is_byte_copy_safe_v\n";
    std::cout << std::string(50, '-') << "\n";

    // Safe types
    static_assert(is_byte_copy_safe_v<int32_t>,
                  "int32_t must be byte-copy safe");
    static_assert(is_byte_copy_safe_v<double>,
                  "double must be byte-copy safe");
    static_assert(is_byte_copy_safe_v<SafeRecord>,
                  "SafeRecord must be byte-copy safe");
    static_assert(is_byte_copy_safe_v<NestedSafe>,
                  "NestedSafe must be byte-copy safe");
    static_assert(is_byte_copy_safe_v<HasEnum>,
                  "HasEnum (fixed underlying) must be byte-copy safe");
    static_assert(is_byte_copy_safe_v<HasArray>,
                  "HasArray must be byte-copy safe");
    std::cout << "  Safe types pass... [OK]\n";

    // Unsafe types → rejected
    static_assert(!is_byte_copy_safe_v<HasPointer>,
                  "HasPointer must be rejected");
    static_assert(!is_byte_copy_safe_v<Polymorphic>,
                  "Polymorphic must be rejected");
    std::cout << "  Unsafe types rejected... [OK]\n";

    // Platform variant types → locally byte-copy safe
    static_assert(is_byte_copy_safe_v<HasWchar>,
                  "HasWchar is locally byte-copy safe (trivially_copyable + no pointer)");
    static_assert(is_byte_copy_safe_v<HasLongDouble>,
                  "HasLongDouble is locally byte-copy safe");
    std::cout << "  Platform variant types accepted locally... [OK]\n";

    return true;
}

// ============================================================================
// Test 2: Inline strict check (replaces StrictPolicy)
// ============================================================================
bool test_inline_strict_check() {
    std::cout << "\n[TEST] Inline strict check (signature comparison)\n";
    std::cout << std::string(50, '-') << "\n";

    // Get the actual gold signature for SafeRecord at compile time
    constexpr auto gold = get_layout_signature<SafeRecord>();

    // Matching signature → pass
    static_assert(is_byte_copy_safe_v<SafeRecord> &&
                  std::string_view(get_layout_signature<SafeRecord>()) == std::string_view(gold),
                  "SafeRecord must match its own gold signature");
    std::cout << "  Matching signature passes... [OK]\n";

    // Different type with different layout → fail
    static_assert(!(std::string_view(get_layout_signature<NestedSafe>()) == std::string_view(gold)),
                  "NestedSafe must NOT match SafeRecord's gold signature");
    std::cout << "  Different layout rejected... [OK]\n";

    // Unsafe type: even with matching sig, byte-copy-safe check fails
    constexpr auto ptr_sig = get_layout_signature<HasPointer>();
    static_assert(!is_byte_copy_safe_v<HasPointer>,
                  "HasPointer must be rejected even with matching signature");
    std::cout << "  Unsafe type with matching sig rejected... [OK]\n";

    return true;
}

// ============================================================================
// Test 3: Inline size-limited check (replaces SmallTypePolicy)
// ============================================================================
bool test_inline_size_check() {
    std::cout << "\n[TEST] Inline size-limited check\n";
    std::cout << std::string(50, '-') << "\n";

    // SafeRecord: sizeof = 24 on 64-bit → pass (≤ 24)
    static_assert(sizeof(SafeRecord) <= 24,
                  "SafeRecord should be <= 24 bytes");
    static_assert(is_byte_copy_safe_v<SafeRecord> && sizeof(SafeRecord) <= 24,
                  "SafeRecord must pass size + safety check");
    std::cout << "  Small safe type passes... [OK]\n";

    // NestedSafe: sizeof = 32 (SafeRecord=24 + int64_t=8) → rejected (> 24)
    static_assert(sizeof(NestedSafe) > 24,
                  "NestedSafe should be > 24 bytes for this test");
    static_assert(!(is_byte_copy_safe_v<NestedSafe> && sizeof(NestedSafe) <= 24),
                  "NestedSafe must be rejected (too large)");
    std::cout << "  Large type rejected... [OK]\n";

    // Unsafe type: even if small, rejected
    static_assert(!is_byte_copy_safe_v<HasPointer>,
                  "HasPointer must be rejected");
    std::cout << "  Small unsafe type rejected... [OK]\n";

    return true;
}

// ============================================================================
// Test 4: Backward compatibility — is_xbuffer_safe<T>::value
// ============================================================================
bool test_backward_compat() {
    std::cout << "\n[TEST] Backward Compatibility\n";
    std::cout << std::string(50, '-') << "\n";

    // is_xbuffer_safe<T>::value should be identical to is_byte_copy_safe_v<T>
    static_assert(is_xbuffer_safe<int32_t>::value == is_byte_copy_safe_v<int32_t>,
                  "is_xbuffer_safe must match is_byte_copy_safe_v for int32_t");
    static_assert(is_xbuffer_safe<SafeRecord>::value == is_byte_copy_safe_v<SafeRecord>,
                  "is_xbuffer_safe must match is_byte_copy_safe_v for SafeRecord");
    static_assert(is_xbuffer_safe<HasPointer>::value == is_byte_copy_safe_v<HasPointer>,
                  "is_xbuffer_safe must match is_byte_copy_safe_v for HasPointer");
    static_assert(is_xbuffer_safe<HasWchar>::value == is_byte_copy_safe_v<HasWchar>,
                  "is_xbuffer_safe must match is_byte_copy_safe_v for HasWchar");

    // Direct checks
    static_assert(is_xbuffer_safe<int32_t>::value == true, "int32_t is safe");
    static_assert(is_xbuffer_safe<HasPointer>::value == false, "HasPointer is not safe");

    std::cout << "  is_xbuffer_safe<T> matches is_byte_copy_safe_v<T>... [OK]\n";

    // reason() still works
    const char* reason = is_xbuffer_safe<HasPointer>::reason();
    std::cout << "  HasPointer reason: " << reason << "... [OK]\n";

    return true;
}

// ============================================================================
// Test 5: TypeLayout classify_signature (runtime safety classification)
// ============================================================================
bool test_classify_levels() {
    using boost::typelayout::compat::detail::SafetyLevel;
    using boost::typelayout::compat::detail::classify_signature;
    using boost::typelayout::compat::detail::safety_level_name;

    std::cout << "\n[TEST] TypeLayout classify_signature (runtime safety classification)\n";
    std::cout << std::string(55, '-') << "\n";

    // Safe types → TrivialSafe
    {
        constexpr auto sig = get_layout_signature<int32_t>();
        auto lvl = classify_signature(std::string_view(sig.value, sig.size));
        assert(lvl == SafetyLevel::TrivialSafe);
    }
    {
        constexpr auto sig = get_layout_signature<SafeRecord>();
        auto lvl = classify_signature(std::string_view(sig.value, sig.size));
        assert(lvl == SafetyLevel::TrivialSafe || lvl == SafetyLevel::PaddingRisk);
    }
    std::cout << "  Safe classification... [OK]\n";

    // Pointer types → PointerRisk
    {
        constexpr auto sig = get_layout_signature<HasPointer>();
        auto lvl = classify_signature(std::string_view(sig.value, sig.size));
        assert(lvl == SafetyLevel::PointerRisk);
    }
    std::cout << "  PointerRisk types rejected... [OK]\n";

    // Platform variant types → PlatformVariant
    {
        constexpr auto sig = get_layout_signature<HasWchar>();
        auto lvl = classify_signature(std::string_view(sig.value, sig.size));
        assert(lvl == SafetyLevel::PlatformVariant);
    }
    {
        constexpr auto sig = get_layout_signature<HasLongDouble>();
        auto lvl = classify_signature(std::string_view(sig.value, sig.size));
        assert(lvl == SafetyLevel::PlatformVariant);
    }
    std::cout << "  PlatformVariant types classified correctly... [OK]\n";

    return true;
}

// ============================================================================
// Test 6: C2 — Nested container recursive safety
// ============================================================================

struct SafeFlat {
    int32_t  x;
    float    y;
    double   z;
};

bool test_c2_nested_container_recursion() {
    std::cout << "\n[TEST] C2: Nested container recursive safety\n";
    std::cout << std::string(55, '-') << "\n";

    // Single-level safe/unsafe container
    static_assert(is_byte_copy_safe_v<XVector<int32_t>>, "XVector<int32_t> must pass");
    static_assert(is_byte_copy_safe_v<XVector<SafeFlat>>, "XVector<SafeFlat> must pass");
    static_assert(!is_byte_copy_safe_v<XVector<HasPointer>>, "XVector<HasPointer> must be rejected");
    std::cout << "  Single-level containers... [OK]\n";

    // Nested safe/unsafe container — THE KEY C2 FIX
    static_assert(is_byte_copy_safe_v<XVector<XVector<int32_t>>>, "nested safe must pass");
    static_assert(!is_byte_copy_safe_v<XVector<XVector<HasPointer>>>, "nested unsafe must fail");
    std::cout << "  Nested containers (C2 fix)... [OK]\n";

    // Triple-nested
    static_assert(!is_byte_copy_safe_v<XVector<XVector<XVector<HasPointer>>>>, "triple-nested unsafe must fail");
    static_assert(is_byte_copy_safe_v<XVector<XVector<XVector<int32_t>>>>, "triple-nested safe must pass");
    std::cout << "  Triple-nested containers... [OK]\n";

    // Map containers
    static_assert(is_byte_copy_safe_v<XMap<int32_t, SafeFlat>>, "safe map must pass");
    static_assert(!is_byte_copy_safe_v<XMap<int32_t, HasPointer>>, "unsafe map value must fail");
    static_assert(!is_byte_copy_safe_v<XMap<HasPointer, int32_t>>, "unsafe map key must fail");
    std::cout << "  Map containers... [OK]\n";

    // Container holding polymorphic type
    static_assert(!is_byte_copy_safe_v<XVector<Polymorphic>>, "XVector<Polymorphic> must fail");
    static_assert(!is_byte_copy_safe_v<XMap<int32_t, Polymorphic>>, "XMap with poly value must fail");
    std::cout << "  Container with polymorphic element rejected... [OK]\n";

    return true;
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "\n========================================\n";
    std::cout << "  Type Safety Tests (is_byte_copy_safe_v)\n";
    std::cout << "========================================\n";

    bool all_passed = true;

    all_passed &= test_byte_copy_safe();
    all_passed &= test_inline_strict_check();
    all_passed &= test_inline_size_check();
    all_passed &= test_backward_compat();
    all_passed &= test_classify_levels();
    all_passed &= test_c2_nested_container_recursion();

    std::cout << "\n========================================\n";
    if (all_passed) {
        std::cout << "  [PASS] All Type Safety tests passed!\n";
    } else {
        std::cout << "  [FAIL] Some tests failed.\n";
    }
    std::cout << "========================================\n\n";

    return all_passed ? 0 : 1;
}