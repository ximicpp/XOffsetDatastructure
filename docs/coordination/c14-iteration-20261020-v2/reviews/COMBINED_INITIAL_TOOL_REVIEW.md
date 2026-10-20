# Combined initial tool review

Passed the limited source review for `REQUESTS_COMBINED_INITIAL.json` (53,428 bytes, SHA256 `3006c3057c6f05300d391e71cb5a70ccdfd0417abc1819bdc9df1c836bde0e2c`). No substantive blocker was found. Validation can reuse this review for its independent exact initial admission. This does not report any new executed test or performance result.

The request binds combined main `155f0847…`, identity `f88dba6a…`, source acceptance `6b92309f…`, source154/production14 and the fixed 61-byte outer launcher `c8ea06a7…`. The dispatcher checks exact initial tool/input pins, candidate members, root selection, retained component references and production14 before handing off. The inherited worker retains source/input checks and the launcher check after execution. The existing combined source acceptance is reused; its proof is not repeated here.

The new `combined_full/full_entry.py` differs from the previously verified M2 entry in exactly two places: its documentation/output namespace becomes `validation/combined_full`, and its candidate whitelist becomes `COMBINED`. The old M2 entry still matches its previous normal request pin `b7abd036…`; shared `run_native_targets.py` still matches the previous M2 request pin `35b8d192…`. Nineteen small execution-tool files, including the original supervisor, were compared against the R1/M2 screen freeze pins and all remain identical. No frozen shared tool was changed by this review or required to support this initial request.

The initial scope is three jobs: normal73 with a 2,400-second outer cap, SAN73 with the same cap, and external4 under the original 900-second supervisor. The full dispatchers run outside a lease and each acquires the original shared lease once. The external request enters that supervisor directly once; the reused native helper requires its held lock and does not create another queue or lease. Output namespaces are fresh and separate from solo R1/M2 evidence. There is no retry or replacement allowance.

The inherited checks retain at least 8 GiB free on WSL and backing storage, a backing snapshot no older than 900 seconds, build parallelism two, and CTest's per-test 120-second cap. These are static checks, not a fresh resource measurement by this reviewer. Validation must still confirm current resources and the actual supervisor cleanup records. Failed prefixes and failure receipts remain preserved; a failure or timeout cannot become a timing result.

Full73 continues to use the original runtime, subprocess and tooling inventory and real JUnit with exact names and successful run status. Normal assertions, compile database, source snapshot and SAN flags/runtime environment remain part of actual postrun verification. The SAN environment remains `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:allocator_may_return_null=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.

The two external fixtures are byte-identical to their accepted solo requests and current files:

- `test_r1_thread_handoff_v2.cpp`: 6,867 bytes, SHA256 `4ede309cf78c7e62faa16960470298c174ac4418512897a2a6d8b42994ff2cd6`.
- `test_m2_owns.cpp`: 3,033 bytes, SHA256 `83bd4f85106cdf929e6418d29fe3c00f2f4c8b04a66e7c59c977ed409e1d8e0d`.

Each fixture has normal and SAN compilation/run commands. The eight ordered commands preserve `-UNDEBUG` after `-DNDEBUG`; four compile caps of 120 seconds plus four native caps of 20 seconds total 560 seconds within the unchanged 900-second outer cap. Actual successful return codes, SAN environment and exact commands still require execution receipts.

Later business tools are explicitly outside the immutable initial closure and must not block this initial admission. Initial correctness completion will not establish complete combined business gates, screen eligibility, Q32/F64 acceptance or any competitor performance target. This review ran zero project Python programs, queues, compiles, project native programs or experiments and rehashed no large body.
