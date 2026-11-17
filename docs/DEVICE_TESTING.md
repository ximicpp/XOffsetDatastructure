# Real-device testing (4.0.0)

This page describes real-device testing for 4.0.0: pull this version on a clean computer, connect the
phone over USB, run one command, and send back the result file.

Sections 1–6 cover an **Android phone + Linux server**: functional checks, SLG timing and memory limits;
section 7 covers the **iPhone**: library tests, wire corpus, SLG timing and memory limits.

## 1. What is tested

`tools/device/android_device.sh` does four things on the phone. The first two are the same checks that
already pass on the Android emulator; the last two are performance and memory on the phone:

1. **Library**: the full CTest of the C++20 field-list build (Android NDK r29, arm64-v8a), each test
   program pushed with adb to the phone's `/data/local/tmp/xo` and run there; then the wire corpus
   check: the phone reads the reference wires written by the Linux server build (GCC 16, C++26
   reflection), which must all be accepted, and the bytes the phone writes must be byte-identical to the
   reference. The 15 compile-fail tests (cases that must fail at compile time) need the NDK and run on
   the computer.
2. **Game demo** (`demo/mapstate`): Linux servers A/B (GCC 16, C++26 reflection) run in docker
   containers on the computer; the demo clients (builds with and without exceptions) run on the phone and
   connect to the computer through `adb reverse` (127.0.0.1:17800–18049 on the phone is the computer).
   Each client runs the self-test, replays the snapshot corpus A wrote, runs 500 steps each with 1, 100
   and 10000 units (rotating dense, compact and zstd snapshots), and malicious input.
3. **slg timing** (SC-6): the frozen `bench/slg` runs on the field-list build, with FlatBuffers 25.12.19
   for comparison. First verify, then time 3 times (scale M, 41 rounds), recording the thermal state and
   battery level before each run. `XO_SLG_RUNS`, `XO_SLG_SCALE` and `XO_SLG_CYCLES` can be changed. While
   timing, keep the charger plugged in, close other apps and keep the screen on.
4. **Memory limits** (SC-12): `xo_mem_probe` measures the largest `maximum_capacity` of a single buffer
   and how much address space can be held at once, then commits memory in 64 MiB steps until the system
   kills it (the default limit is the phone's MemTotal; change it with `XO_MEM_LIMIT`). This step
   temporarily makes the phone sluggish and background apps get closed; that is expected.

## 2. Preparing the computer

macOS (Apple silicon or Intel) or Linux (x86-64, arm64). Leave 8 GB of disk space and make sure
github.com, dl.google.com and Docker Hub are reachable (x86-64 uses the `mirror.gcr.io` mirror).

**macOS**:

```sh
xcode-select --install                       # git, python3 (skip if Xcode is installed)
brew install cmake ninja colima docker       # without Homebrew, install it first: https://brew.sh
colima start --cpu 4 --memory 6 --disk 40    # or use Docker Desktop
```

**Linux**: git, python3, unzip, ninja, docker (the current user can run `docker info`), CMake 3.30 or
newer (when the distribution's is too old: `pip3 install --user cmake`).

**Windows**: the script does not support it directly. The 3.2.0-rc.1 real-device results were obtained by
running the script inside WSL2 (Ubuntu 22.04) and connecting to the phone through Windows' `adb.exe`, with
a separate adb wrapper: it converts WSL paths to Windows paths with `wslpath -w` and keeps the trailing
`/.` of directories (without it, `adb push` puts the corpus one directory level deeper and the demo replay
cannot find `corpus.csv`). The approach and the wrapper script are in section 4 of
[validation/2026-10-28-rc1-platform/README.md](validation/2026-10-28-rc1-platform/README.md).

You do not need to install adb and the NDK yourself: when they are missing, the script downloads
platform-tools and NDK r29 (about 1 GB, SHA-1 checked) from dl.google.com to `~/xo-device/sdk`. If they
are already installed, put `adb` on PATH or set `ADB=`, and set `ANDROID_NDK=` for the NDK.

## 3. Preparing the phone

Requirements: 64-bit (arm64-v8a), Android 9 or newer.

1. Settings → About phone → tap "Build number" 7 times to turn on developer options.
2. In developer options turn on "USB debugging"; also turning on "Stay awake" (keep the screen on while
   charging) is recommended, so long runs are not interrupted by sleep. Xiaomi, OPPO, vivo and others
   may have "USB debugging (Security settings)"; it does not need to be on: the script only runs programs
   under `/data/local/tmp` and installs no app.
3. Connect the phone to the computer with a USB data cable, unlock it, and when "Allow USB debugging?"
   appears, choose "Always allow" and confirm.

## 4. Pulling this version

```sh
git clone --branch v4.0.0 --depth 1 https://github.com/ximicpp/XOffsetDatastructure.git xoffset-4.0.0
cd xoffset-4.0.0
cat XOFFSET_VERSION          # 4.0.0
git rev-parse HEAD           # matches the commit in the release record
```

Submodules (TypeLayout and the boost headers used) are fetched by the script as needed.

## 5. Running

```sh
bash tools/device/android_device.sh
```

It runs setup (checks tools, downloads adb/NDK if missing), build (flatc in docker, the Android programs,
the server and reference corpus in docker) and run (the five steps on the phone) in order. The first run
takes about 30–60 minutes, mostly compiling. The stages can be run separately:

```sh
bash tools/device/android_device.sh setup
bash tools/device/android_device.sh build    # build once
bash tools/device/android_device.sh run      # run once per phone
```

With several phones connected, look up serial numbers with `adb devices -l`, then run them one at a time
with `ANDROID_SERIAL=<serial> bash tools/device/android_device.sh run`. When done,
`bash tools/device/android_device.sh clean` removes the containers and the temporary directory on the
phone. If a run fails because the phone disconnected or locked midway, just `run` again; no rebuild is
needed.

Exit codes: 0 all passed, 3 some step failed (see RESULT.md), 2 missing tools or no phone connected
(nothing ran).

## 6. Sending back results

At the end the script prints where the result package is, like:

```
result: ~/xo-device/4.0.0-<first 7 of commit>/out-<UTC time>/RESULT.md; send back ~/xo-device/4.0.0-<first 7 of commit>/out-<UTC time>.tar.gz
```

Send **`out-<UTC time>.tar.gz`** back to the project, and say which phone model it is. The package
contains:

| File | Contents |
| --- | --- |
| `RESULT.md` | The verdict of each step (read this first) |
| `device.txt` | Model, SoC, Android version, ABI, page size, memory |
| `ctest.log`, `host_compile_tests.txt` | CTest on the phone, compile-fail tests on the computer |
| `corpus-*.log`, `corpus.sha256` | Wire corpus check |
| `demo/E2E_RESULT.md` and directories | Each demo client's self-test, replay, three tiers, malicious input |
| `slg/ANALYSIS.md`, `slg/run_*.jsonl`, `slg/info_*.txt` | slg timing table, raw data, thermal state and battery before each timing run |
| `mem/SUMMARY.md` and files | Memory probe results, low-memory kill logs |
| `SHA256SUMS` | Checksums of the files above |

What passing looks like: in RESULT.md the five lines `library CTest (phone)`, `wire corpus`, `slg`,
`mem` and `demo` all say passed, and the compile-fail test line shows everything passing.

## 7. iPhone

An iPhone cannot run command-line programs directly: `tools/device/ios_device.sh` builds each test as a
module inside the test host app (`tools/test_host`), signs it with the Apple ID signed in to Xcode,
installs it on the phone, runs one at a time, and collects results from the app's data container. **This
step has run on the iOS simulator but not yet on an iPhone.**

Preparation (one Mac):

1. Install Xcode; `export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer`; have CMake 3.30 or
   newer and ninja on PATH (`brew install cmake ninja`).
2. Sign in with an Apple ID in Xcode → Settings → Accounts (a free account works; signatures are valid
   for 7 days).
3. Connect the iPhone to the Mac with a data cable and tap "Trust This Computer"; once it has appeared in
   Xcode's Devices window, open Settings → Privacy & Security → Developer Mode and restart as prompted.
4. Set Auto-Lock to "Never" and plug in the charger.

Run (in the pulled directory):

```sh
bash tools/device/ios_device.sh device out-iphone --steps build,info,ctest,slg,mem
```

- With several iPhones connected, look up the UDID with `xcrun devicectl list devices` and add
  `--udid <UDID>`.
- Wire corpus check: add `,corpus` to the steps and `--ref <reference directory>`. The reference
  directory is the `~/xo-device/4.0.0-<first 7 of commit>/ref-corpus` produced by the Android script in
  section 5 (written by Linux GCC 16 C++26).
- To compare `slg` with FlatBuffers, add `--fb-include <include directory of flatbuffers 25.12.19>` and
  `--fb-generated <directory containing slg_generated.h>`; without them only XOffset is measured.
- Only the `net` and `clients` steps use `--server-host` (the Mac's address on the same Wi-Fi:
  `ipconfig getifaddr en0`); the first time a "Local Network" permission prompt appears; tap allow within
  3 minutes. The demo on iPhone is not among the steps in this version.

Sending back: `tar -czf out-iphone.tar.gz out-iphone`, and send the package to the project. Read
`RESULT.md` first; the rest are `info.txt`, the ctest log, `slg/ANALYSIS.md` and `run_*.jsonl`, and
`mem/SUMMARY.md`. The `mem` commit probe being killed by the system (rc 125) is expected: that is exactly
the memory limit being measured.

## 8. Common problems

- **`adb devices` shows unauthorized**: the phone did not allow USB debugging. Unlock the phone and
  replug; if the prompt still does not appear, use "Revoke USB debugging authorizations" in developer
  options and plug in again.
- **no permissions on Linux**: udev rules are missing. Install the distribution's
  `android-sdk-platform-tools-common` (Debian/Ubuntu) or `android-udev-rules` (Arch) and replug.
- **The container cannot write the working directory**: the working directory defaults to `~/xo-device`,
  and Docker Desktop and colima share the home directory by default; if you change `XO_WORK`, put it in
  a shared directory.
- **Port in use**: ports 17800–17849, 17900–17949 and 18000–18049 on the computer's 127.0.0.1 must be
  free, and so must the same ports on the phone (`adb reverse` listens on the phone). When they are taken,
  switch to another set with `XO_PORT_BASE=<another start>`; the start must be below 32768: Android's
  ephemeral ports (usually 32768–60999, 37000–50000 on some vendors) can be taken by the phone's own
  connections at any time. The 3.2.0-rc.1 script started at 47800, inside that range, and
  `adb reverse tcp:48024` failed on a Xiaomi phone; when testing with rc.1, change the 47800 start in
  `PORTS` on line 52 and `MAPDEMO_PORT_BASE` on line 344 of the script to 17800.
- **Re-running only one step**: `XO_SKIP="ctest corpus slg mem" bash tools/device/android_device.sh run`
  runs only the demo.
- **No docker to build flatc**: use the `slg_generated.h` produced by `run_phone_mac.sh fb` on a Mac with
  `XO_FB_GENERATED=<its directory>`; `XO_NO_FB=1` runs slg without FlatBuffers.

## 9. Existing results

| Version | Phone | Computer | Result | Record |
| --- | --- | --- | --- | --- |
| 3.2.0-rc.1 (39de636), 2026-10-28 | Xiaomi M2102K1C, Android 11 (API 30), arm64-v8a | Windows 11 + WSL2, server in WSL2 Docker | Compile-fail tests 15/15, phone CTest 114/114, 6 reference wires byte-identical, both demo clients E2E PASSED (the first two attempts failed because of ports and the adb path respectively; records kept) | [validation/2026-10-28-rc1-platform/README.md](validation/2026-10-28-rc1-platform/README.md) |
| a473fb27 (after 3.2.0-rc.2, with #178, #181), 2026-10-28 | iPhone 15 Pro Max (iPhone16,2), iOS 26.6.2, 8 GB, signed with a free Apple ID, USB | Mac mini M4, server in colima (GCC 16, arm64) | Test host app with 104 libraries; CTest 138/138; all 24 reference wires accepted, written bytes identical; slg (SC-6) verify passed, 3 runs × M scale × 41 cycles, thermal state 0; memory probe (SC-12) see [DEPLOYMENT.md](DEPLOYMENT.md) section 5; both demo clients E2E PASSED. The 5th attempt did not fully pass because of fork and slg address space, and the 6th regressed in resident mode, recorded under section 7 | Table below |
| 8cf9e16c (= the content of ea7edd3a merged by #185, with #183), 2026-10-29 | Same as above | Same as above | Resident mode (one app runs all programs in sequence; slg timing and the memory probe each use a freshly launched app): CTest 138/138; 24 reference wires byte-identical; slg verify passed, 3 runs × M × 41 cycles, thermal state 0; memory probe passed; both demo clients E2E PASSED; no CoreDeviceError 7000. The 8th attempt (084d4859) did not fully pass because of `.ready` copy errors (fixed by #185) | Test host section of [CLIENT_TARGETS.md](CLIENT_TARGETS.md) |

SC-6 on the iPhone 15 Pro Max (slg M scale, 3 runs pooled, xoffset against FlatBuffers 25.12.19 on the
same device, geometric mean / one-sided 95% upper bound, < 1 means xoffset is faster; player, battle and
report buffers reserve 2 MiB each, `SLG_REUSE=1`):

| Scenario | xoffset ÷ FlatBuffers | xoffset median (ms per cycle) | FlatBuffers median (ms per cycle) |
| --- | --- | ---: | ---: |
| A battle server round trip | 0.305 / 0.322 | 0.609 | 2.176 |
| B cross-server migration | 0.179 / 0.183 | 1.752 | 9.890 |
| C zone snapshot | 0.252 / 0.267 | 0.294 | 1.211 |
| D read-only battle report | 0.858 / 0.909 | 0.072 | 0.081 |

Against FlatBuffers reading in place after verification (no copy, no unpacking), D is 1.204x for
xoffset (reference only). Wire bytes (per-cycle totals): B's player state is about 64% larger for xoffset
and D's battle report about 25% larger. Phone numbers are compared only with the same phone, not with the
Mac gate numbers.
