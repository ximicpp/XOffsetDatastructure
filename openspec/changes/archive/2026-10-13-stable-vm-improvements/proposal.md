# Complete the reviewed stable-VM improvements

The stable storage implementation preserves its base, but review reproduced a
global-destruction mutex failure and cv-qualified handle rejection. Measurements
also identified repeated transport writes and linear owner lookup as avoidable
costs. The user approved the reviewed implementation plan on 2026-10-13.

Implement the lifecycle repairs, fresh-owner byte initialization, direct final
save output, exact-base owner index, and bounded live-tail container expansion.
Evaluate allocator fragmentation, small-buffer churn, and retained pages before
deciding whether optional policy changes belong in this delivery.

The detailed contract and acceptance matrix are in
`docs/VM_STORAGE_IMPROVEMENT_PLAN.md`. No publication, remote CI, or push is part
of this change.
