# Formal acceptance harness, XOffset part

The XOffset side of the R3 formal acceptance harness (Mac tree
`meta-r3/experiments/serialization_comparison`): a fixed-schema mutable
snapshot is loaded, queried, mutated, emitted, retired, reloaded, queried and
destroyed, with one timer per phase. The competitor adapters (cista,
FlatBuffers, Cap'n Proto, cereal), the campaign tools and `formal_r3.py` stay on
the Mac; formal timing conclusions come only from the Mac batch.

Origin and per-file sha256 are in `MANIFEST` (`python3 bench/check_manifest.py`).

## Build

```
cmake -S bench/formal -B build/formal -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
cmake --build build/formal && ctest --test-dir build/formal
```

`SC_XOFFSET_SOURCE` defaults to this repository. GCC 16 or P2996 Clang; every
translation unit is GNU++26 and the driver is `-O3 -DNDEBUG`, as in R3.

## Fixtures

```
python3 bench/formal/scripts/make_fixtures.py --out build/fixtures          # S and M
python3 bench/formal/scripts/smoke.py --driver build/formal/bin/sc_xoffset --fixtures build/fixtures
```

`model/reference.py` (sha256 2ce49fc6…) is the independent Python generator.
The script fails unless S equals `fixtures/golden-S` byte for byte and M equals
the Mac `prepared-v1` M fixture (`fixtures/prepared-v1-M/fixture.json` lists
each file's sha256). M takes about 30 seconds.

`smoke.py` encodes each scale's input wire, runs `validate` (every stage digest,
both query sets, emit and reload) and one `run`. For M the input wire must be
the pinned formal input wire: `89fd02d2…49d2edcc` (8,649,368 bytes) since wire
format 2.1 (2026-10-27); it was the formal candidate wire `2bf0c071…a25c2a4`
from the R3 FREEZE.json under format 2.0. A library change that alters the
wire fails here; whether that is acceptable is susu's call, the smoke test only
makes it explicit.

## Instruction counts (CI performance gate)

`-DSC_IR_COUNT=ON` builds a driver for callgrind: under
`valgrind --tool=callgrind --collect-atstart=no`, `run` collects only the phase
named by `SC_IR_PHASE` (load, query_before, mutate, emit_after, retire, reload,
query_after, cleanup). Without the option the timed region is unchanged.
