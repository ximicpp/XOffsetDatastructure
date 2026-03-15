// ============================================================================
// Test: Policy Trait & is_xbuffer_compatible
// Purpose: Validates the unified signature-based safety architecture.
//
// Tests:
//   1. DefaultPolicy — Safe types pass, Warning/Risk types rejected
//   2. StrictPolicy<GoldSig> — compile-time signature comparison
//   3. Custom Hook — user-defined policy
//   4. Backward compatibility — is_xbuffer_safe<T>::value
//   5. classify_for_xoffset levels
//
// Note: RelaxedPolicy was removed in the Serialization-free unification.
//       XOffset's zero-encoding model categorically rejects all pointers
//       (Warning→Risk escalation), so a "relaxed" mode is not meaningful.
// ============================================================================

#include <iostream>
#include <cstdint>
#include <cassert>

#include "../xoffsetdatastructure.hpp"

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
// Test 1: DefaultPolicy
// ============================================================================
bool test_default_policy() {
    std::cout << "\n[TEST] DefaultPolicy\n";
    std::cout << std::string(50, '-') << "\n";

    // Safe types
    static_assert(is_xbuffer_compatible<int32_t>(),
                  "int32_t must be compatible");
    static_assert(is_xbuffer_compatible<double>(),
                  "double must be compatible");
    static_assert(is_xbuffer_compatible<SafeRecord>(),
                  "SafeRecord must be compatible");
    static_assert(is_xbuffer_compatible<NestedSafe>(),
                  "NestedSafe must be compatible");
    static_assert(is_xbuffer_compatible<HasEnum>(),
                  "HasEnum (fixed underlying) must be compatible");
    static_assert(is_xbuffer_compatible<HasArray>(),
                  "HasArray must be compatible");
    std::cout << "  Safe types pass... [OK]\n";

    // Warning types → escalated to Risk → rejected by Default
    static_assert(!is_xbuffer_compatible<HasPointer>(),
                  "HasPointer must be rejected (Warning→Risk)");
    static_assert(!is_xbuffer_compatible<Polymorphic>(),
                  "Polymorphic must be rejected (Warning→Risk)");
    std::cout << "  Warning types rejected... [OK]\n";

    // Platform variant types → now accepted by DefaultPolicy (locally serialization-free)
    static_assert(is_xbuffer_compatible<HasWchar>(),
                  "HasWchar is locally safe (trivially_copyable + no pointer)");
    static_assert(is_xbuffer_compatible<HasLongDouble>(),
                  "HasLongDouble is locally safe");
    std::cout << "  Platform variant types accepted locally... [OK]\n";

    return true;
}

// ============================================================================
// Test 2: StrictPolicy — compile-time signature comparison
// ============================================================================
bool test_strict_policy() {
    std::cout << "\n[TEST] StrictPolicy\n";
    std::cout << std::string(50, '-') << "\n";

    // Get the actual gold signature for SafeRecord at compile time
    constexpr auto gold = get_layout_signature<SafeRecord>();

    // Matching signature → pass
    static_assert(is_xbuffer_compatible<SafeRecord, StrictPolicy<gold>>(),
                  "SafeRecord must match its own gold signature");
    std::cout << "  Matching signature passes... [OK]\n";

    // Different type with different layout → fail
    static_assert(!is_xbuffer_compatible<NestedSafe, StrictPolicy<gold>>(),
                  "NestedSafe must NOT match SafeRecord's gold signature");
    std::cout << "  Different layout rejected... [OK]\n";

    // Unsafe type: even if we somehow had a matching sig, it should still fail
    // because StrictPolicy also requires classify_for_xoffset == Safe
    constexpr auto ptr_sig = get_layout_signature<HasPointer>();
    static_assert(!is_xbuffer_compatible<HasPointer, StrictPolicy<ptr_sig>>(),
                  "HasPointer must be rejected even with matching signature");
    std::cout << "  Unsafe type with matching sig rejected... [OK]\n";

    return true;
}

// ============================================================================
// Test 3: Custom Hook
// ============================================================================

/// A user-defined policy that only accepts types smaller than 24 bytes
/// AND classified as Safe.
struct SmallTypePolicy {
    template<typename T>
    static consteval bool accept() {
        return sizeof(T) <= 24
            && XOffsetDatastructure::detail::DefaultPolicy::template accept<std::remove_cv_t<T>>();
    }
};

bool test_custom_hook() {
    std::cout << "\n[TEST] Custom Hook\n";
    std::cout << std::string(50, '-') << "\n";

    // SafeRecord: sizeof = 24 on 64-bit → pass (≤ 24)
    static_assert(sizeof(SafeRecord) <= 24,
                  "SafeRecord should be <= 24 bytes");
    static_assert(is_xbuffer_compatible<SafeRecord, SmallTypePolicy>(),
                  "SafeRecord must pass SmallTypePolicy");
    std::cout << "  Small safe type passes... [OK]\n";

    // NestedSafe: sizeof = 32 (SafeRecord=24 + int64_t=8) → rejected (> 24)
    static_assert(sizeof(NestedSafe) > 24,
                  "NestedSafe should be > 24 bytes for this test");
    static_assert(!is_xbuffer_compatible<NestedSafe, SmallTypePolicy>(),
                  "NestedSafe must be rejected (too large)");
    std::cout << "  Large type rejected... [OK]\n";

    // Unsafe type: even if small, rejected
    static_assert(!is_xbuffer_compatible<HasPointer, SmallTypePolicy>(),
                  "HasPointer must be rejected by SmallTypePolicy");
    std::cout << "  Small unsafe type rejected... [OK]\n";

    return true;
}

// ============================================================================
// Test 5: Backward compatibility — is_xbuffer_safe<T>::value
// ============================================================================
bool test_backward_compat() {
    std::cout << "\n[TEST] Backward Compatibility\n";
    std::cout << std::string(50, '-') << "\n";

    // is_xbuffer_safe<T>::value should behave identically to
    // is_xbuffer_compatible<T, DefaultPolicy>()
    static_assert(is_xbuffer_safe<int32_t>::value == is_xbuffer_compatible<int32_t>(),
                  "is_xbuffer_safe must match is_xbuffer_compatible for int32_t");
    static_assert(is_xbuffer_safe<SafeRecord>::value == is_xbuffer_compatible<SafeRecord>(),
                  "is_xbuffer_safe must match is_xbuffer_compatible for SafeRecord");
    static_assert(is_xbuffer_safe<HasPointer>::value == is_xbuffer_compatible<HasPointer>(),
                  "is_xbuffer_safe must match is_xbuffer_compatible for HasPointer");
    static_assert(is_xbuffer_safe<HasWchar>::value == is_xbuffer_compatible<HasWchar>(),
                  "is_xbuffer_safe must match is_xbuffer_compatible for HasWchar");

    // is_xbuffer_compatible (default policy) must also agree
    static_assert(is_xbuffer_compatible<int32_t>() == true, "int32_t is safe");
    static_assert(is_xbuffer_compatible<HasPointer>() == false, "HasPointer is not safe");

    std::cout << "  is_xbuffer_safe<T> matches is_xbuffer_compatible<T>... [OK]\n";
    std::cout << "  is_xbuffer_compatible<T>() matches... [OK]\n";

    // reason() still works
    constexpr const char* reason = is_xbuffer_safe<HasPointer>::reason();
    std::cout << "  HasPointer reason: " << reason << "... [OK]\n";

    return true;
}

// ============================================================================
// Test 6: TypeLayout classify_v + is_local_serialization_free_v
// ============================================================================
bool test_classify_levels() {
    using boost::typelayout::classify_v;
    using boost::typelayout::SafetyLevel;
    using boost::typelayout::is_local_serialization_free_v;

    std::cout << "\n[TEST] TypeLayout classify_v + is_local_serialization_free_v\n";
    std::cout << std::string(55, '-') << "\n";

    // Safe types → is_local_serialization_free_v == true
    static_assert(classify_v<int32_t> == SafetyLevel::TrivialSafe,
                  "int32_t must be TrivialSafe");
    static_assert(is_local_serialization_free_v<int32_t>,
                  "int32_t must be serialization free");
    static_assert(is_local_serialization_free_v<SafeRecord>,
                  "SafeRecord must be serialization free");
    std::cout << "  Safe classification... [OK]\n";

    // Pointer types → PointerRisk
    static_assert(classify_v<HasPointer> == SafetyLevel::PointerRisk,
                  "HasPointer must be PointerRisk");
    static_assert(!is_local_serialization_free_v<HasPointer>,
                  "HasPointer must NOT be serialization free");
    std::cout << "  PointerRisk types rejected... [OK]\n";

    // Platform variant types → PlatformVariant (but locally serialization-free)
    static_assert(classify_v<HasWchar> == SafetyLevel::PlatformVariant,
                  "HasWchar must be PlatformVariant");
    static_assert(is_local_serialization_free_v<HasWchar>,
                  "HasWchar is locally serialization free (trivially_copyable + no pointer)");
    static_assert(classify_v<HasLongDouble> == SafetyLevel::PlatformVariant,
                  "HasLongDouble must be PlatformVariant");
    static_assert(is_local_serialization_free_v<HasLongDouble>,
                  "HasLongDouble is locally serialization free");
    std::cout << "  PlatformVariant types classified correctly... [OK]\n";

    return true;
}

// ============================================================================
// Test 6: C2 — Nested container recursive safety
// (Merged from test_remediation_fixes.cpp)
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
    static_assert(is_xbuffer_compatible<XVector<int32_t>>(), "XVector<int32_t> must pass");
    static_assert(is_xbuffer_compatible<XVector<SafeFlat>>(), "XVector<SafeFlat> must pass");
    static_assert(!is_xbuffer_compatible<XVector<HasPointer>>(), "XVector<HasPointer> must be rejected");
    std::cout << "  Single-level containers... [OK]\n";

    // Nested safe/unsafe container — THE KEY C2 FIX
    static_assert(is_xbuffer_compatible<XVector<XVector<int32_t>>>(), "nested safe must pass");
    static_assert(!is_xbuffer_compatible<XVector<XVector<HasPointer>>>(), "nested unsafe must fail");
    std::cout << "  Nested containers (C2 fix)... [OK]\n";

    // Triple-nested
    static_assert(!is_xbuffer_compatible<XVector<XVector<XVector<HasPointer>>>>(), "triple-nested unsafe must fail");
    static_assert(is_xbuffer_compatible<XVector<XVector<XVector<int32_t>>>>(), "triple-nested safe must pass");
    std::cout << "  Triple-nested containers... [OK]\n";

    // Map containers
    static_assert(is_xbuffer_compatible<XMap<int32_t, SafeFlat>>(), "safe map must pass");
    static_assert(!is_xbuffer_compatible<XMap<int32_t, HasPointer>>(), "unsafe map value must fail");
    static_assert(!is_xbuffer_compatible<XMap<HasPointer, int32_t>>(), "unsafe map key must fail");
    std::cout << "  Map containers... [OK]\n";

    // Container holding polymorphic type — C1+C2 interaction
    static_assert(!is_xbuffer_compatible<XVector<Polymorphic>>(), "XVector<Polymorphic> must fail");
    static_assert(!is_xbuffer_compatible<XMap<int32_t, Polymorphic>>(), "XMap with poly value must fail");
    std::cout << "  Container with polymorphic element rejected... [OK]\n";

    return true;
}

// ============================================================================
// Test 7: diagnose_unsafe_members with Policy parameter
// (Merged from test_remediation_fixes.cpp — L2 fix)
// ============================================================================

// Custom policy: accepts all types (for testing diagnose API)
struct AcceptAllPolicy {
    template<typename T>
    static consteval bool accept() { return true; }
};

bool test_diagnose_with_policy() {
    std::cout << "\n[TEST] diagnose_unsafe_members accepts Policy\n";
    std::cout << std::string(55, '-') << "\n";

    diagnose_unsafe_members<SafeFlat, DefaultPolicy>();
    std::cout << "  diagnose<SafeFlat, Default> compiles... [OK]\n";

    diagnose_unsafe_members<HasPointer, AcceptAllPolicy>();
    std::cout << "  diagnose<HasPointer, AcceptAllPolicy> compiles... [OK]\n";

    return true;
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "\n========================================\n";
    std::cout << "  Policy Trait & Safety Tests\n";
    std::cout << "========================================\n";

    bool all_passed = true;

    all_passed &= test_default_policy();
    all_passed &= test_strict_policy();
    all_passed &= test_custom_hook();
    all_passed &= test_backward_compat();
    all_passed &= test_classify_levels();
    all_passed &= test_c2_nested_container_recursion();
    all_passed &= test_diagnose_with_policy();

    std::cout << "\n========================================\n";
    if (all_passed) {
        std::cout << "  [PASS] All Policy & Safety tests passed!\n";
    } else {
        std::cout << "  [FAIL] Some tests failed.\n";
    }
    std::cout << "========================================\n\n";

    return all_passed ? 0 : 1;
}
