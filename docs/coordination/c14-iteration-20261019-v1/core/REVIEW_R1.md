# R1 independent source and safety review

Date: 2026-10-19 (Asia/Shanghai). Reviewer: `m1_safety_review`. This is a read-only production-source review; no compiler, native executable, sanitizer, performance measurement, or heavy archive was run.

## Decision and exact scope

**No blocking source defect found. Admit this exact R1 candidate to code-generation inspection only.** Correctness and actual mechanism admission remain separate pending gates. There is no performance result or permission to integrate this candidate into retained C14.

Reviewed `build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1/source`, actual `R1.patch`, and `IDENTITY.json`. This candidate derives independently from C14 commit `1904b9c71230026ac6f2d58f919a9ab76447d264`, with 154 recorded members and one changed production header. It does not combine M1.

| Independently hashed item | SHA-256 |
| --- | --- |
| baseline root `xoffsetdatastructure.hpp` | `66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3` |
| candidate `xoffsetdatastructure.hpp` | `b4c61b2d286d6d4ffdfbd44af3ff7151b1640dd07f8b9a2c9bfcfa01a6a9c23f` |
| `R1.patch` | `0bfbad574e9d9d5bdd6e326b5ac3a495812f9042b74d40a957b3c3faf44b67ad` |

The reviewer read the actual baseline and candidate code and compared the extracted original cold body, starting at `const auto address = reinterpret_cast<std::uintptr_t>(anchor);` and ending at `return recover(record);`. The 3,973-character source substrings are exactly equal, including whitespace. The patch changes only the enclosing function boundary, private/public labels, explanatory comments, and guarded noinline annotation. This is not an independent audit of every identity-record member.

## Source and lifetime findings

The public `canonical_bytes_at` still first executes exactly one `canonical_active_count_.load(std::memory_order_acquire)` and returns null on zero. The wrapper introduces no earlier anchor conversion, TLS lookup, registry access, allocation, callback, or retained pointer. For a nonzero observation it calls the private active helper with the same three arguments. The active helper performs no second ledger load and does not assume the observed nonzero value remains true.

All original active-path protections remain in the identical body:

- Acquire owner loads in `live`; containing-anchor priority; exact target subtraction/bounds checks; terminate behavior for an invalid selected record; and legacy `canonical_origin == nullptr` fallback.
- The four-slot TLS search and `last` updates, including the special zero-length/one-past rule that prevents an adjacent reservation from choosing a different canonical array origin.
- Cold lookup under the same registry mutex, anchor lookup before target-only fallback, retaining the selected shared record, and leaving the mutex scope before recovery/record destruction.
- No installation of a cold result in TLS, no eviction of a terminal TLS record, no extra shared-record temporary or explicit reset, and no new replaceable-delete opportunity in the original critical section.

The local `record` is destroyed at active-helper exit before the wrapper returns its result to the caller. Originally it was destroyed at canonical-helper exit before the caller received the result. No caller-visible use or publication is inserted between recovery and destruction. The wrapper contains no owned object to destroy, and does not retain the record or returned pointer beyond the call. Its added stack frame does not change the source-level release order. Existing assumptions concerning synchronized owner use are unchanged; this does not make simultaneous unsynchronized payload release safe.

Registration still increments the same ledger with release ordering under the existing mutex; unregister clears the owner, decrements with acq_rel, and extracts its registry node in the original order. These functions are unmodified. A transition between the wrapper's acquire observation and active lookup was already possible between the original acquire and original cold body. The new call adds no cache of the zero observation, no acquisition elision across calls, and no new synchronization promise. In particular, an ordinary owner when the canonical count is zero still uses the original null-return path even if the ordinary-owner registry is nonempty.

## Access, header linkage, and source risks

The new helper is a uniquely named private static member. The public wrapper can name a later-declared member inside the same complete class context. The `public:` label resumes immediately before the original `growth_policy()` and subsequent API; it does not accidentally hide any pre-existing public member. No data field, object layout, static ledger, registry/cache representation, allocator, or wire descriptor changes.

Both methods are defined within the class, so they retain inline linkage semantics for a header definition. `noinline` controls code-generation decisions and does not remove the implicit inline/ODR rules. There is no out-of-class non-inline definition or TU-local duplicate registry introduced. `_MSC_VER` selects `__declspec(noinline)` (including clang-cl mode); otherwise Clang/GCC select the existing compiler extension syntax `__attribute__((noinline))`. On another compiler the attribute is omitted, preserving source behavior while leaving the mechanism unproven. These are static source conclusions, not a compiler acceptance result.

No pre-existing call site is redirected to the private helper, and no caller is permitted to bypass the acquire check. The original wrapper signature and noexcept contract remain unchanged. Reflection-based canonical tests that retrieve specific existing members by name do not depend on the total Core member count in the reviewed fixture. Adding a private function should not replace any of those exact-name lookups; the real P2996 test build remains the acceptance check.

## Code-generation admission and next checks

The writer reported baseline `control-driver.s` observations of an out-of-line canonical helper with pushes before the zero-ledger test and calls from `query_view`. The reviewer has not independently inspected that assembly artifact, and does not treat this report as candidate mechanism proof. The sole heavy executor should first compile the exact ordinary `-O3 -DNDEBUG` driver/call closure, with no extra force-inline flag or instrumentation that changes the actual opportunity.

Codegen must show whether real query/read call sites acquire-load/test the ledger inline and keep inactive execution out of the active helper's prologue. Confirm the active helper remains an actual call/tail-call boundary and that the inactive path does not resolve TLS, retain a shared record, or touch the mutex. A wrapper that remains out of line may retain the same costs; source size alone is not evidence. Report any additional active-path call, frame, text-size, or branch cost. If the intended repeated inactive-path overhead is already absent in the baseline or remains unchanged in the candidate, reject R1's mechanism without timing.

On mechanism success, reuse the existing tests against this source identity in normal and SAN as registered:

- `test_canonical_zero_target`: adjacent reservations, anchor priority, zero-length/one-past and ordinary-domain cases.
- `test_canonical_cold_no_callback`: exact cold no-eviction behavior, actual multi-block drain, and terminal-delete owner-move controls.
- `test_canonical_registry_v2`: cold/hot record resolution, many owners, retirement, move/swap, and allocation-failure/new-handler behavior.
- `test_owner_identity`: stable owner identity and callback-safe reclamation, with its actual assertions and failure injections.

The complete applicable matrix still governs business/V1/consumer and any related lifecycle gates before a screen. These tests are relevant because the code was moved; their existence or historical success is not a candidate pass. No new mirrored test is requested for a body proven unchanged. The screen may still reject an extra active-path cost or fail to establish a read improvement.

## Write boundary

Only this assigned review file was created. Retained C14, M1, the R1 candidate, manifests, validation-owned files, parent PLAN/TASKS/STATE, leases, and historical evidence were not changed by the reviewer.
