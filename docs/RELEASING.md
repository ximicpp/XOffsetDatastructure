# Release process

Releases and prereleases are cut from `main`. GitHub CI is currently off; release evidence comes from
running the local validation scripts in this repository. Tags are created and pushed by a repository
maintainer after validation and the release PR pass (pushing tags from automated runners is disabled).

## 1. Version numbers

- `XOFFSET_VERSION` at the repository root is the only version source: `MAJOR.MINOR.PATCH`, plus
  `-(alpha|beta|rc).N` for prereleases, no leading zeros. CMake and the packaging tools fail on a
  malformed value.
- The tag is `v` plus the version, annotated, on the merge commit of the release PR. `v1.0.0` and
  `v2.0-*` belong to older implementations; check `git ls-remote --tags origin` for collisions.
- The install package accepts only the full version string in `find_package`
  ([BUILD_AND_PLATFORMS.md](BUILD_AND_PLATFORMS.md)). No API, ABI or wire compatibility is promised
  between versions.

## 2. Before release: Ir gates

The candidate commit C runs the GCC 16 and P2996 Ir gates against the previous release tag (the `ir`
and `p2996-ir` steps of section 3). Each compiler is judged separately on head ÷ base
([PERF_GATE.md](PERF_GATE.md) section 3):

- **Fail**: any total > 1.010 or any stage > 1.05. Fix it (find the PR, revert or fix) or escalate the
  data to a maintainer.
- **Warning**: any total > 1.003 or any stage > 1.02. Put the metrics and the function diff table in the
  release PR and the release notes.

Merges use the same cloud gates: class S and L PRs pass both Ir gates before merging, class L also
attaches a cache-sim comparison ([PERF_GATE.md](PERF_GATE.md) section 5a). The Mac only reviews after
merges and releases, report only.

## 2a. Cloud x86 columns

`tools/perf_gate/x86_columns.sh <slg_bench> <directory> 10` (GCC 16 `-O3`, M scale, native off, one
process per format per batch, 10 interleaved batches, one pinned core; `D3=1` adds the shared-delta
rows). `tools/perf_gate/x86_columns.py` pairs ratios by (batch, cycle ≥ 1) and judges the geometric
mean of xoffset_trusted ÷ rival (the upper bound is printed, not judged; the host is noisy):

- Pass ≤ 1.00; warning 1.00–1.05 (run 10 more batches, judge on 20, still a warning goes in the
  release PR); fail > 1.05 (as for the Ir gates).
- UL.send, ULF.recv and bytes are report only.

## 2b. The scorecard in one command

`tools/perf_gate/repro_scorecard.sh <ref> <directory> [batches=8] [cycles=21]` reproduces the SLG
scorecard from a tag or commit on any x86-64 Linux with docker, git, python3 and network access.

- Pinned inputs: the gcc:16 image by digest, zpp_bits v4.7.6, cista v0.16, zstd v1.5.7, lz4 v1.10.0,
  fetched by commit; the `<ref>` tree via `git archive`; `slg_bench.cpp` as one translation unit, `-O3`.
- Run: M scale, seed 20261021, `SLG_REUSE=1 SLG_CASES=demo`; per batch one native-off process per
  format, one XOffset `SLG_D3=1` process and one per rival shared encoding; interleaved, pinned to
  `CPU=` (default 2), fresh container per process. A second binary with compression runs one batch for
  zstd-3 bytes (`ZSTD=0` skips it). Compression is not timed.
- Output: `SCORECARD.md`, `scorecard.json`, `env.json` (commit, image, compiler, flags, binary hashes,
  CPU, load), `raw/`, `rawz/`, `logs/`.
- Scoring (`tools/perf_gate/scorecard.py`, today's rules): each main cell is xoffset_trusted ÷ the
  strongest rival (D cells also against the rival's shared encoding), paired by (batch, cycle ≥ 1).
  **Win < 0.95, tie 0.95–1.05, loss > 1.05.** UL.send is report only, so 19 cells count.
- About 5 minutes to build and 25 minutes for 8 batches on a 4-core container; build nothing else
  meanwhile. `BASELINE=tools/perf_gate/scorecard_v3.6.0.json` prints a baseline per cell. Single cells
  move about 10% between runs of the same code: read the class, not the third decimal.

## 2c. Re-judging borderline D3 cells

Run one formal scorecard on C (section 2b, `CPU=2`, 8 × 21, nothing else running). A D3 main cell (DP,
DC, DPTW, DCTW) with a geometric mean between **1.02 and 1.08** is layout-sensitive and is re-judged:

1. Run the whole scorecard once under each fixed layout, on the same machine:

   ```sh
   for a in 16 32 64; do
     EXTRA_CXXFLAGS=-falign-functions=$a tools/perf_gate/repro_scorecard.sh <C> <directory>/align$a
   done
   ```

2. The cell takes the **median** of the three: ≤ 1.05 tie, > 1.05 loss. All three go into the release
   record. Other cells keep the default-layout verdict.
3. A loss: do not tag; escalate the numbers to a maintainer.

## 3. Before release: local validation

```sh
tools/local_validate.sh --release --base <previous tag>
```

`--release` adds GCC 16 TSan, the whole C++20/C++23 matrix and all client columns to the PR tier.
Every step must pass; the Ir gates are judged per section 2. Client columns reported "not run" (no
toolchain) are stated in the release PR. Paste `REPORT.md` into the release PR. Run it in parallel with
section 3.1.

### 3.1 Pre-release fuzzing (K4)

One ASan+UBSan round over every receive entry point, on C, one background job per phase (start dockerd
first for the GCC phase, see `fuzz-nightly/README.md`):

```sh
OUT="$PWD/build/release-X.Y.Z/fuzz"
tools/fuzz_prerelease.sh --out "$OUT" --phase gcc --budget hour
tools/fuzz_prerelease.sh --out "$OUT" --phase libfuzzer --budget hour
tools/fuzz_prerelease.sh --out "$OUT" --phase client --budget hour
tools/fuzz_prerelease.sh --out "$OUT" --phase verdict --budget hour
```

`--budget hour` is about 1 hour plus builds (`release` about 4 hours; `quick` does not count); all phases
use the same budget. VERDICT is PASS only when every phase ran on a clean C with no findings. On a
finding: `tools/fuzz_report.py` opens an issue, the fix puts the minimized input in
`tests/fuzz/regressions/<target>/`, and the round repeats on the new C. Paste `VERDICT.md` into the
release PR.

## 4. Release PR (class 0)

Title `Release X.Y.Z`, base `main`, version and docs only:

1. Set `XOFFSET_VERSION`.
2. `CHANGELOG.md`: move `## Unreleased` under `## X.Y.Z: <theme>` with two or three sentences of scope,
   and leave an empty `## Unreleased` above it. Keep entries short.
3. Update the old version in current documents: `git grep -n '<old version>' -- 'README*.md' 'docs/*.md'`.
   Measured numbers stay until a new scorecard replaces them; `docs/validation/` is not changed.
4. Regenerate `SOURCE_MANIFEST.json` and check the packaging end to end (prepare the pinned submodules
   first, [PACKAGING.md](PACKAGING.md#fixed-inputs)):

   ```sh
   python3 tools/prepare_release.py create --source-dir . --out build/release-source
   cp build/release-source/xoffsetdatastructure-X.Y.Z.manifest.json SOURCE_MANIFEST.json
   python3 tools/prepare_release.py audit \
     --archive build/release-source/xoffsetdatastructure-X.Y.Z.tar.gz \
     --manifest build/release-source/xoffsetdatastructure-X.Y.Z.manifest.json \
     --checksums build/release-source/SHA256SUMS
   ```

5. Run `tools/local_validate.sh --steps gcc,p2996`, then one storage install:

   ```sh
   cmake -S . -B build/release-storage -DXOFFSET_STORAGE_ONLY=ON -DXOFFSET_BUILD_TESTS=OFF \
     -DCMAKE_INSTALL_PREFIX=$PWD/build/release-storage-prefix
   cmake --build build/release-storage && cmake --install build/release-storage
   python3 tools/prepare_release.py pack-prefix --prefix build/release-storage-prefix \
     --component storage --out build/release-storage-pack
   ```

Merge when sections 2 and 3 passed on C and nothing else merged into `main` after C; otherwise re-run
section 3 on the new head.

## 5. Tagging (3 commands)

`<commit>` is the release PR's merge commit; check the push with `git ls-remote` afterwards:

```sh
git fetch origin main
git tag -a vX.Y.Z <commit> -m "XOffset vX.Y.Z"
git push origin vX.Y.Z
```

Optionally draft a GitHub release from the tag with the CHANGELOG section as the body (tick
"pre-release" for prereleases).

## 6. When a verdict does not pass

Tag the last commit that passed sections 2 and 3 (branch the release PR from it), or postpone the
release. Changes that did not pass stay under `## Unreleased`.

## 7. After the release

- The tag is the baseline for the next version's "no regression" verdicts.
- Class-0 PR: move `bench/perf/ANCHOR` to the tag's commit and stale ledger entries to
  `bench/perf/ledger_archive.json` ([PERF_GATE.md](PERF_GATE.md) section 4).
- Mac stage review (head = tag, anchor = previous tag), report only, in the first free Mac slot; a
  regression gets a fix in the next patch release.
- Record the tag, commit, `REPORT.md` and the Mac review path in the project files.
