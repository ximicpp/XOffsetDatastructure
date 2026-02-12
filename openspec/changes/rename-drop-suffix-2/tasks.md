## 1. 核心库头文件
- [x] 1.1 重命名 `xoffsetdatastructure2.hpp` → `xoffsetdatastructure.hpp`
- [x] 1.2 更新 include guard: `X_OFFSET_DATA_STRUCTURE_2_HPP` → `X_OFFSET_DATA_STRUCTURE_HPP`
- [x] 1.3 更新命名空间: `namespace XOffsetDatastructure2` → `namespace XOffsetDatastructure` (2处)
- [x] 1.4 更新所有 `XOffsetDatastructure2::` 限定引用 (~20处)
- [x] 1.5 更新错误信息字符串中的 "XOffsetDatastructure2"
- [x] 1.6 更新 TypeLayout 特化中的命名空间引用 (4处)

## 2. Examples
- [x] 2.1 `player.hpp`: 更新 `#include` 路径和 `using namespace`
- [x] 2.2 `game_data.hpp`: 更新 `#include` 路径和 `using namespace`
- [x] 2.3 `helloworld.cpp`: 更新 `#include`、`using namespace`、注释/字符串
- [x] 2.4 `demo.cpp`: 更新 `#include`、`using namespace`、注释/字符串 (4处)
- [x] 2.5 `examples/CMakeLists.txt`: target 重命名 `xoffsetdatastructure2_demo` → `xoffsetdatastructure_demo`
- [x] 2.6 `examples/README.md`: 更新引用

## 3. Tests (25 files)
- [x] 3.1 批量更新所有 `tests/*.cpp` 的 `#include` 路径
- [x] 3.2 批量更新所有 `tests/*.cpp` 的 `using namespace`
- [x] 3.3 更新注释/字符串中的引用 (test_splice_operations, test_type_introspection, test_typelayout_integration, run_all_tests)
- [x] 3.4 `tests/CMakeLists.txt`: 更新 status message

## 4. Tools
- [x] 4.1 `tools/export_signatures.cpp`: 更新 `#include` 和注释
- [x] 4.2 `tools/check_compat.cpp`: 更新注释

## 5. 构建脚本
- [x] 5.1 `build.sh`: 更新 demo binary 路径引用 (3处) + 注释/banner (2处)
- [x] 5.2 `scripts/build_clang_p2996_wsl.sh`: 更新说明文字

## 6. 文档
- [x] 6.1 `AGENTS.md`: 更新文件名引用 (6处)
- [x] 6.2 `docs/*.md`: 批量更新所有 13 个 docs 文件
- [x] 6.3 `tests/README.md`: 更新引用 (4处)
- [x] 6.4 `tools/README.md`: 更新引用 (1处)

## 7. 验证
- [x] 7.1 全量构建测试通过 (25/25)
- [x] 7.2 确认零 "XOffsetDatastructure2" 残留引用 (排除 openspec/archive)