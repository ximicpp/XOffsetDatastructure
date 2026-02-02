# Change: Add CI/CD with Docker Support for Clang P2996

## Why
The XOffsetDatastructure project requires Clang with P2996 reflection support, which is not available in standard CI environments. Currently, there is no automated build and test pipeline, making it difficult to ensure code quality and catch regressions early. Additionally, local development requires manual setup of the specialized Clang compiler, creating barriers for new contributors.

## What Changes
- Add GitHub Actions CI/CD workflow using Docker to build and test the project
- Create Dockerfile with Clang P2996 pre-built for consistent development and CI environments
- Ensure all 18 tests (including reflection tests) run successfully in CI
- Provide Docker-based local development option alongside existing WSL workflow
- Add CI status badge to README.md

## Impact
- Affected specs: ci-cd (new capability)
- Affected code:
  - `.github/workflows/ci.yml` (new file)
  - `Dockerfile` (new file)
  - `.dockerignore` (new file)
  - `README.md` (add CI badge and Docker usage instructions)
  - `openspec/project.md` (update with CI/CD conventions)
- Benefits:
  - Automated testing on every push/PR
  - Reproducible build environment
  - Easier onboarding for new contributors
  - Early detection of build failures and test regressions
