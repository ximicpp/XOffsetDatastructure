# F64 host source-only handoff

Status: ready for independent source review; **no F freeze, host action or timing admission**. This work makes no claim that Q's original audit or root engineering acceptance has passed. It does not wait for or replace those decisions.

## Fixed source identities

| Source | Bytes | SHA-256 |
| --- | ---: | --- |
| `formal_host_v1/execute.py` | 19409 | `5ead5e942d97698196a4e1a2b99799f3f44e19957c51073dc41e4062d0af9a6c` |
| `formal_host_v1/process_pause.py` | 16289 | `b5c4d5890309bab3bdd68e5e271b0c9978834741760dbce55435ce3dd78549b8` |
| `formal_host_v1/pause-control.ps1` | 12670 | `0898a5f0e1be74368be71161be970db6e85ae63c4782df8638aa705f471e5465` |

The source is derived directly from `validation/q32_host_v1/`. `F64_HOST_DERIVATION_v1.patch` contains the line diff; `F64_HOST_DERIVATION_v1.json` contains all UTF-8 byte replacements with old/new byte offsets and source hashes. Replaying those byte edits reconstructs all three derived files exactly. Both Python sources were AST-parsed without importing or executing them. Only execute.py's `pause_module`, `prerequisites`, `prelude`, `main` ASTs change; all other functions are identical.

## Delta and retained behavior

- Qualification labels and directories become formal, with distinct controller, host record, pause state, power state, frozen source, output, prelude and timing queue paths. New root F64 review/host/quiet/once-ledger paths are requirements only; none was written.
- Original formal whole window is 5400 seconds; the process guard is 5520 = 5400 + 60 setup + 60 restore. Both actual pre-dispatch conservative remaining checks require at least 5460. W1 prelude remains 60/wait0 and exits before direct original W3 timing5400/wait0.
- Original bound `PowerWindow` source/helper remains untouched, automatically using 5400 + 60 = 5460 seconds and its existing 7200 upper bound. Launcher only reads power; original helper owns temporary power and restoration. Owned process resume, held handles, identity/run-token checks, startup recovery, watchdog and outer finally remain the original mechanisms. Audit follows actual restoration.
- No actual F manifest exists in this work. Launcher accepts its SHA through the existing required CLI field and checks it against distinct F manifest plus exact root review/host/quiet/request identities. This avoids editing reviewed source after a genuine freeze; a placeholder, copied Q pin, or draft request cannot satisfy admission.
- The only added prerequisite block closes the original formal run's root-record gap: root REVIEW must pin current `COORD/qualification-acceptance.json`, the exact same ref must occur in the final timing request, and F's frozen acceptance copy must have identical bytes. Its three pinned evidence refs must name this Q's original audit and queue request/result, with postrun pin `b3db8adafebb4079f65f7557e00917ac108a71184dfcb93ed7bfc2df992449ce`. Original Q audit must actually report complete/qualified/pass,4656 actual and2328 primary children, no budget violations, exact Q queue-result identity. The root writes/accepts this record only after complete original audit and engineering acceptance; this author wrote only its schema.

## Exact draft interfaces

`F64_TIMING_REQUEST_DRAFT_v1.json`: direct unchanged bound entry run/formal and original supervisor5400;9264 actual=4632 primary+4632 conditioning;24 primary warmups,4608 inference,64 macrocycles;20-second native cap,retry0,capture0,compile0. All F freeze/acceptance identities are null/placeholders and status is deliberately non-executable. Materialize a separate actual request after independent review and actual accepted freeze; preserve this draft.

`QUALIFICATION_ACCEPTANCE_SCHEMA_DRAFT_v1.json`: actual maintained `require_qualified_q` schema. Identity bytes are decimal **strings**, as required by its validator. The original formal freeze performs complete Q audit/queue/manifest/postpin/current-tools checks and copies root Q acceptance. The original formal run does not reread root records. The admitted external launcher and root exact request/record pins are therefore mandatory; calling original run directly is outside this preparation's admission boundary.

`F64_ROOT_FIELD_REQUIREMENTS_DRAFT_v1.json`: lists exact REVIEW/HOST_REVIEW/GLOBAL_QUIET/ledger fields and launcher CLI template, with independent F paths. It contains no root signature, fresh quiet assertion, PID plan, power fact or passed admission.

`F64_ORIGINAL_AUDIT_INTERFACE_DRAFT_v1.json`: unchanged original audit/formal CLI and separate original W3 functional3600/wait0 queue. Actual F postrun pin and manifest must be supplied only after a sealed F run and restoration. No new audit wrapper, math, sample or diagnostic.

Original bound eleven source files and original Q32 host three files were hashed before and after and are unchanged. Formal phase math, original protocol, statistics, thresholds, schedule and native/child modes are untouched; formal still needs its original30AA and12 primary contrasts. This is source preparation, not Q/F acceptance or a performance result.

Executions by this task: native0,compiler0,capture0,queue0,freeze0,host probe0,pause0,power0. Root records and actual qualification-acceptance.json written:0. Next: independent source review; root/validator materialization only after root's actual Q acceptance. Core is idle after this handoff.
