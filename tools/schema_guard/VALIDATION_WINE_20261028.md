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
