# XOffsetDatastructure

[![CI](https://github.com/ximicpp/XOffsetDatastructure/actions/workflows/ci.yml/badge.svg)](https://github.com/ximicpp/XOffsetDatastructure/actions/workflows/ci.yml)

### Introduction
XOffsetDatastructure is a serialization library designed to reduce or even eliminate the performance consumption of serialization and deserialization by utilizing zero-encoding and zero-decoding. It is also a collection of high-performance data structures designed for efficient read and in-place/non-in-place write, with performance comparable to STL.  

### CppCon 2024
[CppCon 2024: Using Modern C++ to Build XOffsetDatastructure: A Zero-Encoding and Zero-Decoding High-Performance Serialization Library](https://github.com/CppCon/CppCon2024/blob/main/Presentations/Using_Modern_Cpp_to_Build_XOffsetDatastructure.pdf)

### CppCon 2025
[CppCon 2025: Cross-platform XOffsetDatastructure: Ensuring Zero-encoding/Zero-decoding Serialization Compatibility Through Compile-time Type Signatures](https://github.com/ximicpp/XOffsetDatastructure/blob/main/docs/Compile-timeTypeSignatures.pdf)

### Requirements

| Requirement | Status | Notes |
|-------------|--------|-------|
| **C++26 Reflection (P2996)** | ✅ Required | Library depends on `<experimental/meta>` |
| **Clang P2996 Fork** | ✅ Required | Standard compilers not supported |
| **64-bit Architecture** | ✅ Required | 32-bit not supported |
| **Little-endian** | ✅ Required | Big-endian not supported |

> ⚠️ **Non-Reflection Mode**: This library does **NOT** support a non-reflection fallback mode. The C++26 P2996 reflection feature is integral to the type signature system and cannot be disabled. Use the provided Docker image or build Clang P2996 manually.

### Build and Test

#### Option 1: Docker (Recommended for CI and Quick Start)

**Requirements:** Docker installed on your system

**Quick Start:**
```bash
# Build Docker image (first time only, takes 1-3 hours)
./scripts/docker-build.sh

# Or use docker-compose
docker-compose build

# Run tests in container
docker run -it -v $(pwd):/workspace xoffset-clang-p2996:latest ./build.sh

# Or with docker-compose
docker-compose run --rm xoffset-dev ./build.sh

# Interactive development
docker-compose run --rm xoffset-dev bash
```

**Advantages:**
- No need to manually build Clang P2996
- Consistent environment across all platforms
- Same environment as CI
- Easy onboarding for new contributors

#### Option 2: WSL/Linux with Manual Clang P2996 Build

**Requirements:**
- WSL2 (for Windows) or native Linux
- 8+ GB RAM, 50+ GB free disk space
- 1-3 hours for Clang build

**Setup:**
```bash
# 1. Build Clang P2996 (one-time setup)
cd scripts
./build_clang_p2996_wsl.sh

# 2. Build and test the project
cd ..
./build.sh

# Build without reflection tests
./build.sh --no-reflection

# Debug build
./build.sh --debug
```

See `AGENTS.md` for detailed build instructions and troubleshooting.

### Development Workflows

**Docker Workflow:**
- Mount source code into container
- Edit code on host, build in container
- All dependencies pre-installed

**WSL Workflow:**
- Direct native builds
- Full control over compiler installation
- Potential for slightly faster builds

### PS:
Benchmark code and results: see the tag ["v1.0.0: CppCon 2024 Milestone Release (Latest)"](https://github.com/ximicpp/XOffsetDatastructure/releases/tag/v1.0.0)