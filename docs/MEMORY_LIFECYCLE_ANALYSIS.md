# XOffsetDatastructure — Memory Lifecycle Analysis

> This document traces every memory operation in `examples/` line by line,
> mapping each to the formal model (C1, C2, P1–P3, Domain A, Domain S).
>
> Companion to: `docs/CORE_FORMAL_MODEL.md`

---

## File 1: player.hpp — Data Structure Definition

### F1.1 `alignas(8)` and Buffer Layout

```cpp
class alignas(8) Player {   // line 9
    int32_t id{0};           // offset 0,  size 4, align 4
    int32_t level{0};        // offset 4,  size 4, align 4
    XString name;            // offset 8,  size 32, align 8
    XVector<int32_t> items;  // offset 40, size 32, align 8
};                           // total: 72 bytes, align 8
```

**Layout analysis:**

```
Byte offset  Field           Size   Align  Notes
───────────  ──────────────  ─────  ─────  ────────────────────────
 0           id (int32_t)    4      4      S₀ primitive
 4           level (int32_t) 4      4      S₀ primitive
 8           name (XString)  32     8      No padding needed: 4+4=8, already 8-aligned
40           items (XVector)  32     8      8+32=40, already 8-aligned
72           <end>                          sizeof(Player) = 72
```

**`alignas(8)` effect**: Player's natural alignment is already 8 (driven by XString
and XVector, which are both 8-aligned). The explicit `alignas(8)` is therefore
**redundant** but serves as documentation — it makes the alignment contract visible
to readers and protects against future member reordering.

**When placed in a segment**: The segment manager's allocator guarantees that the
returned address satisfies the type's alignment requirement. For `alignas(8)` on a
64-bit platform (P1: buffer alignment ≥ 8), every allocation is naturally 8-aligned.
The `alignas` specifier is encoded in the TypeLayout signature as `a:8`, which is
part of C1 (layout determinism) verification.

### F1.2 Template Allocator Constructor Pattern

```cpp
template <typename Allocator>
Player(Allocator allocator) : name(allocator), items(allocator) {}  // line 13
```

**Mechanism — allocator propagation chain:**

1. **Caller**: `xbuf.make<Player>("Hero")` invokes `construct<Player>("Hero")(segment_manager)`.
2. **Boost.Interprocess** calls `Player(segment_manager)`. The segment manager pointer
   is implicitly convertible to any `allocator<T, segment_manager>` via Boost's
   allocator constructor.
3. **Template deduction**: `Allocator` is deduced as the segment manager type
   (`XBuffer::segment_manager*`).
4. **Member initialization**:
   - `name(allocator)` → `XString(allocator)`:
     - XString is `basic_string<char, ..., allocator<char, SM>>`.
     - Boost.Container's string constructor accepts the segment manager and internally
       **rebinds** it to `allocator<char, SM>` via `allocator_traits::rebind_alloc`.
     - The string is default-constructed (empty, no char buffer allocated yet).
   - `items(allocator)` → `XVector<int32_t>(allocator)`:
     - Similar rebind: segment manager → `allocator<int32_t, SM>`.
     - The vector is default-constructed (size=0, capacity=0, no buffer allocated).

5. **Scalar members** `id{0}` and `level{0}` use in-class default initializers.
   They are written directly into the segment memory (placement new).

**Key insight**: The allocator is **not stored by value** in the Player object.
It is propagated to each container member, which stores it internally as part of
its control block. The allocator's core state is a single `offset_ptr<segment_manager>`
(8 bytes), which is part of each XString/XVector's 32-byte footprint.

**Formal model mapping**:
- The allocator carries a reference to the segment manager → all allocations go
  through the segment manager → buffer self-containment invariant (C2 prerequisite)
- The allocator uses `offset_ptr` internally → C2 (referential integrity)

### F1.3 Full Constructor — Initialization Order

```cpp
template <typename Allocator>
Player(Allocator allocator, int id_val, int level_val, const char* name_val)
    : id(id_val)                    // (1) scalar write at offset 0
    , level(level_val)              // (2) scalar write at offset 4
    , name(name_val, allocator)     // (3) XString: allocates char buffer IN SEGMENT
    , items(allocator)              // (4) XVector: empty, no allocation
{}
```

**Initialization order** follows **member declaration order** (C++ standard §15.6.2),
NOT the order in the initializer list. Since the list matches declaration order here,
the actual sequence is:

1. `id = id_val` → 4 bytes written at `this + 0` (trivial, in-segment)
2. `level = level_val` → 4 bytes written at `this + 4` (trivial, in-segment)
3. `name(name_val, allocator)` → **critical**: XString constructor:
   - Computes `strlen(name_val)` (name_val is a stack/heap `const char*`)
   - Allocates `strlen + 1` bytes from the segment's free-list via the allocator
   - Copies the characters from the external `name_val` into the segment
   - Sets internal offset_ptr to the allocated char buffer
   - Sets internal size/capacity fields
   - **All data now resides in the segment** (C2: no external references)
4. `items(allocator)` → XVector default-constructed, no allocation

**Allocation timeline** (within a single `construct<Player>` call):

```
Segment free-list state:
  Before:  [...free...]
  After construct<Player>("Hero"):
    - 72 bytes allocated for Player object (by segment_manager::construct)
    - If name is non-empty: strlen+1 bytes allocated for char buffer
    - Total: 72 + (strlen + 1 + allocator overhead) bytes consumed
```

### F1.4 TypeLayout Signature Verification

```cpp
static_assert(boost::typelayout::get_definition_signature<Player>() ==
             "[64-le]record[s:72,a:8]{"
             "@0[id]:i32[s:4,a:4],"
             "@4[level]:i32[s:4,a:4],"
             "@8[name]:string[s:32,a:8],"
             "@40[items]:vector[s:32,a:8]<i32[s:4,a:4]>}",
              "Type signature mismatch for Player - "
              "Binary layout changed! This breaks serialization compatibility.");
```

**Signature breakdown — each token maps to a formal model element:**

| Token | Meaning | Model mapping |
|-------|---------|---------------|
| `[64-le]` | 64-bit, little-endian | Domain A: TargetArch = Arch64LE |
| `record` | Composite type (class/struct) | S_composite |
| `[s:72,a:8]` | sizeof=72, alignof=8 | C1: layout determinism |
| `@0[id]:i32[s:4,a:4]` | Field `id` at byte offset 0, type int32_t | C1: exact offset encoded |
| `@4[level]:i32[s:4,a:4]` | Field `level` at byte offset 4 | C1: no padding between int32_t fields |
| `@8[name]:string[s:32,a:8]` | Field `name` at byte offset 8, XString | C1: XString opaque (32 bytes, 8-align) |
| `@40[items]:vector[s:32,a:8]<i32[s:4,a:4]>` | Field `items` at offset 40, XVector<int32_t> | C1: container + element type encoded |

**Compile-time verification flow:**

1. `get_definition_signature<Player>()` is `consteval` — evaluated at compile time
2. It uses C++26 reflection (`std::meta::nonstatic_data_members_of`) to enumerate fields
3. For each field: name, offset, type signature are concatenated
4. For opaque types (XString, XVector): uses TYPELAYOUT_OPAQUE_* registered sizes
5. The result is compared to the expected string literal
6. If mismatch → `static_assert` fails → **compile error prevents binary incompatibility**

**This is C1's final defense**: even if Domain A passes (same 64-bit LE platform),
the signature catches ABI differences (e.g., different compiler padding strategy).

### F1.5 XVector<int32_t> — Safe Type Recursive Check

When `validate_xbuffer_type<Player>()` is called (inside `make<Player>`), the
type safety check follows this path:

```
is_safe_type<Player>()
  → is_safe_leaf<Player>? NO (not in whitelist)
  → is_enum<Player>? NO
  → is_class<Player>? YES
  → are_all_members_safe<Player>()
    → is_polymorphic<Player>? NO ✓
    → has_bases<Player>? NO ✓
    → is_union<Player>? NO ✓
    → for each member:
      [0] is_safe_type<int32_t>() → is_safe_leaf<int32_t>? YES ✓  (S₀)
      [1] is_safe_type<int32_t>() → is_safe_leaf<int32_t>? YES ✓  (S₀)
      [2] is_safe_type<XString>() → is_safe_leaf<XString>? YES ✓  (S_string)
      [3] is_safe_type<XVector<int32_t>>()
        → is_safe_leaf<XVector<int32_t>>? YES (S_container)
        → has value_type? YES → is_safe_type<int32_t>()? YES ✓ (S₀)
    → all members safe ✓
  → Player ∈ S ✓
```

**Well-foundedness**: The recursion depth is bounded:
- Player → {int32_t, int32_t, XString, XVector<int32_t>}
- XVector<int32_t> → int32_t (leaf, S₀)
- Maximum depth = 2 (Player → XVector → int32_t)

---

## File 2: game_data.hpp — Nested Composite Types

### F2.1 Item Layout — Padding Analysis

```cpp
class alignas(8) Item {
    int32_t item_id{0};      // offset 0,  size 4
    int32_t item_type{0};    // offset 4,  size 4
    int32_t quantity{0};     // offset 8,  size 4
    // 4 bytes PADDING        // offset 12, size 4  ← WHY?
    XString name;            // offset 16, size 32
};                           // total: 48 bytes
```

**Why `@16[name]` and not `@12[name]`?**

XString has `alignof(XString) = 8`. The three int32_t fields consume 12 bytes
(offsets 0, 4, 8). The next available offset is 12, but 12 is NOT 8-aligned
(12 % 8 = 4). The compiler inserts **4 bytes of padding** at offset 12 to align
`name` to offset 16 (16 % 8 = 0).

```
Byte map:
 0  1  2  3    4  5  6  7    8  9 10 11   12 13 14 15   16 ... 47
[  item_id  ] [item_type  ] [ quantity ] [ PADDING    ] [   name (XString)   ]
```

This padding is captured by the TypeLayout signature (`@16[name]` not `@12[name]`),
which is **exactly why C1 needs the TypeLayout verifier** — architecture alone
doesn't determine padding; the actual compiler's struct layout rules do.

### F2.2 GameData Layout — Offset Chain

```cpp
class alignas(8) GameData {
    int32_t player_id{0};                    // @0   size 4
    int32_t level{0};                        // @4   size 4
    float health{0.0f};                      // @8   size 4
    // 4 bytes PADDING                        // @12  size 4
    XString player_name;                     // @16  size 32
    XVector<Item> items;                     // @48  size 32
    XSet<int32_t> achievements;              // @80  size 32
    XMap<XString, int32_t> quest_progress;   // @112 size 32
};                                           // total: 144 bytes
```

**Offset derivation step by step:**

| Field | Natural offset | Alignment req | Actual offset | Padding |
|-------|---------------|---------------|---------------|---------|
| player_id | 0 | 4 | 0 | 0 |
| level | 4 | 4 | 4 | 0 |
| health | 8 | 4 | 8 | 0 |
| player_name | 12 | 8 | **16** | **4** |
| items | 48 (16+32) | 8 | 48 | 0 |
| achievements | 80 (48+32) | 8 | 80 | 0 |
| quest_progress | 112 (80+32) | 8 | 112 | 0 |
| <end> | 144 (112+32) | — | 144 | 0 |

**Total padding**: 4 bytes (between `health` and `player_name`).
**sizeof(GameData)** = 144 = 3×4 + 4(pad) + 32 + 32 + 32 + 32.

All container fields are 32-byte, 8-aligned → once the first container is
8-aligned, all subsequent containers are naturally aligned with zero padding.

### F2.3 Nested Container — Recursive Type Safety Check

`XVector<Item>` triggers a deeper recursion than `XVector<int32_t>`:

```
is_safe_type<GameData>()
  → are_all_members_safe<GameData>()
    [0] is_safe_type<int32_t>() → S₀ ✓
    [1] is_safe_type<int32_t>() → S₀ ✓
    [2] is_safe_type<float>() → S₀ ✓
    [3] is_safe_type<XString>() → S_string ✓
    [4] is_safe_type<XVector<Item>>()
      → is_safe_leaf<XVector<Item>>? YES
      → has value_type → is_safe_type<Item>()
        → is_safe_leaf<Item>? NO
        → is_class<Item>? YES
        → are_all_members_safe<Item>()
          [0] is_safe_type<int32_t>() → S₀ ✓
          [1] is_safe_type<int32_t>() → S₀ ✓
          [2] is_safe_type<int32_t>() → S₀ ✓
          [3] is_safe_type<XString>() → S_string ✓
        → Item ∈ S ✓
      → XVector<Item> ∈ S ✓
    [5] is_safe_type<XSet<int32_t>>()
      → is_safe_leaf? YES → value_type → is_safe_type<int32_t>() → S₀ ✓
    [6] is_safe_type<XMap<XString, int32_t>>()
      → is_safe_leaf? YES → key_type + mapped_type
      → is_safe_type<XString>() → S_string ✓
      → is_safe_type<int32_t>() → S₀ ✓
    → all members safe ✓
  → GameData ∈ S ✓
```

**Maximum recursion depth** = 3 (GameData → XVector<Item> → Item → int32_t/XString).

### F2.4 XMap<XString, int32_t> — Dual Type Check

The map check splits into key and value:

```cpp
// In is_safe_type(), when CleanT = XMap<XString, int32_t>:
if constexpr (is_safe_leaf<CleanT>::value) {       // YES (LEAF-4 registration)
    if constexpr (requires { typename CleanT::key_type; typename CleanT::mapped_type; }) {
        return is_safe_type<typename CleanT::key_type>()      // XString → S_string ✓
            && is_safe_type<typename CleanT::mapped_type>();   // int32_t → S₀ ✓
    }
}
```

This protects **C2**: if someone tried `XMap<std::string, int32_t>`, the key check
would fail because `std::string` is not in S (it contains heap raw pointers).

### F2.5 GameData TypeLayout Signature — Recursive Encoding

```
[64-le]record[s:144,a:8]{
  @0[player_id]:i32[s:4,a:4],
  @4[level]:i32[s:4,a:4],
  @8[health]:f32[s:4,a:4],
  @16[player_name]:string[s:32,a:8],
  @48[items]:vector[s:32,a:8]<record[s:48,a:8]{
      @0[item_id]:i32[s:4,a:4],
      @4[item_type]:i32[s:4,a:4],
      @8[quantity]:i32[s:4,a:4],
      @16[name]:string[s:32,a:8]}>,
  @80[achievements]:set[s:32,a:8]<i32[s:4,a:4]>,
  @112[quest_progress]:map[s:32,a:8]<string[s:32,a:8],i32[s:4,a:4]>}
```

**Key observations:**

1. **Nested record**: `XVector<Item>` encodes the **full Item signature** inside the
   angle brackets. This means C1 verification is **transitive** — if the outer
   GameData signature matches, the inner Item layout is also verified.

2. **Opaque containers**: `vector[s:32,a:8]`, `set[s:32,a:8]`, `map[s:32,a:8]` all
   show the same 32-byte, 8-aligned footprint. Their internal field arrangement
   (offset_ptr to backing store, size, capacity) is NOT encoded — this is the
   "outer shell" limitation covered by P3 (same Boost version).

3. **Map key+value encoding**: `map[...]<string[...],i32[...]>` encodes both the
   key type (XString) and value type (int32_t). A change to either would change
   the signature.

### F2.6 GameData Allocator Propagation

```cpp
GameData(Allocator allocator) 
    : player_name(allocator)        // (1) XString: rebind → allocator<char, SM>
    , items(allocator)              // (2) XVector<Item>: rebind → allocator<Item, SM>
    , achievements(allocator)       // (3) XSet<int32_t>: rebind → allocator<int32_t, SM>
    , quest_progress(allocator)     // (4) XMap<XString,int32_t>: rebind → allocator<pair<XString,int32_t>, SM>
{}
```

**Rebind chain for each container:**

| Container | Receives | Rebinds to | Stores internally |
|-----------|----------|------------|-------------------|
| `player_name` (XString) | SM* | `allocator<char, SM>` | offset_ptr<SM> |
| `items` (XVector<Item>) | SM* | `allocator<Item, SM>` | offset_ptr<SM> |
| `achievements` (XSet<int32_t>) | SM* | `allocator<int32_t, SM>` | offset_ptr<SM> |
| `quest_progress` (XMap<...>) | SM* | `allocator<pair<XString,int32_t>, SM>` | offset_ptr<SM> |

**All 4 containers store the same offset_ptr<SM>** — they just rebind the element
type for allocation sizing. The segment manager pointer is stored as `offset_ptr`,
ensuring C2 (referential integrity): the allocator reference itself uses relative
addressing.

**When a container allocates (e.g., `items.push_back(...)`):**
1. Container calls `allocator.allocate(n)` where n = number of elements needed
2. Allocator forwards to `segment_manager->allocate(n * sizeof(Item))`
3. Segment manager finds space in the free-list within the buffer
4. Returns an `offset_ptr<Item>` to the container
5. Container stores this offset_ptr as its backing store pointer
6. **All memory remains within the buffer** → self-containment invariant

---

## File 3: helloworld.cpp — Complete Lifecycle (Line-by-Line)

### Line 17: `XBufferExt xbuf(4096);` — Buffer Creation

**What happens (6 steps):**

```
User code                    Library internals
─────────────                ──────────────────
XBufferExt xbuf(4096)
  │
  ├─(1)─→ XBuffer(4096)     [XBufferExt inherits XBuffer's constructors]
  │          │
  │          ├─(2)─→ m_buffer(4096, char(0))
  │          │         std::vector<char> allocates 4096 bytes on HEAP
  │          │         Fills with zeros
  │          │
  │          ├─(3)─→ addr = m_buffer.data()
  │          │         Gets raw pointer to heap allocation
  │          │
  │          └─(4)─→ create_impl(addr, 4096)
  │                    Boost.IPC initializes segment manager:
  │                    ┌─────────────────────────────────────────┐
  │                    │ Segment Header (~128 bytes)              │
  │                    │  - segment_manager control block         │
  │                    │  - rbtree/seq free-list root (offset_ptr)│
  │                    │  - iset_index root (offset_ptr)          │
  │                    │  - total size, free size counters        │
  │                    ├─────────────────────────────────────────┤
  │                    │ Free block (~3968 bytes)                 │
  │                    │  - Single contiguous free node           │
  │                    │  - prev/next offset_ptr (free-list)      │
  │                    └─────────────────────────────────────────┘
  │
  └─(5)─→ xbuf is a stack object; m_buffer lives on heap
           xbuf itself: ~sizeof(vector<char>) + base_t state
```

**Segment manager overhead**: On a 4096-byte buffer, the segment header typically
consumes ~128 bytes (x_seq_fit: simpler header) or ~192 bytes (x_best_fit: red-black
tree overhead). The `XBuffer` typedef uses `x_seq_fit` → **~128 bytes overhead**.

**Available user space**: ~3968 bytes for objects. This is why `stats()` shows
~7% usage for an empty buffer.

**Model mapping**:
- `std::vector<char>` is **outside** the formal model — it's the hosting mechanism
- `create_impl` initializes all internal pointers as `offset_ptr` → C2 (referential integrity)
- Buffer is contiguous → self-containment invariant
- P1: `std::vector` guarantees contiguous aligned memory (typically 16-byte on 64-bit)

**⚠️ Usability issue U2.3**: The user passes `4096` but gets ~3968 usable bytes.
There's no API to estimate how much overhead the segment manager will consume, nor
a way to specify "I need 3000 bytes of usable space" and have the library calculate
the total. The user must guess-and-check.

---

### Line 21: `auto* player = xbuf.make<Player>("Hero");` — Named Object Construction

**What happens (8 steps):**

```cpp
// XBufferExt::make<Player> (line 882):
T* make(const char* name) {
    validate_xbuffer_type<T>();                                    // (1) compile-time
    return this->construct<T>(name)(this->get_segment_manager());  // (2)-(8)
}
```

**(1) `validate_xbuffer_type<Player>()`** — compile-time gate (zero runtime cost):

```
validate_xbuffer_type<Player>()
  → static_assert(detail::is_safe_type<Player>(), "...")
  → is_safe_type<Player>()
    → Player ∈ S_composite (see F1.5 for full trace)
  → PASS ✓
```

**(2)-(8) `construct<Player>("Hero")(segment_manager)`** — Boost.IPC two-phase construction:

```
Step 2: construct<Player>("Hero")
  → Returns a proxy object (named_object_creator)
  → The proxy stores: name = "Hero", type info, segment manager reference

Step 3: proxy(segment_manager)  — the () call triggers actual construction:
  │
  ├─(4)─→ iset_index lookup: is "Hero" already registered?
  │         If yes: throw already_exists_error
  │         If no: continue
  │
  ├─(5)─→ Allocate name storage in segment:
  │         "Hero" = 4 chars + 1 null = 5 bytes
  │         + iset_index node overhead (offset_ptr to object, name hash, etc.)
  │         Total: ~48 bytes for the index entry
  │
  ├─(6)─→ Allocate Player object in segment:
  │         sizeof(Player) = 72 bytes
  │         Alignment: 8 bytes (from alignas(8))
  │         Segment manager finds free block ≥ 72 bytes
  │         Splits free block: [72 used | rest free]
  │         Returns offset_ptr<Player> → converts to Player*
  │
  ├─(7)─→ Placement new: new(addr) Player(segment_manager)
  │         Calls Player(Allocator allocator) — see F1.2
  │         id = 0, level = 0 (default initializers)
  │         name = empty XString (allocator stored, no char buffer yet)
  │         items = empty XVector (allocator stored, no backing store yet)
  │
  └─(8)─→ Register in iset_index: "Hero" → offset_ptr<Player>
           Now findable via find<Player>("Hero")
```

**Return type**: `auto* player` is `Player*` — a **raw pointer into the segment**.

**⚠️ Usability issue U1.2**: `make()` returns a raw pointer. This pointer becomes
**dangling** after ANY of: `grow()`, `shrink_to_fit()`, `update_after_shrink()`,
`compact_automatic()`. There is no compile-time protection. The user must remember
to re-acquire via `find()` after any buffer mutation.

**⚠️ Correctness concern C1.1**: The returned pointer is a **raw `Player*`** (absolute
address), not an `offset_ptr`. This is safe ONLY as long as the user doesn't store
it across buffer mutations. The pointer is derived from `offset_ptr` internally but
converted to raw for user convenience. This is a design trade-off: ease of use (`player->id`)
vs safety (could use `offset_ptr<Player>` but then `player->id` becomes `player->id` through
operator-> overhead).

---

### Lines 22–23: `player->id = 1; player->level = 10;` — Scalar Assignment

```
player->id = 1;     // Write 4 bytes (value 0x01000000 LE) at player + 0
player->level = 10; // Write 4 bytes (value 0x0A000000 LE) at player + 4
```

**Memory effect**: Direct writes into the segment buffer via the raw pointer.
No allocation, no offset_ptr involved. These are S₀ primitives — C1 guarantees
their byte representation is deterministic.

**No usability issue**: This is the simplest case — it works exactly like normal C++.

---

### Line 24: `player->name = XString("Alice", xbuf.allocator<XString>());` — ⚠️ CRITICAL

This single line involves **5 distinct operations** across stack and segment:

```
player->name = XString("Alice", xbuf.allocator<XString>());
               ──────────────────────────────────────────── 
               │                                          │
               │  (A) xbuf.allocator<XString>()           │
               │  (B) XString("Alice", allocator)         │
               │                                          │
               ────────────────────────────────────────────
                                   │
                          (C) operator=(temporary)
                                   │
                          (D) temporary destruction
```

**(A) `xbuf.allocator<XString>()`** (line 888):

```cpp
allocator<T, XBuffer::segment_manager> allocator() {
    validate_xbuffer_type<T>();   // compile-time: XString ∈ S_string ✓
    return allocator<T, SM>(this->get_segment_manager());
}
```

Returns `allocator<XString, SM>`. Note: this allocator's type parameter is `XString`,
but XString internally will **rebind** it to `allocator<char, SM>`.

**⚠️ Usability issue U1.1**: The user writes `xbuf.allocator<XString>()` but
the allocator is immediately rebound to `allocator<char>` inside XString. The
template parameter `XString` is misleading — the user might think they need to
match the target type exactly, but any type would work (the segment manager
pointer is what matters). A simpler API would be `xbuf.allocator()` (no template
parameter needed).

**(B) `XString("Alice", allocator)`** — temporary on STACK:

```
Stack frame                          Segment buffer
────────────                         ──────────────
┌─────────────────────┐
│ XString temporary    │              
│  ┌─────────────────┐│   offset_ptr  ┌──────────┐
│  │ offset_ptr→chars │├─────────────→│"Alice\0" │ 6 bytes
│  │ size = 5        ││              └──────────┘
│  │ capacity = 5    ││              (allocated from free-list)
│  │ alloc(→SM)      ││
│  └─────────────────┘│
│ 32 bytes on STACK    │
└─────────────────────┘
```

**Key insight**: The XString control block (32 bytes) lives on the **stack**, but
the character data ("Alice\0", 6 bytes) is allocated **in the segment** via the
allocator. The `offset_ptr` inside this stack-resident XString points from the
stack into the segment.

**This is a cross-boundary offset_ptr**: `stored_value = segment_addr - stack_addr`.
This is a HUGE negative number (segment and stack are far apart in virtual memory).
It works correctly because `offset_ptr::get()` computes `this + stored_value`
regardless of the distance. But it would NOT survive a byte_copy (the stack-resident
XString is not part of the buffer). This is fine because the temporary is short-lived.

**(C) `player->name = <temporary>`** — move assignment:

```
Before:
  player->name: empty XString at offset 8 within Player (IN SEGMENT)
  temporary: "Alice" XString on STACK, char data IN SEGMENT

Move assignment:
  1. player->name steals the temporary's internal state
  2. offset_ptr recalculated: was (stack→segment), now (segment→segment)
     Actually: the char data address doesn't change; the offset_ptr
     is recalculated because `this` moves from stack to segment location
  3. temporary is left in moved-from state (empty/null)

After:
  player->name.offset_ptr → "Alice\0" (segment→segment, VALID for byte_copy)
  temporary.offset_ptr → null/sentinel
```

**Formal model significance**: After the move, `player->name` contains an `offset_ptr`
from one location in the segment to another location in the segment. This is a
**self-contained reference** — exactly what C2 requires. Before the move, the
cross-boundary offset_ptr was temporary and never persisted in the buffer.

**(D) Temporary destruction:**

The stack-resident XString temporary is destroyed at the end of the full expression
(`;`). Since it was moved-from, its destructor is a no-op (no char buffer to deallocate).
If the move had NOT happened (e.g., copy assignment), the temporary's destructor would
deallocate the char buffer from the segment.

**⚠️ Usability issue U2.1**: This entire line is verbose and non-obvious:

```cpp
// Current — 55 characters, requires understanding allocator model:
player->name = XString("Alice", xbuf.allocator<XString>());

// Ideal — 23 characters, like normal C++:
player->name = "Alice";
```

The library could support the shorter form by providing:
- `XString::operator=(const char*)` that uses the existing allocator stored in the XString
- Since `player->name` already has an allocator (from Player's constructor), this is feasible

---

### Lines 28–30: `player->items.push_back(101/102/103)` — Vector Growth

```cpp
player->items.push_back(101);  // line 28
player->items.push_back(102);  // line 29
player->items.push_back(103);  // line 30
```

**push_back(101) — first insertion (cold start):**

```
Before: items.size=0, items.capacity=0, no backing store allocated

push_back(101):
  1. capacity == size → need reallocation
  2. New capacity: growth_factor_custom(0) → initial allocation
     Boost.Container typically starts with capacity = 1 for first push_back
  3. Allocate: 1 * sizeof(int32_t) = 4 bytes from segment free-list
     (actual allocation may be larger due to allocator overhead/alignment)
  4. Store 101 at backing_store[0]
  5. Update: size=1, capacity=1
  6. offset_ptr points to the new backing store (segment→segment)
```

**push_back(102) — triggers 1.1x reallocation:**

```
Before: size=1, capacity=1

push_back(102):
  1. capacity == size → need reallocation
  2. New capacity: 1 * 1.1 = 1.1 → rounds up to 2
     (growth_factor_custom: 11/10 ratio)
  3. Allocate: 2 * sizeof(int32_t) = 8 bytes from free-list
  4. Copy old data: backing_store_new[0] = 101
  5. DEALLOCATE old backing store (4 bytes → returns to free-list)
  6. Store 102 at backing_store_new[1]
  7. Update: size=2, capacity=2, offset_ptr → new backing store
```

**push_back(103) — triggers another reallocation:**

```
Before: size=2, capacity=2

push_back(103):
  1. capacity == size → need reallocation
  2. New capacity: 2 * 1.1 = 2.2 → rounds up to 3
  3. Allocate: 3 * sizeof(int32_t) = 12 bytes
  4. Copy: [101, 102]
  5. DEALLOCATE old (8 bytes → free-list)
  6. Store 103 at [2]
  7. Update: size=3, capacity=3
```

**Free-list fragmentation after 3 push_backs:**

```
Segment state:
┌──────────┬─────────┬──────┬──────┬──────┬──────────────┐
│ Seg Hdr  │ iset +  │ FREE │ FREE │items │    FREE      │
│ ~128 B   │ Player  │ 4B   │ 8B   │ 12B  │   ~3700B     │
│          │ ~120 B  │(old) │(old) │(cur) │              │
└──────────┴─────────┴──────┴──────┴──────┴──────────────┘
```

Two small free blocks (4B and 8B) are created by deallocated old backing stores.
With x_seq_fit, these are linked into the free-list. They may or may not be
coalesced depending on adjacency.

**⚠️ Performance concern**: The 1.1x growth factor means many more reallocations
than std::vector's typical 2x. For 3 elements: 3 allocations (capacity 1→2→3).
With 2x growth: 2 allocations (capacity 1→2→4). The trade-off is memory efficiency
(1.1x wastes less) vs allocation count (more reallocations = more fragmentation).

---

### Lines 44: `auto data = xbuf.save_to_string();` — Serialization

```cpp
std::string save_to_string() {                           // line 905
    auto* buffer = this->get_buffer();                   // → &m_buffer
    return std::string(buffer->begin(), buffer->end());  // byte-for-byte copy
}
```

**What happens:**

```
1. get_buffer() returns &m_buffer (the std::vector<char>)
2. std::string constructor copies ALL 4096 bytes:
   data[0..4095] = m_buffer[0..4095]

   This includes:
   - Segment header (128 bytes) — all offset_ptrs
   - iset_index entries — "Hero" → offset_ptr<Player>
   - Player object (72 bytes) — id, level, name(offset_ptr→chars), items(offset_ptr→data)
   - XString char data ("Alice\0", 6 bytes)
   - XVector backing store ([101, 102, 103], 12 bytes)
   - Free-list nodes (offset_ptrs to next free block)
   - Unused space (zeros)
```

**Formal model**: This IS the `byte_copy(B) → B'` operation from §1.3. The
`std::string` constructor copies every byte at its original offset. The displacement
δ = (string buffer address) - (m_buffer address) is arbitrary.

**C1 relevance**: The copy includes padding bytes (e.g., the 4 bytes between
`quantity` and `name` in Item). These are unspecified but don't affect semantics.

**C2 relevance**: All offset_ptrs are copied with their stored values intact.
When the buffer is later loaded at a different address, Lemma C2.1 ensures
they still resolve correctly.

**⚠️ Usability issue U1.3**: `save_to_string()` copies the **entire buffer**
including unused space. For a 4096-byte buffer with only 300 bytes of data,
this copies 3796 bytes of zeros. Alternatives:
- Return `std::span<char>` or `std::string_view` (zero-copy, but caller must
  ensure buffer outlives the view)
- Add `save_to_string_compact()` that shrinks first then saves
- Add `save_to_vector()` that returns `std::vector<char>` by move

---

### Line 49: `XBufferExt loaded = XBufferExt::load_from_string(data);` — Deserialization

```cpp
static XBufferExt load_from_string(const std::string& data) {  // line 910
    std::vector<char> buffer(data.begin(), data.end());         // (1) copy into vector
    XBufferExt xbuf(buffer);                                    // (2) construct from vector
    return xbuf;                                                // (3) return (NRVO)
}
```

**(1) Copy string → vector**: Another full copy. Now we have: original m_buffer → string
→ vector. Two copies total for a round-trip. This is the cost of the `std::string` API.

**(2) `XBufferExt(buffer)`** — calls `XManagedMemory(std::vector<char>&)` (line 242):

```cpp
XManagedMemory(std::vector<char> &externalBuffer) 
    : m_buffer(std::move(externalBuffer))     // (2a) MOVE, not copy! Zero-cost.
{
    void *addr = m_buffer.data();
    size_type size = m_buffer.size();
    BOOST_ASSERT((alignment check));          // (2b) P1 enforcement
    if (!base_t::open_impl(addr, size))       // (2c) segment rediscovery
    {
        throw interprocess_exception("...");
    }
}
```

**(2a) Move**: The vector is moved, not copied. The `externalBuffer` is left empty.

**(2b) `BOOST_ASSERT`**: Verifies P1 (buffer alignment). `std::vector<char>::data()`
returns a pointer with implementation-defined alignment. On all major implementations,
small buffer optimization or allocator alignment guarantees ≥ 8-byte alignment.
If this assertion fails at runtime, it means the platform's vector allocator doesn't
provide sufficient alignment.

**(2c) `open_impl`**: The critical re-discovery step:

```
open_impl(addr, 4096):
  1. Cast addr to segment_manager*
  2. Read segment header:
     - Verify magic number / size consistency
     - Read free-list root offset_ptr → follow to first free block
     - Read iset_index root offset_ptr → follow to index tree root
  3. All offset_ptrs resolve correctly (Lemma C2.1):
     - The buffer is now at a new address (different from original)
     - But stored_values haven't changed
     - Each resolve(): new_this + stored_value = correct target in new buffer
  4. Segment manager is now operational at the new address
```

**This is the Corollary from §4.1 in action**: C1+C2+P1+P2+P3 → open_impl succeeds.

**(3) Return (NRVO)**: Named Return Value Optimization. The compiler constructs
`xbuf` directly in the caller's stack frame, avoiding a move of the XManagedMemory
(which would require swapping the vector and base_t state). With NRVO, zero copies.

---

### Line 50: `auto [loaded_player, found] = loaded.find_ex<Player>("Hero");`

```cpp
std::pair<T*, bool> find_ex(const char* name) {   // line 894
    auto result = this->find<T>(name);             // Boost.IPC find
    return {result.first, result.second};
}
```

**`this->find<Player>("Hero")`** — iset_index lookup:

```
1. Hash "Hero" → bucket index
2. Traverse iset_index tree (offset_ptr chain):
   root_offset_ptr → node → node → ... → match "Hero"
3. Node contains: offset_ptr<Player> → the Player object
4. Dereference: offset_ptr.get() → raw Player*
5. Return: {Player*, count} where count = 1 (named objects are unique)
```

**Return type**: Again a raw `Player*`. Same usability concern as `make()`.

**Note**: `find_ex` wraps `find` but changes the second element from `size_type`
(count) to `bool`. This is a usability improvement — users typically want "found
or not", not a count.

---

### Lines 74–78: push_back + pop_back — Fragmentation

```cpp
player->items.push_back(201);  // line 74: size 3→4, may trigger realloc (cap 3→4)
player->items.push_back(202);  // line 75: size 4→5, trigger realloc (cap 4→5)
player->items.push_back(203);  // line 76: size 5→6, trigger realloc (cap 5→6)
player->items.pop_back();      // line 77: size 6→5, NO deallocation
player->items.pop_back();      // line 78: size 5→4, NO deallocation
```

**pop_back behavior**: Only decrements `size`. The backing store remains allocated
at capacity 6 (24 bytes). The memory for elements [4] and [5] is "wasted" but
still owned by the vector. This is standard vector behavior.

**Fragmentation state**: The old backing stores from all reallocation cycles are
now free blocks in the segment's free-list. With 1.1x growth from cap 3 to 6,
there are additional small free blocks (12, 16, 20 bytes) scattered in the segment.

---

### Line 87: `XBuffer compacted = XBufferCompactor::compact_automatic<Player>(xbuf, "Hero");`

**What happens (8 steps):**

```cpp
static XBuffer compact_automatic(XBuffer& old_xbuf, const char* object_name) {
    validate_xbuffer_type<T>();                                          // (1)
    auto stats = XBufferVisualizer::get_memory_stats(old_xbuf);         // (2)
    std::size_t new_size = stats.used_size + (stats.used_size / 10);    // (3)
    if (new_size < 4096) new_size = 4096;
    
    XBuffer new_xbuf(new_size);                                         // (4)
    auto* old_obj = old_xbuf.find<T>(object_name).first;               // (5)
    if (!old_obj) return new_xbuf;
    
    auto* new_obj = new_xbuf.construct<T>(object_name)                 // (6)
                        (new_xbuf.get_segment_manager());
    migrate_members(*old_obj, *new_obj, old_xbuf, new_xbuf);           // (7)
    new_xbuf.shrink_to_fit();                                          // (8)
    return new_xbuf;
}
```

**(3) Size estimation**: `used_size + 10%` headroom. If the old buffer uses 304 bytes,
new_size = 334, but clamped to minimum 4096. This means compaction of small buffers
always creates a 4096-byte buffer — no space saving unless used_size > ~3700.

**⚠️ Usability issue**: The 4096 minimum means `compact_automatic` on a buffer with
300 bytes of data creates a 4096-byte buffer. The subsequent `shrink_to_fit()` (step 8)
reduces it, but the intermediate allocation is wasteful.

**(7) `migrate_members` — reflection-driven per-field migration:**

For Player with 4 fields:

```
migrate_members(old_player, new_player, old_xbuf, new_xbuf):
  template for (auto member : nonstatic_data_members_of(^Player)):
    │
    ├─ [0] id (int32_t):
    │    resolve_strategy → TrivialCopy
    │    new_player.id = old_player.id  (direct byte copy)
    │
    ├─ [1] level (int32_t):
    │    resolve_strategy → TrivialCopy
    │    new_player.level = old_player.level
    │
    ├─ [2] name (XString):
    │    resolve_strategy → AllocatorAware
    │    migrate_allocator_aware:
    │      new_player.name = XString(old_player.name.c_str(),
    │                                new_xbuf.get_segment_manager())
    │    → Allocates NEW char buffer in new_xbuf's segment
    │    → Copies "Alice" from old segment to new segment
    │    → new_player.name.offset_ptr → new segment (self-contained)
    │
    └─ [3] items (XVector<int32_t>):
         resolve_strategy → Container (SequentialContainer)
         migrate_container:
           for each element in old_player.items:
             new_player.items.push_back(element)
           → Each int32_t is trivially copied
           → new_player.items allocates its own backing store in new segment
           → 4 elements: [101, 102, 103, 201]
           → New backing store is contiguous, no fragmentation
```

**Result**: The new buffer contains a clean, defragmented copy of all data.
All offset_ptrs in the new buffer are self-contained (segment→segment).

**(8) `new_xbuf.shrink_to_fit()`**: Reduces the buffer to its used_size.

```cpp
void shrink_to_fit() {
    base_t::shrink_to_fit();              // Boost: compact free-list, update size
    m_buffer.resize(base_t::get_size());  // Shrink vector
    update_after_shrink();                // Re-create segment from resized vector
}

void update_after_shrink() {
    auto *pBuf = get_buffer();
    std::vector<char> new_buf(pBuf->data(), pBuf->data() + pBuf->size());
    XManagedMemory new_mem(new_buf);      // open_impl on copied buffer
    this->swap(new_mem);                  // swap in the new state
}   // old state destroyed here
```

**⚠️ Correctness concern**: `shrink_to_fit` calls `update_after_shrink` which
creates a **third copy** of the buffer data (copy into new_buf, then open_impl).
This is functionally correct but involves 2 extra copies of the entire buffer.

---

### End of main — Destructor Chain

When `main()` returns, stack unwinding destroys:

1. `compacted` (XBuffer) → `~XManagedMemory()`:
   - `priv_close()` → `destroy_impl()` (tells Boost the segment is gone)
   - `std::vector<char>().swap(m_buffer)` → frees the heap allocation
   - **No individual object destructors called** — the segment is bulk-freed

2. `loaded` (XBufferExt) → same destructor chain

3. `xbuf` (XBufferExt) → same destructor chain

**⚠️ Correctness concern C3.1**: Object destructors (`~Player`, `~XString`,
`~XVector`) are **NOT called** when the segment is destroyed. This is correct
for the current use case (all data is segment-internal), but would be a bug if
any object held external resources (file handles, GPU buffers, etc.). The formal
model's Domain S prevents such types from entering the buffer, but users may not
realize their destructors won't run.

---

## File 4: demo.cpp — 7 Demo Functions (Line-by-Line)

### Demo 1: demo_basic_usage (lines 53–115)

#### D1.1 `xbuf.make<GameData>("player_save")` — Complex Composite Construction

```cpp
XBufferExt xbuf(4096);
auto* game = xbuf.make<GameData>("player_save");  // line 63
```

GameData is larger and more complex than Player (144 bytes vs 72 bytes):

```
make<GameData>("player_save"):
  validate_xbuffer_type<GameData>() → see F2.3 (depth-3 recursion) ✓
  construct<GameData>("player_save")(segment_manager):
    ├─ iset_index: allocate name "player_save" (12 chars + overhead ≈ 56 bytes)
    ├─ Allocate GameData: 144 bytes, 8-aligned
    ├─ Placement new: GameData(segment_manager)
    │    player_id = 0, level = 0, health = 0.0f
    │    player_name = empty XString (allocator stored)
    │    items = empty XVector<Item> (allocator stored)
    │    achievements = empty XSet<int32_t> (allocator stored)
    │    quest_progress = empty XMap<XString,int32_t> (allocator stored)
    └─ Register: "player_save" → offset_ptr<GameData>
```

**Segment consumption**: ~128 (header) + 56 (index) + 144 (GameData) ≈ **328 bytes**.
Remaining: ~3768 bytes free.

**Comparison with helloworld**: Same pattern, same usability issues (U1.2: raw pointer).

#### D1.2 `game->items.emplace_back(allocator, ...)` — Nested Container Element Construction

```cpp
game->items.emplace_back(
    xbuf.allocator<Item>(),  // allocator for Item (rebound internally)
    i + 1,                   // item_id
    i % 3,                   // item_type
    (i + 1) * 10,            // quantity
    item_name.c_str()        // name (const char*)
);  // line 80-86
```

**This is fundamentally different from `push_back(101)` in helloworld:**

```
push_back(101):           Simple — copy a 4-byte int into segment
emplace_back(alloc,...):  Complex — construct an Item IN-PLACE inside the vector's
                          backing store, which is itself in the segment
```

**Step-by-step trace:**

```
emplace_back(allocator, 1, 0, 10, "Potion 1"):
  │
  ├─(1) Check capacity: items.size == items.capacity?
  │     If first item: allocate backing store for 1 Item (48 bytes) from segment
  │     If at capacity: 1.1x growth → reallocate larger backing store
  │
  ├─(2) In-place construction at items.data()[items.size]:
  │     new(&backing_store[n]) Item(allocator, 1, 0, 10, "Potion 1")
  │       ├─ item_id = 1       (4 bytes at item+0)
  │       ├─ item_type = 0     (4 bytes at item+4)
  │       ├─ quantity = 10     (4 bytes at item+8)
  │       ├─ [4 bytes padding] (at item+12)
  │       └─ name("Potion 1", allocator):
  │            Allocate 9 bytes ("Potion 1\0") from segment free-list
  │            offset_ptr: segment→segment (backing store → char data)
  │
  └─(3) items.size++
```

**Key difference from helloworld**: Each Item contains an XString, which means
**each `emplace_back` triggers TWO segment allocations**: one for the backing store
growth (if needed) and one for the XString char data. With 5 items:

```
Allocations:  5 × XString char data + multiple backing store reallocs
Growth:       cap 0→1→2→3→4→5 (5 reallocations with 1.1x growth!)
```

**⚠️ Performance concern**: 5 items trigger 5 vector reallocations (each with copy
of all existing Items + their offset_ptrs). The 1.1x growth factor is particularly
painful here because each Item is 48 bytes — reallocation copies are expensive.
A `reserve(5)` call would eliminate all reallocations.

**⚠️ Usability issue**: There's no `items.reserve(n)` example in the demos, and
users familiar with std::vector may not realize the amplified cost of 1.1x growth
in a segment-managed context.

#### D1.3 Item emplace_back — Allocator Propagation Deep Dive

The allocator passed to `emplace_back` is `xbuf.allocator<Item>()`. But this
allocator is NOT used to allocate the Item itself (the vector does that). It is
passed as the **first argument** to Item's constructor:

```cpp
Item(Allocator allocator, int item_id_val, ..., const char* name_val)
    : item_id(item_id_val)
    , ...
    , name(name_val, allocator)  // allocator propagated to XString
```

**Allocator journey**:
1. `xbuf.allocator<Item>()` → `allocator<Item, SM>(SM*)`
2. Passed to `emplace_back` → forwarded to Item constructor as first arg
3. Item constructor receives it as `Allocator allocator`
4. `name(name_val, allocator)` → XString rebinds to `allocator<char, SM>`
5. XString allocates char buffer from the segment

**⚠️ Usability issue**: The user must pass the allocator as the first argument
to `emplace_back`. This is non-standard — `std::vector::emplace_back` doesn't
take an allocator. New users will be confused by:

```cpp
// Standard std::vector:
items.emplace_back(1, 0, 10, "Potion 1");          // no allocator

// XVector in segment:
items.emplace_back(xbuf.allocator<Item>(), 1, 0, 10, "Potion 1");  // allocator first
```

#### D1.4 `game->achievements.insert(i)` — flat_set Insertion

```cpp
for (int ach_id : achievements) {
    game->achievements.insert(ach_id);  // line 94
}
```

`XSet<int32_t>` is `flat_set<int32_t, less<int32_t>, x_vector_impl<int32_t>>`.

**flat_set::insert mechanics:**

```
insert(1):
  ├─ Binary search in sorted backing store: O(log n)
  ├─ Not found → insert at sorted position
  ├─ If at capacity: 1.1x growth on backing store
  ├─ Shift elements right to make room: O(n) memmove
  └─ Store value at sorted position

Sequence: insert(1, 5, 10, 25, 50, 100)
  After insert(1):    [1]                    cap=1
  After insert(5):    [1, 5]                 cap=2  (growth from 1)
  After insert(10):   [1, 5, 10]             cap=3  (growth from 2)
  After insert(25):   [1, 5, 10, 25]         cap=4  (growth from 3)
  After insert(50):   [1, 5, 10, 25, 50]     cap=5  (growth from 4)
  After insert(100):  [1, 5, 10, 25, 50, 100] cap=6 (growth from 5)
```

**6 insertions = 5 reallocations** (same as push_back pattern). Since elements are
int32_t (trivially copyable), insertion is: grow + memmove + write.

**No usability issue**: `insert(i)` is natural flat_set API.

#### D1.5 `game->quest_progress[XString(...)] = 75` — flat_map with XString Key

```cpp
game->quest_progress[XString("Main Quest", xbuf.allocator<XString>())] = 75;  // line 100
```

This is the **most complex single operation** in the entire examples directory.

**Step-by-step:**

```
(A) XString("Main Quest", xbuf.allocator<XString>())
    → Temporary XString on STACK
    → "Main Quest\0" (11 bytes) allocated IN SEGMENT
    → Cross-boundary offset_ptr (stack→segment)

(B) quest_progress[temporary_xstring]
    → flat_map::operator[]:
      1. Binary search for key in sorted pair vector: O(log n)
      2. Key not found → insert new pair at sorted position
      3. Need pair<XString, int32_t> in segment:
         a. Backing store growth (if at capacity): 1.1x
         b. Move existing pairs right (shift)
         c. Construct new pair at insertion point:
            pair.first = move(temporary_xstring)
              → XString move: steal offset_ptr, recalculate
              → Now: pair.first in segment → "Main Quest\0" in segment
              → Both within segment → C2 satisfied ✓
            pair.second = int32_t{} (default, value 0)

(C) = 75
    → Assigns 75 to the pair.second (the mapped value)

(D) Temporary XString destroyed (moved-from → no-op)
```

**Two segment allocations per map entry:**
1. XString char data ("Main Quest\0", 11 bytes)
2. Backing store growth (pair<XString, int32_t> is 36 bytes + padding)

**⚠️ Usability issue U2.1 (amplified)**: The user must write:
```cpp
game->quest_progress[XString("Main Quest", xbuf.allocator<XString>())] = 75;
```
Instead of:
```cpp
game->quest_progress["Main Quest"] = 75;  // would require implicit conversion
```

This is even more painful than the XString assignment case because the XString
is used as a **map key**, not a member that already has an allocator.

#### D1.6 XString Key Storage in flat_map

After the operation, the map's backing store (in segment) looks like:

```
backing_store: pair<XString, int32_t>[]
  [0] { XString("Main Quest", offset_ptr→segment), 75 }
  [1] { XString("Side Quest A", offset_ptr→segment), 100 }
  [2] { XString("Side Quest B", offset_ptr→segment), 50 }

Each pair: XString(32) + int32_t(4) + padding(4) = 40 bytes (estimated)
```

All XString keys own their char data in the segment. The offset_ptrs are
segment→segment. C2 is fully satisfied.

---

### Demo 2: demo_memory_management (lines 121–155)

#### D2.1 `xbuf.grow(4096)` — Buffer Expansion

```cpp
xbuf.grow(4096);  // line 143 — grow from 1024 to 5120 bytes
```

**Trace through grow() (line 268):**

```cpp
bool grow(size_type extra_bytes) {
    const size_type original_size = m_buffer.size();     // 1024
    m_buffer.resize(original_size + extra_bytes);        // resize to 5120
    // std::vector may RELOCATE its heap allocation!
    // All raw pointers into the buffer are now INVALID.
    
    base_t::close_impl();                                // detach segment manager
    
    if (!base_t::open_impl(&m_buffer[0], m_buffer.size())) {  // re-attach
        // rollback...
        return false;
    }
    
    base_t::grow(extra_bytes);  // tell segment manager about new space
    // Segment manager extends the free-list to cover the extra 4096 bytes
    return true;
}
```

**Critical sequence:**

```
Before grow:
  m_buffer: heap addr = 0x1000, size = 1024
  segment_manager attached at 0x1000

grow(4096):
  Step 1: m_buffer.resize(5120)
    → std::vector may move to new heap address!
    → m_buffer: heap addr = 0x2000 (new), size = 5120
    → ALL raw pointers (like `game`) now point to FREED memory at 0x1000

  Step 2: close_impl()
    → Detach segment manager from old address

  Step 3: open_impl(0x2000, 5120)
    → Re-attach segment manager at new address
    → All offset_ptrs in the buffer resolve correctly (Lemma C2.1)
    → But raw pointers held by user code are STALE

  Step 4: base_t::grow(4096)
    → Segment manager extends free-list by 4096 bytes
```

**⚠️ Correctness concern C1.1 (CRITICAL)**: In demo_memory_management, the code
does `xbuf.grow(4096)` on line 143, then continues using `game` pointer (from
line 132: `auto* game = xbuf.make<GameData>("game")`). But between these lines,
`stats = xbuf.stats()` is called on line 138. The `game` pointer is NOT used
after grow — the demo only calls `xbuf.stats()`. This is **safe by accident**
but demonstrates the danger pattern.

#### D2.2 Post-grow Pointer Validity

After `grow()`, the `game` pointer from `make<GameData>("game")` is **dangling**.
The demo doesn't use `game` after grow, so no crash. But if a user added:

```cpp
xbuf.grow(4096);
game->level = 50;  // ⚠️ USE-AFTER-FREE! game points to old heap allocation
```

**⚠️ Usability issue U1.2 (amplified)**: The compiler gives NO warning. The `game`
pointer looks valid. This is a category of bug that static analysis tools might
catch, but the library provides no help.

#### D2.3 `xbuf.shrink_to_fit()` — Triple Reconstruction

```cpp
xbuf.shrink_to_fit();  // line 150
```

**Trace through shrink_to_fit() (line 311):**

```
shrink_to_fit():
  Step 1: base_t::shrink_to_fit()
    → Boost.IPC coalesces free blocks at the end of the segment
    → Updates internal size counter to reflect compacted size
    
  Step 2: m_buffer.resize(base_t::get_size())
    → Truncate the vector to the new (smaller) size
    → This is safe: we only removed trailing free space
    
  Step 3: update_after_shrink()
    → Copy m_buffer into a NEW vector (full copy #1)
    → Construct NEW XManagedMemory from the copy (open_impl)
    → Swap the new into *this
    → Old state is destroyed
```

**Why the extra copy?** After `m_buffer.resize()`, the segment manager's internal
state may have stale pointers to the old buffer extent. `update_after_shrink()`
performs a clean re-initialization by copying the (now smaller) buffer and calling
`open_impl` on the copy. This is a correctness safeguard — not the most efficient
approach, but reliable.

#### D2.4 Post-shrink Pointer Invalidation

After `shrink_to_fit()`, ALL pointers into the buffer are invalid:
- `game` pointer from `make()` → dangling
- Any reference to `game->player_name` → dangling
- Any iterator from `game->achievements.begin()` → dangling

The demo doesn't use any pointers after shrink_to_fit(), which is correct.

---

### Demo 3: demo_serialization (lines 161–204)

#### D3.1 Complete Data Flow

```
src_buf.make<GameData>("save")
  → populate data
  → src_buf.save_to_string() → binary_data (std::string, 2048 bytes)
  → XBufferExt::load_from_string(binary_data) → dst_buf
  → dst_buf.find<GameData>("save") → dst_game
```

**Copy count analysis:**

```
Copy 1: save_to_string() — m_buffer → std::string (2048 bytes)
Copy 2: load_from_string() — string → vector<char> (2048 bytes)
Move 1: vector → m_buffer (zero-cost move)
─────────────────────────────────────────
Total: 2 full copies of 2048 bytes = 4096 bytes of memory traffic
```

**⚠️ Usability issue U1.3**: For network transmission, the natural flow would be:
1. `save_to_string()` → copies buffer to string
2. Send string over network
3. Receiver has string → `load_from_string()` → copies string to vector

Two copies of the full buffer are unavoidable with the `std::string` API. A
`save_to_vector()` + `load_from_vector(std::vector<char>&&)` API could reduce
this to one copy + one move.

#### D3.2 find<GameData>("save") vs find_ex<Player>("Hero")

The demo uses `find<T>()` (Boost.IPC native) instead of `find_ex<T>()`:

```cpp
auto* dst_game = dst_buf.find<GameData>("save").first;  // line 183
```

`find<T>()` returns `pair<T*, size_type>` where size_type is the count of
objects with that name (always 1 for named objects). The demo accesses `.first`
to get the pointer. This works but is less readable than `find_ex` which returns
`pair<T*, bool>`.

**No correctness issue**: Both paths use the same iset_index lookup.

#### D3.3 Data Integrity Verification

```cpp
bool integrity_ok = (
    std::string(dst_game->player_name.c_str()) == "SavedHero" &&
    dst_game->player_id == 99999 &&
    dst_game->level == 99
);
```

This verifies:
- **C1**: Scalar values (player_id, level) preserved across byte_copy
- **C2**: XString's offset_ptr survived relocation (`.c_str()` dereferences it)
- **P1-P3**: open_impl succeeded (otherwise dst_game would be null)

The verification is correct but incomplete — it doesn't check containers
(items, achievements, quest_progress). For a test, this would be a gap.

---

### Demo 4: demo_type_signatures (lines 210–245)

#### D4.1 Compile-Time Signature Evaluation

```cpp
constexpr auto item_sig = boost::typelayout::get_definition_signature<Item>();
std::cout << "  Item (definition):\n    " << item_sig.value << "\n\n";
```

`get_definition_signature<Item>()` is `consteval` — the signature string is
computed **entirely at compile time**. The `item_sig` variable is a compile-time
constant containing the signature string.

At runtime, `item_sig.value` is just a `const char*` pointing to a string
literal in the binary's read-only data section. **Zero runtime computation cost**.

#### D4.2 Runtime Output vs Compile-Time Verification

The `static_assert` in `game_data.hpp` (line 70) and the runtime `std::cout`
in this demo use the **exact same function**: `get_definition_signature<Item>()`.

- `static_assert`: Runs at compile time → prevents incompatible builds
- `std::cout`: Runs at runtime → displays for human inspection

These are independent uses of the same information. The runtime output exists
solely for demonstration — it has no correctness function.

**No usability or correctness issues.** This demo is purely informational.

---

### Demo 5: demo_automatic_compaction (lines 251–335)

#### D5.1 compact_automatic<GameData> — Full Migration Path

GameData has 7 fields (vs Player's 4), including nested containers:

```
migrate_members(old_game, new_game, old_xbuf, new_xbuf):
  │
  ├─ [0] player_id (int32_t): TrivialCopy → direct assignment
  ├─ [1] level (int32_t): TrivialCopy → direct assignment
  ├─ [2] health (float): TrivialCopy → direct assignment
  ├─ [3] player_name (XString): AllocatorAware
  │      → XString("FragmentedHero", new_segment_manager)
  │      → Allocate new char buffer in new segment
  │
  ├─ [4] items (XVector<Item>): Container
  │      → migrate_container (see D5.2)
  │
  ├─ [5] achievements (XSet<int32_t>): Container
  │      → migrate_container (see D5.3)
  │
  └─ [6] quest_progress (XMap<XString,int32_t>): Container
         → migrate_container (see D5.4)
```

#### D5.2 XVector<Item> Migration — Recursive Container

```cpp
migrate_container(old_items, new_items, old_xbuf, new_xbuf):
  // Item is NOT trivially copyable (contains XString with offset_ptr)
  // Therefore: per-element migration

  for each item in old_items:  // 17 items (20 - 3 popped)
    auto migrated = migrate_element(item, old_xbuf, new_xbuf)
      → resolve_strategy<Item>() = Composite (not registered, not trivially copyable)
      → Item new_item(new_xbuf.get_segment_manager())  // construct empty in new segment
      → migrate_members(item, new_item, old_xbuf, new_xbuf):
          [0] item_id: TrivialCopy
          [1] item_type: TrivialCopy
          [2] quantity: TrivialCopy
          [3] name: AllocatorAware → XString(old.name, new_SM)
              → Allocate chars in NEW segment
      → return new_item  // ⚠️ RVO/move

    new_items.emplace_back(std::move(migrated))
```

**Per-item allocation in NEW segment:**
- Item object (48 bytes) — allocated as part of vector backing store
- XString char data (variable) — separate allocation

**For 17 items**: 17 separate XString char allocations + multiple vector growth
reallocations. Total: ~30+ segment allocations.

**⚠️ Correctness subtlety**: `migrate_element` creates a temporary `Item` on the
stack (via `Item new_item(new_xbuf.get_segment_manager())`), then returns it.
This temporary has the same cross-boundary offset_ptr issue as XString temporaries
(F3.1 in helloworld analysis). The `return new_item` should trigger NRVO or move,
ensuring the offset_ptrs are properly recalculated when landing in the vector.

#### D5.3 XSet<int32_t> Migration — Trivial Fast Path

```cpp
migrate_container(old_achievements, new_achievements, old_xbuf, new_xbuf):
  // int32_t IS trivially copyable
  // Fast path:
  new_achievements = old_achievements;
```

**Wait — this is a CROSS-SEGMENT assignment!** (line 593-596):

```cpp
if constexpr (std::is_trivially_copyable_v<ElementType>) {
    new_container = old_container;  // ⚠️ Cross-segment copy
    return;
}
```

This copies the entire XSet from old segment to new segment via `operator=`.
The XSet's internal `x_vector_impl` handles this:
1. Allocates new backing store in new segment (via new_container's allocator)
2. Copies all int32_t elements (trivial byte copy)
3. **Does NOT copy the old backing store's offset_ptr** — creates new one

This is **correct** because Boost.Container's `operator=` is allocator-aware:
when source and destination have different allocators, it performs element-wise
copy, not a memcpy of the internal state.

#### D5.4 XMap<XString, int32_t> Migration — Key+Value

```cpp
migrate_container(old_quest_progress, new_quest_progress, old_xbuf, new_xbuf):
  // pair<XString, int32_t> is NOT trivially copyable (XString has offset_ptr)
  // Per-element migration:

  for (const auto& [key, value] : old_quest_progress):
    auto new_key = migrate_element(key, old_xbuf, new_xbuf)
      → resolve_strategy<XString>() = AllocatorAware
      → XString(old_key, new_SM)  // copy char data to new segment

    auto new_value = migrate_element(value, old_xbuf, new_xbuf)
      → resolve_strategy<int32_t>() = TrivialCopy
      → return old_value (direct copy)

    new_quest_progress.emplace(std::move(new_key), std::move(new_value))
```

**For 3 entries**: 3 XString key copies (each allocates chars in new segment)
+ 3 int32_t value copies (trivial).

#### D5.5 Free-list Fragmentation Elimination

**Before compaction** (old_xbuf):

```
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬───────────┐
│ Hdr  │ idx  │ Game │ FREE │ str  │ FREE │ vec  │  FREE...  │
│      │      │ Data │(frag)│ data │(frag)│ data │           │
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴───────────┘
Fragmented: many small free blocks interspersed with data
```

**After compaction** (new_xbuf, post shrink_to_fit):

```
┌──────┬──────┬──────┬───────┬──────┬──────┐
│ Hdr  │ idx  │ Game │ str+  │ vec  │      │
│      │      │ Data │ chars │ data │ tiny │
│      │      │      │       │      │ free │
└──────┴──────┴──────┴───────┴──────┴──────┘
Defragmented: all data packed, single free block at end
```

The compacted buffer has **zero internal fragmentation**. All data is contiguous,
and the only free space is at the trailing end (if any, after shrink_to_fit).

---

### Demo 6: demo_performance (lines 341–390)

#### D6.1 1000× emplace_back — Growth Trigger Count

```cpp
XBufferExt xbuf(65536);  // 64KB buffer
auto* game = xbuf.make<GameData>("perf_test");
for (int i = 0; i < 1000; i++) {
    game->items.emplace_back(xbuf.allocator<Item>(), ...);  // line 370
}
```

**1.1x growth from capacity 0 to 1000:**

The sequence of capacities follows: 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10
→ 11 → 13 → 15 → 17 → 19 → 21 → 24 → 27 → 30 → 33 → 37 → 41 → 46 → 51 → 57
→ 63 → 70 → 77 → 85 → 94 → 104 → 115 → 127 → 140 → 154 → 170 → 187 → 206
→ 227 → 250 → 275 → 303 → 334 → 368 → 405 → 446 → 491 → 541 → 596 → 656
→ 722 → 795 → 875 → 963 → 1060

**~55 reallocations** to reach capacity ≥ 1000. Compare with 2x growth: ~10 reallocations.

Each reallocation:
1. Allocates new backing store: `cap * sizeof(Item)` = `cap * 48` bytes
2. Moves all existing Items (each 48 bytes, contains offset_ptr in XString)
3. Deallocates old backing store → free-list fragment

**⚠️ Performance**: 55 reallocations of increasing size means:
- Sum of all moved bytes ≈ Σ(cap_i × 48) for each realloc ≈ several MB
- 55 free-list fragments of sizes 48, 96, 144, ..., 45,600 bytes

#### D6.2 Free-list Fragment Sizes

Each growth creates a free block of `old_cap * 48` bytes:

```
Growth  Old cap  Free block size
  1     0→1      0 (initial)
  2     1→2      48 bytes
  3     2→3      96 bytes
  ...
  55    963→1060  46,224 bytes
```

With x_seq_fit, these are linked in order. Large fragments can be reused for
subsequent allocations. The fragmentation is less severe than the count suggests
because each new fragment is larger than the previous one.

#### D6.3 Buffer Size Estimation

Each Item in the segment needs:
- 48 bytes in the vector backing store (Item struct)
- ~10–15 bytes for XString char data (item names like "Item_999")
- Allocator overhead: ~8–16 bytes per allocation (free-list node headers)

Estimated per-item cost: ~70 bytes.
For 1000 items: ~70,000 bytes ≈ 68 KB.

With 65,536 bytes (64 KB) initial buffer, this is **borderline insufficient**.
The buffer will need to `grow()` during the loop, or allocations will fail.

**⚠️ Usability issue U2.3**: The demo uses 65536 bytes for 1000 items but doesn't
explain the sizing rationale. Users would benefit from a sizing guide or helper.

---

### Demo 7: demo_advanced_features (lines 396–432)

#### D7.1 No Memory Operations

This demo only prints comparison tables and feature lists. No buffer operations,
no allocations, no memory model interactions. **No analysis needed.**

---

## Findings Summary (Part I)

### Usability Issues Found

| ID | Severity | Location | Issue |
|----|----------|----------|-------|
| U1.1 | Nice-to-have | H3.2, D1.5 | `allocator<XString>()` template parameter misleading |
| U1.2 | **Important** | H2.1, D2.2 | `make()` returns raw pointer; dangling after grow/shrink |
| U1.3 | Important | H5.1, D3.1 | `save_to_string()` copies full buffer including unused space |
| U2.1 | **Important** | H3.5, D1.5 | XString assignment verbose; `= "Alice"` not supported |
| U2.3 | Important | H1.1, D6.3 | No API to estimate segment overhead or required buffer size |

### Correctness Concerns Found

| ID | Severity | Location | Issue |
|----|----------|----------|-------|
| C1.1 | **Critical** | H2.1, D2.2 | Raw pointer use-after-grow/shrink risk; no compile-time protection |
| C3.1 | Important | H7.2 | Object destructors not called on segment destruction |
| C4.x | Needs audit | D5.3 | Cross-segment `operator=` correctness depends on Boost.Container allocator awareness |

### Performance Concerns Found

| ID | Severity | Location | Issue |
|----|----------|----------|-------|
| P1 | Nice-to-have | H4.2, D6.1 | 1.1x growth: ~55 reallocs for 1000 items (vs ~10 with 2x) |
| P2 | Nice-to-have | H6.3, D2.3 | shrink_to_fit() involves extra buffer copy |

---

## Cross-cutting Concerns

### X1: Allocator Propagation in Boost.Container (Rebind Mechanism)

**The rebind chain** is the most non-obvious mechanism in the library:

```
User calls:           xbuf.allocator<XString>()
Returns:              allocator<XString, segment_manager>
Passed to:            XString(text, allocator)
XString rebinds to:   allocator<char, segment_manager>  (via allocator_traits::rebind_alloc)
XString stores:       offset_ptr<segment_manager>        (8 bytes within the 32-byte control block)
Allocates char buf:   segment_manager->allocate(n * sizeof(char))
```

**Why rebind exists**: C++ allocators are typed (`allocator<T>`), but containers
may need to allocate different types internally. A `vector<Item>` needs to allocate
`Item` objects, but also free-list nodes. Rebind allows: `allocator<Item>` → `allocator<free_list_node>`.

**In XOffset context**: The segment manager pointer is the real state. The type
parameter is just sizing information. This is why `xbuf.allocator<XString>()` and
`xbuf.allocator<int32_t>()` would both work — they carry the same segment manager.

### X2: x_seq_fit vs x_best_fit Fragmentation

```
x_seq_fit (simple_seq_fit):
  ├─ Free-list: singly-linked, first-fit allocation
  ├─ Overhead: ~128 bytes segment header
  ├─ Allocation: O(n) scan of free-list
  ├─ Fragmentation: higher (first-fit doesn't minimize waste)
  └─ Used by: XBuffer (default)

x_best_fit (rbtree_best_fit):
  ├─ Free-list: red-black tree, best-fit allocation
  ├─ Overhead: ~192 bytes segment header
  ├─ Allocation: O(log n) tree search
  ├─ Fragmentation: lower (best-fit minimizes waste)
  └─ Used by: XBufferBestFit (available but not default)
```

**Why x_seq_fit is default**: For typical use cases (few large objects, not many
small allocations), first-fit is fast enough. The 1.1x growth factor creates
sequential allocations that are well-served by first-fit. For workloads with
many small allocations and deletions, `XBufferBestFit` would be better.

### X3: ASCII Memory Layout Diagrams

**Phase: After Buffer Creation (4096 bytes)**
```
Offset  0                    128                                        4096
        ┌────────────────────┬──────────────────────────────────────────┐
        │ Segment Header     │ Free Block (single contiguous)           │
        │ ~128 bytes         │ ~3968 bytes                              │
        │ [SM control block] │ [prev: null | next: null | size: 3968]   │
        │ [iset root: null]  │                                          │
        │ [free root: →128]  │                                          │
        └────────────────────┴──────────────────────────────────────────┘
```

**Phase: After Player Construction + Data Fill**
```
Offset  0        128    176     248  254    258  266    278          4096
        ┌────────┬──────┬───────┬────┬──────┬────┬──────┬───────────┐
        │ Seg Hdr│ iset │Player │FREE│"Alice│FREE│items │   FREE    │
        │ 128B   │~48B  │ 72B   │ 4B │\0"6B │ 8B │12B   │ ~3700B   │
        │        │"Hero"│id=1   │(v1)│      │(v2)│101   │           │
        │        │→Play │lv=10  │    │      │    │102   │           │
        │        │      │name→  │    │      │    │103   │           │
        │        │      │items→ │    │      │    │      │           │
        └────────┴──────┴───────┴────┴──────┴────┴──────┴───────────┘
                                 ↑ old vector backing stores (fragmentation)
```

**Phase: After Compaction**
```
Offset  0        128    176     248  254      278
        ┌────────┬──────┬───────┬────┬────────┬──┐
        │ Seg Hdr│ iset │Player │"Al"│items   │F │
        │ 128B   │~48B  │ 72B   │ 6B │4×4=16B │  │
        │        │"Hero"│id=1   │    │101,102 │  │
        │        │→Play │lv=10  │    │103,201 │  │
        │        │      │name→  │    │        │  │
        │        │      │items→ │    │        │  │
        └────────┴──────┴───────┴────┴────────┴──┘
         Zero internal fragmentation. Only trailing free.
```

### X4: offset_ptr Mechanics Across Stack↔Segment

```
                  STACK                              HEAP (segment buffer)
                  ─────                              ────────────────────
                  │                                  │
  XString temp    │  ┌──────────┐                    │  ┌──────────┐
  (32 bytes)      │  │offset_ptr│───────────────────→│  │"Alice\0" │
                  │  │stored =  │   stored_value =   │  │ 6 bytes  │
                  │  │ (tgt-this)│   segment_addr -   │  └──────────┘
                  │  │= -大负数  │   stack_addr       │
                  │  │size = 5  │   (e.g., -0x7FFC..)│
                  │  │cap = 5   │                    │
                  │  │alloc→SM  │                    │
                  │  └──────────┘                    │
                  │                                  │
  After move to   │                                  │  ┌──────────┐
  segment member: │                                  │  │offset_ptr│──→ "Alice\0"
                  │                                  │  │stored =  │     (short distance)
                  │                                  │  │ +小正数   │
                  │                                  │  │size = 5  │
                  │                                  │  └──────────┘
```

**Key rule**: Cross-boundary offset_ptrs (stack→segment) are valid for immediate
use but MUST NOT be persisted in the buffer. The move assignment operation
(step C in line 24 analysis) recalculates the offset_ptr from the new `this`
location (in segment) to the target (also in segment), producing a self-contained
reference that survives byte_copy.

### X5: Document Assembly

The analysis has been assembled into `docs/MEMORY_LIFECYCLE_ANALYSIS.md` covering:
- File 1 (player.hpp): 5 sections
- File 2 (game_data.hpp): 6 sections
- File 3 (helloworld.cpp): 7 phases, line-by-line
- File 4 (demo.cpp): 7 demos, line-by-line
- Cross-cutting: 4 topics with ASCII diagrams

---

## Part II: Correctness Audit

### C1. Pointer/Reference Validity

#### C1.1 grow() Post-Invalidation Risk

**Audit finding: ⚠️ CRITICAL — Documentation insufficient**

The `grow()` function has a one-line WARNING comment (line 299):
```cpp
// WARNING: All existing pointers/references into the buffer are invalidated
// after this call. Re-acquire them via find<T>() or find_or_construct<T>().
```

This is insufficient because:
1. The comment is on `update_after_shrink()`, not on `grow()` itself
2. `grow()` (line 268) has NO warning comment at all
3. The returned `Player*` from `make()` gives no indication it may be invalidated
4. No runtime assertion can detect use-after-grow (it's a dangling pointer)

**Recommendation**: Add `[[nodiscard("pointer invalidated after grow/shrink")]]`
attribute to `make()`, or return an `offset_ptr<T>` wrapper that auto-resolves.

#### C1.2 shrink_to_fit() Invalidation Chain

**Audit finding: ⚠️ Documentation exists but incomplete**

`shrink_to_fit()` (line 311) and `update_after_shrink()` (line 301) both have
WARNING comments. However, the chain is: `shrink_to_fit()` → `base_t::shrink_to_fit()`
→ `resize()` → `update_after_shrink()`. The first two steps already invalidate
pointers, but only `update_after_shrink` has the warning.

**The invalidation is actually DOUBLE**: both `resize()` (vector may relocate)
AND `update_after_shrink()` (creates new buffer and swaps) invalidate pointers.

#### C1.3 compact_automatic Post-Invalidation

**Audit finding: ✅ Safe by design**

`compact_automatic()` returns a NEW `XBuffer`. The old buffer is untouched.
The user's old pointers remain valid for the old buffer. The risk is ONLY if the
user confuses old and new buffers. No code fix needed, but documentation should
make clear: "The returned buffer is independent; old buffer is NOT modified."

### C2. Exception Safety

#### C2.1 XString Temporary Construction Failure

**Audit finding: ✅ Safe**

```cpp
player->name = XString("Alice", xbuf.allocator<XString>());
```

If the XString constructor fails (segment full → `bad_alloc`):
1. No char buffer is allocated (allocation failed)
2. Stack-resident temporary is destroyed (nothing to clean up)
3. `player->name` is unchanged (assignment never executes)
4. No resource leak

**Exception safety level**: Strong guarantee (state unchanged on failure).

#### C2.2 push_back Allocation Failure

**Audit finding: ✅ Safe (Boost.Container guarantees)**

If `push_back` fails during reallocation:
1. New backing store allocation fails → `bad_alloc`
2. Old backing store is untouched (not yet deallocated)
3. Vector size/capacity unchanged
4. Strong guarantee

If `push_back` fails during element construction (after reallocation):
1. New backing store is allocated
2. Old elements are copied/moved to new store
3. New element construction fails
4. Boost.Container unwinds: deallocate new store, restore old state
5. Strong guarantee

#### C2.3 compact_automatic Mid-Failure

**Audit finding: ⚠️ Partial — new buffer leaked on exception**

```cpp
XBuffer new_xbuf(new_size);                    // (1) may throw
auto* old_obj = old_xbuf.find<T>(...).first;   // (2) no throw
auto* new_obj = new_xbuf.construct<T>(...)(...); // (3) may throw
migrate_members(*old_obj, *new_obj, ...);       // (4) may throw
new_xbuf.shrink_to_fit();                       // (5) may throw
return new_xbuf;                                // (6) NRVO
```

If (3) or (4) throws:
- `new_xbuf` is destroyed by stack unwinding (correct: RAII)
- `old_xbuf` is untouched (correct: old data preserved)
- **No data loss** — the old buffer is intact

If (5) throws:
- `new_xbuf` contains migrated data but is not shrunk
- Stack unwinding destroys `new_xbuf` (data lost)
- `old_xbuf` is untouched
- **Data loss in new buffer only** — old buffer preserved

**Assessment**: Basic exception safety (no resource leak). Not strong guarantee
because the new buffer's partial state is lost. Acceptable for a compaction operation.

### C3. Destruction Completeness

#### C3.1 Segment Destruction — No Object Destructors

**Audit finding: ⚠️ Important — by design, but poorly documented**

When `XManagedMemory` is destroyed:
```cpp
~XManagedMemory() { this->priv_close(); }
void priv_close() {
    base_t::destroy_impl();                  // detach segment manager
    std::vector<char>().swap(m_buffer);      // free heap allocation
}
```

`destroy_impl()` does NOT call destructors on named objects. The entire segment
is bulk-freed. This is correct for segment-contained data (all resources are
within the buffer), but:

1. **XString**: Destructor would deallocate char buffer — unnecessary (bulk free)
2. **XVector**: Destructor would deallocate backing store — unnecessary (bulk free)
3. **User types**: Destructor would be trivial if all members are in S — correct

**Risk**: If a user's type has a side-effecting destructor (e.g., logging), it
won't fire. Domain S doesn't prevent this (a struct with only int32_t members
can have a non-trivial destructor). But since Domain S prevents external resources
(no file handles, no raw pointers), the practical risk is minimal.

#### C3.2 XString/XVector Destructor Dependency

**Audit finding: ✅ Safe**

XString/XVector destructors call `allocator.deallocate()`, which calls
`segment_manager->deallocate()`. If the segment manager is already destroyed
(priv_close was called), this would be use-after-free.

BUT: `priv_close` calls `destroy_impl()` first, which marks the segment as
closed. Then `vector().swap(m_buffer)` frees the memory. The segment manager
is invalidated but the destructors of individual objects are never called
(as per C3.1). So there's no conflict.

### C4. Cross-Segment Operations

#### C4.1 migrate_container Cross-Segment Assignment

**Audit finding: ✅ Correct — Boost.Container allocator-awareness**

```cpp
if constexpr (std::is_trivially_copyable_v<ElementType>) {
    new_container = old_container;  // cross-segment
}
```

Boost.Container's `operator=` checks if `allocator == other.allocator`. When
allocators are from different segments, they compare unequal (different segment
manager pointers). In this case, `operator=` performs element-wise copy using
the destination's allocator. This correctly allocates in the new segment.

**Verified by**: Boost.Container documentation + the fact that all tests pass
after compaction with cross-segment migration.

#### C4.2 Stack-Resident XString with Segment Allocator

**Audit finding: ✅ Safe — allocator outlives temporary**

```cpp
XString temp("Alice", xbuf.allocator<XString>());
// temp lives on stack, char data in segment
// temp destroyed at end of expression
```

The temporary's destructor calls `allocator.deallocate(char_buffer)`. This is
a call to the segment manager, which is still alive (xbuf is in scope). The
deallocate correctly returns the char buffer to the segment's free-list.

**Risk scenario**: If `xbuf` were destroyed BEFORE the temporary, the segment
manager would be invalid. This cannot happen in normal code (temporaries are
destroyed before locals in the same scope).

#### C4.3 Cross-Segment Allocator Comparison

**Audit finding: ✅ Correct**

Two allocators from different segments compare **unequal** because their internal
`offset_ptr<segment_manager>` values are different. This causes Boost.Container
to use copy semantics (not move) when assigning across segments, which is correct:
copy means "allocate in my own segment and copy elements".

### C5. Concurrency Safety

#### C5.1 null_mutex_family Documentation

**Audit finding: ⚠️ Important — no explicit warning**

`XBuffer` uses `null_mutex_family` (no-op mutex). This means:
- No thread safety for ANY operation
- Concurrent `make()`, `find()`, `push_back()` will cause data corruption
- Even concurrent reads may be unsafe if the segment manager is being modified

Current documentation: **None**. The `null_mutex_family` is a template parameter
deep in the typedef chain. Users may not realize the buffer is not thread-safe.

**Recommendation**: Add a prominent comment in `XBufferExt` or the README:
"⚠️ XBuffer is NOT thread-safe. All access must be externally synchronized."

---

## Part III: Usability Analysis

### U1. API Traps

#### U1.1 allocator<XString>() Template Parameter

**Finding**: The template parameter in `xbuf.allocator<XString>()` is misleading.
The allocator is immediately rebound to `allocator<char>` inside XString. Any type
would work: `xbuf.allocator<int>()` would produce the same segment manager.

**Recommendation**: Add a no-template overload `xbuf.allocator()` that returns
the segment manager pointer directly. Keep the typed version for explicitness.

#### U1.2 make() Returns Raw Pointer

**Finding**: `make<T>()` returns `T*` which becomes dangling after grow/shrink.
This is the most dangerous usability trap in the library.

**Recommendations** (in order of impact):
1. **Documentation**: Bold warning on `make()`, `find()`, `find_ex()`
2. **Named handle**: Return a lightweight `XHandle<T>` that stores the name
   and re-resolves via `find()` on each dereference (safe but slower)
3. **offset_ptr wrapper**: Return `offset_ptr<T>` which survives buffer
   relocation (fast but requires operator-> overhead)

#### U1.3 save_to_string() Full Copy

**Finding**: Copies entire buffer including unused space (zeros).

**Recommendations**:
1. Add `save_to_string_compact()` — shrink first, then save
2. Add `save_to_span()` — return `std::span<const char>` (zero-copy)
3. Add `save_to_vector()` — return `std::vector<char>` by move

### U2. Missing Convenience APIs

#### U2.1 XString Direct Assignment

**Finding**: `player->name = "Alice"` doesn't work; requires full XString constructor.

**Recommendation**: The XString already has an allocator stored internally. Adding:
```cpp
// In XString (or as a free function):
XString& operator=(const char* s) {
    this->assign(s);  // Boost.Container basic_string already has assign()
    return *this;
}
```
This should work because `assign()` uses the string's existing allocator to
allocate the new char buffer. **This is likely the highest-impact usability fix.**

#### U2.2 Range-Based push_back

**Finding**: No batch insertion API. Users must loop manually.

**Recommendation**: Add `append(initializer_list<T>)` or `insert(range)` wrappers.
Lower priority — the container's own API works, just verbose.

#### U2.3 Buffer Size Estimation

**Finding**: No API to predict buffer size requirements.

**Recommendation**: Add a static helper:
```cpp
static size_t estimate_buffer_size(size_t user_data_bytes) {
    return user_data_bytes + 256;  // 128 header + 128 margin
}
```

### U3. Error Diagnostics

#### U3.1 Segment Full Error Message

**Finding**: When the segment is full, Boost.IPC throws `bad_alloc` with a
generic message. The user doesn't know which allocation failed or how much
space was needed.

**Recommendation**: Wrap allocation calls with a try/catch that adds context:
"XBuffer allocation failed: requested N bytes, available M bytes."

#### U3.2 static_assert Member Identification

**Finding**: When `validate_xbuffer_type<T>()` fails, the error message says
"Type T is not safe for XBuffer" but doesn't identify WHICH member is unsafe.

**Recommendation**: Use C++26 reflection to enumerate members and generate
per-member `static_assert` messages. This is a significant improvement.

#### U3.3 grow() Failure Diagnosis

**Finding**: `grow()` returns `bool` with no failure reason.

**Recommendation**: Add `std::expected<void, std::string> grow_ex(size_type)`
or at minimum, log the failure reason internally.

### U4. Documentation & Mental Model

#### U4.1 offset_ptr Understanding Requirement

**Finding**: Users do NOT need to understand offset_ptr for basic usage. The
library abstracts it away. However, users MUST understand that raw pointers
into the buffer are invalidated by grow/shrink/compact.

**Recommendation**: The documentation should focus on the "pointer invalidation"
rule rather than offset_ptr internals.

#### U4.2 Pointer Invalidation Documentation Visibility

**Finding**: The WARNING comments are buried in the source code (lines 299, 309).
They don't appear in any user-facing documentation.

**Recommendation**: Add a "⚠️ Critical Rules" section to README.md:
1. All pointers from `make()` / `find()` are invalidated by `grow()` / `shrink_to_fit()`
2. Always re-acquire pointers after buffer mutations
3. Never store raw pointers across buffer operations

---

## Part IV: Summary & Implementation Plan

### I1. Classified Findings

#### 🔴 Critical (Fix immediately)
| ID | Issue | Fix |
|----|-------|-----|
| C1.1 | Raw pointer use-after-grow/shrink | Add WARNING comments on `grow()`, `make()`, `find()` |

#### 🟡 Important (Fix in next iteration)
| ID | Issue | Fix |
|----|-------|-----|
| U1.2 | make() dangling pointer risk | Design XHandle<T> or document prominently |
| U2.1 | XString assignment verbosity | Add operator=(const char*) if feasible |
| U1.3 | save_to_string copies unused space | Add save_to_vector() alternative |
| U2.3 | No buffer size estimation | Add estimate_buffer_size() helper |
| C3.1 | Object destructors not called | Document as "by design" in API docs |
| C5.1 | No thread-safety documentation | Add prominent warning |

#### 🟢 Nice-to-have (Backlog)
| ID | Issue | Fix |
|----|-------|-----|
| U1.1 | allocator template parameter misleading | Add no-template overload |
| U2.2 | No range-based push_back | Add append() wrapper |
| U3.1 | Segment full error message generic | Add allocation context |
| U3.2 | static_assert doesn't identify unsafe member | Reflection-based diagnostics |
| U3.3 | grow() returns bool only | Add grow_ex() with reason |
| U4.1/U4.2 | Pointer invalidation underdocumented | README "Critical Rules" section |
| P1 | 1.1x growth factor fragmentation | Consider configurable growth |
| P2 | shrink_to_fit extra copy | Optimize update_after_shrink |
