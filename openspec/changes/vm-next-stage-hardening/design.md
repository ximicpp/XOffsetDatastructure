# Design entry point

The overall staged design is maintained in
[VM_NEXT_STAGE_PLAN.md](../../../docs/VM_NEXT_STAGE_PLAN.md).
The executable Batch III allocator design, approved for implementation delegation
on 2026-10-15, is [VM_STAGE3_PLAN.md](../../../docs/VM_STAGE3_PLAN.md).
Completed research and decisions are recorded in
[VM_STAGE3_RESULTS.md](../../../docs/VM_STAGE3_RESULTS.md): all six candidates were rejected
and both production headers remain F. Combination and D gate assessments are complete,
with neither implementation nor experiment executed because prerequisites were absent.
Broader N3 attribution, native Windows validation and actual-consumer N7 work remain open.

Batch III preserves accepted F as the control, evaluates Core and wire splitting
independently, isolates donor selection from indexing, and permits batch coalescing
only after a demonstrated fragmentation case. The detailed plan owns experiment
variants, ownership/failure invariants, randomized model validation, adoption gates,
source/evidence handling, resource limits and the final integration matrix.

Read `tasks.md` for current completion status and `specs/allocator-research.md`
for the requirements of this research and any resulting production adoption.
