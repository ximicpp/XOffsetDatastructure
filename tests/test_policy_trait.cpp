// ============================================================================
// Test: Policy Trait & is_xbuffer_compatible
// Purpose: Validates the new signature-based Policy Trait safety architecture.
//
// Tests:
//   1. DefaultPolicy — Safe types pass, Warning/Risk types rejected
//   2. StrictPolicy<GoldSig> — compile-time signature comparison
//   3. RelaxedPolicy — Warning allowed, Risk rejected
//   4. Custom Hook — user-defined policy
//   5. Backward compatibility — is_xbuffer_safe<T>::value
//   6. Container skip — is_safe_leaf containers with safe/unsafe elements
// ============================================================================

#include <iostream>
#include <cstdint>
#include <cassert>

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;
using namespace XOffsetDatastructure::detail;
using namespace boost::typelayout;
using namespace boost::typelayout::compat;

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

    // Risk types → rejected
    static_assert(!is_xbuffer_compatible<HasWchar>(),
                  "HasWchar must be rejected (Risk)");
    static_assert(!is_xbuffer_compatible<HasLongDouble>(),
                  "HasLongDouble must be rejected (Risk)");
    std::cout << "  Risk types rejected... [OK]\n";

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
// Test 3: RelaxedPolicy — Warning allowed, Risk rejected
// ============================================================================
bool test_relaxed_policy() {
    std::cout << "\n[TEST] RelaxedPolicy\n";
    std::cout << std::string(50, '-') << "\n";

    // Safe types → pass
    static_assert(is_xbuffer_compatible<int32_t, RelaxedPolicy>(),
                  "int32_t must pass Relaxed");
    static_assert(is_xbuffer_compatible<SafeRecord, RelaxedPolicy>(),
                  "SafeRecord must pass Relaxed");
    std::cout << "  Safe types pass... [OK]\n";

    // Warning types → allowed under Relaxed
    static_assert(is_xbuffer_compatible<HasPointer, RelaxedPolicy>(),
                  "HasPointer must pass Relaxed (Warning allowed)");
    std::cout << "  Warning types allowed... [OK]\n";

    // Risk types → still rejected
    static_assert(!is_xbuffer_compatible<HasWchar, RelaxedPolicy>(),
                  "HasWchar must be rejected even under Relaxed");
    static_assert(!is_xbuffer_compatible<HasLongDouble, RelaxedPolicy>(),
                  "HasLongDouble must be rejected even under Relaxed");
    std::cout << "  Risk types rejected... [OK]\n";

    return true;
}

// ============================================================================
// Test 4: Custom Hook
// ============================================================================

/// A user-defined policy that only accepts types smaller than 24 bytes
/// AND classified as Safe.
struct SmallTypePolicy {
    template<typename T>
    static consteval bool accept() {
        return sizeof(T) <= 24
            && classify_for_xoffset<T>() == SafetyLevel::Safe;
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

    // is_safe_type must also agree
    static_assert(is_safe_type<int32_t>() == true, "int32_t is safe");
    static_assert(is_safe_type<HasPointer>() == false, "HasPointer is not safe");

    std::cout << "  is_xbuffer_safe<T> matches is_xbuffer_compatible<T>... [OK]\n";
    std::cout << "  is_safe_type<T>() matches... [OK]\n";

    // reason() still works
    constexpr const char* reason = is_xbuffer_safe<HasPointer>::reason();
    std::cout << "  HasPointer reason: " << reason << "... [OK]\n";

    return true;
}

// ============================================================================
// Test 6: classify_for_xoffset levels
// ============================================================================
bool test_classify_levels() {
    std::cout << "\n[TEST] classify_for_xoffset Levels\n";
    std::cout << std::string(50, '-') << "\n";

    // Safe
    static_assert(classify_for_xoffset<int32_t>() == SafetyLevel::Safe,
                  "int32_t must be Safe");
    static_assert(classify_for_xoffset<SafeRecord>() == SafetyLevel::Safe,
                  "SafeRecord must be Safe");
    std::cout << "  Safe classification... [OK]\n";

    // Warning → escalated to Risk
    static_assert(classify_for_xoffset<HasPointer>() == SafetyLevel::Risk,
                  "HasPointer Warning must be escalated to Risk");
    std::cout << "  Warning→Risk escalation... [OK]\n";

    // Native Risk
    static_assert(classify_for_xoffset<HasWchar>() == SafetyLevel::Risk,
                  "HasWchar must be Risk");
    static_assert(classify_for_xoffset<HasLongDouble>() == SafetyLevel::Risk,
                  "HasLongDouble must be Risk");
    std::cout << "  Risk classification... [OK]\n";

    return true;
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "\n========================================\n";
    std::cout << "  Policy Trait & is_xbuffer_compatible\n";
    std::cout << "========================================\n";

    bool all_passed = true;

    all_passed &= test_default_policy();
    all_passed &= test_strict_policy();
    all_passed &= test_relaxed_policy();
    all_passed &= test_custom_hook();
    all_passed &= test_backward_compat();
    all_passed &= test_classify_levels();

    std::cout << "\n========================================\n";
    if (all_passed) {
        std::cout << "  [PASS] All Policy Trait tests passed!\n";
    } else {
        std::cout << "  [FAIL] Some tests failed.\n";
    }
    std::cout << "========================================\n\n";

    return all_passed ? 0 : 1;
}
