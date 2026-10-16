# Stable XBuffer virtual-memory storage

Replace the relocating vector payload with a reserved virtual-address owner so
growth, logical shrink, and save preserve the base. Preserve v1 wire and frozen
container layouts, explicit growth, and the existing reflection requirement.
Failures at the configured maximum or during OS page enablement must leave the
old logical arena usable without a heap fallback or whole-payload copy.

The implementation and exact API migration are documented in
`docs/STABLE_VM_STORAGE_DESIGN.md` and `docs/STABLE_VM_STORAGE_MIGRATION.md`.
Automatic growth, allocator restructuring, physical page reclamation, and new
transmission protocols are outside this change.
