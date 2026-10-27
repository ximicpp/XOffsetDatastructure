# Relocation tax benchmark (PERF_GOAL G9)

This benchmark times single container operations on XOffset containers against the same operations on std containers. Both sides run in one process and nothing is encoded or sent. Each round gives one ratio per comparison, XOffset ÷ baseline.

G9 says that, for every item, the Bonferroni one-sided 95% upper bound (UB) of that ratio must be ≤ 1.05.

The benchmark comes from the X1 micro part of `experiments/mutable_relocatable/reloc_tax.cpp` on branch `claude/mutable-relocatable-positioning`. The cloud results are in `RESULT_X1_X2_20261022.md` on that branch. clone() / clone_into() (G10) will be added once clone is implemented.

The origin and every file's sha256 are listed in `MANIFEST`, and `python3 bench/check_manifest.py` checks them. Changing a case changes what is measured. When you do, update `MANIFEST` and add an entry to `bench/perf/ledger.json`.

## Cases

**G9 cases** are the ones G9 judges. Each is judged against its primary baseline.

**Reported cases** are measured with the same statistics but are not part of the verdict.

| Case | What is timed | Primary baseline |
| --- | --- | --- |
| `vec.push_back` | 8 × (clear, then push_back 4096 int32), capacity kept | `std::vector` |
| `vec.push_back_grow` | push_back 4096 into each of 8 empty vectors (with growth) | `std::vector` |
| `vec.iterate` | sum over 4096 int32, 32 passes | `std::vector` |
| `vec.mid_insert` / `vec.mid_erase` | 512 inserts / erases at the middle of about 2048 elements | `std::vector` |
| `hero.mid_erase` / `hero.mid_insert` / `hero.push_back_grow` `.ord` | 64 erases / inserts at the middle of 256 heroes; 4 × 256 emplace_back with growth. A hero is 3 ints, a string and two vectors | `std::vector` |
| `str.assign_short` / `str.assign_long` | 32 passes assigning 256 strings: 8 B (inline on both sides), or 40 B | `std::string` |
| `str.append` | 32 × 256 × 6 appends. **Skipped:** XString has no `append()` today. The case turns on when the library has one | `std::string` |
| `map.{insert,find,erase}.fresh`, `set.{…}.fresh` | 512 inserts; 4096 lookups among 512 keys; 512 erases. Keys, lookups and erase order are new every round | `std::flat_map` / `std::flat_set` |
| `nested.build.ord` | build 32 armies (8 heroes, 6 troops) into empty roots | `std` (construct + fill) |
| `reset` | `reset<T>()` of 64 buffers that each hold a built army | `std` (`army = Army{}`) |

Reported only:

- **Node containers.** `std::map` / `std::set`, beside the flat containers.
- **`*.reused`.** This is X1's protocol: the same keys every round, and lookups that walk the keys in insertion order. The branch predictor can learn this sequence: "容器操作提速" found that it makes std::flat_map lookups look fast. The fresh-key cases are the ones judged.
- **`str.assign_short.view` / `.copy`.** `str.assign_short` assigns a `std::string_view` on the XOffset side and copies a `std::string` on the std side: libc++ copies a short string as its 24-byte representation, while the view has to be measured and copied byte by byte. These two compare like for like: both sides assign from a view (`.view`), or both copy a string of their own type, an XString from the same buffer on the XOffset side (`.copy`).
- **`.plain`.** The same hero and army records, not declared ordinary. The `.ord` records are declared with `XOFFSET_ORDINARY_RECORDS`, as a user following the guide would write them: together with the records they hold as container elements (`Skill`, `Buff`, `Troop`; user guide section 3.1). Before 2026-10-26 those three were not declared, so the `.ord` heroes took the element-by-element path of the `.plain` ones. On a library without the macro, the two specializations are written out. `static_assert`s check that each declaration took effect and, on a library with the macro, that `.ord` heroes move by SHIFT and `.plain` heroes do not.

Choices worth knowing:

- **`reset` baseline.** `reset` is compared with assigning an empty object to a built std army. Both operations drop the old content (std frees its blocks) and leave an empty root. X1 compared against constructing an empty std object into empty storage, which costs about 2 ns and is not the same operation.
- **Setup.** Every side does its own setup for the round outside the timed region: refill, rebuild, `reset<T>()` of fresh buffers, or destroying the old std objects.
- **Checks.** After every round, a digest of the result must match across all sides.
- **Timer resolution.** Each timed region takes at least about 10 µs on the cloud machine, so that the 24 MHz timer on Apple M-series (41.7 ns per tick) stays below about 0.5%.

## Build and run

```
cmake -S bench/reloc -B build/reloc -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
cmake --build build/reloc
build/reloc/reloc_bench --mode verify                      # 3 rounds, digests only
build/reloc/reloc_bench --rounds 201 --out raw.jsonl       # timing
python3 bench/reloc/tools/analyze.py raw.jsonl [raw2.jsonl ...]
```

- **Compilers.** GCC 16 or P2996 Clang. `XOFFSET_ROOT` defaults to this repository.
- **Flat containers.** std::flat_map / std::flat_set need `<flat_map>` / `<flat_set>`. If the standard library lacks them, the binary prints the cases as skipped, and those maps / sets are reported against `std::map` / `std::set` only.
- **Selecting cases.** `--cases a,b` selects cases by exact name, or by prefix when the name ends in `*` (for example `map.*`).

## Protocol and statistics

- **Round order.** Within a round the sides run back to back, after 3 warm-up rounds. The order rotates by one every round: XOffset first in even rounds when there are two sides.
- **Ratios.** `analyze.py` pools the rounds of all files it is given. For each comparison it takes the per-round ratios r_i and reports:
  - the geometric mean GM = exp(mean ln r);
  - UB = exp(mean ln r + t(1 − 0.05/k, n − 1) · sd(ln r)/√n), where k is the number of G9 comparisons in the data.
- **Verdict.** A G9 comparison passes when UB ≤ 1.05.
- **Reported comparisons.** These use the same α/k, so that their bounds read on the same scale.
- **Where the numbers come from.** Cloud numbers show ratios and orders of magnitude only. G9 numbers come from the Mac batch.

## Instruction counts (CI performance gate)

`-DRELOC_IR_COUNT=ON` builds a binary for callgrind. Run it under `valgrind --tool=callgrind --collect-atstart=no`:

- Collection is on only inside the XOffset side's timed region of the cases named by `RELOC_IR_KEYS` (comma-separated: an exact case name, or a prefix ending in `*`).
- Collection is off in rounds below `RELOC_IR_SKIP_ROUNDS` (default 0) and in warm-up rounds.
- Without the option, nothing changes in the timing path and no valgrind header is needed.

Measured on cloud GCC 16 with valgrind 3.22, `--rounds 5 --warmup 1`, `RELOC_IR_SKIP_ROUNDS=2`, one callgrind run per case with `--cases <case>`:

- Ir is identical across repeated runs for all 28 cases.
- All 28 runs take about 3 s on 4 workers.

That is cheap enough for the gate's three trees (anchor, base, head). The binary also builds against the anchor 30bb5ca.
