# 3.2.0-rc.1 cross-platform functional test record (2026-10-28)

**Source identity**: `v3.2.0-rc.1` (annotated tag object `6bced0b6a493a50f078916b74f64df747b5e893c`) →
commit `39de6364563d89f0ef745a23d1937c7f212f9a3c`. The results on this page belong to this commit only.
When this page was written, `next_cpp26` had already merged #166 (59012329), #163 (964b2bc0), #167
(32ab499d) and others after 39de636; those commits did not go through this round of tests and cannot
reuse the conclusions here.

Functionality only, no timing: no performance conclusions, no iPhone, no bare-metal Linux (see section 5).

## 1. Evidence and verification

The raw evidence is the handoff package `xoffset-3.2.0-rc1-handoff-20261028.zip` (12,591,750 bytes,
SHA-256 `44a368e49c4a581ad72fd3573e7f6f2717b44120358306d1fb76bd2b68a6d484`), kept at the root of the
project files. The package contains device serial numbers and local paths of the test machines, so the
raw logs do not go into the repository. Files in the package (each listed in `SHA256SUMS`):

| Path | Contents | SHA-256 (first 16) |
| --- | --- | --- |
| `platform-rc1-20261028/REPORT.md` | The tester's conclusions | `4246f3c60dbe4128` |
| `platform-rc1-20261028/windows-results.tar.gz` | Windows: the failed `windows-cxx20-run2`, the passing `windows-cxx20-run3` and `windows-cxx23-run3` | `705b4c91b2a29cf4` |
| `platform-rc1-20261028/macos-results.tar.gz` | macOS C++20, C++23 | `ce0539491d9deb52` |
| `platform-rc1-20261028/macos-reflection-results.tar.gz` | macOS C++26 reflection | `e6560cfa89626044` |
| `platform-rc1-20261028/linux-remote-results.tar.gz` | Remote Linux C++20 | `45253508bccbb469` |
| `platform-rc1-20261028/linux-server-wsl-results.tar.gz` | WSL2 Docker GCC 16 C++26 reflection | `1ac248b88e65764a` |
| `android-3.2.0-rc1-20261028/out-20261028T082542Z.tar.gz` | Android run 1 (demo not run) | `e9204ad95b914508` |
| `android-3.2.0-rc1-20261028/out-20261028T082851Z.tar.gz` | Android run 2 (demo replay failed) | `15a914b0565448a4` |
| `android-3.2.0-rc1-20261028/out-20261028T083435Z.tar.gz` | Android final (all passed) | `800ad40358f364dc` |
| `supplement/ref-corpus-rc1.tar.gz` | 6 reference wires (written by WSL2 GCC 16.2 C++26 reflection) | `770b272e62a4abd8` |
| `supplement/*-supplement.tar.gz`, `android-build-evidence.tar.gz` | Each build's CMakeCache and `LastTest.log`, Android build logs | |
| `supplement/source-used-wsl-export.tar.gz` | Copy of the source actually used by the WSL2 and Android builds (no `.git`, with `.xo-commit`) | `2c6d60da3e156932` |

Verification on the Mac on 2026-10-28 (read only; the scripts in the package were not executed):

- `sha256sum -c SHA256SUMS`: all 27 files OK; the hashes of the 6 result packages listed in `REPORT.md`
  match. The `SHA256SUMS` inside the Android final package, 229 entries, are all OK; the two failed
  packages' 8 and 223 entries are all OK.
- The corpora written by the 7 builds (Windows ×2, macOS ×3, remote Linux, WSL2) were compared file by
  file with the reference wires using `cmp`; all 6 files are identical. The Android final package's
  `corpus.sha256` matches the reference hashes, and all 6 lines of `corpus-read.log` say "accepted and
  byte-identical".
- The summary line of each `ctest.log` matches the table below; the 8 `test-inventory.json` files split
  into two groups by standard, identical within each group.
- The source copy compared with `git archive 39de636`: no differences except `.xo-commit` (whose content is
  39de636), `tools/__pycache__` and submodules; `external/typelayout` matches the content of submodule
  commit fe295929. The Boost subset was not checked file by file.
- `android_device.sh`, `native_targets.sh` and `platform_matrix.sh` in the package are byte-identical to
  the files of the same names at 39de636.

## 2. Results

| Environment | Compiler and standard | CTest | Reference wires | demo | Result package |
| --- | --- | --- | --- | --- | --- |
| Windows 11 x64, physical machine | MSVC 19.44.35229 (toolset 14.44.35207), Ninja, Release, `/MT`, `/utf-8`; C++20, C++23 | 129/129 each | All 6 read and accepted, written byte-identical | Not run | windows-results |
| macOS arm64, physical machine (SDK 26.4, Darwin 25.4.0) | Apple clang 21.0.0 (clang-2100.0.123.102), deployment target 14.0; C++20, C++23 | 129/129 each | Same as above | Not run | macos-results |
| The same Mac | clang-p2996 9ffb96e3 (clang 21.0.0git), C++26 reflection | 158/158 | Same as above | Not run | macos-reflection-results |
| Remote Linux x86-64 (recorded by the tester as a KVM virtual machine) | Clang 20.1.2 from Zig, C++20 | 129/129 | Same as above | Not run | linux-remote-results |
| WSL2 Linux x86-64 (6.6.87.2-microsoft-standard-WSL2), Docker | GCC 16.2.0 (image pinned by digest), C++26 reflection | 158/158 | The build that wrote the reference reads and writes it itself, byte-identical | Server A/B | linux-server-wsl-results |
| Xiaomi M2102K1C, Android 11 (API 30), arm64-v8a, 4 KiB pages | NDK r29 (29.0.14206865) Clang 21, API 28, C++20 | 114/114 on the phone, compile-failure tests 15/15 on the computer | All 6 read and accepted on the phone, written byte-identical | android and android-noexc both PASS | out-20261028T083435Z |

Android demo (the final package's `demo/E2E_RESULT.md`): servers A/B are the GCC 16 C++26 reflection build
in WSL2 Docker, the two clients run on the phone and connect to A through Windows adb's `adb reverse`.
Both clients pass self-test 23/23, replay 60/60 identical digests (dense, compact and zstd 20 each), 500
steps each at 1/100/10000 units and the malicious inputs (android 74 verdicts, android-noexc 75), and
android-noexc's step wires on all three tiers equal the reference host's. Times in the table are single
functional runs on this phone and are not performance data.

OS versions and machine types in the environment column that the logs do not contain (Windows 11, the
remote Linux distribution and virtualization) are taken from the tester's `REPORT.md`.

## 3. Test counts cannot be added

| Build | Test programs | Subprocess scenarios | Tool checks | CTest | Configure-time checks (separate) |
| --- | ---: | ---: | ---: | ---: | ---: |
| C++20 / C++23 field list | 110 | 18 | 1 | 129 | 3 |
| C++26 reflection | 133 | 20 | 5 | 158 | 3 |

Neither set contains the other: the C++26 build has 35 items the C++20/23 builds do not, and C++20/23
have 6 items (`receive_raw_nonfixed_*_fail`) the C++26 build does not. `cxx26_only_not_built` lists the 31
programs and tools not compiled in the field-list build (including `export_signatures`, which is not in
CTest); it does not equal the CTest difference. So 129 and 158 each only show a full pass on their own
backend and cannot be added up into a coverage figure. Android's 114 + 15 is the same 129-item list
(`android-build-evidence/android/test-inventory.json`) split into what ran on the phone and what ran on
the computer.

## 4. Failed runs (kept, not overwritten)

- **Windows `windows-cxx20-run2`**: configure passed, build exited with code 2. A non-ASCII character in
  `tests/test_memory_efficiency.cpp` line 35 was read as GBK under code page 936, giving C2001/C3688; the
  library headers also gave C4819 warnings. `run3` reran C++20 and C++23 in a new directory with
  `-DCMAKE_CXX_FLAGS=/utf-8`, and all passed. The package has no `run1` directory; the tester's report
  calls run2 the first build, without explaining the numbering.
- **Android run 1 (08:25Z)**: library and wire passed; `adb reverse tcp:48024` failed because the port
  was taken on the phone (47800–48049 falls in the phone's ephemeral port range); the demo did not run,
  exit code 3.
- **Android run 2 (08:28Z)**: ports moved to start at 17800; library and wire passed; all 60 replay
  snapshots were missing: Windows adb's path conversion dropped the trailing `/.` of the directory, the 61
  corpus files were pushed one directory level too deep on the phone, and the client reported
  `cannot read /data/local/tmp/xo/corpus/corpus.csv`, exit code 2. Self-test, the three tiers and the
  malicious inputs all passed; the 2 failed verdicts both belong to replay (the report says "tiers: 2 of
  74 verdicts failed", which is imprecise).
- **Android final (08:34Z)**: reran all three steps with the fixed adb wrapper, exit code 0.

The adaptations are only in the test environment: `android_device-wsl-ports.sh` changes only 3 port lines
compared with the repository script (47800 → 17800), and `adb-windows-fixed` converts WSL paths into
Windows paths and keeps `/.`. Library code and tests were not changed. "commit unknown" in the Android
demo summary and "exported" in the WSL2 platform report come from the scripts excluding `.git` when
copying the source; the source identity is in section 1.

## 5. Scope not covered

- Performance, memory, package size: not tested.
- Physical iPhone: not tested (iOS only has simulator records, see `docs/CLIENT_TARGETS.md`).
- Bare-metal Linux: the remote Linux is a virtual machine; C++26 reflection on Linux only ran in WSL2
  Docker, and the remote machine had no reflection build. Linux arm64 did not run this round.
- Cross-device demos on Windows and macOS: not run, only the library CTest and wire corpora. Windows only
  tested MSVC; clang-cl and MinGW did not run on the physical machine; Windows ARM64 and macOS x86-64 did
  not run.
- Android: only one phone (Android 11, 4 KiB pages); 16 KiB page devices, other Android versions and NDK
  r30 did not run on a physical device. The demo's server ran in WSL2 Docker, not on a separate Linux
  server.
- No sanitizer runs; no Mac checkpoint (preview release, see the release notes).
- Hardware models (the Windows i9-13900K, the Mac M4 Pro, the remote Linux's Debian and KVM) come only
  from the tester's `REPORT.md`: the result pages of `native_targets.sh` do not record the machine, and
  the raw logs do not contain this information.

## 6. Issues found and follow-up

| Issue | Handling |
| --- | --- |
| MSVC fails to compile the tests on non-UTF-8 code pages (`/utf-8` was not in the project) | The project adds `/utf-8` for MSVC; 9 comment lines of `xoffsetdatastructure.hpp` were changed to ASCII, so public headers no longer depend on the source character set |
| `android_device.sh`'s demo ports 47800–48049 are inside Android's ephemeral port range | Default changed to 17800, configurable with `XO_PORT_BASE` |
| On Android 11 the SoC in `device.txt` is empty (`ro.soc.*` exists only from API 31) | When empty, record `ro.board.platform` and `ro.hardware` instead |
| With exported source (no `.git`) the result page cannot record the commit | `platform_matrix.sh`, `native_targets.sh` and `run_tier.sh` read `.xo-commit` |
| `native_targets.sh`'s result page does not record the machine | Added `machine: uname -srm` |
| `e2e_report.py` recorded every failed verdict as "tiers" | Changed to "verdicts: N of M failed (scope ...)" |
| A Windows host (WSL2 + Windows adb) is not a host `android_device.sh` supports | This round worked around it with the adb wrapper in the package; whether to support it formally is undecided, and `docs/DEVICE_TESTING.md` states this for now |
| The hardware information in the tester's report has no raw record | See section 5; later result pages carry a `machine` line |

All of these changes came after 39de636 and only count as validated after a rerun on the next rc.
