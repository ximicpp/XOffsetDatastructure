# Conference Session Proposal

## Title

**"From Boilerplate to Zero: How C++26 Reflection Redesigned a Zero-Encoding Serialization Library"**

## Abstract

Serializing a complex game state at zero cost — no encoding, no decoding, just raw bytes — sounds ideal until you discover that every container member demands a hand-written allocator constructor. That is the architectural tension at the heart of zero-encoding serialization: the offset_ptr-based containers that make it possible require a memory-pool handle at construction time, and without compile-time member introspection, only the user can wire that up.

Before C++26, a real library worked around this with a ~600-line Python code generator that produced allocator constructors from YAML schemas, aggregate-only mirror types that enabled Boost.PFR-based reflection, and 150 lines of hand-written concepts that verified member safety. Each layer compensated for the same language-level gap.

C++26 static reflection (P2996) closes that gap. This talk traces how one primitive — `nonstatic_data_members_of` — cascades through the library. It intercepts allocator `construct()` and reflects over members to inject allocators automatically, eliminating hand-written constructors. `access_context::unchecked()` sees private members; `bases_of` traverses inheritance — together they eliminate mirror types and lift the aggregate-only restriction. The code generator is deleted entirely, and ~80 lines of library-specific boilerplate per type drop to zero — users write plain C++ structs with no library artifacts. The same primitive then enables automatic memory compaction — previously requiring hand-written per-type migration code.

Attendees will understand the conditions that make zero-encoding serialization sound, see the architectural tension it creates and how C++26 reflection resolves it, and take away a design principle — reflect at decision points, not definition sites — demonstrated through a real library rewrite.

---

## Outline (60 minutes)

### Part 1: The Architectural Tension in Zero-Encoding (8 min)

Zero-encoding means `save()` returns raw buffer bytes, `load()` maps them back — no per-field processing. Two structural requirements make this sound:

- **All data in one buffer.** Containers must allocate internal storage from the buffer, not the heap — otherwise byte-copying loses the data. This rules out `std::string` and `std::vector`, whose internal storage lives on the heap.
- **Position-independent references.** A regular pointer stores an absolute address; when the buffer is loaded at a different address, the pointer is invalid. `offset_ptr` stores `target - this` (a relative offset) instead, so every reference self-adjusts on byte-copy.

Together: every container member must be constructed with the allocator that owns the buffer's memory pool — called a *segment manager*. This is a structural requirement of zero-encoding, not a design choice.

**The tension:** user burden scales linearly with type richness. Every container member adds a line to the allocator-propagating constructor. Every type adds a constructor to maintain. Inheritance and nesting multiply the cost:

```cpp
struct GameData {
    using allocator_type = XAllocator;
    template <typename Alloc>
    GameData(Alloc alloc)
        : player_name(alloc), items(alloc),
          achievements(alloc), quest_progress(alloc) {}
    template <typename Alloc>
    GameData(GameData&& other, Alloc alloc)
        : player_id(other.player_id), level(other.level),
          health(other.health),
          player_name(std::move(other.player_name), alloc),
          items(std::move(other.items), alloc),
          achievements(std::move(other.achievements), alloc),
          quest_progress(std::move(other.quest_progress), alloc) {}
    int32_t player_id{0};
    int32_t level{0};
    float health{0.0f};
    XString player_name;
    XVector<Item> items;
    XSet<int32_t> achievements;
    XMap<XString, int32_t> quest_progress;
};
```

Four container members → four lines in each constructor. This is one type. A real project has dozens.

### Part 2: The Workaround Stack (7 min)

The constructor is only the beginning. The library also needs to **reflect** over user types (for type safety checks) and **verify** member safety (for admission control). Without compile-time member introspection, each need generates its own workaround:

- **Construction:** Library can't see which members are containers → users write allocator-propagating constructors (shown above).
- **Reflection:** Boost.PFR (a library providing field-by-field access to aggregate types) offers partial introspection but only for aggregates → users create **mirror types** (aggregate copies of their types, stripped of private members and inheritance) to enable PFR.
- **Automation:** Constructors + mirror types are error-prone at scale → a ~600-line Python code generator automates both from YAML schemas.
- **Admission:** Member safety must be verified at compile time → 150 lines of hand-written concepts recursively check each member.

Construction and reflection are parallel needs; the code generator automates both; admission is a third parallel need with its own workaround. Shared root cause: the library cannot see the user's type at compile time.

### Part 3: One Primitive Eliminates the Stack (25 min)

C++26 P2996 provides `nonstatic_data_members_of(^^T, access_context::unchecked())` — compile-time iteration over any type's members, including private and inherited. Combined with `bases_of` (base class traversal), `type_of` (member type inspection), and splice syntax (injecting a reflected member back into code as an expression), the library gains complete vision into user types.

**The allocator interceptor (12 min).** Instead of generating a constructor for each type, the library intercepts the allocator's `construct()` call — the decision point — and uses reflection to handle each member: primitives are value-initialized, containers receive the segment manager, composites recurse, base classes are traversed via `bases_of`. One mechanism handles every type — no per-type code, no registration.

Nesting is handled by recursion: `XVector<XVector<Item>>` where `Item` has an `XString` — each level triggers the interceptor, each level reflects over its members. Move and reallocation use `reflect_transfer_init_all<T>()` with the same member-iteration logic — one mechanism, two code paths.

Result: users write plain structs with no constructor, no `allocator_type`, no macro. `make<T>()` constructs them with correct allocator propagation automatically. Types with existing hand-written allocator constructors continue to work — the reflection path activates only for types that lack them.

**Cascading consequences (13 min).** The same capability eliminates the rest of the stack, in two categories:

*What gets deleted:*
- `access_context::unchecked()` sees private members → mirror types deleted; the actual type IS the reflection source.
- Constructors and mirror types no longer need generation → YAML schemas and the code generator deleted.
- Admission delegates to a companion type-safety library using the same P2996 member traversal → 150 lines of hand-maintained concepts become a 1-line delegation.

*What gets unlocked:*
- `bases_of` traverses inheritance → single, multi-level, and mixin inheritance with container members at every level now work.
- Polymorphic types (virtual functions, virtual inheritance) are statically rejected via `static_assert` — turning a potential runtime bug (corrupted vtable in serialized data) into a compile error.

~80 lines of library-specific boilerplate per type → zero. Users write plain C++ structs.

**Why intercept, not generate.** The design principle: reflect at the decision point (`construct()`), not the definition site (don't generate constructors per type). Interception composes — move/reallocation reuses the same logic, new types work without registration, existing allocator-aware types coexist. One mechanism, multiple uses, zero per-type code.

### Part 4: Reflection-Driven Compaction (10 min)

Arena-style buffers fragment as objects are created and destroyed. Before C++26, compacting an offset_ptr-based object graph required hand-written per-type migration code.

`compact<T>()` deep-migrates the entire object graph to a minimal new buffer. Each member kind has a different relationship to the buffer's address space — a plain int can be bit-copied, but an offset_ptr-based container must be reconstructed through the new buffer's allocator to update its internal offsets. A `consteval` function reflects over members and selects the per-member strategy at compile time: bitwise copy, allocator-aware move, container element migration, or composite recursion.

The same `nonstatic_data_members_of` applied at a different decision point — migration instead of construction. The audience sees the same mechanism solve a fundamentally different problem, confirming the design principle through reuse rather than assertion.

### Part 5: Practicalities and Takeaway (5 min)

Compiler status: requires a Clang fork with P2996 support; CI runs in Docker with the fork pre-built. The Docker image and all source code are publicly available — attendees can reproduce every example without building the compiler fork. Architectural constraint: 64-bit little-endian only. Limitations: no schema evolution (adding a field changes the layout), `constexpr` step limits on very large types. Runtime overhead: zero — all reflection is `consteval` and produces no runtime code on the serialization path.

Zero-encoding serialization's architectural tension — performance vs. usability — existed because the language could not inspect type members at compile time. C++26 `nonstatic_data_members_of` is the first language primitive that resolves it: not by making the workarounds easier, but by making them unnecessary.

### Q&A (5 min)
