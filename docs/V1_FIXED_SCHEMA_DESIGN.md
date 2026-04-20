# XOffsetDatastructure V1 Fixed-Schema Detailed Design

## 1. Decision

`v1` explicitly drops field evolution.

The compatibility model for `v1` is:

- same `format_major`
- same root type
- same schema hash
- same layout baselines across the target platform set

Any field addition, removal, reorder, type change, alignment change, or container ABI change is a breaking change in `v1`.

This is an intentional tradeoff. It removes the most complex part of the system and makes the primary goal achievable:

- zero encode/decode transport
- direct in-memory read/write
- mutation-friendly containers
- performance close to native types and STL-style containers

## 2. When Dropping Field Evolution Is Acceptable

It is acceptable to drop field evolution in `v1` if the system matches most of the following:

- sender and receiver are upgraded together
- stored data can be rebuilt or invalidated on version change
- the protocol is internal, not public
- there is no rolling upgrade window where old and new schemas must coexist
- save files are either version-locked or regenerated

It is not acceptable to drop field evolution if any of the following is required:

- old save files must load in new binaries
- client and server may run different schema versions
- hot update or rolling deployment must keep mixed versions online
- the format is public or third-party facing

For this repository, `v1` should take the simpler and stronger position:

- `v1` is a fixed-schema zero-copy object format
- schema mismatch is rejected at load time

## 3. Product Goal

The target is a data structure system for Windows, Linux, iOS, and Android under the constrained platform set:

- 64-bit
- little-endian
- mainstream CPU ABIs only

The system must support:

- user-defined types
- nested structs
- strings
- vectors
- contiguous associative containers
- zero encode/decode byte transport
- efficient mutation, including non-inplace growth
- C++26 reflection as the authoritative metadata source

The system does not attempt to support:

- arbitrary native C++ types
- native pointers or reference graphs
- polymorphic wire objects
- schema evolution in `v1`

## 4. Core Architectural Choice

`v1` must not serialize STL or Boost container internals as the wire contract.

That means the current repository approach is a useful prototype, but not the final protocol architecture. The final `v1` design should be:

- user structs remain native reflected C++ aggregates
- dynamic fields use XOffset-owned frozen-layout container types
- wire compatibility is validated by exported layout baselines across the exact target platform set
- load-time schema verification rejects mismatches before exposing typed access

This gives `v1` a better balance than either extreme:

- not pure native-layout snapshotting of third-party containers
- not a fully separate proxy-only IDL runtime

The important consequence is that user code can still look like normal C++:

```cpp
struct Player {
    int32_t id;
    XString name;
    XVector<int32_t> items;
};
```

and `root<Player>()` can still expose `Player&`, provided `Player` is in the admitted subset and passes baseline layout checks on all target platforms.

## 5. V1 Type Admission Rules

Only types that satisfy all rules below are legal wire types.

### 5.1 User record constraints

- must be a non-polymorphic standard-layout aggregate or equivalent reflected record
- no inheritance in `v1`
- no virtual functions
- no virtual inheritance
- no unions
- no bit-fields
- no references
- no raw pointers
- no member pointers
- no custom `alignas` greater than `8`
- no user-defined destructor

### 5.2 Allowed scalar types

- `bool`
- `char`, `signed char`, `unsigned char`
- `std::byte`
- `int8_t`, `uint8_t`
- `int16_t`, `uint16_t`
- `int32_t`, `uint32_t`
- `int64_t`, `uint64_t`
- `float`
- `double`

### 5.3 Explicitly banned scalar types

- `short`, `int`, `long`
- `unsigned short`, `unsigned int`, `unsigned long`
- `wchar_t`
- `char8_t`, `char16_t`, `char32_t` as protocol text primitives
- `long double`
- implementation-defined decimal or extended floating types

### 5.4 Allowed enum types

- only enums with an explicit fixed underlying type
- underlying type must itself be an allowed scalar type

### 5.5 Allowed containers and wrappers

- `XString`
- `XVector<T>`
- `XBlob`
- `XFlatSet<T>`
- `XFlatMap<K, V>`
- fixed-size C arrays of admitted element types

`XOptional<T>`, `XHashSet`, and `XHashMap` are not part of the `v1` MVP. They are phase-2 types after the fixed ABI and allocator are stable.

## 6. Binary Compatibility Model

`v1` compatibility is exact-match compatibility.

At load time, the runtime must validate:

- `magic`
- `format_major`
- little-endian marker
- root type id
- schema hash
- buffer integrity fields
- root and allocator offsets are in range
- every reachable dynamic block is in range and correctly aligned
- every container `size/capacity/data` triple is internally consistent
- the reachable graph stays inside `used_bytes`

If any check fails, the buffer is rejected before `root<T>()` succeeds.

There is no partial load and no best-effort compatibility path in `v1`.

This means `v1` load is not just header validation. It is a bounded structural verification pass over the admitted type graph.

Default `v1` save/load semantics are **normalized transport semantics**, not a
full allocator snapshot:

- admitted object graphs must survive save/load intact
- loaded buffers must remain mutable
- identical post-load spare capacity and fragmentation state are not guaranteed

## 7. Buffer Format

The buffer is a single relocatable byte region:

```text
+-------------------+
| XWireHeader       |
+-------------------+
| AllocatorState    |
+-------------------+
| Root Object       |
+-------------------+
| Dynamic Blocks    |
+-------------------+
| Free Space        |
+-------------------+
```

### 7.1 Header layout

Suggested frozen header:

```cpp
struct alignas(8) XWireHeader {
    char     magic[8];          // "XOFFV1\0"
    uint16_t format_major;      // 1
    uint16_t format_minor;      // 0
    uint32_t header_size;       // sizeof(XWireHeader)

    uint64_t used_bytes;        // bytes that must be serialized
    uint64_t reserved_bytes;    // local capacity after load/grow
    uint64_t root_offset;       // byte offset from buffer start
    uint64_t allocator_offset;  // byte offset from buffer start

    uint64_t root_type_id;      // stable hash of canonical schema type name
    uint64_t schema_hash_lo;    // low 64 bits of schema hash
    uint64_t schema_hash_hi;    // high 64 bits of schema hash

    uint32_t flags;             // crc present, compacted, debug, etc.
    uint32_t endian_tag;        // constant 0x01020304
    uint32_t crc32c;            // optional integrity check
    uint32_t reserved0;
};
```

Notes:

- `schema_hash` is the canonical hash of the reflected root type closure, not just the root record.
- `reserved_bytes` is local runtime capacity and does not affect wire compatibility.
- `used_bytes` is the exact serialized byte count.
- normalized `v1` save paths may legally reduce spare tail capacity before transport.

### 7.2 Allocator state

The allocator state is stored in-buffer so that a loaded buffer remains mutable without decode/rebuild.

Suggested shape:

```cpp
struct alignas(8) AllocatorState {
    uint32_t arena_begin;
    uint32_t arena_end;
    uint32_t free_bytes;
    uint32_t epoch;

    uint32_t freelist_head[16]; // size classes
};
```

The exact number of size classes can be tuned, but the layout must freeze once `v1` ships.

## 8. Relative Reference Model

All dynamic references use relative offsets, never native pointers.

`v1` should use self-relative offsets for dynamic container internals:

```cpp
template <class T>
struct XRel32 {
    int32_t delta; // 0 means null
};
```

Resolution rule:

- address is `reinterpret_cast<char*>(&delta) + delta`

Why `rel32` in `v1`:

- smaller headers
- better cache density
- enough for the intended usage model

`v1` therefore sets a hard runtime limit:

- maximum live buffer size must remain below `2 GiB`

If larger buffers are needed later, add a separate `rel64` profile instead of silently widening `v1`.

## 9. Container ABI

All XOffset containers must have frozen layout owned by this project.

### 9.1 XString

Suggested layout:

```cpp
struct alignas(8) XString {
    XRel32<void> arena;     // points to AllocatorState
    XRel32<char> data;      // points to char buffer
    uint32_t     size;      // bytes, not counting trailing '\0'
    uint32_t     capacity;  // bytes, excluding trailing '\0'
};
```

Properties:

- `sizeof(XString) == 16`
- no SSO in `v1`
- storage is UTF-8 bytes plus trailing `'\0'`
- empty string means `data == null`, `size == 0`, `capacity == 0`

Reason to omit SSO in `v1`:

- ABI stays simple
- move and rebase logic stays simple
- protocol behavior is easier to reason about

If short-string optimization is needed later, add a new container type instead of mutating `XString` ABI.

### 9.2 XVector<T>

Suggested layout:

```cpp
template <class T>
struct alignas(8) XVector {
    XRel32<void> arena;
    XRel32<T>    data;
    uint32_t     size;
    uint32_t     capacity;
};
```

Properties:

- `sizeof(XVector<T>) == 16`
- elements are contiguous
- empty vector means `data == null`

### 9.3 XBlob

`XBlob` is a raw byte vector with the same header shape as `XVector<std::byte>`.

Recommended use cases:

- binary attachments
- compressed payloads
- externalized assets

### 9.4 XFlatSet<T>

`XFlatSet<T>` reuses the same header as `XVector<T>` and enforces:

- sorted order
- unique elements

Complexity:

- lookup: `O(log n)`
- insertion: `O(n)`
- erase: `O(n)`

This is acceptable for read-mostly protocol/state data and keeps the wire layout contiguous and zero-copy friendly.

### 9.5 XFlatMap<K, V>

`XFlatMap<K, V>` stores a sorted contiguous array of:

```cpp
template <class K, class V>
struct XKeyValue {
    K key;
    V value;
};
```

Header shape stays identical to `XVector<XKeyValue<K, V>>`.

Complexity:

- lookup: `O(log n)`
- insertion: `O(n)`
- erase: `O(n)`

## 10. Mutation Model

Mutation must remain efficient without encode/decode.

The rule is:

- mutate in place when capacity is sufficient
- allocate-copy-swap when capacity is insufficient

### 10.1 Growth behavior

Recommended growth policy:

- capacity `< 64 bytes`: grow to at least `8`
- capacity `< 1 KiB`: `2x`
- capacity `< 64 KiB`: `1.5x`
- capacity `>= 64 KiB`: `1.25x`

This keeps small mutations fast without letting large buffers explode.

### 10.2 Reallocation steps

For `XVector<T>` and `XString` growth:

1. allocate a new block from the arena
2. move or copy old payload into the new block
3. patch the container header
4. retire the old block to a free list

### 10.3 Element move rules

Element relocation depends on the type category:

- trivially movable leaf types: byte copy
- records containing containers: reflection-driven move/rebase
- container element types: container-specific move

This is where C++26 reflection remains important. It is the mechanism that allows user-defined nested records to remain usable without hand-written allocators or serializers.

## 11. Allocation Model

The allocator is not a general-purpose heap. It is a mutation-oriented arena with reuse.

### 11.1 Block header

Suggested dynamic block prefix:

```cpp
struct alignas(8) BlockHeader {
    uint32_t payload_bytes;
    uint16_t size_class;
    uint16_t flags;
    uint32_t next_free;
    uint32_t reserved0;
};
```

### 11.2 Allocation strategy

- small allocations use size-segregated free lists
- large allocations use exact-size append or a dedicated large-block list
- no per-object `delete` in the public API
- internal frees happen only as a side effect of container growth/shrink operations

### 11.3 Compaction

`compact<T>()` remains an out-of-place rebuild:

- allocate a new buffer
- rebuild the root object via reflection
- copy live reachable blocks only
- discard holes and retired blocks

This is the correct place for full defragmentation. It should not be hidden behind ordinary mutation APIs.

## 12. User Type Construction Rules

To keep the wire model deterministic, `v1` user types should follow these rules:

- prefer plain aggregate records
- allow default member initializers only after the generated initialization path defines exact semantics
- do not require custom allocator constructors
- do not embed runtime-only pointers, mutexes, file handles, or owning external resources

Construction strategy:

- `make<T>()` zero-initializes storage
- reflection walks fields
- XOffset containers are explicitly initialized with the local `AllocatorState`
- `v1` initial value contract is zero-initialization unless the implementation explicitly adds generated default-initializer support and tests it across all supported toolchains

## 13. Reflection Responsibilities

Reflection is still a central part of the design, but its role is narrower and better defined than in the current prototype.

Reflection is responsible for:

- compile-time admission checks
- field enumeration
- container initialization for nested records
- move/rebase for nested records during vector growth and compaction
- canonical schema manifest generation
- baseline export for cross-platform layout verification

Reflection is not responsible for:

- field evolution machinery
- runtime dynamic schema negotiation
- interpreting arbitrary foreign bytes without a matching schema hash

## 14. Schema Identity

Even though `v1` has no field evolution, it still needs strong schema identity.

Recommended schema identity inputs:

- canonical schema type name
- XOffset-owned wire ABI signature of every reachable admitted type
- container ABI version tags
- root type id

Important constraint:

- `root_type_id` must not be derived from compiler-specific pretty names or raw reflection display strings

For `v1`, the type identity source should be one of:

- an explicit user-declared schema name literal such as `"game.Player"`
- a repository-defined normalized qualified name generator that is proven stable across supported toolchains

`v1` should start with the simpler rule:

- every root wire type must declare an explicit canonical schema name

The canonical manifest should be built in a deterministic order:

- sort type entries by canonical name
- hash the normalized text or binary manifest

Recommended behavior:

- `load<T>()` validates the embedded schema hash against the local compile-time constant
- mismatch throws or returns an explicit error code

## 15. Load / Save API Shape

Suggested public surface:

```cpp
class XBuffer {
public:
    template <class T>
    static XBuffer create(std::size_t reserve_bytes = 4096);

    template <class T>
    T* make_root();

    template <class T>
    bool matches_schema() const;

    template <class T>
    T& root();

    std::span<const std::byte> bytes() const;
    std::string save() const;

    template <class T>
    static expected<XBuffer, LoadError> load_verified(std::span<const std::byte>);

    bool grow(std::size_t extra_bytes);
    void shrink_to_fit();
};
```

Key differences from the current prototype:

- load is verified, not blind
- root access is schema-guarded
- the wire contract is explicit

Optional escape hatch for trusted data:

- the implementation may later add `load_unverified()` for benchmark and controlled internal use
- `root<T>()` on the ordinary path must remain gated by verified load success

## 16. Handle and Pointer Stability

The invalidation model remains:

- raw pointers/references into the buffer may dangle after whole-buffer growth
- raw pointers/references into container payloads may dangle after that container mutates

`XHandle<T>` should remain as the stable public abstraction for root access across buffer relocation.

For container elements, `v1` should not promise stable references across mutation.

## 17. Performance Expectations

Expected behavior relative to STL/native code:

- scalar field reads and writes: effectively identical
- root record access: identical syntax and near-identical codegen
- `XVector<T>` on trivially movable `T`: close to `std::vector<T>`
- `XString`: slower than highly optimized `std::string` for tiny strings because `v1` omits SSO
- `XFlatMap/XFlatSet`: faster iteration and better wire density than tree containers, slower insertion than hash/tree structures

This is a good trade for zero-copy state transport and mutable in-buffer data.

## 18. Cross-Platform Verification Strategy

The target matrix for the project goal is:

- Windows x86_64
- Linux x86_64
- iOS arm64
- Android arm64

The project must commit exported baselines for all supported targets before claiming protocol support for that matrix.

Verification layers:

1. compile-time type admission
2. per-target signature export
3. CI cross-target baseline comparison
4. load-time schema hash verification
5. runtime round-trip and mutation tests

The important rule is:

- support is claimed only for target pairs with committed passing baselines

Repository implementation note:

- the project now treats the target matrix as an exact committed set, not an open-ended
  collection of ad hoc local exports
- `tools/export_signatures.cpp` rewrites the exact target set baselines
- `tools/check_signature_matrix.cpp` rejects missing or unexpected baseline files
- current repository baselines are still repository-managed synthetic baselines,
  not yet independently harvested native target observations
- the strongest target-matrix claim therefore still requires native target export
  evidence or another proven-equivalent generation path

## 19. Toolchain Reality

The design assumes C++26 reflection, but the current compiler ecosystem is still uneven.

Pragmatic implementation rule:

- keep reflection as the source of truth for metadata generation and validation
- if some target toolchains lag, generated manifests may be checked into the repository as build artifacts until all target compilers catch up

This does not change the wire design. It only affects the build path.

## 20. Implementation Phases

### Phase 1: Lock the fixed-schema contract

- freeze `v1` scope with no field evolution
- introduce verified load header
- define canonical schema naming for root types
- define admitted type rules
- define schema hash
- implement structural validation rules

### Phase 2: Replace third-party container ABI dependency

- implement frozen-layout `XString`
- implement frozen-layout `XVector<T>`
- implement `XBlob`
- implement reflection-based container init and nested move/rebase

### Phase 3: Contiguous associative containers

- implement `XFlatSet<T>`
- implement `XFlatMap<K, V>`
- add lookup and mutation tests

### Phase 4: Arena reuse and compaction

- add free-list allocator metadata
- implement retired block reuse
- harden `compact<T>()`

### Phase 5: Platform closure

- export baselines for Windows, Linux, iOS, Android
- make cross-platform CI gating mandatory

### Phase 6: Specialization and performance trimming

After the fixed-schema runtime is correct and the target platform set is closed, the
next line of work should explicitly exploit the fact that this project is not trying to
be a general-purpose Boost replacement.

- remove compatibility-only generic hooks kept from the Boost-based prototype
- collapse runtime object lookup to the actual `v1` needs: one root object and one
  allocator state
- specialize backend allocation for admitted alignments and expected container growth
  patterns instead of keeping broad managed-memory behavior
- replace generic free-block scans with size-class structures tuned for `XString` and
  `XVector` mutation workloads
- benchmark the current backend against the earlier Boost-based runtime and keep only
  the fallback paths that still justify their cost
- introduce specialized optional container variants only when they outperform the
  current general frozen containers without destabilizing the `v1` ABI

## 21. Final Recommendation

For `v1`, the correct design choice is:

- drop field evolution
- keep exact-schema compatibility only
- define XOffset-owned frozen-layout containers
- preserve native-looking user structs
- verify layout equality across the exact supported platform set
- reject mismatched bytes at load time

This is the narrowest design that still satisfies the real target:

- zero encode/decode transfer
- efficient direct reads and writes
- mutable containers
- practical cross-platform support

Trying to keep field evolution in `v1` would push the project toward a different architecture entirely: table/directory-based wire objects, proxy accessors, and a much more complicated runtime. That can be a later line of work, but it should not block the fixed-schema zero-copy core.
