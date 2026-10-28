# Platform validation evidence, 2026-10-28

This record describes the tested fixture configurations, not a blanket guarantee
for every OS version, compiler, device, or production game schema.

| Target | Compile layout | Actual runtime and wire exchange |
| --- | --- | --- |
| macOS arm64 | PASS | PASS_REFERENCE, self-check |
| macOS x64 | PASS | PASS_WIRE, Rosetta |
| Linux x64 | PASS | PASS_WIRE, Docker in a Mac VM, x64 emulation |
| Linux arm64 | PASS | PASS_WIRE, Docker in a Mac VM |
| iOS simulator arm64 | PASS | PASS_WIRE, iOS 26.4.1 simulator |
| Android x64 | PASS | PASS_MANUAL_WIRE, Android 35 x64 emulator |
| Android arm64 | PASS | PASS_MANUAL_WIRE, Android 35 x64 emulator with arm64 translation |
| iOS arm64 device | PASS | NOT_RUN, signed device app and device unavailable |
| iOS simulator x64 | PASS | NOT_RUN, x64 simulator runtime unavailable on this Mac |
| Windows x64 | PASS | NOT_RUN, available Windows Clang lacks reflection support |
| Windows arm64 | PASS | NOT_RUN, matching runtime and toolchain unavailable |

Compile PASS means the SLG benchmark catalog passed the compile-only layout
probe under the configured cross-target profiles. In particular, the Windows
probe used MinGW target C headers with the reflection compiler libc++ headers;
it is not a test of MSVC STL. The Android r28c compile sysroot is a partial
archive extraction with entry CRC checks, not a whole-archive checksum.

The Android experiment generated target IR with the Mac P2996 Clang and libc++
21 headers, configured to use the Android __ndk1 ABI namespace. Apple Clang 21
generated objects and Windows NDK 27 linked them against the NDK static libc++.
This mixed-toolchain experiment is evidence for these binaries and fixtures,
not proof of a supported shipping Android toolchain. Both Android executables
reported the correct target and source fingerprint, wrote the same ten files
as macOS arm64, read the macOS corpus, and were read by macOS. The two corpus
directories were compared byte-for-byte. ARM64 ran through native translation
on the x64 simulator, not on ARM hardware.

The full automated gate report predates the manual Android exchange and still
marks Android targets BLOCKED_RUNTIME. It requires live runnable profiles on
the gate host and does not accept a historical report as a substitute. The
Windows-hosted emulator was not reachable from the Mac gate through a
configured runner; no Android gate pass is claimed. Four device/simulator
targets are also unavailable. Until all eleven live profiles pass, build.sh
and run_checked.py refuse to execute their guarded command. Direct CTest and
direct binary execution bypass this gate.

The runtime corpus covers six generic cases and four SLG benchmark roots,
not arbitrary production game schemas. Test cases and manually collected
artifacts remain in the Mac results directories:
- results/schema-wire-gated-20261028-v3/schema-0611c212989f4358a3ac217ccc226500/
- results/schema-wire-android-exchange-20261028-v1/
- results/schema-wire-android-ir-20261028-v4/

The real runtime results compare each target with the macOS arm64 reference,
not every pair of non-reference targets directly. Full source fingerprint:
2f152112650b4040028202b82796638a31bc7cc8a4689053f48d32b44dea0dd5
