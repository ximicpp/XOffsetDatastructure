
# AGENTS.md - XOffsetDatastructure Development Guide

This file contains build commands, code style guidelines, and development practices for agentic coding agents working in this repository.

## ⚡ Local Testing Quick Reference (READ THIS FIRST)

> **Every code change MUST be verified locally before considering it done.**
> There are two ways to build and test. Pick whichever is available.

### Option A: Native P2996 Compiler (if installed locally)

The host machine has Clang P2996 installed. The build script auto-detects it.

```bash
# Just run from the repo root — build.sh finds the compiler automatically
./build.sh

# Searched paths (in order):
#   /usr/local/bin/clang++
#   ~/clang-p2996-install/bin/clang++
#   /opt/clang-p2996/bin/clang++
#   /opt/p2996-toolchain/bin/clang++  (Docker image)
```

### Option B: Docker (if no local P2996 compiler)

Use one of these pre-built Docker images (check which is available locally):
- `ghcr.io/ximicpp/typelayout-p2996:latest` — from TypeLayout CI (P2996 at `/opt/p2996-toolchain/`)
- `xoffset-clang-p2996:latest` — custom local build (P2996 at `~/clang-p2996-install/`)

```bash
# One-liner: build + test in Docker (MUST use 'bash')
# Use whichever image is available:
docker run --rm -v $(pwd):/workspace -w /workspace ghcr.io/ximicpp/typelayout-p2996:latest bash ./build.sh

# Or with the local custom image:
docker run --rm -v $(pwd):/workspace -w /workspace xoffset-clang-p2996:latest bash ./build.sh

# On Apple Silicon (M1/M2/M3), add --platform flag:
docker run --rm --platform linux/amd64 -v $(pwd):/workspace -w /workspace ghcr.io/ximicpp/typelayout-p2996:latest bash ./build.sh

# Or use the convenience script
./scripts/local-docker-test.sh

# Interactive debugging shell
docker run --rm -it -v $(pwd):/workspace -w /workspace ghcr.io/ximicpp/typelayout-p2996:latest bash
```

**⚠️ Docker gotchas:**
- Use `bash ./build.sh`, NOT `./build.sh` (permission issues)
- First local image build takes 1-3 hours: `./scripts/docker-build.sh`
- The image is Linux x86_64; on Apple Silicon **always** use `--platform linux/amd64`

### How to know which option is available

```bash
# Check if P2996 compiler exists locally
ls ~/clang-p2996-install/bin/clang++ 2>/dev/null && echo "Option A available" || echo "Option A NOT available"

# Check if Docker images exist (check both)
docker images --format "{{.Repository}}:{{.Tag}}" 2>/dev/null | grep -E "typelayout-p2996|xoffset-clang" && echo "Option B available" || echo "Option B NOT available"
```

---

## Build System

### Docker Build (Recommended for New Users)

**⚠️ IMPORTANT: Always test locally before pushing to GitHub Actions!**

```bash
# RECOMMENDED: Test locally first (in WSL)
./scripts/local-docker-test.sh

# Build Docker image (one-time, 1-3 hours)
./scripts/docker-build.sh

# Or with docker-compose
docker-compose build

# Run tests in Docker (CORRECT way - use 'bash')
docker run --rm -v $(pwd):/workspace -w /workspace xoffset-clang-p2996:latest bash ./build.sh

# ❌ WRONG: Direct execution may fail due to permissions
# docker run ... ./build.sh

# Interactive shell for debugging
docker-compose run --rm xoffset-dev bash
./scripts/local-docker-test.sh -i
```

**Key Points**:
1. **Use `bash ./build.sh`** not `./build.sh` in Docker
2. **Test locally** before pushing to CI
3. **Review** `docs/BUILD_AND_TEST_GUIDE.md` for details

### Primary Build Commands
```bash
# Full build with tests (recommended)
./build.sh

# Build without reflection support
./build.sh --no-reflection

# Debug build
./build.sh --debug

# Build with specific job count
./build.sh -j 8

# Build with system compiler (no P2996)
./build.sh --no-p2996
```

### CMake Commands
```bash
# Configure and build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Build specific target
cmake --build . --target test_basic_types --config Release
```

### Running Tests
```bash
# Run all tests (via build script)
./build.sh

# In Docker
docker-compose run --rm xoffset-dev ./build.sh

# Run individual tests
cd build
./bin/Release/test_basic_types
./bin/Release/test_vector
./bin/Release/test_reflection_operators

# Run tests via CTest
cd build
ctest --verbose
ctest -R "test_basic_types" --verbose
```

### TypeLayout Library (external dependency)
TypeLayout is integrated as a Git submodule at `external/typelayout`.
It provides the type-signature engine used by XOffsetDatastructure.

```bash
# Initialize submodules (required after fresh clone)
git submodule update --init --recursive

# TypeLayout include path: external/typelayout/include
# Main header: #include <boost/typelayout.hpp>
```

## Code Style Guidelines

### File Organization
- **Header files**: Use `.hpp` extension
- **Source files**: Use `.cpp` extension
- **Main library**: `xoffsetdatastructure.hpp` (single header library)
- **Tests**: Organized in `tests/` directory with descriptive names
- **Examples**: Organized in `examples/` directory

### Include Order
```cpp
// 1. System headers (if needed)
#include <iostream>
#include <vector>
#include <cassert>

// 2. Conditional includes (platform-specific)
#if !defined(__clang__) || __clang_major__ >= 15
#include <chrono>
#endif

// 3. Main library header
#include "../xoffsetdatastructure.hpp"

// 4. Local headers
#include "game_data.hpp"
```

### Namespace Conventions
```cpp
// Main library namespace
using namespace XOffsetDatastructure;

// Type signatures (provided by TypeLayout library)
using namespace boost::typelayout;

// Reflection code (C++26 only)
#ifdef __cpp_reflection
using namespace std::meta;
#endif
```

### Naming Conventions
- **Classes**: `PascalCase` (e.g., `BasicTypes`, `XVector`)
- **Functions**: `snake_case` (e.g., `test_basic_types`, `print_section`)
- **Member variables**: `mPascalCase` (e.g., `mInt`, `mFloat`)
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `BASIC_ALIGNMENT`, `ANY_SIZE`)
- **Templates**: `T` or descriptive names (e.g., `typename Allocator`)

### Error Handling
```cpp
// Use static_assert for compile-time validation
static_assert(is_xbuffer_safe<BasicTypes>::value, 
              "BasicTypes must be safe for XBuffer");

// Use assert for runtime validation
assert(result == expected && "Data integrity check failed");

// Return bool for test functions
bool test_function() {
    // Test implementation
    return true; // or false on failure
}
```

### Test Structure
```cpp
// ============================================================================
// Test: Feature Name
// Purpose: Brief description of what this test validates
// ============================================================================

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// Test data structures
struct TestStruct {
    template <typename Allocator>
    TestStruct(Allocator allocator) {}
    
    // Members...
};

bool test_feature() {
    std::cout << "\n[TEST] Feature Name\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Test implementation
    std::cout << "Test 1: Description... [OK]\n";
    
    return true;
}

int main() {
    if (test_feature()) {
        std::cout << "[PASS] All tests passed!\n";
        return 0;
    }
    return 1;
}
```

### Platform-Specific Code
```cpp
// Compiler detection
#if defined(_MSC_VER)
    // Windows/MSVC specific
#elif defined(__clang__) || defined(__GNUC__)
    // Clang/GCC specific
#endif

// Architecture detection
#if defined(__LP64__) || defined(_WIN64)
    // 64-bit specific
#endif

// Endianness detection
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    // Endianness specific
#endif
```

## Development Practices

### Compiler Requirements
- **Primary**: Clang with P2996 reflection support
- **Standard**: C++26 (`-std=c++26`)
- **Flags**: `-freflection -fexpansion-statements -stdlib=libc++`
- **Architecture**: 64-bit little-endian only

### Adding New Tests
1. Create `tests/test_feature.cpp` following the test structure
2. Add to `tests/CMakeLists.txt`:
   ```cmake
   add_executable(test_feature test_feature.cpp)
   target_include_directories(test_feature PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
   set_target_properties(test_feature PROPERTIES
       RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
   )
   add_test(NAME FeatureTest COMMAND test_feature)
   ```
3. Update `tests/README.md` with documentation

### Adding New Examples
1. Create `examples/feature_example.cpp`
2. Add to `examples/CMakeLists.txt` following existing pattern
3. Update examples documentation

### Memory Management
- Use Boost.Interprocess allocators for shared memory
- Follow RAII principles for resource management
- Validate buffer safety with `is_xbuffer_safe<T>::value`

### Reflection Code (C++26)
```cpp
#ifdef __cpp_reflection
// Reflection-specific code
using namespace std::meta;

// Use reflection operators
auto members = nonstatic_data_members_of(^T);
for (auto member : members) {
    // Process member
}
#endif
```

### Performance Considerations
- Zero-encoding serialization is the primary goal
- Avoid unnecessary copies in serialization paths
- Use compile-time optimizations where possible
- Profile with real-world data structures

## Documentation
- Update relevant README files when adding features
- Document test purposes in test file headers
- Maintain technical overview in `docs/technical_overview.md`
- Update build scripts when adding new dependencies

## Common Issues
- **Clang P2996 not found**: Install in `~/clang-p2996-install/` or use Docker
- **Docker build timeout**: First build takes 1-3 hours, use pre-built image if available
- **Reflection tests fail**: Use `--no-reflection` or ensure proper Clang version
- **Memory alignment issues**: Check `is_xbuffer_safe<T>::value` validation
- **Platform compatibility**: Ensure 64-bit little-endian architecture
- **Docker permission issues**: Add user to docker group: `sudo usermod -aG docker $USER`

## Agent Roles

This repository supports multi-agent development. Each agent session can be assigned
a role that defines its file scope, behavioral rules, and coordination protocol.

**How to use**: Start a new session with a role prompt like:
> "你是 test agent，读 AGENTS.md 的 `Role: tests` 分区，检查 openspec/changes 中标有 `[needs-tests]` 的变更。"

### Role: core

**核心代码开发 — 库的架构、API、和安全模型。**

**Scope (primary files):**
- `xoffsetdatastructure.hpp` — 主库头文件
- `external/typelayout` — TypeLayout 子模块集成
- `examples/*.hpp` — 数据结构定义（`player.hpp`, `game_data.hpp`）

**Rules:**
- API 变更必须创建 `openspec change`，在 tasks.md 中标注 `[needs-tests]` `[needs-docs]`
- 每次修改核心代码后必须 Docker 构建验证（27/27 测试通过）
- TypeLayout 子模块升级需先 `git fetch origin` 检查 main 分支最新
- 不直接修改 `tests/*.cpp`（除非是修复因 API 变更导致的编译错误）
- 不直接修改 `docs/` 或 `README.md`（由 docs agent 负责）

**Coordination output:**
- `openspec/changes/<name>/proposal.md` — 描述变更内容
- `openspec/changes/<name>/tasks.md` — 标注 `[needs-tests]` `[needs-docs]` 任务

### Role: tests

**测试用例开发 — 验证库功能、类型安全、和反射能力。**

**Scope (primary files):**
- `tests/*.cpp` — 所有测试源文件
- `tests/CMakeLists.txt` — 测试注册
- `tests/README.md` — 测试文档
- `build.sh` — 仅测试列表部分（`run_test` 调用）

**Rules:**
- 启动时检查 `openspec/changes/*/tasks.md` 中标有 `[needs-tests]` 的未完成任务
- 新测试文件必须注册到 `tests/CMakeLists.txt` 的 `REFLECTION_TESTS` 列表
- 新测试必须同步更新 `build.sh` 中的 `run_test` 调用
- 合并后的测试文件不超过 300 行
- 每个测试文件 header 注释说明 Purpose
- 修改后必须 Docker 构建验证（全部测试通过）
- 不修改 `xoffsetdatastructure.hpp`（如发现 bug，创建 openspec change 交给 core agent）

**Coordination input:**
```bash
# 查找待处理的测试任务
grep -r "\[needs-tests\]" openspec/changes/*/tasks.md 2>/dev/null
```

**Test suite summary (27 tests):**
- Basic tests: 7 (test_basic_types, test_vector, test_map_set, test_nested, test_compaction, test_modify, test_xbuffer_api)
- Reflection tests: 20 (see tests/README.md for full list)

### Role: docs

**文档维护 — 保持文档与代码实现一致。**

**Scope (primary files):**
- `docs/` — 技术文档目录
- `README.md` — 项目根 README
- `tests/README.md` — 测试列表文档
- `examples/*.cpp` — 示例代码（`demo.cpp`, `helloworld.cpp`）
- `AGENTS.md` — 本文件（Agent 指南更新）

**Rules:**
- 启动时检查 `openspec/changes/*/tasks.md` 中标有 `[needs-docs]` 的未完成任务
- 保持 `tests/README.md` 中的测试列表与 `tests/CMakeLists.txt` 一致
- 保持 `docs/technical_overview.md` 与实际架构一致
- 示例代码必须可编译（但不需要 Docker 构建验证全套测试）
- 不修改 `xoffsetdatastructure.hpp` 或 `tests/*.cpp`

**Coordination input:**
```bash
# 查找待处理的文档任务
grep -r "\[needs-docs\]" openspec/changes/*/tasks.md 2>/dev/null
```

### Cross-Agent Coordination

**标签约定** — 在 `openspec/changes/<name>/tasks.md` 中使用：

| 标签 | 含义 | 谁处理 |
|------|------|--------|
| `[core]` | 核心代码任务 | core agent |
| `[tests]` | 测试开发任务 | tests agent |
| `[docs]` | 文档更新任务 | docs agent |
| `[needs-tests]` | 此核心变更需要测试跟进 | tests agent 认领 |
| `[needs-docs]` | 此变更需要文档跟进 | docs agent 认领 |

**协作流程:**

```
1. core agent 完成 API 变更
   → commit + push
   → tasks.md 标注 [needs-tests] [needs-docs]

2. tests agent 启动（新会话）
   → git pull
   → grep "[needs-tests]" openspec/changes/*/tasks.md
   → 创建/更新测试
   → Docker 验证
   → commit + push
   → 标记 [needs-tests] 任务为 [x]

3. docs agent 启动（新会话）
   → git pull
   → grep "[needs-docs]" openspec/changes/*/tasks.md
   → 更新文档
   → commit + push
   → 标记 [needs-docs] 任务为 [x]
```

**冲突解决**: 如果多个 Agent 同时修改同一文件，由人类（你）仲裁。
Agent 不应自行解决 git merge 冲突。
