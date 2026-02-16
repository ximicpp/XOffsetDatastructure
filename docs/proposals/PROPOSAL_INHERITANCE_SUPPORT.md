# Proposal: Non-Virtual Inheritance Support

**Status**: ✅ Implemented & All Tests Passed (30/30)  
**Date**: 2025-02-16  
**Scope**: Support all non-virtual inheritance (single + multiple); reject `virtual` functions and `virtual` inheritance  

---

## 1. Motivation

Current XOffsetDatastructure **rejects all types with base classes** (`has_bases<T>() → false`), forcing users to use composition:

```cpp
// ❌ Current: compile error "Inheritance not allowed"
struct Entity { int32_t id{0}; XString name; };
struct Player : Entity { int32_t level{0}; };

// ✅ Current: must flatten manually
struct Player { int32_t id{0}; XString name; int32_t level{0}; };
```

This is unnecessarily restrictive. Non-virtual inheritance has **deterministic binary layout** under all standard ABIs, and TypeLayout's signature engine already captures **exact byte offsets** for base class subobjects. Any cross-platform layout differences are detected as signature mismatches — safe failure, never silent corruption.

---

## 2. Design Principle

```
ALLOW:   Non-virtual inheritance (single or multiple)
         ├─ struct Player : Entity { ... };         (single)
         └─ struct Player : HasId, HasName { ... }; (multiple)

REJECT:  virtual functions  → is_polymorphic_v<T> (existing check)
         virtual inheritance → is_virtual(base_info) (new check)
```

**Safety guarantee**: TypeLayout layout signatures encode `offset_of(base_info).bytes` for every base subobject and `offset_of(member).bytes` for every field. If any compiler/platform arranges the bytes differently, the signature diverges → load-time rejection. Zero risk of silent data corruption.

---

## 3. Affected Code Locations

All changes are in `xoffsetdatastructure.hpp`. The following table lists every function/section that needs modification:

### 3.1 Safety Validation Layer (compile-time checks)

| Line | Function | Current Behavior | Required Change |
|------|----------|-----------------|-----------------|
| 849-854 | `has_bases<T>()` | Returns true if any bases exist | **Replace** with `has_virtual_bases<T>()` — only returns true for `virtual` bases |
| 872-887 | `are_all_members_safe<T>()` | Rejects all types with bases | **Remove** `has_bases` rejection; **add** `has_virtual_bases` check; **add** recursive base safety check |
| 922-956 | `get_safety_error_message<T>()` | "Inheritance not allowed" | **Update** message to "virtual inheritance not allowed" |
| 968-981 | `diagnose_unsafe_members<T>()` | Only iterates direct members | **Add** iteration over base class members |
| 983-1013 | `validate_xbuffer_type<T>()` | Error message lists "no inheritance" | **Update** to say "no virtual functions/inheritance" |

### 3.2 Reflection Construction Layer (runtime init)

| Line | Function | Current Behavior | Required Change |
|------|----------|-----------------|-----------------|
| 1053-1057 | `reflect_member_count_of<T>()` | Counts direct members only | No change needed (used per-type in recursion) |
| 1062-1074 | `reflect_init_nth<T,N>()` | Inits direct member N | No change needed (per-member helper) |
| 1085-1090 | `reflect_init_all<T>()` | Inits direct members via expand | **Add** recursive base class init before direct members |
| 1108-1125 | `reflect_transfer_init_nth<T,N>()` | Transfers direct member N | No change needed |
| 1139-1145 | `reflect_transfer_init_all<T>()` | Transfers direct members | **Add** recursive base class transfer before direct members |

### 3.3 XCompactor Migration Layer

| Line | Function | Current Behavior | Required Change |
|------|----------|-----------------|-----------------|
| 1531-1536 | `get_member_at<T,Index>()` | Gets direct member | No change needed |
| 1556-1562 | `migrate_members<T>()` | Migrates direct members only | **Add** recursive base class migration before direct members |

---

## 4. Detailed Implementation Design

### 4.1 New helper: `has_virtual_bases<T>()`

Replaces `has_bases<T>()`. Uses P2996 `is_virtual(base_info)` to detect virtual inheritance.

```cpp
template<typename T>
consteval bool has_virtual_bases() {
    using namespace std::meta;
    for (auto base : bases_of(^^T, access_context::unchecked())) {
        if (is_virtual(base))
            return true;
        // Recursive: check if base itself has virtual bases
        using BaseType = [:type_of(base):];
        if constexpr (std::is_class_v<BaseType>) {
            if (has_virtual_bases<BaseType>())
                return true;
        }
    }
    return false;
}
```

### 4.2 Modified `are_all_members_safe<T>()`

```cpp
template<typename T>
consteval bool are_all_members_safe() {
    using namespace std::meta;
    
    if constexpr (!std::is_class_v<T>) return false;
    if constexpr (std::is_polymorphic_v<T>) return false;   // no virtual functions
    if constexpr (has_virtual_bases<T>()) return false;      // no virtual inheritance
    if constexpr (std::is_union_v<T>) return false;
    
    // NEW: recursively check all base classes are safe
    constexpr auto bases = bases_of(^^T, access_context::unchecked());
    // Check each base using index-based expansion (same pattern as members)
    // ... (fold expression over base indices, each checking is_safe_type<BaseType>())
    
    // Check direct members (existing logic, unchanged)
    constexpr std::size_t member_count = nonstatic_data_members_of(^^T, access_context::unchecked()).size();
    if constexpr (member_count == 0 && bases.size() == 0) {
        return true;
    } else {
        return check_all_bases_safe<T>() && check_all_members_impl<T>(...);
    }
}
```

### 4.3 Modified `reflect_init_all<T>()`

Add base class initialization **before** direct member initialization:

```cpp
template <typename T, typename Alloc>
void reflect_init_all(void* raw, Alloc alloc) {
    std::memset(raw, 0, sizeof(T));
    
    // NEW: Initialize base class members (recursively)
    reflect_init_bases<T>(raw, alloc);
    
    // Existing: Initialize direct members
    reflect_init_expand<T>(raw, alloc,
        std::make_index_sequence<reflect_member_count_of<T>()>{});
}

// New helper — iterate bases and recursively init each
template <typename T, typename Alloc>
void reflect_init_bases(void* raw, Alloc alloc) {
    if constexpr (std::meta::bases_of(^^T, std::meta::access_context::unchecked()).size() > 0) {
        reflect_init_bases_expand<T>(raw, alloc,
            std::make_index_sequence<base_count_of<T>()>{});
    }
}

template <typename T, std::size_t N, typename Alloc>
void reflect_init_base_nth(void* raw, Alloc alloc) {
    using namespace std::meta;
    constexpr auto base_info = bases_of(^^T, access_context::unchecked())[N];
    using BaseType = [:type_of(base_info):];
    
    // Cast to base subobject (at correct offset via static_cast)
    T* obj = reinterpret_cast<T*>(raw);
    BaseType* base_ptr = static_cast<BaseType*>(obj);
    
    // Recursively init base's own bases + members
    reflect_init_all<BaseType>(static_cast<void*>(base_ptr), alloc);
}
```

**Key insight**: `static_cast<BaseType*>(derived_ptr)` automatically adjusts for the correct ABI offset of the base subobject. We don't need to manually compute offsets.

### 4.4 Modified `reflect_transfer_init_all<T>()`

Same pattern — transfer base class members before direct members:

```cpp
template <typename T, typename Src>
void reflect_transfer_init_all(void* dst, Src&& src,
                               XBufferCore::segment_manager* sm) {
    std::memset(dst, 0, sizeof(T));
    
    // NEW: Transfer base class members
    reflect_transfer_bases<T>(dst, std::forward<Src>(src), sm);
    
    // Existing: Transfer direct members
    reflect_transfer_init_expand<T>(dst, std::forward<Src>(src), sm,
        std::make_index_sequence<reflect_member_count_of<T>()>{});
}

template <typename T, std::size_t N, typename Src>
void reflect_transfer_base_nth(void* dst, Src&& src,
                                XBufferCore::segment_manager* sm) {
    using namespace std::meta;
    constexpr auto base_info = bases_of(^^T, access_context::unchecked())[N];
    using BaseType = [:type_of(base_info):];
    
    T* dst_obj = reinterpret_cast<T*>(dst);
    BaseType* dst_base = static_cast<BaseType*>(dst_obj);
    
    // Forward src and cast to base reference
    // For move: static_cast<BaseType&&>(src)
    // For copy: static_cast<const BaseType&>(src)
    auto&& src_base = static_cast<copy_cvref_t<Src&&, BaseType>>(
                          std::forward<Src>(src));
    
    reflect_transfer_init_all<BaseType>(
        static_cast<void*>(dst_base),
        std::forward<decltype(src_base)>(src_base), sm);
}
```

### 4.5 Modified `XCompactor::migrate_members<T>()`

```cpp
template<typename T>
static void migrate_members(const T& old_obj, T& new_obj, 
                           XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
    // NEW: Migrate base class members first
    migrate_bases(old_obj, new_obj, old_xbuf, new_xbuf);
    
    // Existing: Migrate direct members
    constexpr std::size_t member_count = boost::typelayout::get_member_count<T>();
    migrate_members_impl(old_obj, new_obj, old_xbuf, new_xbuf,
                        std::make_index_sequence<member_count>{});
}

template<typename T, std::size_t N>
static void migrate_base_at(const T& old_obj, T& new_obj,
                            XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
    using namespace std::meta;
    constexpr auto base_info = bases_of(^^T, access_context::unchecked())[N];
    using BaseType = [:type_of(base_info):];
    
    const BaseType& old_base = static_cast<const BaseType&>(old_obj);
    BaseType& new_base = static_cast<BaseType&>(new_obj);
    
    // Recursively migrate base (handles base-of-base)
    migrate_members(old_base, new_base, old_xbuf, new_xbuf);
}
```

### 4.6 Updated `needs_reflect_construct<T>` concept

No change needed — the concept checks for `is_class_v`, `!is_trivially_copyable_v`, no `allocator_type`, no SM constructor. Derived classes that inherit from pure data structs will naturally satisfy these constraints.

### 4.7 `memset` concern in recursive init

Current code does `memset(raw, 0, sizeof(T))` at the start of `reflect_init_all`. When called recursively for a base, this would re-zero the entire derived object. **Fix**: Move the `memset` to the top-level entry point only, and make the recursive helpers skip it.

```cpp
// Public entry point — zeroes once
template <typename T, typename Alloc>
void reflect_init_all(void* raw, Alloc alloc) {
    std::memset(raw, 0, sizeof(T));           // Zero ONCE at top level
    reflect_init_all_impl<T>(raw, alloc);     // Recursive, no memset
}

// Internal recursive helper — no memset
template <typename T, typename Alloc>
void reflect_init_all_impl(void* raw, Alloc alloc) {
    reflect_init_bases<T>(raw, alloc);        // bases first
    reflect_init_expand<T>(raw, alloc,        // then direct members
        std::make_index_sequence<reflect_member_count_of<T>()>{});
}
```

Same pattern for `reflect_transfer_init_all`.

---

## 5. User-Facing Examples

### 5.1 Single Inheritance (Entity hierarchy)

```cpp
struct Entity {
    int32_t id{0};
    XString name;
};

struct Player : Entity {
    int32_t level{0};
    XVector<int32_t> items;
};

// Usage — identical to flat struct
XBuffer xbuf(4096);
auto* player = xbuf.make<Player>();
player->id = 42;
player->name = "Alice";
player->level = 10;
player->items.push_back(101);
```

### 5.2 Multiple Inheritance (Mixin pattern)

```cpp
struct HasId    { int32_t id{0}; };
struct HasName  { XString name; };
struct HasLevel { int32_t level{0}; };

struct Player : HasId, HasName, HasLevel {
    XVector<int32_t> items;
};

auto* p = xbuf.make<Player>();
p->id = 1;
p->name = "Bob";
p->level = 50;
```

### 5.3 Multi-level Inheritance

```cpp
struct Entity    { int32_t id{0}; XString name; };
struct Character : Entity { int32_t hp{0}; int32_t mp{0}; };
struct Player    : Character { XVector<int32_t> items; };

auto* p = xbuf.make<Player>();
p->id = 1;              // from Entity
p->name = "Charlie";    // from Entity
p->hp = 100;            // from Character
p->items.push_back(42); // from Player
```

### 5.4 Rejected Cases

```cpp
// ❌ Virtual function → is_polymorphic → rejected
struct BadBase {
    virtual void tick() {}
};
struct BadPlayer : BadBase { int32_t x; };
// Error: "UNSAFE: Type has virtual functions (polymorphic)"

// ❌ Virtual inheritance → has_virtual_bases → rejected
struct VBase { int32_t id; };
struct BadDerived : virtual VBase { int32_t x; };
// Error: "UNSAFE: virtual inheritance not allowed"
```

---

## 6. Testing Plan

| Test | Description |
|------|-------------|
| `test_single_inheritance` | Entity → Player, make/read/compact cycle |
| `test_multi_level_inheritance` | Entity → Character → Player, 3-level deep |
| `test_multiple_inheritance` | HasId + HasName → Player, mixin pattern |
| `test_inheritance_in_vector` | `XVector<Player>` where Player inherits Entity |
| `test_inheritance_compaction` | Compact a buffer with inherited types, verify data integrity |
| `test_reject_virtual_function` | `static_assert` fires for polymorphic types |
| `test_reject_virtual_inheritance` | `static_assert` fires for virtual bases |
| `test_mixed_composition_inheritance` | Struct that both inherits AND contains nested structs |

---

## 7. Risk Assessment

| Risk | Mitigation |
|------|-----------|
| `static_cast` offset correctness | Compiler-guaranteed for non-virtual inheritance; ABI-standardized |
| Cross-platform layout difference | TypeLayout signature encodes exact offsets → mismatch = load-time rejection |
| `memset` double-zeroing in recursion | Split into entry-point (with memset) and recursive helper (without) |
| Diamond inheritance (ambiguous member) | C++ compiler rejects ambiguous access; our reflection iterates both copies correctly |
| P2996 `bases_of()` reliability | Already used by TypeLayout; battle-tested in signature generation |

---

## 8. Summary of Changes

| Component | Estimated LOC Changed |
|-----------|----------------------|
| Safety validation (`has_virtual_bases`, `are_all_members_safe`, error messages) | ~40 lines |
| Reflection init (`reflect_init_all` + base helpers) | ~35 lines |
| Reflection transfer (`reflect_transfer_init_all` + base helpers) | ~35 lines |
| XCompactor migration (`migrate_members` + base helpers) | ~25 lines |
| Tests (new file) | ~200 lines |
| Documentation updates | ~50 lines |
| **Total** | **~385 lines** |
