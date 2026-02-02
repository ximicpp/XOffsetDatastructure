# Project Context

## Purpose
XOffsetDatastructure is a zero-encoding/zero-decoding high-performance serialization library that leverages C++26 reflection features. The project aims to eliminate serialization/deserialization overhead through compile-time type introspection and offset-based data structures.

## Tech Stack
- **Language:** C++26 with reflection support (P2996)
- **Compiler:** Bloomberg Clang P2996 (required)
- **Build System:** CMake 3.10+, Ninja
- **Dependencies:** Boost (Interprocess, PFR, Container)
- **CI/CD:** GitHub Actions with Docker
- **Testing:** Custom test suite (18 tests total)

## Project Conventions

### Code Style
See `AGENTS.md` for complete style guide. Key conventions:
- **Headers:** `.hpp` extension
- **Naming:** 
  - Classes: `PascalCase` (e.g., `BasicTypes`, `XVector`)
  - Functions: `snake_case` (e.g., `test_basic_types`)
  - Members: `mPascalCase` (e.g., `mInt`, `mFloat`)
- **Namespaces:** `XOffsetDatastructure2`, `XTypeSignature`
- **Include order:** System → Platform-specific → Library → Local

### Architecture Patterns
- **Zero-encoding serialization:** Data structures use offset-based pointers
- **Compile-time reflection:** C++26 `std::meta` for type introspection
- **Single-header library:** `xoffsetdatastructure2.hpp`
- **Allocator-based design:** Boost.Interprocess allocators for shared memory
- **Type safety:** `is_xbuffer_safe<T>` trait for validation

### Testing Strategy
- **Test organization:** `tests/` directory with descriptive names
- **Test structure:** Header with purpose, test functions returning bool, main driver
- **Categories:**
  - Basic tests (6): Core functionality without reflection
  - Reflection tests (12): C++26 reflection features
- **Execution:** Via `build.sh` or individual test binaries
- **CI requirement:** All 18 tests must pass for PR merge

### Git Workflow
- **Main branch:** `main` (or `master`)
- **Feature branches:** Create from main, descriptive names
- **Pull requests:** Required for merging, must pass CI
- **Commit messages:** Clear, descriptive, reference issues when applicable
- **CI/CD:** Automated testing on all pushes and PRs

## Domain Context
- **Serialization paradigm:** Zero-encoding means data structures are directly usable in memory without conversion
- **Reflection usage:** C++26 P2996 proposal enables compile-time type metadata
- **Shared memory:** Boost.Interprocess allocators allow data sharing across processes
- **Cross-platform compatibility:** Type signatures ensure binary compatibility
- **Performance focus:** Avoid copies, minimize allocations, compile-time optimizations

## Important Constraints
- **Compiler requirement:** Bloomberg Clang P2996 mandatory (no GCC/MSVC support)
- **Architecture:** 64-bit little-endian only
- **C++ standard:** C++26 (uses bleeding-edge features)
- **Reflection flags:** `-freflection -fexpansion-statements -stdlib=libc++`
- **Platform priority:** Linux primary, macOS secondary, Windows via WSL
- **Build time:** Clang P2996 compilation takes 1-3 hours

## External Dependencies
- **Bloomberg Clang P2996:** Custom LLVM fork with reflection support
  - Repository: https://github.com/bloomberg/clang-p2996
  - Branch: `p2996`
- **Boost libraries:** Bundled in `external/boost`
  - Interprocess, PFR, Container, Core, Config, etc.
- **Build tools:** CMake, Ninja, Git
- **CI infrastructure:** GitHub Actions, Docker
