# Development tasks

Date: 2026-10-15. The delivered batches are recorded in
[stage 1](../../../docs/VM_STAGE1_RESULTS.md), [stage 2](../../../docs/VM_STAGE2_RESULTS.md)
and [stage 3](../../../docs/VM_STAGE3_RESULTS.md). Batch III/N6 research and decisions are complete:
all six allocator candidates were rejected and both production headers remain F.
Remaining platform and quiet-host prerequisites stay open. Work package IDs refer to
[the design](../../../docs/VM_NEXT_STAGE_PLAN.md).

## Analysis delivered

- [x] Inspect current F source, branch/base, previous adoption and rejection evidence.
- [x] Review build/release, performance/policies and allocator design independently.
- [x] Define invariants, dependencies, experiment controls and adoption gates.
- [x] Produce a concrete staged development plan without modifying implementation.

## Batch I — trustworthy validation and measurement

- [x] N0: Record fixed source/toolchain/dependency/runtime manifests for the validated Linux/macOS profiles.
- [x] N0: Pin canonical CI to a verified pullable digest and compiler source recipes to commits; document remaining Ubuntu/apt variability in source rebuilding.
- [x] N0: Add maintained portable validation entry points with explicit archive inputs; remove undeclared local build/runtime dependencies from new runners.
- [x] N0: Create log directories before use; test clean source/build paths and stale storage-only cache recovery.
- [x] N0: Add a positive compilation control and expected-error validation; fail unexpected negative-test success.
- [x] N0: Generate accurate program/CTest inventories and update developer guidance.
- [x] N0: Derive future variant labels from manifests; add historical-label errata without rewriting archived evidence.
- [x] N1: Make C++17 OS-only configuration independent of P2996 and compiler-family flags.
- [x] N1: Add a real reflection compile/link capability check for full-library mode.
- [ ] N1: Validate OS storage/pool on native Linux/macOS/Windows; distinguish compile and execution status.
- [x] N1: Maintain full P2996, sanitizer, TSan and historical wire validation paths with explicit support boundaries.
- [x] N1: Assess Windows full-library toolchain separately; record any unmet prerequisite instead of claiming support. The assessment is accepted; native Windows execution/full-P2996 support remains unverified.
- [x] N3: Define existing metric meanings, estimates and unavailable diagnostics; freeze ordinary timing builds, validate semantics and source/binary attribution.
- [x] N3 foundation: Implement and accept scoped allocator/copying/owner/lock diagnostic instrumentation separately from formal timing builds; exact/structural/unexecuted coverage remains explicit.
- [x] N3 application: Validate bounded network-specific cost assertions and map all five original metric groups to evidence and limits; multi-owner and default-path attribution remain separate open items.
- [x] N3 owner scaling: Deliver current-F fixed-request owner evidence with separate owner scale, thread count and actual work; D033 accepts 78 processes and all 24 cells, with only 7 cells / 2 comparisons qualified (one self-comparison). All multithread comparisons remain uncertain; no registry causal or optimization conclusion.
- [ ] N3: Run A/A calibration and independent-window D/F default-path reproduction; perform one-change ablation only after reproducibility.

The 2026-10-16 default-aa-1 attempt completed all 78 preregistered F/F
processes, with independently audited exact outputs and source identities.
All four metrics failed the fixed noise/drift qualification. This is accepted
execution evidence, not a D/F result; no retry, comparison or ablation follows
from this window. See `docs/coordination/vm-followup/default-aa-1-acceptance.json`.

Stage 1 delivered 3 diagnostic windows (252 measured + 36 warmups), audited data,
and a corrected fail-closed window completion protocol. Background contention
prevented quiet-host regression attribution; the last N3 task is not complete.
Native macOS/Linux storage passed; Windows CI is configured but has not run.

## Batch II — consumption and optional policies

- [x] N2: Add configurable subdirectories, portable interface targets and install/export configuration.
- [x] N2: Declare and validate TypeLayout/Boost/header/toolchain/Threads dependencies and license packaging.
- [x] N2: Pass an out-of-tree installed consumer and multi-translation-unit ownership/lifecycle test.
- [x] N2: Publish an experimental support matrix distinguishing OS, full library and performance environments.
- [x] N4: Compare fixed/mixed/bimodal capacities, phase changes and 1/2/4/8 threads with equal total pool budgets.
- [x] N4: Measure idle/reuse curves, contention and concurrent clear; separate live-owner shrink/reclaim/regrow from pooled-owner retain/clear/reacquire.
- [x] N4: Prototype capacity indexing, shorter lock scope or sharding only for a measured bottleneck; retain no-allocation return and global bounds.
- [x] N4: Adopt a verified candidate or document retaining F with measured applicability limits.
- [x] N5: Compare preallocated/Explicit/Automatic using identical traces and independent initial-capacity/insertion-cadence parameters.
- [x] N5: Explain the existing +6.60% used_end result through allocation/growth events.
- [x] N5: Prototype the smallest justified change and verify hard-limit/nested-failure/cached-block behavior; adopt or retain F with evidence.

Stage 2 delivered N2 packaging and N4/N5 evidence. Pool optimization was not
justified by the screening; a generated Automatic tail-flush candidate was rejected
for exact finite-lifetime space/copy regressions. Both production headers remain F.
Pool lock/scan and allocator-event diagnostics are available, but the broader N3
owner-index/host-metadata instrumentation and quiet-host attribution stay open.

## Batch III — allocator research

Execution design and delegation contract: [VM_STAGE3_PLAN.md](../../../docs/VM_STAGE3_PLAN.md).
Results and evidence status: [VM_STAGE3_RESULTS.md](../../../docs/VM_STAGE3_RESULTS.md).
The user authorized design followed by implementation in a separate local conversation.
All six independent candidates were implemented, validated and screened; none was adopted.

- [x] [docs] Specify S3.0–S3.5, isolated variants, physical-header/ownership constraints, controls, adoption gates and handoff boundaries.
- [x] [core] S3.0: Freeze current F inputs and preregister candidate/workload/metric budgets before experiments.

- [x] N6: Establish interval-model and randomized release/size/alignment/pressure workloads plus fail-after-N metadata injection.
- [x] N6-A: Compare first-fit splitting with physical-tail donors preserved whole, Core and wire independently; reject both implementations with measured counterexamples.
- [x] N6-B: Separately evaluate high-address allocation from tail donors with legal alignment/header/remainder handling; reject both implementations after independent comparison.
- [x] N6-C: Keep whole-block policy fixed; compare stable-slot linear and indexed search with identical donor selection against each other and F; report actual host metadata costs, complete fallback behavior and rejection decisions.
- [x] N6: Verify unique ownership and prepare-before-publish transitions, including wire-to-Core transfer, partial index failures and conservation independent of allocator counters.
- [x] N6: Evaluate the combination gate. No component passed independent screening, so no combined candidate or combination experiment was executed.
- [x] N6-D: Evaluate the residual-fragmentation/coalescing gate. The prerequisite independently successful baseline was absent; D implementation and coalescing experiments were not executed.
- [x] N6: Retain F, validate F with the new tests/tools and archive all candidate rejections; do not claim an adopted-runtime compatibility matrix or unexecuted platform checks.
- [x] [docs] Publish VM_STAGE3_RESULTS.md and per-candidate decisions; independently review the research result. Final evidence packaging and offline-audit status are stated in that report.

Stage 3 completed 800,000 normal and 32,000,000 deep randomized model operations,
with every-step ownership/conservation checks. Candidate correctness records contain
65 commands; supplementary functionality contains 31 runs. Formal screening contains
32 windows, 983 cases and 17,694 rows; separate diagnostics contain 777 rows.
These are screening/rejection results, not three quiet adoption-confirmation windows.
The added diagnostics do not complete the broader N3 attribution tasks above.

## Batch IV — conditional I/O research and final delivery

Current multi-chat execution: [orchestration design](../../../docs/VM_FOLLOWUP_ORCHESTRATION.md)
and [dispatch/gate state](../../../docs/coordination/vm-followup/dispatch.json). The parent
owns design, coordination and acceptance; W1–W4 own separate implementation paths.
Current local research implementation and G0–G4 acceptance are complete under D034; the explicit external/default-attribution/business prerequisites below remain open.

- [x] Dispatch four implementation chats with exclusive write boundaries and dependencies.
- [x] G0: Accept the local POSIX execution entry and freeze W1/W2 integration contracts; daemon/container jobs remain unsupported by this entry.
- [x] G1: Accept scoped network failure/functional coverage, diagnostic foundation and platform prerequisite inventory. W1 sanitizer and unavailable native platforms remain explicit gaps; this does not complete N3 attribution or N7 adoption.
- [x] G2: Audit frozen-baseline network/default-path measurements and limits; accept scoped evidence with failed noise qualifications, without performance/adoption or default D/F claims.
- [x] G3: Record per-candidate unmet prerequisites and retain F; no candidate implementation, experimental rejection or adoption is claimed.
- [x] G4: Accept the final frozen local research delivery, applicable validation inheritance and independent review (D034); this does not complete the overall roadmap.

Parallel preparation is recorded in [VM_IO_PRESTUDY.md](../../../docs/VM_IO_PRESTUDY.md)
and [VM_WINDOWS_HANDOFF.md](../../../docs/VM_WINDOWS_HANDOFF.md). The former is a reference
file consumer without actual business-consumer adoption evidence; the latter is a frozen
Windows storage validation handoff that has not run natively. Neither closes N7 or N1.

- [x] N7 preparation: Build separate total-timing and phase-diagnostic file consumers; both pass the 14-case functional smoke.
- [x] N7 preparation: Execute the preregistered synthetic file matrix (232 processes), preserve exact output equivalence and audit the evidence. All four A/A sizes fail the noise gate; no reliable performance/adoption claim is made.
- [x] [docs] Record network I/O as the user's target with size/protocol/sync mode undetermined; define bounded framing, ownership, cancellation/backpressure and phased reference-consumer work in [VM_NETWORK_PLAN.md](../../../docs/VM_NETWORK_PLAN.md).
- [x] N7 network functionality: Implement and validate a bounded reference receive/send consumer and failure matrix using unchanged V1 bytes.
- [x] N7 network measurement: Independently measure the accepted reference consumer with F pooled loading controls; the completed one-shot comparison is noise-unqualified, not an adoption result.

The reference consumer and frozen dual builds are accepted: each profile passed
34 groups / 182 subcases, with CLI/output equivalence and portable evidence.
The first independent network calibration is accepted: 112 native processes and
1,484 full wire outputs. Only 1 MiB passed both local noise and anchor drift;
4 KiB, 64 KiB and 16 MiB retain their failed prerequisites. The independent
1 MiB pooled-control comparison completed all88 native processes and352 full
wire outputs. Both contrasts passed local A/A but failed their own cross-anchor
drift gate; D026 accepts this noise-limited evidence. No retry, performance
superiority or adoption is inferred. The separate N3 diagnostic
foundation is also accepted: 29 event kinds have count/ledger/partial-snapshot
assertions, 11 structural checks only, 1 invalid-control-only and 22 unexecuted
paths. Consumer-specific attribution and the broader N3 items remain open.
Acceptance records are in `docs/coordination/vm-followup/W1-G1-acceptance.json`
and `W2-G1-acceptance.json`.
The bounded W2 network diagnostic integration is designed and dispatched under
`docs/coordination/vm-followup/contracts/W2-network-v1/acceptance.json`;
its implementation and bounded workload-specific cost evidence are accepted in
`docs/coordination/vm-followup/W2-network-acceptance.json`: 32 processes, 16 valid traces,
zero dropped events, 36 Python checks and 11 artifact tamper controls. This supplies
scoped operation costs, not a formal performance or adoption result.

File-screening results: [VM_IO_SCREENING_RESULTS.md](../../../docs/VM_IO_SCREENING_RESULTS.md).
These synthetic results do not satisfy the actual-consumer or production-API gates below.

- [ ] N7: Profile an actual input/output consumer; distinguish owner reuse, staging direct read and external borrowed input; include F pooled loading as an owner-reuse comparator.
- [ ] N7: If justified, define rollback, partial read/cancellation, validation publication and asynchronous output lifetimes before prototyping.
- [x] N7 decision: Document retaining F and adding no new I/O API because interpretable benefit, exact lifetime/resource contracts and representative consumer prerequisites are absent. Production adoption remains unverified.
- [x] Freeze final F source and related tools; reconcile the applicable validation matrix by accepted full-input/profile inheritance and new tool/metadata evidence. D034 adds no unconditional native/C++ rerun; new and inherited counts remain distinct.
- [x] Audit raw performance results, version labels, hashes, platform limitations and negative results; D026/D033 retain failed or partial noise qualification without adoption claims.
- [x] Reconcile migration/consumer examples and package decisions: D024 README clarifications accepted; existing migration and C++ examples remain applicable because no new production API was adopted. Candidate and platform prerequisites are explicit.
- [x] Review the final change and 245-source/236-metadata capsule independently; D034 accepted with explicit scope limits. Integration/publication actions remain separate and unexecuted.

D028 closes the current three-direction G3 decision deliverable with prerequisite-unmet
results. The conditional prototype task is not activated; it is not an unfinished
local prototype or an experimental rejection. Actual-consumer profiling remains
open, and all original thresholds apply only after a future explicitly scoped design.

D033 accepts the original 78-process owner window and the D031/D032 offline
approval-byte supplement. All 24 cells and 24 comparisons remain present; only
7 cells and 2 comparisons qualify, including one self-comparison. All multithread
comparisons remain uncertain. The original window, original archive failure, and
derived copy with only approval.json restored are preserved. No native workload
was repeated. This completes bounded owner evidence, not default D/F attribution
or a registry causal or production optimization claim.

D034 accepts G4 local research delivery after independent source/metadata review
and completed validation receipts. The user confirmed that new platform/business
prerequisites are unavailable for now and asked to keep them open. N1 native
Windows, independent Linux performance, N3 default D/F attribution, compatible
network sanitizer runtime, and N7 representative business integration remain
uncompleted requirements of the overall roadmap. No additional experiment,
commit, push, publication or remote CI was executed for final acceptance.
