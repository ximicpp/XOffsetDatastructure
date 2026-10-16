# VM memory reuse follow-up

Date: 2026-10-15 (Asia/Shanghai). The user requested research, design and
implementation based on the current branch and previous plan. Work continues
in the existing next_cpp26 working tree, preserving the previous uncommitted
implementation and its immutable 2026-10-13 evidence.

The prior A–D implementation is complete. This follow-up first addresses the
remaining measured fragmentation and investigates the small repeated-load
regression. Implement coordinated best-fit selection and allocation-time
splitting in the Core and payload freelists. Investigate full-reservation fresh
load mapping as a separate candidate, adopting it only after isolated functional
and performance evidence. Evaluate Automatic growth, physical reclamation and
bounded pooling as independent contracts; do not silently change default growth,
capacity or shrink behavior. A user priority response may reorder this work.

The user subsequently selected: advance ALL follow-up items, adopting each only
after separate verification. Scope therefore also includes explicit bounded
Automatic growth, tail-page reclamation, an opt-in bounded shared VM pool, and
Core-only physical coalescing after splitting. These require prototypes and
actual functional/performance checks rather than deferral on analysis alone.

Completed adoption decision: F keeps Automatic, explicit reclaim, bounded pooling
and narrow direct-RW copied loading. The allocator E prototype is archived rather
than adopted: full and ablated measurements demonstrate substantial CPU and
incremental-growth space regressions despite correctness passing. Final source,
full platform checks, 420 wire operations and all positive/negative performance
evidence are preserved in docs/VM_MEMORY_REUSE_RESULTS.md and its evidence links.
