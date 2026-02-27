// ============================================================================
// Test: long Reflection Probe
// Purpose: Verify whether P2996 reflection can distinguish `long` from its
//          fixed-width typedef aliases (int32_t on LLP64, int64_t on LP64).
//
// This is a diagnostic test to determine if "Plan B" (reflection-based
// detection of `long` inside structs) is feasible.
// ============================================================================

#include <iostream>
#include <cstdint>
#include <type_traits>
#include <experimental/meta>
#include <string_view>

using namespace std::meta;

// Typedef aliases to work around ^^(multi-word-type) limitation
using long_t = long;
using ulong_t = unsigned long;
using llong_t = long long;
using ullong_t = unsigned long long;

// Test struct with various integer types
struct ProbeStruct {
    int32_t       a;
    int64_t       b;
    long          c;
    unsigned long d;
    long long     e;
    int           f;
};

// ---- Compile-time probes (consteval) ----

// Check if display_string_of can distinguish long from int64_t
template<typename T>
consteval bool has_long_member_display() {
    auto mems = nonstatic_data_members_of(^^T, access_context::unchecked());
    for (size_t i = 0; i < mems.size(); ++i) {
        auto tname = display_string_of(type_of(mems[i]));
        if (tname.find("long") != std::string_view::npos)
            return true;
    }
    return false;
}

// Check type_of identity: 'long c' vs 'int64_t b'
template<typename T>
consteval bool member_long_equals_int64() {
    auto mems = nonstatic_data_members_of(^^T, access_context::unchecked());
    if (mems.size() < 3) return false;
    return type_of(mems[1]) == type_of(mems[2]); // int64_t b vs long c
}

// Compile-time: ^^long == ^^int64_t ?
consteval bool reflect_long_eq_i64() { return ^^long == ^^int64_t; }
consteval bool reflect_long_eq_i32() { return ^^long == ^^int32_t; }
consteval bool reflect_llong_eq_i64() { return ^^llong_t == ^^int64_t; }
consteval bool reflect_ulong_eq_u32() { return ^^ulong_t == ^^uint32_t; }
consteval bool reflect_ulong_eq_u64() { return ^^ulong_t == ^^uint64_t; }

// Compile-time: display_string_of for key types
consteval std::string_view ds_int32()  { return display_string_of(^^int32_t); }
consteval std::string_view ds_int64()  { return display_string_of(^^int64_t); }
consteval std::string_view ds_long()   { return display_string_of(^^long); }
consteval std::string_view ds_ulong()  { return display_string_of(^^ulong_t); }
consteval std::string_view ds_llong()  { return display_string_of(^^llong_t); }
consteval std::string_view ds_int()    { return display_string_of(^^int); }

// Compile-time: get display_string_of for each member of ProbeStruct
template<size_t I>
consteval std::string_view probe_member_name() {
    auto mems = nonstatic_data_members_of(^^ProbeStruct, access_context::unchecked());
    return identifier_of(mems[I]);
}
template<size_t I>
consteval std::string_view probe_member_type_display() {
    auto mems = nonstatic_data_members_of(^^ProbeStruct, access_context::unchecked());
    return display_string_of(type_of(mems[I]));
}

int main() {
    std::cout << "=== P2996 Long Reflection Probe ===\n\n";

    // ---- Section 1: Platform info ----
    std::cout << "[1] Platform Info:\n";
    std::cout << "  sizeof(long)          = " << sizeof(long) << "\n";
    std::cout << "  sizeof(long long)     = " << sizeof(long long) << "\n";
    std::cout << "  sizeof(int32_t)       = " << sizeof(int32_t) << "\n";
    std::cout << "  sizeof(int64_t)       = " << sizeof(int64_t) << "\n";
    std::cout << "  long == int32_t?      " << std::is_same_v<long, int32_t> << "\n";
    std::cout << "  long == int64_t?      " << std::is_same_v<long, int64_t> << "\n";
    std::cout << "  long long == int64_t? " << std::is_same_v<long long, int64_t> << "\n";
    std::cout << "\n";

    // ---- Section 2: Direct type display_string_of ----
    std::cout << "[2] Direct Type display_string_of:\n";
    std::cout << "  int32_t:       " << ds_int32() << "\n";
    std::cout << "  int64_t:       " << ds_int64() << "\n";
    std::cout << "  long:          " << ds_long() << "\n";
    std::cout << "  unsigned long: " << ds_ulong() << "\n";
    std::cout << "  long long:     " << ds_llong() << "\n";
    std::cout << "  int:           " << ds_int() << "\n";
    std::cout << "\n";

    // ---- Section 3: Reflection identity comparison ----
    std::cout << "[3] Reflection Identity (^^type == ^^type):\n";
    std::cout << "  ^^long == ^^int32_t?             " << reflect_long_eq_i32() << "\n";
    std::cout << "  ^^long == ^^int64_t?             " << reflect_long_eq_i64() << "\n";
    std::cout << "  ^^(long long) == ^^int64_t?      " << reflect_llong_eq_i64() << "\n";
    std::cout << "  ^^(unsigned long) == ^^uint32_t?  " << reflect_ulong_eq_u32() << "\n";
    std::cout << "  ^^(unsigned long) == ^^uint64_t?  " << reflect_ulong_eq_u64() << "\n";
    std::cout << "\n";

    // ---- Section 4: Struct member type inspection ----
    std::cout << "[4] Struct Member Type Inspection (ProbeStruct):\n";
    std::cout << "  " << probe_member_name<0>() << ": " << probe_member_type_display<0>() << "\n";
    std::cout << "  " << probe_member_name<1>() << ": " << probe_member_type_display<1>() << "\n";
    std::cout << "  " << probe_member_name<2>() << ": " << probe_member_type_display<2>() << "\n";
    std::cout << "  " << probe_member_name<3>() << ": " << probe_member_type_display<3>() << "\n";
    std::cout << "  " << probe_member_name<4>() << ": " << probe_member_type_display<4>() << "\n";
    std::cout << "  " << probe_member_name<5>() << ": " << probe_member_type_display<5>() << "\n";
    std::cout << "\n";

    // ---- Section 5: Detection feasibility ----
    std::cout << "[5] Detection Feasibility:\n";
    constexpr bool can_detect = has_long_member_display<ProbeStruct>();
    std::cout << "  Can detect 'long' via display_string_of? " << can_detect << "\n";

    constexpr bool member_types_equal = member_long_equals_int64<ProbeStruct>();
    std::cout << "  type_of('long c') == type_of('int64_t b')? " << member_types_equal << "\n";
    std::cout << "\n";

    // ---- Section 6: Conclusion ----
    if constexpr (can_detect) {
        std::cout << "[RESULT] Plan B IS FEASIBLE: P2996 display_string_of can distinguish 'long' from fixed-width types!\n";
    } else if constexpr (!member_types_equal) {
        std::cout << "[RESULT] Plan B PARTIALLY FEASIBLE: display_string_of can't, but type_of identity can distinguish them.\n";
    } else {
        std::cout << "[RESULT] Plan B NOT FEASIBLE: P2996 treats 'long' and its typedef alias as identical.\n";
        std::cout << "         Falling back to Plan C (documentation + coding guidelines).\n";
    }

    return 0;
}