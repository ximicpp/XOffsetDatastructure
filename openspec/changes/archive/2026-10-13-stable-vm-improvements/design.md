# Design

Use the reviewed repository plan as the design source:
`docs/VM_STORAGE_IMPROVEMENT_PLAN.md`.

- Give synchronization/index state process lifetime and normalize only local
  root identity with `remove_cv_t`; retain existing wire/schema cv rules.
- Initialize fresh load storage by complete copying before logical publication.
  Ordinary grow and shrink/regrow retain zeroing and deterministic failure hooks.
- Build verified save into its final owning string while preserving normalized
  v1 bytes and its existing allocation-failure behavior.
- Index exact arena bases in a synchronized address hash table. Registration
  may fail; moves and swaps update existing entries without allocation.
- Expand only a trusted live physical-tail allocation inside existing logical
  capacity. Validate all Core and wire-allocator metadata changes before commit.
- Keep explicit growth, capacity budgets, stable addresses, and wire layout.
  Optional pooling/reclamation/free-block policy changes require separate
  evidence; they are not implied by the core optimizations.

Freeze corrected V1 and each implementation stage, use matched inputs and
sequential paired runs, and permit allocator-layout changes only in comparisons
that validate semantic interchange with old and new readers.
