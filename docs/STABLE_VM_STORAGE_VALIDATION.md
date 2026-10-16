# Stable VM storage: implementation and validation

Validated on 2026-10-13. Starting checkout: `next_cpp26`,
`07d649909d7f43ca0d11b85accb6d9dab991e284`, initially clean. All changes are
local; no package was published, no branch was pushed, and no remote CI was
triggered.

**Follow-up implementation:** the review found global-buffer shutdown and
const-root handle regressions outside the initial suite. Both are now repaired,
and transport, owner indexing and bounded container-tail expansion are
implemented. See [the final improvement report](VM_STORAGE_IMPROVEMENTS_RESULTS.md)
for current validation and measurements. The original 33-test evidence below
is retained as history; it does not validate code added after that run.

## Delivered contract

Owning `XBufferCore`, `XBuffer`, `TypedXBuffer`, create, byte-copy raw and verified
load, and compactor output all use `VirtualMemoryStorage`. Successful growth,
logical shrink, and save retain the base of the same storage. There is no
whole-buffer growth copy, reallocation, `mremap`, or vector/malloc fallback.
Reaching the reservation limit or failing enable/preparation reports failure
with the old logical bytes and allocator state still usable. Compaction creates
new storage; replacement releases the old one.

The default limit is 64 MiB, adjustable with
`XOFFSET_DEFAULT_MAXIMUM_CAPACITY` or `maximum_capacity(bytes)`. The explicit
limit is bounded by `MAX_PAYLOAD` (`0x7ffffff8`); page rounding does not enlarge
it. Legacy `max_capacity` and numeric parameters are minimum hints used when
selecting `max(initial, default, hint)` as the new reservation's limit.

`get_buffer()` now returns a borrowed span, and `copy_bytes()` explicitly copies
logical bytes. Logical, accessible, reserved, and maximum extents are separate.
The first version retains enabled pages during logical shrink. Save/load retain
v1 wire layout and copy their output/input; they do not export reservation slack.
Internal string/vector reserve, insertion/erase, rebuild, and replacement still
may move elements. Only the arena base and existing live allocations receive
the storage stability guarantee.

## Historical initial-VM execution

| Environment | Toolchain / checks | Result |
| --- | --- | --- |
| macOS 26.6.2, arm64 | Unmodified baseline, P2996 Clang 21 (`d77eff1cbd78fd065668acf93b1f5f400d39134d`) | 31/31 CTest passed |
| macOS 26.6.2, arm64 | Initial-VM full P2996 Release suite; assertions enabled in tests | 33/33 CTest passed |
| Local Linux x86_64 container, CPU emulation on arm64 host | P2996 Clang 21 (`a5270822dede40aaaf2c62381b89f110d2cfda4c`), Debug | 33/33 CTest passed |
| Same Linux x86_64 container | Original P2996 compiler, ASan/UBSan, LLVM compiler-rt 21.1.8 | 12/12 affected integration tests passed |
| macOS arm64 | P2996 stable-storage integration with ThreadSanitizer | Passed, including four independent buffer threads |
| macOS arm64 | Independent VM layer, ordinary Apple Clang 21, C++17, warnings as errors, ASan/UBSan | Passed |
| Local Linux arm64 container, no CPU emulation | Independent VM layer, GCC 13.3, C++17, warnings as errors, ASan/UBSan | Passed |
| Local Linux x86_64 container, CPU emulation | Independent VM layer, Clang 21 C++17 and GCC 13.3 ASan/UBSan | Passed |
| Windows | Backend implementation and `VirtualQuery` test branch supplied | Not compiled or executed here |

Full CTest discovery was checked, not inferred from build success: 7 basic,
22 reflection integration, 1 independent storage test, and 3 tooling checks
total 33. The three tooling checks cover signature regeneration, matrix inventory,
and compatibility; committed signature baselines have no changes. CI's source
inventory also recognizes every one of the 30 library test files.

The Linux sanitized integration selection consists of `test_nested`,
`test_compaction`, `test_xbuffer_api`, `test_xhandle`, `test_error_paths`,
`test_complex_nesting`, `test_adaptive_reservation`, `test_main_containers`,
`test_runtime_regressions`, `test_mutation_failures`, `test_wire_validation`,
and `test_stable_storage`. Leak detection and immediate ASan failure were enabled;
UBSan uses immediate failure and `-fno-sanitize-recover=all`. Separate programs
compiled with actual P2996 reflection syntax verified that the selected runtimes
detect a heap buffer overrun and signed integer overflow, respectively.

`stable_storage`, `helloworld`, and `xoffsetdatastructure_demo` all ran successfully
on macOS. The new `stable_storage` example also ran on Linux. A separate P2996
configuration probe verified a 128 KiB compilation default, explicit smaller
per-buffer limits, larger legacy hints, and exact-limit failure.

## Evidence and coverage

The final completion audit found and fixed an additional alignment defect:
the allocator had rounded relative offsets instead of actual addresses. A
64 KiB request returned an address with a 16 KiB remainder when the reservation
base was not 64 KiB aligned. Allocations now check address addition and padding
before aligning the actual address; free-block reuse and root lookup validate
that address too. The regression covers 64/128 KiB allocation, reuse, local
over-aligned roots/handles, exact capacity, invalid alignment, and a 4 GiB type
rejected before byte-count narrowing. Restoring the old alignment code in an
isolated copy makes this formal regression fail. The working tree was never
reverted.

The same audit made raw and verified save/load/regrowth explicitly run three
rounds each, including mutation, re-save, re-load, further growth, and continued
mutation. Every growth compares the complete old logical payload and allocator
state and retains the same base/root/allocator addresses and epoch.

Both changes were followed by fresh complete macOS and Linux P2996 runs
(33/33 each), Linux ASan/UBSan (12/12), native ThreadSanitizer, all three native
examples, and a new old/new wire interoperability check. The native and TSan
runs covered 14 reservation bases not aligned to 128 KiB; both Linux runs
covered 16 such bases. Source-placement coincidence therefore did not hide the
alignment defect.

### Numbered requirement audit

The second report's twelve validation requirements map to direct source and
runtime evidence as follows. Integration functions are in
`tests/test_stable_storage.cpp`; OS functions are in
`tests/test_virtual_memory_storage.cpp`.

| Item | Direct coverage |
| --- | --- |
| 1. Small start, repeated cross-page stable growth | `page_boundaries_and_capacity`, `test_growth_and_hard_cap` |
| 2. Objects, relative references, allocator state | `objects_and_relative_references_survive_growth`; old payload and allocator byte comparisons in transport cycles |
| 3. Independent object address | `StableScalar` root and eight separately allocated `int64_t` values retained across growth |
| 4. Page/zero/exact-limit/over-limit cases | `page_boundaries_and_capacity`, `test_empty_and_geometry`, `test_growth_and_hard_cap` |
| 5. Arithmetic, alignment, and narrowing | `actual_addresses_honor_large_alignment`, container/allocator oversize rejection, `test_overflow_and_input_validation` |
| 6. Reserve/enable/post-enable failures | `deterministic_growth_failures`, `test_failure_paths`; complete old payload/state comparisons and successful retry |
| 7. Repeated shrink/save/load and growth | Three shrink cycles and three raw plus three verified transport cycles in `shrink_regrow_and_transport` |
| 8. Ownership and handles | `test_move_swap_and_release`, `handle_storage_identity`, `handles_reject_different_root_types`; includes Core transfers, matching epochs, and OS release queries |
| 9. Container-local moves remain valid | Vector and string reserve move their data blocks while their arena and root remain fixed; original container/nested/mutation suites also run |
| 10. Existing wire and logical-only export | Frame-length/reservation checks, mutation after load, signature suite, latest old/new reader/writer comparison |
| 11. Actual reserved/access distinction | `query_page_access` performs native Mach or Linux mapping queries; never-enabled and enabled tails are checked |
| 12. No old-payload growth copy | `XBufferCore::grow` only calls `grow_to`; that function enables pages and initializes `[old logical, new logical)` without moving or copying old bytes |

The A–E deliverables are also present: current-implementation analysis and
contract in the design document, migration guidance, the independent OS owner
and tests, Core/XBuffer/load/save/compactor integration, lifecycle/failure/wire
tests, and updated runnable examples. Windows execution remains an explicitly
reported platform coverage limit, as the second report permits. Optional or
conflicting first-report extensions are distinguished in the design document;
they are not silently described as implemented.

- `test_virtual_memory_storage` queries actual page permissions through
  `mach_vm_region` on macOS and `/proc/self/maps` on Linux. It confirms a
  read/write prefix and inaccessible reservation tail; these tests do not
  infer reservation behavior from RSS. Observed page sizes were 16 KiB and
  4 KiB, respectively. Windows uses `VirtualQuery` but was not executed.
- Both VM and library tests exercise page-internal, exact-page, cross-page,
  zero growth, non-page-aligned exact maximum, oversize, pointer/length and
  page-rounding overflow rejection, and repeated shrink/regrow zeroing.
- Deterministic one-shot reserve, enable, and post-enable preparation failures
  leave old payload bytes, logical extent, root, and allocator state usable.
  The preparation case additionally checks that newly enabled pages are recorded
  without publishing them as allocator capacity. No actual memory exhaustion
  was used to trigger these failures.
- Independent root and allocator allocations retain addresses, contents,
  bindings, relative references, and allocator state across growth. Container
  reserve separately demonstrates that element storage can still move.
- Move, swap, repeated assignment with matching source epochs, self move,
  moved-from handles, incompatible root replacement, raw replacement,
  and transfers through Core references have regressions. Checked handles reject
  incompatible or unverified replacements. The synchronized registry uses
  immutable reservation geometry; a four-thread independent buffer workload
  passes ThreadSanitizer.
- Save/raw load/verified load/regrow, normalized header fields, compaction as
  separate storage, borrowed spans, and independent output copies are covered.
  Neither spans nor save include inaccessible or reserved tail bytes.
- Before editing storage, an old writer/reader binary produced a 352-byte
  registered-root sample. The old writer's output was read and mutated by the
  new reader; the new writer's output was read and mutated by the old reader.
  The two emitted frames were byte-identical on macOS.
- Source review confirms `XBufferCore::grow` calls only `grow_to`; that function
  enables tail pages and initializes only the newly exposed logical interval.
  The allocator has no throwing segment-resize update after storage publication.
  Load/save and container-local moves retain their expected, explicit copies.

## Reproducing the local macOS build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/local/bin/clang++ \
  -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
cmake --build build -j 4
ctest --test-dir build -N
ctest --test-dir build --output-on-failure -C Release
build/bin/Release/stable_storage
build/bin/Release/helloworld
build/bin/Release/xoffsetdatastructure_demo
```

The original cached SDK path did not exist. The installed default 27.0 SDK then
exposed an unsupported TAPI architecture to the installed linker. Selecting the
available 26.5 SDK resolved the environment issue while keeping the P2996 compiler,
reflection flags, assertions, schema validation, and signature checks enabled.
The quick build script automatically chooses the machine's default SDK; use the
explicit CMake command above on this machine until that SDK/linker pair matches.

The P2996 installations did not bundle ASan/UBSan runtimes. The Apple ASan runtime
has a different compiler ABI marker and was not used to claim a native full-library
ASan pass. Instead, the Linux run used the official LLVM 21.1.8
`libclang-rt-21-dev` package from `apt.llvm.org`, SHA256
`d6f84c34953b0404ec9648ba7364004a82454f79cc447f67894b9a27317ef6ce`.
It was downloaded and extracted only under the ignored build directory. A
resource directory supplies those libraries while keeping the original toolchain's
builtin headers, P2996 compiler, C++26 mode, and libc++.

Local logs reside under the ignored `build/` directory: `baseline-build.log`,
`baseline-ctest.log`, `stable-vm-build.log`, `stable-vm-inventory.log`,
`stable-vm-ctest.log`, `thread-sanitizer.log`, the example logs, and
`wire-interop.log`, plus
`linux-vm/{compiler,platform,configure-final,build-final,ctest-inventory,ctest}.log`.
Linux sanitizer configuration/build/test logs reside in `linux-vm/sanitized/`;
runtime provenance and negative detector probes are in `linux-vm/runtime/`.

That initial-VM audited source is identified by `build/goal-audit-sources.json`.
Final macOS evidence is in `goal-audit-{build,inventory,ctest,last-test}.log`,
`goal-audit-thread-sanitizer.log`, `goal-audit-wire-interop.log`, and the three
`goal-audit-*` example logs. `goal-audit-alignment-old.log` preserves the isolated
negative alignment regression. Final Linux evidence is in
`linux-vm/goal-audit-final-{build,ctest-inventory,ctest,last-test,source-mtime}.log`
and the corresponding files under `linux-vm/sanitized/`. Older logs are retained
but are not used as proof for changes made after their compilation.

## Discovered issues corrected during validation

The new lifecycle checks exposed unsafe cached handle interpretation after
replacing storage with another schema or raw bytes. Checked resolvers now
revalidate the current type and access state. Root type identity is bound to the
logical reflected type, including roots without registered schema names, and
schema/access state follows storage even through Core move/swap.

Shared registry lookup now holds a lock and reads immutable reservation bounds,
so independently growing buffers do not race on each other's logical size.
The new OS test was added to CI's inventory collection. An old Linux-only test
failure was corrected by acknowledging that `int64_t` can be a typedef of `long`;
the library's fixed-width admission predicate was not weakened. Examples now
use verified load/save, expose logical capacity before post-save mutation,
and describe stable arena addresses and copying transport accurately.

## Coverage limits of the initial validation

Windows and native Linux x86_64 execution are not available here; the x86_64
Linux integration runs use CPU emulation. Linux arm64 testing covers the OS layer,
not the full reflection library. That validation did not establish physical
decommit/RSS reduction, native cross-platform wire interchange, or performance
improvement. Subsequent native macOS performance evidence is in the linked
improvement report; it does not remove the platform limits. Logical
shrink retains pages. Future physical-memory failure during a POSIX page fault
is outside the recoverable OS enable/preparation contract. Same-buffer concurrent
mutation/destruction still requires external synchronization.
