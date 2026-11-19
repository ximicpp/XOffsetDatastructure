# XOffsetDatastructure User Guide (4.1.0, wire format 2.1)

This guide is for users of the library. It explains how to define types, build,
modify, save and load them, and the constraints you must follow.
If this is your first time, start with [GETTING_STARTED.md](GETTING_STARTED.md): it has one runnable
example of the recommended pattern per use case (server to client, client uplink, server to server).
Deployment requirements are in [DEPLOYMENT.md](DEPLOYMENT.md), format details in [WIRE_FORMAT.md](WIRE_FORMAT.md),
and the input and compatibility boundary in [ALPHA_COMPATIBILITY.md](ALPHA_COMPATIBILITY.md).

## 1. Basic model

- An `XBuffer` owns one contiguous arena. The root object and every string, array and set it
  references live in that arena and point at each other with relative offsets, so the arena
  works at any address (storage: section 3.4).
- The arena bytes are the serialized form; reading and writing fields needs no encoding or decoding.
- `save_verified` copies the arena into a wire (`std::string`) owned by the caller; the sender
  can keep modifying the original buffer afterwards.
- `load_verified` first validates the whole wire, then copies the bytes into a new arena. Once
  validation passes, reading is plain member access.
- So sending needs no encode or decode, but it is not zero-copy: the sender copies once when
  saving and the receiver copies once when loading (the receive entry point that reads straight
  into the final arena is that same single copy).

## 2. Defining types

```cpp
#include "xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

struct Profile {
    std::int32_t           id{0};
    XString                name;     // strings of <= 10 bytes are stored inline in the 12-byte header
    XString                bio;      // longer strings take one arena block
    XVector<std::int32_t>  scores;
    XSet<std::int32_t>     tags;
    XMap<XString, std::int32_t> stats;
};
XOFFSET_RECORD(Profile, id, name, bio, scores, tags, stats)   // field list (section 2.1)
XOFFSET_REGISTER_SCHEMA_NAME(Profile, "example.Profile")
```

- Plain aggregates are enough; there is no IDL. Write one `XOFFSET_RECORD` line at global scope
  for every record (section 2.1): C++20/C++23 builds (clients) learn the record's shape from it,
  and C++26 builds check it entry by entry against reflection, so the same source compiles in both
  builds with identical wire bytes, and every field gets an accessor of the same name (section 3.5).
  Code compiled only as C++26 may omit it; layout and schema identity then come from P2996 reflection.
- Members may only be scalars, enums, fixed-size arrays, library containers (`XString`, `XVector`,
  `XSet`, `XMap`) and structs that recursively satisfy these rules. Raw pointers,
  references, `union`, polymorphic types, `std::string`/`std::vector` and the like are rejected at
  compile time.
- Register a unique and stable name for each root type with `XOFFSET_REGISTER_SCHEMA_NAME`.

### 2.1 Field lists: `XOFFSET_RECORD`

In builds without reflection (C++20/C++23, or `XOFFSET_SCHEMA_BACKEND_DESCRIBE=1`) the library
learns a record's shape from a one-line field list. C++26 builds may omit it; if it is written,
the macro checks it against reflection entry by entry (members, order, offsets, types, bases) and
fails to compile on any mismatch, so the same source describes the same schema in both builds and
the wire bytes are identical.

```cpp
struct Troop { std::int32_t type{}; std::int32_t count{}; };
struct March { std::int64_t id{}; std::int32_t eta{}; XVector<Troop> troops; };
struct Entity { std::int64_t id{}; };
struct Player : Entity { std::int32_t level{}; XVector<std::int32_t> items; };

// Global scope, after the type definitions and before first use; contained records come first.
XOFFSET_RECORD(Troop, type, count)
XOFFSET_RECORD(March, id, eta, troops)
XOFFSET_RECORD(Entity, id)
XOFFSET_RECORD_DERIVED(Player, (Entity), level, items)   // bases in parentheses, in declaration order
```

- List **all** non-static data members in declaration order. A misspelled name does not compile;
  a missing, duplicated or misordered member is caught by a compile-time completeness proof:
  offsets strictly increasing and non-overlapping, plus the aggregate initialization count
  (`T{x1..xE}` is valid and `T{x1..xE+1}` is not). `sizeof` alone is not enough: leaving out a
  1-byte member that looks like padding does not change the size.
- Records must therefore be aggregates (no user-declared constructors, private or protected
  members, or virtual functions). Use an alias when a type name contains a comma.
- `XOFFSET_RECORD` also declares the record ordinary (section 3.1); do not write both. Without
  reflection that is a promise that the record has no user-declared destructor, copy/move constructor
  or assignment, which the library can only partly check; C++26 builds check it fully. A record that
  cannot make the promise uses `XOFFSET_RECORD_DERIVED(T, (), members...)` (field list only), and then
  middle inserts, erases and growth of an `XVector` of it move elements one by one instead of one
  `memmove`.
- Enums on the wire must have a fixed underlying type (`enum class`, or
  `enum E : std::uint8_t {...}`); otherwise compilation fails with a suggested fix (section 2.2).
- `XString`, `XVector`, `XSet`, `XMap`, `XKeyValue` and `std::array` come with library field lists.
- Unions and non-aggregate classes (`std::string`, classes with virtual functions or constructors)
  cannot have a field list. Without reflection the library treats them as opaque: not admitted
  (`is_byte_copy_safe_v` is false), and no "missing field list" error.
- `tools/describe_dump` (built with GCC 16 or P2996) prints these lines for every record reachable
  from a root type, in dependency order.
- Before release, compile the schema at least once with a C++26 compiler so reflection checks the
  field lists and the promises.
- The field list also generates by-name accessors on the record's proxy (section 3.5): `u.hp()` is
  `u.field<&Unit::hp>()`. Records described only by reflection, with no field list, have no
  accessors; use `field<&T::m>()`.

### 2.2 Portable scalars

To exchange data between Linux, Windows, Android, iOS and macOS (all 64-bit little-endian), use
only the types in the left column in a schema.

| Use | Do not use | Why |
| --- | --- | --- |
| `std::int8_t` … `std::int64_t`, `std::uint8_t` … `std::uint64_t` | `long`, `unsigned long`, `std::size_t`, `std::ptrdiff_t`, `std::intptr_t`, `std::time_t` | Width or underlying type varies by platform: `long` is 32-bit on Windows; `std::size_t` is `unsigned long` on Linux and `unsigned long long` on Windows. On a platform where they happen to be `std::int64_t` or `std::uint64_t` they compile; on another platform compilation fails (for example `long` on Windows and Apple platforms, `std::size_t` on Apple platforms). The library cannot catch this on Linux, where they are the same types as the fixed-width integers |
| `float`, `double` | `long double` | 16 bytes on Linux x86-64 and MinGW, 8 bytes on MSVC and Apple platforms; rejected at compile time |
| `bool`, `std::byte` | `wchar_t`, `char16_t`, `char32_t` | `wchar_t` is 2 bytes on Windows and 4 bytes elsewhere; all of these are rejected at compile time |
| `char` for plain fields; `std::int8_t`, `std::uint8_t` or `XString` for set and map keys | Keys ordered by `char`: `XSet<char>`, `XMap<char, V>`, and arrays or record members containing `char` in a key, enums whose underlying type is `char` | `char` is signed on x86-64, Apple and Windows and unsigned on Linux and Android arm64, so the two sides order keys differently and reject each other's wire, while the schema hash is the same and the handshake cannot notice; rejected at compile time. `XString` compares as unsigned bytes on every platform and can be a key |
| Enums with an explicit underlying type: `enum class E { ... }`, `enum E : std::uint32_t { ... }` | `enum E { ... }` without an underlying type | The compiler picks the underlying type: `int` on MSVC, `unsigned int` on GCC and Clang when there are no negative values, so the schemas differ; rejected at compile time |

## 3. Building and modifying

The recommended pattern (also used by the README and `examples/helloworld.cpp`):

```cpp
auto cap = XBuffer::maximum_capacity(32u << 20);   // hard limit
auto buf = TypedXBuffer<Profile>::create(4096, cap);
auto p = buf.root();                    // proxy (XRef<Profile>): stays valid when the buffer moves (section 3.5)
p.id() = 7;
p.name() = "Alice";
p.bio() = "a much longer biography text";
p.scores().push_back(90);
p.tags().insert(3);
p.stats().insert_or_assign("hp", 100);

buf.edit([](Profile& r) {               // hot loop: plain references inside the pinned scope, as fast as a plain struct
    std::vector<std::int32_t> v{1, 2, 3, 4};
    r.scores.assign(v.begin(), v.end());   // contiguous and trivially copyable: one exact allocation
});
if (!buf.ok()) { /* buf.error(): reason for the first failure; failures do not throw */ }
```

Without the wrapper, `XBuffer::create<Profile>(initial, cap)` and `buf.root<Profile>()` give a
plain reference: no proxy protection, it dangles when the buffer moves, and running out of space
throws. The growth rules below describe this form.

- The default growth policy is `XGrowPolicy::Explicit`: running out of space throws `XBadAlloc`.
  Choose `XGrowPolicy::Automatic` explicitly when you need automatic growth. `maximum_capacity` is
  always a hard limit.
- One payload is at most 2 GiB - 8 bytes (`XBufferCore::MAX_PAYLOAD`; offsets inside a payload
  are 32-bit). If `maximum_capacity` exceeds it, creating the buffer fails immediately with
  `XBadAlloc`, error code `capacity_exceeded`, and a message stating the limit. Split larger data
  across several buffers.
- Automatic growth usually doubles the logical length; after `reload_verified`, within the range
  used before, it grows by only 4 KiB at a time (re-exposed bytes must be zeroed, and zeroing on
  demand is cheaper), and goes back to doubling past the previous high-water mark. The logical
  length only affects `get_size()`, not the wire.
- Container headers must live inside their arena. Pass native values such as `std::string_view`
  or `std::string` to insert functions; do not build arena-bound `XString` temporaries on the stack.
- Growth, insertion and erasure move elements under the same rules as `std::vector`; `XSet` and
  `XMap` are sorted flat arrays with the same rules as `std::flat_set` and `std::flat_map`,
  **not like `std::set` and `std::map`**. Per-operation rules are in section 3.2. Automatic growth
  and container operations do not move the arena; an explicit `grow()` may move it on a movable
  slot (section 3.4).
- Declare record types used as `XVector` elements ordinary (section 3.1); otherwise middle
  insert/erase and growth move elements one by one.
- What you can and cannot change (schema, writers, ownership) is in section 3.3.
- The typed wrapper `TypedXBuffer<T>` has `create`, `root()`, `edit`/`read`, `save_verified()` and
  `load()` (the validating path). `root()` returns a proxy (`XRef<T>`), not a plain reference; see
  section 3.5. When you need a plain reference use `edit`/`read`, or `plain_root()`.

### 3.1 Erase and insert performance

Erasing or inserting in the middle of an `XVector` shifts all following elements, so erasing one by
one in a loop is O(n·k).

- For bulk erasure use `stable_erase_if`: one pass, original order kept, each kept element moved at
  most once.
- When elements are record types with `XString`/`XVector`/`XMap` members, declare those record
  types (and the record types they use as members) with `XOFFSET_ORDINARY_RECORDS`. This enables a
  faster path: middle `erase`/`insert`, append growth and `reserve` move the whole element range
  with one memmove/memcpy and only rebuild the inline container headers; `stable_erase_if` becomes
  available too. The declaration is a promise about the **source**: the record has only ordinary
  non-static members, no bases, `union`, bit-fields or `[[no_unique_address]]`, and members do not
  overlap.

```cpp
struct Troop { std::int32_t type{}; std::int32_t count{}; };
struct March {
    std::int64_t id{};
    std::int32_t eta{};
    XVector<Troop> troops;
};

// Global scope, after the type definitions and before first use in a container.
XOFFSET_ORDINARY_RECORDS(Troop, March)

marches.stable_erase_if([](const March& m) { return m.eta <= 0; });
```

- The macro checks each type at compile time: it must be a class (not a `union`), with no bases or
  bit-fields, and the members' `[offset, offset + sizeof)` ranges must be pairwise disjoint.
  `[[no_unique_address]]` members that share storage are detected; members that "could overlap but
  do not this time" cannot be, so the declaration remains a promise about the source, and a wrong
  declaration is undefined behavior. The library never assumes it without a declaration.
- The macro expands to two specializations, `wire_record_layout<T>::ordinary_members = true` and
  `StableEraseOrdinaryMembers<T>`; writing them by hand has the same effect but no checks.
- Without the declaration, behavior and results are identical; only the slower element-by-element
  path is taken, and `stable_erase_if` is unavailable (compile error). Both paths produce the same
  wire bytes.

### 3.2 When references and iterators are invalidated

The library does not track references to elements for you; as with std containers you reason per
operation. Addresses come in three levels:

| Level | What | Example |
| --- | --- | --- |
| Element | Element objects in a container, including container headers nested in elements | `p.scores[0]`, `marches[3]`, the header `marches[3].troops` itself |
| Child data | Blocks owned by an element | The characters `p.bio.view()` points at, the element array of `marches[3].troops` |
| Region | The whole payload: the root object and everything reachable from it | `buf.root<Profile>()` |

**Container operations only affect that container.** Allocation takes new blocks only from free
blocks or the end of the region, and freeing only records blocks in the free directory; elements and
child data of other containers never move.

| Operation | References, pointers, iterators to this container's elements | Compare |
| --- | --- | --- |
| `push_back`, `emplace_back` | All valid without growth; all invalidated on growth | Same as `std::vector` |
| `insert(pos, …)` | `pos` and later invalidated; all on growth | Same as `std::vector` |
| `erase(pos)`, `pop_back()` | The erased element and later invalidated | Same as `std::vector` |
| `stable_erase_if(pred)` | From the first erased position on | Same as `std::erase_if` |
| `reserve(n)`, `resize(n)` beyond capacity | All invalidated | Same as `std::vector` |
| `resize(n)` within capacity | Shrinking invalidates element n and later; growing keeps all valid | Same as `std::vector` |
| `clear()`, `assign(…)` | All invalidated | Same as `std::vector`; `clear()` keeps capacity. The old elements' bytes stay in the slack by default; `XOFFSET_SCRUB_ERASED=1` zeroes them (DEPLOYMENT section 4.1) |
| `XString` assignment, `clear()`, `reserve()` | Previously obtained `view()`, `data()`, `c_str()` invalidated | Same as `std::string` |
| `XSet` `insert`, `emplace`; `XMap` `try_emplace`, `emplace`, and `insert_or_assign` / `operator[]` inserting a new key | Insert position and later invalidated; all on growth | Same as `std::flat_set`, `std::flat_map`; **unlike `std::set`, `std::map`** (where inserts do not affect other elements) |
| `XMap` `insert_or_assign` on an existing key | Other elements unaffected | Same as std |
| `XSet`, `XMap` `erase(key)` | The erased element and later invalidated | Same as `std::flat_map`; unlike `std::map` |
| Reading or changing existing element values (container length unchanged) | All valid | Same as std |
| Move assignment within one buffer, `a = std::move(b)` | `b`'s elements do not move; they now belong to `a`, and `b` becomes empty; `a`'s old elements are invalidated. Across buffers it is a copy and `b` is unchanged | Same as `std::vector` move assignment |

- Growth can sometimes happen in place (when the container's block is at the end of the region),
  and then addresses do not change. That is not promised; write code as if "growth invalidates
  everything".
- When an element moves, the child data it owns usually stays put: the container header is rebuilt
  at the new position and still points at the original block. This is also an implementation detail,
  like small-string optimization in `std::vector<std::string>`; do not rely on it.
- `XSet`/`XMap` lookups are O(log n) and inserts and erases O(n); compare them with
  `std::flat_set`/`std::flat_map`, not the node-based `std::set`/`std::map`.
- To build an `XSet`/`XMap` from input that is already sorted by key with no duplicates (a
  `std::map`, a sorted `std::vector`, a range or iterator pair), call `assign_sorted(range)`: it
  replaces the content with one exact-size block and no lookups, and a contiguous range of the
  container's own element type is copied in one go. Input out of order is checked only when
  `XOFFSET_CHECK_SORTED` is 1 (the default in builds without `NDEBUG` and with `XOFFSET_HARDENED=1`),
  which throws `XErrc::invalid_argument` and leaves the container unchanged; otherwise lookups on
  the result miss.

**Buffer operations affect the whole region.** The epoch is a counter on the buffer object;
`XHandle` re-resolves the root when it sees the epoch change.

| Operation | Root and region address | References, spans, iterators into the payload | Epoch |
| --- | --- | --- | --- |
| Automatic growth triggered by container operations; explicit `grow()` within current storage | Unchanged (extends within the reservation or slot) | Invalidated only per the table above | Unchanged |
| Explicit `grow()` past a movable slot (section 3.4) | Changes: moves to new storage | All invalidated | +1 |
| `save_verified`, `save_verified_into`, `save` | Unchanged (only shrinks the logical length) | Valid | Unchanged |
| `reload_verified`, `begin_verified_reload`, `reset<T>()` | Unchanged (mapping reused); a movable slot that cannot hold the new content switches storage (section 3.4) | All invalidated (`begin_verified_reload` invalidates at the start, not at `finish()`) | +1 |
| `src.clone_into(dst)` | dst unchanged (mapping reused); a movable-slot dst that cannot hold it switches storage (section 3.4) | Everything previously in dst invalidated; `src` unaffected | dst +1 (unchanged when a self-copy is rejected) |
| `clone()` | The result is a new buffer with the same storage kind as the source (movable slot or address-stable) | Source unaffected | New buffer starts at 0 |
| `load_verified`, `load_unverified`, `XCompactor::compact` | The result is a new buffer | Source unaffected (compaction only reads the source, also when retrying with a larger capacity) | New buffer starts at 0 |
| `XBuffer` move construction, move assignment, `swap` | Unchanged; storage moves with the object | Still valid, but now belong to the other object | Both +1 |
| Destruction | Released | All invalidated | n/a |

- `XHandle` compares the epoch on every dereference (two loads and a compare, inlined); it
  re-resolves (not inlined) only when the epoch changed. When handling several events from one
  packet, **resolve once per packet**: `auto& book = *handle;`, and use that reference inside the
  packet (no operations that move or reload the buffer inside the packet). Dereferencing per event
  cost about 7 more instructions per event in the order book test and keeps the compiler from
  inlining the caller's loop.
- `XHandle` is bound to the buffer object, not to the payload: after a move the original object is
  empty and an `XHandle` bound to it resolves to null; after `swap` it resolves to that object's
  current content. To follow the data, create a new `XHandle` on the new object.
- The payload contains no absolute addresses, so between any two operations the region's bytes can
  be copied elsewhere as a whole and used there (that is what save and load do); but the copy is a
  new buffer and references at the old address do not point into it.

### 3.3 What you can and cannot change

You can: write any field; make variable-length changes, that is, add, remove and change strings,
arrays, sets and maps; reuse free blocks; keep modifying after receiving (`load_verified`) and send
again.

Three things you cannot do; they are limits of this kind of data structure:

1. **You cannot change the schema.** Adding, removing, reordering or changing any field makes old
   wires rejected (section 7). Update the type definition on both ends together.
2. **One buffer has one writer at a time.** The library does not lock; writes must be mutually
   exclusive with other reads and writes on that buffer, synchronized by the caller (section 5).
   Several processes reading and writing one buffer through shared memory at the same time is not
   supported.
3. **Ownership is a tree.** Each block belongs to exactly one container header: two containers
   cannot share child data, objects cannot reference each other, and there are no cycles. Raw
   pointers and references are rejected at compile time (section 2); to express a graph, store
   element indices or IDs (for example a `std::uint32_t` index, or an `XMap` key). Copy assignment
   is a deep copy; move assignment within one buffer transfers ownership. **Do not move-assign a
   container into a container in its own subtree, or the other way round**: Release builds do not
   check this, and the result is undefined behavior.

### 3.4 Storage: VM, pinned slot, movable slot

When a buffer is created (`create`, constructors, `load*`) the storage is chosen from the
parameters; `storage_kind()` reports it:

| Storage | Chosen when | Address |
| --- | --- | --- |
| VM (`XStorageKind::Vm`) | With an `XBufferPool`; `with_storage(XStorage::Pinned)`; Automatic with a limit above `XBuffer::SLOT_MAX`; initial size above `SLOT_MAX` | Fixed for the whole lifetime |
| Pinned slot (`PinnedSlot`) | Limit not above `SLOT_MAX` (any growth policy) | Fixed for the whole lifetime |
| Movable slot (`MovableSlot`) | Everything else: Explicit growth (the default), a larger limit, an initial size that fits a slot | Changes only at the explicit points below |

- `SLOT_MAX` defaults to 256 KiB − 64 B and can be changed at compile time with
  `XOFFSET_SLOT_MAX_BYTES` (a power of two; 0 disables slots). Slots come from a slab inside the
  library, in power-of-two classes, so small messages no longer reserve a range of virtual address
  space per buffer; creating and destroying a slot takes no system call.
- A movable slot moves at these points: `grow()` past the current slot moves to a larger slot
  (past `SLOT_MAX` it moves to VM and is pinned from then on); `reload_verified` and
  `begin_verified_reload` switch storage first when the incoming content does not fit (without
  copying the old content); `set_growth_policy(XGrowPolicy::Automatic)` first moves to VM, because
  automatic growth happens inside container operations, where moving is impossible.
- After a move, previously obtained root references, element pointers, `allocator<T>()`, `arena()`
  and borrowed spans are all invalid, and `epoch()` changes. Use `XHandle` (relocates
  automatically) or call `root<T>()` again. When `address_stable()` is true the buffer never moves;
  write `with_storage(XStorage::Pinned)` when you need an address that never changes.
- Automatic growth, container operations, `save*`, `shrink_to_fit` and `reset<T>()` never move.
- Do not call `grow()` from a host callback of the same buffer (`new_handler`, the predicate of
  `stable_erase_if`).
- Released slots stay in the process for the next buffer to reuse (zeroed before being exposed
  again) and count toward resident memory like a pool; `XBuffer::trim_slot_storage()` drops the
  pages of free slots and `XBuffer::slot_storage_stats()` reports usage.
- The slot free list, the host free list and the owner lookup each have a per-thread cache. When
  GCC targets MinGW/Cygwin, TLS is emulated (emutls) and caches may be freed before destructors
  run at thread exit, so on those targets the caches are off and everything uses the global
  structures (`XOFFSET_THREAD_CACHES` defaults to 0 there and to 1 elsewhere; Clang targeting
  MinGW uses native TLS and keeps the caches).
- A default-constructed empty `XBuffer` can go straight to `reload_verified` or
  `begin_verified_reload`; storage is created by the rules of `load_verified` without a limit
  (64 MiB, Explicit).

### 3.5 Buffers that move: proxies, field accessors, pinned scopes

A buffer from `XBuffer::relocating()` first reserves 1 MiB and moves to a larger reservation when
that is used up, so plain references can dangle. There are two safe ways to access it:

```cpp
struct Unit { std::uint32_t id; std::int32_t hp; XString name; XVector<std::int32_t> items; };
XOFFSET_RECORD(Unit, id, hp, name, items)
struct View { std::uint64_t tick; XVector<Unit> units; };
XOFFSET_RECORD(View, tick, units)

auto world = TypedXBuffer<View>::create(4096, XBuffer::relocating());
auto v = world.root();                     // XRef<View>
auto u = v.units().emplace_back();         // XRef<Unit>; the buffer may move, u stays valid
u.id() = 7;
u.hp() -= 3;
u.name() = "unit-7";
u.items().push_back(42);
std::int32_t hp = u.hp();                  // read: converts to a value
for (auto x : v.units()) total += x.hp();  // iterators are proxies too
if (!world.ok()) { /* world.error(): reason for the first failure */ }

world.edit([&](View& w) {                  // pinned scope: plain references, as fast as a plain struct
    for (Unit& x : w.units) x.hp -= dmg;
});
```

- **Proxy (`XRef<T>`)**: returned by `TypedXBuffer<T>::root()`, `reset()` and `XBuffer::ref<T>()`.
  It remembers the buffer's control block and a path (root offset; container + index; map + key),
  compares a version number on every access, and stays valid when the buffer moves (including when
  the `XBuffer` object itself is moved); after the buffer is destroyed or its content is replaced by
  `reset`/`reload` it becomes stale: reads give value-initialized results and writes return false.
- **Field accessors**: `XOFFSET_RECORD` / `XOFFSET_RECORD_DERIVED` generate a member function of the
  same name for each listed field that returns the field's proxy; when the bases also have field
  lists, derived records get accessors for the base fields too. When a field name equals one of
  the proxy's own functions (`get`, `set`, `field`, `member`, `valid`, `error`) the proxy's function
  wins; use `field<&T::m>()` for that field. Proxies of C array fields have `size()`, subscripts and
  iterators; out of range gives an empty proxy. Accessors are ordinary member functions with no
  reflection or exceptions, and work on MSVC and in no-exception builds.
- **Failures do not throw or crash**: an allocating operation that cannot be done has no effect,
  returns false (or an empty proxy), and records the first error in the buffer (`error()`:
  `capacity_exceeded`, `out_of_storage`, `borrowed`, `stale`, `out_of_range`); later allocating
  operations fail immediately and the buffer refuses to send until `clear_error()`.
- **Pinned scope** `edit(f)` / `read(f)`: f gets plain references and runs as fast as a plain
  struct. The buffer does not move inside the scope (operations that would need to move fail and
  record `borrowed`). Containers keep their own invalidation rules (table above, same as
  `std::vector`): after growth or erasure, earlier references must not be used, and reclaimed blocks
  may be reused by another container right away. When compiled with `XOFFSET_PIN_QUARANTINE=1`,
  reclaimed blocks are reused only after the scope ends, so invalidated references still read the
  old content. f must not return a pointer, reference, `string_view` or `span` (checked at compile
  time). Use it on hot paths and to hand data to existing code that takes `T&`.
- `plain_root()` is the old `root()`: an unprotected plain reference that dangles when the buffer
  moves. Use it only on storage that never moves (pinned slot, VM, Pinned), or not at all.

## 4. Saving and loading

```cpp
std::string wire = buf.save_verified();                 // without the wrapper: buf.save_verified<Profile>()

auto in = TypedXBuffer<Profile>::load(wire, cap);       // full validation first; same as XBuffer::load_verified<Profile>(wire, cap)
std::int32_t id = in.root().id();                       // a proxy read gives a value
in.read([](const Profile& r) { print(r.name.view()); });   // read plain members inside a pinned scope
```

- A wire is the buffer's image: a 40-byte header, then the payload with its free blocks (a sparse
  wire has an extent table after the header; see [WIRE_FORMAT.md](WIRE_FORMAT.md) section 3). By
  default (`XWireEmission::automatic`) a buffer whose free blocks are at least 512 bytes and an
  eighth of the image (after many edits) is sent as its compact wire (`save_compact`, below):
  the same contents, laid out afresh, faster to send and receive than the churned image. Spare
  capacity counts too: a vector that grew to at least 1 KiB of capacity and then shrank to a
  quarter of it or less (`pop_back`, `erase`, `resize`, `clear`, `stable_erase_if`, a shorter
  assign) is remembered by its buffer (up to 8 of them, on the host only), and its spare is
  added to the free bytes for this choice. Any other buffer is sent as its image; only the
  bodies of free blocks in runs that are "at least 4 KiB long and made of free blocks of at least
  1 KiB" are skipped (for example the hole left by deleting many records at once), and the rest
  is copied in one go.
  `XSaveOptions{.emission = XWireEmission::free_runs}` always sends the image, and
  `XSaveOptions{.emission = XWireEmission::dense}` forces a full copy of it.
- When sending across machines you can turn on zero-run omission:
  `save_verified<Profile>(XSaveOptions{.emission = XWireEmission::zero_runs})`
  (`save_verified_into` takes the same option). It skips all-zero runs of at least 64 bytes
  (container slack, holes, zero fields); the receiver fills zeros at the original offsets, the
  loaded payload is byte-identical to a dense wire, and slack is kept, so further edits are not
  slower. The cost is one extra scan of the payload when sending; when the payload is under 4 KiB
  or the saving is less than max(256 B, 1/16 of the payload) it falls back to dense. Keep the
  default for same-machine shared memory or in-process transfer. The receiver needs no setting and
  accepts every wire kind.
- To make the wire smaller, use `save_compact<Profile>()` (or `save_compact_into<Profile>(out)`):
  it walks the object graph in validation order and writes a compact wire (no holes) directly,
  without a temporary buffer, and leaves the source buffer unchanged. By default containers
  (except strings) with at least 8 elements get size/8+1 elements of slack, so the receiver does
  not have to move them as soon as it modifies them. Read-only or archiving receivers pass
  `XCompactOptions{.slack_div = 0}`: capacity equals length and the wire is smallest,
  byte-identical to `XCompactor::compact<Profile>(b).save_verified<Profile>()`, but the receiver
  must move any container the first time it grows it.
- When forwarding received data to a third party (for example a server forwarding client A's uplink
  message to client B), rebuild it with `save_compact` before sending; do not `load_verified` and
  then `save_verified` directly: the latter carries whatever bytes A wrote into holes and container
  slack to B unchanged. The rules and what `save_compact` does not cover are in
  [DEPLOYMENT.md](DEPLOYMENT.md) section 4.2.
- To assemble one message from several pieces of existing state, use the composing overload of
  `save_compact_into` instead of assigning first:

  ```cpp
  auto& msg = req.root<BattleMsg>();          // draft; scalars are set directly as usual
  msg.battle_id = id;
  req.save_compact_into<BattleMsg>(out, {}, xassign(msg.attacker, att), xassign(msg.defender, def));
  ```

  The wire written equals the result of `msg.attacker = att; msg.defender = def;` followed by
  `save_compact_into` (both tight and default slack), but the draft, `att` and `def` are unchanged
  and nothing is allocated in the draft. Rules: each target is a member subobject or array element
  of the root object (any depth, scalars allowed), not an element inside a container; targets must
  not overlap; a source has the same type as its target and may live in any buffer, or be another
  part of the draft itself (all sources are read with their values from before the call); a source
  containing containers must be a valid object in some XOffset buffer, which the sender does not
  validate. Implicit padding bytes of a target are written as 0. Breaking a rule or exceeding the
  wire limit throws `XException` and leaves `out` empty.
- Validation on load covers: header and version, schema identity, the free directory and its free
  blocks, a full graph walk from the root, and "live blocks + free blocks exactly tile the used area". If any check fails it throws
  `XException` and the root is never published. To avoid exceptions call `try_load_verified`; see
  section 6.1.
- A loaded buffer can be modified and saved again; free space from before the save comes along in the
  image (the free blocks and their directory) and keeps being reused.
- `load_unverified` skips validation and is only for data the caller already trusts.

### 4.1 Many small messages: slots, or reuse mappings with `XBufferPool`

Small messages with Explicit growth (the default) land on slots (section 3.4) and need no system
call per buffer. A VM-backed buffer (Automatic growth with a large limit) reserves and releases its own
mapping, which costs microseconds per message; such senders and receivers should share a pool so a
destroyed mapping is reused by the next message:

```cpp
XBufferPool pool({16, 16 * cap_bytes, 64u << 20});   // caches at most 16 free mappings
const auto cap = XBuffer::maximum_capacity(cap_bytes).with_pool(pool);

XBuffer in = XBuffer::load_verified<Msg>(wire, cap);  // still fully validated
// ... use in; when it goes out of scope the mapping returns to the pool instead of the OS
```

- The pool only reuses mappings; it skips no validation and does not change the wire. Reused
  mappings are zeroed before being exposed again.
- The pool matches on the exact `maximum_capacity`; use the same limit for the same kind of message.
- Free mappings in the pool still hold virtual address space and touched pages; the three `Limits`
  (block count, reserved bytes, accessible bytes) cap how much is kept; `pool.clear()` releases
  everything immediately.
- `XBufferPool` is currently an experimental interface (see section 8).

### 4.2 Whole-buffer copies in one process: `clone()` / `clone_into()`

When you need an independent copy in the same process (forking state, rollback, continuing to edit
after a snapshot), there is no need to go through a wire:

```cpp
XBuffer snapshot = buf.clone();     // new owner: size, maximum_capacity, growth policy, pool and storage kind same as buf
buf.clone_into(scratch);            // reuses scratch's mapping and address, like reload_verified
```

- Copies the used area once (free blocks included) and carries typed root access and schema binding along;
  no wire is produced and nothing is validated (the source is a buffer this process is using). The
  copy is fully independent of the source; the same edits afterwards give the same bytes, and
  `save_verified` output is identical too.
- An unvalidated source (`load_unverified`) stays unvalidated after copying: `root<T>()` still refuses.
- After `clone_into(dst)`, root references, element pointers, `string_view`s and spans into dst's
  old content are all invalid, `dst.epoch()` is incremented, and `XHandle` re-resolves to the new
  content; dst's address, `maximum_capacity` and growth policy are unchanged, and its size becomes
  the source's (not above dst's limit).
- Failures: `clone_into(dst)` with dst as its own source throws `XException` and changes nothing;
  a source whose used area exceeds dst's `maximum_capacity` throws `XBadAlloc`; any failure other
  than self-copy leaves dst as an empty arena without a root, as when `reload_verified` fails, after
  which you can `clone_into` or `reset<T>()` again. A failed `clone()` never changes the source.
- Detailed semantics are in [BUFFER_REUSE.md](BUFFER_REUSE.md).

### 4.3 Design conventions: which costs come from the contract

Two conventions are prerequisites for whole-region relocation (K0) and "what is sent is the arena
bytes" (K3, K5). Their cost falls on a few kinds of operations, and the contract will not change to
remove it. Below are the causes and what users can do.

**Convention one: one arena per buffer, with its own free blocks.** Free space is recorded in the
buffer's own payload (a directory block and the free blocks themselves) and travels with the image, so
after a whole-region move or send, further edits do not degrade (K5) and nothing depends on
process-global allocator state. The cost is that metadata is spread per buffer: the first edit of a
buffer that has gone cold reads its header and directory from memory.

- Remedies: keep buffers long-lived and reuse them message by message (`reload_verified` /
  `reset<T>()`, see [BUFFER_REUSE.md](BUFFER_REUSE.md)), so the mapping stays in place; concentrate
  mergeable edits on a few buffers.
- When you know which buffers are about to be edited while the data for the edit is still on its way
  (for example a battle result is being received and decoded), call `prefetch_for_update()` on those
  buffers first. It only issues non-blocking cache prefetches (owner map entry, owner word, arena
  header) and changes no content or
  allocation decision; the cold-metadata misses then overlap with receiving instead of being waited
  for one by one during the edit.

**Convention two: container slack is part of the payload.** Slack is sent with the wire like any
other bytes, and bytes newly exposed by growth must be zeroed first (uninitialized bytes must never
be sent). So the library does not reserve extra on your behalf: the first `assign` or copy
assignment into an empty container allocates only the length (later growth doubles), and
`save_compact` with `slack_div = 0` also makes capacity equal length. The cost is that when a
received message is edited on the receiver, the first growth of a container must allocate, move the
elements and zero the newly exposed part; every new message starts over.

- When the final size is known, call `reserve` on the `XVector` / `XString` first to allocate once
  instead of doubling several times.
- When sending to a receiver that edits, use the default slack of `save_compact` (`slack_div = 8`:
  containers (except strings) with at least 8 elements get size/8+1), so the receiver's first insert
  does not move; read-only or archiving receivers use `slack_div = 0` for the smallest wire.
- When sending state with slack across machines, use `XWireEmission::zero_runs` (section 4): zero
  runs of at least 64 bytes in the slack take no bytes on the wire, and the slack is still there
  after the receiver loads it.
- When the receiver reuses buffers, after `reload_verified` growth within the previously used range
  is 4 KiB at a time (section 3), with zeroing done on demand.

Neither convention affects correctness or the wire format; the remedies above change only
performance, not semantics or the set of accepted wires.

### 4.4 Fixed-size messages: sent as their own bytes, without XBuffer

A type that contains only scalars, enums, arrays and records made of them, with no arena container
inside (`XString`, `XVector`, `XSet`, `XMap`, `XBlob`) and no implicit padding, is a fixed-size
message (`is_fixed_message_v<T>`): small messages such as quote updates, orders and execution
reports. Its bytes contain no offsets, so it needs no arena, root or wire header, and
`xoffset_fixed_message.hpp` sends and receives it directly as its own bytes:

```cpp
#include "xoffset_fixed_message.hpp"

struct alignas(8) Quote { std::uint64_t ts; std::int64_t price; std::uint32_t qty, instrument;
                          Side side; std::uint8_t flags; std::uint8_t reserved[6]; };
XOFFSET_RECORD(Quote, ts, price, qty, instrument, side, flags, reserved)
XOFFSET_REGISTER_SCHEMA_NAME(Quote, "mkt.Quote")

// On connect: list it in the handshake like any other root type (xoffset_handshake.hpp); after that messages carry no header.
std::string hello = make_schema_hello<Quote, Ack>("feed-1");

// Send: one message is sizeof(Quote) bytes; a batch is an 8-byte header {count, type tag} plus the records.
std::size_t n = write_fixed_batch<Quote>(send_buf, quotes);      // or place_fixed_batch to fill in place
// Receive: read directly on the received bytes, no copy.
if (auto v = view_fixed_batch<Quote>(recv_bytes))
    for (const Quote& q : v.records) on_quote(q);
```

- The view checks the length (exactly `sizeof(T)` for one message, consistent with the count for a
  batch), the alignment of `T`, the batch type tag, and that every `bool` byte is 0 or 1 (as in
  `load_verified`; enum ranges are not checked). That is all the structural validation a fixed-size
  message has: with no offsets there is nothing to bound-check. For types without `bool` the value
  check disappears at compile time; with `bool` a single message costs about 5 more instructions,
  and a batch about 4 more per record plus about 10 fixed. Use `std::uint8_t` for flags to avoid the
  check. On failure `status` gives the reason (`wrong_size`, `misaligned`, `wrong_tag`,
  `invalid_value`).
- A single message carries no tag; send single messages only on a connection that carries one type.
  When a connection carries several fixed-size types, send batches and dispatch on
  `peek_fixed_batch_tag(bytes)` against `fixed_message_tag_v<T>`.
- The view reads in place; the received bytes must not be changed or freed until you are done. The
  bytes must be written by an operation that implicitly creates objects: `memcpy`, reading a socket
  into a byte buffer, or the write functions here.
- The same type can also be an element in an ordinary root (for example `XVector<Quote>` in a
  snapshot); the record bytes are the same in both.
- No exceptions, no allocation. Experimental interface (section 8, E6).

Cost: a few instructions more than a hand-written struct, several times fewer than the same record
in an XOffset root (`bench/fixedmsg/README.md`).

### 4.5 Delta sync: send only the changed bytes

When the receiver already holds a copy, `xoffset_delta.hpp` (included by the main header) sends only the
changed payload bytes and the receiver patches its copy in place; afterwards both payloads are
byte-identical. Usage, format, checks and guarantees: [DELTA_SYNC.md](DELTA_SYNC.md); a complete loop:
`examples/delta_sync.cpp`.

```cpp
XDeltaBaseline base;                                   // sender: one per receiver; the first delta is a key delta
if (world.save_delta_into<View>(d, base); !d.empty()) send(d);

XReceiver<XLink::from_server> rx(XBuffer::maximum_capacity(1 << 20));   // receiver
XDeltaCursor cursor;
if (XStatus r = rx.apply_delta<View>(held, d, cursor); !r) ask_for_key_delta();
```

- A delta that does not match the copy's version (loss, reordering, restart) returns
  `delta_base_mismatch` and changes nothing; the sender calls `base.reset()` and sends a key delta.
- The copy is read-only; to send changes back, use deltas in the other direction.
- Send deltas only when the handshake answers `same` for the root type; on `convert` send converted
  whole buffers (section 7).

### 4.6 Shared deltas: viewport sync

For a viewport onto a shared world, shared deltas (`xoffset_shared_delta.hpp`, format XS 2.0) are
written per record: the server encodes each changed entity once per tick with the library publisher
(`xoffset_shared_publisher.hpp`), every connection that sees it gets the same bytes, and the client
writes each record in place in its own storage through `XReceiver<L>::apply_sync`. Usage, target
interface, format and checks: [SHARED_DELTA.md](SHARED_DELTA.md); a complete loop:
`examples/shared_delta.cpp`.

## 5. Constraints you must follow

| Constraint | Consequence if broken |
| --- | --- |
| Memory allocated by hand (for example `XBuffer::allocator<T>()`, `arena()->allocate_aligned`) that is not reachable from the root must be freed before `save_verified` | The saved wire fails validation on load (the tiling check fails) |
| Writes to one buffer are synchronized by the caller | Data race, undefined behavior |
| Never move-assign a container into a container in its own subtree (or the other way round, section 3.3) | Undefined behavior in Release builds |
| The buffer object outlives its `XHandle`s and borrowed spans | Dangling access |
| All translation units in one executable use the same header version, compile options and toolchain | Inconsistent layout or schema identity |
| Before receiving untrusted input, limit the wire length, `maximum_capacity` and the number of concurrent buffers | Even a single valid wire can use a lot of memory |
| 64-bit little-endian platforms only (32-bit Android builds are not supported) | `#error` at compile time on other platforms |
| Apple deployment targets of at least macOS 14 and iOS 17 (tvOS 17, watchOS 10): validated loading and related interfaces use `std::pmr` from the system libc++, which Apple ships only from these versions, and the SDK has no availability annotations | `#error` at compile time for lower deployment targets (otherwise it would fail to launch on older systems) |

## 6. Error handling

- All library errors are `XException` (derived from `std::runtime_error`); running out of space is
  its subclass `XBadAlloc`. Every `XException` carries an error code: `e.code()` returns an `XErrc`
  (`xoffset_error.hpp`, for example `bad_magic`, `schema_mismatch`, `invalid_payload`,
  `capacity_exceeded`; `to_string(code)` gives the name).

### 6.1 Non-throwing receive entry points

Every receive entry point has a non-throwing version that returns `XResult<T>` (a value or
`XError{code, message}`) or `XStatus`. Validation is exactly the same as in the throwing version; on
failure the error code and text are what the throwing version would throw, and the buffer is left
in the same state (empty, without a root, after a failed reload).

```cpp
auto in = XBuffer::try_load_verified<Profile>(wire, cap);     // XResult<XBuffer>
if (!in) { log(to_string(in.code()), in.error().message); return; }
const Profile& r = in->root<Profile>();

if (auto s = buf.try_reload_verified<Profile>(wire); !s) drop(s.error());

auto token = buf.try_begin_verified_reload<Profile>(used, extents);
// ... write token->image_bytes() / extent_bytes(), complete(...)
XStatus done = token ? token->try_finish() : XStatus(token.error());
```

The entries take the wire as a `std::string`, a `std::string_view` or a byte span (`XByteView`),
and an optional `upstream` memory resource for the validation bitmaps; there are also
`TypedXBuffer<T>::try_load_verified / try_reload_verified`. `XResult::value()` throws when there is
no value (in no-exception builds it calls the fatal handler); `*r` / `r->` do not check.

### 6.2 No-exception builds (`-fno-exceptions`, MSVC `/EHs-c-`)

The whole library can be used with exceptions disabled, for client engines that do not enable them.
It detects `__cpp_exceptions` / `_CPPUNWIND`; `-DXOFFSET_NO_EXCEPTIONS=1` forces it. RTTI is not
required.

- The receive path (wire header, extent table, capacity checks, OS storage reservation and copy,
  owner registration, graph validation, free-table loading) returns the first failure up the call
  chain, and the `try_*` entry points above hand it to the caller. The same wire gives the same
  result and the same error text in builds with and without exceptions
  (`tools/no_exceptions_differential.sh` compares 200,000 mutated wires one by one).
- Other failures (arena out of space while modifying, `root<T>()` with no root, exceeding the wire
  limit when saving, a rejection in a throwing receive entry point, ...) have no caller to return to;
  they call the fatal handler and then `abort()`. Engines plug in their own crash reporting with
  `set_fatal_handler(fn)`; `fn(XErrc, const char*)` must not return.
- A failing host `operator new` terminates as the standard library does without exceptions.
- Two places that recover from `XBadAlloc` when exceptions are on are fatal when they are off: the
  whole-block copy in container copy assignment (with exceptions it falls back to element-by-element
  copy) and the doubling retry in `XCompactor::compact` (never reached when 2x the used space is
  enough). Clients that only receive, read and make small edits are not affected.
- When an allocation fails during a library container modification, the original logical content is
  kept, but capacity and allocator accounting are not necessarily rolled back. Multi-step business
  operations are not transactions.

## 7. Schema changes

The wire header carries the schema identity, and a reader rejects a wire whose identity differs from
its type: adding, removing, reordering or changing any field needs both ends updated together. When
ends run different versions, the server converts with the version chain of
[SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md) and the client only adds a handshake. Pin shared types with
`XOFFSET_PIN_SCHEMA` ([SCHEMA_PINS.md](SCHEMA_PINS.md)) so a compiler that lays one out differently
fails to compile.

## 8. Core guarantees and API stability

Source API and ABI stability between versions are not promised. K0–K9 are the core guarantees; later
versions must not regress them, and CHANGELOG names any renamed interface with its replacement. The
tests behind each are in [STABILITY_TESTS.md](STABILITY_TESTS.md).

| # | Guarantee | Interface / mechanism |
| --- | --- | --- |
| K0 | Relocatable: the payload has no absolute addresses, so after any modification the whole region can be copied to any address, process or machine and used there | Compile-time admission (`is_byte_copy_safe_v`) rejects raw pointers, member pointers, references and vtables; container heads hold relative offsets |
| K1 | Schemas are plain C++ aggregates, no IDL; C++26 reflection or a C++20 field list, with identical wire bytes | `XOFFSET_RECORD`, `XOFFSET_RECORD_DERIVED`, `XOFFSET_REGISTER_SCHEMA_NAME`, the containers |
| K2 | Objects are built and modified in place in the arena, including variable-length changes, with free-block reuse | `create`, `root`, container members, `maximum_capacity`, `XGrowPolicy` |
| K3 | What is sent is a copy owned by the caller; the sender keeps modifying | `save_verified<T>` |
| K4 | Receive checks in three tiers (below); the full tier rejects truncation, out-of-bounds, overlap, incomplete tiling, invalid values and schema mismatch, deltas included; one ASan fuzz round with no findings before every release | `XReceiver`, `load_verified<T>`, delta apply |
| K5 | A received buffer is modified and sent again; free space travels with the hole table | Same as K3, K4 |
| K6 | Schema fingerprints must match exactly; different versions use the evolution handshake (E0) | Wire header `schema_hash` |
| K7 | The same object representation on every verified compiler and platform | `XOFFSET_PIN_SCHEMA`; library signature baselines in `tools/sigs/`; the cross-compiler wire corpus |
| K8 | Clients (Windows, iOS, Android) build as C++20 with their native compilers and use the same library and wire as servers ([CLIENT_TARGETS.md](CLIENT_TARGETS.md)) | Field-list backend; `tools/native_targets.sh` |
| K9 | Native delta sync: whole-state deltas (format 2.0) and shared viewport deltas (XS 2.0), checked at the link's tier and fuzzed before release | `apply_delta`, `apply_sync` ([DELTA_SYNC.md](DELTA_SYNC.md), [SHARED_DELTA.md](SHARED_DELTA.md)) |

The unit that moves is the whole region (container heads cannot be copied on their own). A buffer moves
only at explicit points (output, receive, reload, `XCompactor`); `XHandle` follows it by epoch.

Receive checks have three tiers, chosen by the link a `XReceiver` serves (`xoffset_receiver.hpp`):

| Tier | Default for | What it checks | What you can do after receiving |
| --- | --- | --- | --- |
| Trusted | Server to server, server to client, your own saved files | Wire header admission (fingerprint, limit, root offset) and hole-table bounds; skips the graph walk and delta check-table maintenance | Read and write directly |
| Bounds | Client to server (`view`) | Wire header admission as above; one walk of the container headers from the root: every block inside the payload and aligned, size not above capacity, heap strings NUL-terminated, inline strings at most 10 bytes, bools only 0 or 1, total element bytes walked not above the payload size. Does not check block overlap, tiling, set/map key order or padding bytes | Read-only in place (`XView<T>`, const access only), no copy |
| Full | When modifying data sent by clients in place; can be turned on explicitly on any link | Full graph structure validation | Read and write in place, send again |

The trusted tier assumes the sender is your own service from the same build and the link has TLS or
mTLS or is on an internal network; it is not an entry point for untrusted input. Debug and sanitizer
builds still run full validation in the trusted tier. Data sent by clients is untrusted input: the
bounds tier only guarantees that reads stay in bounds, and the data is used read-only; to modify it
in place, use the full tier. A deployment whose clients are also its own code over a protected link
can build with `-DXOFFSET_CHECK_UPLINK=0`, so that the client link defaults to the trusted tier too
(receiving an SLG uplink order takes 9.6% fewer instructions); do not build a server that faces
outside clients that way.

```cpp
#include "xoffset_receiver.hpp"

// Receivers are built per connection type, and the type decides the tier; there is no global switch and no trusted member on XBuffer.
XReceiver<XLink::from_server> rx(XBuffer::maximum_capacity(16 << 20));   // client receiving from the server
XStatus st = rx.reload<Profile>(held, wire);        // trusted tier: check the header, copy (the free directory comes along), no graph walk
XStatus full = rx.reload<Profile, XCheck::full>(held, wire);   // explicit full validation on the same link
auto fresh = rx.load<Profile>(wire);                // new buffer (a new mapping each time; use reload on hot paths)
XDeltaCursor cur;                                   // which delta stream held follows
XStatus d = rx.apply_delta<Profile>(held, delta, cur);   // trusted-tier delta: checks only format, stream and version, block and hole bounds

XReceiver<XLink::from_client> up(XBuffer::maximum_capacity(64 << 10));   // server receiving from a client
if (auto v = up.view<Order>(wire)) {                // bounds tier: read-only in place, no copy
    const Order& o = v->root();                     // const access only; writes do not compile
    apply_order(world, o);                          // read the fields you need and write them into the server's own state
}
(void)up.reload<Profile>(state, wire);              // keeping and modifying a client's whole data: full tier only; XCheck::admit does not compile
```

Preconditions of `view`: the wire is dense (no extent table; wires under 4 KiB are always dense, send
larger ones with `XSaveOptions{XWireEmission::dense}`), the bytes start on an 8-byte boundary, and
they are not changed or freed until you are done with the view. Sparse wires and misaligned bytes
return `XErrc::invalid_argument`. Bounds-tier results may be garbage (for example when two containers
share a block, or set keys are out of order and lookups miss), but no const access reads outside the
wire; since blocks may overlap, the walk is capped in total by the payload size, so a small message
cannot make the walk revisit the same large block over and over. On trusted links `view` defaults to
the trusted tier (only the wire header and that the root is inside the payload are checked); you can
also write `view<T, XCheck::bounds>` explicitly. With `XOFFSET_FORCE_FULL_CHECK=1`, `view` also runs
full validation on a copy and rejects whatever the full tier rejects.

The trusted tier accepts any V2 wire (every `save_verified` emission, `save_compact`), and as with the
full tier the result can be read and written in place and sent again. The trusted tier for deltas does
not build or maintain a check table (docs/DELTA_SYNC.md section 3); if the same cursor later switches
to the full tier, the first full delta validates the whole buffer once and rebuilds the check table.
`XOFFSET_FORCE_FULL_CHECK=1` makes the trusted tier run full validation: this is the default in builds
without `NDEBUG` and in ASan and TSan builds, and release builds can turn it on by hand for stress
tests or staged rollouts.

Preconditions: the receiver sets `maximum_capacity` (limits in [DEPLOYMENT.md](DEPLOYMENT.md)
section 5). No tier checks content: a well-formed message with changed field values is accepted.
K7 covers only toolchains that ran the checks ([CLIENT_TARGETS.md](CLIENT_TARGETS.md)).

The following interfaces are provided but optional or experimental and may change in later versions:

| # | Interface | Notes |
| --- | --- | --- |
| E0 | Schema evolution: `xoffset_evolution.hpp` (version chain, `evolve_load_verified`, `XPeerSchemas` handshake), freeze tool | Interop between ends on different versions (since 3.2.0, [SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md)); conversion runs on the server, clients contain no conversion code |
| E2 | `save_verified_into`, `TypedXBuffer<T>` | `TypedXBuffer<T>` is the recommended pattern of section 3; the shape of both may still change |
| E3 | `XBufferPool`, reservation and growth policy details, `XStorage` / `storage_kind()`, `SLOT_MAX`, `trim_slot_storage` | High-frequency small buffers |
| E4 | `load_unverified` | Only for data the caller already trusts |
| E5 | `XCompactor` | Same-type compaction |
| E6 | `xoffset_fixed_message.hpp` (fixed-size messages, section 4.4) | Send and receive fixed-size records without XBuffer |

`XHandle`, diagnostic and statistics interfaces, and the low-level members of `XBufferCore` are not
covered by the guarantees.

## 9. Performance numbers

Numbers, methods and limits are in [PERFORMANCE.md](PERFORMANCE.md). When quoting one, also state the
machine, the workload and the adapter style.
