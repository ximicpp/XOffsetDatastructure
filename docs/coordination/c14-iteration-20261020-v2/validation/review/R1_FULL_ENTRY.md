# R1 full normal/SAN execution entry

The normal/SAN entry is source-ready and frozen at16059 bytes,
SHA256 `44b7bc6480e6467432d1d0a1a43039c4a5410ce991e2564fa5c70217221902eb`.
Author ran AST and read-only hash/inventory checks only; compiler/native/queue
execution count is0. Actual passes must come from the root executor's new jobs.

From outside any lease:

```sh
H=/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/tools/c14_iteration_20261020_v2/validation/r1_full_job.py
/usr/bin/python3 -I -B "$H" check normal
/usr/bin/python3 -I -B "$H" run normal
/usr/bin/python3 -I -B "$H" run sanitized
```

Root creates a distinct fresh actual host observation before each mode:
`validation/windows-r1-full-normal.json` and
`validation/windows-r1-full-sanitized.json`. Observations must be≤900s old, with
backing and guest reserves each≥8GiB. The old shared resource record referenced
by B/M2 must remain immutable. This entry controls no user process or power state.

Each mode retains2400s and the original full73 CTest inventory with120s per case,
separate configure3, actual P2996/libc++/SAN policy, dependency lock and original
assertions. The wrapper calls reviewed `full_entry.execute()` under one original
global lease. It never wraps a dispatching coordinator inside another lease.

The actual R1 source remains the independently frozen154-member candidate,
main `b4c61b2d286d6d4ffdfbd44af3ff7151b1640dd07f8b9a2c9bfcfa01a6a9c23f`.
The immutable inner recipe is SHA `2d963e6a…`; final outer v2 is
`68bec6b9…`. All75 actual tools and actual source154/14 match. Actual prior R1
B10 and same-source codegen are required. Phase C is independent and does not
block these functional checks. M1 results are never substituted for R1.

The new entry additionally captures the exact61-byte `clang++-libcxx` forwarding
wrapper before and after each new job, along with actual compiler/runtime/SAN,
all fixed tool IDs and selected dependency-provider header/license IDs. It does
not retroactively claim this observation existed for completed R1 B or M2 jobs.
The13 old SHA-only runtime declarations are accepted by their schema; admission
captures actual byte size+SHA for the new job's complete postcheck.

Worker proof includes actual flock-busy on the original DEFAULT queue inode,
its own out/run ID, owner/kind, exact admission-bound command, actual `/proc`
self/supervisor argv, cwd, runner hash and2400s cap. Final checks require actual
full inventory/JUnit73, exact request/receipt/compile database, all154 staged
files, all copied dependency files, actual tools and empty queue cleanup.

Standard artifacts remain compatible with the frozen core business bridge:

```text
/root/xoffset-c14-iteration/20261020/v2/validation/r1-full-v1-normal/
/root/xoffset-c14-iteration/20261020/v2/validation/r1-full-v1-sanitized/
/root/xoffset-c14-iteration/20261020/v2/validation/r1-full-v1-{mode}-lease/
validation/r1_full/R1_FULL_{NORMAL,SANITIZED}_REQUEST_v1.json
validation/r1_full/R1_FULL_SOURCE_BINDING.json
validation/r1_full/R1_FULL_RESULT.json
validation/review/R1_FULL_ENTRY_{NORMAL,SANITIZED}_{ADMISSION,DISPATCH,WORKER,RESULT}.json
```

The standard full summary is written only after both new actual R1 modes pass.
The entry starts no business program, diagnostic, timing sample or screen.
Business/V1/consumers/lifecycle follow through a separate reviewed entry; the
running full tool must remain unchanged.
