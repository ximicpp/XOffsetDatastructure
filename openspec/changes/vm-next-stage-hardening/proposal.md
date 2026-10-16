# XBuffer next-stage hardening and measured optimization

Date: 2026-10-15. Status: first, second and third batches delivered with remaining platform
and measurement prerequisites tracked in tasks.md. See [stage 1 results](../../../docs/VM_STAGE1_RESULTS.md)
and [stage 2 results](../../../docs/VM_STAGE2_RESULTS.md). The user authorized designing
Batch III and delegating its implementation to a separate conversation; the executable
design is [VM_STAGE3_PLAN.md](../../../docs/VM_STAGE3_PLAN.md). The completed research and
decisions are in [VM_STAGE3_RESULTS.md](../../../docs/VM_STAGE3_RESULTS.md): all six independent
allocator candidates were rejected, F remains unchanged, and combination/coalescing
conditions were not met. Full N3 attribution, native Windows and actual-consumer N7 work remain open.

The current `next_cpp26` working tree contains accepted F on top of Git base
`07d649909d7f43ca0d11b85accb6d9dab991e284`. F is not represented by that commit
alone. Its source hashes, validated behavior and accepted costs are recorded in
[the next-stage design](../../../docs/VM_NEXT_STAGE_PLAN.md).

The user first requested systematic analysis and a next development plan, then
authorized phased implementation. The first batch hardens build/validation and
adds measurement infrastructure. Accepted F implementation headers and historical
performance/validation evidence remain unchanged.

## Problem and intended outcome

Stable VM storage and the optional growth, reclamation and pooling policies are
implemented. Their evidence is stronger than the maintained reproduction and
consumer build paths: CI uses mutable toolchain references, OS-only tests cannot
be configured independently, the negative compilation test can falsely pass,
Windows remains unverified, and packaging has no independent installation test.
Performance coverage also needs mixed capacities, contention, random frees and
identical operation traces before making broader optimization decisions.

First make validation portable and trustworthy, establish separate OS/full-library
support matrices and a consumer package. Then measure default-path regressions,
pool behavior and Automatic layout costs. Research allocator search and splitting
independently, admitting only candidates that pass correctness and measured
tradeoff gates. Direct input/output APIs require a concrete consumer and separate
lifetime design before implementation.

## Boundaries

- Keep F as baseline; rejected E remains evidence, not the starting allocator.
- Preserve stable bases, hard limits, explicit defaults, V1 wire format, validation,
  pool budgets and current documented exception/lifetime guarantees.
- Keep runtime ownership unique and indexes outside wire storage.
- Distinguish OS-layer Windows success from full P2996 library support.
- Keep historical artifacts immutable; explain stale labels in a new sidecar.
- Phased implementation is authorized. Source/tool changes and local validation
  are recorded per batch; no commit, push or CI dispatch has been performed.

Detailed design, sequencing, candidate rejection criteria and file boundaries are
in [VM_NEXT_STAGE_PLAN.md](../../../docs/VM_NEXT_STAGE_PLAN.md). Execution is tracked
in [tasks.md](tasks.md); unfulfilled external-platform prerequisites remain visible.
