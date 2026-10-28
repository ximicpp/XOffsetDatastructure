# Windows runtime verification, 2026-10-28

Full compile and runtime gate result:

| Target | Compile layout | Runtime and wire |
| --- | --- | --- |
| macOS arm64 | PASS_COMPILE | PASS_REFERENCE |
| macOS x64 | PASS_COMPILE | PASS_WIRE, Rosetta |
| Linux arm64 | PASS_COMPILE | PASS_WIRE, container in Mac VM |
| Linux x64 | PASS_COMPILE | PASS_WIRE, emulation in Mac VM |
| iOS simulator arm64 | PASS_COMPILE | PASS_WIRE |
| Android x64 | PASS_COMPILE | PASS_WIRE, Windows emulator |
| Android arm64 | PASS_COMPILE | PASS_WIRE, translated on x64 emulator |
| Windows x64 | PASS_COMPILE | PASS_WIRE, native Windows x64 |
| Windows arm64 | PASS_COMPILE | BLOCKED_RUNTIME |
| iOS arm64 device | PASS_COMPILE | BLOCKED_RUNTIME |
| iOS simulator x64 | PASS_COMPILE | BLOCKED_RUNTIME |

The Windows x64 binary was cross-compiled using the Mac P2996 compiler and
Apple Clang 21 backend, linked on Windows with Android NDK LLVM 18 and
checksum-verified static MSYS2 MinGW libc++ and runtime packages, and executed
on Windows x64. The gate transferred ten reference and target corpus files
through the nodecli relay, tested both read directions, and compared bytes.
The complete matrix remained failed and did not start the guarded command.

The Windows arm64 object and executable were also built and linked, but no
Windows arm64 system is available. An ARM64 Debian 13 container using Wine 10
was attempted as a supplementary compatibility test. Wine initialization
remained busy in wineboot for over two minutes; a separate bounded strace run
timed out. Neither run reached fixture identification or wire exchange.
Wine would not by itself establish a native Windows result even if it worked.

No iPhone was found by devicectl and no signed device runner is configured.
The available Apple Silicon simulator runtime does not run the x64 simulator
target. The Android ARM64 result uses translation on an x64 simulator.
The compile matrix uses cross-target C headers and the reflection compiler
libc++ headers, not a Microsoft STL validation; the partially extracted Android
NDK sysroot lacks whole-archive checksum verification. The wire corpus covers
six generic cases and four SLG benchmark roots, not production game schemas.
Direct CTest and direct execution bypass build.sh and run_checked.py.

Mac evidence:
- results/schema-wire-windows-gate-20261028-v1/schema-57e1975d8abe4ef0859976ab3dd27b37/
- results/schema-wire-windows-builder-20261028-v2/
- results/schema-win-preparer-20261028-v1/
- results/schema-win-arm-runtime-20261028-v1/

The Windows runtime directory and cross-node relay require explicit setup each
time; previous passing reports are not substituted for a live run.
