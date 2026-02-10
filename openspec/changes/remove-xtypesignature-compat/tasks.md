## 1. 代码清理：删除 XTypeSignature 命名空间
- [x] 1.1 将 `BASIC_ALIGNMENT` 和 `ANY_SIZE` 常量迁移到 `XOffsetDatastructure2` 命名空间
- [x] 1.2 将平台 `static_assert` 断言移到 `xoffsetdatastructure2.hpp` 文件顶层（`#include` 之后、命名空间之前）
- [x] 1.3 删除整个 `XTypeSignature` 命名空间块（第 57-115 行）
- [x] 1.4 删除相关的注释块（XTypeSignature 说明注释）

## 2. 更新测试文件
- [x] 2.1 `tests/test_typelayout_integration.cpp`：删除 Test 7（XTypeSignature 兼容层测试），更新测试计数
- [x] 2.2 `tests/test_reflection_type_signature.cpp`：将 `using namespace XTypeSignature` 替换为 `using namespace boost::typelayout`，更新所有相关 API 调用
- [x] 2.3 `tests/README.md`：移除对 XTypeSignature 集成的引用

## 3. 更新示例文件
- [x] 3.1 `examples/player.hpp`：将注释中的 `XTypeSignature::BASIC_ALIGNMENT` 替换为字面量 `8`
- [x] 3.2 `examples/game_data.hpp`：将 `XTypeSignature::BASIC_ALIGNMENT` 替换为字面量 `8`

## 4. 更新文档
- [x] 4.1 `docs/MIGRATION_TYPELAYOUT.md`：移除 "兼容层保留" 章节，标记兼容层已删除
- [x] 4.2 `docs/core-features-analysis.md`：更新类型签名系统描述，移除 `XTypeSignature` 引用
- [x] 4.3 `AGENTS.md`：将命名空间约定从 `XTypeSignature` 更新为 `boost::typelayout`
- [x] 4.4 `openspec/project.md`：更新命名空间列表，更新测试数量

## 5. 更新规范
- [x] 5.1 创建 delta spec（REMOVED: XTypeSignature 兼容层，MODIFIED: TypeLayout 依赖集成）

## 6. 验证
- [x] 6.1 在 Docker 中构建并运行所有测试（19/19 通过，EXIT_CODE=0）
- [x] 6.2 全项目 grep 确认无残留的 `XTypeSignature` 引用（archive 和 migration guide 历史说明除外）
