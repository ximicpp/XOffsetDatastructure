# Fuzz regression inputs

Inputs that once made a fuzz harness fail. Every CI job replays them through
the harness built without libFuzzer, with that job's compiler and sanitizers
(P2996 Clang, GCC 16, ASan+UBSan, Linux arm64, Windows):

| Directory | Harness | CTest |
| --- | --- | --- |
| `v2_wire/` | `fuzz_v2_wire.cpp` (one V2 wire per file) | `fuzz_v2_wire_replay` |
| `wire_schemas/` | `fuzz_wire_schemas.cpp` (first byte picks the schema, the rest is the wire) | `fuzz_wire_schemas_replay` |

Each file is the raw harness input, exactly as libFuzzer saved it (after
`-minimize_crash`). Name it by the crash signature from the nightly report, so
the nightly `fuzz-crash/<signature>` branch and its issue point at it.

Adding one: the PR that fixes the crash commits the input here, and its CI
replays it. Inputs are never removed. Dot files are skipped (`.gitkeep` keeps
the empty directories).

Replaying by hand:

```bash
build/bin/Debug/fuzz_v2_wire_replay tests/fuzz/regressions/v2_wire       # a directory
build/bin/Debug/fuzz_v2_wire_replay crash-0123abcd                        # or single files
```

A differential failure of `test_v2_fuzz` (GCC 16 nightly) reports
`test_v2_fuzz --replay <seed> <case>`; with `XOFFSET_FUZZ_DUMP_DIR` set it also
writes the failing input wire, which can go into `v2_wire/` when the failure
happens before the post-accept mutation step. A failure after that step may
not reproduce from the wire alone, because the harness derives its mutation
stream from the input bytes; keep the `--replay` command in the issue.
