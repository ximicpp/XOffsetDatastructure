# Change: Drop "2" suffix from XOffsetDatastructure2

## Why
项目名已经是 XOffsetDatastructure（仓库名、URL），代码中的 "2" 后缀是历史遗留，
对新用户造成困惑（"是不是还有 v1？"），且命名空间/头文件/文档不一致。

## What Changes
- 命名空间: `XOffsetDatastructure2` → `XOffsetDatastructure`
- 头文件重命名: `xoffsetdatastructure2.hpp` → `xoffsetdatastructure.hpp`
- Include guard: `X_OFFSET_DATA_STRUCTURE_2_HPP` → `X_OFFSET_DATA_STRUCTURE_HPP`
- CMake target: `xoffsetdatastructure2_demo` → `xoffsetdatastructure_demo`
- 构建脚本 (`build.sh`): demo 路径引用更新
- 所有 `.cpp`/`.hpp` 中的 `#include` 和 `using namespace` 更新
- 所有注释/字符串中的 `XOffsetDatastructure2` → `XOffsetDatastructure`
- 所有文档 `.md` 中的引用更新
- **BREAKING**: 外部代码如果使用旧命名空间/头文件名将编译失败

## Impact
- Affected specs: `examples`
- Affected code:
  - `xoffsetdatastructure2.hpp` → `xoffsetdatastructure.hpp` (核心库)
  - `examples/*.cpp`, `examples/*.hpp` (4 files)
  - `tests/*.cpp` (25 files)
  - `tools/*.cpp` (2 files)
  - `examples/CMakeLists.txt`, `tests/CMakeLists.txt`
  - `build.sh`, `scripts/build_clang_p2996_wsl.sh`
  - `AGENTS.md` + 13 docs files + `examples/README.md` + `tests/README.md` + `tools/README.md`
