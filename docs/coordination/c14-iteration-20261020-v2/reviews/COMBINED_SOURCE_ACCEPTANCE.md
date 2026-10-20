# Combined R1 + M2 source acceptance

Date: 2026-10-20 (Asia/Shanghai). Reviewer: `phase_c_source_review`.

**Passed for exact static source composition only.** The combined candidate still requires its own functional gates and final comparison; this review proves no combined elapsed benefit.

Independently hashed all154actual baseline members and all154combined members, matched14production members, and checked the exact source inventory. Only `xoffsetdatastructure.hpp` differs. Baseline main is `66818de4...`, actual combined main is279066bytes/`155f0847...`; actual/core identity files are byte-identical. Root selection `bcbb7c0e...` and proof/precheck identities match. Baseline154 remained unchanged across review.

Independently parsed and applied each original unified patch in memory, with unique exact context and verified hunk lengths, without changing source. R1 alone reproduces its real component main; M2 alone reproduces its real component main. R1-then-M2 and M2-then-R1 both reproduce the actual combined bytes. Regenerating the baseline-to-combined diff reproduces COMBINED.patch2520bytes/`02273bde...` exactly. Therefore no M1/L1 or other source change is included.

The active canonical body remains byte-identical to baseline. Every remaining canonical invocation retains its original acquire; there is no cross-call inactive cache or new TLS/owner lifetime rule. Local cold-helper record release finishes before returning to M2's data capture. M2 retains original capacity containment before size, null-safe pointer arithmetic, live-size traversal, child early return and unqualified ADL recursion. Exact union leaves both public owns directions/short-circuit order and all string, array, reflection, move/retire and error paths otherwise unchanged.

No new interaction conflict was found under the previously reviewed live-owner and valid-container/lifetime contract. Holding a current-node pointer is not a mapping pin; ADL may enter user code and does not gain a generic no-callback guarantee. Original range traversal already held its begin/end pointers across child recursion. R1/M2 individual speed evidence cannot be summed or transferred to this combination.

Only source, patch and small metadata were read. No tools/full build was changed and no compiler/native/queue or benchmark-body hash ran. Exact evidence: `COMBINED_SOURCE_ACCEPTANCE.json`. Reviewer returns idle after reporting.
