# SLG 4.0 latency optimization stage review

The performance goal has been paused. The optimization branch is `codex/xoffset-v4-latency-20261115`,
starting from `ee7a2c5f`. Formal performance results were measured on
`8d3cba8c6de448083bccb761b3cb27ee69519064`. The stage close added a Windows dirty-module storage
regression test, run on the same commit under test.

## Performance review

The M scenario uses a pinned zpp_bits executable on the same machine. The table gives the time ratio of
XOffset divided by zpp_bits, with paired 95% confidence intervals. An item passes when the upper bound of
its interval is no more than `1.00`.

| Native platform | `UL.send` | `UP.recv+apply` |
| --- | ---: | ---: |
| Windows MSVC | 0.9623 [0.9481, 0.9768] | 0.9131 [0.9068, 0.9193] |
| Mac mini M4 Pro | 1.0125 [1.0055, 1.0195] | 0.9771 [0.9728, 0.9814] |

Both machines pass the S/M regular and D3 differential checks. The commit under test passes 5 native SLG
regression tests on Windows and 4 on the Mac. The Linux native storage checks pass 3 items, including the
no-exception version. Under a 256 MiB address space limit, the bounded uplink fallback reserves about
1 MiB; the old reservation policy fails under the same limit. The Linux evidence covers storage and
reservation bounds.

## Code review and candidate decisions

- Uplink writes reuse a V2 frame prepared by length. The existing differential test compares 2,048
  commands byte by byte against an independent compact encoding, covering empty armies, fixed slot
  boundaries, large armies, seed wraparound and both receive validations. The macOS by-value entry point
  and the entry point that keeps RNG state share the same generation logic.
- macOS same-arena moves are only used to move the tail unit into the slot of the unit being deleted,
  then update the index and remove the tail. In the M scenario `US.apply` is 4.9% shorter than that
  round's XOffset control, and login `L.recv` grows about 1.3%. On Windows the M scenario `UL.recv` grows
  about 2.7%, and full validation `ULF.recv` is about 15.1% shorter. Each stage and the A/A controls are
  in the raw audit. Mac `UL.send` still does not pass, and the time regression in the receive stages
  still needs to be verified and handled.
- The Windows dirty-module candidate replaces field-by-field copying with the existing native army copy.
  Over 12 reruns, M scenario `SD.save_encode` is 24.1% shorter, and `SD.save_encode + SD.store_recv`
  22.9% shorter. The same candidate's `UL.send` upper bound against zpp_bits is 1.0082. At the pause,
  the isolated reruns had collected 5 groups for M, and S timing had not started. The candidate stays in
  the experiment directory. This commit adds the differential check of the existing storage path to the
  Windows regression tests, covering empty modules, reused receive, field digests and independent full
  validation.
- Mac login receive returns an already bound root object, and the send function keeps a separate call.
  At the pause, 13 groups for M had been collected, and the candidate's `UL.send` against zpp_bits is
  0.9978 [0.9905, 1.0051]. The S/M regular and D3 differential checks pass; S scenario timing had not
  started. Initial screens that separately adjusted the validation workspace, the receive frame length
  formula and the send cache layout did not confirm a gain. The candidate stays in the experiment
  directory.

## Reruns and evidence

Each formal timing batch starts four processes independently: the same XOffset A binary twice, plus
candidate B and the pinned zpp_bits, in rotating order. Each platform completes 16 batches each of S and
M, with fixed seed `20261021`, 21 cycles per process, discarding cycle 0. Time ratios are computed per
cycle, log-transformed and averaged within a batch, and two-sided Student t intervals are computed over
the independent paired batches. Formal timing sets `SLG_REUSE=1 SLG_CASES=demo SLG_NATIVE_OFF=1`. The
differential check runs `--mode verify --scale S|M --cycles 3 --seed 20261021`. The D3 differential sets
`SLG_D3=1`, clears `SLG_NATIVE_OFF`, and adds `zpp_bits_dirty` to the format list.

The build entry points and commands of the native regression tests are in the
[SLG README](../../bench/slg/README.md). The stage close rebuilt in a separate Windows source directory
with the locked dependencies installed, and the 5 tests passed. `environment.json`, `commands.jsonl` and
the per-process JSONL in the experiment archive record binary hashes, arguments and times.
`verify_extreme_windows_release_prepared_overlay_16blocks.py`, `verify_extreme_mac_release_16blocks.py`,
`campaign.py` and `analyze.py` in the experiment directory `xoffset-v4-parity-20261115` can rerun
collection and analysis.

| File in the experiment directory | Contents |
| --- | --- |
| `latency-8d3cba8c-exact-cross-platform-audit.json` | Formal results for both platforms and evidence hashes |
| `windows-extreme-release-8d3cba8c-evidence.zip` | Windows formal raw timings and check logs |
| `mac-extreme-release-8d3cba8c.tar.gz` | Mac formal raw timings and check logs |
| `linux-fe17f084-storage-native-evidence.tar.gz` | Linux address space, storage and regression evidence |
| `latency-8d3cba8c-stage-close-index.json` | Data index of candidate experiments and interrupted trial runs |
| `windows-stage-close-8d3cba8c-tests/ctest.log` | Windows native regression output of the stage close |
