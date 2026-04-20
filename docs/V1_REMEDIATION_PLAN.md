# V1 Remediation Plan

## Goal

This plan closes the two remaining gaps identified in the current `v1 fixed-schema`
implementation:

- protocol evidence is not yet strong enough to justify the full target-matrix claim
- save/load preserves mutable object semantics, but does not yet preserve full runtime
  allocator state

The two gaps are related, but they should be handled separately.

## Current status

Completed in the current repository:

- Phase A0: documentation now describes the matrix baselines as repository-managed
  synthetic baselines
- Phase A1: the old example-driven exporter catalog has been replaced with an
  explicit wire coverage catalog under `tools/`
- Phase A2: verified-wire schema hashing now uses XOffset-owned
  `wire_abi_signature<T>()`
- Phase A3: `ctest` now includes a regenerated-baseline equality check
- Phase B0: public docs and API comments now describe normalized transport semantics
- Phase B1: regression tests now pin the “normalized but still mutable” contract

Still open:

- Phase A4: native per-target harvesting/evidence
- Phase B3 only if the product later requires allocator-state snapshot fidelity

## Gap A: Protocol Closure Evidence

### Current state

The repository currently proves these things:

- the frozen main-container runtime works locally
- verified load/save works locally
- the repository maintains an exact committed target-matrix baseline set
- committed baselines remain pairwise compatible under the current TypeLayout check

The repository does **not** currently prove these stronger statements:

- each target baseline was independently harvested from a native target toolchain
- the full admitted protocol surface is covered by the baseline set
- schema hashing will change for every frozen-container ABI drift

### Root causes

1. `tools/export_signatures.cpp` currently exports one host-observed layout set into
   every target baseline, then swaps only the target ABI metadata.
2. historically, `wire_schema_hash_v<T>()` depended on TypeLayout signatures over
   opaque container registrations; the current repository has replaced that path
   with `wire_abi_signature<T>()`.
3. the signature catalog is currently a small hand-maintained example set rather than
   the declared protocol-root set.
4. `ctest` validates committed baselines, but it does not regenerate them before
   comparison.

### Required end state

To claim protocol closure for the target matrix, the repository should satisfy all of:

1. target baselines are produced either:
   - by native target toolchains, or
   - by a generator that is itself proven to derive the same ABI signature as those
     toolchains for the admitted surface
2. schema hashing includes the frozen container ABI contract, not only the old
   opaque tag/size/align shell
3. the baseline catalog is generated from the protocol root registry, not from
   examples
4. CI verifies:
   - baseline inventory
   - regenerated baseline equality
   - compatibility verdict

### Implementation phases

#### Phase A0: Correct the claim boundary

Short-term documentation correction:

- describe the current matrix baselines as repository-managed synthetic baselines
- state explicitly that they are generated from target ABI metadata plus the current
  exported catalog
- avoid wording that implies native per-target harvesting already exists

This phase is low risk and should land first.

#### Phase A1: Separate protocol-root registration from examples

Introduce an explicit protocol-root registry, for example:

- `XOFFSET_PROTOCOL_ROOTS(X)`
- or a generated registry file under `tools/`

Then make all of these consume that registry:

- `wire-schema` audit tooling
- baseline export
- compatibility checking
- documentation examples

This removes the current example-driven blind spot.

#### Phase A2: Replace opaque-derived schema hashing

The schema hash for verified wire should become an XOffset-owned hash, not a raw
TypeLayout hash over opaque containers.

Recommended design:

- define a `wire_abi_signature<T>()` trait family
- for scalars/enums: fixed canonical tokens
- for user records: recurse fields and bases
- for `XFixedString`: emit explicit token including its frozen header contract
- for `XFixedVector<T>`: emit explicit token for the frozen vector header plus
  element signature
- for `XFixedFlatSet<T>` / `XFixedFlatMap<K,V>`: emit explicit container tokens
- hash `schema_name<T>() + wire_abi_signature<T>()`

This should become the source of:

- `wire_schema_hash_v<T>()`
- target baseline content for the admitted protocol surface

TypeLayout can still stay as a safety delegate and secondary audit source, but it
should no longer be the only source of the verified wire schema hash. This phase
is complete in the current repository, and XOffset no longer registers its own
containers through TypeLayout's opaque mechanism.

#### Phase A3: Add regeneration checks to the test chain

Current `ctest` proves that committed baselines are self-consistent.
It does not prove that source changes still match those baselines.

Recommended fix:

- keep `check_signature_matrix` and `check_compat` in `ctest`
- add a separate CI job or tooling test that:
  - runs the exporter into a temp directory
  - diffs generated files against committed `tools/sigs/`
  - fails on drift

This should not rewrite committed files in-place during normal `ctest`.

#### Phase A4: Native target evidence

If the project wants to make the strongest possible target-matrix claim, add one of:

- native CI export on Windows/Linux/macOS+iOS-target toolchains/Android NDK toolchains
- or committed generated manifests produced by those target jobs

This is the final evidence layer, not the first one.

## Gap B: Save/Load Mutability Fidelity

### Current state

Current save/load preserves:

- the live object graph
- allocator state embedded in payload
- free-list state embedded in `XAllocatorStateV1`
- continued mutation of loaded objects

Current save/load does **not** preserve:

- pre-save spare tail capacity, because `save()` and `save_verified()` normalize with
  `shrink_to_fit()`
- backend `free_blocks_` state, because that state is process-resident rather than
  payload-resident

So the current system preserves **mutable semantics**, but not a **bit-equivalent
runtime snapshot** of future allocation behavior.

### Decision that must be made

The project should explicitly choose one of these semantics:

1. **Normalized transport semantics**
   - save/load preserves the logical object graph
   - save/load may normalize allocator slack and reclaimed backend blocks
   - post-load mutation remains supported, but capacity budget may differ

2. **Runtime snapshot semantics**
   - save/load preserves the logical object graph
   - save/load also preserves allocator topology and reusable backend blocks
   - post-load mutation capacity and fragmentation behavior should match pre-save

These are different product guarantees.

### Recommended path

For `v1`, the cleanest approach is:

- define the default wire path as **normalized transport semantics**
- document that this is not a full allocator snapshot
- add an optional later `snapshot` mode only if workloads truly need allocator-state
  fidelity

This keeps the protocol simple and aligns with the current implementation.

If the product requirement is actually full runtime-state preservation, then the
current backend design needs another implementation round.

### Implementation phases

#### Phase B0: Document actual semantics

Update the public contract so it says:

- verified load/save preserves admitted object graphs and continued mutation
- it does not guarantee identical post-load spare capacity or fragmentation state

This should happen immediately unless the team chooses snapshot semantics and plans to
implement it right away.

#### Phase B1: Add explicit tests for post-load mutation fidelity

Add tests that:

1. create churn that produces allocator free-list and backend free-block state
2. save and reload
3. continue mutating after load
4. compare:
   - whether mutation still succeeds
   - whether the buffer grows earlier than before save

These tests should distinguish:

- semantic mutability preservation
- exact allocator-state preservation

#### Phase B2: If normalized semantics remain the product contract

Then the implementation only needs to:

- keep current behavior
- document it clearly
- possibly rename or annotate the save path as normalized

Possible API direction:

- `save_verified()` keeps current normalized semantics
- optional future alias: `save_verified_normalized()`

#### Phase B3: If snapshot semantics become required

Then the runtime must be changed so backend allocator state is payload-resident too.

Two viable directions:

1. **Persist backend free blocks explicitly**
   - move `free_blocks_` into payload state
   - rebuild them on load
   - validate them during verified load

2. **Remove the extra backend free-block layer**
   - make all reclaimable capacity flow through the payload allocator state only
   - eliminate the process-resident allocator state split

The second option is architecturally cleaner, but is a larger refactor.

### Recommended technical direction

If snapshot semantics are required, prefer:

- reducing allocator state to one payload-owned source of truth

That means:

- allocator metadata for reusable capacity should live entirely in payload
- verified load should be able to reconstruct all future allocation behavior from
  payload alone
- `XBufferCore` should not keep extra mutable allocation topology that is invisible to
  the wire format

## Recommended execution order

1. A0: narrow the protocol-closure wording
2. B0: narrow the save/load fidelity wording
3. A1: explicit protocol-root registry
4. A2: XOffset-owned wire ABI signature and schema hash
5. A3: regenerated-baseline CI verification
6. B1: tests that distinguish semantic mutation from allocator snapshot fidelity
7. choose:
   - B2 if normalized transport is acceptable
   - B3 if runtime snapshot fidelity is required

## Bottom line

The current `v1` implementation is already a strong **normalized zero-decode transport**
system for fixed-schema admitted types.

It is not yet fully proven as a complete cross-target protocol contract, and it is not
yet a full allocator-state snapshot format.

Those are the two remaining closure tasks.
