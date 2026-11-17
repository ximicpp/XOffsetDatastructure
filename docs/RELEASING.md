# Release process

This page is the fixed process for cutting releases and prereleases from `main`. GitHub CI is
permanently off (2026-10-23); every step's evidence comes from local validation in the cloud, without
waiting for any GitHub job or for the Mac (decided by susu on 2026-11-05: releases use only the cloud;
the Mac stage review runs after the release, section 7).

Division of work: cloud verdicts, local validation and the release PR are done by the development
thread; the tag is pushed by susu on their own machine, or by a Mac session on request (pushing tags
from the cloud is refused with 403). For each release susu only needs to run the 3 commands of step 5.

## 1. Version numbers

- The only version source is `XOFFSET_VERSION` at the repository root: `MAJOR.MINOR.PATCH` for releases
  (such as `3.0.0`), plus `-(alpha|beta|rc).N` for prereleases (such as `1.1.0-rc.1`), with no leading
  zeros. `cmake/XOffsetVersion.cmake`, `tools/validate_vm_package.py` and `tools/prepare_release.py` all
  read it, and configuration or packaging fails outright on a malformed value.
- The tag name is `v` plus the version (`v3.0.0`), an annotated tag on the merge commit of the release
  PR. The repository's `v1.0.0` (2024, the old v1 library, with a public Release) and `v2.0-*` belong to
  the two previous implementations; version numbers must not collide with them, so check once with
  `git ls-remote --tags origin` before releasing.
- The install package only accepts the full version string: numeric, `EXACT` and range requests in
  `find_package` are all rejected, and consumers check `XOffsetDatastructure_VERSION STREQUAL "<version>"`
  themselves ([BUILD_AND_PLATFORMS.md](BUILD_AND_PLATFORMS.md), "Build and consume"). No API, ABI or wire
  compatibility is promised between any two versions; the scope of wire freezes within 1.x is as stated
  in CHANGELOG.

## 2. Before release: cloud performance verdict

Since 3.3.1 the pre-release performance verdict uses only the cloud (MERGE_RULES_V2 addendum,
2026-11-05); Mac release batches no longer run:

- The candidate commit (the commit on `main` being prepared for release, called C below) runs the GCC 16
  and P2996 Ir gates against the previous release tag. They are the `ir` and `p2996-ir` steps of step 3's
  `local_validate --release --base <previous tag>`: the base is the previous tag, and step 7 moved
  `bench/perf/ANCHOR` to the previous tag, so head÷base and head÷anchor are both C against the previous
  tag.
- The verdict (each compiler judged separately) is the tool's verdict line for head÷base, with the same
  thresholds as the class-S gate ([PERF_GATE.md](PERF_GATE.md) section 3): any total > 1.010 or any stage
  > 1.05 is a fail; any total > 1.003 or any stage > 1.02 is a warning. There is no Mac check before
  release any more and the cloud is the only release verdict, so it is no looser than the tool's own
  lines. A fail cannot be released: fix it, or take the data to susu. Warning metrics, with the function
  diff table, go into the release PR; the performance thread decides on them, and they go into the
  release notes.
- If it does not pass: find the specific PR, revert or fix it, and run again; if time runs out, tag the
  last passing commit and move the failing changes to the next version (section 6).
- Merge rules also use only the cloud (susu 2026-11-05 01:26Z): class S and class L PRs only pass the GCC
  16 and P2996 Ir gates before merging (class S thresholds, i.e. the set above), then `local_validate`
  runs on the merge result; class L also attaches a cache-sim comparison of D1/LL misses as a warning
  item ([PERF_GATE.md](PERF_GATE.md) section 5a). The Mac only reviews after merges and releases, report
  only, and never blocks a merge.

## 2a. Before release: cloud x86 scorecard columns and uplink columns (since 3.6)

The two columns added in 3.5 (cloud Linux x86, client uplink) were report only; since 3.6 they have
thresholds. The cloud machine is not quiet: two builds of the same code over 10 interleaved batches
(A/A) already differ by 0.98–1.04 in geometric mean, with one-sided 95% upper bounds up to 1.12
(2026-11-11, cloud Xeon 2.3 GHz, 12 cells), so the cloud judges only the point estimate and leaves room
for this noise; once 4.0 moves to a quiet x86 machine the thresholds tighten to the upper bound
(roadmap R2).

- How to run: build `bench/slg` with GCC 16 `-O3`, then
  `tools/perf_gate/x86_columns.sh <slg_bench> <directory> 10`: M scale, native off, one process per
  format per batch, 10 interleaved batches, pinned to one core; `D3=1` additionally runs one XOffset
  process with `SLG_D3=1` per batch (native on: with native off the bench does not even run the D rows),
  and its `D*` rows are compared with the same batch's rival `U*` rows (`x86_columns.py --d3-tag`). The
  verdict comes from `tools/perf_gate/x86_columns.py`: messages of the same batch and cycle are the same
  in every format, ratios are paired by (batch, cycle), cycle 0 is not counted; log-mean within a batch,
  geometric mean across batches; the upper bound is printed (t distribution, df = batches − 1) but does
  not take part in the verdict.
- Main cells (XOffset uses `xoffset_trusted`, rivals zpp_bits and cista_unchecked; flatbuffers_noverify
  is added when there is a FlatBuffers build): server `*.send` for L / EB / LD / UP / UC (D3 adds DP / DC:
  XOffset's publish plus send `D*.pub + D*.send` ÷ the rival's `U*.send`), client single-device hot view
  `*.recv` for EBW / LDW / UPW / UCW / USW (D3 adds DPTW / DCTW ÷ the rival's UPW / UCW), and the uplink
  column's server receive UL.recv (bounds tier).
  - Pass: geometric mean ≤ 1.00.
  - Warning: 1.00 < geometric mean ≤ 1.05 (within A/A noise). Run 10 more batches and judge on 20; if
    it is still a warning, put it in the release PR.
  - Fail: geometric mean > 1.05. As with the Ir gates, fix it or take the data to susu.
- Uplink UL.send (client sending uplink) is report only, not judged (susu 2026-11-12, 4.0 plan: no more
  optimization for it). 3.6.0 measured 1.35 × zpp_bits in the cloud (the 3.6 limit of 1.80 was dropped);
  the gap is the per-message fixed cost of small messages (reset, first allocation, save). ULF.recv
  (full validation, against the rival's UL.recv) and bytes are report only.

## 2b. Reproducing the scorecard with one command (since 4.0)

`tools/perf_gate/repro_scorecard.sh <ref> <directory> [batches=8] [cycles=21]` reproduces the SLG
scorecard from a tag or commit on any x86-64 Linux with docker, git, python3 and network access:

- Pinned inputs: the gcc:16 image by digest (the same as `local_validate.sh`), rivals zpp_bits v4.7.6
  (`4a57ecd`) and cista v0.16 (`015b94e`), compression with zstd v1.5.7 and lz4 v1.10.0, all fetched by
  commit and checked; the `<ref>` tree is exported with `git archive` (no submodules);
  `bench/slg/bench/slg_bench.cpp` is built as a single translation unit with `-O3`.
- How it runs: M scale, seed 20261021, `SLG_REUSE=1 SLG_CASES=demo`; each batch runs one
  `SLG_NATIVE_OFF=1` process per format (tag `head`), then one XOffset process with `SLG_D3=1` (native on,
  tag `d3`), and one `SLG_D3=1` process for each rival shared encoding (`SHARED`, default zpp_bits and the
  diagnostic format zpp_bits_dirty, 4.0 W1; skipped when the bench of `<ref>` does not have them);
  batches are interleaved and pinned to one core (`CPU=`, default 2), and every process is a fresh
  container without network. A second binary built with `SLG_WITH_COMPRESSION=1` runs 1 batch only to
  collect zstd-3 bytes (`ZSTD=0` skips it). Timing covers only data read/write and send/receive;
  compression is not timed (project rule).
- Output: `SCORECARD.md` (the scorecard), `scorecard.json`, `env.json` (commit, image digest, compiler,
  parameters, binary sha256, CPU model, start/end times and load), `raw/` and `rawz/` (one JSONL per
  process), `logs/`.
- Scoring (`tools/perf_gate/scorecard.py`, using the rules of the checkout the script lives in, so old
  tags are scored by today's rules): 20 main cells — server to server A / B / S / SD (sum of stages),
  server send L / EB / LD / UP / UC and D3's DP / DC (publish plus send ÷ the rival's `U*.send` or the
  rival shared encoding's `D*.pub + D*.send`), client L.recv and hot views EBW / LDW / UPW / UCW, D3's
  DPTW / DCTW (÷ the rival's `U*W.recv` or its shared encoding's `D*TW.recv`), uplink UL.recv and
  UL.send. D cells are scored against the rival's strongest approach (susu, 4.0 plan): per-client and
  shared encoding are each a column ("zpp_bits shared"), and the column where XOffset is slowest counts.
  Each cell is xoffset_trusted ÷ the strongest rival (the one XOffset is slowest against), paired by
  (batch, cycle ≥ 1), log-mean within a batch, geometric mean across batches. **Win < 0.95, tie
  0.95–1.05, loss > 1.05.** UL.send is report only, so 19 cells count. Bytes are report only.
- Time: about 5 minutes to build both binaries in a 4-core cloud container, about 25 minutes for 8
  batches. Do not build anything else on the same machine while it runs.
- Comparison: `BASELINE=tools/perf_gate/scorecard_v3.6.0.json` prints the baseline and the difference
  after each cell. This baseline is a reproduction of v3.6.0 in a clean copy (2026-11-12, cloud Xeon
  2.1 GHz, 4 cores, 8 batches × 21 cycles, 34 minutes): 19 cells, 16 wins, 3 ties (UP.send 1.048,
  UPW.recv 0.997, UL.recv 0.987), 0 losses, UL.send 1.36; every cell is in the same class as in the 3.6
  scorecard (6 batches), with the largest single-cell difference 9.6% (L.recv). The cloud machine is not
  quiet, and single-cell differences of around 10% between two rounds of the same code are normal, so
  read the scorecard by win/tie/loss class, not by the third decimal place.

## 2c. Re-judging borderline D3 main cells (since 4.0)

Decided by susu on 2026-11-15 (4.0 plan, "handling borderline DP"): before release, run one formal
scorecard on candidate C (section 2b, `CPU=2`, 8 batches × 21 cycles, nothing else running on the same
machine). When the geometric mean of a D3 main cell (DP, DC, DPTW, DCTW) falls between **1.02 and
1.08**, the cell is sensitive to the code layout of the whole binary (I-cache set conflicts; different
builds of the same code differ by 3–5%), so it is not judged on a single scorecard but re-judged:

1. On the same machine, run the whole 8-batch scorecard once more under each of three code layouts,
   adding only one compile flag, the same for every format, with everything else unchanged. The layouts
   are fixed in advance, not chosen after seeing results:

   ```sh
   for a in 16 32 64; do
     EXTRA_CXXFLAGS=-falign-functions=$a tools/perf_gate/repro_scorecard.sh <C> <directory>/align$a
   done
   ```

   `EXTRA_CXXFLAGS` is appended to the compile flags of both binaries, recorded in `env.json` as
   `toolchain.extra_cxxflags`, and printed on the "Extra compile flags" line of `SCORECARD.md`. 16 is
   close to GCC's default alignment on x86-64 (the default also has a padding cap); all three are run
   with the flag given explicitly.
2. A triggered cell takes the **median** of the three geometric means: ≤ 1.05 counts as a tie, > 1.05
   as a loss. All three numbers go into the release record.
3. Other cells keep the verdict of the default-layout scorecard. For cells outside 1.02–1.08 the
   scorecard's own verdict stands.
4. If the re-judgment is a loss: do not tag; take the three numbers to susu.

## 3. Before release: local validation in the cloud

Run the full local validation, including TSan, on C:

```sh
tools/local_validate.sh --release --base <previous tag>
```

`--release` adds GCC 16 TSan full CTest, the whole C++20/C++23 matrix and all client columns
([PERF_GATE.md](PERF_GATE.md), the tier table at the top) on top of the PR-tier steps (GCC 16 Release
full CTest, ASan+UBSan, `test_v2_fuzz`, validation differential, bench smoke, the Ir gates for both
compilers, P2996 build and signature check). The release PR itself only changes the version and docs,
and the candidate before the PR has already run `--release`. Fuzzing (section 3.1) and `--release` do
not depend on each other; run them at the same time in two threads instead of one after the other.
Every step must pass, and the two Ir gates are judged per section 2. For client columns that report
"not run" because the container has no toolchain (for example the Android NDK), state in the release PR
and the docs that this release did not re-run them. Paste `REPORT.md` into the release PR.

### 3.1 Pre-release fuzzing (K4)

On the same C, run one round of ASan+UBSan fuzzing for every receive entry point (decided by susu on
2026-11-04; no longer nightly). Cloud background jobs are stopped after 2 hours, so run it in phases,
one background job per phase (for the GCC phase, start dockerd first per step 1 of
`fuzz-nightly/README.md`):

```sh
OUT=/mnt/project-files/release-X.Y.Z/fuzz
tools/fuzz_prerelease.sh --out $OUT --phase gcc        # GCC 16 Docker differential fuzzing + replay, about 50 minutes
tools/fuzz_prerelease.sh --out $OUT --phase libfuzzer  # all libFuzzer targets of the server reflection build
tools/fuzz_prerelease.sh --out $OUT --phase client     # client field-list build + comparing results of three builds
tools/fuzz_prerelease.sh --out $OUT --phase verdict    # writes VERDICT.md; continue only on PASS
```

Budget: the default `--budget release` (GCC 2700 seconds, libFuzzer 1200 seconds per target per build,
about 4 hours in total). Since 3.5, per susu's decision of 2026-11-10, use `--budget hour` (GCC 900
seconds, libFuzzer 300 seconds per target, about 1 hour for the nine targets, plus build time); all four
phases must use the same `--budget`; `quick` is only a trial run and does not count.

VERDICT is PASS only when all three phases ran on C (clean worktree), the budget is `release` or `hour`,
and every job succeeded with no findings. On a finding: `tools/fuzz_report.py` opens an issue (usage at
the top of `tools/fuzz_nightly.sh`), the fix PR puts the minimized input in
`tests/fuzz/regressions/<target>/`, and after it merges the run is repeated on the new C. Paste
VERDICT.md into the release PR.

## 4. Release PR (class 0)

A PR that changes only the version and docs, titled `Release X.Y.Z`, with base `main`:

1. Change `XOFFSET_VERSION` to the new version.
2. `CHANGELOG.md`: move the entries under `## Unreleased` as a whole under a new heading
   `## X.Y.Z: <one-line theme>`; under the heading, first write two or three sentences on the scope of
   this release (main changes since the previous tag, known gaps), and leave an empty `## Unreleased`
   above it.
3. Update the old version number in current documents: everything listed by
   `git grep -n '<old version>' -- 'README*.md' 'docs/*.md'`: the version at the top of `README.md`,
   the consumption example in `BUILD_AND_PLATFORMS.md`, document titles such as the user guide, and the
   consumption example in `ALPHA_COMPATIBILITY.md`. The numbers and methods quoted in the README
   performance section (measured on a particular version) are not changed; replace them when there is a
   new scorecard or comparison. Historical records such as `docs/perf/` and `docs/validation/` are not
   changed.
4. Regenerate the source manifest `SOURCE_MANIFEST.json`, and confirm that the packaging tool runs end to
   end on this commit:

   ```sh
   python3 tools/prepare_release.py create --source-dir . --out build/release-source
   cp build/release-source/xoffsetdatastructure-X.Y.Z.manifest.json SOURCE_MANIFEST.json
   python3 tools/prepare_release.py audit \
     --archive build/release-source/xoffsetdatastructure-X.Y.Z.tar.gz \
     --manifest build/release-source/xoffsetdatastructure-X.Y.Z.manifest.json \
     --checksums build/release-source/SHA256SUMS
   ```

   `create` reads the current worktree (not Git), so first prepare the pinned submodules per
   [PACKAGING.md](PACKAGING.md#fixed-inputs). `SOURCE_MANIFEST.json` is not in the source archive, so
   copying it in does not change the manifest itself.
5. This PR runs `tools/local_validate.sh --steps gcc,p2996` (the version goes into the CMake
   configuration and install metadata), then does one storage install in a new build directory to
   confirm `pack-prefix` passes:

   ```sh
   cmake -S . -B build/release-storage -DXOFFSET_STORAGE_ONLY=ON -DXOFFSET_BUILD_TESTS=OFF \
     -DCMAKE_INSTALL_PREFIX=$PWD/build/release-storage-prefix
   cmake --build build/release-storage && cmake --install build/release-storage
   python3 tools/prepare_release.py pack-prefix --prefix build/release-storage-prefix \
     --component storage --out build/release-storage-pack
   ```

Merge condition: steps 2 and 3 pass on C, and nothing other than this PR has merged into `main` after C.
The release PR does not change library code, so C's cloud verdict and local validation results apply
unchanged to its merge commit. If another PR merges in the meantime, replace C with the new head and
re-run step 3 (step 2's two Ir gates are in the same run).

## 5. Tagging (susu, locally, 3 commands)

After the release PR merges, run in a local repository (`<commit>` is the merge commit of the release
PR; the development thread gives the full hash in the thread); a Mac session can also run them on
request in the Mac thread (3.2.1 and 3.3.0 were pushed that way). After the push, the development thread
checks with `git ls-remote`:

```sh
git fetch origin main
git tag -a vX.Y.Z <commit> -m "XOffset vX.Y.Z"
git push origin vX.Y.Z
```

Optional: on the GitHub website, Releases → Draft a new release, pick the tag just pushed, title
`XOffset vX.Y.Z`, and paste this release's whole CHANGELOG section as the body; attach the source archive
and `SHA256SUMS` from step 4 if needed. For prereleases, tick "Set as a pre-release".

## 6. When the cloud verdict does not pass

- The tag goes on the last commit that passed steps 2 and 3; `XOFFSET_VERSION` and CHANGELOG on that
  commit do not carry the new version, so branch from that commit for the release PR (changing only the
  step 4 content) and tag its merge commit after it merges, or postpone the release. State in the thread
  which of the two was chosen.
- Changes that did not pass go into the next version's plan and stay under `## Unreleased` in CHANGELOG.

## 7. After the release

- All "no regression" verdicts for the next version use this tag's commit as the baseline.
- Move the Ir gate anchor forward: open a class-0 PR that only changes `bench/perf/ANCHOR` (to the tag's
  commit), and at the same time move stale ledger entries to `bench/perf/ledger_archive.json`
  ([PERF_GATE.md](PERF_GATE.md) section 4).
- Mac stage review: run once after the release (head is this tag's commit, anchor the previous tag),
  scheduled in the first free Mac slot after the release and not postponed indefinitely; judged by the
  L2 rules, report only, with results in the project files under
  `perf-gate/mac/<date>-<batch name>/VERDICT.md`. A regression gets a fix PR in the next patch release
  and is mentioned in the next release notes.
- Record the tag, commit, cloud verdict (`local_validate`'s `REPORT.md`) and the path of the later Mac
  review VERDICT in the project files.
