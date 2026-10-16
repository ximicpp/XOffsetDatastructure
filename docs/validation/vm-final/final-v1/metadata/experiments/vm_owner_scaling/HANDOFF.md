# Fixed-request owner scaling — W1 handoff

D018 authorizes the isolated implementation and bounded functional/diagnostic work. D022 corrects the native Apple arm64 checker layout to alignment8/stride24. Both decisions are frozen under `docs/coordination/vm-followup/contracts/W1-owner-scaling-v1/`.

Implemented: stable `XBufferCore` + uint64 allocator, fixed owner i → worker i%T, six owner/thread conditions, four separately clocked F phases, untimed semantic/geometry/conservation checks, complete worker cleanup and join. Diagnostic R256 and F R256/R8192 use the same unchanged harness; no production, W2 runtime, W3 or network files changed.

Completed exactly26 native workload children:18 success +8 precise expected failures. The original first child succeeded but its collector rejected the draft stride formula. That historical failed queue/claimed attempt remain unchanged; D022 corrected auditor separately re-audited it offline, then only25 remaining unique cases ran. No first-case retry or post-D022 recompile. Three original build failures and one zero-native Python preflight failure are retained.

Results: `docs/VM_OWNER_SCALING_RESULTS.md`. Frozen evidence: `docs/validation/vm-owner-scaling/final-v1/`. Archive: `docs/validation/vm-owner-scaling/owner-scaling-v1.tar.gz`, SHA256 `0f0593a92ba738c919c9fb5f82bcdf8b41735993e715a884faf616ee441852c0`. It contains462 regular files including the manifest. Archive SHA and manifest identity are in `owner-scaling-v1.tar.seal.json`.

Original immutable build: `build/vm-owner-scaling/build-v4/` (also copied into evidence). Timing binary SHA256 `2ff7b74f1561276c151a4b9323735d06eb9a65f87513317f1000ff3dfe703b45`; diagnostic `a9c176625111d668b501bf6b7a95d44352a27e4380baaa1e171031b001617f8f`. Actual include closure has1018/1019 files; F and generated diagnostic headers cannot shadow each other. `/usr/local/bin/clang++` driver spelling and original toolchain are preserved.

Frozen corrected tools: `tooling-errata-v2/`; supplemental first-case proof: `recovery-v2/`. Full audit verifies26 cases/claims,18 successes/8 exact failures, complete lifecycle/metadata/registry checks and three rejected bad copies.13 synthetic tests pass in both Python modes. Maximum actual process events5135 < hard5631; final owner registry empty; only one exact shared bucket remains.

All C++ builds/runs and bulk packaging used the accepted default POSIX queue. Functional wall values have `timing_evidence=false`; they do not establish lookup latency or performance benefit. Formal timing remains W3-only and unexecuted here. Candidate implementation and production edits remain unauthorized.

D027 accepts the scoped build and all26 native functionality/diagnostic cases. Independent archive/relocation review passed: `docs/validation/vm-owner-scaling/independent-review.json`, SHA256 `a9393f54da27684ad6cc09968b53f64b538017cfba25dc475921b36eb21648c3`. All462 files and459 copied originals match; relocated frozen audit reproduces the full result without C++ execution. Owned evidence at review is205647683 bytes, below512MiB. No work remains in this W1 delivery scope. Original network and candidate-static evidence stay frozen.
