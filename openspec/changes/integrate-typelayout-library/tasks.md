# 实施任务清单

## Phase 1: 依赖集成

- [x] 1.1 添加 TypeLayout 作为 Git Submodule
  ```bash
  git submodule add https://github.com/ximicpp/TypeLayout.git external/typelayout
  ```

- [x] 1.2 更新 `.gitmodules` 配置
  - 添加 `external/typelayout` 条目
  - 设置分支跟踪 `main`

- [x] 1.3 更新根 `CMakeLists.txt`
  - 添加 `external/typelayout/include` 到 include 路径
  - 确保与现有 Boost 路径不冲突
  - 同步更新 `tests/CMakeLists.txt` 和 `examples/CMakeLists.txt`

- [ ] 1.4 验证构建
  - 确认 `#include <boost/typelayout.hpp>` 可用
  - 运行现有测试确保无回归

## Phase 2: XTypeSignature 兼容层

- [x] 2.1 创建兼容层头文件
  - 在 `xoffsetdatastructure2.hpp` 中保留 `XTypeSignature` 命名空间
  - 添加 `[[deprecated]]` 属性到 `get_XTypeSignature()` API
  - 底层委托到 `boost::typelayout`
  - `CompileString<N>` → `boost::typelayout::FixedString<N>` 别名
  - `TypeSignature<T>` → `boost::typelayout::TypeSignature<T, Definition>` 别名

- [x] 2.2 处理签名格式差异
  - 直接使用 TypeLayout 的新格式（包含 `[64-le]` 平台前缀）
  - `struct` → `record` (TypeLayout 使用 `record` 表示 class/struct)
  - 测试中的 static_assert 需要相应更新

- [x] 2.3 更新 XVector/XString/XMap/XSet 的签名特化
  - 在 `boost::typelayout` 命名空间注册（而非 `XTypeSignature`）
  - 使用 `SignatureMode` 模板参数支持 Layout/Definition 两种模式

## Phase 3: 测试迁移

- [x] 3.1 更新 `test_reflection_type_signature.cpp`
  - 迁移到 `boost::typelayout::get_definition_signature<T>()` API
  - 更新签名字符串验证（`struct` → `record`，添加 `[64-le]` 前缀）

- [x] 3.2 更新 `test_class_type_signatures.cpp`
  - 迁移到新 API `boost::typelayout::get_definition_signature<T>()`
  - 使用 `operator<<` 替代 `.print()`

- [x] 3.3 更新 `test_field_limit_fix.cpp` 和 `test_vptr_layout.cpp`
  - 迁移到新 API
  - 使用 `operator<<` 替代 `.print()`

- [x] 3.4 添加 TypeLayout 集成测试（新增）
  - `test_typelayout_integration.cpp`：7 个测试覆盖 Definition/Layout 签名、平台前缀、容器、继承、原始类型、向后兼容
  - 注册到 `tests/CMakeLists.txt` 和 `build.sh`

## Phase 4: 示例更新

- [x] 4.1 更新 `examples/player.hpp`
  - 迁移 `static_assert` 到 `boost::typelayout::get_definition_signature<T>()`
  - 签名格式: `[64-le]record[...]` 替代 `struct[...]`

- [x] 4.2 更新 `examples/game_data.hpp`
  - 迁移 Item 和 GameData 的 `static_assert` 到新 API
  - 嵌套 `struct` → `record` 全部更新

- [x] 4.3 更新 `examples/helloworld.cpp`
  - 使用 `boost::typelayout::get_definition_signature<T>()`
  - 使用 `operator<<` 替代 `.print()` 循环

- [x] 4.4 `examples/demo.cpp` 已使用新 API（无需额外更改）

## Phase 5: 构建系统更新

- [x] 5.1 更新 `build.sh`
  - 添加 submodule 初始化检查
  - 自动 `git submodule update --init --recursive` 如果缺失

- [x] 5.2 Dockerfile 无需修改（使用 volume mount，submodule 在宿主机初始化）

- [x] 5.3 docker-compose.yml 无需修改（同理 volume mount）

## Phase 6: 文档更新

- [x] 6.1 创建迁移指南 `docs/MIGRATION_TYPELAYOUT.md`
  - XTypeSignature → TypeLayout 完整映射表
  - 前后代码对比示例
  - 两层签名系统说明
  - FAQ

- [x] 6.2 更新 `README.md`
  - 添加 TypeLayout 介绍和代码示例
  - 添加 `git clone --recursive` 说明

- [x] 6.3 更新 `docs/technical_overview.md`
  - 更新第 4/5 章节中的签名示例（`struct` → `record`，添加 `[64-le]` 前缀）
  - 添加 TypeLayout 两层签名系统说明
  - 添加 `layout_signatures_match` 示例

- [x] 6.4 更新 `AGENTS.md`
  - 更新 TypeLayout 作为外部依赖的说明
  - 替换旧的 TypeLayout Module 章节

## Phase 7: CI/CD 更新

- [x] 7.1 更新 GitHub Actions workflow
  - 添加 `submodules: recursive` 到 `ci.yml` 和 `ci-test.yml` 的 checkout 步骤

- [ ] 7.2 验证完整 CI 流程
  - 确保所有 21 个测试通过（6 基础 + 15 反射含新增 test_typelayout_integration）
  - 确保 Docker 构建成功

## 验收标准

- [x] 所有现有测试通过（无回归）— Docker 验证通过（21/21 PASS + demo + helloworld）
- [x] 新增测试覆盖 TypeLayout API（test_typelayout_integration.cpp，7 个测试用例）
- [x] 文档完整，迁移指南清晰（docs/MIGRATION_TYPELAYOUT.md）
- [ ] CI 绿色 — 待推送验证
