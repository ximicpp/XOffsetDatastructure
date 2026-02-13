# XOffsetDatastructure — Core Formal Model

> Version 3.0 | 2026-02-11
> This document defines the theoretical foundation of XOffsetDatastructure.
> Every claim maps to code; every invariant maps to an enforcement mechanism.

---

## §1 Goal and Definitions

### 1.1 The Goal: Zero-Encoding Serialization

Traditional serialization performs two transformations:

```
Write:  In-memory objects  →  encode()  →  Wire format
Read:   Wire format        →  decode()  →  In-memory objects
```

XOffsetDatastructure eliminates both transformations:

```
Write:  Buffer bytes  →  (identity)  →  Buffer bytes
Read:   Buffer bytes  →  (identity)  →  Buffer bytes
```

The in-memory representation IS the wire format. No encoding, no decoding,
no schema, no code generation.

**When is this possible?** When the following theorem holds:

> byte_copy(Buffer) produces a semantically equivalent object graph.

The rest of this document states the **two necessary and sufficient conditions**
for this theorem, proves it, and lists the **operational assumptions** required
to actually use the copied buffer at runtime.

### 1.2 Definition: Semantic Equivalence

> Object graphs G (in B) and G' (in B') are **semantically equivalent** if for
> every named object `o` at byte offset k in G, the corresponding object `o'`
> at the same byte offset k in G' satisfies:
> 1. Same type
> 2. Same field values (recursively) for all non-padding bytes
> 3. All internal pointer-like references resolve to the corresponding targets in B'
>
> Padding bytes between struct fields are excluded — they are unspecified and do
> not participate in object semantics.

### 1.3 Definition: byte_copy

> `byte_copy(B) → B'` means allocating a new contiguous byte array B' of the
> same size as B, and copying every byte at its original offset:
>
> `B'[k] = B[k]  for all k ∈ [0, |B|)`
>
> The copy is lossless and order-preserving. B' may reside at a different base
> address than B; the displacement `δ = base(B') − base(B)` may be arbitrary.
>
> *Precondition*: B must be quiescent during the copy — no concurrent writes.

### 1.4 Definition: Buffer Model

A **Buffer** is a contiguous byte array (`std::vector<char>`) managed by a
Boost.Interprocess segment manager:

```
┌──────────────────────────────────────────────────────┐
│                  std::vector<char>                     │
│  ┌──────────┬───────────┬──────────┬────────────────┐ │
│  │ Segment   │ iset_index │ Free-list│  User objects   │ │
│  │ Header    │ (names)    │  nodes   │ (Player, etc.)  │ │
│  └──────────┴───────────┴──────────┴────────────────┘ │
│  ↑ base address                                        │
└──────────────────────────────────────────────────────┘
```

Everything — the segment header, the name index, the free-list, and all user
objects — lives inside this single contiguous buffer. There is nothing outside.

---

## §2 Two Conditions for Correctness

A buffer contains two kinds of information: **values** (scalar field data) and
**references** (pointers between objects). Zero-encoding requires preserving both:

| Condition | Role | What it preserves | Core mechanism |
|-----------|------|------------------|----------------|
| **C1: Layout Determinism** | Value preservation | Scalar fields, sizes, offsets | Domain A + Domain S + TypeLayout signature |
| **C2: Referential Integrity** | Reference preservation | Pointers between objects | Domain S + offset_ptr |

```
C1 (value preservation) + C2 (reference preservation) = semantic equivalence
```

Both conditions share **Domain S** (the Safe Type Set) as a common foundation.
Domain S serves C1 by constraining type widths, and C2 by excluding absolute addresses.

If either condition is violated, zero-encoding is broken.

---

### §2.1 Condition 1: Layout Determinism

#### What it requires

For any type T used in the buffer, the byte-level layout (field offsets, sizes,
padding, endianness) must be **identical** on the producing platform and the
consuming platform.

#### Why it's needed

If `sizeof(int32_t)` were 4 on the producer but 2 on the consumer, a struct
containing `int32_t` would have different field offsets. Byte-copying would place
the wrong bytes at the wrong positions.

#### How it's guaranteed: Two Domains + One Verifier

Layout determinism is not a single check — it is the result of two **domain
definitions** that constrain the problem space, and a **verifier** that confirms
the concrete byte layout within that space:

```
┌─────────────────────────────────────────────────┐
│  Domain A: Architecture Set                      │
│  "On which platforms is zero-encoding valid?"    │
│  Defined by: #error + static_assert              │
│  Current: A = { Arch64LE }                       │
├─────────────────────────────────────────────────┤
│  Domain S: Safe Type Set                         │
│  "Which types may enter the buffer?"             │
│  Defined by: is_safe_type() recursive check      │
│  Shared with C2 (referential integrity)          │
├─────────────────────────────────────────────────┤
│  Verifier: TypeLayout Signature                  │
│  "Does T actually have the expected layout?"     │
│  Operates on: (A × S)                            │
│  Mechanism: get_definition_signature<T>()        │
└─────────────────────────────────────────────────┘
```

**Domain A** and **Domain S** are prerequisites — they define the space within
which zero-encoding is meaningful. The **TypeLayout signature** is the verifier
that confirms layout identity within that space.

**Domain A: Architecture Set**

A defines the set of valid platform configurations:

```
A = { Arch64LE, Arch64BE, Arch32LE, Arch32BE }
```

Each architecture is a tuple:

```
a = (ptr_size, endianness, sizeof_table, align_table)
```

At compile time, one member is selected:

```
TargetArch ∈ A    (currently defaults to Arch64LE)
```

Producer and consumer must use the **same TargetArch**. Taking `Arch64LE` as reference:

| Property | Value |
|----------|-------|
| ptr_size | 8 |
| endianness | LE |
| sizeof(int8_t..int64_t) | 1, 2, 4, 8 |
| sizeof(float, double) | 4, 8 |
| sizeof(bool, char) | 1, 1 |
| alignof(void*) | 8 |

Domain A is enforced by compile-time gates:
- `#error` rejects 32-bit or big-endian platforms at preprocessing
- 15 `static_assert` statements verify sizeof/alignof of each primitive type

**Domain S: Safe Type Set**

S defines which types may be stored in a managed buffer. It is described in
full in §3. For C1, the relevant property of S is that it **only admits
fixed-width types** (S₀ = `int8_t`, `int32_t`, `float`, `double`, etc.) and
**excludes platform-dependent types** (`int`, `long`, `size_t`).

This ensures that within a given architecture `a ∈ A`, every primitive type in
S has an unambiguous size. Without this constraint, two platforms in the same
architecture could disagree on `sizeof(long)`.

> Note: Domain S also serves **Condition C2** by excluding types that contain
> absolute addresses (raw pointers, std containers, virtual classes). See §3.

**Verifier: TypeLayout Signature**

Within the domain (A × S), the TypeLayout library generates a compile-time
string that encodes the **exact** byte layout of a type:

```
[64-le]record[s:72,a:8]{
  @0[id]:i32[s:4,a:4],
  @4[level]:i32[s:4,a:4],
  @8[name]:string[s:32,a:8],
  @40[items]:vector[s:32,a:8]<i32[s:4,a:4]>}
```

Each element of this signature encodes a specific layout fact:

| Signature element | What it encodes |
|-------------------|----------------|
| `[64-le]` | Architecture: pointer size + endianness (from A) |
| `s:72,a:8` | Total sizeof and alignof of the struct |
| `@0`, `@4`, `@8`, `@40` | **Exact field offsets** (includes all padding) |
| `i32[s:4,a:4]` | Each field's type, size, and alignment |
| Recursive nesting | Container element types are fully encoded |

**The signature is a complete ABI fingerprint for user-defined composite
types** — every field offset is explicitly encoded. For library container
types (XVector, XString, etc.), the signature encodes the **outer shell**
(total size, alignment, element type) but not the internal field arrangement;
internal layout consistency for these types is guaranteed by P3 (same Boost
version). If two platforms produce the same signature for a composite type T,
then T has the same byte layout on both — regardless of compiler vendor, ABI
standard, or padding strategy.

This resolves the ABI compatibility problem: architecture alone does not
determine struct padding (GCC and MSVC may differ), but the signature captures
the **actual** padding. If padding differs, the `@N` offsets differ, the
signatures differ, and `static_assert` fails at compile time.

#### Formal statement

> **C1**: For all data in the buffer — user objects AND infrastructure
> (segment header, index, free-list, container internals) — the byte layout
> must be identical on the producing and consuming platforms:
>
> `layout(T)_producer == layout(T)_consumer`  for every type T present in B
>
> *Enforcement* (split by control boundary):
> - **User types (T ∈ S)**: Domain A + Domain S + TypeLayout signature
> - **Infrastructure types**: Same Boost.IPC version + same architecture (P3)

#### Code enforcement

| Role | Mechanism | What it checks | Location |
|------|-----------|---------------|----------|
| **Domain A** | Preprocessor `#error` | Rejects 32-bit or big-endian | `#ifndef XOFFSET_DISABLE_PLATFORM_CHECKS` block |
| **Domain A** | 15 `static_assert` | sizeof/alignof of primitives match TargetArch | Platform validation block after `namespace XOffsetDatastructure` |
| **Domain S** | `is_safe_type()` check | Only fixed-width, pointer-safe types | §3, `detail::is_safe_type()` |
| **Verifier** | TypeLayout signature | Exact field offsets, sizes, padding, nesting | `static_assert(get_definition_signature<T>() == ...)` |

---

### §2.2 Condition 2: Referential Integrity

#### What it requires

Every address reference stored inside the buffer must resolve to the correct
target after the buffer is byte-copied to a different memory location.

#### Why it's needed

A buffer contains not just scalar values but also **pointers**: XVector points to
its element array, XString points to its char buffer, the name index points to
objects. If any of these pointers break after copy, the data is corrupted.

#### How it's guaranteed: Domain S + offset_ptr Mechanism

Referential integrity is the result of a **domain constraint** that prevents
absolute addresses from entering the buffer, and a **mechanism** that ensures
relative addresses survive relocation:

```
┌─────────────────────────────────────────────────┐
│  Domain S: Safe Type Set (shared with C1)        │
│  "No absolute address may enter the buffer"      │
│  Excludes: raw pointers, std containers,         │
│            virtual classes, type-erased types     │
│  → Guarantees Lemma C2.2 (No Absolute Escape)    │
├─────────────────────────────────────────────────┤
│  Mechanism: offset_ptr                           │
│  "All address references are relative"           │
│  stored_value = target_address - this_address    │
│  → Guarantees Lemma C2.1 (Relocation Survival)   │
└─────────────────────────────────────────────────┘
```

**Mechanism: offset_ptr**

An `offset_ptr<T>` stores the **signed distance** from its own address to the target:

```
stored_value = target_address - this_address
```

When the entire buffer is byte-copied as a whole with displacement δ:

```
new_this   = this + δ
new_target = target + δ  (because target is in the same buffer)
resolve()  = new_this + stored_value
           = (this + δ) + (target - this)
           = target + δ
           = new_target  ✓
```

> **Lemma C2.1 (offset_ptr Relocation)**: If an `offset_ptr` at address `a_p` in
> buffer B points to target at address `a_t` in B, and B is byte-copied to B'
> as a whole with uniform displacement δ, then the copied `offset_ptr` resolves
> to `a_t + δ` (the target's copy in B').
>
> *Precondition*: The copy is a whole-buffer copy — every byte at offset k in B
> appears at offset k in B'. Both the pointer and its target are within B.
>
> *Proof*: `stored = a_t - a_p`. Copied pointer is at `a_p + δ`.
> `resolve() = (a_p + δ) + (a_t - a_p) = a_t + δ`. ∎
>
> *Null case*: A null `offset_ptr` stores a sentinel value (implementation-
> defined, typically `stored = 1`). Byte-copy preserves this sentinel, so
> the copied pointer remains null. The null case holds trivially.

**Domain S: No Absolute Address Escape**

Lemma C2.1 only works if ALL address references in the buffer are offset_ptr.
A single raw pointer would break. **Domain S** (the Safe Type Set, shared with
C1) prevents this:

> **Lemma C2.2 (No Absolute Address Escape)**: For any buffer B whose user-facing
> types all belong to S, no absolute address exists in B.

*Proof sketch*:
- Primitives, enums: contain no pointers ✓
- XString, XVector, XSet, XMap: use `allocator<T, segment_manager>` which
  internally uses `offset_ptr` as its pointer type ✓
- iset_index, free-list: Boost.IPC internals use `offset_ptr` ✓
  (**Dependency assumption** — this relies on Boost.Interprocess's implementation,
  not on our type system.)
- Composite types: recursively checked; all members must be in S ✓
- S excludes: raw pointers, std containers (heap raw pointers), virtual classes
  (vtable raw pointer), type-erased types (hidden raw pointers) ✓

The Safe Type Set S is defined in detail in §3.

#### Formal statement

> **C2**: Every address reference stored in B is an `offset_ptr` pointing to a
> location within B. After byte_copy(B) → B' (whole-buffer copy), all
> offset_ptr instances in B' resolve to the correct targets within B'.
>
> This contains two sub-properties:
> 1. **No absolute address** — all references are offset_ptr (Lemma C2.2, enforced by Domain S)
> 2. **Self-containment** — all offset_ptr targets are within B (enforced by segment manager + Domain S)
>
> Given (1) and (2), offset_ptr's arithmetic ensures all references survive (Lemma C2.1).

#### Code enforcement

| Role | Mechanism | What it checks | Location |
|------|-----------|---------------|----------|
| **Domain S** | `is_safe_type()` check | No absolute address types | §3, `detail::is_safe_type()` |
| **Domain S** | `validate_xbuffer_type<T>()` | User-facing gate rejects unsafe types | `validate_xbuffer_type<T>()` function |
| **Mechanism** | offset_ptr via Boost.IPC allocator | All containers use relative pointers | Boost.Container + allocator |
| **P3** | Same Boost.IPC version | Infrastructure types (index, free-list) use offset_ptr | Dependency assumption |

---

### §2.3 Implementation Assumptions and Operational Prerequisites

C1 and C2 are the theoretical correctness conditions. In practice, their
enforcement depends on one **implementation assumption** (P3), and using the
copied buffer requires two **operational prerequisites** (P1, P2).

#### P3: Infrastructure Compatibility (Implementation Assumption)

The buffer contains not only user objects (governed by S) but also **infrastructure
data** managed by Boost.Interprocess: the segment header, the iset_index, and
free-list nodes, as well as container internal state (size, capacity).

These are Boost-internal types **not in S**. C1 and C2 cover the entire buffer
(including infrastructure), but our type system can only enforce them for user
types. For infrastructure types, C1 and C2 are guaranteed by using the **same
version of Boost.Interprocess** compiled with the same architecture.

> P3 is not a separate correctness condition — it is how C1 and C2 are
> **enforced** for the part of the buffer we do not control. If the framework
> were reimplemented without Boost, P3 would not exist, but C1 and C2 would
> still apply.

> If P3 is violated: container internal state (e.g., `size()`) may be
> misinterpreted, or `open_impl` may fail to parse the segment header.

#### P1: Buffer Alignment (Operational Prerequisite)

The destination buffer's base address must satisfy the segment manager's
alignment requirement (typically 8-byte alignment for 64-bit systems).

In the framework, Boost.Interprocess internally aligns allocations within the
segment. The code verifies the buffer base address with `BOOST_ASSERT`.

> If P1 is violated: hardware may fault on unaligned access, or struct fields
> may be read at wrong byte boundaries. The data is correct, but inaccessible.

#### P2: Object Lifetime (C++ Language Formalism)

In strict ISO C++, accessing non-trivially-copyable objects (XString, XVector)
through byte-copied memory is technically undefined behavior — the objects'
lifetime was not formally started in the new buffer.

The framework relies on Boost.Interprocess's established "memory-as-bytes"
pattern, which is reliable on all major compilers. C++23's
`std::start_lifetime_as` provides a standards-conformant path.

> If P2 is violated: a hypothetical strictly-conforming compiler could refuse
> to access the objects. No known compiler exhibits this behavior.

#### Invariant: Buffer Self-Containment

Lemma C2.1 requires that both the pointer and its target reside in the same
buffer. This invariant — **every allocated object and every internal pointer
target is within [base, base + size)** — is guaranteed by two mechanisms:

1. **Segment manager allocation model**: All `construct<T>()` and container
   element allocations go through the segment manager, which only allocates
   from within the buffer.
2. **Domain S**: Excludes raw pointers (which could point anywhere) and
   `XOffsetPtr` (whose target may be outside). See §6.3 for opt-in.

#### Corollary: open_impl succeeds

Given C1, C2, P1, P2, and P3:

`open_impl(B')` reads the segment header (correct by C1 + P3), follows the
index root and free-list root `offset_ptr`s (valid by C2 + P3), and rebuilds
the runtime segment manager state. This is not an independent condition — it
is a direct consequence of the correctness conditions plus the operational
assumptions.

---

## §3 The Safe Type Set S

The Safe Type Set S is a shared domain that serves both correctness conditions:
- **For C1**: S admits only fixed-width types, ensuring deterministic layout
- **For C2**: S excludes types containing absolute addresses, ensuring referential integrity

### 3.1 Inductive Definition

```
S₀          = { int8_t, int16_t, int32_t, int64_t,
                uint8_t, uint16_t, uint32_t, uint64_t,
                float, double, bool, char }

S_enum      = { E | is_enum(E) ∧ underlying_type(E) ∈ S₀ }

S_string    = { XString }

S_container = { XVector<T> | T ∈ S }
            ∪ { XSet<T>    | T ∈ S }
            ∪ { XMap<K,V>  | K ∈ S ∧ V ∈ S }

S_composite = { C | is_class(C) ∧ ¬polymorphic(C) ∧ ¬has_bases(C)
                   ∧ ¬union(C)
                   ∧ ∀m ∈ nonstatic_data_members(C) : type(m) ∈ S }

S = S₀ ∪ S_enum ∪ S_string ∪ S_container ∪ S_composite
```

> **Well-foundedness**: The recursive definition of S terminates because C++
> prohibits by-value self-containment (a struct cannot contain itself as a
> direct member). Containers break the containment chain via indirection
> (offset_ptr), so the membership check always reaches a base case (S₀).

### 3.2 Each Rule Protects a Condition

| Rule | What it excludes | Which condition it protects |
|------|------------------|---------------------------|
| S₀ only fixed-width types | `int`, `long`, `size_t` (width varies) | C1 (layout determinism) |
| S_enum requires fixed underlying type | Unscoped enums with compiler-chosen width | C1 (layout determinism) |
| S excludes raw pointers `T*` | Absolute addresses | C2 (referential integrity) |
| S excludes `std::string`, `std::vector` | Internal heap raw pointers | C2 (referential integrity) |
| S excludes polymorphic classes | vtable pointer (absolute address) | C2 (referential integrity) |
| S excludes classes with base classes | Potential vtable; ABI-sensitive layout | C2 + C1 (conservative: non-virtual inheritance may be safe if TypeLayout signature matches; future relaxation possible) |
| S excludes unions | Active member ambiguous after byte copy | C1 (active member is value information; byte copy cannot preserve it) |
| S excludes `std::function`, `std::any` | Type-erased, hidden raw pointers | C2 (referential integrity) |
| S_container requires elements ∈ S | Recursive safety | C2 (referential integrity) |
| S_composite requires all members ∈ S | Recursive safety | C2 (referential integrity) |
| XOffsetPtr excluded by default | Target may be outside buffer | C2 (Lemma C2.1 precondition) |

### 3.3 Code Mapping

| Model concept | Code mechanism | Location |
|---------------|----------------|----------|
| S₀ | `is_safe_leaf<int8_t>` ... `<char>` : true_type | LEAF-1 block in `detail` namespace |
| S_enum | `is_fixed_enum<CleanT>()` in `is_safe_type` | `is_safe_type()` enum branch |
| S_string | `is_safe_leaf<XString>` : true_type | LEAF-3 in `detail` namespace |
| S_container | `is_safe_leaf<XVector<T>>`, `<XSet<T>>`, `<XMap<K,V>>` | LEAF-4 in `detail` namespace |
| S_composite | `are_all_members_safe<CleanT>()` | `are_all_members_safe()` function |
| Exclusion of polymorphic | `std::is_polymorphic_v<T>` | Inside `are_all_members_safe()` |
| Exclusion of bases | `has_bases<T>()` via reflection | Inside `are_all_members_safe()` |
| Exclusion of union | `std::is_union_v<T>` | Inside `are_all_members_safe()` |
| User-facing gate | `validate_xbuffer_type<T>()` static_assert | `validate_xbuffer_type()` function |

---

## §4 The Zero-Encoding Correctness Theorem

### 4.1 Statement

> **Theorem (Zero-Encoding Correctness)**:
>
> If conditions C1 and C2 both hold, then for any valid managed memory
> segment B:
>
> `byte_copy(B) → B'` produces an object graph in B' that is
> **semantically equivalent** to the object graph in B.

> **Corollary (Usability)**:
>
> When C1 and C2 are enforced in practice (P3 ensures C1/C2 for infrastructure
> types), and operational prerequisites P1 and P2 hold, then `open_impl(B')`
> successfully rebuilds runtime navigation, `find<T>("name")` locates all
> named objects in B', and the copied buffer is **fully operational** — existing
> objects can be read and modified, and new objects can be allocated (the
> free-list and segment header are also preserved by C1+C2).

### 4.2 Proof

The proof has four steps:

```
Step 1  (C1)              Layout Determinism     → same bytes = same values
Step 2  (C2, Lemma C2.2)  No Escape              → ALL references are offset_ptr
Step 3  (C2, Lemma C2.1)  Pointer Relocation     → offset_ptr resolves correctly
Step 4  (C1+C2, induction on S) Value Preservation → every field is preserved
═══════════════════════════════════════════════════════════════════════════
∴  Object graph in B' is semantically equivalent to B  ∎
```

**Step 1 — Layout Determinism** (from C1):

C1 guarantees that for every type T present in the buffer (user types and
infrastructure alike), the byte layout is identical on producer and consumer.
Therefore, the bytes in B' represent the same field offsets and sizes as in B.
Every scalar value occupies the same position and has the same endianness.

**Step 2 — No Escape** (from C2, Lemma C2.2):

The type set S excludes all types that could introduce absolute addresses.
Therefore, every address reference in B is an `offset_ptr`.

**Step 3 — Pointer Relocation** (from C2, Lemma C2.1):

Given Step 2, every `offset_ptr` in B stores `target - this`. After byte_copy
with displacement δ, both pointer and target shift by δ:

```
resolve() = (this + δ) + (target - this) = target + δ  ✓
```

This holds for all offset_ptr instances: in container backing stores, string char
buffers, name index entries, and free-list nodes. Combined with Step 2, every
address reference in B' is valid.

**Step 4 — Value Preservation** (structural induction on S):

- **S₀ (primitives)**: Fixed-width, fixed-endian values. Byte-copy preserves. ∎
- **S_enum**: Stored as underlying type ∈ S₀. Same argument. ∎
- **XString**: offset_ptr<char> → valid by Step 2. Internal state (size, etc.)
  preserved by C1 (whole-type layout identity). Char data ∈ S₀. ∎
- **Containers**: offset_ptr<E> → valid by Step 2. Internal state (size, capacity)
  preserved by C1 (whole-type layout identity). Elements ∈ S → by induction. ∎
- **Composites**: No vtable (C2). All members ∈ S → by induction. ∎

**Whole buffer**: All user objects preserved (Step 4). All metadata preserved
(offset_ptr internals, Step 2). ∎

**Corollary proof** (open_impl succeeds):

The segment header is a Boost-internal struct within B — its layout is correct
(C1, enforced by P3 for infrastructure types), its internal offset_ptrs are
valid (C2, enforced by Boost's use of offset_ptr). Given P1 (alignment) and
P2 (lifetime), `open_impl` can read the header and traverse the index. ∎

### 4.3 Necessity Proof

The sufficiency proof (§4.2) shows C1 ∧ C2 ⟹ semantic equivalence. We now
show the converse: if semantic equivalence holds for **all** valid buffers,
then both C1 and C2 must hold. This establishes that C1 and C2 are the
**necessary and sufficient** conditions.

> **Necessity of C1** (Layout Determinism):
>
> Assume ¬C1: there exists a type T present in the buffer such that
> `layout(T)_producer ≠ layout(T)_consumer`. Then at least one of the
> following differs: a field offset, a field size, or the endianness of a
> scalar field.
>
> *Case 1 — field offset differs*: An object `o` of type T occupies bytes
> `[k, k + sizeof(T))` in B. After byte_copy, the consumer interprets
> field `f` starting at `k + offset_consumer(f)` instead of `k + offset_producer(f)`.
> The bytes read are from a different position, so the value is wrong.
> Semantic equivalence property (2) — same field values — is violated. ∎
>
> *Case 2 — field size differs*: The consumer reads fewer or more bytes for
> field `f`, yielding a truncated or over-extended value. Property (2) violated. ∎
>
> *Case 3 — endianness differs*: The consumer interprets the byte sequence
> in reversed order, yielding a byte-swapped value. Property (2) violated. ∎
>
> In all cases, ¬C1 ⟹ ¬semantic_equivalence. Contrapositive: semantic
> equivalence ⟹ C1. ∎

> **Necessity of C2** (Referential Integrity):
>
> Assume ¬C2: there exists an address reference `r` in B that is either
> (a) an absolute address, or (b) an offset_ptr whose target is outside B.
>
> *Case (a) — absolute address*: After byte_copy with displacement δ ≠ 0,
> the absolute address still points to the **original** location in B, not
> the copy in B'. The reference does not resolve to the corresponding
> target in B'. Semantic equivalence property (3) is violated. ∎
>
> *Case (b) — offset_ptr target outside B*: The target is not copied as
> part of byte_copy(B). The offset_ptr resolves to `target + δ`, but the
> actual target has not moved (it's outside B). The reference resolves to
> an invalid address. Property (3) violated. ∎
>
> In both cases, ¬C2 ⟹ ¬semantic_equivalence. Contrapositive: semantic
> equivalence ⟹ C2. ∎

> **Combined**: C1 ∧ C2 ⟹ semantic_equivalence (§4.2) and
> semantic_equivalence ⟹ C1 ∧ C2 (above). Therefore, C1 and C2 are
> **necessary and sufficient** for zero-encoding correctness. ∎

---

## §5 Enforcement Chain

### 5.1 Condition → Code Mapping

| Condition | Enforcement | Code |
|-----------|-------------|------|
| **C1**: Same architecture | 15 `static_assert` matching TargetArch | Platform validation block |
| **C1**: Same architecture | Preprocessor `#error` for wrong platform | `XOFFSET_DISABLE_PLATFORM_CHECKS` block |
| **C1**: Same ABI | TypeLayout signature comparison | `get_definition_signature<T>()` |
| **C2**: No absolute addresses | `is_safe_leaf` whitelist | LEAF-1~5 in `detail` namespace |
| **C2**: No absolute addresses | `is_safe_type()` recursive check | `detail::is_safe_type()` function |
| **C2**: No absolute addresses | `validate_xbuffer_type<T>()` gate | `validate_xbuffer_type()` function |
| **C2**: offset_ptr usage | Boost.IPC containers with segment_manager allocator | Boost.Container + allocator |
| **P1**: Alignment | `BOOST_ASSERT` on buffer address | `XManagedMemory` constructors |
| **P2**: Lifetime | Boost.IPC "memory-as-bytes" pattern | Boost.Interprocess convention |
| **P3**: Infrastructure | Same Boost.IPC version on producer and consumer | Dependency assumption |

### 5.2 TypeLayout Signature

TypeLayout's role as C1's verifier is detailed in §2.1. The signature encodes
the complete byte layout (architecture, field offsets, sizes, nesting) and
serves as the final validation that producer and consumer agree on layout.

---

## §6 Boundary Conditions

> **Scope note**: This formal model is a **correctness model** — it proves that
> well-formed buffers are preserved by byte_copy. It is NOT a security model.
> If B' is received from an untrusted source, no guarantees are provided:
> malicious offset_ptr values could cause out-of-bounds access, and corrupted
> size fields could trigger buffer overreads. Input validation is the caller's
> responsibility.

### 6.1 When Each Condition Breaks

| Scenario | Which condition breaks | Detection |
|----------|----------------------|-----------|
| 64-bit ↔ 32-bit | C1 — sizeof differs | static_assert at compile time |
| LE ↔ BE | C1 — endianness differs | static_assert / preprocessor |
| GCC ↔ MSVC (different padding) | C1 — ABI differs | TypeLayout signature mismatch |
| `long double` (f80) | C1 — size varies (8/10/12/16) | Not in S₀; rejected |
| `int`, `long`, `size_t` | C1 — width varies | Not in S₀; rejected |
| Raw pointer `T*` in struct | C2 — absolute address | `is_safe_type` rejects |
| `std::string` / `std::vector` | C2 — heap raw pointers | `is_safe_type` rejects |
| Virtual class | C2 — vtable pointer | `is_safe_type` rejects |
| `XOffsetPtr` target outside buffer | C2 — Lemma C2.1 precondition | Excluded from S by default |
| Different Boost.IPC versions | P3 violated — infrastructure layout differs | No automatic detection |
| Buffer loaded at misaligned address | P1 violated — may fault on access | `BOOST_ASSERT` at runtime |
| Buffer truncated/corrupted | Data integrity lost | Exception thrown by open_impl |

### 6.2 Migration Path

When C1 is violated (cross-architecture, ABI change), the framework provides
**reflection-driven migration** via `XBufferCompactor`:

1. Detect mismatch via TypeLayout signature comparison
2. Reconstruct the object graph in a new buffer with the target's layout
3. Per-member migration: `TrivialCopy`, `AllocatorAware`, `Container`, `Composite`

### 6.3 XOffsetPtr: Opt-in for C2

`XOffsetPtr<T>` is excluded from S because its target may be outside the buffer,
violating Lemma C2.1's precondition. Users may opt in by specializing
`is_safe_leaf<XOffsetPtr<MyType>>` if they guarantee the target is always in the
same buffer and provide a custom `migrate_as` strategy for compaction.

---

## §7 Summary

### 7.1 Core Insight

> In a controlled space of architectures (Domain A) and types (Domain S),
> **value preservation** (C1, verified by TypeLayout) and **reference
> preservation** (C2, guaranteed by offset_ptr) are the two necessary and
> sufficient conditions for byte_copy to produce a semantically equivalent
> object graph. Everything else — alignment (P1), object lifetime (P2),
> infrastructure compatibility (P3) — is either an operational prerequisite
> or an implementation-level enforcement mechanism for these two conditions.

### 7.2 Framework Structure

```
┌─────────────────────────────────────────────────────────────┐
│  Theorem: C1 + C2 ⟹ semantic equivalence                   │
│                                                              │
│  C1 (Value Preservation)     C2 (Reference Preservation)    │
│    Domain A (architecture)     Domain S (type constraints)   │
│    Domain S (type widths)      offset_ptr (mechanism)        │
│    TypeLayout (verifier)                                     │
│                                                              │
│  Shared foundation: Domain S                                 │
├─────────────────────────────────────────────────────────────┤
│  Enforcement (split by control boundary):                    │
│    User types:          Domain A + S + TypeLayout             │
│    Infrastructure:      P3 (same Boost version)              │
├─────────────────────────────────────────────────────────────┤
│  Operational prerequisites:                                  │
│    P1: Buffer alignment                                      │
│    P2: Object lifetime (C++ language formalism)              │
├─────────────────────────────────────────────────────────────┤
│  Corollary: C1 + C2 + P1 + P2 + P3                          │
│    ⟹ open_impl succeeds, buffer fully operational            │
└─────────────────────────────────────────────────────────────┘
```

### 7.3 Proof Skeleton

```
Step 1 (C1)           same bytes = same values
Step 2 (C2, C2.2)     all references are offset_ptr (no absolute address escape)
Step 3 (C2, C2.1)     offset_ptr resolves correctly after whole-buffer copy
Step 4 (C1+C2, ind.)  structural induction on S: every field preserved
═══════════════════════════════════════════════════════════════════
∴ byte_copy(B) → B' is semantically equivalent to B  ∎
```

### 7.4 Scope and Limitations

- This is a **correctness model**, not a security model (§6 scope note).
- The framework assumes **quiescent buffers** — no concurrent writes during copy.
- TypeLayout provides **complete** verification for user composite types but only
  **outer-shell** verification for library container types (P3 bridges the gap).
- Domain S's exclusion of base classes is **conservative** — future relaxation is
  possible when TypeLayout signature matching is sufficient.

---

## Appendix: Notation Summary

| Symbol | Meaning |
|--------|---------|
| A | Architecture Set (Domain A) — defines valid platforms |
| S | Safe Type Set (Domain S) — defines valid types; shared by C1 and C2 |
| S₀ | Base case: fixed-width primitives |
| TargetArch | Compile-time selected member of A; producer and consumer must match |
| C1 | Layout Determinism — same type, same bytes |
| C2 | Referential Integrity — all pointers survive copy |
| P1 | Buffer Alignment — operational assumption |
| P2 | Object Lifetime — C++ language formalism assumption |
| P3 | Infrastructure Compatibility — same Boost.IPC version assumption |
| offset_ptr | Relative pointer: stored = target − this |
| Lemma C2.1 | offset_ptr survives whole-buffer copy |
| Lemma C2.2 | No absolute address escape in managed buffer |
