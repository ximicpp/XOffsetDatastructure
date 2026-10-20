# Phase C final diagnostic source review

Date: 2026-10-20 (Asia/Shanghai). Reviewer: `phase_c_source_review`.

## Decision and scope

**No blocking static finding in the reviewed final-source tools.** This conclusion applies to the exact revisions below, not to a runnable Phase C closure. The root must materialize the final sources, bind actual generated source/compiler/runtime/input identities, enforce the full 600-second window and existing resource gates, and perform the already scoped observer checks before admission. No compile, materializer, dummy, C++ program, or performance process was executed by this reviewer. Only this review file is written.

Reviewed `event_channel.hpp`, `prepare_final_driver.py`, `common_diag.hpp`, the prepared C14 driver and save markers, `SOURCE_READY.json`, `INTERFACE.json`, and the prior hook review. The observer wrapper's already accepted monitor/reaper/self-test scope is not re-reviewed here. This is not a new full 154-member or SDK audit.

## Stable source identities

These three files had the same byte count and SHA-256 on their initial read and final verification:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `event_channel.hpp` | 4576 | `c4dd064b5bc97fbc8e093790dad862ee0de6a453b40acab6e5a126e8623fbf6f` |
| `prepare_final_driver.py` | 17439 | `1af8d7329d39aede0454f11c4da455b60d18c173d9db6d533ef22225fcc82b29` |
| `common_diag.hpp` | 2654 | `55221d97734679cfda69cef3f590b80574e54b313afbfa6918316d6d477ed689` |

The observed `SOURCE_READY.json` is 90563 bytes, SHA-256 `b5619305ad36e9f7c3aaece61fea1baa66867f4231a7b9bd922abd78c8099118`. Its pending-materialization status is accurate; its declarations are not treated as actual generated final-driver hashes.

## Event channel

- `event_fd()` checks the inherited descriptor is a FIFO with `O_NONBLOCK`; it does not alter that event descriptor's open-file-description flags. Every accepted line uses a single `write`; EAGAIN, EINTR, EPIPE and short writes are recorded as failures without retry.
- Event `once()` blocks SIGPIPE only in the calling thread, records whether SIGPIPE was already pending, consumes a newly generated SIGPIPE only after EPIPE with no previously pending one, and restores the original thread mask. It does not install or alter a process-wide signal disposition. Errors in mask operations are retained, and caller `errno` is restored, including the outer `event()` and JSON helper paths. This is the reviewed Linux main-thread diagnostic scope, not a universal signal-race guarantee for unrelated asynchronous senders.
- At most 64 attempted events are admitted. The fixed successful flow has 36 events. The fixed labels contain no separators or line breaks. With sequence at most 63, a 20-digit timestamp and four maximum-width uint64 POD fields, the longest POD event is 122 bytes including newline, below the 128-byte array and contract. No NUL terminator is required by the span write. Bounds/conversion failure prevents the write and records an explicit failure.
- JSON formatting and stdout writes happen outside the owning emit. JSON has its own 65536-byte record and 1 MiB attempted-total checks; a partial nonblocking stdout write is an explicit failed diagnostic, not an accepted complete record. Wrapper persistence and combined-output enforcement remain in the existing observer acceptance scope.

## Original run and first output

The materializer's unique-context replacements preserve the primary public sequence and t0-through-t8 clock captures: load, query-before, all original mutations, one owning emit, retirement, reload, query-after and cleanup. It removes the standalone validate command and rejects any derived driver with other than one `A::emit(` occurrence.

Before the primary timer, the original wire/trace reads, trace decoding, hashes and answer-vector preparation remain. There is no added native-state export, DTO, audit, replay, reserve/prefault or owning-output generation before the first owning emit. The save markers retain the original checks, normalization, allocation/size preparation, header/payload memcpy and return in place. Inside emit, the observer only uses fixed POD TLS/stack samples and the existing clock/resource calls; formatting, event delivery and flush occur after t4.

The after-t4 flush does add observer work to the diagnostic t4-to-t5 interval, and the phase-boundary channel calls add observer work to neighboring intervals. This is consistent with diagnostic-only status; the resulting eight times are not bare performance evidence or precise uninstrumented retirement cost.

## Post-t8 replay and result meaning

The complete extra truth replay is bracketed by its own `truth.replay.before/after` events, after the original post-audit and after `enabled=false`. It reads the original wire once, loads once, checks initial trace/query truth, mutates the nine ordered groups with stage exports/hashes, checks final query truth and retires the replay state. There is no owning emit in replay. Its own resource report declares the additional work and `owning_emit_calls:0`.

Primary counters/phase snapshots and `primary_usage` are already frozen before count flush, post-audit and replay, so replay does not enter primary counts or t0-to-t8 CPU/fault deltas. The original native-result JSON key-generation code is left intact. Because original `all_usage` is observed after the inserted replay, its existing `peak_rss_after_audit_kib` value also includes replay and diagnostic activity; it must not be described as the unmodified driver's isolated post-audit peak. Explicit replay records and diagnostic-only interpretation keep this from being a source blocker.

Emit sample validation checks one invocation, all nine seen bits, both syscall statuses and nonzero monotone clocks. Exact stage identity is checked by the complete replay, not by the mutation bitmask alone. Sample/phase instrumentation still has overhead and can inhibit optimization; counts are logical mechanism observations, not cycle estimates.

## Body and identity bounds

The injected common `write(span)` guard checks cumulative body size without overflow and at most four files before `fopen`. Every original body-output overload reaches that guard. Original writes remain after t8; success requires exactly four. Thus a role cannot produce more than 64 MiB through the four original body writers, and the two C roles total at most 128 MiB. This does not substitute for the wrapper's independent log, disk and descendant/resource controls.

The materializer requires a fresh destination, verifies prepared generated inputs and the fixed driver/adapter/hook, derives the same final driver for all roles, and copies source inputs without modifying originals. It rechecks prepared/source origins and tool/input hashes around materialization. It inventories actual copied/generated files before writing each `FINAL_SOURCE_IDENTITY.json`, then emits `MATERIALIZATION.json`; it does not claim a speculative final source hash before generation. Those actual artifacts and the external compile/runtime/input closure still require the root's final freeze. Preparation success alone grants no native run and consumes no diagnostic sample.
