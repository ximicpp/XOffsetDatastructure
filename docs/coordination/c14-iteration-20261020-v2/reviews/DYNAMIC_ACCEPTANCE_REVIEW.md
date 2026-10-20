# Dynamic acceptance supplement review

Date: 2026-10-20 (Asia/Shanghai). Reviewer: `phase_c_source_review`.

## Decision and scope

**No blocking static finding.** The supplement correctly separates logical mechanism evidence from successful execution admission, bare codegen and performance. This review does not admit a C or M2 run, a screen, or Q/F. It does not turn a successful synthetic case or positive counts into speed evidence.

Read the specification, source bridge, offline checker, selected actual final diagnostic source branches, and the final audit's output schema. Independently hashed the actual final materialization, source identities and selected files described below. No actual diagnostic results were consumed; the checker, materializer, synthetic cases, compiler, native program and timing were not executed. Only this review file is written. Original producer/materializer and the full source/SDK closure are not re-reviewed.

## Reviewed stable identities

The initial and final reads agree:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `DYNAMIC_ACCEPTANCE.md` | 7977 | `4a0be688c34f1af2d35dd9db3b18fe99632a49494ca6c5af62fc295f73c935bd` |
| `DYNAMIC_ACCEPTANCE_BRIDGE.json` | 8664 | `7ed0a1106699d4a0ccde27d0087ff378848fd4dc3ec223f44ceaa19da730473e` |
| `check_dynamic_acceptance.py` | 7389 | `7315074f7cd37f60c893d5a22e71574324bb80194ac86fb217940b62e96cf48c` |

Independently read and hashed the actual WSL `common-diag-final-v1/MATERIALIZATION.json` (255150 bytes, `ca28fec8e0cf8119d6d73fb28e74dc4d2a97f9dae3d60a048c7de66925c82f38`) and all three `FINAL_SOURCE_IDENTITY.json` files; each matches the bridge. For each role, the actual main header, hook, adapter, final driver and event channel match the five recorded selected identities. All three final driver hashes are `399282a2f5eb7cb85b6e89ba5de6b3380074c2d5e0cdc1ad17ff0048508dd15c`. The draft requests, independent assembled-source review, stable materializer and wrapper identities recorded in the bridge also match actual files. The Phase C job is expressly a current snapshot, not frozen by this supplement.

This verifies the selected source bridge into actual materialized files; it does not independently repeat the 154-member identity audit, execute the materializer to reproduce a driver, or bind a final executable/runtime/input. Those remain external gates.

## Snapshot and predicate audit

- The checker requires exact nine-field uint64 count dictionaries and excludes booleans. It verifies canonical=inactive+active and vector subset relationships in every cumulative and derived count. Phase subtraction rejects reversal. Python arithmetic does not silently wrap uint64 sums.
- Primary P0=0, P8=counts and all eight phase increments are retained. `read_sum` is query-before plus query-after increments, not the intervening interval. Mutation stage 1 is M1−P2; subsequent stages are Mi−M(i−1), with M0=0, M9=P3 and nine-stage conservation against primary mutate. This avoids attributing load/query counts to mutation stage 1.
- R1 requires equality of all nine logical fields at primary, read_sum, each phase and all nine mutation stages. Its positive target predicate is specifically read_sum.inactive>0. A hit elsewhere does not pass. Zero target hits close without promotion; changed work closes invalid. This establishes actual instrumented inactive-path eligibility only, not a dynamic saved-call or cycle measurement.
- The M2 formula matches the actual frozen branches. The original non-string/nontrivial containment miss subsequently invokes begin(data) and end(data?data+size:null). A null miss removes two counted data calls and no canonical calls; a nonnull miss removes three data calls and three canonical calls. Thus with N=nontrivial misses, D=data-call reduction, K=D−2N and C=canonical-call reduction, E≤K≤N and C=3K are correct for this closure. Empty allocated vectors can contribute K without E, so requiring K=E would be wrong.
- M2 checks equal owns/vector/hit/miss/nonempty structure at every scope, nonincreasing active/inactive classifications, the exact data/canonical conservation equation and positive mutate E/D/C. Both null and nonnull paths retain their original traversal counts; no requirement is incorrectly imposed that every modification stage must contain eligible work. These equations do not claim callback safety for arbitrary ADL/type closures, nor replace the separate bare 4-to-1 codegen and lifecycle proofs.

## Failure and evidence boundaries

The checker reads exact externally SHA-pinned audit bytes, rejects duplicate JSON keys and nonfinite constants, requires successful diagnostic/truth status and complete primary masks, and reports invalid evidence for parse/schema/arithmetic failures. The final Phase C audit source emits the `status`, `diagnostic_only`, `mechanism` and `replay` structure the checker expects.

As documented, the caller must establish that each pinned audit is the authoritative output of the admitted full external closure, including exact role/receipt/binary/profile/input, event and syscall checks, full truth and resource/deadline/cleanup gates. The offline helper itself deliberately does not prove those by trusting an embedded `status=passed`. A failed/incomplete C14 role cannot serve as M2 control, and missing or changed shared-control identity does not admit a new control, candidate run, replacement or prefix comparison.

The synthetic-status list is explicitly metadata-only and was not independently executed by this review. Neither a synthetic pass, mechanism hit nor checker exit 0 grants elapsed benefit or performance admission: the returned flags explicitly leave execution/codegen unchecked and speed/screen/QF false. No evidence prefix from a failed registered diagnostic is accepted as a favorable result. Output intervals remain nested observations with observer overhead and do not establish pure allocator cost or explain the historical M1 timeout.
