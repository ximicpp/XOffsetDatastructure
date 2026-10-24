# Map state demo

A minimal two-dimensional map with moving units that exercises the two core
XOffset paths with real processes and real TCP:

1. **Server to server**: server A owns the full unit state and sends the whole
   `UnitStateBatch` to server B every simulation step; B verifies it, advances
   one step **in the received arena** (positions, hit points, behavior, names,
   path arrays), saves it and returns it; A verifies the result and swaps it in
   as its new state.
2. **Server to client**: a C++ client asks A for its viewport; A filters the
   current batch, builds a `ViewSnapshot`, saves and sends it; the client
   verifies it and draws by reading fields **directly from the live, verified
   snapshot buffer**.

Every process takes `--buffers reuse|fresh`. `reuse` (the default) keeps one
buffer per role and refills it in place (`reload_verified`, `reset`,
`save_verified_into`), the way a long-running service would; `fresh` makes a
new `XBuffer` per message and destroys it after use. Both run the same
validator and send the same bytes: every step's request and result wire digest
is logged, and `summarize.py` fails unless the two modes agree tick for tick.
The same digests compared across runs show the same bytes from another
compiler or platform (`MAPDEMO_COMPARE` below).

The per-step full-batch round trip is a validation workload, not a recommended
game architecture. Contracts: [`INTERFACES.md`](INTERFACES.md) (file ownership,
command lines, protocol, CSV formats) and the design notes in the project
folder (`slg-demo-20261022/MAP_DEMO_DESIGN_20261022.md`). Results:
[`RESULT.md`](RESULT.md).

## Layout

| Path | Contents |
| --- | --- |
| `include/mapdemo/schema.hpp` | XOffset types (`UnitStateBatch`, `ViewSnapshot`, `MapCatalog`, ...), receive caps, no-padding asserts |
| `include/mapdemo/std_model.hpp` | the same fields in std types; reference model for checks only |
| `include/mapdemo/rules.hpp`, `rules_impl.hpp` | deterministic generation and step rules, one template for both models |
| `include/mapdemo/net.hpp`, `src/net.cpp` | 24-byte frame header, per-type length caps checked before allocation, handshake, payload recycling |
| `include/mapdemo/buffer_mode.hpp` | the `--buffers` option (`reuse`, `fresh`) |
| `include/mapdemo/metrics.hpp`, `src/metrics.cpp` | samples, mean / p50 / p95 / max, CSV |
| `include/mapdemo/checks.hpp`, `src/checks.cpp` | invariants, reference comparison, snapshot comparison, snapshot and wire digests |
| `src/server_a.cpp`, `src/server_b.cpp` | A (single-threaded `poll` loop) and B (blocking step loop) |
| `src/client_core.*`, `src/client_main.cpp`, `src/client_gui.cpp` | client network, snapshot holder, headless sampling, `--evil`, raylib window |
| `src/selftest.cpp` | allocation / free / reuse across save + load and reload, both buffer modes byte for byte, independent output, failure inputs, rect rule |
| `tools/run_tier.sh`, `tools/run_all_tiers.sh`, `tools/summarize.py` | one tier, all tiers + evil in both modes, result tables, cross-run digest comparison |

## Build

The demo is off by default; it needs the same toolchain as the library.

**Linux, GCC 16 (Docker; the project's local validation)**

```bash
git submodule update --init --recursive        # or the boost libs listed in the design notes
python3 tools/prepare_vm_cmake.py --platform linux-x86_64 --out build/cmake-tool
docker run --rm -v "$PWD:/w" -w /w -e PATH=/w/build/cmake-tool/root/bin:/usr/local/bin:/usr/bin:/bin \
  gcc@sha256:27470de79376e1033091a1f2cbfa805a10d790dcbd980389a07c4d6dc3606240 bash -c '
    cmake -S . -B build/mapdemo -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ \
          -DXOFFSET_BUILD_DEMOS=ON -DXOFFSET_BUILD_TESTS=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF &&
    cmake --build build/mapdemo -j4 &&
    demo/mapstate/tools/run_all_tiers.sh build/mapdemo/bin/Release build/mapdemo-results'
```

Binaries need the image's libstdc++, so run them inside the container. For
ASan+UBSan use a Debug build with
`-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer"`,
`-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined`, run with
`UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1` (every UBSan check on). The project does not use GitHub CI; the
`mapdemo-gcc16` job left in `.github/workflows/ci.yml` is not run.

**macOS, clang-p2996 (window client)**

The window client uses raylib 5.5 (commit
`c1ab645ca298a2801097931d1079b10ff7eb9df8`), built once as a static C library
with the system clang and found through `CMAKE_PREFIX_PATH`. Without raylib,
`mapdemo_client` is built `--headless` only and CMake says so.

```bash
git clone --depth 1 -b 5.5 https://github.com/raysan5/raylib.git deps/raylib-5.5-src
cmake -S deps/raylib-5.5-src -B deps/raylib-5.5-build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=/usr/bin/clang -DBUILD_EXAMPLES=OFF -DBUILD_SHARED_LIBS=OFF
cmake --build deps/raylib-5.5-build -j && cmake --install deps/raylib-5.5-build --prefix deps/raylib-5.5

cmake -S . -B build-demo -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=<p2996>/bin/clang++ \
      -DXOFFSET_BUILD_DEMOS=ON -DXOFFSET_BUILD_TESTS=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF \
      -DCMAKE_PREFIX_PATH=$PWD/deps/raylib-5.5
cmake --build build-demo -j
```

All demo code is compiled with `-ffp-contract=off` (B's real state and A's
reference model are compared bit for bit); never add `-ffast-math`.

## Run

Interactive demo, three terminals (start order B, A, client; connections retry
for 5 s):

```bash
build-demo/bin/Release/mapdemo_b
build-demo/bin/Release/mapdemo_a --units 10000          # checks on by default; --no-check skips the reference model
build-demo/bin/Release/mapdemo_client                   # left-drag pans; hover a unit for name / hp
```

Add `--buffers fresh` to any of them to make a new buffer per message.

Automatic checks and the three tiers (no window, same types and network path):

```bash
demo/mapstate/tools/run_all_tiers.sh build-demo/bin/Release mapdemo-results
```

This runs `mapdemo_selftest`, then 1, 100 and 10000 units x 500 steps (seed 1,
A and the headless client with `--check`) in both buffer modes, then the
`--evil` online failure-input run (100 units, 200 steps) in both modes, each on
its own port pair, and finally `tools/summarize.py`. Output:
`mapdemo-results/RESULT_TABLES.md`, `summary.json`, and per run `selftest/`,
`tier_<N>_<mode>/`, `evil_<mode>/` with the CSV files of INTERFACES.md
section 6, process logs, `exit_codes.txt` and `meta.txt` (host, commit, buffer
mode, load average before and after). Exit status 0 means every process exited
0 and every verdict passed. Variables: `MAPDEMO_TIERS`, `MAPDEMO_STEPS`,
`MAPDEMO_MODES` (default `fresh reuse`), `MAPDEMO_PORT_BASE`,
`MAPDEMO_SKIP_EVIL`, `MAPDEMO_LABEL`, `MAPDEMO_COMPARE`. One tier alone:
`MAPDEMO_BUFFERS=reuse tools/run_tier.sh BIN_DIR UNITS OUT_DIR [STEPS]`.

Same bytes on another compiler or platform: copy one results directory to the
other machine and run there with `MAPDEMO_COMPARE=<that directory>` (or call
`summarize.py ROOT --compare OTHER_ROOT`). Every tier of this run must then
have the same request and result digest as the other run's tier of the same
unit count at every common tick, over at least 500 ticks (the rules and seed are fixed, and the demo is
built with `-ffp-contract=off`, so the bytes are expected to be identical).

`summarize.py` verdicts per tier: exit codes; at least 500 steps with
consecutive ticks, every step check passing and a wire digest on every step
row; every row of `a_checks.csv` and `client_checks.csv` passing (A's six
fixed rows present); A's and the client's snapshot logs identical per request
id (tick, category, visible count, wire bytes, digest); normal, full-scene and
empty requests all sampled, empty ones with 0 units and full-scene ones with
every unit; every metric name of INTERFACES.md section 6 present (no destroy
metrics in `reuse`). Across tiers: `fresh` and `reuse` with the same unit
count send identical step wires over at least 500 common ticks; with
`--compare`, every tier matches the other run. Self-test: exit 0, every row
passing. Evil: all three processes exit 0, A logged at least four `rejected:`
lines and every row of the evil client's `client_checks.csv` passes.

## Deterministic generation (design section 3)

Hash `h(seed, id, salt)` = splitmix64. One scene (id 1, "Demo Plains",
`[0,4096)^2`), 16 sites on a jittered 4x4 grid. Unit ids `0..N-1`; start point
uniform in `[0,4095]^2`; 4 integer path points within +-256, looped; speed
`20 + h % 61` scene units per second (1 to 4 per 0.05 s step); short name
`U%05u`, long name = short name + a fixed marker (always out of line); hp 100,
behavior `Patrol`. Every 20 ticks, units with `id % 100 == 0` get a new hp in
50..100, rotate behavior, toggle long / short name, and grow or shrink the
path by one point between 4 and 8. Rects are min-inclusive, max-exclusive.

## What the demo does not claim

Structural validation is not authentication or tamper-proofing. No
`XBufferPool`, compression, incremental sync, LOD or gameplay; one arena per
batch, per snapshot and for the catalog (kept and refilled in `reuse`, new per
message in `fresh`). Cloud (GCC 16, x86-64 VM) numbers are smoke
numbers; formal timing is the Mac run recorded in `RESULT.md`.
