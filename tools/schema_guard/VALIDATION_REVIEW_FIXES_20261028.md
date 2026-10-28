# Review fixes and validation (2026-10-28)

The six review findings are addressed in the implementation. Work remains on
native platform coverage and on rebuilding older V1 platform fixtures.

- Platform builds use build.sh --schema-gate local by default. The dedicated
  release invocation is build.sh --schema-gate full (or run_checked.py); it
  still requires every compile/runtime entry. The existing Linux CI build
  invocation can reach its local CTest without Mac SDK or remote runners.
- XSG_RUNTIME_V2 reports Windows/Wine as detected inside the target process.
  Misdeclared Wine is rejected before wire exchange. This assumes trusted
  fixture source and runner scripts; it is not protection against a malicious
  replacement executable.
- Reachable record field lists are required recursively, including container
  elements, map keys/values and arrays.
- SLG reads compare all reflected loaded field values, strings, container
  contents and map query results with the constructed reference roots.
- Build manifests bind source, canonical commands, compiler/backend/linker
  files, SDK/runtime inputs and binary digests. Windows link inputs are checked
  live on Windows via the relay, while frontend inputs are checked on Mac.
- Each exchange receives four step timeouts plus a margin. A regression test
  checks the outer budget and propagation of the per-step timeout.

The build ID and expected policy are supplied by trusted build tooling. The
caller must include any additional inputs used by custom wrappers. These
checks do not assert the provenance of arbitrary unregistered wrapper logic.

## Checks completed

- 50 Python tests passed with P2996 integration tests enabled, no skips.
- The new C++ semantic regression executable compiled and ran on macOS arm64.
  It detected altered nested heroes, map query values, battle events, march
  troops and report text, and passed the six generic wire self-tests.
- The C++ semantic regression is registered as test_schema_guard_slg_values
  in CTest. The whole existing CTest suite and remote GitHub workflows were not
  rerun in this session.
- bash syntax/help and git diff whitespace checks passed.
- All 11 cross-target layout compile checks passed.
- The exact missing-Hero review reproduction now reports FAIL_SEMANTIC.
- Real Wine execution declared as Windows now reports FAIL_IDENTITY.

## Fresh V2 runtime results

| Target | Result |
| --- | --- |
| macOS arm64 | PASS_REFERENCE |
| macOS x64 | PASS_WIRE (translation) |
| Linux x64 / arm64 | PASS_WIRE (Docker) |
| iOS simulator arm64 | PASS_WIRE |
| Windows x64 | PASS_WIRE (local Windows, live link-input verification) |
| Windows arm64 | PASS_WIRE_WINE (patched Wine, live Windows link-input verification) |
| iOS device arm64 | BLOCKED_RUNTIME |
| iOS simulator x64 | FAIL_RUNTIME (installed simulator architecture mismatch) |
| Android x64 / arm64 | BLOCKED_RUNTIME (V2 fixtures not rebuilt this session) |

Windows arm64 has not run on native Windows. Previous Android V1 pass reports
remain historical evidence; they are not accepted as current V2 results.
The complete release gate still returns failure. Its guarded command marker
was verified absent. The final runtime-only matrix also returns failure.

Evidence preserved on Mac:
- /Users/netease/XOffsetDatastructure-20261027/results/schema-review-20261028-v1/full-gate-v2/schema-6fc1f5a25a0f4663a6ea42ba173f9efe/REPORT.md
- /Users/netease/XOffsetDatastructure-20261027/results/schema-review-20261028-v1/final-runtime-v3/REPORT.md
- /Users/netease/XOffsetDatastructure-20261027/results/schema-review-20261028-v1/missing-hero-now-rejected/REPORT.md
- /Users/netease/XOffsetDatastructure-20261027/results/schema-review-20261028-v1/mislabeled-runtime-v2/REPORT.md

No branch or workflow was pushed. The checked-in GitHub one-shot inputs are
historical and require regeneration before any future execution.
