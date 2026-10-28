# Schema gate on next_cpp26: review merge validation (2026-10-28)

Source: handoff head 631d6f85 (8 commits on v3.1.0 9ef5ce75), merged with
next_cpp26 32ab499d, plus the review fixes in this branch. Host: Mac mini
arm64, macOS 27.2, P2996 Clang d77eff1 (clang 21), Xcode 27.1, Command Line
Tools MacOSX26.5 SDK. Wire fixture source fingerprint after the merge:
.

These results belong to this merged source only. The handoff's evidence
directories describe the pre-merge source and were not rerun here unless
listed below.

## Ported to next_cpp26

The core headers no longer include TypeLayout or define
detail::reflected_member_count. probe.cpp now includes <boost/typelayout.hpp>
itself; probe.cpp and wire_runtime.cpp count members with
schema::member_count_v. Before the port, five compile-gate tests failed with
FAIL_SEMANTIC on every catalog.

## Executed

| Check | Result |
| --- | --- |
| Python tests (tools/schema_guard, real P2996 compiler, no skips) | 53/53 pass |
| build.sh --schema-gate local (P2996 Release, MacOSX26.5 SDK) | CTest 160/160 pass, signature matrix and compat pass, tools/sigs unchanged |
| test_schema_guard_slg_values (in the CTest run and run alone) | pass |
| Compile gate, catalog_slg.hpp: macos-arm64, macos-x64, ios-arm64, ios-simulator-arm64, ios-simulator-x64 | 5/5 PASS_COMPILE (SLG pins unchanged by next_cpp26) |
| Compile gate, catalog_slg.hpp without the Hero field list | FAIL_SEMANTIC (reachable record field list missing) |
| Runtime matrix: macos-arm64 reference self-check | PASS_REFERENCE |
| Runtime matrix: macos-x64 (Rosetta) bidirectional read + byte comparison | PASS_WIRE |
| run_checked.py full gate with Linux/Android/Windows unavailable | stopped at the compile gate; command not started |
| git diff --check, bash -n build.sh | pass |

## Not covered by this run

- Linux x64/arm64 compile and wire: Docker was not started on this Mac (no
  Docker daemon during the run); BLOCKED here.
- Android, Windows x64/arm64 (native and Wine): no runner or cross profile on
  this Mac; BLOCKED here.
- iOS simulator wire: the fixture does not build with P2996 Clang 21 against
  the Xcode 27 iOS 27.1 SDK (INFINITY undefined via float.h); no older
  simulator SDK is installed. BLOCKED here. iOS device: no signed runner.
- The handoff's other uncovered items (native Windows arm64, physical devices,
  MSVC STL, production schemas) remain uncovered.

The full release gate therefore does not pass on this host, as intended:
missing targets never count as passed.
