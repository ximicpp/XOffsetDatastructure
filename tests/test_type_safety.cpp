#include "../xoffsetdatastructure.hpp"
#include <iostream>
#include <cassert>
#include <cstddef>   // std::byte, std::nullptr_t

using namespace XOffsetDatastructure;
using namespace boost::typelayout;

// ============================================================================
// Test 1: Basic Types (Should PASS)
// ============================================================================

struct BasicTypes {
    int32_t i32;
    int64_t i64;
    uint32_t u32;
    uint64_t u64;
    float f32;
    double f64;
    bool flag;
    char ch;
    
    template<typename Allocator>
    BasicTypes(Allocator) : i32(0), i64(0), u32(0), u64(0), f32(0.0f), f64(0.0), flag(false), ch('\0') {}
};

static_assert(is_byte_copy_safe_v<BasicTypes>, "BasicTypes should be safe");

// ============================================================================
// Test 2: Container Types (Should PASS)
// ============================================================================

struct ContainerTypes {
    XString name;
    XVector<int32_t> numbers;
    XSet<int32_t> unique_ids;
    XMap<int32_t, XString> id_to_name;
    
    template<typename Allocator>
    ContainerTypes(Allocator alloc) 
        : name(alloc), numbers(alloc), unique_ids(alloc), id_to_name(alloc) {}
};

static_assert(is_byte_copy_safe_v<ContainerTypes>, "ContainerTypes should be safe");

// ============================================================================
// Test 3: Nested Containers (Should PASS)
// ============================================================================

struct NestedContainers {
    XVector<XVector<int32_t>> matrix;
    XMap<XString, XVector<int32_t>> name_to_numbers;
    XVector<XString> strings;
    
    template<typename Allocator>
    NestedContainers(Allocator alloc) 
        : matrix(alloc), name_to_numbers(alloc), strings(alloc) {}
};

static_assert(is_byte_copy_safe_v<NestedContainers>, "NestedContainers should be safe");

// ============================================================================
// Test 4: User-Defined Nested Types (Should PASS)
// ============================================================================

struct Point {
    float x;
    float y;
    float z;
};

struct Player {
    XString name;
    int32_t level;
    Point position;
    XVector<int32_t> inventory;
    
    template<typename Allocator>
    Player(Allocator alloc) 
        : name(alloc), level(0), position{}, inventory(alloc) {}
};

static_assert(is_byte_copy_safe_v<Point>, "Point should be safe");
static_assert(is_byte_copy_safe_v<Player>, "Player should be safe");

// ============================================================================
// Test 5: Complex Nested Structure (Should PASS)
// ============================================================================

struct Item {
    int32_t id;
    XString name;
    float weight;
    
    Item() = default;
    Item(int32_t i, XString n, float w) : id(i), name(n), weight(w) {}
};

struct Inventory {
    XVector<Item> items;
    XMap<int32_t, Item> quick_access;
    
    template<typename Allocator>
    Inventory(Allocator alloc) 
        : items(alloc), quick_access(alloc) {}
};

struct GameState {
    Player player;
    Inventory inventory;
    XVector<Point> waypoints;
    
    template<typename Allocator>
    GameState(Allocator alloc) 
        : player(alloc), inventory(alloc), waypoints(alloc) {}
};

static_assert(is_byte_copy_safe_v<Item>, "Item should be safe");
static_assert(is_byte_copy_safe_v<Inventory>, "Inventory should be safe");
static_assert(is_byte_copy_safe_v<GameState>, "GameState should be safe");

// ============================================================================
// Test 6: Types that SHOULD FAIL
// ============================================================================

// 6.1: Polymorphic type (has virtual function)
struct PolymorphicType {
    virtual void foo() {}  // Virtual function = NOT ALLOWED
    int32_t data;
};

static_assert(!is_byte_copy_safe_v<PolymorphicType>, 
    "PolymorphicType should NOT be safe (has virtual function)");

// 6.2: Contains raw pointer
struct WithRawPointer {
    int32_t* ptr;  // Raw pointer = NOT ALLOWED
    int32_t data;
};

static_assert(!is_byte_copy_safe_v<WithRawPointer>, 
    "WithRawPointer should NOT be safe (has raw pointer)");

// 6.3: Contains std::string
struct WithStdString {
    std::string name;  // std::string = NOT ALLOWED
    int32_t id;
};

static_assert(!is_byte_copy_safe_v<WithStdString>, 
    "WithStdString should NOT be safe (uses std::string)");

// 6.4: Contains std::vector
struct WithStdVector {
    std::vector<int32_t> data;  // std::vector = NOT ALLOWED
};

static_assert(!is_byte_copy_safe_v<WithStdVector>, 
    "WithStdVector should NOT be safe (uses std::vector)");

// 6.5: Nested unsafe type
struct UnsafeNested {
    Point position;  // Safe
    PolymorphicType bad;  // NOT SAFE - contains polymorphic type
};

// TypeLayout now synthesizes a ptr[s:N,a:N] field for every type that
// introduces a vptr.  When PolymorphicType is flattened into UnsafeNested's
// layout signature, the synthesized ptr[ marker causes classify_safety to
// return Warning, which XOffset escalates to Risk → is_byte_copy_safe_v = false.
static_assert(!is_byte_copy_safe_v<UnsafeNested>,
    "UnsafeNested should NOT be safe (contains nested polymorphic type with vptr)");

// 6.6: long / unsigned long — locally safe on ALL platforms (C2).
// TypeLayout classifies long as i32 or i64 depending on platform — both are
// TrivialSafe locally.  Cross-platform mismatch (LP64 vs LLP64) is caught
// by signature comparison in CI (C1 layer).
struct HasLong {
    int32_t ok;
    long    platformDependent;
};

// long is always locally serialization-free: it's a trivially copyable
// scalar with no pointers.  is_byte_copy_safe_v<HasLong> == true.
static_assert(is_byte_copy_safe_v<HasLong>,
    "HasLong: locally safe on all platforms (C2)");

// 6.7: unsigned long — same reasoning as long
struct HasUnsignedLong {
    uint32_t ok;
    unsigned long platformDependent;
};

static_assert(is_byte_copy_safe_v<HasUnsignedLong>,
    "HasUnsignedLong: locally safe on all platforms (C2)");

// 6.8: uint64_t is ALWAYS safe regardless of platform (LP64 or LLP64)
struct HasUint64 {
    uint64_t alwaysSafe;
    int64_t  alsoSafe;
};

static_assert(is_byte_copy_safe_v<HasUint64>,
    "HasUint64 should ALWAYS be safe (fixed-width integers)");

// ============================================================================
// Test 7: Boundary Types (Audit P5)
// Purpose: Validate safety classification for edge-case types:
//   - std::nullptr_t  (Accept: cross-platform binary identical, all zeros)
//   - std::byte       (Safe: alias for unsigned char)
//   - T C::*          (Unsafe: member pointer, encoded as memptr[...])
// ============================================================================

// 7.1: std::nullptr_t — Accepted as safe (P2 decision)
// TypeLayout encodes as nullptr[s:8,a:8], which does NOT contain "ptr["
// so classify_safety returns Safe.  This is intentional: nullptr_t's
// binary representation is all-zeros and cross-platform identical.
struct HasNullptr {
    int32_t x;
    std::nullptr_t n;
};

static_assert(is_byte_copy_safe_v<HasNullptr>,
    "HasNullptr should be safe (nullptr_t is accepted, cross-platform all-zeros)");

// 7.2: std::byte — Safe scalar type
// TypeLayout encodes as byte[s:1,a:1], a safe fundamental type.
struct HasByte {
    std::byte b1;
    std::byte b2;
    int32_t x;
};

static_assert(is_byte_copy_safe_v<HasByte>,
    "HasByte should be safe (std::byte is a safe scalar)");

// 7.3: Member pointer (T C::*) — Should be REJECTED
// TypeLayout encodes as memptr[s:N,a:N], which triggers Warning → Risk.
// Member pointers are implementation-defined and not safe for serialization.
struct Foo { int x; double y; };

struct HasMemberPointer {
    int Foo::* mp;
    int32_t data;
};

static_assert(!is_byte_copy_safe_v<HasMemberPointer>,
    "HasMemberPointer should NOT be safe (member pointer is process-local)");

// 7.4: Member function pointer — Should also be REJECTED
// TypeLayout encodes as memptr[...] or fnptr[...], both trigger Warning → Risk.
struct HasMemberFuncPointer {
    void (Foo::* mfp)();
    int32_t data;
};

static_assert(!is_byte_copy_safe_v<HasMemberFuncPointer>,
    "HasMemberFuncPointer should NOT be safe (member function pointer)");

// ============================================================================
// Runtime Verification — print static_assert results summary
// (Runtime tests for basic/container/nested types removed: covered by
//  test_basic_types, test_vector, test_map_set, test_nested respectively)
// ============================================================================

int main() {
    std::cout << "╔══════════════════════════════════════════╗" << std::endl;
    std::cout << "║  XBuffer Type Safety Compile-Time Test   ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════╝" << std::endl;

    std::cout << "\n✅ SAFE TYPES (static_assert passed):\n";
    std::cout << "  - BasicTypes\n";
    std::cout << "  - ContainerTypes\n";
    std::cout << "  - NestedContainers\n";
    std::cout << "  - Point / Player / GameState\n";
    std::cout << "  - HasLong / HasUnsignedLong / HasUint64\n";
    std::cout << "  - HasNullptr / HasByte\n";

    std::cout << "\n❌ UNSAFE TYPES (static_assert passed):\n";
    std::cout << "  - PolymorphicType (virtual function)\n";
    std::cout << "  - WithRawPointer (raw pointer)\n";
    std::cout << "  - WithStdString / WithStdVector (std containers)\n";
    std::cout << "  - UnsafeNested (nested polymorphic)\n";
    std::cout << "  - HasMemberPointer / HasMemberFuncPointer\n";

    std::cout << "\n╔══════════════════════════════════════════╗" << std::endl;
    std::cout << "║    ✓ ALL COMPILE-TIME CHECKS PASSED!     ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════╝" << std::endl;

    return 0;
}
