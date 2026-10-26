# Changelog

## Unreleased

- Linux output prefetch: in a `save_verified` / `save_verified_into` output of
  at least 1 MiB, each copy of at least 256 KiB into pages that are not yet
  resident is preceded, per 256 KiB chunk, by `madvise(MADV_POPULATE_WRITE)`,
  so the kernel faults the pages in one call instead of one trap per page.
  Reused (resident) outputs and smaller ones are unchanged. Kernels before
  5.14 answer `EINVAL` once, after which outputs are copied plainly. Same wire
  bytes; Darwin and Windows code unchanged. On the formal M final state (12.6
  MB) a new output takes 36 page-fault traps instead of 3083 (cloud, GCC 16).
- Send path: `XSaveOptions{.emission}` picks the wire form of
  `save_verified` / `save_verified_into`; both APIs emit the same bytes for
  the same state.
  - `XWireEmission::free_runs` (default) skips each run of at least 4 KiB of
    adjacent free blocks of at least 256 B. It replaces N10 (which built a
    1 bit / 8 B free bitmap on every save of a buffer with 4 KiB of holes):
    only the bins of at least 256 B and the large blocks are visited, sorted
    by offset with a radix sort and merged. On the formal M final state it
    skips the same run as N10 (about 6x cheaper to find, cloud box), and on the
    golden model its bytes equal the earlier N10 pin. Skipped bytes are all
    holes, so 0.3.0-beta.1 receivers accept it.
  - `XWireEmission::dense`: one copy of the payload.
  - `XWireEmission::zero_runs` skips every zero run of at least 64 bytes
    (capacity slack, holes, zero fields), for cross-machine sends; the
    receiver loads a byte-identical payload, so spare capacity is kept (K5
    unchanged).
  Receivers no longer require gap bytes to lie in holes: gaps load as zero and
  the payload is validated as before, so the payload-level accept/reject set is
  unchanged (fuzzed: every sparse wire has the outcome and error of its dense
  equivalent); the wire-level accept set grows by sparse wires whose gaps
  cover live zero bytes.
- `save_verified_into` into an output whose capacity already holds the dense
  wire examines and normalizes the source once instead of twice.
- New `save_compact<T>(XCompactOptions)` / `save_compact_into<T>(out, ...)`
  write a compact wire (preorder layout, no holes) by direct traversal of the
  source graph, with no temporary buffer; the source is not modified. By
  default vectors of at least 8 elements (not strings) keep size / 8 + 1
  zeroed spare elements (`slack_div = 8`), so a receiver that edits them does
  not relocate on the first insert. With `slack_div = 0` capacity == size and
  the bytes are identical to `XCompactor::compact<T>(b).save_verified<T>()`
  (tested over 600 random models), 7x (840 KB shard) to 85x (1.6 KB) faster
  than compact-then-save on the cloud box.
- Same-process whole-buffer copy: `XBuffer::clone()` (a new owner with the
  source's size, maximum capacity, growth policy and pool) and
  `clone_into(dst)` (reuses dst's mapping and address like `reload_verified`;
  dst's epoch advances; on failure dst is left an empty arena, a self copy is
  refused unchanged). One copy of the used payload; the exact free table,
  typed root access and schema binding come along, so no wire and no
  verification. `TypedXBuffer<T>` has both. See `docs/BUFFER_REUSE.md`.
- Nightly fuzz without GitHub Actions (threat model F4): `tools/fuzz_nightly.sh`
  runs the GCC 16 ASan+UBSan differential fuzz (`test_v2_fuzz`, 4 processes,
  date-based seeds, about 45 minutes) and the replay of
  `tests/fuzz/regressions/` and the seed corpora on any Linux host with
  Docker, writing findings.json per job. `tools/fuzz_report.py` turns those
  (or the Actions artifacts) into one issue per crash signature (`fuzz-crash`:
  a new signature opens it, a repeat comments, a closed one is reopened as a
  regression) and pushes the input to `fuzz-crash/<signature>` as
  `tests/fuzz/regressions/<target>/<signature>`; failures without a finding go
  to one rolling `fuzz-infra` issue, closed by the next green nightly. The
  libFuzzer jobs need P2996 Clang and are not part of this nightly.
- Fuzz regressions (threat model F4): the two libFuzzer harnesses also build
  without libFuzzer as `fuzz_v2_wire_replay` and `fuzz_wire_schemas_replay`
  (any compiler, not gated by `XOFFSET_BUILD_FUZZERS`). CTest replays
  `tests/fuzz/regressions/<target>/` and each harness's seed corpus in every
  CI job. `test_v2_fuzz` draws each case from its own stream of
  (seed, case), reruns one case with `--replay <seed> <case>`, and with
  `XOFFSET_FUZZ_DUMP_DIR` set writes a failing input wire before aborting.
  `-DXOFFSET_FUZZ_INJECT_CRASH=ON` (drill builds only) makes inputs starting
  with `XOFFSET-INJECT` crash the harnesses. The case streams changed, so a
  given seed now generates different cases than in 0.3.0-beta.1.
- Performance: verified loads index the payload with one bit per 8-byte
  granule (live and free bitmaps; inline up to 32 KiB, otherwise one scratch
  block, retained by `reload_verified`) instead of the paged byte index and two
  monotonic resources, and check each container head once (binding, block
  bounds and alignment) before falling back to the full check sequence for its
  error. Accept/reject sets and first error messages are unchanged
  (`tools/validation_differential.sh`, CI job `validation-differential`).
- Performance: canonical wires (no holes, blocks laid out back to back in
  visiting order, as `XCompactor::compact` writes them) are validated with a
  single cursor instead of the bitmaps, and take no scratch memory at any
  size. At the first block elsewhere or the first hole the live bitmap is set
  to the cursor's prefix and validation continues there, so accept/reject sets
  and first errors are unchanged (`test_v2_fuzz` compares every case with the
  cursor off). Map/set key order is checked without an early exit, and
  validation errors are raised from one out-of-line function.
- API: `XBuffer::begin_verified_reload<T>(used, holes, extents)` receives a
  wire directly into an existing buffer (no library copy, no new owner): the
  transport writes the header, extent table, payload (dense, or each extent at
  its offset) and hole table into the token's spans, calls `complete()` with
  the counts written, then `finish()`. Verification is `reload_verified`'s
  (same validator, same accept/reject set, `test_v2_fuzz` acceptor 6); an
  incomplete, aborted or mis-declared delivery rejects and leaves the buffer
  rootless. Nothing is cleared at begin: a dense payload is overwritten whole,
  and only the parts of sparse gaps that earlier content made dirty are
  zeroed at finish. `VerifiedReloadToken<T>`; `XOFFSET_VERIFIED_RELOAD_API`.
- Diagnostics: building with `XOFFSET_VALIDATION_LAYOUT_STATS` counts, per
  root type, canonical / ascending-with-holes / out-of-order wires that pass
  verified validation and prints them to stderr at exit (not thread-safe;
  diagnostic builds only).

## 0.3.0-beta.1: production-readiness beta

Release gates G1–G8 (`PRODUCTION_READINESS_GOAL`): K1–K7 mapped to tests, two
compilers with byte-identical wires, schema pins, threat model with
mitigations, continuous fuzzing, sanitizers on both compilers, client-target
feasibility. Changes since 0.2.0-alpha.1 (which was never tagged):

- Client targets (K8, `docs/CLIENT_TARGETS.md`): CI runs the full CTest on
  native Linux arm64 (GCC 16) and Windows x64 (MSYS2 UCRT64, MinGW-w64
  GCC 16.2) and exchanges the wire corpus with Linux x86-64 (byte-identical,
  mutually accepted). Android and iOS are not feasible yet (no P2996
  toolchain or SDK); see the report. On MinGW the top-level build links the
  runtime statically so test replacements of `operator new` see library
  allocations; the signature exporter writes LF on every host.
- Sanitizers: GCC 16 ASan+UBSan job over the full CTest suite, next to the
  existing Clang ASan+UBSan job.
- `docs/STABILITY_TESTS.md` maps each guarantee K1–K7 to the tests and CI jobs
  that enforce it.
- In-place buffer reuse: `XBuffer::reload_verified<T>(wire)` (same reject set
  as `load_verified`; on failure the buffer is left empty) and
  `XBuffer::reset<T>()`; `reload_verified` reuses its validation scratch
  blocks (`release_reload_scratch()`). See `docs/BUFFER_REUSE.md`.
- Performance: `XVector` middle erase/insert shifts the tail with one
  `memmove` for eligible records (`wire_record_layout<T>::ordinary_members`,
  `stable_erase_if`); host free tables are recycled per thread instead of
  rebuilt per load (`XBufferCore::trim_free_table_cache()`); verified loads
  record ascending live ranges instead of marking the byte index
  (accept/reject and messages unchanged).
- Documentation: USER_GUIDE recommends `XBufferPool` for many small messages;
  K1–K7 and DEPLOYMENT guide; threat model `docs/V2_THREAT_MODEL.md`.

- GCC 16 support for the full library: `<meta>` is used when available
  (`<experimental/meta>` otherwise), and CMake accepts GCC 16+ with
  `-std=c++26 -freflection` and libstdc++.
- Fix: set/map keys of an enum type that defines `operator<=>` but no
  `operator<` were ordered differently by Clang (rewritten `<=>`) and GCC
  (builtin `<`), so each compiler rejected the other's wires. The library now
  calls `<=>` explicitly for such keys; Clang's order, and so its wires, are
  unchanged.
- K7 schema pins: `XOFFSET_PIN_SCHEMA(T, "<signature>")` fails compilation when
  a compiler or target lays out `T` differently from the pinned text, and the
  diagnostic shows the computed layout; `schema_pin<T>(name)` prints the pin.
  See `docs/SCHEMA_PINS.md`. CTest `schema_pin_mismatch` requires the failure.
- Untrusted input (threat model `docs/V2_THREAT_MODEL.md`):
  - M1: `load_verified` / `TypedXBuffer::load` and `begin_verified_receive`
    without an explicit `maximum_capacity` accept a wire that declares at most
    the default maximum (64 MiB, `XOFFSET_DEFAULT_MAXIMUM_CAPACITY`); larger
    declared sizes are rejected before any reservation. Previously a sparse wire
    of a few hundred bytes could make the receiver reserve up to 2 GiB.
    **Behavior change:** pass `XBuffer::maximum_capacity()` for wires above
    64 MiB.
  - M2: the rejection names the declared size and the limit (`XBadAlloc`).
  - M3: a block is cleared when it is retired (freed), so holes and the
    unwritten bytes of reused blocks are zero in the arena itself and every
    output path (`save_verified`, `save_verified_into`, `freeze_verified`)
    stays a plain copy. Holes that a loaded wire brought in keep the sender's
    bytes and are forwarded unchanged on re-save; they are never the
    receiver's memory.
  - M4: container shrink (`clear`, `resize` down, `erase`, `pop_back`,
    `stable_erase_if`, shorter `assign`) zeroes the vacated element and
    character bytes, so spare capacity never carries old values.
  - M6: implicit padding. The library zeroes the padding of every container
    element it constructs or relocates, including map entries (an
    `XMap<std::int32_t, std::int64_t>` entry has 4 padding bytes between key
    and value), so copied temporaries and reused arena blocks no longer put
    the caller's stack or stale bytes into a wire, and the same content
    always yields the same wire. Padding of a record the caller assigns
    directly is not reachable by the library: `has_implicit_padding_v<T>`,
    `XOFFSET_REQUIRE_NO_PADDING(T)` and the opt-in
    `XOFFSET_REJECT_IMPLICIT_PADDING` switch report such types at compile
    time.
  - Wires with holes or spare capacity change bytes (golden values updated).
- New `test_cross_compiler_wire` corpus. CI jobs: GCC 16 build and CTest, and
  a P2996 Clang <-> GCC 16 exchange that requires byte-identical wires and
  mutual `load_verified` acceptance.
- Coverage-guided fuzzing (G5): `fuzz_v2_wire` (libFuzzer, option
  `XOFFSET_BUILD_FUZZERS`) feeds each input to the spec oracle, typed load, raw
  validation and verified receive, which must agree; accepted wires must
  survive read -> mutate -> save -> load byte-stably. It shares its core with
  `test_v2_fuzz` (`tests/fuzz/v2_fuzz_core.hpp`). The new `Fuzz` workflow runs
  it under ASan+UBSan for 5 minutes on pull requests and pushes and for an hour
  nightly, keeping the corpus between runs. `fuzz_wire_schemas` adds three
  schemas (fixed arrays, enums and byte blobs; nested maps with string keys;
  three-level nested vectors) with a differential check across typed load,
  raw validation and verified receive. All acceptors in the fuzz core now
  use one 4 MiB limit, and the oracle rejects wires that declare more.

## 0.2.0-alpha.1: V2 storage and wire format

Experimental prerelease from next_cpp26. Same toolchain requirements as
0.1.0-alpha.1 (pinned clang-p2996). Schema evolution is not supported: a wire is
accepted only when the reader's type layout matches the writer's exactly.

Breaking: the V2 format replaces V1. V1 wires are rejected with
"unsupported wire format version 1"; there is no V1 reader. See
`docs/V2_FORMAT.md`.

- Blocks carry no headers: extents are derived from container heads
  (8-byte aligned, multiples of 8); `XAllocatorStateV1` and in-arena free
  lists are removed.
- Host-side binned free table with exact reuse and splitting; it travels as
  the wire hole table, so free space survives reloads and reloads never scan.
- Validators require live blocks plus holes to tile the payload exactly.
- Strings of up to 10 bytes are stored inline in their 16-byte head.
- The owner is found through a host page before the payload (no registry or
  thread-local lookup on allocate/retire).
- Sparse wires skip free runs of at least 4 KiB; Darwin outputs of at least
  1 MiB are prefaulted in 256 KiB chunks.
- `XFixedVector::assign(first, last)`; move-assignment ownership walk only
  in debug builds (`XOFFSET_MOVE_OWNERSHIP_CHECKS`).
- Public API changes from V1 are listed in `docs/V2_FORMAT.md` section 6.
- Documentation: README, `docs/ALPHA_COMPATIBILITY.md` and `docs/VM_PACKAGING.md`
  describe V2; new `docs/USER_GUIDE.md` (usage, constraints, intended-stable API).
- CI: the main workflow also runs on pushes and pull requests to next_cpp26, and
  adds a Linux ASan+UBSan job (P2996 image, pinned compiler-rt).
- Tests: `test_assign_range` covers the iterator form of `assign(first, last)`.
- `docs/USER_GUIDE.md` section 8 lists the core guarantees K1–K7 (K8, the
  client cross-compile, is a goal) and the optional interfaces E1–E5; new
  `docs/DEPLOYMENT.md` (same-version release, untrusted input, default receive
  limits, client capacity, client-link compression).

## 0.1.0-alpha.1

First alpha packaging of the accepted XOffsetDatastructure implementation.
The production headers and V1 wire format are unchanged by the release packaging.

- Reflected root objects and offset containers in an address-stable VM arena,
  with bounded growth, verified transport, schema registration and handles.
- Explicit growth by default; opt-in Automatic growth, idle-reservation pooling
  and explicit tail-page reclamation.
- Independent C++17 OS storage target and full P2996 C++26 target.
- Relocatable CMake full/storage packages, exact prerelease metadata, bundled
  dependency headers/licenses and multi-translation-unit consumer validation.
- Source and installed-prefix archive tooling, per-file manifests and archive
  integrity audit; compiler, SDK and runtime remain external inputs.

Recorded baseline validation includes native Windows storage, WSL storage/full/
package profiles, WSL network ASan/UBSan and historical macOS evidence. The initial
WSL full 48-test profile and final WSL package/network profile are separate results.
See the source distribution's `docs/VM_PACKAGING.md` for counts and boundaries.
Release-archive validation is reported separately from these baseline results.

This is an experimental prerelease. It does not promise stable source API/ABI
between alphas, general cross-toolchain/schema wire compatibility, Windows full
P2996 support, production asynchronous transport or N7 I/O extensions. The current
WSL F/F calibration did not establish the required noise qualification; no D/F
performance, causal attribution or optimization adoption is claimed. WSL and
Docker remain eligible for subsequent qualified Linux measurements.

Save and load copy bytes. Verified format checks and integrity hashes are not
cryptographic authentication. See `docs/ALPHA_COMPATIBILITY.md` in the source
distribution, or `ALPHA_COMPATIBILITY.md` beside this file in an installed package,
for the type domain, lifetime rules and resource limits.
