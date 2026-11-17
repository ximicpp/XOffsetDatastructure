# 4.0.0 compatibility and input boundaries

(The file name dates from the alpha period.) This pre-release is aimed at early users who pin their
compiler, dependencies and schemas and do their own acceptance testing. The core implementation is
usable, but source API and ABI stability between pre-releases is not promised (the only stability
promises are K0–K9, see `docs/STABILITY_TESTS.md`), nor is long-term wire compatibility across arbitrary
combinations of compilers, standard libraries, schema evolution and layouts. This version uses the V2
storage and wire format (`docs/V2_FORMAT.md`) and does not read V1 wires.

## Version and toolchain

`XOFFSET_VERSION` at the source root (`share/XOffsetDatastructure/VERSION` after install) is the single
text source of the full version `4.0.0`; the CMake project's numeric version is `4.0.0`. Installed
consumers look the package up without a version and then check the full string:

```cmake
find_package(XOffsetDatastructure CONFIG REQUIRED COMPONENTS full)
if(NOT XOffsetDatastructure_VERSION STREQUAL "4.0.0")
    message(FATAL_ERROR "This consumer requires XOffsetDatastructure 4.0.0")
endif()
```

Storage consumers use `COMPONENTS storage` and `XOffset::storage` instead. Numeric, EXACT and version
range lookups are all rejected, so a pre-release is not taken for a compatible stable version. In source
builds the full version variable is `XOFFSET_VERSION_FULL`.

The full library requires 64-bit little-endian, P2996 Clang and a matching libc++; plain C++26 is not a
substitute. The pinned recipes and dependency sources are in the source distribution's
`docs/PACKAGING.md`, `toolchains/README.md` and `toolchains/vm-validation.json`; these recipes are not
copied into the install prefix. Support verdicts are given per concrete profile; full compiler support
cannot be inferred from an OS API being present. On Windows, MSVC only validates storage; the full
library under MinGW-w64 GCC 16 is validated by CI (`docs/CLIENT_TARGETS.md`). Results from WSL and Docker
must keep their actual virtualization/emulation category.

Within one executable, use consistent production headers, TypeLayout/Boost contents, toolchain and
compile options. Macros such as `XOFFSET_DEFAULT_MAXIMUM_CAPACITY` must be consistent across all
translation units. The validated multi-translation-unit install and source consumption does not extend to
ABI guarantees for shared libraries, dynamic plugins, crossing CRTs, mixed standard libraries or mixed
header versions.

## Admission and compatibility of V2 wires

V2 is the current fixed format, wire format version 2.1 (magic bytes `'X' 'O' 2 1`, 40-byte wire header).
V1 wires are always rejected with `wire format 1 is not supported`, and 2.0 wires (3.0.0 and earlier) with
`wire format 2.0 is not supported; this build reads 2.1 (sender and receiver must upgrade together)`; the
library has no read or migration path for V1 or 2.0, and both ends must upgrade together. No schema
evolution in this path, general schema migration or arbitrary ABI conversion is offered: loading requires
matching root/schema/type/layout identities and a valid object graph (the live block and hole tables tile
the used region exactly); registering the same schema name does not make different layouts compatible.
When persisted data must be read across versions, keep a toolchain, schema and conversion program that
can read the original format, and validate the migration result separately.

- Stored types must satisfy `XOffsetDatastructure::is_byte_copy_safe_v<T>`.
- The fixed-schema verified path also requires `is_v1_wire_admitted_v<T>` (the name is kept from the old
  version; the rule applies to V2); this is an extra wire admission constraint and does not mean "every
  trivially copyable C++ type can be used".
- Use the library's arena containers such as XString/XVector. Ordinary process addresses, native
  pointers/references, member pointers and unions cannot enter this admitted wire domain; ordinary
  `std::string/std::vector` are not substitutes for the offset containers.
- Record types must pass the actual admission checks, including standard-layout, non-polymorphic, no base
  classes, alignment no more than 8, and all members recursively admitted. Arrays, scalars, enums and
  container elements are checked recursively too.
- Register each root with `XOFFSET_REGISTER_SCHEMA_NAME(T, "a unique and stable name")`. Do not take a
  name, a single signature or one roundtrip as proof of compatibility with an arbitrary target platform.

The four-target signatures in the repository's `tools/sigs/` are baselines generated from the explicit
wire catalog and target ABI metadata; they are not native export/run evidence for those targets, and do
not establish compatibility of arbitrary user schemas.

`save()`/`save_verified()` use canonicalizing transport semantics: they save the admitted object graph and
support further changes after loading, without promising to keep the original spare capacity or
allocator fragmentation. save produces an owning byte copy; load copies the input into a new
reservation. This is not an external read-only mapping or a general zero-copy load API.

## Verified is not an authentication protocol

`TypedXBuffer<T>::load()` uses the verified path; `XBuffer::load_verified<T>()` validates the wire,
schema, allocator and object graph before publishing the typed root. `save_verified()` checks the typed
root/schema/allocator and canonicalizes the output, but should not be described as a full object graph
scan or authentication of malicious input. `load_unverified()` is only for raw payloads the caller
already trusts.

The signature, FNV and file SHA-256 lists in the format are for identity/integrity checks; they provide no
sender authentication, confidentiality, MAC or digital signature. The format 2.1 wire header has no
checksum field (the `crc32c` field fixed at 0 in the 2.0 header was removed), and no CRC check is done.
When the application needs source authenticity or protection against modification, authenticate in an
outer protocol and protect the corresponding version/schema/length information there; the library's
verified mark does not replace that protocol.

When accepting untrusted input, the caller must bound the total wire length before allocating/loading,
and set a reasonable `XBuffer::maximum_capacity(bytes)`. Separately bound the number of concurrent
buffers, the aggregate memory/virtual address budget, queue lengths, execution time and cleanup after
failure; the validity of a single buffer cannot bound total resource use. The payload limit is
`0x7ffffff8`, but that is not a budget to reserve for each request.

## Lifetime, growth and concurrency

After a successful create/load, the arena base stays stable across grow, logical shrink and save; on
reaching the hard limit or on OS failure it does not switch to a relocating allocation. Container growth,
insert/erase, assignment and replacement can still move or destroy elements; do not keep element pointers
long term because the arena base is stable. Compaction creates a separate buffer.

Container objects themselves should live in their owning arena; pass native strings/views to insertion
interfaces rather than constructing arena-bound XString temporaries on the stack. There is no per-object
delete; the buffer owns all objects. `get_buffer()` is a borrowed view; `copy_bytes()` is the explicit
owning copy.

`XHandle<T>` follows the C++ buffer object and cannot extend its lifetime; the buffer object must be
destroyed after the handle. move/swap/replacement and changes of type/verified state follow the handle
checking rules; a handle cannot be used as shared ownership across owners. Writes need synchronization by
the caller.

The default Explicit growth can throw `XBadAlloc` when the current logical region is too small; Automatic
is still bounded by the hard limit and OS failures. Library container changes keep the logical content
when allocation fails, but capacity and allocator bookkeeping need not roll back; multi-step business
operations, ordinary record assignment and external construction side effects do not become
transactions because of this.

The default maximum is 64 MiB, or the initial length if larger. `maximum_capacity()` is the hard limit;
the old `max_capacity(bytes)` argument is only a reservation hint. `reserved_bytes()` is the page-rounded
virtual address range; `accessible_bytes()` is the readable/writable pages, not RSS. Logical shrink keeps
pages already enabled; explicit reclaim can complete partially, so check the returned result. Pool limits
only bound idle reservations, not live owners or whole-process memory; policies and pools are not
serialized.

## Promises not included in this version

Production asynchronous transport, real business network integration, the three N7 I/O extensions and
performance attribution are not promised by this version. The existing network acceptance is a
synchronous TCP loopback reference plus a separately injected lifetime model. The current WSL F/F
calibration did not reach the planned 2% noise qualification, so no speedup, equivalence, regression or
candidate adoption can be inferred from it. Later Linux measurements may use WSL or Docker; inputs must
still be fixed in advance, the environment recorded, and the noise qualification passed. This version's
documents do not rewrite the scope, failure evidence or start conditions of earlier experiments.

## Address model for directly received data (W9)

A buffer obtained by direct receive (verified receive), like an ordinary load, resolves internal relative
offsets with the flat address model (`origin + delta`), relying on the same disclosed
implementation-defined behavior. The library no longer promises that pointers into received data are
derived from the byte array of the receiving storage; `XBufferCore::canonical_bytes_at` is kept as a
public interface for source compatibility, but no longer takes part in resolution inside the library.
