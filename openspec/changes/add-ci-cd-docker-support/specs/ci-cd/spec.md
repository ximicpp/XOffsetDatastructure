# CI/CD Capability Specification

## ADDED Requirements

### Requirement: Automated Build and Test Pipeline
The system SHALL provide automated continuous integration and deployment pipeline that builds and tests the project on every push and pull request.

#### Scenario: Successful build and test on push
- **WHEN** a developer pushes code to any branch
- **THEN** the CI pipeline automatically triggers
- **AND** the Docker environment is set up with Clang P2996
- **AND** all source code is compiled successfully
- **AND** all 18 tests (6 basic + 12 reflection) are executed
- **AND** test results are reported in the GitHub UI

#### Scenario: Pull request validation
- **WHEN** a pull request is created or updated
- **THEN** the CI pipeline runs automatically
- **AND** PR status shows pass/fail based on build and test results
- **AND** developers can see detailed logs for any failures

#### Scenario: Build failure notification
- **WHEN** the build or tests fail
- **THEN** the CI status shows as failed
- **AND** error logs are accessible in the GitHub Actions UI
- **AND** the commit status reflects the failure

### Requirement: Docker-Based Build Environment
The system SHALL provide a Docker container with pre-built Clang P2996 compiler for consistent and reproducible builds.

#### Scenario: Docker image provides complete build environment
- **WHEN** the Docker image is built
- **THEN** it contains Clang P2996 with reflection support
- **AND** it includes all necessary build tools (CMake, Ninja, Git)
- **AND** it includes Boost dependencies
- **AND** it has proper environment variables configured (CC, CXX, PATH, LD_LIBRARY_PATH)

#### Scenario: Local development with Docker
- **WHEN** a developer runs the Docker container locally
- **THEN** they can build the project without installing Clang P2996 on their host
- **AND** they can run all tests inside the container
- **AND** they can mount their local source code for iterative development

#### Scenario: CI uses cached Docker image
- **WHEN** the Dockerfile hasn't changed
- **THEN** the CI reuses the cached Docker image
- **AND** the CI build completes in under 15 minutes (excluding first-time setup)
- **AND** the Clang P2996 build step is skipped

### Requirement: Multi-Stage Docker Build
The system SHALL use a multi-stage Docker build to minimize image size and build time.

#### Scenario: Builder stage compiles Clang P2996
- **WHEN** the Docker image is built from scratch
- **THEN** the builder stage clones Bloomberg's Clang P2996 repository
- **AND** it compiles Clang with CMake and Ninja
- **AND** it installs Clang to `/opt/clang-p2996`
- **AND** the build uses configuration flags matching `scripts/build_clang_p2996_wsl.sh`

#### Scenario: Runtime stage contains minimal dependencies
- **WHEN** the runtime Docker stage is created
- **THEN** it copies only `/opt/clang-p2996` from the builder stage
- **AND** it installs minimal runtime dependencies
- **AND** the final image size is under 5 GB
- **AND** it excludes build artifacts and intermediate files

### Requirement: Build Status Visibility
The system SHALL display CI/CD build status prominently in the repository.

#### Scenario: README shows build status badge
- **WHEN** a developer views the README.md
- **THEN** they see a CI status badge (passing/failing)
- **AND** clicking the badge navigates to the latest CI run
- **AND** the badge reflects the status of the default branch

### Requirement: Support for Both WSL and Docker Workflows
The system SHALL support both WSL-based and Docker-based development workflows without requiring developers to choose one exclusively.

#### Scenario: WSL workflow remains functional
- **WHEN** a developer uses the existing WSL workflow
- **THEN** the `build.sh` script works as before
- **AND** the `scripts/build_clang_p2996_wsl.sh` script is unaffected
- **AND** WSL documentation remains accurate

#### Scenario: Docker workflow is documented
- **WHEN** a developer wants to use Docker
- **THEN** README.md contains clear Docker setup instructions
- **AND** example commands for building and running tests are provided
- **AND** differences between WSL and Docker workflows are explained

### Requirement: Efficient CI Caching Strategy
The system SHALL implement caching to minimize CI build times and resource usage.

#### Scenario: Docker layer caching works
- **WHEN** the Dockerfile is unchanged
- **THEN** GitHub Actions reuses cached Docker layers
- **AND** only changed layers are rebuilt
- **AND** the Clang P2996 build step is cached

#### Scenario: Cache invalidation on Dockerfile change
- **WHEN** the Dockerfile is modified
- **THEN** affected layers are rebuilt
- **AND** subsequent layers are invalidated
- **AND** the cache key is updated

#### Scenario: Clang version pinning
- **WHEN** the Clang P2996 commit hash is specified in Dockerfile
- **THEN** the cache remains valid across CI runs
- **AND** updating the commit hash triggers a rebuild
- **AND** developers can control when to update Clang

### Requirement: Comprehensive Test Execution
The system SHALL execute all project tests in the CI environment, including reflection-dependent tests.

#### Scenario: All tests run in CI
- **WHEN** the CI pipeline executes
- **THEN** all 6 basic tests are run (test_basic_types, test_vector, test_map_set, test_nested, test_compaction, test_modify)
- **AND** all 12 reflection tests are run
- **AND** demo programs are executed
- **AND** any test failure causes the CI to fail

#### Scenario: Test results are reported
- **WHEN** tests complete
- **THEN** the CI log shows individual test results
- **AND** passing tests are clearly indicated
- **AND** failing tests show error details
- **AND** test summary is displayed at the end
