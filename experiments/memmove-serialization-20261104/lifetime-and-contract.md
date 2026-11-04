# Controlled wire construction and one-copy image contract

This implementation adds opt-in interfaces. It does not change the representation,
default emission, admission, or validation of existing XBuffer entry points.

## Interfaces

`xoffset_dense_export.hpp` provides `dense_wire_size`, `emit_dense_into`,
`pack_dense`, `PackedWire<T>`, and `copy_packed_into`.

`emit_dense_into` writes three prepared spans: header, payload, holes. It copies
the entire payload once. `pack_dense` makes an immutable contiguous frame once;
`copy_packed_into` copies that frame with one memmove. Pack preparation is O(B+H)
and remains charged to the first use. The complete source arena, including the
48 unsent header bytes, is protected against output aliasing.

`xoffset_image_builder.hpp` provides the experimental `image::ImageBuilder<T>`.
It constructs the final Wire 2.1 frame directly, without constructing an owning
T inside it. It permits ordinary aggregate records, scalars, arrays, XString and
XVector recursively. Each record needs the existing `wire_record_layout<T>`
ordinary-members attestation. Pointer/reference/cv members, inheritance, unions,
bit fields and custom activation policies are rejected. Dynamic vectors can be
initialized once; heap strings can be rewritten within their initial capacity.
There is no erase, graph move, allocator replay, or general ImageEditor.

The caller sets values through offset handles. No T&, mutable byte span, raw
container descriptor, or arbitrary-wire constructor is public. Scalars are
supplied as valid native values. Bulk scalar input must designate live,
initialized native elements for the duration of assignment. A caller must not
mutate a builder concurrently with another operation on that builder.

```cpp
namespace xo = XOffsetDatastructure;
namespace image = xo::image;
struct Row { std::uint64_t id; xo::XString name; };
struct Message { std::uint64_t epoch; xo::XVector<Row> rows; };
XOFFSET_REGISTER_SCHEMA_NAME(Message, "example.ImageMessage.v1")
namespace XOffsetDatastructure {
template<> struct wire_record_layout<Row> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<Message> { static constexpr bool ordinary_members = true; };
}

image::ImageBuilder<Message> builder(64 * 1024);
auto root = builder.root();
root.get<0>().set(123);
auto rows = root.get<1>().resize(1);
rows[0].get<0>().set(7);
rows[0].get<1>().assign("hello");
auto wire = std::move(builder).seal(); // fixed header writes; no graph walk/copy

image::PreparedImageSlot<Message> destination(wire.bytes().size());
auto received = image::copy_image_into(wire, destination); // one memmove
auto row = received.root().get<1>()[0];
assert(row.get<0>() == 7 && row.get<1>() == "hello");
```

`xoffset_image_view.hpp` provides `PreparedImageSlot<T>`, `ImageView<T>`, and
`copy_image_into`. This is a read-only byte proxy, not a native owning T. Both
controlled builder output and prepared verified snapshots can produce its
`PackedWire<T>` source. There is no trusted flag or arbitrary-byte overload.
New external input still goes through the existing full validator.

## Construction invariant

The root starts at logical payload offset 48 (wire offset 40). Root/block sizes
are rounded to eight bytes. All inline root bytes start zero; on the matched
Wire 2.1 ABI this represents valid zero scalars, empty descriptors and padding.
Ordinary record layout and supported fields are checked recursively at compile
time; the non-overlap attestation is a source contract, as in raw receive.

Every dynamic block is appended to previously unused bytes and immediately
linked from exactly one existing head. The block is never shared with a second
head, detached, shrunk, or overwritten by an allocation. A vector allocates its
complete capacity equal to its size, initializing all elements before linking.
Nested heads start empty. Strings use the current <=10 byte inline encoding or
an exact capacity+NUL allocation; unused rounded bytes are zero. Scalar bulk
assignment copies initialized elements and zeros only alignment padding.

Consequently, root + live reachable blocks tile the payload with no holes or
overlap. Bounds and rel32 representability follow from the fixed size limit
below the current payload maximum. Every handle carries a storage identity and
an offset, so string reallocation cannot invalidate its address calculation.
Mutations that could orphan an allocation are rejected. A public bulk-assignment
template cannot substitute a different element type.

All fallible allocation is before linking a new block. Reentrant mutation is
rejected while preparing an allocation. A failed append leaves no unreachable
block in the logical wire. Seal checks the builder state, writes the fixed
40-byte header and transfers an existing shared owner. It allocates nothing,
walks no fields and copies no payload. Retained builder handles reject mutation
after seal. Initial allocation, zeroing, user value writes, scalar bulk copies
and producer cleanup remain construction costs, not free serialization work.

## Receive lifetime argument

The destination is a real byte array owned by a shared control block. The
receive operation copies the *entire* immutable dense frame and propagates only
fixed metadata (typed provenance, root offset, used length and storage owner).
No XString, XVector, user T, or dynamic T[capacity] is ever treated as a live
object in the copied array. Thus this branch does not rely on beginning any
nontrivial container or array-element lifetime by memcpy or reinterpret_cast.

Every descriptor/scalar read is a fixed-size memcpy into a separate live,
trivially copyable local object. That local object, not bytes in the array, is
then accessed with its type. Record/array traversal uses byte offsets and the
compile-time layout of T. Signed rel32 displacements remain unchanged because
all payload bytes move by the same displacement. Removing the arena header and
adding the wire header shifts both ends equally by -8. C++ pointer arithmetic
stays within the destination byte array; no owning-head typed pointer or dynamic
T* arithmetic is formed. Only live sequence indices may be read, so spare bool
or enum representations are never evaluated.

Provenance has two origins: the existing fully verified immutable snapshot, or
the closed controlled builder above. Full verification proves scalar validity,
bounds, ownership and shape for the first origin; construction establishes them
for the second. The complete immutable frame is copied, so those byte properties
are address-independent. Schema/ABI/byte order must match. This is not an
authentication or remote trust protocol.

An ImageView pins destination storage, even if the slot or original producer is
destroyed. Reuse is rejected while any ImageView exists. Child proxies and
string_views borrow that view; callers must retain it while using them, exactly
as for a span. Reusing a slot is single-threaded. Bytes past the copied logical
length are not exposed, and a later larger copy overwrites every newly exposed
byte; old dirty tails cannot leak through this interface.

Preflight rejects empty/moved sources or slots, insufficient capacity and a slot
with outstanding views before writing any byte. The successful copy/commit has
no allocation, graph walk, user callback, allocator reconstruction or object
activation. Unlike existing destructive reload, a preflight rejection leaves
the destination untouched; no fallible publication step follows the memmove.

## Explicit borrowed receive alternative

`image::BorrowedImageBuffer<T>` is the second, separately measured receiver:

```cpp
image::BorrowedImageBuffer<Message> receiver(wire.bytes().size());
receiver.reload(wire); // one memmove, fixed metadata stores, no refcount
auto epoch = receiver.epoch();
auto root = receiver.root(); // borrowed proxy, retain receiver while using it
assert(root.get<0>() == 123);
// Finish using root/children/strings BEFORE the next reload or move.
```

This exclusive owner has the ordinary XBuffer/span borrowing contract: reload,
move and destruction invalidate old borrowed proxies and strings. The caller
excludes concurrent access and stale borrows. It does not automatically pin
children or reject outstanding borrowed references. `root()` and `bytes()` on
temporary buffers are deleted. Empty/moved sources, moved buffers and inadequate
capacity reject before writing, preserving the previous content and epoch.

The original shared-owner ImageView API remains available when a consumer must
retain independently owned storage or automatically prevent slot overwrite.
The borrowed interface does not silently replace that API. Its optimized reload
has one memmove, fixed checks and metadata stores, and no atomic refcount, graph
walk, allocation or system call on success. The two receive contracts have
separate performance results; samples are never pooled between them.

## Complexity and verification boundary

Build is O(bytes written + construction operations). Seal is O(1). Immutable
copy is one full-frame memmove plus fixed checks/refcount/metadata operations,
independent of node count and hole count. Querying a sequence is O(elements
visited), with fixed-size POD reads. Query cost is measured separately; the
proxy is an explicit contract and not a silent implementation of native T&.

The strict claim is for a process-local controlled/verified immutable source
and a prepared destination. It does not extend to arbitrary external input,
existing mutable owning receive, network/kernel zero-copy, cold page costs,
producer business construction, or the missing general ImageEditor. Those
operations keep their costs and their original checks.

Relevant standard rules: [cstring.syn](https://eel.is/c++draft/cstring.syn),
[intro.object](https://eel.is/c++draft/intro.object), and
[basic.types.general](https://eel.is/c++draft/basic.types.general). This proxy
deliberately avoids claiming that implicit object creation solves the current
owning representation's nonimplicit lifetime frontier.
