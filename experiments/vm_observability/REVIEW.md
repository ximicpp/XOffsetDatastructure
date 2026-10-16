# Independent review: W2 / N3 diagnostic G1 handoff

Date: 2026-10-15, Asia/Shanghai. Reviewer: delegated independent source/evidence review (`existing_coverage`). Only this file was written by the reviewer. No C++ program was compiled or rerun during this review.

**Verdict: PASS for the scoped diagnostic foundation and G1 handoff represented by `build-06` / `collection-06`. No blocking finding remains in that reviewed scope.** This does not certify all 63 declared event paths, a production change, complete N3 attribution, or performance adoption. W3 still owns consumer integration, workload-specific resource/semantic assertions, formal timing and D/F reproduction.

The final 06 artifacts supersede 04/05 for source acceptance. Earlier failures and superseded records remain intact. Review fixes were performed by the implementation owner; this reviewer inspected their final source and the resulting evidence.

## Reviewed identities

The following current source files exactly match `build/vm-observability/build-06/input/`:

| File | SHA-256 |
|---|---|
| `generate.py` | `94785b7c11ac7c2dfa0dc5913a59a66a06629caf8522a64cc1701d002ebdc826` |
| `observe.py` | `d632be7084efc21af34ce742d27613198bd9e9667bcffd4103ebbb3046c20f6a` |
| `n3_probe.cpp` | `177ccfed682b689f016bf0d78974fcd1701bb43f56634deac3119e509ebd00ba` |
| `n3_runtime.hpp` | `0ee7e25b9a76f0e7b06acc25554be57219cd17982e3d6e1898a376ddacfba10a` |
| `audit_events.py` | `91ac07d1c54f306f965235b156a7ff95f18040c8eb65660434654342a626ee08` |
| `contract.json` | `6e5dc6f1a72621fac1f3955f5ce5b16f640003223147edfafc4eb476236af16a` |
| `event_kinds.json` | `7bc7b0a77d24a8557a601a793daead2690406603fed26d78931cd3b942bb2e2c` |
| `coverage.json` | `a62688caef037752d2191f249da26810b0731d493483738de100251916b79de9` |

Additional reviewed identities:

| File | SHA-256 |
|---|---|
| `runtime_controls.cpp` | `04b590e53826d7c041185b8e47141c8f04934b1014ece593cbba2f15cf471a98` |
| `verify_runtime_controls.py` | `06f3052fe75e4a0b28db6d3a09bac62196f095838662dbafa9d7a6353a5659c1` |
| `test_audit_events.py` | `45b00799f67c0e5a0463ba614d62b8bfaa0dd3d4f2be0fc8d06e0b94194094fb` |
| `test_observe.py` | `0694aae05ddcd2a01fd964825edbe42cacb6256f24fc799ea020d5ffbc9531b4` |
| `event-validation-coverage.json` | `7d26d6b55b410660048ece69af53edc7dd22a42d5c878c1dd3e51ae16c8631dc` |

The accepted contract/catalog/design-coverage hashes are unchanged. Both production headers still match fixed F:

- `xoffsetdatastructure.hpp`: `9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2`
- `xoffset_virtual_memory.hpp`: `22f181b52c11ce442a5576ffc0dc35af1828f77deb4424f387d0acfa658239ad`

The reused N5 generator is checked against `339fce86f70d13d9df82ebb1ad5149339f1c866e124213cbc7adb62089a45b1e`. Prepared-artifact audit regenerates the diagnostic headers and compares all 71 anchor records and resulting text, rather than trusting only a declared baseline label.

Final evidence identity:

- `build-06/result.json`: `5860c08dc5f614993aa88708e2c5a741dd30a69a5362a2df8a50951b592ce579`
- `collection-06/result.json`: `32fa091f6f76e0d8c48655198cc3e10f3434c9ccb36c6252ec76700b75587c03`
- `n3_original`: `498928f7ed536fe1ebda17b213e285525dd27b82febd673d58620c0c649a33a8`
- `n3_diagnostic`: `784d8e36a5a0a2f8c827a6991f7c41d04fad304f387728affca8aeae08cd083e`
- Auxiliary runtime-control binary: `f559b982a29e775e3d2ecb152571411985806881d6d9439679cfdc09bc584515`

## Independent checks and results

The reviewer executed these lightweight checks from the repository root:

```text
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s experiments/vm_observability -p 'test_*.py' -v
PYTHONDONTWRITEBYTECODE=1 python3 experiments/vm_observability/observe.py audit --record build/vm-observability/build-06
PYTHONDONTWRITEBYTECODE=1 python3 docs/validation/vm-observability/collection-06/tool/observe.py audit --record docs/validation/vm-observability/collection-06
PYTHONDONTWRITEBYTECODE=1 python3 experiments/vm_coordination/run_job.py audit --out docs/validation/vm-observability/collect-job-06
```

Results: 57/57 Python tests passed; build and frozen collection audits passed; the final collection queue receipt was `audit_passed`, `job_status=complete`, run ID `d0784eb198a44ed190670e1b5d652878`.

An inline Python invocation of W4's read-only `run_job.audit()` also passed for `build-job-06` (run ID `0de4d5dc465c4d87a99e7b711fe5b5cf`) and both auxiliary runtime build/execution jobs. Earlier 04/05 queue receipts were inspected and audited as historical/superseded evidence, not substituted for final 06.

An independent inline Python check copied the actual final collection into a temporary directory, ran its frozen `tool/observe.py audit --record` with that unrelated working directory, and compared complete file inventories before/after. Both original and relocated audits returned zero; neither changed any of the 50 archived files, including marker/manifest files. No C++ binary was invoked by this portability check.

Auxiliary-control offline checks independently verified the completion hash, complete file inventory, exact three-case receipt set, binary/source hashes and raw-output summaries. The thread and label controls were rejected as invalid; the allocator control recomputed a valid three-event ledger with no outstanding allocation. These checks used archived stdout and did not rerun the native controls.

`event-validation-coverage.json` was independently checked against the exact 63-kind catalog. Every listed ordinary-run occurrence count was recomputed from its referenced raw stdout; the classification totals match: 29 with count/byte/ledger/partial-snapshot assertions, 11 structural-only, one invalid-control-only, and 22 not executed this round. A partially asserted kind is not a claim that every event or value for that kind was independently predicted.

The W4 receipts and archived process outputs support two built profiles and 20 final native process runs: ten original F comparators and ten diagnostics, including the required invalid overflow control. The compiler identity is Bloomberg Clang 21/P2996 commit `d77eff1cbd78fd065668acf93b1f5f400d39134d`; compilation used the explicit macOS 26.5 SDK. The final build records 1,018 external-file hashes. This is native macOS evidence, not native Linux or Windows execution.

## Source and semantic review

The transform preserves F's first-fit whole-donor selection, wire layout/cache transitions, bounds checks and return/exception decisions. The wrappers forward the original byte operations and POSIX calls; OS wrappers restore the real call's `errno`. Additional recorder calls do not allocate or throw. The tagged allocators forward to `std::allocator`, preserve always-equal/no-allocation move/swap behavior, and isolate diagnostic injection by thread/domain. Their changed types and the probes can still perturb layout or timing, so these binaries remain diagnostic/semantic evidence only.

The recorder uses one writer per lifetime TLS slot and reads buffers only after joins/quiescence. Cross-thread sequence assignment is atomic. Recording an allocation before it is published and recording a free before backend release give the allocation-pointer ledger a usable address-reuse order. This reasoning does not establish physical simultaneous allocator peaks, nor does the current evidence constitute a new TSan run.

Prior findings are resolved in the final reviewed evidence:

- Page-reclaim inputs cross complete pages on the actual 16 KiB-page host. The VM failure case records the expected Prepare/Reclaim bits and a real successful reclaim retry.
- Fixed cases check deterministic semantic constants and reject the earlier empty transfer/VM traces. Non-registry outstanding allocations fail the fixed-probe check.
- Shared domains 2/3/4 are excluded from the nonshared owner metric; allocation release attribution comes from the matched allocation's origin rather than the current Scope.
- Thread/event/UTF-8-label limits, unknown kinds/domains, primitive types, sequence continuity and ledger/registration consistency are checked even under Python optimization.
- The final transfer trace contains one exact-zero-owner lookup miss and four moved-from snapshots with owner zero and dynamic bytes zero despite an enclosing owner Scope. Live `copy_bytes` reports 4,096 bytes; moved-from empty `copy_bytes` emits no fictitious copy event.
- Bad bool input is rejected for the specific invalid-bool diagnostic, not any exception.
- Build/collection execute recursive parent audits; offline collection binds fixed F, profile flags, binaries, exact receipts/raw outputs and frozen observer/auditor/catalog. Source generation and audit no longer create cache files in frozen evidence.

## Remaining limits and handoff actions

No blocking source/evidence finding remains for the scoped verdict above. Before publishing the handoff, the implementation owner should refresh `HANDOFF.md`, `handoff.json` and any superseded proposal-status fields to final 06 identities and actual executed status. At review time those status files still contained earlier no-C++/working text. They are outside this reviewer's authorized write set.

The fixed metadata-failure probe directly checks unchanged free-memory accounting and successful donor retry. It does not write/read a payload sentinel or explicitly assert logical length in that case. The implementation owner corrected the results table to these actual checks after this review reported the wording; broader payload/length preservation remains source reasoning rather than a new assertion from this case.

Interpretation must retain these restrictions:

- The 22 unexecuted kinds include growth, wire flush/transfer, some container shifts, fallback string branches and OS result paths. Their anchors/catalog entries are reviewed, but this round provides no native execution claim for them. W3 must add relevant workload assertions before relying on them for attribution.
- Allocation bytes cover five scoped backend-request domains. Fixed Core object capacity is a separate snapshot. These are not process allocation totals, allocator usable size, RSS, or owner-exclusive shared metadata. Current raw-identity peaks can merge reused arena generations.
- The fixed-probe end condition admits retained domain-2 registry metadata and separately proves no active registry owner. The generic auditor reports outstanding resources rather than deciding a custom workload's allowed retention. W3 must preregister and check its exact retained resource classes/counts/bounds.
- Core erase movement is estimated logical record movement. Explicit byte-operation extents, completed vector-element extents and logical string-output extents have different meanings and cannot be added into total physical copy traffic.
- Lock durations include probe perturbation and omit recording/unlock tail overhead. Global order is observation order. Dump requires full emitter quiescence; recorder overflow invalidates the whole trace.
- Original-F parity is intentionally narrower for diagnostic-only allocator injection and overflow controls. `n3_original` is a semantic comparator, not a certified formal-timing binary.
- No network consumer, business workload, D/F reproduction, single-variable ablation, Windows result, native Linux result, new sanitizer/TSan result or adoption claim is supplied by this review. Full N3 and the overall roadmap remain open.

The next valid gate is W3's integration of the frozen diagnostic package with explicit workload/resource checks and a separate uninstrumented timing build. No production-header or public-API change is approved by this review.
