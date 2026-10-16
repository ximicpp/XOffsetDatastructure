# Design

The current implementation analysis, storage invariants, OS API choices,
failure ordering, compatibility decisions, and lifecycle contract are maintained
in `docs/STABLE_VM_STORAGE_DESIGN.md`.

The VM layer is isolated in `xoffset_virtual_memory.hpp`; the public library
remains included through `xoffsetdatastructure.hpp`. Process-local reservations,
type identity, schema/access state, and registry synchronization are owned outside
the wire payload. `XBufferCore` reads the logical extent directly, avoiding a
throwing allocator resize or segment-manager reopen after enabling pages.

Validation evidence is maintained in `docs/STABLE_VM_STORAGE_VALIDATION.md`.
