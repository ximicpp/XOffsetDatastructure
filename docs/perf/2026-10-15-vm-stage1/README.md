# VM stage 1 measurement foundation — 2026-10-15

This is a new investigation of the historical D→F first-save and tail-append
observations. Historical evidence remains unchanged. The implementation headers
are unchanged D and F; this experiment adds a runner and evidence, with no
performance fix or new policy. **The recorded windows are diagnostic only:
background contention violated the quiet-host precondition.** Another task's
container was active in OrbStack (confirmed by the coordinator after window 1).
It was left running. This dataset cannot establish 2–3% non-regression,
equivalence, or a source-level cause. Final counts and results appear below.

## Observed results and decision

Three separately launched windows produced **252 measured samples and 36 warmup
samples**. All 288 samples passed full semantic/work/layout identity checks.
For each case D and F also produced the same wire checksum and length. Both
source headers match the historical D/F fingerprints; no implementation change
was made. The evidence audit recomputes every summary exactly and verifies the
archived sources, raw hashes and binary labels.

The values below are medians of **paired before/after ratios**, not ratios of
overall time medians. A/A is F/F with identical executable bytes; D/F compares
the frozen historical stages. Values below 1 indicate the after run was slower.

| Workload / metric | A/A W1 | A/A W2 | A/A W3 | D/F W1 | D/F W2 | D/F W3 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Single first save, 64 MiB | 1.0148 | 0.9450 | 0.9962 | 1.0274 | 0.9918 | 1.0695 |
| First call of four saves, 64 MiB | 1.0265 | 1.0278 | 0.9777 | 1.0139 | 0.9457 | 0.9991 |
| Mean of save calls 2–4, 64 MiB | 0.9983 | 1.0835 | 1.0642 | 1.0009 | 0.9324 | 1.0020 |
| Tail append, 1M values | 1.0009 | 0.9969 | 0.9863 | 0.9856 | 0.9789 | 0.9880 |

The first-save D/F direction did not repeat. Tail-append paired medians indicate
approximately 1.46%, 2.15% and 1.22% more time for F, but the third-window A/A
median alone indicates approximately 1.38% more time for the identical F binary.
The paired ranges and order strata further overlap. Consequently neither an
underlying tail regression nor its absence is established at the 2% resolution.
The historical 3.2%/2.4% observations remain intact and **are not explained or
retroactively invalidated by this experiment**.

Noise is material: single-first-save A/A window 2 reports an apparent 5.82%
slowdown between identical binaries, and its descriptive bootstrap interval
for the ratio (0.8796–0.9870) excludes 1. Such an interval cannot substitute for
control of the host, independence assumptions or repeated consistent behavior.
No code-level cause, stable speedup, non-inferiority or production p99 is claimed.
No ablation or performance fix is justified by this dataset.

| Window (UTC+08:00) | Start | End | Gap since previous end | Start load average (1 min) |
| --- | --- | --- | ---: | ---: |
| 1 | 17:56:37 | 17:57:00 | — | 4.98 |
| 2 | 18:00:11 | 18:00:35 | 191.1 s | 3.62 |
| 3 | 18:02:06 | 18:02:30 | 91.7 s | 4.42 |

The first window recorded OrbStack Helper at 99.9% CPU at the start and 131.3%
at the end, plus active desktop processes. The host reports no recorded thermal
warning, which does not certify a fixed clock speed or no contention. All
windows therefore carry the same background-contention limitation. The
machine-readable decision is in [interpretation.json](interpretation.json).

**N3 status:** the reproducible runner, A/A calibration, three separate windows,
raw paired distributions, audit and metric definitions are delivered. The
quiet-host prerequisite and source-level regression attribution remain unmet.
Repeat the same frozen protocol on a genuinely quiet host before deciding
whether further repetitions or narrow ablations are warranted. Mixed-capacity
pool and identical-trace Automatic studies remain separate later work.

## Design fixed before timing

- Same compiler executable, `-O3 -DNDEBUG`, C++26 reflection/expansion options,
  macOS 26.5 SDK, libc++, Boost and TypeLayout revisions for both variants.
  `metadata.json` records exact commands, executable/runtime hashes and source
  hashes. The compiler is invoked as `clang++`; resolving that symlink and
  invoking `clang-21` changes implicit C++ linkage.
- The **unchanged** `bench_vm_improvements.cpp` and `bench_vm_compare.cpp` are
  frozen with both headers. D and F are compiled once before any window.
  A/A invokes the exact same F executable for both labels; it does not compare
  two independent F builds. It estimates scheduling/process/host variability for
  this runner, not compiler/build reproducibility or a causal noise model.
- Each sample is a fresh child process. Each window has one warmup pair and
  seven measured pairs for each of three cases and each comparison (A/A, D/F).
  Cases rotate between windows. Within a case, paired endpoint order and
  comparison order alternate. Those two orders are coupled by the schedule;
  order strata are descriptive, not a fully randomized factorial experiment.
- Three windows are launched separately, with at least 60 seconds between the
  previous window's completion and the next start. Builds and other benchmarks
  for this task are excluded by coordination; an unrelated container remains
  active. Start/end process names, load average, thermal,
  swap and power observations are recorded. Ordinary macOS applications and
  scheduler placement are not controlled; no CPU affinity is imposed. Separate
  wall-clock windows do not imply independent machines or thermal resets. The
  runner's `quiet-note` field and "quiet window" console label are operator
  annotations, not automatic certification that the host is quiet. Window 1's
  note predates discovery of the unrelated container; its recorded process
  observations already expose the contention.
- The predeclared resolution of interest is 2%. No optimization or adoption
  threshold is changed after seeing the observations. A difference must repeat
  across windows and be distinguishable from A/A variation before attribution
  or ablation is justified. Failure to establish a difference does not establish
  equivalence or negate the historical observation.
- Every sample validates full values outside its timer and performs a verified
  save/load roundtrip. Common semantic checksum, request count, initial/final
  capacity, used extent, wire length, observed moves and retry counts must agree
  across all endpoints and windows. Wire hashes are preserved independently.
  The deterministic generated data uses the frozen harness's `value_at(i)`;
  there is no external input file or workload seed for these three cases.

## Timing boundaries and metrics

| Reported quantity | Definition and limit |
| --- | --- |
| First save, 64 MiB | One `save_verified` call including graph validation, normalization and output allocation/copy. The arena is populated and its entire 129 MiB logical extent pretouched before timing. Output destruction, full-value verification, wire hashing and readback are outside the timer. This is first save of a prepared owner, not a cold boot or filesystem write. |
| Repeated save, 64 MiB | Four calls on the same prepared owner. `first_ms` is call 1; `repeated_mean_ms` is the sum of calls 2–4 divided by three. Mach memory queries, value checks, wire hashing and output destruction occur between calls. It is not a tight uninterrupted save loop. |
| Tail append, 1M uint32 values | Arena starts with equal 32 MiB logical and maximum capacity. The vector starts unreserved. Timers cover batches of at most 4,096 appends, including pointer comparisons, retry handling and request counting. Allocation of the sample vector, setup, full-value verification and wire save/load are outside timed batches. |
| `container_moves` | Address changes of the observed vector/string buffer after successful container operations. It is not a count of all allocations, object move constructors or hidden copies. |
| `container_copy_bytes_est` | Old live element bytes when an observed container address changed. This is an estimate; it excludes other copies, zeroing and allocator metadata work. |
| `growth_count`, `newly_exposed_bytes`, `zeroed_new_bytes`, `vm_enable_calls_est`, `vm_enabled_bytes_est` | Legacy harness counters for explicit `record_growth` calls. They do not instrument all growth/OS calls or prove total bytes actually written. These preallocated cases have no such growth. |
| `allocation_failures` | Caller-observed `XBadAlloc` retries in the harness. This is not all failed internal allocation attempts. |
| `used_end` | End offset of the arena's used extent, including internal layout and holes; not useful live payload or RSS. |
| `free_bytes` | Existing `get_free_memory()` meaning: Core reusable blocks plus tail. It excludes wire-cache bytes and is not total free address space. |
| `cached_free_bytes` | Only assigned in the historical fragmentation workload. Its default zero here is **not an instrumented measurement of wire cache occupancy**. |
| Logical / accessible / reserved | Logical length, page-rounded accessible extent, and reserved virtual mapping are separate. Accessible bytes do not establish physical residency or Windows commit charge. |
| RSS / virtual / peak RSS | Whole child-process observations, including runtime and validation allocations. They cannot be attributed exclusively to one arena. High-water peak is not a phase-local peak. |
| `operation_p99_ms` and similar legacy fields | Quantiles of a small set of save calls or append batches. They are retained raw but not reported as production tail latency. |
| Owner/allocator diagnostics | Owner probe count is `null`. Core/wire scan counts, host allocation count/peak, metadata moves, cache-transfer counts, exact copy bytes, expansion-failure reasons, OS-call count and lock waiting/holding time are **not instrumented**. No causal conclusion uses these unavailable measurements. |

The save case does not time owner creation or destruction. The append case times
the existing harness's data operations, not the entire application lifecycle.
No explicit pool, Automatic growth or reclaim is enabled. Cold/warm pool
measurements belong to a later separate experiment; this evidence cannot make a
claim about mixed-capacity pools, contention or Automatic's layout costs.

`summary.json` preserves every paired ratio, each window's median/IQR/range,
endpoint-order strata, and a deterministic 10,000-resample percentile bootstrap
interval for the paired-ratio median. Ratios are **before / after**: below one
means the after endpoint was slower. IQR is not a confidence interval. Seven
pairs and three windows do not provide a reliable population interval or an
equivalence test. Per-operation timings are not pooled as independent samples,
and windows are not flattened into an artificially large independent sample.

## Reproduction and audit

Use a new output directory and the documented compiler/runtime, SDK and dependency
revisions. Extract `source_snapshots.tar.gz` into a scratch directory. Dependencies
may be placed anywhere and are explicit runner arguments; they must be clean Git
checkouts. Compile before reserving a quiet measurement window:

```text
python3 <scratch>/snapshots/benchmark_vm_stage1.py prepare \
  --d-source <scratch>/snapshots/D --f-source <scratch>/snapshots/F \
  --harness <scratch>/snapshots/D \
  --boost <boost-checkout> --typelayout <typelayout-checkout> \
  --compiler /usr/local/bin/clang++ --runtime-lib /usr/local/lib \
  --sdk /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk \
  --output <new-output>
python3 <scratch>/snapshots/benchmark_vm_stage1.py run-window \
  --output <new-output> --quiet-note "Builds and other benchmark processes stopped"
```

Run the second command three times in separately established quiet windows,
waiting at least 60 seconds between windows. A failed or partial window is a
failure; it is not silently skipped. Preserve it and prepare a new experiment
if needed. Runtime/binary/source identities are checked before and after each
window. The archived metadata's absolute paths describe the original machine;
the explicit preparation arguments allow relocation. A changed toolchain or
machine constitutes a new experiment.

```text
python3 tools/benchmark_vm_stage1.py audit --output docs/perf/2026-10-15-vm-stage1
python3 tools/test_benchmark_vm_stage1.py
```

The audit recomputes pairing summaries, checks semantic/work/layout consistency,
validates A/A and D/F binary attribution, and verifies source archives and raw
sample hashes. The small runner tests reject incomplete samples and tampered
raw/source/binary attribution, and distinguish the median of paired ratios from
the ratio of endpoint medians. No executable or object file is retained in this
portable evidence directory.
