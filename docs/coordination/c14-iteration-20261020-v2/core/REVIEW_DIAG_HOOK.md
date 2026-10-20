# Common diagnostic hook and materializer source review

Date: 2026-10-20 (Asia/Shanghai). Reviewer: `m1_safety_review`. Reviewed `tools/c14_iteration_20261020_v2/core/common_diag.hpp` and `prepare_common_diag.py`, the generated C14 source/driver/adapter deltas, and the three prepared role identities. No compiler, materializer execution, native program, performance process, or heavy test was run. Only this assigned review document is written.

## Decision

**The hook/materializer structure is suitable for diagnostic-only preparation, subject to the sample-error fix below. No native execution is admitted by this review.** The validation-owned bounded external channel, monitor, flush, and post-primary complete nine-stage truth replay remain absent from the reviewed driver and must be implemented, tested, reviewed and jointly bound before any C run. A generated identity saying `diagnostic_only` is not an execution gate or a full input identity audit.

One evidence-integrity issue needs resolution before native freeze: `sample()` currently ignores both `clock_gettime` and `getrusage` return codes. Their zero-initialized outputs can become a misleading complete-looking zero/partial sample on failure, and `emit.seen` still marks that index present. Add nonthrowing POD status/failure bits (per sample or accumulated) recording each return code and make the final reader reject any failure. Do not throw or allocate to report this from the noexcept output callback. Positive/monotone timestamps alone cannot establish that the resource call succeeded.

## Reviewed identity

Independent `Get-FileHash` reads match:

| Tool | SHA-256 |
| --- | --- |
| `common_diag.hpp` | `ab9ea2c8c26a0dc297a81bdc94d09ef4dd5f9da47be458e5cbff7c48f6460308` |
| `prepare_common_diag.py` | `206fc8889221732de5b3994a890e2fc0c565a473003d64ed42531fb2f01db232` |

The prepared identities record identical hook/tool bytes for C14, R1 and M2. They also record identical diagnostic driver `5c6aa00b666a2f998318af731a87e67bb5fec89c264f77fe87df2690e019f2db` and adapter `335114f010466a453bf56cf9aed23c77bebd48b7288828e5df41753e5c1d4766` in all three roles. The recorded instrumented main headers are C14 `1abf4095345f93913aa905fe0faa1448ea5128832e635d6bdd9cd23566694b9b`, R1 `230e23e6d2fc7a005c6e84a4bb70baaf6dc364b32065e551782434dab87c49c2`, M2 `089036e7f8f3e94144d7c41f2e2fae38eec54066175216ad359a233bdb01bfa3`. These are preparation identities, not final runnable closure identities; the pending fixes and validation additions will change them.

Generated Windows text files have CRLF differences from retained LF inputs. The reviewer used an end-of-line-insensitive diff to inspect semantic insertions after diagnosing the initially inflated whole-file diff; raw byte hashes remain distinct and must be bound exactly. This review does not assert that rewritten bytes are identical to the bare source. The 154 copied-member closure and locked dependencies still need the final validation identity audit.

## Count hooks and actual call ordering

The canonical hook increments `canonical` and then evaluates the original acquire load **once**. Its observed zero branch increments `inactive`; its nonzero branch increments `active`. It does not reread the atomic to classify the event or hoist the result to another call. On complete ordinary execution the accounting invariant is `canonical == inactive + active`, including separate cumulative stage deltas. The active implementation body and all original TLS/registry decisions remain unchanged in this diagnostic insertion.

`OwnsScope` records entry and depth using POD TLS state; its destructor only reverses the depth when that instance observed enabled state. No allocator, custom lifetime function, destructor of a production object, comparison, callback, wire hook or record release is introduced. The existing predicate's recursion, early returns and short circuits remain. Its generic ADL boundary remains as described in `REVIEW_M2.md`; counters measure entries to this actual implementation, not arbitrary external same-name ADL overloads.

The const-vector-data counter is inserted at the start of the actual `XFixedVector<T>::data() const` body. It counts entries only while diagnostic counts are enabled and at least one observed owns frame is active. It performs no extra `data()` call or canonical recovery. Counts can therefore cover nested descendants and any code reached while inside the owns dynamic scope; the name must not imply an unqualified per-node optimized instruction count.

Vector-node, capacity-hit and nontrivial-miss hooks preserve the existing containment call and its position. The diagnostic nonempty classification reads `value.size()` only after a nontrivial containment miss. This is an extra pure descriptor read, not a data resolution or an extra acquire load; M2's own later size read remains in place. Its observer overhead and changed optimization behavior must be disclosed. Instrumenting `data()` makes source-level entries observable and can inhibit compiler elimination; use bare optimized codegen separately for the real mechanism and never convert these logical counts into cycle percentages or timing gains.

`MutationScope` begins after the original nonempty group span is formed. The original `while (offset < operations.size())`, `stop = offset + 1`, and matching-stage expansion guarantee `group.front()` exists. `Mutation::stage` is uint32 and converts to the unsigned stage parameter. The scope covers the original erase-stage `continue`, so that group's snapshot is retained. No mutation operation, original grouping, public move call, trace content or order is changed.

Mutation snapshots are cumulative counts, not isolated counters; the reader must subtract consecutive group boundaries (and subtract the preceding primary phase snapshot for the first stage). Trace stages are 1 through 9, using array slots 1 through 9 and the corresponding seen bits. The array has ten slots. A destructor during an exception can also set a seen bit for a partially executed group, so `mutation_seen` alone is not proof of a successful stage: require normal exit and the complete independent stage truth replay. Repeated stage groups would overwrite a slot; the fixed original trace's contiguous, ordered nine-stage structure must be validated rather than inferred from the mask.

## Primary driver and output materialization

The inspected generated driver diff preserves the original single `run` calls and order: load, before-query, all mutations, one owning emit, modified-state retirement, reload, after-query and cleanup, with original t0 through t8. It only enables counts/phase0 before t0, snapshots immediately after the original t1–t8 observations, and disables after phase8. Post-audit remains after t8. In this preparation there is no extra pre-primary native export, owning output, DTO construction, mutation replay or output warmup. Phase snapshots themselves add POD writes to neighboring diagnostic time intervals; none of those clocks are bare performance evidence.

The save hook only adds counter writes and fixed-size `Sample` writes. `Counts`, arrays, `Emit`, counters and flags have static/POD TLS storage; no diagnostic vector, string, ostream, map, heap allocation or dynamic event buffer is created in emit. `sample` uses local POD `timespec`/`rusage` and the named libc system interfaces. The added hooks do not call replaceable C++ new/delete, custom wire policy, PMR or registry replacement. This is a source-level property for the locked Linux profile, not a universal claim about every platform/interposition/signal environment. RUSAGE_SELF describes the process, not a uniquely isolated thread.

Original save prerequisites and mutations stay in emit: typed/schema checks, `shrink_to_fit`, root/allocator resolution, payload-size retrieval, full wire-header preparation, original string size/limit checks, `resize_and_overwrite`, header memcpy, payload memcpy and return. The hook does not create another output string or reserve/resize the output earlier. The first marker inside the callback is before header/payload copies but receives only a byte count, so it does not access the uninitialized output storage. Markers after each memcpy follow the original writes. They touch diagnostic TLS/stack storage only, inside emit; they do not prefault the output buffer or move output page preparation into load/setup.

The marker labels must be read precisely:

- 0→1 includes save entry checks, normalization and root/allocator lookup, not exclusively normalization.
- 1→2 includes header construction.
- 2→3 includes default string construction, size checks and output-size preparation.
- 3→4 is entry-to-callback string preparation, allocation and implementation work; it is not a pure malloc measurement and its page faults are not independent of materialization.
- 4→5 and 5→6 bracket header and payload copies respectively, with observer syscalls at their boundaries.
- 6→7 includes callback return and resize-and-overwrite completion; 7→8 is the pre-return interval, not caller-observed return/move completion. The original outer t3→t4 remains the inclusive owning-emit observation.

These intervals are nested in the original emit operation and include observer costs. Do not add their fault deltas to an already inclusive total or treat page faults, allocation and copying as independent additive components. A prior sample syscall itself can contribute resource time/faults to the next delta.

The hook records one set of emit samples and an invocation count, not an arbitrary sequence of outputs. The fixed run should produce one observed save invocation; the final reader must require that and `emit.seen == 0x1ff` for indices 0–8. The current slot9 is unused. If the compiler takes the existing fallback reserve/append branch instead of `resize_and_overwrite`, indices 3–7 are absent; reject that execution/profile rather than silently compare incomplete samples. Failure/partial masks and excessive invocations are not zero-cost results.

## Source derivation and final execution gate

The materializer requires a fresh output path, copies only the recorded 154 source members, adds the hook under a new name, and edits that copied main header. It does not overwrite the input production source or historical manifests. `once` rejects missing/ambiguous exact contexts instead of silently applying a partial insertion. A failed preparation can leave a partial new output; the script intentionally does not overwrite it on retry. Diagnose before selecting a fresh revision.

The tool reads its source argument and uses the presence of M2's local-data line to select an insertion marker. It does not independently prove that the source is one of the three authorized exact bare identities, that the experiment inputs are frozen, or that all unchanged members/dependencies match a lock. The final freeze must bind those complete inputs and the generated outputs with absolute resolved paths. In particular, the prepared C14 input string `.` is not by itself a source identity. No support for unrelated or combined source should be inferred from successful text matching.

Before native execution the final joint closure must include the corrected hook, complete derivative hashes, exact common driver/adapter, locked profile, fixed wire/trace, bounded external event channel/monitor/flush and true error-path tests, body/log/resource limits, registered deadlines/reaper behavior, and post-primary full nine-stage truth audit. Emit-internal markers must remain POD-only; formatting/IO/flush must occur outside emit. The driver/wrapper must retain events from a timeout without relying on normal-exit flushing. Error/timeout stops the remaining registered runs according to PLAN; preparation success is no permission to substitute another run.

The C-stage C14 process must be explicitly registered **before it runs** as the shared M2 control under exactly this final common observer. R1 and future M2 diagnostics must use that same frozen observer/driver/wrapper and original input. M2 subsequently gets its one candidate process under that registration, not a new control chosen after looking at counts. A hook/driver change after C14 execution invalidates that shared-control comparison; it cannot be repaired by splicing a differently instrumented M2 process or reopening an exhausted diagnostic budget. All such evidence is mechanism/first-materialization diagnosis only. Bare C14/R1/M2 binaries and any eventual screen remain hook-free.

## Corrected hook / prepared v2 final-source addendum

The writer addressed the sample error finding before execution. Read the actual revised hook and independently verified SHA-256 `55221d97734679cfda69cef3f590b80574e54b313afbfa6918316d6d477ed689`. `Sample` now contains POD `int clock_status` and `int rusage_status`; `sample()` records the exact return values of its existing two calls. It still makes each call once, constructs no error string, throws no exception, and changes no production/output lifetime. The materializer remains unchanged at SHA `206fc8889221732de5b3994a890e2fc0c565a473003d64ed42531fb2f01db232`.

The old `diag-prepared/*-v1` derivatives are retained, unexecuted drafts. Read the new `c14-v2`, `r1-v2` and `m2-v2` preparation identities and independently rehashed all four generated files per role (12 checks); every actual hash matches its respective identity. All three use the corrected hook, and retain the identical driver/adapter hashes stated above. The three instrumented main-header hashes likewise remain those above, because the status fix is in the separately included hook. The final binary identity must include that hook file rather than infer identity from the main header alone.

**The common hook and source materializer now have no outstanding blocking static finding within this review's scope.** The earlier sample-error requirement is resolved in source. The final validation-side reader must still enforce both status fields equal zero for every required sample, valid nonzero monotone clocks, complete masks and expected one invocation; recording status is not the same as validating it. Include actual failure-path behavior in the validation tool checks.

This resolves approval for the diagnostic-only preparation components, not for a runnable diagnostic closure. The validation-owned external event/monitor/flush changes, their error tests, full post-primary nine-stage replay, shared-control preregistration, final driver/wrapper hashes and registered resource/deadline limits are still unbound and were not reviewed here. No native run, budget consumption, timing interpretation or separate later M2 control is authorized by this addendum. No code or generated artifact was modified by the reviewer.
