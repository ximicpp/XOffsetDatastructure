# V1 Correctness Repair

Scope: findings from the review of commit `22330049`. Keep the frozen container
layout and explicit whole-buffer growth model. TypeLayout's opaque facility is
outside this change.

- [x] Enable assertions in all test configurations and repair stale tests.
- [x] Validate allocation sizes, free-block reuse, and buffer owner lifetimes.
- [x] Remove arena-bound stack temporaries from admitted container paths; fix empty strings and aliasing.
- [x] Make supported container mutations transactional on allocation failure.
- [x] Verify wire ranges and allocation metadata before following their contents.
- [x] Apply schema checks consistently to normal typed APIs.
- [x] Run Debug/Release regressions, malformed-input tests, and available sanitizers.
- [x] Re-run the existing performance workloads and document remaining limits.

Allocation failure must preserve logical contents of the individual supported
operation, but may change capacity or allocator bookkeeping. A sequence of user
operations is not a transaction. Retry only the failed operation after explicit
`grow()` and reacquiring the root or handle. No implicit callback replay is added.

Native target baseline harvesting remains separate from these runtime repairs.

## Implementation

- Free-list classes are search buckets, not guaranteed allocation sizes. Reuse checks actual bytes and alignment before unlinking a node.
- The host-side owner registry uses intrusive links, with no allocation during noexcept registration/move. Move assignment invalidates the destination handle cache; moved-from handles resolve to null.
- Counts are checked at host width before conversion/multiplication. The payload limit is `0x7ffffff8`; invalid growth returns false without changing the buffer.
- New owning values are staged inside the arena. In-arena relocation rebases ownership heads without copying nested payloads. Recognized records/containers can therefore reserve and erase without deep-copying their contents. Copying/resize fallback uses a replacement range and cleans up partially built members on exceptions.
- The validator protects the arena header, allocator, root, allocation headers, live blocks, and free blocks. It checks offsets before dereference, cycles, overlap, size classes, accounting, bool representations, and ordered-container invariants.
- `handle<T>()` and `save_verified<T>()` enforce the bound schema. Raw-loaded payloads cannot be relabeled as verified by saving them. Failed duplicate `make<T>()` does not replace the schema identity.
- Derived container classes are not mistaken for the exact frozen container family; added fields cannot disappear from admission checks.
- Frozen wire layouts/signature baselines and TypeLayout's opaque mechanism are unchanged.

## Verification: 2026-09-27

Host: macOS arm64; P2996 Clang 21 (`d77eff1c`), libc++, C++26 reflection.
The working SDK is the CommandLineTools MacOSX15.4 SDK. Builds use
`DEVELOPER_DIR=/Library/Developer/CommandLineTools` because the installed Xcode
developer-tools launcher fails on this host.

| Configuration | Result |
| --- | --- |
| Release, assertions enabled | 31/31 passed |
| Debug | 31/31 passed |
| Debug + UBSan, fail on first diagnostic | 31/31 passed |
| Signature regeneration / matrix / compatibility | Passed in all three runs |

The new tests exercise these concrete cases:

- `test_runtime_regressions`: undersized free-block reuse, buffer moves, typed load followed by mutation, empty/self-aliased strings and vectors, nested payload ownership, count overflow, schema access guards.
- `test_mutation_failures`: 11 operations over 65 arena-slack levels (715 cases). There were 171 allocation failures preserving the original logical contents and 544 successful first attempts. Failed operations were saved/loaded, explicitly grown and retried; successful results were saved/loaded again.
- `test_wire_validation`: explicit hostile-offset/metadata/cycle/order cases plus 1,312 single-byte perturbations. 939 were rejected; 373 valid or harmlessly changed images could grow, mutate, and pass a second verified round trip.
- Existing tests no longer depend on disabled Release assertions, obsolete opaque container signatures, allocator-bound stack strings, or raw pointers retained across save relocation.

Reproduction example (change the build directory/type for Release):

```sh
env DEVELOPER_DIR=/Library/Developer/CommandLineTools cmake -S . -B /tmp/xoffset-repair-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/local/bin/clang -DCMAKE_CXX_COMPILER=/usr/local/bin/clang++ -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk
env DEVELOPER_DIR=/Library/Developer/CommandLineTools cmake --build /tmp/xoffset-repair-debug -j 8
ctest --test-dir /tmp/xoffset-repair-debug --output-on-failure
```

UBSan additionally uses:
`-DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -resource-dir=/Library/Developer/CommandLineTools/usr/lib/clang/21"`.

## Performance Smoke Run

The unchanged default `bench_compare` workloads all completed. Raw output is in
`V1_CORRECTNESS_REPAIR_BENCH.tsv`; its `unknown` label is the benchmark's default
version label for a normal local build. These are current-tree timings, not a
new controlled comparison against the historical Boost tree.

| Workload | Median ms |
| --- | ---: |
| 100,000 integer appends | 0.476 |
| 20,000 nested item appends | 2.264 |
| 12,000 map inserts + lookups | 2.774 |
| 120,000 string assignments | 1.326 |
| Build and save snapshot | 6.387 |
| 40 raw snapshot loads | 2.923 |
| Fragment and compact snapshot | 22.215 |

## Evidence Limits

- ASan did not run: the P2996 installation lacks its ASan runtime, and both installed Apple runtimes fail linking with the required `__asan_version_mismatch_check_v8` symbol. UBSan is an executed check, not a substitute claim of ASan coverage.
- Windows/Linux/iOS/Android native binaries were not run here. Existing signature-matrix checks are not native execution evidence.
- The mutation corpus is finite, not a formal proof. Normalized transport still does not promise allocator snapshot fidelity or identical allocation success with the same spare capacity.
- Raw/unsafe access remains a trusted-input escape hatch. User record assignments, multi-operation callbacks, and external side effects in user constructors are not rolled back by container exception guarantees.
