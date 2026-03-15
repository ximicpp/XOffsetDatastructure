# Zero-Boilerplate Type Definitions

> C++26 Reflection-Powered Automatic Allocator Construction

## Overview

XOffsetDatastructure v0.10+ uses C++26 reflection (P2996) to **completely eliminate** the boilerplate traditionally required for user-defined types. Users write plain structs — no constructors, no macros, no typedefs.

## Before vs. After

### Before (v0.9 and earlier)

Every type that contained XString, XVector, XMap, or XSet required ~15 lines of allocator plumbing:

```cpp
class Player {
public:
    using allocator_type = XAllocator;

    template <typename Allocator>
        requires (!std::is_same_v<std::decay_t<Allocator>, std::allocator_arg_t>)
    Player(Allocator allocator)
        : name(allocator), items(allocator) {}

    template <typename Allocator>
    Player(Player&& other, Allocator allocator)
        : id(other.id), level(other.level)
        , name(std::move(other.name), allocator)
        , items(std::move(other.items), allocator) {}

    int32_t id{0};
    int32_t level{0};
    XString name;
    XVector<int32_t> items;
};
```

### After (v0.10+)

Just a plain struct:

```cpp
struct Player {
    int32_t id{0};
    int32_t level{0};
    XString name;
    XVector<int32_t> items;
};
```

**Usage is identical:**

```cpp
XBuffer xbuf(4096);
auto* p = xbuf.make<Player>();
p->name = "Alice";
p->items.push_back(100);
```

## Supported Scenarios

### Layer 1: Root Objects

Pure aggregates work directly with `XBuffer::make<T>()`:

```cpp
struct GameState {
    int32_t round;
    XString title;
    XVector<int32_t> scores;
    XMap<int32_t, int32_t> score_map;
    XSet<int32_t> active_set;
};

XBuffer xbuf(8192);
auto* g = xbuf.make<GameState>();  // ✅ Just works
g->title = "Championship";
g->scores.push_back(100);
g->score_map[1] = 500;
g->active_set.insert(1);
```

### Layer 2: Container Elements

Pure aggregates work inside `XVector`, including during reallocation:

```cpp
struct Item {
    int32_t id;
    int32_t quantity;
    XString name;
};

struct Inventory {
    int32_t owner_id;
    XVector<Item> items;     // ← pure aggregate as element
};

XBuffer xbuf(8192);
auto* inv = xbuf.make<Inventory>();
inv->items.emplace_back();           // ✅ reflection constructs Item
inv->items[0].id = 1;
inv->items[0].name = "Sword";

// Reallocation is safe — allocator auto-injected per member
for (int i = 0; i < 100; i++) {
    inv->items.emplace_back();       // ✅ move during realloc works
}
```

### Deep Nesting

Supports arbitrary nesting depth (tested up to 4 levels):

```cpp
struct Tag     { int32_t id; XString label; };
struct Skill   { int32_t skill_id; XString name; XVector<Tag> tags; };
struct Character { int32_t id; XString name; XVector<Skill> skills; };
struct Guild   { int32_t guild_id; XString name; XVector<Character> members; };
struct World   { int32_t world_id; XString name; XVector<Guild> guilds; };

XBuffer xbuf(256 * 1024);
auto* w = xbuf.make<World>();
w->guilds.emplace_back();
w->guilds[0].members.emplace_back();
w->guilds[0].members[0].skills.emplace_back();
w->guilds[0].members[0].skills[0].tags.emplace_back();
w->guilds[0].members[0].skills[0].tags[0].label = "offensive";
// ✅ All levels initialized correctly via reflection
```

### Persistence

Save/load works seamlessly:

```cpp
auto saved = xbuf.save();
auto loaded = XBuffer::load(saved);
auto& w = loaded.root<World>();
assert(w.guilds[0].members[0].skills[0].tags[0].label == "offensive");
```

### Memory Compaction

`XCompactor::compact<T>()` handles pure aggregates automatically:

```cpp
XBuffer compacted = XCompactor::compact<World>(xbuf);
auto& w = compacted.root<World>();
// All data preserved, memory optimized
```

### Backward Compatibility

Types with explicit allocator constructors still work:

```cpp
// Legacy style — still fully supported
struct LegacyType {
    template <typename Allocator>
    LegacyType(Allocator alloc) : name(alloc) {}
    XString name;
};
```

The system auto-detects: if `T(segment_manager*)` is valid, the traditional path is used; otherwise, reflection kicks in.

## How It Works

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│ User Code                                                   │
│   struct Item { int32_t id; XString name; };                │
│   xbuf.make<Inventory>()                                    │
│   inv->items.emplace_back()                                 │
└────────────────────┬────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────┐
│ Dispatch Layer (compile-time)                               │
│                                                             │
│   has_segment_manager_ctor<T>?                              │
│     YES → Traditional path: T(segment_manager*)             │
│     NO  → needs_reflect_construct<T>?                       │
│             YES → Reflection path                           │
│             NO  → Standard placement new                    │
└────────────────────┬────────────────────────────────────────┘
                     │ (Reflection path)
┌────────────────────▼────────────────────────────────────────┐
│ Reflection Engine                                           │
│                                                             │
│   For each member via std::meta::nonstatic_data_members_of: │
│     if has allocator_type → construct_at(&member, alloc)    │
│     else                  → construct_at(&member)           │
└─────────────────────────────────────────────────────────────┘
```

### Key Components

| Component | Role |
|---|---|
| `needs_reflect_construct<T>` | Concept: detects pure aggregates needing reflection |
| `x_reflect_scoped_alloc<T>` | Custom allocator that overrides `construct()` |
| `reflect_init_all<T>` | Initializes all members on raw memory (default construction) |
| `reflect_transfer_init_all<T>` | Moves/copies all members with allocator injection (reallocation) |
| `ReflectRoot<T>` | Wrapper for root object construction in managed memory |

### `x_reflect_scoped_alloc::construct()` — The Core Interceptor

This is the heart of Layer 2. It inherits from `scoped_allocator_adaptor` and overrides `construct()`:

```cpp
template <typename U>
void construct(U* p) {
    if constexpr (needs_reflect_construct<U>) {
        // Pure aggregate: use reflection to init each member
        reflect_init_all<U>(p, segment_manager);
    } else {
        // Standard: delegate to uses_allocator protocol
        Base::construct(p);
    }
}

template <typename U, typename Arg>
void construct(U* p, Arg&& arg) {
    if constexpr (needs_reflect_construct<U> && is_same_v<decay_t<Arg>, U>) {
        // Move/copy: per-member transfer with allocator injection
        reflect_transfer_init_all<U>(p, forward<Arg>(arg), segment_manager);
    } else {
        Base::construct(p, forward<Arg>(arg));
    }
}
```

### Per-Member Dispatch

For each member `M` of type `T`, reflection applies:

| Member Type | Action |
|---|---|
| POD (`int32_t`, `float`, etc.) | `construct_at(&member)` — value-initialization (zero) |
| `XString` | `construct_at(&member, segment_manager)` — allocator-aware init |
| `XVector<T>` | `construct_at(&member, segment_manager)` — allocator-aware init |
| `XMap<K,V>` | `construct_at(&member, segment_manager)` — allocator-aware init |
| `XSet<T>` | `construct_at(&member, segment_manager)` — allocator-aware init |

During move (reallocation), each member uses:

| Member Type | Move Action |
|---|---|
| POD | `construct_at(&dst.m, move(src.m))` |
| Container | `construct_at(&dst.m, move(src.m), segment_manager)` |

## Test Coverage

23 tests total, including:

| Test | Scenarios |
|---|---|
| `test_zero_boilerplate` | Root objects: POD, XString+XVector, XMap+XSet, XHandle, grow(), legacy compat |
| `test_zero_boilerplate_vector` | Vector elements: simple/complex aggregates, reallocation, persistence, clear/reuse, grow |
| `test_complex_nesting` | 4-level nesting, 50×5 realloc stress, compaction, container-only/POD-only structs, 14 fields |

## Limitations

1. **Requires C++26 P2996 reflection** — no fallback for standard compilers
2. **No multi-arg `emplace_back`** — pure aggregates have no constructors, so use `emplace_back()` + field assignment:
   ```cpp
   // ❌ Not available for pure aggregates
   items.emplace_back(1, 10, "Sword");

   // ✅ Correct pattern
   items.emplace_back();
   items.back().id = 1;
   items.back().quantity = 10;
   items.back().name = "Sword";
   ```
3. **`alignas` on structs** — if you need specific alignment, add `alignas(8)` to the struct (not required in most cases)
