## ADDED Requirements

### Requirement: Documentation test counts match code

All documentation files SHALL display test counts and test names consistent with the authoritative source (`tests/CMakeLists.txt`).

#### Scenario: Test count accuracy
- **WHEN** a user reads any documentation file (`docs/QUICK_REFERENCE.md`, `docs/README.md`, `docs/ZERO_BOILERPLATE.md`)
- **THEN** the stated total test count SHALL be 23 (7 basic + 16 reflection), matching `tests/CMakeLists.txt`

#### Scenario: Test name accuracy
- **WHEN** a user reads `docs/QUICK_REFERENCE.md` test matrix
- **THEN** every test name listed SHALL correspond to an actual `.cpp` file in `tests/`

### Requirement: Docker command correctness

Root `README.md` Docker examples SHALL use `bash ./build.sh` instead of bare `./build.sh`.

#### Scenario: Docker quick start command
- **WHEN** a user copies the Docker run command from `README.md`
- **THEN** the command SHALL include `bash` prefix to avoid permission errors
