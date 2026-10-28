# Wine and runtime-kind follow-up (2026-10-28)

The release gate continues to require native execution. Windows profiles must
declare runtime_kind as windows (actual Windows host/guest) or wine. The
declaration is trusted configuration and is not independently attested.
Successful Wine wire exchange is reported as PASS_WIRE_WINE and never satisfies
the Windows release gate.

## Full matrix run

A fresh run using the Windows x64 host, Android emulators, Apple simulator and
Linux containers produced the report below. All 11 compile-time targets passed;
that does not prove execution.

| Runtime target | Result |
| --- | --- |
| macOS arm64 | PASS_REFERENCE |
| macOS x64 | PASS_WIRE |
| Linux arm64 and x64 | PASS_WIRE |
| Android arm64 and x64 | PASS_WIRE |
| iOS arm64 simulator | PASS_WIRE |
| Windows x64 | PASS_WIRE |
| Windows arm64 via Debian Wine 10 | FAIL_RUNTIME (Wine initialization) |
| iOS device arm64 | BLOCKED_RUNTIME |
| iOS x64 simulator | BLOCKED_RUNTIME |

The Android arm64 fixture ran through x64 emulator translation; it was not a
physical arm64-device run. The macOS arm64 reference self-checks. PASS_WIRE means both directions
were read and byte-compared with the reference. The device and x64 simulator
remain blocked, and Wine is not a native Windows host. The full gate returned
failure and did not start the guarded command.

Evidence:
- Runtime report: /Users/netease/XOffsetDatastructure-20261027/results/schema-wire-wine-gate-20261028-v1/schema-77df98f1467543bb9bd4fa0765e03aac/runtime/REPORT.md
- Compiler report: /Users/netease/XOffsetDatastructure-20261027/results/schema-wire-wine-gate-20261028-v1/schema-77df98f1467543bb9bd4fa0765e03aac/REPORT.md
- Wine diagnostic: /Users/netease/XOffsetDatastructure-20261027/results/schema-win-arm-runtime-20261028-v1/wine-seh-diagnostic-20261028-v1.log

The Debian Wine 10 run encountered repeated illegal-instruction and
stack-overflow exceptions on a Linux ARM64 guest with PAC/BTI. This corresponds
to upstream fixes for ARM64 pointer authentication unwind and BTI dispatcher
entry points. A separately compiled Wine experiment is kept in the results
directory; it is not a platform pass or an official Wine release.

The scope is catalog_slg.hpp and its wire fixture, not all production schemas.
Android compile sysroot used partial archive extraction without full archive
SHA1 verification. Windows compilation used MinGW headers rather than MSVC
STL, and no real Windows ARM64, iOS device, or usable iOS x64 simulator is
established by this run.

## Patched Wine compatibility follow-up

A separate Wine 10 image with the upstream PAC-unwind fix and BTI dispatcher
entries successfully ran the Windows ARM64 fixture against the macOS arm64
reference in both directions. The matrix records PASS_WIRE_WINE, and its overall
status remains FAIL because this is not native Windows.

Report: /Users/netease/XOffsetDatastructure-20261027/results/schema-wine-patched-wire-20261028-v1/REPORT.md

The isolated build and diagnostic logs are under
/Users/netease/XOffsetDatastructure-20261027/results/schema-wine-fix-20261028-v1.
The experiment uses the upstream Wine 10.0 release plus the Debian server
temporary-directory patch, and a protocol header regenerated from the Debian
source. The Debian and upstream protocol definitions were byte-identical; the
generated headers differed only in their protocol version. This custom image
is an experiment, not an official release or a production runtime.

The GitHub-hosted Windows ARM64 workflow is prepared as a one-shot diagnostic
but was not pushed or run. It does not replace the live native Windows gate.

## Fresh complete gate after Wine fix

All 11 target compile checks passed. The runtime matrix returned PASS_REFERENCE
for macOS arm64; PASS_WIRE for Linux arm64/x64, macOS x64, iOS simulator arm64,
Android arm64/x64 and Windows x64; PASS_WIRE_WINE for Windows arm64 via the
experimental patched image; and BLOCKED_RUNTIME for iOS device arm64 and iOS
simulator x64. macOS x64 ran under translation, Android arm64 used x64 emulator
translation, and these are not physical-device results.

The complete gate returned exit 2; the guarded command marker was verified
absent. Windows arm64 is still uncovered on native Windows, despite the Wine
result.

- Compile report: /Users/netease/XOffsetDatastructure-20261027/results/schema-wire-wine-fixed-gate-20261028-v1/schema-e10a123f2efd483885e7a76dda69dd80/REPORT.md
- Runtime report: /Users/netease/XOffsetDatastructure-20261027/results/schema-wire-wine-fixed-gate-20261028-v1/schema-e10a123f2efd483885e7a76dda69dd80/runtime/REPORT.md
- Suite: 41 tests passed with the P2996 compiler enabled; no skips.

Before the push was declined, the synthetic Mac corpus and Windows test binary
were uploaded as two unreferenced Git objects to the public GitHub repository.
No branch was pushed, the workflow was not run, and GitHub may eventually
garbage-collect those objects. Refresh the inputs before any later run.
