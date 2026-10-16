# Allocator research and adoption requirements

Batch III/N6 research and decisions are complete; see
[VM_STAGE3_RESULTS.md](../../../../docs/VM_STAGE3_RESULTS.md).
All six independent candidates were rejected and F remains unchanged. Combination and
D gate assessments concluded condition not met; no combination or D implementation or
experiment was performed. This result does not complete full N3 attribution, native
Windows validation or N7 actual-consumer work.

## Baseline and compatibility

Research SHALL compare frozen accepted F with separately identified candidates.
Git HEAD alone SHALL NOT stand for the current uncommitted F source.
Stable owner bases, fixed hard limits, explicit defaults, V1 layout/signatures,
wire validation, initialization and existing lifecycle/exception guarantees SHALL
remain intact. Historical source and evidence archives SHALL remain immutable.

## Physical blocks and ownership

A physical interval SHALL have one owner: a live allocation, Core free storage or
one wire cache node. An index SHALL reference that owner without acquiring another
ownership claim. Split fragments SHALL each carry a valid Core allocation header;
alignment SHALL be checked against actual addresses. Advertised wire capacity and
physical capacity SHALL be accounted for separately. Unknown alignment gaps SHALL
NOT be reclaimed by inference.

Failure-prone metadata and all bounds SHALL be prepared before publishing a split,
transfer, index insertion or merge. Invalid or unprepared split candidates SHALL
retain legal whole-block reuse. Existing best-effort free under host OOM SHALL NOT
be reported as a new capacity-conservation or allocation-free guarantee.

## Controlled experiments

Core-only and wire-only changes SHALL first be evaluated independently. Tail
protection and high-end tail allocation SHALL be separate variants. An index-only
comparison SHALL use the same donor eligibility, order and tie-break as its linear
control; changed selection SHALL be labelled and controlled separately. A bounded
fast search SHALL have a complete fallback and SHALL NOT omit available donors.
Coalescing SHALL require a demonstrated residual-fragmentation case and SHALL NOT
silently merge live or cross-layer-owned storage.

## Validation and decisions

Independent interval-model checks, deterministic randomized traces, real metadata
failure injection and compatibility tests SHALL precede performance adoption.
Formal timings SHALL use uninstrumented frozen binaries, equal useful work,
balanced order, A/A controls and recorded execution environments. Pressure tests
with unequal completed work SHALL report success counts separately.

Adoption thresholds SHALL be recorded before candidate timing results. Correctness,
wire compatibility and applicable integration checks SHALL be hard gates.
Missing prerequisites or uncertain measurements SHALL NOT be reported as passes.
Every required candidate SHALL receive an evidenced decision: adopt, reject,
condition not met or prerequisite unavailable. Retaining F is a valid research
outcome; unperformed implementation/validation SHALL remain visibly incomplete.
