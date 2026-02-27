// ============================================================================
// Test: Remediation Fixes — C1, C2, M1, M2, L2
// Purpose: Validates all architectural fixes from the safety audit.
//
// Tests:
//   1. C1: Polymorphic member vptr propagation in TypeLayout signatures
//   2. C2: Nested container recursive safety (XVector<XVector<...>>)
//   3. M1: RelaxedPolicy uniform recursive behaviour
//   4. M2: long / unsigned long explicit rejection
//   5. L2: diagnose_unsafe_members with Policy parameter
//   6. Combined: interaction between fixes
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
// Test Types
// ============================================================================

// --- Safe base types ---
struct SafeFlat {
    int32_t  x;
    float    y;
    double   z;
};

// --- Polymorphic types (for C1) ---
struct PolyBase {
    virtual ~PolyBase() = default;
    int32_t a;
};

struct PolyDerived : PolyBase {
    float b;
};

// Struct that *embeds* a polymorphic member
struct EmbedsPoly {
    int32_t     id;
    PolyBase    poly_member;  // C1: must cause rejection
    float       val;
};

// Struct that embeds a polymorphic derived
struct EmbedsPolyDerived {
    PolyDerived pd;
    int32_t     tag;
};

// Deeply nested: safe struct containing struct that embeds polymorphic
struct DeepPoly {
    int32_t      head;
    EmbedsPoly   inner;       // transitively polymorphic
};

// --- Pointer types ---
struct HasPointer {
    int32_t* ptr;
    int32_t  val;
};

// --- Risk types ---
struct HasWchar {
    wchar_t ch;
    int32_t x;
};

struct HasLongDouble {
    long double ld;
    int32_t     x;
};

// --- Long types (for M2) ---
struct HasLong {
    long    val;
    int32_t x;
};

struct HasUnsignedLong {
    unsigned long val;
    int32_t       x;
};

struct HasLongInNested {
    SafeFlat  safe;
    HasLong   unsafe;
};

// On LP64 (Linux), long == int64_t, so both classify_for_xoffset<long>() and
// classify_safety<HasLong>() return Safe (identical binary layout).
// On LLP64 (Windows), long != int64_t, so long is Risk (platform-variable size).
constexpr bool long_is_int64 = std::is_same_v<long, int64_t>;

// ============================================================================
// Test 1: C1 — Polymorphic member vptr propagation
// ============================================================================
bool test_c1_polymorphic_member() {
    std::cout << "\n[TEST] C1: Polymorphic member vptr propagation\n";
    std::cout << std::string(55, '-') << "\n";

    // Direct polymorphic type → rejected
    static_assert(!is_xbuffer_compatible<PolyBase>(),
                  "PolyBase (polymorphic) must be rejected");
    static_assert(!is_xbuffer_compatible<PolyDerived>(),
                  "PolyDerived (polymorphic) must be rejected");
    std::cout << "  Direct polymorphic types rejected... [OK]\n";

    // Struct embedding polymorphic member → rejected (C1 fix!)
    static_assert(!is_xbuffer_compatible<EmbedsPoly>(),
                  "EmbedsPoly (embeds PolyBase) must be rejected");
    static_assert(!is_xbuffer_compatible<EmbedsPolyDerived>(),
                  "EmbedsPolyDerived (embeds PolyDerived) must be rejected");
    std::cout << "  Struct with polymorphic member rejected... [OK]\n";

    // Deeply nested polymorphic → rejected
    static_assert(!is_xbuffer_compatible<DeepPoly>(),
                  "DeepPoly (transitively embeds polymorphic) must be rejected");
    std::cout << "  Deeply nested polymorphic rejected... [OK]\n";

    // Verify TypeLayout classify_safety also catches embedded polymorphic
    // classify_safety should see the vptr marker in the parent's signature
    constexpr auto lvl_embed = classify_safety<EmbedsPoly>();
    static_assert(lvl_embed == SafetyLevel::Warning,
                  "EmbedsPoly should be Warning (has vptr via embedded member)");
    std::cout << "  classify_safety detects embedded vptr (Warning)... [OK]\n";

    // Print signatures for diagnostic
    constexpr auto sig_poly = get_layout_signature<PolyBase>();
    constexpr auto sig_embed = get_layout_signature<EmbedsPoly>();
    std::cout << "  PolyBase signature: " << sig_poly << "\n";
    std::cout << "  EmbedsPoly signature: " << sig_embed << "\n";

    return true;
}

// ============================================================================
// Test 2: C2 — Nested container recursive safety
// ============================================================================
bool test_c2_nested_container_recursion() {
    std::cout << "\n[TEST] C2: Nested container recursive safety\n";
    std::cout << std::string(55, '-') << "\n";

    // Single-level safe container
    static_assert(is_xbuffer_compatible<XVector<int32_t>>(),
                  "XVector<int32_t> must pass");
    static_assert(is_xbuffer_compatible<XVector<SafeFlat>>(),
                  "XVector<SafeFlat> must pass");
    std::cout << "  Single-level safe containers pass... [OK]\n";

    // Single-level unsafe container
    static_assert(!is_xbuffer_compatible<XVector<HasPointer>>(),
                  "XVector<HasPointer> must be rejected");
    std::cout << "  Single-level unsafe containers rejected... [OK]\n";

    // Nested safe container
    static_assert(is_xbuffer_compatible<XVector<XVector<int32_t>>>(),
                  "XVector<XVector<int32_t>> must pass");
    static_assert(is_xbuffer_compatible<XVector<XVector<SafeFlat>>>(),
                  "XVector<XVector<SafeFlat>> must pass");
    std::cout << "  Nested safe containers pass... [OK]\n";

    // Nested unsafe container — THE KEY C2 FIX
    static_assert(!is_xbuffer_compatible<XVector<XVector<HasPointer>>>(),
                  "XVector<XVector<HasPointer>> must be rejected (C2 fix!)");
    std::cout << "  XVector<XVector<HasPointer>> rejected... [OK]\n";

    // Triple-nested
    static_assert(!is_xbuffer_compatible<XVector<XVector<XVector<HasPointer>>>>(),
                  "Triple-nested with HasPointer must be rejected");
    static_assert(is_xbuffer_compatible<XVector<XVector<XVector<int32_t>>>>(),
                  "Triple-nested with int32_t must pass");
    std::cout << "  Triple-nested containers checked... [OK]\n";

    // Map containers
    static_assert(is_xbuffer_compatible<XMap<int32_t, SafeFlat>>(),
                  "XMap<int32_t, SafeFlat> must pass");
    static_assert(!is_xbuffer_compatible<XMap<int32_t, HasPointer>>(),
                  "XMap<int32_t, HasPointer> must be rejected");
    static_assert(!is_xbuffer_compatible<XMap<HasPointer, int32_t>>(),
                  "XMap<HasPointer, int32_t> must be rejected (unsafe key)");
    std::cout << "  Map containers checked... [OK]\n";

    // Container holding polymorphic type — C1+C2 interaction
    static_assert(!is_xbuffer_compatible<XVector<PolyBase>>(),
                  "XVector<PolyBase> must be rejected");
    static_assert(!is_xbuffer_compatible<XVector<EmbedsPoly>>(),
                  "XVector<EmbedsPoly> must be rejected");
    std::cout << "  Container with polymorphic element rejected... [OK]\n";

    // Nested container with long element — C2+M2 interaction
    // On LP64 (Linux), long == int64_t, so HasLong is safe (same binary layout).
    // On LLP64/macOS, HasLong is unsafe.
    static_assert(is_xbuffer_compatible<XVector<XVector<HasLong>>>() == long_is_int64,
                  "XVector<XVector<HasLong>> depends on platform long size");
    std::cout << "  Nested container with long: platform-aware check... [OK]\n";

    return true;
}

// ============================================================================
// Test 3: M1 — RelaxedPolicy uniform recursive behaviour
// ============================================================================
bool test_m1_relaxed_policy_recursion() {
    std::cout << "\n[TEST] M1: RelaxedPolicy recursive behaviour\n";
    std::cout << std::string(55, '-') << "\n";

    // Safe types → pass Relaxed
    static_assert(is_xbuffer_compatible<int32_t, RelaxedPolicy>(),
                  "int32_t must pass Relaxed");
    static_assert(is_xbuffer_compatible<SafeFlat, RelaxedPolicy>(),
                  "SafeFlat must pass Relaxed");
    std::cout << "  Safe types pass Relaxed... [OK]\n";

    // Warning types (pointers) → pass Relaxed
    static_assert(is_xbuffer_compatible<HasPointer, RelaxedPolicy>(),
                  "HasPointer (Warning) must pass Relaxed");
    std::cout << "  Warning types pass Relaxed... [OK]\n";

    // Risk types → rejected even under Relaxed
    static_assert(!is_xbuffer_compatible<HasWchar, RelaxedPolicy>(),
                  "HasWchar (Risk) must be rejected by Relaxed");
    static_assert(!is_xbuffer_compatible<HasLongDouble, RelaxedPolicy>(),
                  "HasLongDouble (Risk) must be rejected by Relaxed");
    std::cout << "  Risk types rejected by Relaxed... [OK]\n";

    // Container with safe elements → pass Relaxed
    static_assert(is_xbuffer_compatible<XVector<int32_t>, RelaxedPolicy>(),
                  "XVector<int32_t> must pass Relaxed");
    std::cout << "  Container with safe elements passes Relaxed... [OK]\n";

    // Container with Warning elements → pass Relaxed (Warning allowed)
    static_assert(is_xbuffer_compatible<XVector<HasPointer>, RelaxedPolicy>(),
                  "XVector<HasPointer> must pass Relaxed");
    std::cout << "  Container with Warning elements passes Relaxed... [OK]\n";

    // Container with Risk elements → rejected under Relaxed
    static_assert(!is_xbuffer_compatible<XVector<HasWchar>, RelaxedPolicy>(),
                  "XVector<HasWchar> must be rejected by Relaxed");
    std::cout << "  Container with Risk elements rejected by Relaxed... [OK]\n";

    // Nested container with Risk elements → M1 recursion fix
    static_assert(!is_xbuffer_compatible<XVector<XVector<HasWchar>>, RelaxedPolicy>(),
                  "Nested with HasWchar must be rejected by Relaxed");
    std::cout << "  Nested container with Risk rejected by Relaxed... [OK]\n";

    // Nested container with Warning → allowed under Relaxed
    static_assert(is_xbuffer_compatible<XVector<XVector<HasPointer>>, RelaxedPolicy>(),
                  "Nested with HasPointer must pass Relaxed (Warning OK)");
    std::cout << "  Nested container with Warning passes Relaxed... [OK]\n";

    // Long types under Relaxed — platform-dependent (M2 interaction)
    // On LP64, long == int64_t → HasLong is Safe → Relaxed accepts.
    // On LLP64, long != int64_t → HasLong is Risk → Relaxed rejects.
    static_assert(is_xbuffer_compatible<HasLong, RelaxedPolicy>() == long_is_int64,
                  "Relaxed on HasLong: platform-dependent");
    static_assert(is_xbuffer_compatible<XVector<HasLong>, RelaxedPolicy>() == long_is_int64,
                  "Relaxed on XVector<HasLong>: platform-dependent");
    std::cout << "  Long types under Relaxed: platform-aware check... [OK]\n";

    return true;
}

// ============================================================================
// Test 4: M2 — long / unsigned long explicit rejection
// ============================================================================
bool test_m2_long_rejection() {
    std::cout << "\n[TEST] M2: long/unsigned long rejection\n";
    std::cout << std::string(55, '-') << "\n";

    // long and unsigned long:
    // On LP64 (Linux), long == int64_t → classify_for_xoffset returns Safe.
    // On LLP64 (Windows), long != int64_t → classify_for_xoffset returns Risk.
    //
    // NOTE: We cannot use if constexpr branches with static_assert in a
    // non-template function — the discarded branch's static_assert is still
    // evaluated by the compiler.  Use implication-style assertions instead:
    //   long_is_int64 → Safe, and !long_is_int64 → Risk.
    static_assert(!long_is_int64 || classify_for_xoffset<long>() == SafetyLevel::Safe,
                  "long == int64_t on LP64, so classify_for_xoffset must return Safe");
    static_assert(long_is_int64 || classify_for_xoffset<long>() == SafetyLevel::Risk,
                  "long != int64_t on LLP64, so classify_for_xoffset must return Risk");
    static_assert(!long_is_int64 || classify_for_xoffset<unsigned long>() == SafetyLevel::Safe,
                  "unsigned long == uint64_t on LP64, so classify_for_xoffset must return Safe");
    static_assert(long_is_int64 || classify_for_xoffset<unsigned long>() == SafetyLevel::Risk,
                  "unsigned long != uint64_t on LLP64, so classify_for_xoffset must return Risk");
    if constexpr (long_is_int64) {
        std::cout << "  classify_for_xoffset: long==int64_t on LP64, Safe... [OK]\n";
    } else {
        std::cout << "  classify_for_xoffset: long!=int64_t on LLP64, Risk... [OK]\n";
    }

    // Struct containing long:
    // On LP64 (Linux/macOS-64), long == int64_t, so classify_for_xoffset<long>()
    // is Safe (not rejected) and classify_safety<HasLong>() is also Safe.
    // On LLP64 (Windows), long is 4 bytes and NOT equivalent, so it's Risk.
    static_assert(is_xbuffer_compatible<HasLong>() == long_is_int64,
                  "HasLong: safe iff long == int64_t on this platform");
    static_assert(is_xbuffer_compatible<HasUnsignedLong>() == long_is_int64,
                  "HasUnsignedLong: safe iff unsigned long == uint64_t");
    std::cout << "  Structs with long/ulong: platform-aware check... [OK]\n";

    // Nested struct containing long → same platform logic
    static_assert(is_xbuffer_compatible<HasLongInNested>() == long_is_int64,
                  "HasLongInNested: safe iff long == int64_t");
    std::cout << "  Nested struct with long: platform-aware check... [OK]\n";

    // Container with long element → same platform logic
    static_assert(is_xbuffer_compatible<XVector<HasLong>>() == long_is_int64,
                  "XVector<HasLong>: safe iff long == int64_t");
    std::cout << "  XVector<HasLong>: platform-aware check... [OK]\n";

    // DefaultPolicy
    static_assert(DefaultPolicy::accept<HasLong>() == long_is_int64,
                  "DefaultPolicy on HasLong: platform-dependent");
    std::cout << "  DefaultPolicy on HasLong: platform-aware check... [OK]\n";

    // RelaxedPolicy — same as Default for structs with long
    static_assert(RelaxedPolicy::accept<HasLong>() == long_is_int64,
                  "RelaxedPolicy on HasLong: platform-dependent");
    std::cout << "  RelaxedPolicy on HasLong: platform-aware check... [OK]\n";

    // Contrast: int64_t is fine (fixed-width)
    static_assert(is_xbuffer_compatible<int64_t>(),
                  "int64_t must pass (fixed width)");
    std::cout << "  int64_t passes (fixed width contrast)... [OK]\n";

    return true;
}

// ============================================================================
// Test 5: L2 — diagnose_unsafe_members with Policy
// ============================================================================
bool test_l2_diagnose_with_policy() {
    std::cout << "\n[TEST] L2: diagnose_unsafe_members accepts Policy\n";
    std::cout << std::string(55, '-') << "\n";

    // diagnose_unsafe_members<T, Policy> must compile without error for safe types
    // Using DefaultPolicy
    diagnose_unsafe_members<SafeFlat, DefaultPolicy>();
    std::cout << "  diagnose_unsafe_members<SafeFlat, Default> compiles... [OK]\n";

    // Using RelaxedPolicy
    diagnose_unsafe_members<SafeFlat, RelaxedPolicy>();
    std::cout << "  diagnose_unsafe_members<SafeFlat, Relaxed> compiles... [OK]\n";

    // HasPointer: would fail with DefaultPolicy, but should pass with RelaxedPolicy
    // (Warning is allowed under Relaxed, so diagnose should not fire)
    // Note: We can't actually call diagnose on a failing type (static_assert fires).
    // But we can verify that the diagnose function accepts Policy template arg.
    diagnose_unsafe_members<HasPointer, RelaxedPolicy>();
    std::cout << "  diagnose<HasPointer, Relaxed> compiles (Warning OK)... [OK]\n";

    return true;
}

// ============================================================================
// Test 6: Combined — interaction between all fixes
// ============================================================================
bool test_combined_interactions() {
    std::cout << "\n[TEST] Combined: Fix interactions\n";
    std::cout << std::string(55, '-') << "\n";

    // C1+C2: Container of struct that embeds polymorphic
    static_assert(!is_xbuffer_compatible<XVector<EmbedsPoly>>(),
                  "XVector<EmbedsPoly> must be rejected (C1+C2)");
    static_assert(!is_xbuffer_compatible<XVector<XVector<EmbedsPoly>>>(),
                  "Nested XVector<EmbedsPoly> must be rejected (C1+C2)");
    std::cout << "  C1+C2: nested container + embedded poly... [OK]\n";

    // C2+M2: Container nesting long type — platform-dependent
    static_assert(is_xbuffer_compatible<XVector<XVector<HasLong>>>() == long_is_int64,
                  "Nested XVector<HasLong>: platform-dependent (C2+M2)");
    std::cout << "  C2+M2: nested container + long type: platform-aware... [OK]\n";

    // M1+C2: Relaxed policy on nested unsafe container
    static_assert(!is_xbuffer_compatible<XVector<XVector<HasWchar>>, RelaxedPolicy>(),
                  "Relaxed: nested XVector<HasWchar> Risk must be rejected (M1+C2)");
    static_assert(is_xbuffer_compatible<XVector<XVector<HasPointer>>, RelaxedPolicy>(),
                  "Relaxed: nested XVector<HasPointer> Warning must pass (M1+C2)");
    std::cout << "  M1+C2: Relaxed on nested containers... [OK]\n";

    // Map with polymorphic value — C1+C2
    static_assert(!is_xbuffer_compatible<XMap<int32_t, EmbedsPoly>>(),
                  "XMap<int32_t, EmbedsPoly> must be rejected");
    std::cout << "  C1+C2: map with polymorphic value... [OK]\n";

    // All safe paths still work
    static_assert(is_xbuffer_compatible<XVector<XVector<XVector<SafeFlat>>>>(),
                  "3-level nested safe containers must pass");
    static_assert(is_xbuffer_compatible<XMap<int32_t, XVector<SafeFlat>>>(),
                  "Map<int, XVector<SafeFlat>> must pass");
    std::cout << "  All safe nested paths still pass... [OK]\n";

    return true;
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "\n============================================\n";
    std::cout << "  Remediation Fixes Validation (C1-M2-L2)\n";
    std::cout << "============================================\n";

    bool all_passed = true;

    all_passed &= test_c1_polymorphic_member();
    all_passed &= test_c2_nested_container_recursion();
    all_passed &= test_m1_relaxed_policy_recursion();
    all_passed &= test_m2_long_rejection();
    all_passed &= test_l2_diagnose_with_policy();
    all_passed &= test_combined_interactions();

    std::cout << "\n============================================\n";
    if (all_passed) {
        std::cout << "  [PASS] All Remediation Fix tests passed!\n";
    } else {
        std::cout << "  [FAIL] Some tests failed.\n";
    }
    std::cout << "============================================\n\n";

    return all_passed ? 0 : 1;
}
