# R1 targeted native request review

Date: 2026-10-20, Asia/Shanghai. Scope: read-only source review of the new thread test, finite runner and actual request. No compile, native run, sanitizer execution or timing performed. This review changes no baseline, frozen candidate, test, tool or request.

## Decision

Thread test source: **no blocking finding**. Finite command selection/prefix: **matches the intended ten compilations and ten executions**. Final runner/request pair: **no blocking finding**, subject to validation checking the exact original-supervisor dispatch and current input identities. Native correctness remains not run.

Reviewed identities:

- `test_r1_thread_handoff.cpp`: SHA256 `d0b1a99c244247cee70ddf9d6db812915367546197bc6182a3b5f7c70979640b`.
- `run_native_targets.py`: SHA256 `35b8d1928b2cb166935fa83e438970b22ce1e8eab7d87f8ae7521b4d5ded6f2a`.
- `REQUESTS_R1_NATIVE.json`: SHA256 `bcdef196cb2e7f3baa6439c16a145ac79528afd972f544207ef6785269de6833`.
- Request's production main: frozen R1 `b4c61b2d286d6d4ffdfbd44af3ff7151b1640dd07f8b9a2c9bfcfa01a6a9c23f`; its 154-member manifest marks only the main header changed relative to C14.

## Final identity schema recheck

The final runner contains `matches(p, identity)`: SHA256 is mandatory and compared, and byte length is compared when registered. Both pre/post `input_files` loops use `not matches(...)`. Thus the 13 inherited compiler/loader/runtime entries carrying SHA256 only are supported, while the new test/runner and other `{bytes, sha256}` entries retain both checks. Source members independently require exact bytes plus candidate SHA256.

An earlier file read during preparation showed full dictionary comparison and prompted a schema concern. A fresh final read confirms the corrected implementation above, and the final runner's actual SHA256/bytes match the request's registered `35b8d192…` / 3835 exactly. The concern does not apply to the frozen final request and is cleared. No execution failure or rejected native window is recorded.

## Thread ownership and synchronization

For each `round`, `live = 4*round+1`. Worker creates/checks real C before publishing `transfer` and `phase=live` under the mutex. It then waits for `live+1`; it does not access that mapping while main holds it. Main moves the buffer out under the same mutex, uses it exclusively, moves it back and publishes `live+1`. Main then waits for `live+2` without accessing C. Worker reacquires it, checks and destroys it, verifies count zero, and only then publishes `live+2`. Worker waits for main's `live+3` before creating the next C. Thus zero/one/zero observations are sequenced after registration/retirement, and the old numeric-anchor query occurs before the next round can acquire a new mapping.

The ordinary source/copy buffer and immutable wire are initialized before thread creation; the worker only reads the wire. Only main checks/mutates the ordinary copy. Registry and active-count activity across threads is intentional; the same payload is never concurrently accessed or retired. Moves through the shared optional occur while holding the mutex. CV predicate reads, phase/cancel/error publication and shared-optional access are synchronized. Final `error` read follows `join`.

Worker exceptions publish `error` and `cancel` under the mutex and notify. Every main wait observes cancel; main rethrows the recorded exception, sets cancel, notifies and joins. Main-side failures likewise cancel and join; worker cancellation returns from either wait and releases its lock by RAII. `unique_lock` unwinds before the worker's outer catch locks the mutex. No ordinary exception path reviewed leaves main waiting without notification or destroys a joinable thread. OS-level hangs and mutex failures are outside this source proof and remain bounded by the native command deadline and original supervisor cleanup.

The C owner is genuine: `begin_verified_receive` + acquire operation + full header/payload copies + complete + `finish_verified`. The reflected active ledger assertions test ordinary zero, live C one, then retired zero. `check` repeats real root/data/canonical queries, covering first lookup and subsequent TLS-warmed uses on each participating thread. Main additionally exercises move/swap while worker waits; the allocation reuse assertion checks actual live C allocator reuse. It does not claim guaranteed OS mapping-address reuse, TSAN coverage or a runtime result before execution.

## Command closure and original lease precondition

The request contains four retained programs (`test_canonical_zero_target`, `test_canonical_cold_no_callback`, `test_canonical_registry_v2`, `test_owner_identity`) and the new handoff program, each normal/SAN, in compile-then-run order. All ten compile prefixes were compared with the corresponding prior `M1_CORRECTNESS_RESULT.json` normal/SAN command prefix after replacing only the old staged candidate include root. They match. Each command supplies the intended final source and output; candidate headers precede the source through the explicit candidate include path, and retained test source paths remain inside the frozen R1 tree. The final `-UNDEBUG` preserves assertions. SAN retains address/undefined flags and the registered resource directory. Compile/native deadlines are 120/20 seconds. There is no codegen rebuild, benchmark, screen or retry command.

The request binds source members, new tools/test and runtime inputs; runner verifies them before commands, uses argv lists without a shell, creates a fresh output directory, stops at the first command error, and records commands/status with zero timing samples. It performs a nonblocking observation of the existing queue lock and immediately releases it if unexpectedly acquired; it does not create a second lease/scheduler.

**Required validation execution precondition:** validation must launch `worker_argv` as the command supervised by the original `run_job.py`, inside that supervisor's owned session and current global lease. Merely running this tool while another process holds `queue.lock` is insufficient: the runner's guard proves that *some* holder reports running, not that this invocation belongs to it. Validation must bind its exact outer dispatch, current lease/run identity, timeout and output before execution; this need not mutate the frozen request.

Under that precondition, descendant cleanup is already provided by the original supervisor: it launches the runner with `start_new_session=True`, scans the owned session after leader exit, fails a leader leaving running descendants, and calls `ensure_stopped` before releasing the queue lock (`run_job.py` around 235–264 and final cleanup). New runner subprocesses do not start another session, so compiler/linker descendants stay within that cleanup domain. `subprocess.run`'s timeout alone would only kill its immediate child, but the retained supervisor closes that gap. Do not introduce a separate session that escapes this original tracking. No cleanup behavior was executed by this reviewer.

With final identities plus exact outer dispatch checked by validation, this source review has no blocking finding for the bounded B batch. It does not admit Phase C, full candidate gates, screen or Q/F, and it does not establish native correctness or speed.

## Final v2 wording-only appendix

The active request is now `REQUESTS_R1_NATIVE_v2.json`, SHA256 `edcf2fb2d29f5d5249d8d1ebc2d7727bafcd2d44a503ff1bacd839018edf7d4a`. It binds `test_r1_thread_handoff_v2.cpp`, 6867 bytes, SHA256 `4ede309cf78c7e62faa16960470298c174ac4418512897a2a6d8b42994ff2cd6`. The runner and frozen production candidate remain unchanged.

**Correction to the earlier coverage paragraph:** repeated `check` calls do not prove a warm TLS cache. Cold canonical recovery deliberately does not install a TLS entry. The earlier claim that this test demonstrates both first and warmed lookups is withdrawn. The thread test proves synchronized ownership transitions, stable repeated domain results, moves, retirement and live allocation reuse. Actual deliberate cold/hot cache coverage is supplied collectively by the retained fixtures, notably `test_canonical_zero_target.cpp`'s controlled cold/hot anchor records (around 135–152), `test_canonical_cold_no_callback.cpp`'s cache controls, and `test_owner_identity.cpp`'s asserted hot/cold lookup counters (186–210). These remain source coverage claims pending native execution.

Normalized source comparison confirms only the comment and success-output wording changed; assertions, loops, synchronization, schema, ownership operations and error paths are identical. The v2 file also has CRLF line endings compared with the preserved v1 LF file; this accounts for the broad raw diff and has no C++ behavior effect. No new native behavior was added to manufacture cache coverage.

All 20 v2 command argv lists were checked against v1: the only argv changes are `r1-native-v1` to the fresh `r1-native-v2` output prefix and the two compilation source arguments to `test_r1_thread_handoff_v2.cpp`. Their kinds/deadlines match; the four handoff command labels gain `-v2`. Source-member manifest, R1 main hash, runtime identities, compiler prefixes and runner are unchanged. Existing input bindings remain identical; v2 adds bindings for the new test, preserved v1 request and revision generator. `worker_argv` selects the v2 request, and revision/READY accurately record the supersession.

READY identifies v1 as `superseded_unexecuted_request`; v1 remains preserved. Both compile/native budgets remain ten total for B, not two independent batches. **No blocking finding for this final v2 request**, under the unchanged original-supervisor/lease execution precondition and validation's final current-identity/dispatch check. This reviewer performed no compile/native execution and grants no timing or screening admission.
