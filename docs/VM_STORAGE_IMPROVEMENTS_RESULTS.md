# XBuffer reviewed improvements: implementation and measurements

2026-10-13; local branch `next_cpp26`, starting HEAD
`07d649909d7f43ca0d11b85accb6d9dab991e284`. Changes are uncommitted and unpushed;
no package publication or remote CI was triggered.

## Outcome and scope

The approved [reviewed plan](VM_STORAGE_IMPROVEMENT_PLAN.md) implements A–D:

- **A:** process-lifetime registry synchronization and cv-normalized local root
  identity fix the reproduced shutdown crash and const-handle rejection.
- **B:** fresh-owner load completely copies input before publishing its logical
  extent; verified save fills the final owning string without a temporary
  payload string. Ordinary growth/regrowth still zeroes newly exposed bytes.
- **C:** a synchronized exact-base hash index replaces the linear owner list.
  Registration can fail; move, swap and destruction do not allocate index nodes.
- **D:** vector/string reserve extends a trusted live physical-tail allocation
  when existing logical space suffices. Core and wire allocator metadata stay
  consistent, including oversized reused blocks. Other cases keep their existing
  migration behavior; no implicit `grow` is introduced.

Default capacity and legacy hints, explicit hard limits, stable arena bases,
handle epochs, reflection/type admission, v1 layout/signatures, and corruption
checks remain intact. Container layouts may differ after D while representing
the same values. Save/load remain copying APIs. E is an evidence-based policy
decision, recorded below, rather than an unimplemented mandatory allocator rewrite.

## Frozen stages and reproducibility

V0 is the original `07d6499` vector implementation. V1 is the VM implementation
with A repaired and validated; B adds transport, C adds the index, D adds tail
expansion. V0 is a historical/interchange reference, not the denominator for
these optimization claims.

| Stage | `xoffsetdatastructure.hpp` SHA256 |
|---|---|
| V1 | `3978ba36a2556a17bf84a72cafd724a46bdabfc1b16cacae1337248c37f97dc0` |
| B | `b4be9eb10ef2b402811fcf6a62c0b90a65c794481df77aa8afabb883b7c7089b` |
| C | `13e644973f827bd0f010bede54dcaad4174e3efbb9c1bee2b05961249546f488` |
| D | `896ac72ba55564bfa6177df3b89d7c192e0b2584c092fdb409c63e828bfadfb0` |

B/C/D VM owner SHA256:
`af90d9ea4281cba703878a1042ec63aedeb3fa1807fbe8feacd4b6dfae72b186`.
Full hashes, commands and raw samples are retained in the
[portable experiment evidence](perf/2026-10-13-vm-improvements/README.md).
The earlier [18-case vector/initial-VM comparison](VM_PERFORMANCE_COMPARISON_RESULTS.md)
and its runner/harness are preserved unchanged.

Measurements use macOS 26.6.2 arm64, Apple M3 Pro, 36 GiB RAM, 16 KiB pages,
P2996 Clang 21 `d77eff1cbd78fd065668acf93b1f5f400d39134d`, SDK 26.5, `-O3`,
and the same clean Boost/TypeLayout dependencies for both sides. Each case has
one warmup pair and seven measured alternating before/after pairs, with a fresh
child process per sample. Builds and other test suites were suspended during
measurements. All results include complete value/request-count checks and zero
arena address changes; B/C require exact wire bytes. D permits layout changes
and verifies both writers with both readers, including mutation/save/reload.

The primary matrix consists of 16 B transport cases (224 measured samples),
32 C owner cases (448), 5 D container cases (70), and a separate 61-case V1→D
combined run (854): **1,596 measured samples plus 228 warmup samples**. The
combined run independently repeats each optimization's target workloads.
Transport sizes are 1 KiB, 64 KiB, 8 MiB and 64 MiB; repeated samples perform
64 small or 4 large operations. Owner cases use 1/100/1000/5000 live buffers,
sequential/random order and 1/2/4/8 threads, each with independent buffers.
Container cases use one million values or an 8 MiB string. Eight diagnostics
cover churn, both free-block layers, and shrink/regrow.

## Individual effects

Times are per-case medians in milliseconds. The ratio column is the median
**paired before/after** ratio with its interquartile range; it is not the quotient
of the two displayed medians or a confidence interval. Larger than one means
faster after. Full distributions are in JSON/CSV.

| Change and workload | Before ms | After ms | Paired ratio [IQR] |
|---|---:|---:|---:|
| B: first verified save, 64 MiB | 10.248 | 4.217 | 2.439 [2.343, 2.460] |
| B: first verified load, 64 MiB | 5.963 | 4.563 | 1.324 [1.228, 1.368] |
| C: 5000 owners, one thread, sequential, all four phases | 821.050 | 33.082 | 25.013 [23.919, 25.383] |
| D: single-tail vector, 1M appends | 3.021 | 2.760 | 1.096 [1.083, 1.115] |
| D: same vector, explicit arena growth | 3.933 | 3.258 | 1.203 [1.171, 1.268] |
| D: interleaved vector growth | 3.736 | 3.637 | 1.028 [1.010, 1.054] |
| D: 16 interleaved nested vectors | 3.787 | 3.831 | 0.984 [0.974, 1.000] |
| D: geometric string assignment, final 8 MiB | 0.455 | 0.259 | 1.723 [1.694, 1.755] |

B eliminates one full input memset on fresh load and one temporary payload
copy on verified save. Its supported string path also avoids pre-initializing
the output; a reserve/append fallback was built and checked against the same
wire fixtures. Neither the OS's first-touch work nor the required payload copy
and graph validation disappear.

C's 5000-owner allocation phase changes from 370.199 to 5.453 ms, and free phase
from 368.509 to 3.513 ms. The four-phase total includes creation, allocation,
free and destruction plus barriers/scheduling. It is **not single-lookup latency**;
N=1 is especially dominated by synchronization overhead. The mutex remains, and
hash lookup has expected average rather than worst-case constant complexity.
Hot-path hash probe counts were not instrumented (`owner_probe_count: null`);
scaling measurements must not be presented as observed probe counts.

An independent, untimed `operator new` probe ran each B/C owner count three times
with identical results. At 5000 owners, the index requests 211,368 live heap
bytes (160,000 node bytes plus 51,368 bucket bytes). After all owners are removed,
51,368 bucket bytes remain until process exit. B uses zero additional heap bytes
for its linked index. These figures exclude allocator overhead, payload mappings,
static state and the preallocated owner array; they are **not RSS**.

| D structural effect | C | D |
|---|---:|---:|
| Single-tail vector data moves | 18 | 0 |
| Estimated bytes copied by those moves | 4,194,288 | 0 |
| Single-tail vector `used_end` | 8,389,120 | 4,194,544 |
| Explicit-growth vector final logical bytes | 16,777,216 | 8,388,608 |
| Explicit-growth vector arena grows/retries | 12 | 11 |
| Interleaved vector data moves | 26 | 13 |
| Nested-interleaved data moves | 224 | 224 |
| String data moves | 23 | 0 |

The string case times geometric assignments up to 8 MiB, not just reserve and
not an 8 MiB per-character append loop. Copy-byte counters estimate container
relocation from observed address changes and live lengths, not hardware memory
traffic. Preallocated arena capacity remains identical between sides; reduced
`used_end` does not itself reduce the already enabled pages. D helps eligible
tails; nested/interleaved allocations can prevent expansion entirely.

## Combined behavior and tradeoffs

The separate V1→D run reproduces the main directions: 64 MiB first save
11.429→4.477 ms; first load 4.928→4.367 ms; 5000 sequential single-thread owners
805.696→31.334 ms; explicit-growth vector 4.219→3.525 ms; string 0.469→0.293 ms.
These are measurements from a different run, not numbers to mix into the
individual-stage ratios.

The nested-interleaved case changes 4.048→4.112 ms; its paired ratio IQR
[0.976, 1.002] crosses one. Both runs show a small median slowdown and no movement
reduction in this case; no general nested-container speedup is claimed.

There is a reproducible small-message tradeoff: 64 consecutive 64 KiB loads
change 0.379→0.422 ms in the combined run (paired ratio 0.899, IQR
[0.857, 0.926]). A separate seven-pair repetition of the same frozen binaries
again has its ratio below one (0.874, IQR [0.827, 0.953]). These correspond to
about 11% and 14.5% longer paired total times, not a universal load speedup. Individual
B and a targeted B→C repetition both have intervals crossing one; the experiments
do **not** establish that hash registration alone causes this difference.
The implementation retains this measured tradeoff for the repeatable large-message,
many-owner and eligible-tail gains. Small-message-critical users should evaluate
the supplied workload before adopting it; the root cause remains unresolved.

Two additional V1→D guards compile the unchanged historical harness separately:

| Guard | V1 ms | D ms | Paired ratio [IQR] |
|---|---:|---:|---:|
| 128 MiB raw arena growth, equal reservation | 7.041 | 7.198 | 1.010 [0.995, 1.045] |
| 10M appends, arena preallocated | 27.301 | 21.621 | 1.257 [1.239, 1.283] |

The raw-growth interval crosses one; there is no demonstrated regression or
additional gain in that path. The append guard preallocates the **arena**, not
the vector capacity, so it can benefit from D. Guards, targeted load ablation
and the independent load repetition add 56 measured and 8 warmup samples;
all seven experiment runs total **1,652 measured and 236 warmup samples**.

## E: decisions on optional memory policies

| Item | Observed evidence | Delivery decision |
|---|---|---|
| Free-block split/coalesce | At 5000 mixed Core blocks, 400,000 live user bytes retain `used_end` 7,920,176; wire-container churn has 560,000 live bytes, `used_end` 8,280,240 and 160,000 cached free bytes. V1/D geometry is identical. | Fragmentation remains real. Defer coordinated two-layer allocator redesign; changing only Core best-fit is insufficient, and no split/coalesce prototype or benefit is claimed. |
| Small buffers / VM pool | 8 rounds of 1000 live owners take 49.553 ms at default capacity, 25.381 ms at explicit 64 KiB in D. Concurrent reservations are 62.5 GiB versus 62.5 MiB. Both still enable at least one 16 KiB host page per nonempty owner. | Retain explicit small budgets. Defer bounded pool until a separate workload and lifecycle design justify it; these are fresh mappings, not cold/warm pool results. |
| Physical tail reclamation | Eight save/regrow cycles at 64 MiB payload change 49.929→25.157 ms after copy reduction, while final logical/access/reserved remain 129 MiB. Logical shrink still retains enabled pages. | Keep logical shrink as default. No physical reclamation or idle RSS reduction is claimed; a future opt-in policy needs platform-specific reclaim/failure/regrow measurements. |
| Automatic growth | D reduces the explicit-growth vector from 12 to 11 grows and halves final logical extent, without removing retry semantics. | Keep explicit growth and hard limits. Automatic is an independent feature with a separate exception/publication contract. |

`used_end` is a high-water extent including headers, retired blocks and padding,
not pure wasted or resident bytes. The synthetic fragmentation ratios are not
application-wide memory ratios. Churn comparisons across capacity choices are
descriptive, not a separately randomized paired capacity-policy experiment.
The diagnostic `shrink_regrow` fields `used_end`/`free_bytes` and churn's
`live_payload_bytes` were not collected and contain placeholder zeroes in raw
output; they must not be interpreted as measured zero utilization.
RSS is whole-process Mach measurement; accessible bytes denote permissions,
not resident memory or Windows commit charge. Batch percentiles/maximums from
these small samples do not establish production p99 latency.

## Functional verification

All final checks use D's frozen headers, with no production repair required
after the final native run. Portable commands, logs, source/test snapshot and
hashes are in [validation evidence](validation/2026-10-13-vm-improvements/README.md).

| Environment / check | Actual result |
|---|---|
| Native macOS arm64, full P2996 Release CTest including signatures | 44/44 passed |
| Linux x86_64 P2996, CPU emulation on this arm64 host, full CTest | 44/44 passed |
| Same Linux, ASan/UBSan with leak detection and immediate failure | 24/24 affected tests/scenarios passed |
| Native macOS P2996 ThreadSanitizer, five executables and lifecycle/OOM modes | 12/12 process cases passed |
| Native macOS V0/V1/D readers/writers, two mutation rounds with fresh tail/non-tail reserves | 156/156 process operations passed |
| Independent VM layer, native Apple Clang C++17 ASan/UBSan, warnings as errors | Passed; Apple's runtime does not support leak detection here |
| Independent VM layer, native Linux arm64 GCC 13 C++17 ASan/UBSan with leak detection | Passed |
| Windows backend and full P2996 integration | **Not compiled or executed** |

The 44 CTest cases are 34 library executables, five natural-exit modes, two
actual node/rehash allocation-failure modes, and three signature tooling checks.
Committed signature baselines are unchanged. Native full-library ASan is not
claimed: that P2996 runtime lacks the matching Apple ASan ABI; full integration
ASan/UBSan uses the recorded Linux LLVM 21.1.8 runtime instead.

Coverage includes const local roots through checked/unsafe/Core handles,
incompatible and unverified replacements, global/static destruction after main,
move/swap without allocation, failed registration preserving existing owners,
actual heap node/rehash failure, repeated VA reuse, independent-buffer threading,
load/save allocation and VM failure recovery, over-aligned allocations, and both
free-block layers during tail expansion. An existing wire-corruption fixture
now explicitly forces a non-tail retired block and asserts that its hostile
freelist cycle was injected, preserving its negative-test coverage after D.

The six lifecycle cases fail against an isolated pre-A snapshot and pass after
repair. B's output and fallback output match V1 byte-for-byte for fixed fixtures;
D's 156-operation matrix validates values and continued mutation rather than
requiring byte-identical allocator histories. Final independent static review
found no confirmed production defect. Same-buffer concurrency still requires
external synchronization; future POSIX page-fault OOM is outside recoverable
page-enable failures. Windows and native full-library Linux runs remain coverage
limits, not inferred passes.

## Reproduction

Extract a comparison's `source_snapshots.tar.gz` to obtain both header trees and
its exact harness/runner. With the recorded compiler, SDK and dependencies:

```sh
python3 tools/run_vm_improvements.py \
  --before build/vm_improvements/snapshots/v1 \
  --after build/vm_improvements/snapshots/d \
  --suite all --wire-compat semantic --runs 7 --warmups 1 \
  --output-dir build/vm_improvements/repeat
```

For V1→B use `--suite transport --wire-compat exact`; for B→C use
`--suite owner --wire-compat exact`; for C→D use
`--suite containers --wire-compat semantic`. `--prepare-only` freezes/builds
without timing; `--resume OUTPUT` validates the prepared hashes before running.
Absolute paths in metadata identify the original host; extracted snapshot paths
can replace the example paths. Do not run timing alongside builds or sanitizers.
