# Independent design review: network N3 integration v1

Date: 2026-10-16, Asia/Shanghai. The draft began on October 15. Scope: `PLAN.md`, `plan.json`, `anchor-census.json` and `design-checks.json`, assessed against the previously inspected frozen W1 revision-2 source and W2 runtime semantics.

**Verdict: PASS, design only. No unresolved blocking design finding remains after the warm-output and release-checkpoint refinements.** This review authorizes no implementation or execution and reports no new native result. Counts, syscall totals and budgets below remain predictions.

During the active W3 timing window this reviewer only read the four small design files and wrote this review. No build, C++ run, workload, inventory/hash pass, archive/copy operation or Python test suite was performed. The draft's `design-checks.json` describes its author's earlier lightweight checks; this review does not promote those into runtime evidence.

## Predictions and scope

The proposed 12 fresh-process network cases, two work exchanges and one additional recorded warm exchange only in warm mode keep the experiment small. The independent fixture processes are correctly separated from request cost. Reusing the accepted F diagnostic headers/runtime/catalog and copying only the frozen consumer avoids changing production headers, W1/W3 files or either accepted event contract.

The revised census describes 20 safety anchors across seven logical insertion windows, including the explicit `worker.after_serve` anchor. Those are design constraints, not 20 implemented or executed probes. The supplied check metadata now names the same count; this reviewer did not repeat source hashes or an inventory scan during the timing window.

For the stated nonempty schema and pinned libc++ assumptions, the proposed arithmetic is consistent with source:

- `E=2+(pool==warm)`; run registration and erasure totals are each `1+2E`, including the separate preflight load.
- Each exchange has two load-header copies totaling `2H`, payload loads `Pi+Po`, one save-header copy `H`, one payload save `Po`, and one 21-byte string assignment. Both labels have length 21 and processing changes values in place, so no new arena allocation, growth, wire transfer or vector relocation is expected. `Pi=Po` still requires the planned byte oracle rather than being treated as an executed fact.
- Graph validation registers five ranges: arena header, allocator, root, values and label. With 16-byte range records and capacities 1/2/4/8, each verified load requests 240 bytes in four allocations and releases all four. The transient old/new vector overlap is 192 bytes. This predicts metadata requests, not total verification work or physical memory.
- For two work exchanges, successful reserve/enable/unmap totals of 5/5/5 for none and cold, and 3/3/3 for warm, correctly include preflight and final cleanup. Cold unmaps belong to the subsequent clear/final-clear phases rather than being invented at owner return. Warm work has four hits and four scans; warm preparation has two misses. Stats locks remain included as shared diagnostic work.
- The run's active registered-owner peak is one: the server owner leaves its original block before response send, and client load follows full response receive. Cached mappings may coexist without two active owners. The fixture's readback load overlaps its original owner, so its predicted owner peak is two.
- The independent fixture's one Automatic growth and two extent requests are supported by reserve crossing the initial 4096-byte extent, first without growth and then with Automatic retry. Empty wire cache means no flush or transfer. These are new paths to validate, not inherited execution results.

The exact retained-bucket policy is preferable to allowing any domain-2 leak. Its 16-byte/count/origin prediction is explicitly ABI-specific and must fail on mismatch; it is not permission to loosen the policy after observing a different result. Rebind counts appropriately remain subject to the frozen compiler's actual transfer ledger rather than an assumed universal NRVO count.

## Phase and lifetime design

The holder-before-resource rule is necessary and correct. In the frozen consumer, relevant anchors are receive construction/copy at line 176, server owner load at 219 and original block end at 223, send at 226, client reset/finish at 248, preflight at 283, warm/work at 288/291, and final teardown at 298–299. Keeping the server's original `auto owner` avoids extra moves introduced by an optional owner.

An optional Phase holder declared before the original resources may change labels with `emplace`; its release label must already be active when the original owner block ends. Any causal Scope holder must likewise outlive that owner destruction, then be reset before unrelated send/string teardown work so a dead arena identity does not become an accidental causal owner. The generated loader's explicit unowned input scope and validation-owner scope must remain intact.

The persistent worker's immutable warm-first flag must be initialized before `thread_` starts. Worker-local ordinal assignment and main's existing call-site ordinals avoid a new shared counter or protocol field. Existing ready/done synchronization, deadlines, full completion waits and join remain unchanged. Dump after `run()` returns is the correct quiescence boundary; `worker.finish()` alone is not a replacement for the planned join boundary.

The revised design resolves the release-checkpoint issue: server `wire`/`saved` locals are destroyed as `serve_one()` returns, after forming its return value. It now publishes the server-strings-complete checkpoint in `Worker::loop` after `result_=serve_one(...)` returns and before the existing synchronized done publication (frozen lines 234–235). A destructor guard must not instead write flags into a returned local `ServerResult`, which would depend on NRVO. The selected caller-side checkpoint adds no owner moves and is independent of return-value optimization. It remains a logical lifetime checkpoint, not proof of an intercepted malloc free.

## Sidecar semantics and warm-output oracle

The proposed sidecar is the appropriate place for transport-header assembly, receive materialization, string size/capacity and IOProgress. Those are not existing N3 allocation domains or VM/copy kinds. No `grow_zero` alias, generic host-allocation alias, socket-byte-to-memcpy conversion or new kind is justified. Any such extension requires a separate contract revision.

Before implementation, specify units and checkpoints precisely: receive size and initialization request include the existing wire header; `std::string::capacity()` excludes its terminator and is not allocated/usable/RSS bytes; a completed 88-byte header memcpy is separate from kernel receive bytes; saved/client-output capacities are snapshots while their strings are live. Keep raw consumer observations separate from the final collector-bound sidecar so stdout, output and trace hashes can be added after process completion without rewriting W1 JSON serialization. Phase labels for a stage with no emitted N3 event do not measure its duration or prove zero cost.

The original warm call discards its returned output. The revised draft now adopts the bounded archival addition and accurately distinguishes its oracle from a direct comparison against an unrecorded W1 warm artifact. This is reasonable for a diagnostic-only copy:

1. Receive the existing warm `Sample` into a short lexical scope, archive its owning output, and destroy it before the first work call. Keep owner reset, server finish and all protocol operations unchanged.
2. Label archive/release as diagnostic preparation. This deliberately extends the warm response-string lifetime and adds file I/O; do not call it the unchanged original string lifetime or timing evidence.
3. Compare it against the canonical original-W1 `output-0.wire` for the same deterministic input and budgets. Name this comparison accurately: N3 warm versus original W1 work response. There is no original W1 warm artifact to claim a direct pairing against.
4. Bind the extra artifact in the sidecar and file-budget forecast. Across four sizes it adds about 17 MiB before metadata; the revised raw-output estimate is about 257 MiB, still inside the proposed 512 MiB ceiling, subject to actual pre-write checks.

This addition need not change a W1 CLI field, schema or arena-owner move. The same marker, label and per-value checks remain required. The revised design also correctly states that Worker mutex/condition-variable work is outside the existing lock kinds, and that pre-registration load/pool/OS observations must bind to the next unique successful generation using endpoint/cycle/phase/thread/base rather than to an ended generation. OS mapping lifetimes remain separate.

## Budgets and remaining gates

The 16 MiB payload does not by itself threaten the recorder: fixture reserves before filling, normal processing is in place, and no socket-chunk or element events are proposed. The per-thread 2048-event design limit and two-thread run/one-thread fixture limits are conservative hypotheses below the unchanged 16384-event/16-lifetime-thread runtime ceilings. They must be checked on the future complete traces, with warm and preparation prefixes retained. Runtime overflow, a stricter design-budget violation or a true OS failure invalidates that condition; neither sampling nor expectation tuning is allowed.

The 512 MiB file ceiling must cover the proposed new trace, sidecar, fixture, output and any warm archive files; it is not a process-memory limit. The roughly 44 MiB recorder object space, owner reservations, accessible prefixes and shared idle-pool bounds remain separate. Repeated bytes, shared metadata and concurrent request counts must retain the meanings already stated in the draft.

No blocker was found in the stated count formulas or minimal source windows. Safe publication of string-release checkpoints and the chosen warm-byte oracle/lifetime exception are now explicit in the revised design. Future implementation still needs exact transformed-source review, frozen sidecar/phase definitions, original-W1 byte comparisons, network-specific negative audits and W4-gated bounded execution. None of those results exists by virtue of this design review. Formal timing and adoption remain solely with W3 and the parent gate.
