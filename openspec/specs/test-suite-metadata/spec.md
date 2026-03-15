## ADDED Requirements

### Requirement: Test metadata consistency across sources
All test-suite metadata sources (build.sh, AGENTS.md, CMakeLists.txt, tests/README.md) SHALL report the same total test count and the same classification of basic vs reflection tests.

#### Scenario: Non-reflection build runs all basic tests
- **WHEN** `./build.sh --no-reflection` is executed
- **THEN** all 7 basic tests (including test_xbuffer_api) SHALL be executed

#### Scenario: Full build runs 23 tests
- **WHEN** `./build.sh` is executed with reflection enabled
- **THEN** exactly 23 tests SHALL be executed (7 basic + 16 reflection)

#### Scenario: AGENTS.md matches actual test count
- **WHEN** a developer reads the test suite summary in AGENTS.md
- **THEN** the documented count SHALL match the actual number of test files registered in CMakeLists.txt
