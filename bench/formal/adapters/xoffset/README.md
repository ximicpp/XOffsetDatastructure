# XOffset alpha.1 adapter

Input is the validated alpha source tar identified by P1, not repository HEAD.
The exact archive SHA is also declared in the adapter; the `commit` describe
field retains the handoff commit and does not replace that archive identity.

P0 policy is unchanged: `TypedXBuffer` verified load/save, independent owning
arena, Automatic growth explicitly enabled, 4096-byte initial arena, 512 MiB
hard maximum and no pool. The owning output is the native `std::string`.
Trusted here describes the input source; it does not remove native checks or
establish the separate untrusted-mutable contract.

All container-bearing records are constructed inside the arena. Initial record
capacity may be reserved; future trace capacity is not reserved. Queries read
native containers through the common view and never materialize a DTO.

Stage 7 checks each operation's sequential status with a temporary removed-ID
set, stably compacts survivors with ordinary record/member move assignments,
then shrinks the native vector. XOffset's public move assignments can perform
deep copies of nested arena values; those allocations/copies are real measured
mutation work. No bytewise relocation, allocator internals or production API
changes are used. Stage 8 has an increasing-ID append fast path and a binary
insertion fallback for small functional fixtures.

Implementation and validation are separate: inspect the root-managed build,
adapter contracts and fixture/stage evidence before claiming this adapter passes.
