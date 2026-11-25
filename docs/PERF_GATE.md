# Performance regression gate (instruction counts, L0)

**Instruction counts are used only to catch regressions.** No claim of "faster" may use the numbers
here as evidence; published speed numbers are in [PERFORMANCE.md](PERFORMANCE.md).

**Merge rules**: all verdicts happen in the cloud. Class S and class L PRs pass the GCC 16 and P2996 Ir
gates before merging (section 3), then `tools/local_validate.sh` runs on the merge result; class L also
attaches a cache-sim comparison (section 5a, warning only). Mac batches only review after merges and
releases and never block a merge. Where the text below says "run Mac L2 before merging", attach the
function diff table and the cache-sim comparison instead.

**Tiered validation**: `tools/local_validate.sh` runs the PR tier by default
(`--tier pr`); the merge evidence is every step of that tier passing, with the two Ir gates judged per
section 3:

| Tier | Used for | Steps |
| --- | --- | --- |
| `pr` (default) | PRs that change the library (class S and L, and class 0 PRs that change library sources) | meta, gcc, asan, fuzz, diff, smoke, ir, p2996, p2996-ir, asm, plus diff-c++20 and one C++20 client column (client-c++20, MSVC first) |
| `quick` | PRs that do not touch the library (only bench/, tools/, tests/, docs/) | meta, gcc, smoke, p2996; refuses to run when top-level sources, CMakeLists.txt or cmake/ differ from the base |
| `daily` | Once a day, on the latest commit of `main` | tsan, demo (the map state demo end to end), the whole C++20/C++23 matrix, all client columns, wire-matrix (the parts the `pr` tier leaves out) |
| `release` (= `--release`) | Release candidates | All steps |

**Stacked PRs**: in a chain of dependent PRs (each one based on the one below
it), only the top PR runs the full `pr` tier, against `main` (on the merge result, as for any PR); that run
covers the whole chain. Every PR of the chain, the top included, also runs
`tools/local_validate.sh --steps "ir p2996-ir asm" --base <its parent>` (the PR below it, or `main` for the
lowest), so each PR keeps its own Ir verdict and class. Once all of them pass, merge from the bottom up. When
a PR of the chain changes, rerun these steps for every PR whose head or base changed, and the top's full tier.

Failures found by `daily` get a fix PR the same day, and nothing is released until they are fixed.
GitHub CI is off; `perf-gate.yml` is kept but the gate runs through the local validation script
(section 6).

## 1. What it does

`.github/workflows/perf-gate.yml` runs on PRs targeting `main` (ignoring PRs that only change `docs/**`
and `*.md`), on pushes to `main` and on manual triggers, with one job each for GCC 16 and P2996 Clang,
both judged:

1. Export three copies of the library code in the same job: **anchor** (`bench/perf/ANCHOR`), **base**
   (the merge-base for a PR, the first parent for a push) and **head**.
2. All three are compiled into `slg_bench`, `sc_xoffset` and `reloc_bench` with **head's bench code**
   (for manual replays, the bench code of the chosen branch) (`bench/slg/`, `bench/formal/`,
   `bench/reloc/`). When base or anchor does not compile with head's bench (the public API changed), it
   uses its own version's bench instead, marked "different bench" with a warning; if that still does not
   compile it is marked "not comparable" and the job does not fail; before this PR merges, the PR body
   must explain it and attach a cache-sim comparison (section 5a), and the post-merge Mac review must
   cover it. `reloc_bench` is optional: when base or anchor cannot compile it, only the `reloc.*`
   comparisons are missing (warning); when head cannot compile it, the build fails. `family_ir`
   (`bench/evolution/family_ir.cpp`) is optional the same way: a base or anchor tree without `XSpare` and
   `XOFFSET_SCHEMA_FAMILY` only loses `family.*`.
3. Correctness is checked before counting: SLG `--mode verify --scale S` (xoffset agrees with cereal,
   once with the reuse protocol and once with the per-message protocol); the formal harness generates S
   and M input wires with its own `encode` and `validate`s S and M; `reloc_bench --mode verify` (each
   side's result digest agrees per round, with the item names); `family_ir --check` (both rows of each
   pair give the same wire, the same reloaded state and the same replica). If head fails any of them, the
   gate fails right away with no numbers.
4. Count instructions (Ir) per stage under valgrind callgrind with `--collect-atstart=no`, turning
   collection on only inside the measured stage. One process per stage, `nproc` in parallel.
   - Every process carries
     `GLIBC_TUNABLES=glibc.cpu.x86_rep_movsb_threshold=0x7fffffffffffffff:glibc.cpu.x86_rep_stosb_threshold=0x7fffffffffffffff`,
     turning off glibc memcpy's rep movsb path and memset's rep stosb path.
   - Why: valgrind counts each iteration of `rep movsb` / `rep stosb` as one instruction, so copying or
     zeroing on that path costs about one Ir per byte; the same bytes through a vector loop take a few
     dozen times fewer.
   - A memcpy example: #83 split a 12 MiB output into 256 KiB chunks, and the Ir of formal.emit_after
     became 5.96x although the actual work did not change. With this path off, memcpy's Ir follows the
     bytes copied, and #83 is 1.058x.
   - A memset example: glibc sends memsets ≥ 2 KiB (the default `x86_rep_stosb_threshold`) through rep
     stosb. A prototype merging several small zeroings into one 3 KiB zeroing measured A.simulate at
     1.416x with only movsb off (memset 617k Ir), and 1.0003x with both off (memset 57k Ir)
     (`A_SIMULATE_REGROW_20261026.md` section 4).
   - The measure changed from movsb alone to both on 2026-10-26 (PR: ledger `record_only` entry
     `ir-env-stosb`). Absolute Ir in earlier gate JSON, calibration tables and the ledger use the old
     measure and cannot be compared directly with later absolute values; head÷base ratios differ only when
     a change alters the number or size of memsets ≥ 2 KiB; re-judgments of existing gate pairs were
     recorded in the 2026-10-26 Ir calibration archive.
5. One SLG stage of head, formal mutate and the first reloc item each run once more, and the two Ir
   counts must be bit-identical, or the gate fails (results not trustworthy).
6. Judge by the thresholds of section 3: one table in the job summary and a JSON artifact
   `perf-gate-<compiler>-<head sha>.json` (90 days). No PR comments.
7. For every metric with a warning, a failure or a ledger exemption, list a function diff table from the
   same callgrind output (see section 5).

No stored baselines and no nightly runs are needed: Ir is deterministic, all three copies are built and
measured fresh every time, and changes in the runner image or valgrind version cancel out; Ir does not
change when the code does not, and the push results already cover every merge.

## 2. What it measures

| Source | Settings | Metric names |
| --- | --- | --- |
| SLG | S scale, seed 20261021, `SLG_REUSE=1` (measure c), 8 cycles, collecting only cycles 4–7 (the first cycles are still warming up: in scenario A of d2d9bba the per-cycle Ir of cycles 4–7 is only 74% of cycles 1–2; the formal Mac SLG measures steady state) | Each timed stage `slg.<key>` (such as `slg.A.apply`, `slg.S.ticks_middle`); scenario totals `slg.A`…`slg.D` = the sum of that scenario's stages |
| Formal harness | M fixture (`bench/formal/model/reference.py --scale M`, default seed) | `formal.load`, `formal.query_before`, `formal.mutate`, `formal.emit_after`, `formal.retire`, `formal.reload`, `formal.query_after`, `formal.cleanup`; `formal.read_sum` = query_before + query_after; `formal.total` report only |
| SLG demo case set, trusted tier (hole table items, 4.2.0) | S scale, seed 20261021, `SLG_REUSE=1 SLG_CASES=demo SLG_NATIVE_OFF=1 SLG_XOFFSET_CAP_MIB=16`, `--formats xoffset_trusted`; steady state: 15 cycles, collecting only S.store_recv in cycle 15 (by then a stored player carries about 27 holes); whole run: 21 cycles, every instruction of the process (`--collect-atstart=yes`: setup, timed and untimed stages) | `slgd.S.store_recv` (the steady-state receive of a stored player); `slgd.whole` (the whole run) |
| Relocation tax (G9) | `bench/reloc`, one process per item: `--rounds 5 --warmup 1 --cases <item>`, `RELOC_IR_SKIP_ROUNDS=2`, collecting only XOffset's timed region (rounds 2–4) | Each item `reloc.<item>` (such as `reloc.vec.push_back`, `reloc.map.insert.fresh`), including the report-only `*.reused`, `.plain` and node container comparison items |
| Add-only type evolution ([SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md) sections 1 to 4) | `bench/evolution/family_ir.cpp`: the SLG PlayerState with reserved bytes in two versions of one family, one process per row, `--row <row> 20` (20 measured calls after one warm-up, collecting only inside them) | `family.send.same`, `family.recv.same`, `family.delta.same` (a v2 server and a v2 client) and the `.in_family` row of each (the same server and a v1 client) |

SLG builds only XOffset + cereal (cereal v1.3.2, `ebef1e92`, headers). M-scale SLG exceeds the address
space under valgrind (512 × 256 MiB reservations), so S is used. The formal harness's M input wire is
encoded by each binary itself and its sha256 recorded; when it differs from anchor the summary says "M
input wire changed", and wire format changes require maintainer approval.

## 3. Thresholds

| Subject | Warning | Failure |
| --- | --- | --- |
| Totals: `slg.A`–`slg.D`, `formal.read_sum`, `mutate`, `emit_after`, `load`, `reload`; head÷base | > 1.003 | > 1.010 |
| A single stage with ≥ 5% of its scenario's Ir (`formal.total` for formal); head÷base | > 1.020 | > 1.050 |
| Stages under 5%, `formal.retire`, `formal.cleanup` | Report only (JSON) | — |
| Totals: head÷anchor, after deducting what the ledger exempts | > 1.010 | > 1.020 |
| Any total head÷base < 0.97 | Marked "instruction count dropped", with a reminder that Mac data is needed to claim it is faster | — |

`slgd.whole` is judged as a total and `slgd.S.store_recv` as a scenario of its own (stage thresholds). The
whole run is there so that work moved out of a timed stage still counts: a hole table index built later
(at the first allocation that needs it) instead of during the receive shows up in it at full cost.

The share is computed on whichever of base and head is larger. `slg.S.ticks_middle` (the middle-erase
stress item) is a scenario of its own and judged by the stage thresholds. Each `reloc.<item>` is also a
scenario of its own, judged by the stage thresholds (> 1.02 warning, > 1.05 failure); G9's own verdict
(the UB of XOffset ÷ std ≤ 1.05) comes from the Mac batches, and the gate only catches instruction count
regressions on the XOffset side.

Each `family.*` row is a scenario of its own as well (stage thresholds against base and anchor). On top of
that, in the head every `family.<op>.in_family` row must **equal** its `family.<op>.same` row, to the
instruction: a send, receive or delta between two versions of one family is the same work as between two
programs of one version. Any difference fails the gate, and the ledger cannot exempt it.

## 4. Ledger (intentional regressions)

`bench/perf/ledger.json`:

```json
{
  "schema": "xoffset-perf-ledger-v1",
  "entries": [
    {
      "id": "pr26-mutate",
      "pr": 26,
      "metric": "formal.mutate",
      "compiler": "*",
      "max_ratio": 1.03,
      "reason": "trades output -23% (emit_after) for 2.4% more instructions in the mutate stage",
      "evidence": "m3v2_RESULT.md; Mac mutate 1.038",
      "approved_by": "maintainer (review link, date)"
    }
  ]
}
```

- `metric` may use wildcards (`slg.A.*`), `compiler` is `*`, `gcc16` or `p2996`, and `max_ratio` > 1.
  All fields are required, and `id`s must be unique.
- Entries with `"record_only": true` are records only (for example gains Ir cannot see); they exempt
  nothing and need no maintainer approval; `max_ratio` holds the ratio observed at the time.
- **head÷base**: only entries added by this PR (`id`s not in the base's ledger) can exempt. A metric above
  the failure threshold but not above `max_ratio` is marked "exempted" and the summary lists the entry;
  above `max_ratio` it fails as usual.
- **head÷anchor**: the product of the `max_ratio`s of all matching entries in head's ledger is deducted as
  the exempted part before comparing with the thresholds.
- **Approval**: entries with scenario totals (`slg.A`–`slg.D`, `formal.*`) ≤ 2% are approved in
  performance review and recorded in `approved_by`; above 2%, or when a security/performance trade-off is
  involved, explicit maintainer confirmation is required (`tools/perf_gate/perf_gate.py` checks the
  maintainer handle in `approved_by`), otherwise the gate fails outright.
- **Inlining changes**: for a regression whose function diff table is marked "suspected inlining
  change", first try moving the new cold path (error branches, throwing, rare cases) into a
  `[[gnu::noinline]]` function so the hot function keeps its previous inlining; write an entry only if it
  still regresses after the move, attaching this function diff table (the summary's table or the JSON's
  `functions`) and the attempted change in `evidence`.
- **Moving the anchor**: after a release (the tag is pushed), open a PR that only changes
  `bench/perf/ANCHOR` (to the tag's commit), and at the same time move old entries to
  `bench/perf/ledger_archive.json` (the gate does not read it; stale entries also move there, for example
  `reloc-bench-added` moved in when the gate started measuring `reloc.*`).

## 5. Reading the results

- Job summary: the first line is always the bold sentence above; then the verdict, the list of
  failures/warnings/notes, a table of judged subjects and stages over threshold, and each scenario's
  stages that changed most.
- Artifacts: `perf-gate-<compiler>-<sha>.json` (all rows, including report-only stages), `plan.json`,
  `build-*.json`, `ir-*.json`, build logs.
- Function diff table: for every metric with a warning, a failure or an "exempted" mark, the 10 functions
  whose self Ir (that is, `callgrind_annotate --inclusive=no`; GCC's `[clone …]` suffixes are merged into
  the original function) changed most between the reference (base when triggered by head÷base, anchor
  when triggered only by head÷anchor) and head, both in the summary and in the JSON (the row's
  `functions` field). When a total equals its only stage, it is listed once.
- **Suspected inlining change**: a function in the table that meets either condition below is marked in
  the table header and in "notes":
  - It appears on one side only, and one of its callers or callees changes in the opposite direction by
    at least half its amount (it was inlined into that function or split out of it);
  - A caller and callee pair's self Ir move in opposite directions, and the change of their sum is less
    than half the sum of their changes' magnitudes.

  Both only count functions whose change is at least 10% of the metric's total change. A function that
  appears on one side only with no partner moving the other way is added or removed code; same names
  with different parameters mean a changed signature; neither is marked. **This mark is diagnostic only
  and exempts nothing**, nor does it change the verdict: real regressions can also involve inlining moves
  (the old M3's formal.mutate, slg.C and S.ticks_middle would all be marked), so a mark cannot be taken
  as a false positive; exemptions still come only from the ledger.
- **Allocator recycling type**: when at least half of a marked metric's net Ir increase is the self Ir
  increase of `FreeTableRecycler::recycle`, it is marked in the table header and in "notes", and split
  into two kinds by the decrease of self Ir in malloc/free/realloc/calloc/new/delete (including glibc's
  `_int_*` and `tcache*`): a decrease of at least 10% of the recycle increase counts as "allocator calls
  replaced by in-place recycling" (type D2, 28–55% in calibration); otherwise "more bins to recycle"
  (type COPS, where in calibration the allocator dropped only 1–3% of the recycle increase, noise
  level). In the 2026-10-26 calibration against Mac L2, Ir went up in both cases (D2's slg.B.in_decode
  GCC 1.059, P2996 1.052; COPS's slg.B GCC 1.014), yet both got faster on the Mac (0.989; 0.977), and
  the cache simulation did not reverse D2 either. **This mark is likewise diagnostic only and exempts
  nothing**, and the verdict is unchanged: such PRs are class L anyway because of the Ir warning or
  failure, attach a cache-sim comparison (section 5a), and the performance thread judges; the post-merge
  Mac review is report only.
- When it goes red: first see which stage and which functions; if it is unexpected, fix it; if it is
  marked as suspected inlining, first try moving the new cold path into a `[[gnu::noinline]]` function
  (see section 4); if it is an intentional trade-off, write a ledger entry (see above), and exemptions
  above 1% also attach a cache-sim comparison before merging (section 5a).

## 5a. Cache-sim comparison (class L, warning item)

Ir cannot see changes in memory access (section 8), so class L PRs attach a cache simulation comparison
before merging. It is produced by `perf_gate.py measure` (or `run`) with `--cache-sim`, and `compare`
runs as usual:

- **What runs**: besides the Ir gate's normal measurement, every SLG stage, formal stage and reloc item
  of base and head (not anchor) runs once more under callgrind. It runs the same way as the Ir gate: S
  scale, seed 20261021, 8 cycles, collecting only cycles 4–7, `SLG_REUSE=1`, rep movsb/stosb off, each key
  collected separately.
- **Parameters**: fixed as `--cache-sim=yes --D1=131072,8,128 --LL=16777216,16,128`
  (`CACHE_SIM_ARGS`).
- **What is compared**: per scenario and per stage, D1 misses (D1mr+D1mw) and LL misses (DLmr+DLmw), and
  each one's head÷base.
- **When it warns**: for scenario totals, or stages with at least 5% of their scenario's Ir (the same
  classification as the Ir gate), a warning needs one level of misses to meet both conditions below:
  - head÷base > 1.02;
  - the miss increase is ≥ 100, or ≥ one ten-thousandth of the metric's base Ir.

  The second condition avoids warnings when a count of a few dozen grows by two or three. Stages under 5%
  are only listed in the JSON, without warnings.
- **Where results go**: the verdict line ends with `cache warn|pass`; the summary header shows a cache
  warning, with a cache table; the JSON has it in the `cache` field. The result table goes into the PR
  body, and warnings are decided by the performance thread.
- **Does not block merging**: warning only, never a fail, and it does not change the Ir verdict or exit
  code. The simulated cache is not any real machine, and conclusions still rest on the post-merge Mac
  review. The Ir reported by cache-sim runs differs slightly from normal runs and is not used.

## 6. Manual triggers and replays

Actions → "Perf gate (instruction counts)" → Run workflow, choose a branch with `bench/` (usually main),
and enter `base`, `head`, `anchor` (optional, defaults to that branch's `bench/perf/ANCHOR`) and
`compilers`. The bench code comes from the chosen branch, so commits from before `bench/` was checked in
can be replayed, and Mac candidates can be pre-checked. When replaying commits from before GCC 16 support
landed, the GCC side is not comparable; look only at P2996. When replaying historical commits, set
`anchor` to `base`, otherwise head÷anchor compares against a newer commit.

Locally (needs GCC 16 or P2996 Clang, valgrind, python3):

```bash
git submodule update --init external/typelayout
git clone https://github.com/USCiLab/cereal.git /tmp/cereal && git -C /tmp/cereal checkout ebef1e92
W=build/perf-gate
python3 tools/perf_gate/perf_gate.py prepare --work $W --base origin/main --head HEAD
python3 tools/perf_gate/perf_gate.py run --work $W --compiler gcc16 --cereal /tmp/cereal/include
python3 tools/perf_gate/test_perf_gate.py   # self-test of the verdict logic, no compiler needed
```

`build`'s `--cxx` can be a command (for example g++ wrapped in docker), and `--extra-flags` appends
compile options (add `-static-libstdc++ -static-libgcc` when the host valgrind runs a binary built in a
container).

## 7. Interfaces the bench code must provide

The gate builds with compile commands directly (no CMake), and requires of `bench/`:

| Location | Requirement |
| --- | --- |
| `bench/slg/bench/slg_bench.cpp` | Entry point; include directories `include`, `adapters`, `bench`; compiles with `-DSLG_WITH_XOFFSET=1 -DSLG_WITH_CEREAL=1` |
| `bench/slg/bench/runner.hpp` | With `-DSLG_IR_COUNT=1`: `timed()` toggles collection with `CALLGRIND_TOGGLE_COLLECT` for regions whose key **exactly equals** an entry of `SLG_IR_KEYS` (comma-separated), or matches a prefix entry ending in `*`; with `SLG_IR_SKIP_CYCLES=N`, cycles 0…N−1 are not collected (the main loop sets `slg::ir_cycle() = c` each cycle). Without `SLG_IR_COUNT` it includes no valgrind header and does not change the timing code |
| Output of `bench/slg` | `--mode bench --out f.jsonl` writes one `{"cycle","format","metric","ns"}` per line; the gate uses it to discover stage names |
| `bench/formal/driver.cpp` | With `-DSC_IR_COUNT=1`, collects only the one stage named by `SC_IR_PHASE` (load, query_before, mutate, emit_after, retire, reload, query_after, cleanup); `-DSC_ADAPTER_HEADER="adapters/xoffset/adapter.hpp" -DSC_ADAPTER_TYPE=sc::XOffsetAdapter`, include directories `include` and the root |
| `bench/formal/model/reference.py` | `--scale S|M --out DIR` generates the fixture (`input.canonical.bin`, `trace.bin`, `fixture.json`) |
| `bench/reloc/reloc_bench.cpp` | Single-file entry point needing only the library include directory; with `-DRELOC_IR_COUNT=1`, collects only in XOffset's timed region of the items selected by `RELOC_IR_KEYS` (exact names or `*` prefixes), and with `RELOC_IR_SKIP_ROUNDS=N`, rounds 0…N−1 and the warm-up round are not collected; `--cases <item>` runs only that item; `--mode verify --out f.jsonl` verifies and writes `{"case","round",…}` per line, which the gate uses to discover item names |
| `bench/evolution/family_ir.cpp` | Single-file entry point; include directories: the library, `external/typelayout/include` and `bench/slg/include`; `--list` prints the rows, `--check` verifies them, `--row <row> N` runs N measured calls after one warm-up, and with `-DFAMILY_IR_COUNT=1` collects only inside them |

Exact matching is required: with prefix matching, `C.snapshot` would also count `C.snapshot2`.

## 8. What it cannot catch

Changes that alter only memory access, not instruction counts: #27 made the formal mutate 3.0% faster on
the Mac while Ir changed only −0.1%; the new M3 made B.season_edits 2–3% slower on the Mac while x86 Ir
was −5%. x86 and the Mac can move in opposite directions. On 2026-10-26 the Mac L2 gate pairs were
replayed with both GCC 16 and P2996 Ir: for allocator recycling changes (D2, COPS) both Irs moved
opposite to the Mac; GCC inlining decisions flipping can make GCC Ir move opposite to the Mac (W0b's
formal.mutate: GCC 0.955, P2996 1.005, Mac 1.011); even when the directions agree, Ir and the Mac differ
in magnitude by 0.3–5x. So L0 is only a pre-merge filter. Since 2026-11-05 merges and releases use only
cloud verdicts (see the top of the page); such changes are flagged by the class L cache-sim comparison
(section 5a), backstopped by the post-merge and post-release Mac reviews (report only), with regressions
going into the next patch release.

## 8a. Machine code comparison (class 0 evidence)

`tools/perf_gate/asm_equal.py` answers "is the machine code of the three bench programs (the formal
harness `sc_xoffset`, `slg_bench`, `reloc_bench`) the same before and after the change", the criterion for
MERGE_RULES_V2 class 0, "disassembly identical instruction by instruction under both compilers". The
`asm` step of `tools/local_validate.sh` reuses the builds of the Ir step and compares once each under GCC
16 and P2996, writing the result into REPORT.md; by default it only reports, and with `--asm-strict` a
difference fails. Standalone: `asm_equal.py run --base REV --head REV --compiler gcc16|p2996 --work DIR
--cereal INC [--cxx ...]`, or for existing builds `asm_equal.py compare --base DIR --head DIR`.

- What is compared: each function of the objdump output, cut by symbol size (alignment padding after a
  function counts as placement, not code), with addresses removed: jumps within a function become offsets
  from the function start, references to other functions become that function's identity, and
  rip-relative and absolute references to data become "data". A function's identity is a hash of its
  normalized instructions with referenced functions replaced by their identities, iterated a few rounds,
  so renamed functions still match while calls to a different target do not.
- Verdict: when the multisets of identities on both sides are equal, it is "identical instruction by
  instruction"; if in addition the sequence of (identity, offset from the start of .text, size) in .text
  is the same, it is "same placement". A change of placement only still counts as class 0, but goes into
  the PR body and the ledger as a bisection point for the next stage review (INT1's map.insert had the
  same instructions with only alignment changed, and was 6–8% slower on the Mac).
- When they differ: first list the functions whose own instructions changed (with a diff), then the
  functions that changed only because a function they call changed.
- Limits: GCC `-static` produces a fixed-address executable, so addresses in immediates are taken as
  addresses only when they are a "function start" or "fall in the data segment"; everything else is a
  number; so a constant that happens to fall in the data segment's address range is taken as an address:
  a placement change can then be falsely reported as a difference, and differing such constants on the
  two sides can be missed. P2996 builds are position-independent and all addresses go through
  rip-relative addressing, so they do not have this problem.
- Self-test: `tools/perf_gate/test_asm_equal.py [--cxx ...]` verifies with small programs that a rename
  is judged identical, moving addresses is judged "same instructions, different placement", and adding a
  function or changing a constant is judged different and located to the changed function.
